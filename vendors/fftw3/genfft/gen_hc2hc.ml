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


type ditdif = DIT | DIF
let ditdif = ref DIT
let usage = "Usage: " ^ Sys.argv.(0) ^ " -n <number> [ -dit | -dif ]"

let urs = ref Stride_variable

let speclist = [
  "-dit",
  Arg.Unit(fun () -> ditdif := DIT),
  " generate a DIT codelet";

  "-dif",
  Arg.Unit(fun () -> ditdif := DIF),
  " generate a DIF codelet";

  "-with-rs",
  Arg.String(fun x -> urs := arg_to_stride x),
  " specialize for given R-stride";
]

let rioarray = "cr" 
and iioarray = "ci" 

let genone sign n transform load store vrs =
  let locations = unique_array_c n in
  let input = 
    locative_array_c n 
      (C.array_subscript rioarray vrs)
      (C.array_subscript iioarray vrs)
      locations "BUG" in
  let output = transform sign n (load n input) in
  let ioloc = 
    locative_array_c n 
      (C.array_subscript rioarray vrs)
      (C.array_subscript iioarray vrs)
      locations "BUG" in
  let odag = store n ioloc output in
  let annot = standard_optimizer odag 
  in annot

let byi = Complex.times Complex.i
let byui = Complex.times (Complex.uminus Complex.i)

let sym1 n f i = 
  Complex.plus [Complex.real (f i); byi (Complex.imag (f (n - 1 - i)))]

let sym2 n f i = if (i < n - i) then f i else byi (f i)
let sym2i n f i = if (i < n - i) then f i else byui (f i)

let generate n =
  let rs = "rs"
  and twarray = "W"
  and m = "m" and mb = "mb" and me = "me" and ms = "ms" in

  let sign = !Genutil.sign 
  and name = !Magic.codelet_name 
  and byvl x = choose_simd x (ctimes (CVar "VL", x)) in

  let (bytwiddle, num_twiddles, twdesc) = Twiddle.twiddle_policy 1 false in
  let nt = num_twiddles n in

  let byw = bytwiddle n sign (twiddle_array nt twarray) in

  let vrs = either_stride (!urs) (C.SVar rs) in

  let asch = 
    match !ditdif with
    | DIT -> 
	genone sign n 
	  (fun sign n input -> 
	     ((sym1 n) @@ (sym2 n)) (Fft.dft sign n (byw input)))
	  load_array_c store_array_c vrs
    | DIF -> 
	genone sign n 
	  (fun sign n input -> 
	     byw (Fft.dft sign n (((sym2i n) @@ (sym1 n)) input)))
	  load_array_c store_array_c vrs
  in

  let vms = CVar "ms" 
  and vrioarray = CVar rioarray
  and viioarray = CVar iioarray
  and vm = CVar m and vmb = CVar mb and vme = CVar me 
  in
  let body = Block (
    [Decl ("INT", m)],
    [For (list_to_comma
	    [Expr_assign (vm, vmb);
	     Expr_assign (CVar twarray, 
			  CPlus [CVar twarray; 
				 ctimes (CPlus [vmb; CUminus (Integer 1)],
					 Integer nt)])],
	  Binop (" < ", vm, vme),
	  list_to_comma 
	    [Expr_assign (vm, CPlus [vm; byvl (Integer 1)]);
	     Expr_assign (vrioarray, CPlus [vrioarray; byvl vms]);
	     Expr_assign (viioarray, 
			  CPlus [viioarray; CUminus (byvl vms)]);
	     Expr_assign (CVar twarray, CPlus [CVar twarray; 
					       byvl (Integer nt)]);
	     make_volatile_stride (2*n) (CVar rs)
	   ],
	  Asch asch)])
  in

  let tree = 
    Fcn ("static void", name,
	 [Decl (C.realtypep, rioarray);
	  Decl (C.realtypep, iioarray);
	  Decl (C.constrealtypep, twarray);
	  Decl (C.stridetype, rs);
	  Decl ("INT", mb);
	  Decl ("INT", me);
	  Decl ("INT", ms)],
         finalize_fcn body)
  in
  let twinstr = 
    Printf.sprintf "static const tw_instr twinstr[] = %s;\n\n" 
      (twinstr_to_string "VL" (twdesc n))
  and desc = 
    Printf.sprintf
      "static const hc2hc_desc desc = {%d, \"%s\", twinstr, &GENUS, %s};\n\n"
      n name (flops_of tree)
  and register = "X(khc2hc_register)"

  in
  let init =
    "\n" ^
    twinstr ^ 
    desc ^
    (declare_register_fcn name) ^
    (Printf.sprintf "{\n%s(p, %s, &desc);\n}" register name)
  in

  (unparse tree) ^ "\n" ^ init


let main () =
  begin 
    parse (speclist @ Twiddle.speclist) usage;
    print_string (generate (check_size ()));
  end

let _ = main()
