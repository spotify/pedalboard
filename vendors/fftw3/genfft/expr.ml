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

(* Here, we define the data type encapsulating a symbolic arithmetic
   expression, and provide some routines for manipulating it. *)

(* I will regret this hack : *)
(* NEWS: I did *)
type transcendent = I | MULTI_A | MULTI_B | CONJ

type expr =
  | Num of Number.number
  | NaN of transcendent
  | Plus of expr list
  | Times of expr * expr
  | CTimes of expr * expr
  | CTimesJ of expr * expr  (* CTimesJ (a, b) = conj(a) * b *)
  | Uminus of expr
  | Load of Variable.variable
  | Store of Variable.variable * expr

type assignment = Assign of Variable.variable * expr

(* various hash functions *)
let hash_float x = 
  let (mantissa, exponent) = frexp x
  in truncate (float_of_int(exponent) *. 1234.567 +. mantissa *. 10000.0)

let sum_list l = List.fold_right (+) l 0

let transcendent_to_float = function
  | I -> 2.718281828459045235360287471  (* any transcendent number will do *)
  | MULTI_A -> 0.6931471805599453094172321214
  | MULTI_B -> -0.3665129205816643270124391582
  | CONJ -> 0.6019072301972345747375400015

let rec hash = function
  | Num x -> hash_float (Number.to_float x)
  | NaN x -> hash_float (transcendent_to_float x)
  | Load v -> 1 + 1237 * Variable.hash v
  | Store (v, x) -> 2 * Variable.hash v - 2345 * hash x
  | Plus l -> 5 + 23451 * sum_list (List.map Hashtbl.hash l)
  | Times (a, b) -> 41 + 31415 * (Hashtbl.hash a +  Hashtbl.hash b)
  | CTimes (a, b) -> 49 + 3245 * (Hashtbl.hash a +  Hashtbl.hash b)
  | CTimesJ (a, b) -> 31 + 3471 * (Hashtbl.hash a +  Hashtbl.hash b)
  | Uminus x -> 42 + 12345 * (hash x)

(* find all variables *)
let rec find_vars x =
  match x with
  | Load y -> [y]
  | Plus l -> List.flatten (List.map find_vars l)
  | Times (a, b) -> (find_vars a) @ (find_vars b)
  | CTimes (a, b) -> (find_vars a) @ (find_vars b)
  | CTimesJ (a, b) -> (find_vars a) @ (find_vars b)
  | Uminus a -> find_vars a
  | _ -> []


(* TRUE if expression is a constant *)
let is_constant = function
  | Num _ -> true
  | NaN _ -> true
  | Load v -> Variable.is_constant v
  | _ -> false

let is_known_constant = function
  | Num _ -> true
  | NaN _ -> true
  | _ -> false

(* expr to string, used for debugging *)
let rec foldr_string_concat l = 
  match l with
    [] -> ""
  | [a] -> a
  | a :: b -> a ^ " " ^ (foldr_string_concat b)

let string_of_transcendent = function
  | I -> "I"
  | MULTI_A -> "MULTI_A"
  | MULTI_B -> "MULTI_B"
  | CONJ -> "CONJ"

let rec to_string = function
  | Load v -> Variable.unparse v
  | Num n -> string_of_float (Number.to_float n)
  | NaN n -> string_of_transcendent n
  | Plus x -> "(+ " ^ (foldr_string_concat (List.map to_string x)) ^ ")"
  | Times (a, b) -> "(* " ^ (to_string a) ^ " " ^ (to_string b) ^ ")"
  | CTimes (a, b) -> "(c* " ^ (to_string a) ^ " " ^ (to_string b) ^ ")"
  | CTimesJ (a, b) -> "(cj* " ^ (to_string a) ^ " " ^ (to_string b) ^ ")"
  | Uminus a -> "(- " ^ (to_string a) ^ ")"
  | Store (v, a) -> "(:= " ^ (Variable.unparse v) ^ " " ^
      (to_string a) ^ ")"

let rec to_string_a d x = 
  if (d = 0) then "..." else match x with
  | Load v -> Variable.unparse v
  | Num n -> Number.to_konst n
  | NaN n -> string_of_transcendent n
  | Plus x -> "(+ " ^ (foldr_string_concat (List.map (to_string_a (d - 1)) x)) ^ ")"
  | Times (a, b) -> "(* " ^ (to_string_a (d - 1) a) ^ " " ^ (to_string_a (d - 1) b) ^ ")"
  | CTimes (a, b) -> "(c* " ^ (to_string_a (d - 1) a) ^ " " ^ (to_string_a (d - 1) b) ^ ")"
  | CTimesJ (a, b) -> "(cj* " ^ (to_string_a (d - 1) a) ^ " " ^ (to_string_a (d - 1) b) ^ ")"
  | Uminus a -> "(- " ^ (to_string_a (d-1) a) ^ ")"
  | Store (v, a) -> "(:= " ^ (Variable.unparse v) ^ " " ^
      (to_string_a (d-1) a) ^ ")"

let to_string = to_string_a 10

let assignment_to_string = function
  | Assign (v, a) -> "(:= " ^ (Variable.unparse v) ^ " " ^ (to_string a) ^ ")"

let dump print = List.iter (fun x -> print ((assignment_to_string x) ^ "\n"))

(* find all constants in a given expression *)
let rec expr_to_constants = function
  | Num n -> [n]
  | Plus a -> List.flatten (List.map expr_to_constants a)
  | Times (a, b) -> (expr_to_constants a) @ (expr_to_constants b)
  | CTimes (a, b) -> (expr_to_constants a) @ (expr_to_constants b)
  | CTimesJ (a, b) -> (expr_to_constants a) @ (expr_to_constants b)
  | Uminus a -> expr_to_constants a
  | _ -> []


let add_float_key_value list_so_far k = 
  if List.exists (fun k2 -> Number.equal k k2) list_so_far then
    list_so_far
  else
    k :: list_so_far

let unique_constants = List.fold_left add_float_key_value [] 
