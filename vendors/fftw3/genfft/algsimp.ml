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
open Expr

let node_insert x =  Assoctable.insert Expr.hash x
let node_lookup x =  Assoctable.lookup Expr.hash (==) x

(*************************************************************
 * Algebraic simplifier/elimination of common subexpressions
 *************************************************************)
module AlgSimp : sig 
  val algsimp : expr list -> expr list
end = struct

  open Monads.StateMonad
  open Monads.MemoMonad
  open Assoctable

  let fetchSimp = 
    fetchState >>= fun (s, _) -> returnM s
  let storeSimp s =
    fetchState >>= (fun (_, c) -> storeState (s, c))
  let lookupSimpM key =
    fetchSimp >>= fun table ->
      returnM (node_lookup key table)
  let insertSimpM key value =
    fetchSimp >>= fun table ->
      storeSimp (node_insert key value table)

  let subset a b =
    List.for_all (fun x -> List.exists (fun y -> x == y) b) a

  let structurallyEqualCSE a b = 
    match (a, b) with
    | (Num a, Num b) -> Number.equal a b
    | (NaN a, NaN b) -> a == b
    | (Load a, Load b) -> Variable.same a b
    | (Times (a, a'), Times (b, b')) ->
 	((a == b) && (a' == b')) ||
 	((a == b') && (a' == b))
    | (CTimes (a, a'), CTimes (b, b')) ->
 	((a == b) && (a' == b')) ||
 	((a == b') && (a' == b))
    | (CTimesJ (a, a'), CTimesJ (b, b')) -> ((a == b) && (a' == b'))
    | (Plus a, Plus b) -> subset a b && subset b a
    | (Uminus a, Uminus b) -> (a == b)
    | _ -> false

  let hashCSE x = 
    if (!Magic.randomized_cse) then
      Oracle.hash x
    else
      Expr.hash x

  let equalCSE a b = 
    if (!Magic.randomized_cse) then
      (structurallyEqualCSE a b || Oracle.likely_equal a b)
    else
      structurallyEqualCSE a b

  let fetchCSE = 
    fetchState >>= fun (_, c) -> returnM c
  let storeCSE c =
    fetchState >>= (fun (s, _) -> storeState (s, c))
  let lookupCSEM key =
    fetchCSE >>= fun table ->
      returnM (Assoctable.lookup hashCSE equalCSE key table)
  let insertCSEM key value =
    fetchCSE >>= fun table ->
      storeCSE (Assoctable.insert hashCSE key value table)

  (* memoize both x and Uminus x (unless x is already negated) *) 
  let identityM x =
    let memo x = memoizing lookupCSEM insertCSEM returnM x in
    match x with
	Uminus _ -> memo x 
      |	_ -> memo x >>= fun x' -> memo (Uminus x') >> returnM x'

  let makeNode = identityM

  (* simplifiers for various kinds of nodes *)
  let rec snumM = function
      n when Number.is_zero n -> 
	makeNode (Num (Number.zero))
    | n when Number.negative n -> 
	makeNode (Num (Number.negate n)) >>= suminusM
    | n -> makeNode (Num n)

  and suminusM = function
      Uminus x -> makeNode x
    | Num a when (Number.is_zero a) -> snumM Number.zero
    | a -> makeNode (Uminus a)

  and stimesM = function 
    | (Uminus a, b) -> stimesM (a, b) >>= suminusM
    | (a, Uminus b) -> stimesM (a, b) >>= suminusM
    | (NaN I, CTimes (a, b)) -> stimesM (NaN I, b) >>= 
	fun ib -> sctimesM (a, ib)
    | (NaN I, CTimesJ (a, b)) -> stimesM (NaN I, b) >>= 
	fun ib -> sctimesjM (a, ib)
    | (Num a, Num b) -> snumM (Number.mul a b)
    | (Num a, Times (Num b, c)) -> 
	snumM (Number.mul a b) >>= fun x -> stimesM (x, c)
    | (Num a, b) when Number.is_zero a -> snumM Number.zero
    | (Num a, b) when Number.is_one a -> makeNode b
    | (Num a, b) when Number.is_mone a -> suminusM b
    | (a, b) when is_known_constant b && not (is_known_constant a) -> 
	stimesM (b, a)
    | (a, b) -> makeNode (Times (a, b))

  and sctimesM = function 
    | (Uminus a, b) -> sctimesM (a, b) >>= suminusM
    | (a, Uminus b) -> sctimesM (a, b) >>= suminusM
    | (a, b) -> makeNode (CTimes (a, b))

  and sctimesjM = function 
    | (Uminus a, b) -> sctimesjM (a, b) >>= suminusM
    | (a, Uminus b) -> sctimesjM (a, b) >>= suminusM
    | (a, b) -> makeNode (CTimesJ (a, b))

  and reduce_sumM x = match x with
    [] -> returnM []
  | [Num a] -> 
      if (Number.is_zero a) then 
	returnM [] 
      else returnM x
  | [Uminus (Num a)] -> 
      if (Number.is_zero a) then 
	returnM [] 
      else returnM x
  | (Num a) :: (Num b) :: s -> 
      snumM (Number.add a b) >>= fun x ->
	reduce_sumM (x :: s)
  | (Num a) :: (Uminus (Num b)) :: s -> 
      snumM (Number.sub a b) >>= fun x ->
	reduce_sumM (x :: s)
  | (Uminus (Num a)) :: (Num b) :: s -> 
      snumM (Number.sub b a) >>= fun x ->
	reduce_sumM (x :: s)
  | (Uminus (Num a)) :: (Uminus (Num b)) :: s -> 
      snumM (Number.add a b) >>= 
      suminusM >>= fun x ->
	reduce_sumM (x :: s)
  | ((Num _) as a) :: b :: s -> reduce_sumM (b :: a :: s)
  | ((Uminus (Num _)) as a) :: b :: s -> reduce_sumM (b :: a :: s)
  | a :: s -> 
      reduce_sumM s >>= fun s' -> returnM (a :: s')

  and collectible1 = function
    | NaN _ -> false
    | Uminus x -> collectible1 x
    | _ -> true
  and collectible (a, b) = collectible1 a

  (* collect common factors: ax + bx -> (a+b)x *)
  and collectM which x = 
    let rec findCoeffM which = function
      |	Times (a, b) when collectible (which (a, b)) -> returnM (which (a, b))
      | Uminus x -> 
	  findCoeffM which x >>= fun (coeff, b) ->
	    suminusM coeff >>= fun mcoeff ->
 	      returnM (mcoeff, b)
      | x -> snumM Number.one >>= fun one -> returnM (one, x)
    and separateM xpr = function
 	[] -> returnM ([], [])
      |	a :: b ->
 	  separateM xpr b >>= fun (w, wo) ->
	    (* try first factor *)
 	    findCoeffM (fun (a, b) -> (a, b)) a >>= fun (c, x) ->
 	      if (xpr == x) && collectible (c, x) then returnM (c :: w, wo)
 	      else
	      (* try second factor *)
 		findCoeffM (fun (a, b) -> (b, a)) a >>= fun (c, x) ->
 		  if (xpr == x) && collectible (c, x) then returnM (c :: w, wo)
 		  else returnM (w, a :: wo)
    in match x with
      [] -> returnM x
    | [a] -> returnM x
    | a :: b ->
 	findCoeffM which a >>= fun (_, xpr) ->
 	  separateM xpr x >>= fun (w, wo) ->
 	    collectM which wo >>= fun wo' ->
 	      splusM w >>= fun w' ->
 		stimesM (w', xpr) >>= fun t' ->
 		  returnM (t':: wo')

  and mangleSumM x = returnM x
      >>= reduce_sumM 
      >>= collectM (fun (a, b) -> (a, b))
      >>= collectM (fun (a, b) -> (b, a))
      >>= reduce_sumM 
      >>= deepCollectM !Magic.deep_collect_depth
      >>= reduce_sumM

  and reorder_uminus = function  (* push all Uminuses to the end *)
      [] -> []
    | ((Uminus _) as a' :: b) -> (reorder_uminus b) @ [a']
    | (a :: b) -> a :: (reorder_uminus b)                      

  and canonicalizeM = function 
      [] -> snumM Number.zero
    | [a] -> makeNode a                    (* one term *)
    | a -> generateFusedMultAddM (reorder_uminus a)

  and generateFusedMultAddM = 
    let rec is_multiplication = function
      | Times (Num a, b) -> true
      | Uminus (Times (Num a, b)) -> true
      | _ -> false
    and separate = function
	[] -> ([], [], Number.zero)
      | (Times (Num a, b)) as this :: c -> 
	  let (x, y, max) = separate c in
	  let newmax = if (Number.greater a max) then a else max in
	  (this :: x, y, newmax)
      | (Uminus (Times (Num a, b))) as this :: c -> 
	  let (x, y, max) = separate c in
	  let newmax = if (Number.greater a max) then a else max in
	  (this :: x, y, newmax)
      | this :: c ->
	  let (x, y, max) = separate c in
	  (x, this :: y, max)
    in fun l ->
      if !Magic.enable_fma && count is_multiplication l >= 2 then
	let (w, wo, max) = separate l in
	snumM (Number.div Number.one max) >>= fun invmax' ->
	  snumM max >>= fun max' ->
	    mapM (fun x -> stimesM (invmax', x)) w >>= splusM >>= fun pw' ->
	      stimesM (max', pw') >>= fun mw' ->
		splusM (wo @ [mw'])
      else 
	makeNode (Plus l)


  and negative = function
      Uminus _ -> true
    | _ -> false

  (*
   * simplify patterns of the form
   *
   *  ((c_1 * a + ...) + ...) +  (c_2 * a + ...)
   *
   * The pattern includes arbitrary coefficients and minus signs.
   * A common case of this pattern is the butterfly
   *   (a + b) + (a - b)
   *   (a + b) - (a - b)
   *)
  (* this whole procedure needs much more thought *)
  and deepCollectM maxdepth l =
    let rec findTerms depth x = match x with
      | Uminus x -> findTerms depth x
      |	Times (Num _, b) -> (findTerms (depth - 1) b)
      |	Plus l when depth > 0 ->
	  x :: List.flatten (List.map (findTerms (depth - 1)) l)
      |	x -> [x]
    and duplicates = function
	[] -> []
      |	a :: b -> if List.memq a b then a :: duplicates b
      else duplicates b

    in let rec splitDuplicates depth d x =
      if (List.memq x d) then 
	snumM (Number.zero) >>= fun zero ->
	  returnM (zero, x)
      else match x with
      |	Times (a, b) ->
	  splitDuplicates (depth - 1) d a >>= fun (a', xa) ->
	    splitDuplicates (depth - 1) d b >>= fun (b', xb) ->
	      stimesM (a', b') >>= fun ab ->
		stimesM (a, xb) >>= fun xb' ->
		  stimesM (xa, b) >>= fun xa' ->
		    stimesM (xa, xb) >>= fun xab ->
		      splusM [xa'; xb'; xab] >>= fun x ->
			returnM (ab, x)
      | Uminus a -> 
	  splitDuplicates depth d a >>= fun (x, y) ->
	    suminusM x >>= fun ux -> 
	      suminusM y >>= fun uy -> 
		returnM (ux, uy)
      |	Plus l when depth > 0 -> 
	  mapM (splitDuplicates (depth - 1) d) l >>= fun ld ->
	    let (l', d') = List.split ld in
	    splusM l' >>= fun p ->
	      splusM d' >>= fun d'' ->
	      returnM (p, d'')
      |	x -> 
	  snumM (Number.zero) >>= fun zero' ->
	    returnM (x, zero')

    in let l' = List.flatten (List.map (findTerms maxdepth) l)
    in match duplicates l' with
    | [] -> returnM l
    | d ->
	mapM (splitDuplicates maxdepth d) l >>= fun ld ->
	  let (l', d') = List.split ld in
	  splusM l' >>= fun l'' ->
	    let rec flattenPlusM = function
	      | Plus l -> returnM l
	      | Uminus x ->
		  flattenPlusM x >>= mapM suminusM
	      | x -> returnM [x]
	    in
	    mapM flattenPlusM d' >>= fun d'' ->
	      splusM (List.flatten d'') >>= fun d''' ->
		mangleSumM [l''; d''']

  and splusM l =
    let fma_heuristics x = 
      if !Magic.enable_fma then 
	match x with
	| [Uminus (Times _); Times _] -> Some false
	| [Times _; Uminus (Times _)] -> Some false
	| [Uminus (_); Times _] -> Some true
	| [Times _; Uminus (Plus _)] -> Some true
	| [_; Uminus (Times _)] -> Some false
	| [Uminus (Times _); _] -> Some false
	| _ -> None
      else
	None
    in
    mangleSumM l >>=  fun l' ->
      (* no terms are negative.  Don't do anything *)
      if not (List.exists negative l') then
	canonicalizeM l'
      (* all terms are negative.  Negate them all and collect the minus sign *)
      else if List.for_all negative l' then
	mapM suminusM l' >>= splusM >>= suminusM
      else match fma_heuristics l' with
      |	Some true -> mapM suminusM l' >>= splusM >>= suminusM
      |	Some false -> canonicalizeM l'
      |	None ->
         (* Ask the Oracle for the canonical form *)
	  if (not !Magic.randomized_cse) &&
	    Oracle.should_flip_sign (Plus l') then
	    mapM suminusM l' >>= splusM >>= suminusM
	  else
	    canonicalizeM l'

  (* monadic style algebraic simplifier for the dag *)
  let rec algsimpM x =
    memoizing lookupSimpM insertSimpM 
      (function 
 	| Num a -> snumM a
 	| NaN _ as x -> makeNode x
 	| Plus a -> 
 	    mapM algsimpM a >>= splusM
 	| Times (a, b) -> 
 	    (algsimpM a >>= fun a' ->
 	      algsimpM b >>= fun b' ->
 		stimesM (a', b'))
 	| CTimes (a, b) -> 
 	    (algsimpM a >>= fun a' ->
 	      algsimpM b >>= fun b' ->
		sctimesM (a', b'))
 	| CTimesJ (a, b) -> 
 	    (algsimpM a >>= fun a' ->
 	      algsimpM b >>= fun b' ->
		sctimesjM (a', b'))
 	| Uminus a -> 
 	    algsimpM a >>= suminusM 
 	| Store (v, a) ->
 	    algsimpM a >>= fun a' ->
 	      makeNode (Store (v, a'))
 	| Load _ as x -> makeNode x)
      x

   let initialTable = (empty, empty)
   let simp_roots = mapM algsimpM
   let algsimp = runM initialTable simp_roots
end

(*************************************************************
 * Network transposition algorithm
 *************************************************************)
module Transpose = struct
  open Monads.StateMonad
  open Monads.MemoMonad
  open Littlesimp

  let fetchDuals = fetchState
  let storeDuals = storeState

  let lookupDualsM key =
    fetchDuals >>= fun table ->
      returnM (node_lookup key table)

  let insertDualsM key value =
    fetchDuals >>= fun table ->
      storeDuals (node_insert key value table)

  let rec visit visited vtable parent_table = function
      [] -> (visited, parent_table)
    | node :: rest ->
	match node_lookup node vtable with
	| Some _ -> visit visited vtable parent_table rest
	| None ->
	    let children = match node with
	    | Store (v, n) -> [n]
	    | Plus l -> l
	    | Times (a, b) -> [a; b]
	    | CTimes (a, b) -> [a; b]
	    | CTimesJ (a, b) -> [a; b]
	    | Uminus x -> [x]
	    | _ -> []
	    in let rec loop t = function
		[] -> t
	      |	a :: rest ->
		  (match node_lookup a t with
		    None -> loop (node_insert a [node] t) rest
		  | Some c -> loop (node_insert a (node :: c) t) rest)
	    in 
	    (visit 
	       (node :: visited)
	       (node_insert node () vtable)
	       (loop parent_table children)
	       (children @ rest))

  let make_transposer parent_table =
    let rec termM node candidate_parent = 
      match candidate_parent with
      |	Store (_, n) when n == node -> 
	  dualM candidate_parent >>= fun x' -> returnM [x']
      | Plus (l) when List.memq node l -> 
	  dualM candidate_parent >>= fun x' -> returnM [x']
      | Times (a, b) when b == node -> 
	  dualM candidate_parent >>= fun x' -> 
	    returnM [makeTimes (a, x')]
      | CTimes (a, b) when b == node -> 
	  dualM candidate_parent >>= fun x' -> 
	    returnM [CTimes (a, x')]
      | CTimesJ (a, b) when b == node -> 
	  dualM candidate_parent >>= fun x' -> 
	    returnM [CTimesJ (a, x')]
      | Uminus n when n == node -> 
	  dualM candidate_parent >>= fun x' -> 
	    returnM [makeUminus x']
      | _ -> returnM []
    
    and dualExpressionM this_node = 
      mapM (termM this_node) 
	(match node_lookup this_node parent_table with
	| Some a -> a
	| None -> failwith "bug in dualExpressionM"
	) >>= fun l ->
	returnM (makePlus (List.flatten l))

    and dualM this_node =
      memoizing lookupDualsM insertDualsM
	(function
	  | Load v as x -> 
	      if (Variable.is_constant v) then
		returnM (Load v)
	      else
		(dualExpressionM x >>= fun d ->
		  returnM (Store (v, d)))
	  | Store (v, x) -> returnM (Load v)
	  | x -> dualExpressionM x)
	this_node

    in dualM

  let is_store = function 
    | Store _ -> true
    | _ -> false

  let transpose dag = 
    let _ = Util.info "begin transpose" in
    let (all_nodes, parent_table) = 
      visit [] Assoctable.empty Assoctable.empty dag in
    let transposerM = make_transposer parent_table in
    let mapTransposerM = mapM transposerM in
    let duals = runM Assoctable.empty mapTransposerM all_nodes in
    let roots = List.filter is_store duals in
    let _ = Util.info "end transpose" in
    roots
end


(*************************************************************
 * Various dag statistics
 *************************************************************)
module Stats : sig
  type complexity
  val complexity : Expr.expr list -> complexity
  val same_complexity : complexity -> complexity -> bool
  val leq_complexity : complexity -> complexity -> bool
  val to_string : complexity -> string
end = struct
  type complexity = int * int * int * int * int * int
  let rec visit visited vtable = function
      [] -> visited
    | node :: rest ->
	match node_lookup node vtable with
	  Some _ -> visit visited vtable rest
	| None ->
	    let children = match node with
	      Store (v, n) -> [n]
	    | Plus l -> l
	    | Times (a, b) -> [a; b]
	    | Uminus x -> [x]
	    | _ -> []
	    in visit (node :: visited)
	      (node_insert node () vtable)
	      (children @ rest)

  let complexity dag = 
    let rec loop (load, store, plus, times, uminus, num) = function 
      	[] -> (load, store, plus, times, uminus, num)
      | node :: rest ->
	  loop
	    (match node with
	    | Load _ -> (load + 1, store, plus, times, uminus, num)
	    | Store _ -> (load, store + 1, plus, times, uminus, num)
	    | Plus x -> (load, store, plus + (List.length x - 1), times, uminus, num)
	    | Times _ -> (load, store, plus, times + 1, uminus, num)
	    | Uminus _ -> (load, store, plus, times, uminus + 1, num)
	    | Num _ -> (load, store, plus, times, uminus, num + 1)
	    | CTimes _ -> (load, store, plus, times, uminus, num)
	    | CTimesJ _ -> (load, store, plus, times, uminus, num)
	    | NaN _ -> (load, store, plus, times, uminus, num))
	    rest
    in let (l, s, p, t, u, n) = 
      loop (0, 0, 0, 0, 0, 0) (visit [] Assoctable.empty dag)
    in (l, s, p, t, u, n)

  let weight (l, s, p, t, u, n) =
    l + s + 10 * p + 20 * t + u + n

  let same_complexity a b = weight a = weight b
  let leq_complexity a b = weight a <= weight b

  let to_string (l, s, p, t, u, n) =
    Printf.sprintf "ld=%d st=%d add=%d mul=%d uminus=%d num=%d\n"
		   l s p t u n
		   
end    

(* simplify the dag *)
let algsimp v = 
  let rec simplification_loop v =
    let () = Util.info "simplification step" in
    let complexity = Stats.complexity v in
    let () = Util.info ("complexity = " ^ (Stats.to_string complexity)) in
    let v = (AlgSimp.algsimp @@ Transpose.transpose @@ 
	     AlgSimp.algsimp @@ Transpose.transpose) v in
    let complexity' = Stats.complexity v in
    let () = Util.info ("complexity = " ^ (Stats.to_string complexity')) in
    if (Stats.leq_complexity complexity' complexity) then
      let () = Util.info "end algsimp" in
      v
    else
      simplification_loop v

  in
  let () = Util.info "begin algsimp" in
  let v = AlgSimp.algsimp v in
  if !Magic.network_transposition then simplification_loop v else v

