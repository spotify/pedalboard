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

(*
 * This module contains the definition of a C-like abstract
 * syntax tree, and functions to convert ML values into C
 * programs
 *)

open Expr
open Annotate
open List

let realtype = "R"
let realtypep = realtype ^ " *"
let extended_realtype = "E"
let constrealtype = "const " ^ realtype
let constrealtypep = constrealtype ^ " *"

let stridetype = "stride"

(***********************************
 * C program structure 
 ***********************************)
type c_decl = 
  | Decl of string * string
  | Tdecl of string                (* arbitrary text declaration *)

and c_ast =
  | Asch of annotated_schedule
  | Simd_leavefun
  | Return of c_ast
  | For of c_ast * c_ast * c_ast * c_ast
  | If of c_ast * c_ast
  | Block of (c_decl list) * (c_ast list)
  | Binop of string * c_ast * c_ast
  | Expr_assign of c_ast * c_ast
  | Stmt_assign of c_ast * c_ast
  | Comma of c_ast * c_ast
  | Integer of int
  | CVar of string
  | CCall of string * c_ast
  | CPlus of c_ast list
  | ITimes of c_ast * c_ast
  | CUminus of c_ast
and c_fcn = Fcn of string * string * (c_decl list) * c_ast


let ctimes = function
  | (Integer 1), a -> a
  | a, (Integer 1) -> a
  | a, b -> ITimes (a, b)

(*
 * C AST unparser 
 *)
let foldr_string_concat l = fold_right (^) l ""

let rec unparse_expr_c =
  let yes x = x and no x = "" in

  let rec unparse_plus maybe = 
    let maybep = maybe " + " in
    function
    | [] -> ""
    | (Uminus (Times (a, b))) :: (Uminus c) :: d -> 
	maybep ^ (op "FNMA" a b c) ^ (unparse_plus yes d)
    | (Uminus c) :: (Uminus (Times (a, b))) :: d -> 
	maybep ^ (op "FNMA" a b c) ^ (unparse_plus yes d)
    | (Uminus (Times (a, b))) :: c :: d -> 
	maybep ^ (op "FNMS" a b c) ^ (unparse_plus yes d)
    | c :: (Uminus (Times (a, b))) :: d -> 
	maybep ^ (op "FNMS" a b c) ^ (unparse_plus yes d)
    | (Times (a, b)) :: (Uminus c) :: d -> 
	maybep ^ (op "FMS" a b c) ^ (unparse_plus yes d)
    | (Uminus c) :: (Times (a, b)) :: d -> 
	maybep ^ (op "FMS" a b c) ^ (unparse_plus yes d)
    | (Times (a, b)) :: c :: d -> 
	maybep ^ (op "FMA" a b c) ^ (unparse_plus yes d)
    | c :: (Times (a, b)) :: d -> 
	maybep ^ (op "FMA" a b c) ^ (unparse_plus yes d)
    | (Uminus a :: b) -> 
	" - " ^ (parenthesize a) ^ (unparse_plus yes b)
    | (a :: b) -> 
	maybep ^ (parenthesize a) ^ (unparse_plus yes b)
  and parenthesize x = match x with
  | (Load _) -> unparse_expr_c x
  | (Num _) -> unparse_expr_c x
  | _ -> "(" ^ (unparse_expr_c x) ^ ")"
  and op nam a b c =
    nam ^ "(" ^ (unparse_expr_c a) ^ ", " ^ (unparse_expr_c b) ^ ", " ^
    (unparse_expr_c c) ^ ")"
      			      
  in function
    | Load v -> Variable.unparse v
    | Num n -> Number.to_konst n
    | Plus [] -> "0.0 /* bug */"
    | Plus [a] -> " /* bug */ " ^ (unparse_expr_c a)
    | Plus a -> (unparse_plus no a)
    | Times (a, b) -> (parenthesize a) ^ " * " ^ (parenthesize b)
    | Uminus (Plus [a; Uminus b]) -> unparse_plus no [b; Uminus a]
    | Uminus a -> "- " ^ (parenthesize a)
    | _ -> failwith "unparse_expr_c"

