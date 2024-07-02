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

(* generation of trigonometric transforms *)

open Util
open Genutil
open C


let usage = "Usage: " ^ Sys.argv.(0) ^ " -n <number>"

let uistride = ref Stride_variable
let uostride = ref Stride_variable
let uivstride = ref Stride_variable
let uovstride = ref Stride_variable

type mode =
  | RDFT
  | HDFT
  | DHT
  | REDFT00
  | REDFT10
  | REDFT01
  | REDFT11
  | RODFT00
  | RODFT10
  | RODFT01
  | RODFT11
  | NONE

let mode = ref NONE
let normsqr = ref 1
let unitary = ref false
let noloop = ref false

let speclist = [
  "-with-istride",
  Arg.String(fun x -> uistride := arg_to_stride x),
  " specialize for given input stride";

  "-with-ostride",
  Arg.String(fun x -> uostride := arg_to_stride x),
  " specialize for given output stride";

  "-with-ivstride",
  Arg.String(fun x -> uivstride := arg_to_stride x),
  " specialize for given input vector stride";

  "-with-ovstride",
  Arg.String(fun x -> uovstride := arg_to_stride x),
  " specialize for given output vector stride";

  "-rdft",
  Arg.Unit(fun () -> mode := RDFT),
  " generate a real DFT codelet";

  "-hdft",
  Arg.Unit(fun () -> mode := HDFT),
  " generate a Hermitian DFT codelet";

  "-dht",
  Arg.Unit(fun () -> mode := DHT),
  " generate a DHT codelet";

  "-redft00",
  Arg.Unit(fun () -> mode := REDFT00),
  " generate a DCT-I codelet";

  "-redft10",
  Arg.Unit(fun () -> mode := REDFT10),
  " generate a DCT-II codelet";

  "-redft01",
  Arg.Unit(fun () -> mode := REDFT01),
  " generate a DCT-III codelet";

  "-redft11",
  Arg.Unit(fun () -> mode := REDFT11),
  " generate a DCT-IV codelet";

  "-rodft00",
  Arg.Unit(fun () -> mode := RODFT00),
  " generate a DST-I codelet";

  "-rodft10",
  Arg.Unit(fun () -> mode := RODFT10),
  " generate a DST-II codelet";

  "-rodft01",
  Arg.Unit(fun () -> mode := RODFT01),
  " generate a DST-III codelet";

  "-rodft11",
  Arg.Unit(fun () -> mode := RODFT11),
  " generate a DST-IV codelet";

  "-normalization",
  Arg.String(fun x -> let ix = int_of_string x in normsqr := ix * ix),
  " normalization integer to divide by";

  "-normsqr",
  Arg.String(fun x -> normsqr := int_of_string x),
  " integer square of normalization to divide by";

  "-unitary",
  Arg.Unit(fun () -> unitary := true),
  " unitary normalization (up overall scale factor)";

  "-noloop",
  Arg.Unit(fun () -> noloop := true),
  " no vector loop";
]

let sqrt_half = Complex.inverse_int_sqrt 2
let sqrt_two = Complex.int_sqrt 2

let rescale sc s1 s2 input i = 
  if ((i == s1 || i == s2) && !unitary) then
    Complex.times (input i) sc
  else
    input i

