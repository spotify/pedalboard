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

(* Here, we take a schedule (produced by schedule.ml) ordering a
   sequence of instructions, and produce an annotated schedule.  The
   annotated schedule has the same ordering as the original schedule,
   but is additionally partitioned into nested blocks of temporary
   variables.  The partitioning is computed via a heuristic algorithm.

   The blocking allows the C code that we generate to consist of
   nested blocks that help communicate variable lifetimes to the
   compiler. *)

open Schedule
open Expr
open Variable

type annotated_schedule = 
    Annotate of variable list * variable list * variable list * int * aschedule
and aschedule = 
    ADone
  | AInstr of assignment
  | ASeq of (annotated_schedule * annotated_schedule)

let addelem a set = if not (List.memq a set) then a :: set else set
let union l = 
  let f x = addelem x   (* let is source of polymorphism *)
  in List.fold_right f l

(* set difference a - b *)
let diff a b = List.filter (fun x -> not (List.memq x b)) a

let rec minimize f = function
    [] -> failwith "minimize"
  | [n] -> n
  | n :: rest ->
      let x = minimize f rest in
      if (f x) >= (f n) then n else x

(* find all variables used inside a scheduling unit *)
let rec find_block_vars = function
    Done -> []
  | (Instr (Assign (v, x))) -> v :: (find_vars x)
  | Par a -> List.flatten (List.map find_block_vars a)
  | Seq (a, b) -> (find_block_vars a) @ (find_block_vars b)

let uniq l = 
  List.fold_right (fun a b -> if List.memq a b then b else a :: b) l []

let has_related x = List.exists (Variable.same_class x)

let rec overlap a b = Util.count (fun y -> has_related y b) a