and unparse_expr_generic = 
  let rec u x = unparse_expr_generic x
  and unary op a = Printf.sprintf "%s(%s)" op (u a)
  and binary op a b = Printf.sprintf "%s(%s, %s)" op (u a) (u b)
  and ternary op a b c = Printf.sprintf "%s(%s, %s, %s)" op (u a) (u b) (u c)
  and quaternary op a b c d = 
    Printf.sprintf "%s(%s, %s, %s, %s)" op (u a) (u b) (u c) (u d)
  and unparse_plus = function
    | [(Uminus (Times (a, b))); Times (c, d)] -> quaternary "FNMMS" a b c d
    | [Times (c, d); (Uminus (Times (a, b)))] -> quaternary "FNMMS" a b c d
    | [Times (c, d); (Times (a, b))] -> quaternary "FMMA" a b c d
    | [(Uminus (Times (a, b))); c] -> ternary "FNMS" a b c
    | [c; (Uminus (Times (a, b)))] -> ternary "FNMS" a b c
    | [(Uminus c); (Times (a, b))] -> ternary "FMS" a b c
    | [(Times (a, b)); (Uminus c)] -> ternary "FMS" a b c
    | [c; (Times (a, b))] -> ternary "FMA" a b c
    | [(Times (a, b)); c] -> ternary "FMA" a b c
    | [a; Uminus b] -> binary "SUB" a b
    | [a; b] -> binary "ADD" a b
    | a :: b :: c -> binary "ADD" a (Plus (b :: c))
    | _ -> failwith "unparse_plus"
  in function
    | Load v -> Variable.unparse v 
    | Num n -> Number.to_konst n
    | Plus a -> unparse_plus a
    | Times (a, b) -> binary "MUL" a b
    | Uminus a -> unary "NEG" a
    | _ -> failwith "unparse_expr"

and unparse_expr x = 
  if !Magic.generic_arith then
    unparse_expr_generic x
  else
    unparse_expr_c x

and unparse_assignment (Assign (v, x)) =
  (Variable.unparse v) ^ " = " ^ (unparse_expr x) ^ ";\n"

and unparse_annotated force_bracket = 
  let rec unparse_code = function
      ADone -> ""
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
	| _ -> extended_realtype ^ " " ^ (uvar l)
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

and unparse_decl = function
  | Decl (a, b) -> a ^ " " ^ b ^ ";\n"
  | Tdecl x -> x

and unparse_ast = 
  let rec unparse_plus = function
    | [] -> ""
    | (CUminus a :: b) -> " - " ^ (parenthesize a) ^ (unparse_plus b)
    | (a :: b) -> " + " ^ (parenthesize a) ^ (unparse_plus b)
  and parenthesize x = match x with
  | (CVar _) -> unparse_ast x
  | (CCall _) -> unparse_ast x
  | (Integer _) -> unparse_ast x
  | _ -> "(" ^ (unparse_ast x) ^ ")"

  in
  function
    | Asch a -> (unparse_annotated true a)
    | Simd_leavefun -> "" (* used only in SIMD code *)
    | Return x -> "return " ^ unparse_ast x ^ ";"
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
    | Binop (op, a, b) -> (unparse_ast a) ^ op ^ (unparse_ast b)
    | Expr_assign (a, b) -> (unparse_ast a) ^ " = " ^ (unparse_ast b)
    | Stmt_assign (a, b) -> (unparse_ast a) ^ " = " ^ (unparse_ast b) ^ ";\n"
    | Comma (a, b) -> (unparse_ast a) ^ ", " ^ (unparse_ast b)
    | Integer i -> string_of_int i
    | CVar s -> s
    | CCall (s, x) -> s ^ "(" ^ (unparse_ast x) ^ ")"
    | CPlus [] -> "0 /* bug */"
    | CPlus [a] -> " /* bug */ " ^ (unparse_ast a)
    | CPlus (a::b) -> (parenthesize a) ^ (unparse_plus b)
    | ITimes (a, b) -> (parenthesize a) ^ " * " ^ (parenthesize b)
    | CUminus a -> "- " ^ (parenthesize a)

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


(*************************************************************
 * traverse a a function and return a list of all expressions,
 * in the execution order
 **************************************************************)
