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

#include <string.h>
#include <stdio.h>

#include "config.h"
#include "my-getopt.h"

int my_optind = 1;
const char *my_optarg = 0;
static const char *scan_pointer = 0;

void my_usage(const char *progname, const struct my_option *opt)
{
    int i;
    size_t col = 0;

    fprintf(stdout, "Usage: %s", progname);
    col += (strlen(progname) + 7);
    for (i = 0; opt[i].long_name; i++) {
	size_t option_len;

	option_len = strlen(opt[i].long_name);
	if (col >= 80 - (option_len + 16)) {
	    fputs("\n\t", stdout);
	    col = 8;
	}
	fprintf(stdout, " [--%s", opt[i].long_name);
	col += (option_len + 4);
	if (opt[i].short_name < 128) {
	    fprintf(stdout, " | -%c", opt[i].short_name);
	    col += 5;
	}
	switch (opt[i].argtype) {
	    case REQARG:
		 fputs(" arg]", stdout);
		 col += 5;
		 break;
	    case OPTARG:
		 fputs(" [arg]]", stdout);
		 col += 10;
		 break;
	    default:
		 fputs("]", stdout);
		 col++;
	}
    }

    fputs ("\n", stdout);
}

int my_getopt(int argc, char *argv[], const struct my_option *optarray)
{
     const char *p;
     const struct my_option *l;

     if (scan_pointer && *scan_pointer) {
	  /* continue a previously scanned argv[] element */
	  p = scan_pointer;
	  goto short_option;
     } else {
	  /* new argv[] element */
	  if (my_optind >= argc)
	       return -1; /* no more options */

	  p = argv[my_optind];
     
	  if (*p++ != '-')  
	       return (-1); /* not an option */

	  if (!*p) 
	       return (-1); /* string is exactly '-' */
	       
	  ++my_optind;
     }

     if (*p == '-') {
	  /* long option */
	  scan_pointer = 0;
	  my_optarg = 0;

	  ++p;
	  
	  for (l = optarray; l->short_name; ++l) {
	       size_t len = strlen(l->long_name);
	       if (!strncmp(l->long_name, p, len) && 
		   (!p[len] || p[len] == '=')) {
		    switch (l->argtype) {
			case NOARG: 
			     goto ok;
			case OPTARG: 
			     if (p[len] == '=')
				  my_optarg = p + len + 1;
			     goto ok;
			case REQARG: 
			     if (p[len] == '=') {
				  my_optarg = p + len + 1;
				  goto ok;
			     }
			     if (my_optind >= argc) {
				  fprintf(stderr, 
					  "option --%s requires an argument\n",
					  l->long_name);
				  return '?';
			     }
			     my_optarg = argv[my_optind];
			     ++my_optind;
			     goto ok;
		    }
	       }
	  }
     } else {
     short_option:
	  scan_pointer = 0;
	  my_optarg = 0;

	  for (l = optarray; l->short_name; ++l) {
	       if (l->short_name == (char)l->short_name &&
		   *p == l->short_name) {
		    ++p;
		    switch (l->argtype) {
			case NOARG: 
			     scan_pointer = p;
			     goto ok;
			case OPTARG: 
			     if (*p)
				  my_optarg = p;
			     goto ok;
			case REQARG: 
			     if (*p) {
				  my_optarg = p;
			     } else {
				  if (my_optind >= argc) {
				       fprintf(stderr, 
					  "option -%c requires an argument\n",
					  l->short_name);
				       return '?';
				  }
				  my_optarg = argv[my_optind];
				  ++my_optind;
			     }
			     goto ok;
		    }
	       }
	  }
     }

     fprintf(stderr, "unrecognized option %s\n", argv[my_optind - 1]);
     return '?';

 ok:
     return l->short_name;
}

