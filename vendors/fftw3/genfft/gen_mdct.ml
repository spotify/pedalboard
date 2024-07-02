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
let normalization = ref 1

type mode =
  | MDCT
  | MDCT_MP3
  | MDCT_VORBIS
  | MDCT_WINDOW
  | MDCT_WINDOW_SYM
  | IMDCT
  | IMDCT_MP3
  | IMDCT_VORBIS
  | IMDCT_WINDOW
  | IMDCT_WINDOW_SYM
  | NONE

let mode = ref NONE

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

  "-normalization",
  Arg.String(fun x -> normalization := int_of_string x),
  " normalization integer to divide by";

  "-mdct",
  Arg.Unit(fun () -> mode := MDCT),
  " generate an MDCT codelet";

  "-mdct-mp3",
  Arg.Unit(fun () -> mode := MDCT_MP3),
  " generate an MDCT codelet with MP3 windowing";

  "-mdct-window",
  Arg.Unit(fun () -> mode := MDCT_WINDOW),
  " generate an MDCT codelet with window array";

  "-mdct-window-sym",
  Arg.Unit(fun () -> mode := MDCT_WINDOW_SYM),
  " generate an MDCT codelet with symmetric window array";

  "-imdct",
  Arg.Unit(fun () -> mode := IMDCT),
  " generate an IMDCT codelet";

  "-imdct-mp3",
  Arg.Unit(fun () -> mode := IMDCT_MP3),
  " generate an IMDCT codelet with MP3 windowing";

  "-imdct-window",
  Arg.Unit(fun () -> mode := IMDCT_WINDOW),
  " generate an IMDCT codelet with window array";

  "-imdct-window-sym",
  Arg.Unit(fun () -> mode := IMDCT_WINDOW_SYM),
  " generate an IMDCT codelet with symmetric window array";
]

let unity_window n i = Complex.one

(* MP3 window(k) = sin(pi/(2n) * (k + 1/2)) *)
let mp3_window n k = 
  Complex.imag (Complex.exp (8 * n) (2*k + 1))

(* Vorbis window(k) = sin(pi/2 * (mp3_window(k))^2)
    ... this is transcendental, though, so we can't do it with our
        current Complex.exp function *)

let window_array n w =
    array n (fun i ->
      let stride = C.SInteger 1
      and klass = Unique.make () in
      let refr = C.array_subscript w stride i in
      let kr = Variable.make_constant klass refr in
      load_r (kr, kr))

let load_window w n i = w i
let load_window_sym w n i = w (if (i < n) then i else (2*n - 1 - i))

(* fixme: use same locations for input and output so that it works in-place? *)

(* Note: only correct for even n! *)
let load_array_mdct window n rarr iarr locations =
  let twon = 2 * n in
  let arr = load_array_c twon 
      (locative_array_c twon rarr iarr locations "BUG") in
  let arrw = fun i -> Complex.times (window n i) (arr i) in
  array n
    ((Complex.times Complex.half) @@
     (fun i ->
       if (i < n/2) then
	 Complex.uminus (Complex.plus [arrw (i + n + n/2); 
				       arrw (n + n/2 - 1 - i)])
       else
	 Complex.plus [arrw (i - n/2); 
		       Complex.uminus (arrw (n + n/2 - 1 - i))]))

let store_array_mdct window n rarr iarr locations arr =
  store_array_r n (locative_array_c n rarr iarr locations "BUG") arr

let load_array_imdct window n rarr iarr locations =
  load_array_c n (locative_array_c n rarr iarr locations "BUG")

let store_array_imdct window n rarr iarr locations arr =
  let n2 = n/2 in
  let threen2 = 3*n2 in
  let arr2 = fun i ->
    if (i < n2) then
      arr (i + n2)
    else if (i < threen2) then
      Complex.uminus (arr (threen2 - 1 - i))
    else
      Complex.uminus (arr (i - threen2))
  in
  let arr2w = fun i -> Complex.times (window n i) (arr2 i) in
  let twon = 2 * n in
  store_array_r twon (locative_array_c twon rarr iarr locations "BUG") arr2w

let window_param = function
    MDCT_WINDOW -> true
  | MDCT_WINDOW_SYM -> true
  | IMDCT_WINDOW -> true
  | IMDCT_WINDOW_SYM -> true
  | _ -> false

let generate n mode =
  let iarray = "I"
  and oarray = "O"
  and istride = "istride"
  and ostride = "ostride" 
  and window = "W" 
  and name = !Magic.codelet_name in

  let vistride = either_stride (!uistride) (C.SVar istride)
  and vostride = either_stride (!uostride) (C.SVar ostride)
  in

  let sivs = stride_to_string "ovs" !uovstride in
  let sovs = stride_to_string "ivs" !uivstride in

  let (transform, load_input, store_output) = match mode with
  | MDCT -> Trig.dctIV, load_array_mdct unity_window,
      store_array_mdct unity_window
  | MDCT_MP3 -> Trig.dctIV, load_array_mdct mp3_window,
      store_array_mdct unity_window
  | MDCT_WINDOW -> Trig.dctIV, load_array_mdct
	(load_window (window_array (2 * n) window)),
      store_array_mdct unity_window
  | MDCT_WINDOW_SYM -> Trig.dctIV, load_array_mdct
	(load_window_sym (window_array n window)),
      store_array_mdct unity_window
  | IMDCT -> Trig.dctIV, load_array_imdct unity_window,
      store_array_imdct unity_window
  | IMDCT_MP3 -> Trig.dctIV, load_array_imdct unity_window,
      store_array_imdct mp3_window
  | IMDCT_WINDOW -> Trig.dctIV, load_array_imdct unity_window,
      store_array_imdct (load_window (window_array (2 * n) window))
  | IMDCT_WINDOW_SYM -> Trig.dctIV, load_array_imdct unity_window,
      store_array_imdct (load_window_sym (window_array n window))
  | _ -> failwith "must specify transform kind"
  in
    
  let locations = unique_array_c (2*n) in
  let input = 
    load_input n
      (C.array_subscript iarray vistride)
      (C.array_subscript "BUG" vistride)
      locations
  in
  let output = (Complex.times (Complex.inverse_int !normalization)) 
    @@ (transform n input) in
  let odag =
    store_output n
      (C.array_subscript oarray vostride)
      (C.array_subscript "BUG" vostride)
      locations 
      output
  in
  let annot = standard_optimizer odag in

  let tree =
    Fcn ("void", name,
	 ([Decl (C.constrealtypep, iarray);
	   Decl (C.realtypep, oarray)]
	  @ (if stride_fixed !uistride then [] 
               else [Decl (C.stridetype, istride)])
	  @ (if stride_fixed !uostride then [] 
	       else [Decl (C.stridetype, ostride)])
	  @ (choose_simd []
	       (if stride_fixed !uivstride then [] else 
	       [Decl ("int", sivs)]))
	  @ (choose_simd []
	       (if stride_fixed !uovstride then [] else 
	       [Decl ("int", sovs)]))
	  @ (if (not (window_param mode)) then [] 
	       else [Decl (C.constrealtypep, window)])
	 ),
	 finalize_fcn (Asch annot))

  in
  (unparse tree) ^ "\n"


let main () =
  begin
    parse speclist usage;
    print_string (generate (check_size ()) !mode);
  end

let _ = main()
