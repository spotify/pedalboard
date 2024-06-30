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

(* Here, we have functions to transform a sequence of assignments
   (variable = expression) into a DAG (a directed, acyclic graph).
   The nodes of the DAG are the assignments, and the edges indicate
   dependencies.  (The DAG is analyzed in the scheduler to find an
   efficient ordering of the assignments.)

   This file also contains utilities to manipulate the DAG in various
   ways. *)

(********************************************
 *  Dag structure
 ********************************************)
type color = RED | BLUE | BLACK | YELLOW

type dagnode = 
    { assigned: Variable.variable;
      mutable expression: Expr.expr;
      input_variables: Variable.variable list;
      mutable successors: dagnode list;
      mutable predecessors: dagnode list;
      mutable label: int;
      mutable color: color}

type dag = Dag of (dagnode list)

(* true if node uses v *)
let node_uses v node = 
  List.exists (Variable.same v) node.input_variables

(* true if assignment of v clobbers any input of node *)
let node_clobbers node v = 
  List.exists (Variable.same_location v) node.input_variables

(* true if nodeb depends on nodea *)
let depends_on nodea nodeb =
  node_uses nodea.assigned nodeb ||
  node_clobbers nodea nodeb.assigned

(* transform an assignment list into a dag *)
let makedag alist =
  let dag = List.map
      (fun assignment ->
	let (v, x) = assignment in
	{ assigned = v;
	  expression = x;
	  input_variables = Expr.find_vars x;
	  successors = [];
	  predecessors = [];
	  label = 0;
	  color = BLACK })
      alist
  in begin
    for_list dag (fun i ->
	for_list dag (fun j ->
	  if depends_on i j then begin
	    i.successors <- j :: i.successors;
	    j.predecessors <- i :: j.predecessors;
	  end));
    Dag dag;
  end

let map f (Dag dag) = Dag (List.map f dag)
let for_all (Dag dag) f = 
  (* type system loophole *)
  let make_unit _ = () in
  make_unit (List.map f dag)
let to_list (Dag dag) = dag

let find_node f (Dag dag) = Util.find_elem f dag

(* breadth-first search *)
let rec bfs (Dag dag) node init_label =
  let _ =  node.label <- init_label in
  let rec loop = function
      [] -> ()
    | node :: rest ->
	let neighbors = node.predecessors @ node.successors in
	let m = min_list (List.map (fun node -> node.label) neighbors) in
	if (node.label > m + 1) then begin
	  node.label <- m + 1;
	  loop (rest @ neighbors);
	end else
	  loop rest
  in let neighbors = node.predecessors @ node.successors in
  loop neighbors

