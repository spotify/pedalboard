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

(* utilities common to all generators *)
open Util

let choose_simd a b = if !Simdmagic.simd_mode then b else a

let unique_array n = array n (fun _ -> Unique.make ())
let unique_array_c n = 
  array n (fun _ -> 
    (Unique.make (), Unique.make ()))

let unique_v_array_c veclen n = 
  array veclen (fun _ ->
    unique_array_c n)

let locative_array_c n rarr iarr loc vs = 
  array n (fun i -> 
    let klass = Unique.make () in
    let (rloc, iloc) = loc i in
    (Variable.make_locative rloc klass rarr i vs,
     Variable.make_locative iloc klass iarr i vs))

let locative_v_array_c veclen n rarr iarr loc vs = 
  array veclen (fun v ->
    array n (fun i -> 
      let klass = Unique.make () in
      let (rloc, iloc) = loc v i in
      (Variable.make_locative rloc klass (rarr v) i vs,
       Variable.make_locative iloc klass (iarr v) i vs)))

let temporary_array n = 
  array n (fun i -> Variable.make_temporary ())

let temporary_array_c n = 
  let tmpr = temporary_array n
  and tmpi = temporary_array n
  in 
  array n (fun i -> (tmpr i, tmpi i))

let temporary_v_array_c veclen n =
  array veclen (fun v -> temporary_array_c n)

let temporary_array_c n = 
  let tmpr = temporary_array n
  and tmpi = temporary_array n
  in 
  array n (fun i -> (tmpr i, tmpi i))

let load_c (vr, vi) = Complex.make (Expr.Load vr, Expr.Load vi)
let load_r (vr, vi) = Complex.make (Expr.Load vr, Expr.Num (Number.zero))

let twiddle_array nt w =
  array (nt/2) (fun i ->
    let stride = choose_simd (C.SInteger 1) (C.SConst "TWVL") 
    and klass = Unique.make () in
    let (refr, refi) = (C.array_subscript w stride (2 * i),
			C.array_subscript w stride (2 * i + 1))
    in
    let (kr, ki) = (Variable.make_constant klass refr,
		    Variable.make_constant klass refi)  
    in
    load_c (kr, ki))


let load_array_c n var = array n (fun i -> load_c (var i))
let load_array_r n var = array n (fun i -> load_r (var i))
let load_array_hc n var = 
  array n (fun i -> 
    if (i < n - i) then
      load_c (var i)
    else if (i > n - i) then
      Complex.times Complex.i (load_c (var (n - i)))
    else
      load_r (var i))

let load_v_array_c veclen n var =
  array veclen (fun v -> load_array_c n (var v))

let store_c (vr, vi) x = [Complex.store_real vr x; Complex.store_imag vi x]
let store_r (vr, vi) x = Complex.store_real vr x
let store_i (vr, vi) x = Complex.store_imag vi x

let assign_array_c n dst src =
  List.flatten
    (rmap (iota n)
       (fun i ->
	 let (ar, ai) = Complex.assign (dst i) (src i)
	 in [ar; ai]))
let assign_v_array_c veclen n dst src =
  List.flatten
    (rmap (iota veclen)
       (fun v ->
	 assign_array_c n (dst v) (src v)))

let vassign_v_array_c veclen n dst src =
  List.flatten
    (rmap (iota n) (fun i ->
      List.flatten
	(rmap (iota veclen)
	   (fun v ->
	     let (ar, ai) = Complex.assign (dst v i) (src v i)
	     in [ar; ai]))))

let store_array_r n dst src =
  rmap (iota n)
    (fun i -> store_r (dst i) (src i))

let store_array_c n dst src =
  List.flatten
    (rmap (iota n)
       (fun i -> store_c (dst i) (src i)))

let store_array_hc n dst src =
  List.flatten
    (rmap (iota n)
       (fun i -> 
	 if (i < n - i) then
	   store_c (dst i) (src i)
	 else if (i > n - i) then
	   []
	 else 
	   [store_r (dst i) (Complex.real (src i))]))
	

let store_v_array_c veclen n dst src =
  List.flatten
    (rmap (iota veclen)
       (fun v ->
	 store_array_c n (dst v) (src v)))


let elementwise f n a = array n (fun i -> f (a i))
let conj_array_c = elementwise Complex.conj
let real_array_c = elementwise Complex.real
let imag_array_c = elementwise Complex.imag

let elementwise_v f veclen n a = 
  array veclen (fun v ->
    array n (fun i -> f (a v i)))
let conj_v_array_c = elementwise_v Complex.conj
let real_v_array_c = elementwise_v Complex.real
let imag_v_array_c = elementwise_v Complex.imag


let transpose f i j = f j i
let symmetrize f i j = if i <= j then f i j else f j i

