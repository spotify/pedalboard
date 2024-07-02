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

(* This file contains the instruction scheduler, which finds an
   efficient ordering for a given list of instructions.

   The scheduler analyzes the DAG (directed acyclic graph) formed by
   the instruction dependencies, and recursively partitions it.  The
   resulting schedule data structure expresses a "good" ordering
   and structure for the computation.

   The scheduler makes use of utilties in Dag and other packages to
   manipulate the Dag and the instruction list. *)

open Dag
(*************************************************
 *               Dag scheduler
 *************************************************)
let to_assignment node = (Expr.Assign (node.assigned, node.expression))
let makedag l = Dag.makedag 
    (List.map (function Expr.Assign (v, x) -> (v, x)) l)

let return x = x
let has_color c n = (n.color = c)
let set_color c n = (n.color <- c)
let has_either_color c1 c2 n = (n.color = c1 || n.color = c2)

let infinity = 100000 

let cc dag inputs =
  begin
    Dag.for_all dag (fun node -> 
      node.label <- infinity);
    
    (match inputs with 
      a :: _ -> bfs dag a 0
    | _ -> failwith "connected");

    return
      ((List.map to_assignment (List.filter (fun n -> n.label < infinity)
				  (Dag.to_list dag))),
       (List.map to_assignment (List.filter (fun n -> n.label == infinity) 
				  (Dag.to_list dag))))
  end

let rec connected_components alist =
  let dag = makedag alist in
  let inputs = 
    List.filter (fun node -> Util.null node.predecessors) 
      (Dag.to_list dag) in
  match cc dag inputs with
    (a, []) -> [a]
  | (a, b) -> a :: connected_components b

let single_load node =
  match (node.input_variables, node.predecessors) with
    ([x], []) -> 
      Variable.is_constant x ||
      (!Magic.locations_are_special && Variable.is_locative x)
  | _ -> false

let loads_locative node =
  match (node.input_variables, node.predecessors) with
    | ([x], []) -> Variable.is_locative x
    | _ -> false

let partition alist =
  let dag = makedag alist in
  let dag' = Dag.to_list dag in
  let inputs = 
    List.filter (fun node -> Util.null node.predecessors) dag'
  and outputs = 
    List.filter (fun node -> Util.null node.successors) dag'
  and special_inputs =  List.filter single_load dag' in
  begin
    
    let c = match !Magic.schedule_type with
	| 1 -> RED; (* all nodes in the input partition *)
	| -1 -> BLUE; (* all nodes in the output partition *)
	| _ -> BLACK; (* node color determined by bisection algorithm *)
    in Dag.for_all dag (fun node -> node.color <- c);

    Util.for_list inputs (set_color RED);

    (*
       The special inputs are those input nodes that load a single
       location or twiddle factor.  Special inputs can end up either
       in the blue or in the red part.  These inputs are special
       because they inherit a color from their neighbors: If a red
       node needs a special input, the special input becomes red, but
       if all successors of a special input are blue, the special
       input becomes blue.  Outputs are always blue, whether they be
       special or not.

       Because of the processing of special inputs, however, the final
       partition might end up being composed only of blue nodes (which
       is incorrect).  In this case we manually reset all inputs
       (whether special or not) to be red.
    *)

    Util.for_list special_inputs (set_color YELLOW);

    Util.for_list outputs (set_color BLUE);

    let rec loopi donep = 
      match (List.filter
	       (fun node -> (has_color BLACK node) &&
		 List.for_all (has_either_color RED YELLOW) node.predecessors)
	       dag') with
	[] -> if (donep) then () else loopo true
      |	i -> 
	  begin
	    Util.for_list i (fun node -> 
	      begin
      		set_color RED node;
		Util.for_list node.predecessors (set_color RED);
	      end);
	    loopo false; 
	  end

    and loopo donep =
      match (List.filter
	       (fun node -> (has_either_color BLACK YELLOW node) &&
		 List.for_all (has_color BLUE) node.successors)
	       dag') with
	[] -> if (donep) then () else loopi true
      |	o ->
	  begin
	    Util.for_list o (set_color BLUE);
	    loopi false; 
	  end

    in loopi false;

    (* fix the partition if it is incorrect *)
    if not (List.exists (has_color RED) dag') then 
	Util.for_list inputs (set_color RED);
    
    return
      ((List.map to_assignment (List.filter (has_color RED) dag')),
       (List.map to_assignment (List.filter (has_color BLUE) dag')))
  end

type schedule = 
    Done
  | Instr of Expr.assignment
  | Seq of (schedule * schedule)
  | Par of schedule list



(* produce a sequential schedule determined by the user *)
let rec sequentially = function
    [] -> Done
  | a :: b -> Seq (Instr a, sequentially b)

let schedule =
  let rec schedule_alist = function
    | [] -> Done
    | [a] -> Instr a
    | alist -> match connected_components alist with
	| ([a]) -> schedule_connected a
	| l -> Par (List.map schedule_alist l)

  and schedule_connected alist = 
    match partition alist with
    | (a, b) -> Seq (schedule_alist a, schedule_alist b)

  in fun x ->
    let () = Util.info "begin schedule" in
    let res = schedule_alist x in
    let () = Util.info "end schedule" in
    res


(* partition a dag into two parts:

   1) the set of loads from locatives and their successors,
   2) all other nodes

   This step separates the ``body'' of the dag, which computes the
   actual fft, from the ``precomputations'' part, which computes e.g.
   twiddle factors.
*)
let partition_precomputations alist =
  let dag = makedag alist in
  let dag' = Dag.to_list dag in
  let loads =  List.filter loads_locative dag' in
    begin
      
      Dag.for_all dag (set_color BLUE);
      Util.for_list loads (set_color RED);

      let rec loop () = 
	match (List.filter
		 (fun node -> (has_color RED node) &&
		    List.exists (has_color BLUE) node.successors)
		 dag') with
	    [] -> ()
	  |	i -> 
		  begin
		    Util.for_list i 
		      (fun node -> 
			 Util.for_list node.successors (set_color RED));
		    loop ()
		  end

      in loop ();

	return
	  ((List.map to_assignment (List.filter (has_color BLUE) dag')),
	   (List.map to_assignment (List.filter (has_color RED) dag')))
    end

let isolate_precomputations_and_schedule alist =
  let (a, b) = partition_precomputations alist in
    Seq (schedule a, schedule b)
  