let rec fcn_to_expr_list = fun (Fcn (_, _, _, body)) -> ast_to_expr_list body 
and acode_to_expr_list = function
    AInstr (Assign (_, x)) -> [x]
  | ASeq (a, b) -> 
      (asched_to_expr_list a) @ (asched_to_expr_list b)
  | _ -> []
and asched_to_expr_list (Annotate (_, _, _, _, code)) =
  acode_to_expr_list code
and ast_to_expr_list = function
    Asch a -> asched_to_expr_list a
  | Block (_, a) -> flatten (map ast_to_expr_list a)
  | For (_, _, _, body) ->  ast_to_expr_list body
  | If (_, body) ->  ast_to_expr_list body
  | _ -> []

(***********************
 * Extracting Constants
 ***********************)

(* add a new key & value to a list of (key,value) pairs, where
   the keys are floats and each key is unique up to almost_equal *)

let extract_constants f =
  let constlist = flatten (map expr_to_constants (ast_to_expr_list f))
  in map
       (fun n ->
	  Tdecl 
	    ("DK(" ^ (Number.to_konst n) ^ ", " ^ (Number.to_string n) ^ 
	       ");\n"))
       (unique_constants constlist)
       
(******************************
   Extracting operation counts 
 ******************************)

let count_stack_vars =
  let rec count_acode = function
    | ASeq (a, b) -> max (count_asched a) (count_asched b)
    | _ -> 0
  and count_asched (Annotate (_, _, decl, _, code)) =
    (length decl) + (count_acode code)
  and count_ast = function
    | Asch a -> count_asched a
    | Block (d, a) -> (length d) + (Util.max_list (map count_ast a))
    | For (_, _, _, body) -> count_ast body
    | If (_, body) -> count_ast body
    | _ -> 0
  in function (Fcn (_, _, _, body)) -> count_ast body

let count_memory_acc f =
  let rec count_var v =
    if (Variable.is_locative v)	then 1 else 0
  and count_acode = function
    | AInstr (Assign (v, _)) -> count_var v
    | ASeq (a, b) -> (count_asched a) + (count_asched b)
    | _ -> 0
  and count_asched = function
      Annotate (_, _, _, _, code) -> count_acode code
  and count_ast = function
    | Asch a -> count_asched a
    | Block (_, a) -> (Util.sum_list (map count_ast a))
    | Comma (a, b) -> (count_ast a) + (count_ast b)
    | For (_, _, _, body) -> count_ast body
    | If (_, body) -> count_ast body
    | _ -> 0
  and count_acc_expr_func acc = function
    | Load v -> acc + (count_var v)
    | Plus a -> fold_left count_acc_expr_func acc a
    | Times (a, b) -> fold_left count_acc_expr_func acc [a; b]
    | Uminus a -> count_acc_expr_func acc a
    | _ -> acc
  in let (Fcn (typ, name, args, body)) = f
  in (count_ast body) + 
    fold_left count_acc_expr_func 0 (fcn_to_expr_list f)

let good_for_fma = To_alist.good_for_fma

let build_fma = function
  | [a; Times (b, c)] when good_for_fma (b, c) -> Some (a, b, c)
  | [Times (b, c); a] when good_for_fma (b, c) -> Some (a, b, c)
  | [a; Uminus (Times (b, c))] when good_for_fma (b, c) -> Some (a, b, c)
  | [Uminus (Times (b, c)); a] when good_for_fma (b, c) -> Some (a, b, c)
  | _ -> None

let rec count_flops_expr_func (adds, mults, fmas) = function
  | Plus [] -> (adds, mults, fmas)
  | Plus ([_; _] as a) -> 
      begin
	match build_fma a with
	  | None ->
	      fold_left count_flops_expr_func 
		(adds + (length a) - 1, mults, fmas) a
	  | Some (a, b, c) ->
	      fold_left count_flops_expr_func (adds, mults, fmas+1) [a; b; c]
      end
  | Plus (a :: b) -> 
      count_flops_expr_func (adds, mults, fmas) (Plus [a; Plus b])
  | Times (NaN MULTI_A,_)  -> (adds, mults, fmas)
  | Times (NaN MULTI_B,_)  -> (adds, mults, fmas)
  | Times (NaN I,b) -> count_flops_expr_func (adds, mults, fmas) b
  | Times (NaN CONJ,b) -> count_flops_expr_func (adds, mults, fmas) b
  | Times (a,b) -> fold_left count_flops_expr_func (adds, mults+1, fmas) [a; b]
  | CTimes (a,b) -> 
      fold_left count_flops_expr_func (adds+1, mults+2, fmas) [a; b]
  | CTimesJ (a,b) -> 
      fold_left count_flops_expr_func (adds+1, mults+2, fmas) [a; b]
  | Uminus a -> count_flops_expr_func (adds, mults, fmas) a
  | _ -> (adds, mults, fmas)

