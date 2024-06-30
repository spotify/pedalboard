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

open Util

type color = | RED | BLUE | BLACK | YELLOW

type dagnode = 
    { assigned: Variable.variable;
      mutable expression: Expr.expr;
      input_variables: Variable.variable list;
      mutable successors: dagnode list;
      mutable predecessors: dagnode list;
      mutable label: int;
      mutable color: color}

type dag

val makedag : (Variable.variable * Expr.expr) list -> dag

val map : (dagnode -> dagnode) -> dag -> dag
val for_all : dag -> (dagnode -> unit) -> unit
val to_list : dag -> (dagnode list)
val bfs : dag -> dagnode -> int -> unit
val find_node : (dagnode -> bool) -> dag -> dagnode option
