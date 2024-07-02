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
open Genutil
open C


let usage = "Usage: " ^ Sys.argv.(0) ^ " -n <number>"

let urs = ref Stride_variable
let ucsr = ref Stride_variable
let ucsi = ref Stride_variable
let uivs = ref Stride_variable
let uovs = ref Stride_variable
let dftIII_flag = ref false

let speclist = [
  "-with-rs",
  Arg.String(fun x -> urs := arg_to_stride x),
  " specialize for given real-array stride";

  "-with-csr",
  Arg.String(fun x -> ucsr := arg_to_stride x),
  " specialize for given complex-array real stride";

  "-with-csi",
  Arg.String(fun x -> ucsi := arg_to_stride x),
  " specialize for given complex-array imaginary stride";

  "-with-ivs",
  Arg.String(fun x -> uivs := arg_to_stride x),
  " specialize for given input vector stride";

  "-with-ovs",
  Arg.String(fun x -> uovs := arg_to_stride x),
  " specialize for given output vector stride";

  "-dft-III",
  Arg.Unit(fun () -> dftIII_flag := true),
  " produce shifted dftIII-style codelets"
] 

let hcdftIII sign n input =
  let input' i =
    if (i mod 2 == 0) then
      Complex.zero
    else
      let i' = (i - 1) / 2 in
      if (2 * i' < n - 1) then (input i')
      else if (2 * i' == n - 1) then 
	Complex.real (input i')
      else 
	Complex.conj (input (n - 1 - i')) 
  in Fft.dft sign (2 * n) input'

let generate n =
  let ar0 = "R0" and ar1 = "R1" and acr = "Cr" and aci = "Ci"
  and rs = "rs" and csr = "csr" and csi = "csi" 
  and i = "i" and v = "v"
  and transform = if !dftIII_flag then hcdftIII else Trig.hdft
  in

  let sign = !Genutil.sign 
  and name = !Magic.codelet_name in

  let vrs = either_stride (!urs) (C.SVar rs)
  and vcsr = either_stride (!ucsr) (C.SVar csr)
  and vcsi = either_stride (!ucsi) (C.SVar csi)
  in

  let sovs = stride_to_string "ovs" !uovs in
  let sivs = stride_to_string "ivs" !uivs in

  let locations = unique_array_c n in
  let input = 
    locative_array_c n 
      (C.array_subscript acr vcsr)
      (C.array_subscript aci vcsi)
      locations sivs in
  let output = transform sign n (load_array_hc n input) in
  let oloce = 
    locative_array_c n 
      (C.array_subscript ar0 vrs)
      (C.array_subscript "BUG" vrs)
      locations sovs
  and oloco = 
    locative_array_c n 
      (C.array_subscript ar1 vrs)
      (C.array_subscript "BUG" vrs)
      locations sovs in
  let oloc i = if i mod 2 == 0 then oloce (i/2) else oloco ((i-1)/2) in
  let odag = store_array_r n oloc output in
  let annot = standard_optimizer odag in

  let body = Block (
    [Decl ("INT", i)],
    [For (Expr_assign (CVar i, CVar v),
	  Binop (" > ", CVar i, Integer 0),
	  list_to_comma 
	    [Expr_assign (CVar i, CPlus [CVar i; CUminus (Integer 1)]);
	     Expr_assign (CVar ar0, CPlus [CVar ar0; CVar sovs]);
	     Expr_assign (CVar ar1, CPlus [CVar ar1; CVar sovs]);
	     Expr_assign (CVar acr, CPlus [CVar acr; CVar sivs]);
	     Expr_assign (CVar aci, CPlus [CVar aci; CVar sivs]);
	     make_volatile_stride (4*n) (CVar rs);
	     make_volatile_stride (4*n) (CVar csr);
	     make_volatile_stride (4*n) (CVar csi)
	   ],
	  Asch annot)
   ])
  in

  let tree =
    Fcn ((if !Magic.standalone then "void" else "static void"), name,
	 ([Decl (C.realtypep, ar0);
	   Decl (C.realtypep, ar1);
	   Decl (C.realtypep, acr);
	   Decl (C.realtypep, aci);
	   Decl (C.stridetype, rs);
	   Decl (C.stridetype, csr);
	   Decl (C.stridetype, csi);
	   Decl ("INT", v);
	   Decl ("INT", "ivs");
	   Decl ("INT", "ovs")]),
	 finalize_fcn body)

  in let desc = 
    Printf.sprintf 
      "static const kr2c_desc desc = { %d, \"%s\", %s, &GENUS };\n\n"
      n name (flops_of tree) 

  and init =
    (declare_register_fcn name) ^
    "{" ^
    "  X(kr2c_register)(p, " ^ name ^ ", &desc);\n" ^
    "}\n"

  in
  (unparse tree) ^ "\n" ^ (if !Magic.standalone then "" else desc ^ init)


let main () =
  begin
    parse speclist usage;
    print_string (generate (check_size ()));
  end

let _ = main()
