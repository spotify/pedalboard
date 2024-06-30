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

#ifndef __MY_GETOPT_H__
#define __MY_GETOPT_H__

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

enum { REQARG, OPTARG, NOARG };

struct my_option {
     const char *long_name;
     int argtype;
     int short_name;
};

extern int my_optind;
extern const char *my_optarg;

extern void my_usage(const char *progname, const struct my_option *opt);
extern int my_getopt(int argc, char *argv[], const struct my_option *optarray);

#ifdef __cplusplus
}                               /* extern "C" */
#endif                          /* __cplusplus */

#endif /* __MY_GETOPT_H__ */
