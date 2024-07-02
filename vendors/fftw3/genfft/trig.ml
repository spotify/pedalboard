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

(* trigonometric transforms *)
open Util

(* DFT of real input *)
let rdft sign n input =
  Fft.dft sign n (Complex.real @@ input)

(* DFT of hermitian input *)
let hdft sign n input =
  Fft.dft sign n (Complex.hermitian n input)

(* DFT real transform of vectors of two real numbers,
   multiplication by (NaN I), and summation *)
let dft_via_rdft sign n input =
  let f = rdft sign n input
  in fun i -> 
    Complex.plus
      [Complex.real (f i); 
       Complex.times (Complex.nan Expr.I) (Complex.imag (f i))]

(* Discrete Hartley Transform *)
let dht sign n input =
  let f = Fft.dft sign n (Complex.real @@ input) in
  (fun i ->
    Complex.plus [Complex.real (f i); Complex.imag (f i)])

let trigI n input = 
  let twon = 2 * n in
  let input' = Complex.hermitian twon input
  in
  Fft.dft 1 twon input'

let interleave_zero input = fun i -> 
  if (i mod 2) == 0
      then Complex.zero
  else
    input ((i - 1) / 2)

let trigII n input =
  let fourn = 4 * n in
  let input' = Complex.hermitian fourn (interleave_zero input)
  in
  Fft.dft 1 fourn input'

let trigIII n input =
  let fourn = 4 * n in
  let twon = 2 * n in
  let input' = Complex.hermitian fourn
      (fun i -> 
	if (i == 0) then
	  Complex.real (input 0)
	else if (i == twon) then
	  Complex.uminus (Complex.real (input 0))
	else
	  Complex.antihermitian twon input i)
  in
  let dft = Fft.dft 1 fourn input'
  in fun k -> dft (2 * k + 1)

let zero_extend n input = fun i ->
  if (i >= 0 && i < n)
  then input i
  else Complex.zero

let trigIV n input =
  let fourn = 4 * n
  and eightn = 8 * n in
  let input' = Complex.hermitian eightn 
      (zero_extend fourn (Complex.antihermitian fourn 
			 (interleave_zero input)))
  in
  let dft = Fft.dft 1 eightn input'
  in fun k -> dft (2 * k + 1)

let make_dct scale nshift trig =
  fun n input ->
    trig (n - nshift) (Complex.real @@ (Complex.times scale) @@ 
		       (zero_extend n input))
(*
 * DCT-I:  y[k] = sum x[j] cos(pi * j * k / n)
 *)
let dctI = make_dct Complex.one 1 trigI

(*
 * DCT-II:  y[k] = sum x[j] cos(pi * (j + 1/2) * k / n)
 *)
let dctII = make_dct Complex.one 0 trigII

(*
 * DCT-III:  y[k] = sum x[j] cos(pi * j * (k + 1/2) / n)
 *)
let dctIII = make_dct Complex.half 0 trigIII

(*
 * DCT-IV  y[k] = sum x[j] cos(pi * (j + 1/2) * (k + 1/2) / n)
 *)
let dctIV = make_dct Complex.half 0 trigIV

let shift s input = fun i -> input (i - s)

(* DST-x input := TRIG-x (input / i) *)
let make_dst scale nshift kshift jshift trig =
  fun n input ->
    Complex.real @@ 
    (shift (- jshift)
       (trig (n + nshift) (Complex.uminus @@
			   (Complex.times Complex.i) @@
			   (Complex.times scale) @@ 
			   Complex.real @@ 
			   (shift kshift (zero_extend n input)))))

(*
 * DST-I:  y[k] = sum x[j] sin(pi * j * k / n)
 *)
let dstI = make_dst Complex.one 1 1 1 trigI

(*
 * DST-II:  y[k] = sum x[j] sin(pi * (j + 1/2) * k / n)
 *)
let dstII = make_dst Complex.one 0 0 1 trigII

(*
 * DST-III:  y[k] = sum x[j] sin(pi * j * (k + 1/2) / n)
 *)
let dstIII = make_dst Complex.half 0 1 0 trigIII

(*
 * DST-IV  y[k] = sum x[j] sin(pi * (j + 1/2) * (k + 1/2) / n)
 *)
let dstIV = make_dst Complex.half 0 0 0 trigIV

