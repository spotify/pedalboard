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

let reload_twiddle = ref false

let urs = ref Stride_variable
let uvs = ref Stride_variable
let ums = ref Stride_variable

let speclist = [
  "-dit",
  Arg.Unit(fun () -> ditdif := DIT),
  " generate a DIT codelet";

  "-dif",
  Arg.Unit(fun () -> ditdif := DIF),
  " generate a DIF codelet";

  "-reload-twiddle",
  Arg.Unit(fun () -> reload_twiddle := true),
  " do not collect common twiddle factors";

  "-with-rs",
  Arg.String(fun x -> urs := arg_to_stride x),
  " specialize for given input stride";

  "-with-vs",
  Arg.String(fun x -> uvs := arg_to_stride x),
  " specialize for given vector stride";

  "-with-ms",
  Arg.String(fun x -> ums := arg_to_stride x),
  " specialize for given ms"
]

let generate n =
  let rioarray = "rio"
  and iioarray = "iio"
  and rs = "rs" and vs = "vs"
  and twarray = "W"
  and m = "m" and mb = "mb" and me = "me" and ms = "ms" in

  let sign = !Genutil.sign 
  and name = !Magic.codelet_name in

  let (bytwiddle, num_twiddles, twdesc) = Twiddle.twiddle_policy 0 false in
  let nt = num_twiddles n in

  let svs = either_stride (!uvs) (C.SVar vs)
  and srs = either_stride (!urs) (C.SVar rs) in

  let byw =
    if !reload_twiddle then
      array n (fun v -> bytwiddle n sign (twiddle_array nt twarray))
    else
      let a = bytwiddle n sign (twiddle_array nt twarray)
      in fun v -> a
  in

  let locations = unique_v_array_c n n in

  let ioi = 
    locative_v_array_c n n 
      (C.varray_subscript rioarray svs srs) 
      (C.varray_subscript iioarray svs srs) 
      locations "BUG"
  and ioo = 
    locative_v_array_c n n 
      (C.varray_subscript rioarray svs srs) 
      (C.varray_subscript iioarray svs srs) 
      locations "BUG"
  in

  let lioi = load_v_array_c n n ioi in
  let output =
    match !ditdif with
    | DIT -> array n (fun v -> Fft.dft sign n (byw v (lioi v)))
    | DIF -> array n (fun v -> byw v (Fft.dft sign n (lioi v)))
  in

  let odag = store_v_array_c n n ioo (transpose output) in
  let annot = standard_optimizer odag in

  let vm = CVar m and vmb = CVar mb and vme = CVar me in

  let body = Block (
    [Decl ("INT", m)],
    [For (list_to_comma
	    [Expr_assign (vm, vmb);
	     Expr_assign (CVar twarray, 
			  CPlus [CVar twarray; 
				 ctimes (vmb, Integer nt)])],
	  Binop (" < ", vm, vme),
	  list_to_comma 
	    [Expr_assign (vm, CPlus [vm; Integer 1]);
	     Expr_assign (CVar rioarray, CPlus [CVar rioarray; CVar ms]);
	     Expr_assign (CVar iioarray, CPlus [CVar iioarray; CVar ms]);
	     Expr_assign (CVar twarray, CPlus [CVar twarray; Integer nt]);
	     make_volatile_stride (2*n) (CVar rs);
	     make_volatile_stride (2*0) (CVar vs)
	   ],
	  Asch annot)]) in

  let tree = 
    Fcn (("static void"), name,
	 [Decl (C.realtypep, rioarray);
	  Decl (C.realtypep, iioarray);
	  Decl (C.constrealtypep, twarray);
	  Decl (C.stridetype, rs);
	  Decl (C.stridetype, vs);
	  Decl ("INT", mb);
	  Decl ("INT", me);
	  Decl ("INT", ms)],
         finalize_fcn body)
  in
  let twinstr = 
    Printf.sprintf "static const tw_instr twinstr[] = %s;\n\n" 
      (Twiddle.twinstr_to_c_string (twdesc n))

  and desc = 
    Printf.sprintf
      "static const ct_desc desc = {%d, \"%s\", twinstr, &GENUS, %s, %s, %s, %s};\n\n"
      n name (flops_of tree) 
      (stride_to_solverparm !urs) (stride_to_solverparm !uvs)
      (stride_to_solverparm !ums) 

  and register = 
    match !ditdif with
    | DIT -> "X(kdft_ditsq_register)"
    | DIF -> "X(kdft_difsq_register)"
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
