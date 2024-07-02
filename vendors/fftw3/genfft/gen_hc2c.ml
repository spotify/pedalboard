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

let byi = Complex.times Complex.i
let byui = Complex.times (Complex.uminus Complex.i)

let sym n f i = if (i < n - i) then f i else Complex.conj (f i)

let shuffle_eo fe fo i = if i mod 2 == 0 then fe (i/2) else fo ((i-1)/2)

let generate n =
  let rs = "rs"
  and twarray = "W"
  and m = "m" and mb = "mb" and me = "me" and ms = "ms"

  (* the array names are from the point of view of the complex array
     (output in R2C, input in C2R) *)
  and arp = "Rp" (* real, positive *)
  and aip = "Ip" (* imag, positive *)
  and arm = "Rm" (* real, negative *)
  and aim = "Im" (* imag, negative *)

  in

  let sign = !Genutil.sign 
  and name = !Magic.codelet_name 
  and byvl x = choose_simd x (ctimes (CVar "VL", x)) in

  let (bytwiddle, num_twiddles, twdesc) = Twiddle.twiddle_policy 1 false in
  let nt = num_twiddles n in

  let byw = bytwiddle n sign (twiddle_array nt twarray) in

  let vrs = either_stride (!urs) (C.SVar rs) in

  (* assume a single location.  No point in doing alias analysis *)
  let the_location = (Unique.make (), Unique.make ()) in
  let locations _ = the_location in

  let locr = (locative_array_c n 
		(C.array_subscript arp vrs)
		(C.array_subscript arm vrs)
		locations "BUG")
  and loci = (locative_array_c n 
		(C.array_subscript aip vrs)
		(C.array_subscript aim vrs)
		locations "BUG")
  and locp = (locative_array_c n 
		(C.array_subscript arp vrs)
		(C.array_subscript aip vrs)
		locations "BUG")
  and locm = (locative_array_c n 
		(C.array_subscript arm vrs)
		(C.array_subscript aim vrs)
		locations "BUG")
  in
  let locri i = if i mod 2 == 0 then locr (i/2) else loci ((i-1)/2)
  and locpm i = if i < n - i then locp i else locm (n-1-i)
  in

  let asch = 
    match !ditdif with
    | DIT -> 
	let output = Fft.dft sign n (byw (load_array_c n locri)) in
	let odag = store_array_c n locpm (sym n output) in
	  standard_optimizer odag 

    | DIF -> 
	let output = byw (Fft.dft sign n (sym n (load_array_c n locpm))) in
	let odag = store_array_c n locri output in
	  standard_optimizer odag 
  in

  let vms = CVar "ms" 
  and varp = CVar arp
  and vaip = CVar aip
  and varm = CVar arm
  and vaim = CVar aim
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
	     Expr_assign (varp, CPlus [varp; byvl vms]);
	     Expr_assign (vaip, CPlus [vaip; byvl vms]);
	     Expr_assign (varm, CPlus [varm; CUminus (byvl vms)]);
	     Expr_assign (vaim, CPlus [vaim; CUminus (byvl vms)]);
	     Expr_assign (CVar twarray, CPlus [CVar twarray; 
					       byvl (Integer nt)]);
	     make_volatile_stride (4*n) (CVar rs)
	   ],
	  Asch asch)])
  in

  let tree = 
    Fcn ("static void", name,
	 [Decl (C.realtypep, arp);
	  Decl (C.realtypep, aip);
	  Decl (C.realtypep, arm);
	  Decl (C.realtypep, aim);
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
      "static const hc2c_desc desc = {%d, \"%s\", twinstr, &GENUS, %s};\n\n"
      n name (flops_of tree)
  and register = "X(khc2c_register)"

  in
  let init =
    "\n" ^
    twinstr ^ 
    desc ^
    (declare_register_fcn name) ^
    (Printf.sprintf "{\n%s(p, %s, &desc, HC2C_VIA_RDFT);\n}" register name)
  in

  (unparse tree) ^ "\n" ^ init


let main () =
  begin 
    parse (speclist @ Twiddle.speclist) usage;
    print_string (generate (check_size ()));
  end

let _ = main()
