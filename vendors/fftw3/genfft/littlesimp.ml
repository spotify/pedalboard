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
 * The LittleSimplifier module implements a subset of the simplifications
 * of the AlgSimp module.  These simplifications can be executed
 * quickly here, while they would take a long time using the heavy
 * machinery of AlgSimp.  
 * 
 * For example, 0 * x is simplified to 0 tout court by the LittleSimplifier.
 * On the other hand, AlgSimp would first simplify x, generating lots
 * of common subexpressions, storing them in a table etc, just to
 * discard all the work later.  Similarly, the LittleSimplifier
 * reduces the constant FFT in Rader's algorithm to a constant sequence.
 *)

open Expr

let rec makeNum = function
  | n -> Num n

and makeUminus = function
  | Uminus a -> a 
  | Num a -> makeNum (Number.negate a)
  | a -> Uminus a

and makeTimes = function
  | (Num a, Num b) -> makeNum (Number.mul a b)
  | (Num a, Times (Num b, c)) -> makeTimes (makeNum (Number.mul a b), c)
  | (Num a, b) when Number.is_zero a -> makeNum (Number.zero)
  | (Num a, b) when Number.is_one a -> b
  | (Num a, b) when Number.is_mone a -> makeUminus b
  | (Num a, Uminus b) -> Times (makeUminus (Num a), b)
  | (a, (Num b as b')) -> makeTimes (b', a)
  | (a, b) -> Times (a, b)

and makePlus l = 
  let rec reduceSum x = match x with
    [] -> []
  | [Num a] -> if Number.is_zero a then [] else x
  | (Num a) :: (Num b) :: c -> 
      reduceSum ((makeNum (Number.add a b)) :: c)
  | ((Num _) as a') :: b :: c -> b :: reduceSum (a' :: c)
  | a :: s -> a :: reduceSum s

  in match reduceSum l with
    [] -> makeNum (Number.zero)
  | [a] -> a 
  | [a; b] when a == b -> makeTimes (Num Number.two, a)
  | [Times (Num a, b); Times (Num c, d)] when b == d ->
      makeTimes (makePlus [Num a; Num c], b)
  | a -> Plus a

