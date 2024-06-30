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

(* policies for loading/computing twiddle factors *)
open Complex
open Util

type twop = TW_FULL | TW_CEXP | TW_NEXT

let optostring = function
  | TW_CEXP -> "TW_CEXP"
  | TW_NEXT -> "TW_NEXT"
  | TW_FULL -> "TW_FULL"

type twinstr = (twop * int * int)

let rec unroll_twfull l = match l with
| [] -> []
| (TW_FULL, v, n) :: b ->
    (forall [] cons 1 n (fun i -> (TW_CEXP, v, i)))
    @ unroll_twfull b
| a :: b -> a :: unroll_twfull b

let twinstr_to_c_string l =
  let one (op, a, b) = Printf.sprintf "{ %s, %d, %d }" (optostring op) a b
  in let rec loop first = function
    | [] -> ""
    | a :: b ->  (if first then "\n" else ",\n") ^ (one a) ^ (loop false b)
  in "{" ^ (loop true l) ^ "}"

let twinstr_to_simd_string vl l =
  let one sep = function
    | (TW_NEXT, 1, 0) -> sep ^ "{TW_NEXT, " ^ vl ^ ", 0}"
    | (TW_NEXT, _, _) -> failwith "twinstr_to_simd_string"
    | (TW_CEXP, v, b) -> sep ^ (Printf.sprintf "VTW(%d,%d)" v b)
    | _ -> failwith "twinstr_to_simd_string"
  in let rec loop first = function
    | [] -> ""
    | a :: b ->  (one (if first then "\n" else ",\n") a) ^ (loop false b)
  in "{" ^ (loop true (unroll_twfull l)) ^ "}"
  
let rec pow m n =
  if (n = 0) then 1
  else m * pow m (n - 1)

let rec is_pow m n =
  n = 1 || ((n mod m) = 0 && is_pow m (n / m))

let rec log m n = if n = 1 then 0 else 1 + log m (n / m)

let rec largest_power_smaller_than m i =
  if (is_pow m i) then i
  else largest_power_smaller_than m (i - 1)

let rec smallest_power_larger_than m i =
  if (is_pow m i) then i
  else smallest_power_larger_than m (i + 1)

let rec_array n f =
  let g = ref (fun i -> Complex.zero) in
  let a = Array.init n (fun i -> lazy (!g i)) in
  let h i = f (fun i -> Lazy.force a.(i)) i in
  begin
    g := h;
    h
  end

 
let ctimes use_complex_arith a b =
  if use_complex_arith then
    Complex.ctimes a b
  else
    Complex.times a b

let ctimesj use_complex_arith a b =
  if use_complex_arith then
    Complex.ctimesj a b
  else
    Complex.times (Complex.conj a) b

let make_bytwiddle sign use_complex_arith g f i =
  if i = 0 then 
    f i
  else if sign = 1 then 
    ctimes use_complex_arith (g i) (f i)
  else
    ctimesj use_complex_arith (g i) (f i)

(* various policies for computing/loading twiddle factors *)

let twiddle_policy_load_all v use_complex_arith =
  let bytwiddle n sign w f =
    make_bytwiddle sign use_complex_arith (fun i -> w (i - 1)) f
  and twidlen n = 2 * (n - 1)
  and twdesc r = [(TW_FULL, v, r);(TW_NEXT, 1, 0)]
  in bytwiddle, twidlen, twdesc

(*
 * if i is a power of two, then load w (log i)
 * else let x = largest power of 2 less than i in
 *      let y = i - x in
 *      compute w^{x+y} = w^x * w^y
 *)
let twiddle_policy_log2 v use_complex_arith =
  let bytwiddle n sign w f =
    let g = rec_array n (fun self i ->
      if i = 0 then Complex.one
      else if is_pow 2 i then w (log 2 i)
      else let x = largest_power_smaller_than 2 i in
      let y = i - x in
	ctimes use_complex_arith (self x) (self y))
    in make_bytwiddle sign use_complex_arith g f
  and twidlen n = 2 * (log 2 (largest_power_smaller_than 2 (2 * n - 1)))
  and twdesc n =
    (List.flatten 
       (List.map 
	  (fun i -> 
	    if i > 0 && is_pow 2 i then 
	      [TW_CEXP, v, i] 
	    else 
	      [])
	  (iota n)))
    @ [(TW_NEXT, 1, 0)]
  in bytwiddle, twidlen, twdesc

let twiddle_policy_log3 v use_complex_arith =
  let rec terms_needed i pi s n =
    if (s >= n - 1) then i
    else terms_needed (i + 1) (3 * pi) (s + pi) n
  in
  let rec bytwiddle n sign w f =
    let nterms = terms_needed 0 1 0 n in
    let maxterm = pow 3 (nterms - 1) in
    let g = rec_array (3 * n) (fun self i ->
      if i = 0 then Complex.one
      else if is_pow 3 i then w (log 3 i)
      else if i = (n - 1) && maxterm >= n then
	w (nterms - 1)
      else let x = smallest_power_larger_than 3 i in
      if (i + i >= x) then
	let x = min x (n - 1) in
	  ctimesj use_complex_arith (self (x - i)) (self x)
      else let x = largest_power_smaller_than 3 i in
	ctimes use_complex_arith (self (i - x)) (self x))
    in make_bytwiddle sign use_complex_arith g f
  and twidlen n = 2 * (terms_needed 0 1 0 n)
  and twdesc n =
    (List.map 
       (fun i -> 
	  let x = min (pow 3 i) (n - 1) in
	    TW_CEXP, v, x)
       (iota ((twidlen n) / 2)))
    @ [(TW_NEXT, 1, 0)]
  in bytwiddle, twidlen, twdesc
    
let current_twiddle_policy = ref twiddle_policy_load_all

let twiddle_policy use_complex_arith = 
  !current_twiddle_policy use_complex_arith

let set_policy x = Arg.Unit (fun () -> current_twiddle_policy := x)
let set_policy_int x = Arg.Int (fun i -> current_twiddle_policy := x i)

let undocumented = " Undocumented twiddle policy"

let speclist = [
  "-twiddle-load-all", set_policy twiddle_policy_load_all, undocumented;
  "-twiddle-log2", set_policy twiddle_policy_log2, undocumented;
  "-twiddle-log3", set_policy twiddle_policy_log3, undocumented;
] 
