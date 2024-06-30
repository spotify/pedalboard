/*
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
 */

/*
 * This header file must include every file or define every
 * type or macro which is required to compile a codelet.
 */

#ifndef __RDFT_CODELET_H__
#define __RDFT_CODELET_H__

#include "kernel/ifftw.h"

/**************************************************************
 * types of codelets
 **************************************************************/

/* FOOab, with a,b in {0,1}, denotes the FOO transform
   where a/b say whether the input/output are shifted by
   half a sample/slot. */

typedef enum {
     R2HC00, R2HC01, R2HC10, R2HC11,
     HC2R00, HC2R01, HC2R10, HC2R11,
     DHT, 
     REDFT00, REDFT01, REDFT10, REDFT11, /* real-even == DCT's */
     RODFT00, RODFT01, RODFT10, RODFT11  /*  real-odd == DST's */
} rdft_kind;

/* standard R2HC/HC2R transforms are unshifted */
#define R2HC R2HC00
#define HC2R HC2R00

#define R2HCII R2HC01
#define HC2RIII HC2R10

/* (k) >= R2HC00 produces a warning under gcc because checking x >= 0
   is superfluous for unsigned values...but it is needed because other
   compilers (e.g. icc) may define the enum to be a signed int...grrr. */
#define R2HC_KINDP(k) ((k) >= R2HC00 && (k) <= R2HC11) /* uses kr2hc_genus */
#define HC2R_KINDP(k) ((k) >= HC2R00 && (k) <= HC2R11) /* uses khc2r_genus */

#define R2R_KINDP(k) ((k) >= DHT) /* uses kr2r_genus */

#define REDFT_KINDP(k) ((k) >= REDFT00 && (k) <= REDFT11)
#define RODFT_KINDP(k) ((k) >= RODFT00 && (k) <= RODFT11)
#define REODFT_KINDP(k) ((k) >= REDFT00 && (k) <= RODFT11)

/* codelets with real input (output) and complex output (input) */
typedef struct kr2c_desc_s kr2c_desc;

typedef struct {
     rdft_kind kind;
     INT vl;
} kr2c_genus;

struct kr2c_desc_s {
     INT n;    /* size of transform computed */
     const char *nam;
     opcnt ops;
     const kr2c_genus *genus;
};

typedef void (*kr2c) (R *R0, R *R1, R *Cr, R *Ci,
		      stride rs, stride csr, stride csi,
		      INT vl, INT ivs, INT ovs);
void X(kr2c_register)(planner *p, kr2c codelet, const kr2c_desc *desc);

/* half-complex to half-complex DIT/DIF codelets: */
typedef struct hc2hc_desc_s hc2hc_desc;

typedef struct {
     rdft_kind kind;
     INT vl;
} hc2hc_genus;

struct hc2hc_desc_s {
     INT radix;
     const char *nam;
     const tw_instr *tw;
     const hc2hc_genus *genus;
     opcnt ops;
};

typedef void (*khc2hc) (R *rioarray, R *iioarray, const R *W,
			stride rs, INT mb, INT me, INT ms);
void X(khc2hc_register)(planner *p, khc2hc codelet, const hc2hc_desc *desc);

/* half-complex to rdft2-complex DIT/DIF codelets: */
typedef struct hc2c_desc_s hc2c_desc;

typedef enum {
     HC2C_VIA_RDFT,
     HC2C_VIA_DFT
} hc2c_kind;

typedef struct {
     int (*okp)(
	  const R *Rp, const R *Ip, const R *Rm, const R *Im, 
	  INT rs, INT mb, INT me, INT ms, 
	  const planner *plnr);
     rdft_kind kind;
     INT vl;
} hc2c_genus;

struct hc2c_desc_s {
     INT radix;
     const char *nam;
     const tw_instr *tw;
     const hc2c_genus *genus;
     opcnt ops;
};

typedef void (*khc2c) (R *Rp, R *Ip, R *Rm, R *Im, const R *W,
		       stride rs, INT mb, INT me, INT ms);
void X(khc2c_register)(planner *p, khc2c codelet, const hc2c_desc *desc,
		       hc2c_kind hc2ckind);

extern const solvtab X(solvtab_rdft_r2cf);
extern const solvtab X(solvtab_rdft_r2cb);
extern const solvtab X(solvtab_rdft_sse2);
extern const solvtab X(solvtab_rdft_avx);
extern const solvtab X(solvtab_rdft_avx_128_fma);
extern const solvtab X(solvtab_rdft_avx2);
extern const solvtab X(solvtab_rdft_avx2_128);
extern const solvtab X(solvtab_rdft_avx512);
extern const solvtab X(solvtab_rdft_kcvi);
extern const solvtab X(solvtab_rdft_altivec);
extern const solvtab X(solvtab_rdft_vsx);
extern const solvtab X(solvtab_rdft_neon);
extern const solvtab X(solvtab_rdft_generic_simd128);
extern const solvtab X(solvtab_rdft_generic_simd256);

/* real-input & output DFT-like codelets (DHT, etc.) */
typedef struct kr2r_desc_s kr2r_desc;

typedef struct {
     INT vl;
} kr2r_genus;

struct kr2r_desc_s {
     INT n;    /* size of transform computed */
     const char *nam;
     opcnt ops;
     const kr2r_genus *genus;
     rdft_kind kind;
};

typedef void (*kr2r) (const R *I, R *O, stride is, stride os,
		      INT vl, INT ivs, INT ovs);
void X(kr2r_register)(planner *p, kr2r codelet, const kr2r_desc *desc);

extern const solvtab X(solvtab_rdft_r2r);

#endif				/* __RDFT_CODELET_H__ */
