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

type number

val equal : number -> number -> bool
val of_int : int -> number
val zero : number
val one : number
val two : number
val mone : number
val is_zero : number -> bool
val is_one : number -> bool
val is_mone : number -> bool
val is_two : number -> bool
val mul : number -> number -> number
val div : number -> number -> number
val add : number -> number -> number
val sub : number -> number -> number
val negative : number -> bool
val greater : number -> number -> bool
val negate : number -> number
val sqrt : number -> number

(* cexp n i = (cos (2 * pi * i / n), sin (2 * pi * i / n)) *)
val cexp : int -> int -> (number * number)

val to_konst : number -> string
val to_string : number -> string
val to_float : number -> float

