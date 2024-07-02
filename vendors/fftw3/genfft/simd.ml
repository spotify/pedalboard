(*
 * Copyright (c) 1997-1999 Massachusetts Institute of Technology
 * Copyright (c) 2003, 2007-14 Matteo Frigo
 * Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *)

open Expr
open List
open Printf
open Variable
open Annotate
open Simdmagic
open C

let realtype = "V"
let realtypep = realtype ^ " *"
let constrealtype = "const " ^ realtype
let constrealtypep = constrealtype ^ " *"
let alignment_mod = 2

(*
 * SIMD C AST unparser 
 *)
let foldr_string_concat l = fold_right (^) l ""

let rec unparse_by_twiddle nam tw src = 
  sprintf "%s(&(%s),%s)" nam (Variable.unparse tw) (unparse_expr src)

and unparse_store dst = function
  | Times (NaN MULTI_A, x) ->
      sprintf "STM%d(&(%s),%s,%s,&(%s));\n" 
	!Simdmagic.store_multiple
	(Variable.unparse dst) (unparse_expr x)
	(Variable.vstride_of_locative dst)
	(Variable.unparse_for_alignment alignment_mod dst)
  | Times (NaN MULTI_B, Plus stuff) ->
      sprintf "STN%d(&(%s)%s,%s);\n" 
	!Simdmagic.store_multiple
	(Variable.unparse dst) 
	(List.fold_right (fun x a -> "," ^ (unparse_expr x) ^ a) stuff "")
	(Variable.vstride_of_locative dst)
  | src_expr -> 
      sprintf "ST(&(%s),%s,%s,&(%s));\n" 
	(Variable.unparse dst) (unparse_expr src_expr) 
	(Variable.vstride_of_locative dst)
	(Variable.unparse_for_alignment alignment_mod dst)

and unparse_expr =
  let rec unparse_plus = function
    | [a] -> unparse_expr a

    | (Uminus (Times (NaN I, b))) :: c :: d -> op2 "VFNMSI" [b] (c :: d)
    | c :: (Uminus (Times (NaN I, b))) :: d -> op2 "VFNMSI" [b] (c :: d)
    | (Uminus (Times (NaN CONJ, b))) :: c :: d -> op2 "VFNMSCONJ" [b] (c :: d)
    | c :: (Uminus (Times (NaN CONJ, b))) :: d -> op2 "VFNMSCONJ" [b] (c :: d)
    | (Times (NaN I, b)) :: c :: d -> op2 "VFMAI" [b] (c :: d)
    | c :: (Times (NaN I, b)) :: d -> op2 "VFMAI" [b] (c :: d)
    | (Times (NaN CONJ, b)) :: (Uminus c) :: d -> op2 "VFMSCONJ" [b] (c :: d)
    | (Uminus c) :: (Times (NaN CONJ, b)) :: d -> op2 "VFMSCONJ" [b] (c :: d)
    | (Times (NaN CONJ, b)) :: c :: d -> op2 "VFMACONJ" [b] (c :: d)
    | c :: (Times (NaN CONJ, b)) :: d -> op2 "VFMACONJ" [b] (c :: d)
    | (Times (NaN _, b)) :: (Uminus c) :: d -> failwith "VFMS NaN"
    | (Uminus c) :: (Times (NaN _, b)) :: d -> failwith "VFMS NaN"

    | (Uminus (Times (a, b))) :: c :: d -> op3 "VFNMS" a b (c :: d)
    | c :: (Uminus (Times (a, b))) :: d -> op3 "VFNMS" a b (c :: d)
    | (Times (a, b)) :: (Uminus c) :: d -> op3 "VFMS" a b (c :: negate d)
    | (Uminus c) :: (Times (a, b)) :: d -> op3 "VFMS" a b (c :: negate d)
    | (Times (a, b)) :: c :: d          -> op3 "VFMA" a b (c :: d)
    | c :: (Times (a, b)) :: d          -> op3 "VFMA" a b (c :: d)

    | (Uminus a :: b)                   -> op2 "VSUB" b [a]
    | (b :: Uminus a :: c)              -> op2 "VSUB" (b :: c) [a]
    | (a :: b)                          -> op2 "VADD" [a] b
    | [] -> failwith "unparse_plus"
  and op3 nam a b c =
    nam ^ "(" ^ (unparse_expr a) ^ ", " ^ (unparse_expr b) ^ ", " ^
    (unparse_plus c) ^ ")"
  and op2 nam a b = 
    nam ^ "(" ^ (unparse_plus a) ^ ", " ^ (unparse_plus b) ^ ")"
  and op1 nam a = 
    nam ^ "(" ^ (unparse_expr a) ^ ")"
  and negate = function
    | [] -> []
    | (Uminus x) :: y -> x :: negate y
    | x :: y -> (Uminus x) :: negate y

  in function
    | CTimes(Load tw, src) 
	when Variable.is_constant tw && !Magic.generate_bytw ->
	unparse_by_twiddle "BYTW" tw src
    | CTimesJ(Load tw, src) 
	when Variable.is_constant tw && !Magic.generate_bytw ->
	unparse_by_twiddle "BYTWJ" tw src
    | Load v when is_locative(v) ->
	sprintf "LD(&(%s), %s, &(%s))" (Variable.unparse v) 
	  (Variable.vstride_of_locative v)
	  (Variable.unparse_for_alignment alignment_mod v)
    | Load v when is_constant(v) -> sprintf "LDW(&(%s))" (Variable.unparse v)
    | Load v  -> Variable.unparse v
    | Num n -> sprintf "LDK(%s)" (Number.to_konst n)
    | NaN n -> failwith "NaN in unparse_expr"
    | Plus [] -> "0.0 /* bug */"
    | Plus [a] -> " /* bug */ " ^ (unparse_expr a)
    | Plus a -> unparse_plus a
    | Times(NaN I,b) -> op1 "VBYI" b
    | Times(NaN CONJ,b) -> op1 "VCONJ" b
    | Times(a,b) ->
	sprintf "VMUL(%s, %s)" (unparse_expr a) (unparse_expr b)
    | CTimes(a,Times(NaN I, b)) ->
	sprintf "VZMULI(%s, %s)" (unparse_expr a) (unparse_expr b)
    | CTimes(a,b) ->
	sprintf "VZMUL(%s, %s)" (unparse_expr a) (unparse_expr b)
    | CTimesJ(a,Times(NaN I, b)) ->
	sprintf "VZMULIJ(%s, %s)" (unparse_expr a) (unparse_expr b)
    | CTimesJ(a,b) ->
	sprintf "VZMULJ(%s, %s)" (unparse_expr a) (unparse_expr b)
    | Uminus a when !Magic.vneg -> op1 "VNEG" a
    | Uminus a -> failwith "SIMD Uminus"
    | _ -> failwith "unparse_expr"

