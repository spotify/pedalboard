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

#ifndef __REODFT_H__
#define __REODFT_H__

#include "kernel/ifftw.h"
#include "rdft/rdft.h"

#define REODFT_KINDP(k) ((k) >= REDFT00 && (k) <= RODFT11)

void X(redft00e_r2hc_register)(planner *p);
void X(redft00e_r2hc_pad_register)(planner *p);
void X(rodft00e_r2hc_register)(planner *p);
void X(rodft00e_r2hc_pad_register)(planner *p);
void X(reodft00e_splitradix_register)(planner *p);
void X(reodft010e_r2hc_register)(planner *p);
void X(reodft11e_r2hc_register)(planner *p);
void X(reodft11e_radix2_r2hc_register)(planner *p);
void X(reodft11e_r2hc_odd_register)(planner *p);

/* configurations */
void X(reodft_conf_standard)(planner *p);

#endif /* __REODFT_H__ */
