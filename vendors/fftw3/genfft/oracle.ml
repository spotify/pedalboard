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
 * the oracle decrees whether the sign of an expression should
 * be changed.
 *
 * Say the expression (A - B) appears somewhere.  Elsewhere in the
 * expression dag the expression (B - A) may appear.
 * The oracle determines which of the two forms is canonical.
 *
 * Algorithm: evaluate the expression at a random input, and
 * keep the expression with the positive sign.
 *)

let make_memoizer hash equal =
  let table = ref Assoctable.empty 
  in 
  (fun f k ->
    match Assoctable.lookup hash equal k !table with
      Some value -> value
    | None ->
        let value = f k in
        begin	
          table := Assoctable.insert hash k value !table;
          value
        end)

let almost_equal x y = 
  let epsilon = 1.0E-8 in
  (abs_float (x -. y) < epsilon) ||
  (abs_float (x -. y) < epsilon *. (abs_float x +. abs_float y)) 

let absid = make_memoizer
    (fun x -> Expr.hash_float (abs_float x))
    (fun a b -> almost_equal a b || almost_equal (-. a) b)
    (fun x -> x)

let make_random_oracle () = make_memoizer 
    Variable.hash 
    Variable.same
    (fun _ -> (float (Random.bits())) /. 1073741824.0)

let the_random_oracle = make_random_oracle ()

let sum_list l = List.fold_right (+.) l 0.0

let eval_aux random_oracle =
  let memoizing = make_memoizer Expr.hash (==) in
  let rec eval x = 
    memoizing
      (function
	| Expr.Num x -> Number.to_float x
	| Expr.NaN x -> Expr.transcendent_to_float x
	| Expr.Load v -> random_oracle v
	| Expr.Store (v, x) -> eval x
	| Expr.Plus l -> sum_list (List.map eval l)
	| Expr.Times (a, b) -> (eval a) *. (eval b)
	| Expr.CTimes (a, b) -> 
	    1.098612288668109691395245236 +. 
	       1.609437912434100374600759333 *. (eval a) *. (eval b)
	| Expr.CTimesJ (a, b) -> 
	    0.9102392266268373936142401657 +. 
	      0.6213349345596118107071993881 *. (eval a) *. (eval b)
	| Expr.Uminus x -> -. (eval x))
      x
  in eval

let eval = eval_aux the_random_oracle

let should_flip_sign node = 
  let v = eval node in
  let v' = absid v in
  not (almost_equal v v')

(*
 * determine with high probability if two expressions are equal.
 *
 * The test is randomized: if the two expressions have the
 * same value for NTESTS random inputs, then they are proclaimed
 * equal.  (Note that two distinct linear functions L1(x0, x1, ..., xn)
 * and L2(x0, x1, ..., xn) have the same value with probability
 * 0 for random x's, and thus this test is way more paranoid than
 * necessary.)
 *)
let likely_equal a b =
  let tolerance = 1.0e-8
  and ntests = 20
  in
  let rec loop n =
    if n = 0 then 
      true
    else
      let r = make_random_oracle () in
      let va = eval_aux r a
      and vb = eval_aux r b
      in
      if (abs_float (va -. vb)) > 
	   tolerance *. (abs_float va +. abs_float vb +. 0.0001)
      then
	false
      else
	loop (n - 1)
  in
  match (a, b) with

    (* 
     * Because of the way eval is constructed, we have
     *     eval (Store (v, x)) == eval x
     * However, we never consider the two expressions equal
     *)
  | (Expr.Store _, _) -> false
  | (_, Expr.Store _) -> false

    (*
     * Expressions of the form ``Uminus (Store _)''
     * are artifacts of algsimp
     *)
  | ((Expr.Uminus (Expr.Store _)), _) -> false
  | (_, Expr.Uminus (Expr.Store _)) -> false

  | _ -> loop ntests

let hash x =
  let f = eval x in
  truncate (f *. 65536.0)