and unparse_decl x = C.unparse_decl x

and unparse_ast ast = 
  let rec unparse_assignment = function
    | Assign (v, x) when Variable.is_locative v ->
	unparse_store v x
    | Assign (v, x) -> 
	(Variable.unparse v) ^ " = " ^ (unparse_expr x) ^ ";\n"

  and unparse_annotated force_bracket = 
    let rec unparse_code = function
      | ADone -> ""
      | AInstr i -> unparse_assignment i
      | ASeq (a, b) -> 
	  (unparse_annotated false a) ^ (unparse_annotated false b)
    and declare_variables l = 
      let rec uvar = function
	  [] -> failwith "uvar"
	|	[v] -> (Variable.unparse v) ^ ";\n"
	| a :: b -> (Variable.unparse a) ^ ", " ^ (uvar b)
      in let rec vvar l = 
	let s = if !Magic.compact then 15 else 1 in
	if (List.length l <= s) then
	  match l with
	    [] -> ""
	  | _ -> realtype ^ " " ^ (uvar l)
	else
	  (vvar (Util.take s l)) ^ (vvar (Util.drop s l))
      in vvar (List.filter Variable.is_temporary l)
    in function
        Annotate (_, _, decl, _, code) ->
          if (not force_bracket) && (Util.null decl) then 
            unparse_code code
          else "{\n" ^
            (declare_variables decl) ^
            (unparse_code code) ^
	    "}\n"

  in match ast with 
  | Asch a -> (unparse_annotated true a)
  | Return x -> "return " ^ unparse_ast x ^ ";"
  | Simd_leavefun -> "VLEAVE();"
  | For (a, b, c, d) ->
      "for (" ^
      unparse_ast a ^ "; " ^ unparse_ast b ^ "; " ^ unparse_ast c
      ^ ")" ^ unparse_ast d
  | If (a, d) ->
      "if (" ^
      unparse_ast a 
      ^ ")" ^ unparse_ast d
  | Block (d, s) ->
      if (s == []) then ""
      else 
        "{\n"                                      ^ 
        foldr_string_concat (map unparse_decl d)   ^ 
        foldr_string_concat (map unparse_ast s)    ^
        "}\n"      
  | x -> C.unparse_ast x

and unparse_function = function
    Fcn (typ, name, args, body) ->
      let rec unparse_args = function
          [Decl (a, b)] -> a ^ " " ^ b 
	| (Decl (a, b)) :: s -> a ^ " " ^ b  ^ ", "
            ^  unparse_args s
	| [] -> ""
	| _ -> failwith "unparse_function"
      in 
      (typ ^ " " ^ name ^ "(" ^ unparse_args args ^ ")\n" ^
       unparse_ast body)

let extract_constants f =
  let constlist = flatten (map expr_to_constants (C.ast_to_expr_list f))
  in map
       (fun n ->
	  Tdecl 
	    ("DVK(" ^ (Number.to_konst n) ^ ", " ^ (Number.to_string n) ^ 
	       ");\n"))
       (unique_constants constlist)
