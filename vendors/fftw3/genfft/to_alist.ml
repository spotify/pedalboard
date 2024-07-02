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

(*************************************************************
 * Conversion of the dag to an assignment list
 *************************************************************)
(*
 * This function is messy.  The main problem is that we want to
 * inline dag nodes conditionally, depending on how many times they
 * are used.  The Right Thing to do would be to modify the
 * state monad to propagate some of the state backwards, so that
 * we know whether a given node will be used again in the future.
 * This modification is trivial in a lazy language, but it is
 * messy in a strict language like ML.  
 *
 * In this implementation, we just do the obvious thing, i.e., visit
 * the dag twice, the first to count the node usages, and the second to
 * produce the output.
 *)

open Monads.StateMonad
open Monads.MemoMonad
open Expr

let fresh = Variable.make_temporary
let node_insert x =  Assoctable.insert Expr.hash x
let node_lookup x =  Assoctable.lookup Expr.hash (==) x
let empty = Assoctable.empty

let fetchAl = 
  fetchState >>= (fun (al, _, _) -> returnM al)

let storeAl al =
  fetchState >>= (fun (_, visited, visited') ->
    storeState (al, visited, visited'))

let fetchVisited = fetchState >>= (fun (_, v, _) -> returnM v)

let storeVisited visited =
  fetchState >>= (fun (al, _, visited') ->
    storeState (al, visited, visited'))

let fetchVisited' = fetchState >>= (fun (_, _, v') -> returnM v')
let storeVisited' visited' =
  fetchState >>= (fun (al, visited, _) ->
    storeState (al, visited, visited'))
let lookupVisitedM' key =
  fetchVisited' >>= fun table ->
    returnM (node_lookup key table)
let insertVisitedM' key value =
  fetchVisited' >>= fun table ->
    storeVisited' (node_insert key value table)

let counting f x =
  fetchVisited >>= (fun v ->
    match node_lookup x v with
      Some count -> 
	let incr_cnt = 
	  fetchVisited >>= (fun v' ->
	    storeVisited (node_insert x (count + 1) v'))
	in
	begin
	  match x with
	    (* Uminus is always inlined.  Visit child *)
	    Uminus y -> f y >> incr_cnt
	  | _ -> incr_cnt
	end
    | None ->
        f x >> fetchVisited >>= (fun v' ->
            storeVisited (node_insert x 1 v')))

let with_varM v x = 
  fetchAl >>= (fun al -> storeAl ((v, x) :: al)) >> returnM (Load v)

let inlineM = returnM

let with_tempM x = match x with
| Load v when Variable.is_temporary v -> inlineM x (* avoid trivial moves *)
|  _ -> with_varM (fresh ()) x

(* declare a temporary only if node is used more than once *)
let with_temp_maybeM node x =
  fetchVisited >>= (fun v ->
    match node_lookup node v with
      Some count -> 
        if (count = 1 && !Magic.inline_single) then
          inlineM x
        else
          with_tempM x
    | None ->
        failwith "with_temp_maybeM")
type fma = 
    NO_FMA
  | FMA of expr * expr * expr   (* FMA (a, b, c) => a + b * c *)
  | FMS of expr * expr * expr   (* FMS (a, b, c) => -a + b * c *)
  | FNMS of expr * expr * expr  (* FNMS (a, b, c) => a - b * c *)

let good_for_fma (a, b) = 
  let good = function
    | NaN I -> true
    | NaN CONJ -> true
    | NaN _ -> false
    | Times(NaN _, _) -> false
    | Times(_, NaN _) -> false
    | _ -> true
  in good a && good b

let build_fma l = 
  if (not !Magic.enable_fma) then NO_FMA
  else match l with
  | [a; Uminus (Times (b, c))] when good_for_fma (b, c) -> FNMS (a, b, c)
  | [Uminus (Times (b, c)); a] when good_for_fma (b, c) -> FNMS (a, b, c)
  | [Uminus a; Times (b, c)] when good_for_fma (b, c) -> FMS (a, b, c)
  | [Times (b, c); Uminus a] when good_for_fma (b, c) -> FMS (a, b, c)
  | [a; Times (b, c)] when good_for_fma (b, c) -> FMA (a, b, c)
  | [Times (b, c); a] when good_for_fma (b, c) -> FMA (a, b, c)
  | _ -> NO_FMA

let children_fma l = match build_fma l with
| FMA (a, b, c) -> Some (a, b, c)
| FMS (a, b, c) -> Some (a, b, c)
| FNMS (a, b, c) -> Some (a, b, c)
| NO_FMA -> None


let rec visitM x =
  counting (function
    | Load v -> returnM ()
    | Num a -> returnM ()
    | NaN a -> returnM ()
    | Store (v, x) -> visitM x
    | Plus a -> (match children_fma a with
	None -> mapM visitM a >> returnM ()
      | Some (a, b, c) -> 
          (* visit fma's arguments twice to make sure they are not inlined *)
	  visitM a >> visitM a >>
	  visitM b >> visitM b >>
	  visitM c >> visitM c)
    | Times (a, b) -> visitM a >> visitM b
    | CTimes (a, b) -> visitM a >> visitM b
    | CTimesJ (a, b) -> visitM a >> visitM b
    | Uminus a -> visitM a)
    x

let visit_rootsM = mapM visitM


let rec expr_of_nodeM x =
  memoizing lookupVisitedM' insertVisitedM'
    (function x -> match x with
    | Load v -> 
	if (Variable.is_temporary v) then
	  inlineM (Load v)
	else if (Variable.is_locative v && !Magic.inline_loads) then
          inlineM (Load v)
        else if (Variable.is_constant v && !Magic.inline_loads_constants) then
          inlineM (Load v)
	else
          with_tempM (Load v)
    | Num a ->
        if !Magic.inline_constants then
          inlineM (Num a)
	else
          with_temp_maybeM x (Num a)
    | NaN a -> inlineM (NaN a)
    | Store (v, x) -> 
        expr_of_nodeM x >>= 
	(if !Magic.trivial_stores then with_tempM else inlineM) >>=
        with_varM v 

    | Plus a -> 
	begin
	  match build_fma a with
	    FMA (a, b, c) ->	  
	      expr_of_nodeM a >>= fun a' ->
		expr_of_nodeM b >>= fun b' ->
		  expr_of_nodeM c >>= fun c' ->
		    with_temp_maybeM x (Plus [a'; Times (b', c')])
	  | FMS (a, b, c) ->	  
	      expr_of_nodeM a >>= fun a' ->
		expr_of_nodeM b >>= fun b' ->
		  expr_of_nodeM c >>= fun c' ->
		    with_temp_maybeM x 
		      (Plus [Times (b', c'); Uminus a'])
	  | FNMS (a, b, c) ->	  
	      expr_of_nodeM a >>= fun a' ->
		expr_of_nodeM b >>= fun b' ->
		  expr_of_nodeM c >>= fun c' ->
		    with_temp_maybeM x 
		      (Plus [a'; Uminus (Times (b', c'))])
	  | NO_FMA ->
              mapM expr_of_nodeM a >>= fun a' ->
		with_temp_maybeM x (Plus a')
	end
    | CTimes (Load _ as a, b) when !Magic.generate_bytw ->
        expr_of_nodeM b >>= fun b' ->
          with_tempM (CTimes (a, b'))
    | CTimes (a, b) ->
        expr_of_nodeM a >>= fun a' ->
          expr_of_nodeM b >>= fun b' ->
            with_tempM (CTimes (a', b'))
    | CTimesJ (Load _ as a, b) when !Magic.generate_bytw ->
        expr_of_nodeM b >>= fun b' ->
          with_tempM (CTimesJ (a, b'))
    | CTimesJ (a, b) ->
        expr_of_nodeM a >>= fun a' ->
          expr_of_nodeM b >>= fun b' ->
            with_tempM (CTimesJ (a', b'))
    | Times (a, b) ->
        expr_of_nodeM a >>= fun a' ->
          expr_of_nodeM b >>= fun b' ->
	    begin
	      match a' with
		Num a'' when !Magic.strength_reduce_mul && Number.is_two a'' ->
		  (inlineM b' >>= fun b'' ->
		    with_temp_maybeM x (Plus [b''; b'']))
	      | _ -> with_temp_maybeM x (Times (a', b'))
	    end
    | Uminus a ->
        expr_of_nodeM a >>= fun a' ->
          inlineM (Uminus a'))
    x

let expr_of_rootsM = mapM expr_of_nodeM

let peek_alistM roots =
  visit_rootsM roots >> expr_of_rootsM roots >> fetchAl

let wrap_assign (a, b) = Expr.Assign (a, b)

let to_assignments dag =
  let () = Util.info "begin to_alist" in
  let al = List.rev (runM ([], empty, empty) peek_alistM dag) in
  let res = List.map wrap_assign al in
  let () = Util.info "end to_alist" in
  res


(* dump alist in `dot' format *)
let dump print alist =
  let vs v = "\"" ^ (Variable.unparse v) ^ "\"" in
  begin
    print "digraph G {\n";
    print "\tsize=\"6,6\";\n";

    (* all input nodes have the same rank *)
    print "{ rank = same;\n";
    List.iter (fun (Expr.Assign (v, x)) ->
      List.iter (fun y -> 
	if (Variable.is_locative y) then print("\t" ^ (vs y) ^ ";\n"))
	(Expr.find_vars x))
      alist;
    print "}\n";

    (* all output nodes have the same rank *)
    print "{ rank = same;\n";
    List.iter (fun (Expr.Assign (v, x)) ->
      if (Variable.is_locative v) then print("\t" ^ (vs v) ^ ";\n"))
      alist;
    print "}\n";
    
    (* edges *)
    List.iter (fun (Expr.Assign (v, x)) ->
      List.iter (fun y -> print("\t" ^ (vs y) ^ " -> " ^ (vs v) ^ ";\n"))
	(Expr.find_vars x))
      alist;

    print "}\n";
  end

