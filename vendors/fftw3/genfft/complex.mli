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

type expr
val make : (Expr.expr * Expr.expr) -> expr
val two : expr
val one : expr
val i : expr
val zero : expr
val half : expr
val inverse_int : int -> expr
val inverse_int_sqrt : int -> expr
val int_sqrt : int -> expr
val times : expr -> expr -> expr
val ctimes : expr -> expr -> expr
val ctimesj : expr -> expr -> expr
val uminus : expr -> expr
val exp : int -> int -> expr
val sec : int -> int -> expr
val csc : int -> int -> expr
val tan : int -> int -> expr
val cot : int -> int -> expr
val plus : expr list -> expr
val real : expr -> expr
val imag : expr -> expr
val conj : expr -> expr
val nan : Expr.transcendent -> expr
val sigma : int -> int -> (int -> expr) -> expr

val (@*) : expr -> expr -> expr
val (@+) : expr -> expr -> expr
val (@-) : expr -> expr -> expr

(* a signal is a map from integers to expressions *)
type signal = int -> expr
val infinite : int -> signal -> signal

val store_real : Variable.variable -> expr -> Expr.expr
val store_imag : Variable.variable -> expr -> Expr.expr
val store :
  Variable.variable * Variable.variable -> expr -> Expr.expr * Expr.expr

val assign_real : Variable.variable -> expr -> Expr.assignment
val assign_imag : Variable.variable -> expr -> Expr.assignment
val assign :
  Variable.variable * Variable.variable ->
  expr -> Expr.assignment * Expr.assignment

val hermitian : int -> (int -> expr) -> int -> expr
val antihermitian : int -> (int -> expr) -> int -> expr
