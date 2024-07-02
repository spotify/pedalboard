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

(* The generator keeps track of numeric constants in symbolic
   expressions using the abstract number type, defined in this file.

   Our implementation of the number type uses arbitrary-precision
   arithmetic from the built-in Num package in order to maintain an
   accurate representation of constants.  This allows us to output
   constants with many decimal places in the generated C code,
   ensuring that we will take advantage of the full precision
   available on current and future machines.

   Note that we have to write our own routine to compute roots of
   unity, since the Num package only supplies simple arithmetic.  The
   arbitrary-precision operations in Num look like the normal
   operations except that they have an appended slash (e.g. +/ -/ */
   // etcetera). *)

open Num

type number = N of num

let makeNum n = N n

(* decimal digits of precision to maintain internally, and to print out: *)
let precision = 50
let print_precision = 45

let inveps = (Int 10) **/ (Int precision)
let epsilon = (Int 1) // inveps

let pinveps = (Int 10) **/ (Int print_precision)
let pepsilon = (Int 1) // pinveps

let round x = epsilon */ (round_num (x */ inveps))

let of_int n = N (Int n)
let zero = of_int 0
let one = of_int 1
let two = of_int 2
let mone = of_int (-1)

(* comparison predicate for real numbers *)
let equal (N x) (N y) = (* use both relative and absolute error *)
  let absdiff = abs_num (x -/ y) in
  absdiff <=/ pepsilon ||
  absdiff <=/ pepsilon */ (abs_num x +/ abs_num y)

let is_zero = equal zero
let is_one = equal one
let is_mone = equal mone
let is_two = equal two


(* Note that, in the following computations, it is important to round
   to precision epsilon after each operation.  Otherwise, since the
   Num package uses exact rational arithmetic, the number of digits
   quickly blows up. *)
let mul (N a) (N b) = makeNum (round (a */ b)) 
let div (N a) (N b) = makeNum (round (a // b))
let add (N a) (N b) = makeNum (round (a +/ b)) 
let sub (N a) (N b) = makeNum (round (a -/ b))

let negative (N a) = (a </ (Int 0))
let negate (N a) = makeNum (minus_num a)

let greater a b = negative (sub b a)

let epsilonsq = epsilon */ epsilon
let epsilonsq2 =  (Int 100) */ epsilonsq

let sqr a = a */ a
let almost_equal (N a) (N b) = (sqr (a -/ b)) <=/ epsilonsq2

(* find square root by Newton's method *)
let sqrt a =
  let rec sqrt_iter guess =
    let newguess = div (add guess (div a guess)) two in
    if (almost_equal newguess guess) then newguess
    else sqrt_iter newguess
  in sqrt_iter (div a two)

let csub (xr, xi) (yr, yi) = (round (xr -/ yr), round (xi -/ yi))
let cdiv (xr, xi) r = (round (xr // r), round (xi // r))
let cmul (xr, xi) (yr, yi) = (round (xr */ yr -/ xi */ yi),
                              round (xr */ yi +/ xi */ yr))
let csqr (xr, xi) = (round (xr */ xr -/ xi */ xi), round ((Int 2) */ xr */ xi))
let cabssq (xr, xi) = xr */ xr +/ xi */ xi
let cconj (xr, xi) = (xr, minus_num xi)
let cinv x = cdiv (cconj x) (cabssq x)

let almost_equal_cnum (xr, xi) (yr, yi) =
    (cabssq (xr -/ yr,xi -/ yi)) <=/ epsilonsq2

(* Put a complex number to an integer power by repeated squaring: *)
let rec ipow_cnum x n =
    if (n == 0) then
      (Int 1, Int 0)
    else if (n < 0) then
      cinv (ipow_cnum x (- n))
    else if (n mod 2 == 0) then
      ipow_cnum (csqr x) (n / 2)
    else
      cmul x (ipow_cnum x (n - 1))

let twopi = 6.28318530717958647692528676655900576839433879875021164194989

(* Find the nth (complex) primitive root of unity by Newton's method: *)
let primitive_root_of_unity n =
    let rec root_iter guess =
        let newguess = csub guess (cdiv (csub guess
                                         (ipow_cnum guess (1 - n)))
                                   (Int n)) in
            if (almost_equal_cnum guess newguess) then newguess
            else root_iter newguess
    in let float_to_num f = (Int (truncate (f *. 1.0e9))) // (Int 1000000000)
    in root_iter (float_to_num (cos (twopi /. (float n))),
		  float_to_num (sin (twopi /. (float n)))) 

let cexp n i =
    if ((i mod n) == 0) then
      (one,zero)
    else
      let (n2,i2) = Util.lowest_terms n i
      in let (c,s) = ipow_cnum (primitive_root_of_unity n2) i2
      in (makeNum c, makeNum s)

let to_konst (N n) =
  let f = float_of_num n in
  let f' = if f < 0.0 then f *. (-1.0) else f in
  let f2 = if (f' >= 1.0) then (f' -. (float (truncate f'))) else f'
  in let q = string_of_int (truncate(f2 *. 1.0E9))
  in let r = "0000000000" ^ q
  in let l = String.length r 
  in let prefix = if (f < 0.0) then "KN" else "KP" in
  if (f' >= 1.0) then
    (prefix ^ (string_of_int (truncate f')) ^ "_" ^ 
     (String.sub r (l - 9) 9))
  else
    (prefix ^ (String.sub r (l - 9) 9))

let to_string (N n) = approx_num_fix print_precision n

let to_float (N n) = float_of_num n