let count_flops f = 
    fold_left count_flops_expr_func (0, 0, 0) (fcn_to_expr_list f)

let count_constants f = 
    length (unique_constants (flatten (map expr_to_constants (fcn_to_expr_list f))))

let arith_complexity f =
  let (a, m, fmas) = count_flops f
  and v = count_stack_vars f
  and c = count_constants f
  and mem = count_memory_acc f
  in (a, m, fmas, v, c, mem)

(* print the operation costs *)
let print_cost f =
  let Fcn (_, _, _, _) = f 
  and (a, m, fmas, v, c, mem) = arith_complexity f
  in
  "/*\n"^
  " * This function contains " ^
  (string_of_int (a + fmas)) ^ " FP additions, "  ^
  (string_of_int (m + fmas)) ^ " FP multiplications,\n" ^
  " * (or, " ^
  (string_of_int a) ^ " additions, "  ^
  (string_of_int m) ^ " multiplications, " ^
  (string_of_int fmas) ^ " fused multiply/add),\n" ^
  " * " ^ (string_of_int v) ^ " stack variables, " ^
  (string_of_int c) ^ " constants, and " ^
  (string_of_int mem) ^ " memory accesses\n" ^
  " */\n"

(*****************************************
 * functions that create C arrays 
 *****************************************)
type stride = 
  | SVar of string
  | SConst of string
  | SInteger of int
  | SNeg of stride

type sstride =
  | Simple of int
  | Constant of (string * int)
  | Composite of (string * int)
  | Negative of sstride

let rec simplify_stride stride i =
    match (stride, i) with
      (_, 0) -> Simple 0
    | (SInteger n, i) -> Simple (n * i)
    | (SConst s, i) -> Constant (s, i)
    | (SVar s, i) -> Composite (s, i)
    | (SNeg x, i) -> 
	match (simplify_stride x i) with
	| Negative y -> y
	| y -> Negative y
  
let rec cstride_to_string = function
  | Simple i -> string_of_int i
  | Constant (s, i) -> 
        if !Magic.lisp_syntax then
	  "(* " ^ s ^ " " ^ (string_of_int i) ^ ")"
	else
	  s ^ " * " ^ (string_of_int i)
  | Composite (s, i) -> 
        if !Magic.lisp_syntax then
	  "(* " ^ s ^ " " ^ (string_of_int i) ^ ")"
	else
	  "WS(" ^ s ^ ", " ^ (string_of_int i) ^ ")"
  | Negative x -> "-" ^ cstride_to_string x

let aref name index = 
  if !Magic.lisp_syntax then
    Printf.sprintf "(aref %s %s)"  name index
  else
    Printf.sprintf "%s[%s]"  name index

let array_subscript name stride k = 
  aref name (cstride_to_string (simplify_stride stride k))

let varray_subscript name vstride stride v i = 
  let vindex = simplify_stride vstride v
  and iindex = simplify_stride stride i
  in 
  let index = 
    match (vindex, iindex) with
      (Simple vi, Simple ii) -> string_of_int (vi + ii)
    | (Simple 0, x) -> cstride_to_string x
    | (x, Simple 0) -> cstride_to_string x
    | _ -> (cstride_to_string vindex) ^ " + " ^ (cstride_to_string iindex)
  in aref name index

let real_of s = "c_re(" ^ s ^ ")"
let imag_of s = "c_im(" ^ s ^ ")"

let flops_of f =
  let (add, mul, fma) = count_flops f in
  Printf.sprintf "{ %d, %d, %d, 0 }" add mul fma
