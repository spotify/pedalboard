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

open Complex
open Util

let polyphase m a ph i = a (m * i + ph)

let rec divmod n i =
  if (i < 0) then 
    let (a, b) = divmod n (i + n)
    in (a - 1, b)
  else (i / n, i mod n)

let unpolyphase m a i = let (x, y) = divmod m i in a y x

let lift2 f a b i = f (a i) (b i)

(* convolution of signals A and B *)
let rec conv na a nb b =
  let rec naive na a nb b i =
    sigma 0 na (fun j -> (a j) @* (b (i - j)))

  and recur na a nb b =
    if (na <= 1 || nb <= 1) then
      naive na a nb b
    else
      let p = polyphase 2 in
      let ee = conv (na - na / 2) (p a 0) (nb - nb / 2) (p b 0)
      and eo = conv (na - na / 2) (p a 0) (nb / 2) (p b 1)
      and oe = conv (na / 2) (p a 1) (nb - nb / 2) (p b 0)
      and oo = conv (na / 2) (p a 1) (nb / 2) (p b 1) in
      unpolyphase 2 (function
	  0 -> fun i -> (ee i) @+ (oo (i - 1))
	| 1 -> fun i -> (eo i) @+ (oe i) 
	| _ -> failwith "recur")


  (* Karatsuba variant 1: (a+bx)(c+dx) = (ac+bdxx)+((a+b)(c+d)-ac-bd)x *)
  and karatsuba1 na a nb b =
      let p = polyphase 2 in
      let ae = p a 0 and nae = na - na / 2
      and ao = p a 1 and nao = na / 2
      and be = p b 0 and nbe = nb - nb / 2
      and bo = p b 1 and nbo = nb / 2 in
      let ae = infinite nae ae and ao = infinite nao ao
      and be = infinite nbe be and bo = infinite nbo bo in
      let aeo = lift2 (@+) ae ao and naeo = nae
      and beo = lift2 (@+) be bo and nbeo = nbe in
      let ee = conv nae ae nbe be 
      and oo = conv nao ao nbo bo
      and eoeo = conv naeo aeo nbeo beo in

      let q = function
	  0 -> fun i -> (ee i)  @+ (oo (i - 1))
	| 1 -> fun i -> (eoeo i) @- ((ee i) @+ (oo i))
	| _ -> failwith "karatsuba1" in
      unpolyphase 2 q

  (* Karatsuba variant 2: 
     (a+bx)(c+dx) = ((a+b)c-b(c-dxx))+x((a+b)c-a(c-d)) *)
  and karatsuba2 na a nb b =
      let p = polyphase 2 in
      let ae = p a 0 and nae = na - na / 2
      and ao = p a 1 and nao = na / 2
      and be = p b 0 and nbe = nb - nb / 2
      and bo = p b 1 and nbo = nb / 2 in
      let ae = infinite nae ae and ao = infinite nao ao
      and be = infinite nbe be and bo = infinite nbo bo in

      let c1 = conv nae (lift2 (@+) ae ao) nbe be
      and c2 = conv nao ao (nbo + 1) (fun i -> be i @- bo (i - 1))
      and c3 = conv nae ae nbe (lift2 (@-) be bo) in

      let q = function
	  0 -> lift2 (@-) c1 c2
	| 1 -> lift2 (@-) c1 c3
	| _ -> failwith "karatsuba2" in
      unpolyphase 2 q

  and karatsuba na a nb b =
    let m = na + nb - 1 in
    if (m < !Magic.karatsuba_min) then
      recur na a nb b
    else
      match !Magic.karatsuba_variant with
	1 -> karatsuba1 na a nb b
      |	2 -> karatsuba2 na a nb b
      |	_ -> failwith "unknown karatsuba variant"

  and via_circular na a nb b =
    let m = na + nb - 1 in
    if (m < !Magic.circular_min) then
      karatsuba na a nb b
    else
      let rec find_min n = if n >= m then n else find_min (2 * n) in
      circular (find_min 1) a b

  in
  let a = infinite na a and b = infinite nb b in
  let res = array (na + nb - 1) (via_circular na a nb b) in
  infinite (na + nb - 1) res
    
and circular n a b =
  let via_dft n a b =
    let fa = Fft.dft (-1) n a 
    and fb = Fft.dft (-1) n b
    and scale = inverse_int n in
    let fab i = ((fa i) @* (fb i)) @* scale in
    Fft.dft 1 n fab

  in via_dft n a b