(* reorder a list of schedules so as to maximize overlap of variables *)
let reorder l =
  let rec loop = function
      [] -> []
    | (a, va) :: b ->
	let c = 
	  List.map 
	    (fun (a, x) -> ((a, x), (overlap va x, List.length x))) b in
	let c' =
	  List.sort 
	    (fun (_, (a, la)) (_, (b, lb)) ->
              if la < lb || a > b then -1 else 1)
	    c in
	let b' = List.map (fun (a, _) -> a) c' in
	a :: (loop b') in
  let l' = List.map (fun x -> x, uniq (find_block_vars x)) l in
  (* start with smallest block --- does this matter ? *)
  match l' with
    [] -> []
  | _ ->  
      let m = minimize (fun (_, x) -> (List.length x)) l' in
      let l'' = Util.remove m l' in
      loop (m :: l'')

(* remove Par blocks *)
let rec linearize = function
  | Seq (a, Done) -> linearize a
  | Seq (Done, a) -> linearize a
  | Seq (a, b) -> Seq (linearize a, linearize b)

  (* try to balance nested Par blocks *)
  | Par [a] -> linearize a
  | Par l -> 
      let n2 = (List.length l) / 2 in
      let rec loop n a b =
	if n = 0 then
	  (List.rev b, a)
	else
	  match a with
	    [] -> failwith "loop"
	  | x :: y -> loop (n - 1) y (x :: b)
      in let (a, b) = loop n2 (reorder l) []
      in linearize (Seq (Par a, Par b))

  | x -> x 

let subset a b =
  List.for_all (fun x -> List.exists (fun y -> x == y) b) a

let use_same_vars (Assign (av, ax)) (Assign (bv, bx)) =
  is_temporary av &&
  is_temporary bv &&
  (let va = Expr.find_vars ax and vb = Expr.find_vars bx in
   subset va vb && subset vb va)

let store_to_same_class (Assign (av, ax)) (Assign (bv, bx)) =
  is_locative av &&
  is_locative bv &&
  Variable.same_class av bv

let loads_from_same_class (Assign (av, ax)) (Assign (bv, bx)) =
  match (ax, bx) with
    | (Load a), (Load b) when 
	Variable.is_locative a && Variable.is_locative b 
	-> Variable.same_class a b
    | _ -> false

(* extract instructions from schedule *)
let rec sched_to_ilist = function
  | Done -> []
  | Instr a -> [a]
  | Seq (a, b) -> (sched_to_ilist a) @ (sched_to_ilist b)
  | _ -> failwith "sched_to_ilist" (* Par blocks removed by linearize *)

let rec find_friends friendp insn friends foes = function
  | [] -> (friends, foes)
  | a :: b -> 
      if (a == insn) || (friendp a insn) then
	find_friends friendp insn (a :: friends) foes b
      else
	find_friends friendp insn friends (a :: foes) b

(* schedule all instructions in the equivalence class determined
   by friendp at the point where the last one
   is executed *)
let rec delay_friends friendp sched =
  let rec recur insns = function
    | Done -> (Done, insns)
    | Instr a ->
	let (friends, foes) = find_friends friendp a [] [] insns in
	(Schedule.sequentially friends), foes
    | Seq (a, b) ->
	let (b', insnsb) = recur insns b in
	let (a', insnsa) = recur insnsb a in
	(Seq (a', b')), insnsa
    | _ -> failwith "delay_friends"
  in match recur (sched_to_ilist sched) sched with
  | (s, []) -> s (* assert that all insns have been used *)
  | _ -> failwith "delay_friends"

(* schedule all instructions in the equivalence class determined
   by friendp at the point where the first one
   is executed *)
let rec anticipate_friends friendp sched =
  let rec recur insns = function
    | Done -> (Done, insns)
    | Instr a ->
	let (friends, foes) = find_friends friendp a [] [] insns in
	(Schedule.sequentially friends), foes
    | Seq (a, b) ->
	let (a', insnsa) = recur insns a in
	let (b', insnsb) = recur insnsa b in
	(Seq (a', b')), insnsb
    | _ -> failwith "anticipate_friends"
  in match recur (sched_to_ilist sched) sched with
  | (s, []) -> s (* assert that all insns have been used *)
  | _ -> failwith "anticipate_friends"

let collect_buddy_stores buddy_list sched =
  let rec recur sched delayed_stores = match sched with
    | Done -> (sched, delayed_stores)
    | Instr (Assign (v, x)) ->
	begin
	  try
	    let buddies = List.find (List.memq v) buddy_list in 
	    let tmp = Variable.make_temporary () in
	    let i = Seq(Instr (Assign (tmp, x)),
			Instr (Assign (v, Times (NaN MULTI_A, Load tmp))))
	    and delayed_stores = (v, Load tmp) :: delayed_stores in
	      try
		(Seq (i,
		      Instr (Assign 
			       (List.hd buddies,
				Times (NaN MULTI_B,
				       Plus (List.map 
					       (fun buddy ->
						  List.assq buddy 
						    delayed_stores)
					       buddies))) )))
		  , delayed_stores
	      with Not_found -> (i, delayed_stores)
	  with Not_found -> (sched, delayed_stores)
	end
    | Seq (a, b) ->
	let (newa, delayed_stores) = recur a delayed_stores in
	let (newb, delayed_stores) = recur b delayed_stores in
	  (Seq (newa, newb), delayed_stores)
    | _ -> failwith "collect_buddy_stores"
  in let (sched, _) = recur sched [] in
    sched

let schedule_for_pipeline sched =
  let update_readytimes t (Assign (v, _)) ready_times = 
    (v, (t + !Magic.pipeline_latency)) :: ready_times
  and readyp t ready_times (Assign (_, x)) =
    List.for_all 
      (fun var -> 
	 try 
	   (List.assq var ready_times) <= t
	 with Not_found -> false)
      (List.filter Variable.is_temporary (Expr.find_vars x))
  in
  let rec recur sched t ready_times delayed_instructions =
    let (ready, not_ready) = 
      List.partition (readyp t ready_times) delayed_instructions 
    in match ready with
      | a :: b -> 
	  let (sched, t, ready_times, delayed_instructions) =
	    recur sched (t+1) (update_readytimes t a ready_times)
	      (b @ not_ready)
	  in
	    (Seq (Instr a, sched)), t, ready_times, delayed_instructions
      | _ -> (match sched with
		| Done -> (sched, t, ready_times, delayed_instructions)
		| Instr a ->
		    if (readyp t ready_times a) then
		      (sched, (t+1), (update_readytimes t a ready_times),
		       delayed_instructions)
		    else
		      (Done, t, ready_times, (a :: delayed_instructions))
		| Seq (a, b) ->
		    let (a, t, ready_times, delayed_instructions) =
		      recur a t ready_times delayed_instructions 
		    in
		    let (b, t, ready_times, delayed_instructions) =
		      recur b t ready_times delayed_instructions 
		    in (Seq (a, b)), t, ready_times, delayed_instructions
	        | _ -> failwith "schedule_for_pipeline")
  in let rec recur_until_done sched t ready_times delayed_instructions =
      let (sched, t, ready_times, delayed_instructions) = 
	recur sched t ready_times delayed_instructions
      in match delayed_instructions with
	| [] -> sched
	| _ -> 
	    (Seq (sched,
		  (recur_until_done Done (t+1) ready_times 
		     delayed_instructions)))
  in recur_until_done sched 0 [] []
  
let rec rewrite_declarations force_declarations 
    (Annotate (_, _, declared, _, what)) =
  let m = !Magic.number_of_variables in

  let declare_it declared =
    if (force_declarations || List.length declared >= m) then
      ([], declared)
    else
      (declared, [])

  in match what with
    ADone -> Annotate ([], [], [], 0, what)
  | AInstr i -> 
      let (u, d) = declare_it declared
      in Annotate ([], u, d, 0, what)
  | ASeq (a, b) ->
      let ma = rewrite_declarations false a
      and mb = rewrite_declarations false b
      in let Annotate (_, ua, _, _, _) = ma
      and Annotate (_, ub, _, _, _) = mb
      in let (u, d) = declare_it (declared @ ua @ ub)
      in Annotate ([], u, d, 0, ASeq (ma, mb))

let annotate list_of_buddy_stores schedule =
  let rec analyze live_at_end = function
      Done -> Annotate (live_at_end, [], [], 0, ADone)
    | Instr i -> (match i with
	Assign (v, x) -> 
	  let vars = (find_vars x) in
	  Annotate (Util.remove v (union live_at_end vars), [v], [],
		    0, AInstr i))
    | Seq (a, b) ->
	let ab = analyze live_at_end b in
	let Annotate (live_at_begin_b, defined_b, _, depth_a, _) = ab in
	let aa = analyze live_at_begin_b a in
	let Annotate (live_at_begin_a, defined_a, _, depth_b, _) = aa in
	let defined = List.filter is_temporary (defined_a @ defined_b) in
	let declarable = diff defined live_at_end in
	let undeclarable = diff defined declarable 
	and maxdepth = max depth_a depth_b in
	Annotate (live_at_begin_a, undeclarable, declarable, 
		  List.length declarable + maxdepth,
		  ASeq (aa, ab))
    | _ -> failwith "really_analyze"

  in 
  let () = Util.info "begin annotate" in
  let x = linearize schedule in

  let x =
    if (!Magic.schedule_for_pipeline && !Magic.pipeline_latency > 0) then
      schedule_for_pipeline x 
    else
      x
  in

  let x = 
    if !Magic.reorder_insns then 
      linearize(anticipate_friends use_same_vars x) 
    else 
      x
  in

  (* delay stores to the real and imaginary parts of the same number *)
  let x = 
    if !Magic.reorder_stores then 
      linearize(delay_friends store_to_same_class x) 
    else
      x
  in

  (* move loads of the real and imaginary parts of the same number *)
  let x = 
    if !Magic.reorder_loads then 
      linearize(anticipate_friends loads_from_same_class x) 
    else 
      x
  in

  let x = collect_buddy_stores list_of_buddy_stores x in
  let x = analyze [] x in
  let res = rewrite_declarations true x in
  let () = Util.info "end annotate" in
  res

let rec dump print (Annotate (_, _, _, _, code)) =
  dump_code print code
and dump_code print = function
  | ADone -> ()
  | AInstr x -> print ((assignment_to_string x) ^ "\n")
  | ASeq (a, b) -> dump print a; dump print b