let generate n mode =
  let iarray = "I"
  and oarray = "O"
  and istride = "is"
  and ostride = "os" 
  and i = "i" 
  and v = "v" 
  in

  let sign = !Genutil.sign 
  and name = !Magic.codelet_name in

  let vistride = either_stride (!uistride) (C.SVar istride)
  and vostride = either_stride (!uostride) (C.SVar ostride)
  in

  let sovs = stride_to_string "ovs" !uovstride in
  let sivs = stride_to_string "ivs" !uivstride in

  let (transform, load_input, store_output, si1,si2,so1,so2) = match mode with
  | RDFT -> Trig.rdft sign, load_array_r, store_array_hc, -1,-1,-1,-1
  | HDFT -> Trig.hdft sign, load_array_c, store_array_r, -1,-1,-1,-1 (* TODO *)
  | DHT -> Trig.dht 1, load_array_r, store_array_r, -1,-1,-1,-1
  | REDFT00 -> Trig.dctI, load_array_r, store_array_r, 0,n-1,0,n-1
  | REDFT10 -> Trig.dctII, load_array_r, store_array_r, -1,-1,0,-1
  | REDFT01 -> Trig.dctIII, load_array_r, store_array_r, 0,-1,-1,-1
  | REDFT11 -> Trig.dctIV, load_array_r, store_array_r, -1,-1,-1,-1
  | RODFT00 -> Trig.dstI, load_array_r, store_array_r, -1,-1,-1,-1
  | RODFT10 -> Trig.dstII, load_array_r, store_array_r, -1,-1,n-1,-1
  | RODFT01 -> Trig.dstIII, load_array_r, store_array_r, n-1,-1,-1,-1
  | RODFT11 -> Trig.dstIV, load_array_r, store_array_r, -1,-1,-1,-1
  | _ -> failwith "must specify transform kind"
  in
    
  let locations = unique_array_c n in
  let input = locative_array_c n 
      (C.array_subscript iarray vistride)
      (C.array_subscript "BUG" vistride)
      locations sivs in
  let output = rescale sqrt_half so1 so2
      ((Complex.times (Complex.inverse_int_sqrt !normsqr))
       @@ (transform n (rescale sqrt_two si1 si2 (load_array_c n input)))) in
  let oloc = 
    locative_array_c n 
      (C.array_subscript oarray vostride)
      (C.array_subscript "BUG" vostride)
      locations sovs in
  let odag = store_output n oloc output in
  let annot = standard_optimizer odag in

  let body = if !noloop then Block([], [Asch annot]) else Block (
    [Decl ("INT", i)],
    [For (Expr_assign (CVar i, CVar v),
	  Binop (" > ", CVar i, Integer 0),
	  list_to_comma 
	    [Expr_assign (CVar i, CPlus [CVar i; CUminus (Integer 1)]);
	     Expr_assign (CVar iarray, CPlus [CVar iarray; CVar sivs]);
	     Expr_assign (CVar oarray, CPlus [CVar oarray; CVar sovs]);
	     make_volatile_stride (2*n) (CVar istride);
	     make_volatile_stride (2*n) (CVar ostride)
	   ],
	  Asch annot)
   ])
  in

  let tree =
    Fcn ((if !Magic.standalone then "void" else "static void"), name,
	 ([Decl (C.constrealtypep, iarray);
	   Decl (C.realtypep, oarray)]
	  @ (if stride_fixed !uistride then [] 
               else [Decl (C.stridetype, istride)])
	  @ (if stride_fixed !uostride then [] 
	       else [Decl (C.stridetype, ostride)])
	  @ (if !noloop then [] else
               [Decl ("INT", v)]
	       @ (if stride_fixed !uivstride then [] 
                    else [Decl ("INT", "ivs")])
	       @ (if stride_fixed !uovstride then [] 
                    else [Decl ("INT", "ovs")]))),
	 finalize_fcn body)

  in let desc = 
    Printf.sprintf 
      "static const kr2r_desc desc = { %d, \"%s\", %s, &GENUS, %s };\n\n"
      n name (flops_of tree) 
      (match mode with
      | RDFT -> "RDFT00"
      | HDFT -> "HDFT00"
      | DHT  -> "DHT"
      | REDFT00 -> "REDFT00"
      | REDFT10 -> "REDFT10"
      | REDFT01 -> "REDFT01"
      | REDFT11 -> "REDFT11"
      | RODFT00 -> "RODFT00"
      | RODFT10 -> "RODFT10"
      | RODFT01 -> "RODFT01"
      | RODFT11 -> "RODFT11"
      | _ -> failwith "must specify a transform kind")

  and init =
    (declare_register_fcn name) ^
    "{" ^
    "  X(kr2r_register)(p, " ^ name ^ ", &desc);\n" ^
    "}\n"

  in
  (unparse tree) ^ "\n" ^ (if !Magic.standalone then "" else desc ^ init)


let main () =
  begin
    parse speclist usage;
    print_string (generate (check_size ()) !mode);
  end

let _ = main()