(* utilities for command-line parsing *)
let standard_arg_parse_fail _ = failwith "too many arguments"

let dump_dag alist =
  let fnam = !Magic.dag_dump_file in
  if (String.length fnam > 0) then
    let ochan = open_out fnam in
    begin
      To_alist.dump (output_string ochan) alist;
      close_out ochan;
    end

let dump_alist alist =
  let fnam = !Magic.alist_dump_file in
  if (String.length fnam > 0) then
    let ochan = open_out fnam in
    begin
      Expr.dump (output_string ochan) alist;
      close_out ochan;
    end

let dump_asched asched =
  let fnam = !Magic.asched_dump_file in
  if (String.length fnam > 0) then
    let ochan = open_out fnam in
    begin
      Annotate.dump (output_string ochan) asched;
      close_out ochan;
    end

(* utilities for optimization *)
let standard_scheduler dag =
  let optim = Algsimp.algsimp dag in
  let alist = To_alist.to_assignments optim in
  let _ = dump_alist alist in
  let _ = dump_dag alist in
    if !Magic.precompute_twiddles then
      Schedule.isolate_precomputations_and_schedule alist 
    else
      Schedule.schedule alist 

let standard_optimizer dag =
  let sched = standard_scheduler dag in
  let annot = Annotate.annotate [] sched in
  let _ = dump_asched annot in
  annot

let size = ref None
let sign = ref (-1)

let speclist = [
  "-n", Arg.Int(fun i -> size := Some i), " generate a codelet of size <n>";
  "-sign",
  Arg.Int(fun i -> 
    if (i > 0) then
      sign := 1
    else
      sign := (-1)),
  " sign of transform";
]

let check_size () =
  match !size with
  | Some i -> i
  | None -> failwith "must specify -n"

let expand_name name = if name = "" then "noname" else name

let declare_register_fcn name =
  if name = "" then
    "void NAME(planner *p)\n"
  else 
    "void " ^ (choose_simd "X" "XSIMD") ^
      "(codelet_" ^ name ^ ")(planner *p)\n"

let stringify name = 
  if name = "" then "STRINGIZE(NAME)" else 
    choose_simd ("\"" ^ name ^ "\"")
      ("XSIMD_STRING(\"" ^ name ^ "\")")

let parse user_speclist usage =
  Arg.parse
    (user_speclist @ speclist @ Magic.speclist @ Simdmagic.speclist)
    standard_arg_parse_fail
    usage

let rec list_to_c = function
    [] -> ""
  | [a] -> (string_of_int a)
  | a :: b -> (string_of_int a) ^ ", " ^ (list_to_c b)

let rec list_to_comma = function
  | [a; b] -> C.Comma (a, b)
  | a :: b -> C.Comma (a, list_to_comma b)
  | _ -> failwith "list_to_comma"


type stride = Stride_variable | Fixed_int of int | Fixed_string of string

let either_stride a b =
  match a with
    Fixed_int x -> C.SInteger x
  | Fixed_string x -> C.SConst x
  | _ -> b

let stride_fixed = function
    Stride_variable -> false
  | _ -> true

let arg_to_stride s =
  try
    Fixed_int (int_of_string s)
  with Failure "int_of_string" -> 
    Fixed_string s

let stride_to_solverparm = function
    Stride_variable -> "0"
  | Fixed_int x -> string_of_int x
  | Fixed_string x -> x

let stride_to_string s = function
    Stride_variable -> s
  | Fixed_int x -> string_of_int x
  | Fixed_string x -> x

(* output the command line *)
let cmdline () =
  List.fold_right (fun a b -> a ^ " " ^ b) (Array.to_list Sys.argv) ""

let unparse tree =
  "/* Generated by: " ^ (cmdline ()) ^ "*/\n\n" ^
  (C.print_cost tree) ^
  (if String.length !Magic.inklude > 0 
  then
    (Printf.sprintf "#include \"%s\"\n\n" !Magic.inklude)
  else "") ^
  (if !Simdmagic.simd_mode then
    Simd.unparse_function tree
  else
    C.unparse_function tree)

let finalize_fcn ast = 
  let mergedecls = function
      C.Block (d1, [C.Block (d2, s)]) -> C.Block (d1 @ d2, s)
    | x -> x
  and extract_constants =
    if !Simdmagic.simd_mode then 
      Simd.extract_constants 
    else
      C.extract_constants
	
  in mergedecls (C.Block (extract_constants ast, [ast; C.Simd_leavefun]))

let twinstr_to_string vl x =
  if !Simdmagic.simd_mode then 
    Twiddle.twinstr_to_simd_string vl x
  else
    Twiddle.twinstr_to_c_string x

let make_volatile_stride n x = 
  C.CCall ("MAKE_VOLATILE_STRIDE", C.Comma((C.Integer n), x))
