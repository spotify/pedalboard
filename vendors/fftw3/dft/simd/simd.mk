AM_CPPFLAGS = -I $(top_srcdir)
EXTRA_DIST = $(SIMD_CODELETS) genus.c codlist.c

if MAINTAINER_MODE
$(EXTRA_DIST): Makefile
	(							\
	echo "/* Generated automatically.  DO NOT EDIT! */";	\
	echo "#define SIMD_HEADER \"$(SIMD_HEADER)\"";		\
	echo "#include \"../common/"$*".c\"";			\
	) >$@
endif # MAINTAINER_MODE

