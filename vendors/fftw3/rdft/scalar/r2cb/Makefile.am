# This Makefile.am specifies a set of codelets, efficient transforms
# of small sizes, that are used as building blocks (kernels) by FFTW
# to build up large transforms, as well as the options for generating
# and compiling them.

# You can customize FFTW for special needs, e.g. to handle certain
# sizes more efficiently, by adding new codelets to the lists of those
# included by default.  If you change the list of codelets, any new
# ones you added will be automatically generated when you run the
# bootstrap script (see "Generating your own code" in the FFTW
# manual).

###########################################################################
AM_CPPFLAGS = -I $(top_srcdir)
noinst_LTLIBRARIES = librdft_scalar_r2cb.la

###########################################################################
# r2cb_<n> is a hard-coded complex-to-real FFT of size <n> (base cases
# of real-output FFT recursion)
R2CB = r2cb_2.c r2cb_3.c r2cb_4.c r2cb_5.c r2cb_6.c r2cb_7.c r2cb_8.c	\
r2cb_9.c r2cb_10.c r2cb_11.c r2cb_12.c r2cb_13.c r2cb_14.c r2cb_15.c	\
r2cb_16.c r2cb_32.c r2cb_64.c r2cb_128.c r2cb_20.c r2cb_25.c
# r2cb_30.c r2cb_40.c r2cb_50.c

###########################################################################
# hb_<r> is a "twiddle" FFT of size <r>, implementing a radix-r DIF
# step for a real-output FFT.  Every hb codelet must have a
# corresponding r2cbIII codelet (see below)!
HB = hb_2.c hb_3.c hb_4.c hb_5.c hb_6.c hb_7.c hb_8.c hb_9.c	\
hb_10.c hb_12.c hb_15.c hb_16.c hb_32.c hb_64.c \
hb_20.c hb_25.c # hb_30.c hb_40.c hb_50.c

# like hb, but generates part of its trig table on the fly (good for large n)
HB2 = hb2_4.c hb2_8.c hb2_16.c hb2_32.c \
hb2_5.c hb2_20.c hb2_25.c

# an r2cb transform where the output is shifted by half a sample (input
# is multiplied by a phase).  This is needed as part of the DIF recursion;
# every hb_<r> or hb2_<r> codelet should have a corresponding r2cbIII_<r>
R2CBIII = r2cbIII_2.c r2cbIII_3.c r2cbIII_4.c r2cbIII_5.c r2cbIII_6.c	\
r2cbIII_7.c r2cbIII_8.c r2cbIII_9.c r2cbIII_10.c r2cbIII_12.c		\
r2cbIII_15.c r2cbIII_16.c r2cbIII_32.c r2cbIII_64.c \
r2cbIII_20.c r2cbIII_25.c # r2cbIII_30.c r2cbIII_40.c r2cbIII_50.c

###########################################################################
# hc2cb_<r> is a "twiddle" FFT of size <r>, implementing a radix-r DIF
# step for a real-input FFT with rdft2-style output.  <r> must be even.
HC2CB = hc2cb_2.c hc2cb_4.c hc2cb_6.c hc2cb_8.c hc2cb_10.c hc2cb_12.c	\
hc2cb_16.c hc2cb_32.c \
hc2cb_20.c # hc2cb_30.c

HC2CBDFT = hc2cbdft_2.c hc2cbdft_4.c hc2cbdft_6.c hc2cbdft_8.c	\
hc2cbdft_10.c hc2cbdft_12.c hc2cbdft_16.c hc2cbdft_32.c \
hc2cbdft_20.c # hc2cbdft_30.c

# like hc2cb, but generates part of its trig table on the fly (good
# for large n)
HC2CB2 = hc2cb2_4.c hc2cb2_8.c hc2cb2_16.c hc2cb2_32.c \
hc2cb2_20.c # hc2cb2_30.c
HC2CBDFT2 = hc2cbdft2_4.c hc2cbdft2_8.c hc2cbdft2_16.c hc2cbdft2_32.c \
hc2cbdft2_20.c # hc2cbdft2_30.c

###########################################################################
ALL_CODELETS = $(R2CB) $(HB) $(HB2) $(R2CBIII) $(HC2CB) $(HC2CB2)	\
$(HC2CBDFT) $(HC2CBDFT2)

BUILT_SOURCES= $(ALL_CODELETS) $(CODLIST)

librdft_scalar_r2cb_la_SOURCES = $(BUILT_SOURCES)

SOLVTAB_NAME = X(solvtab_rdft_r2cb)
XRENAME=X

# special rules for regenerating codelets.
include $(top_srcdir)/support/Makefile.codelets

if MAINTAINER_MODE
FLAGS_R2CB=$(RDFT_FLAGS_COMMON) -sign 1
FLAGS_HB=$(RDFT_FLAGS_COMMON) -sign 1
FLAGS_HB2=$(RDFT_FLAGS_COMMON) -sign 1 -twiddle-log3 -precompute-twiddles
FLAGS_HC2CB=$(RDFT_FLAGS_COMMON) -sign 1
FLAGS_HC2CB2=$(RDFT_FLAGS_COMMON) -sign 1 -twiddle-log3 -precompute-twiddles
FLAGS_R2CBIII=$(RDFT_FLAGS_COMMON) -sign 1

r2cb_%.c:  $(CODELET_DEPS) $(GEN_R2CB)
	($(PRELUDE_COMMANDS_RDFT); $(TWOVERS) $(GEN_R2CB) $(FLAGS_R2CB) -n $* -name r2cb_$* -include "rdft/scalar/r2cb.h") | $(ADD_DATE) | $(INDENT) >$@

hb_%.c:  $(CODELET_DEPS) $(GEN_HC2HC)
	($(PRELUDE_COMMANDS_RDFT); $(TWOVERS) $(GEN_HC2HC) $(FLAGS_HB) -n $* -dif -name hb_$* -include "rdft/scalar/hb.h") | $(ADD_DATE) | $(INDENT) >$@

hb2_%.c:  $(CODELET_DEPS) $(GEN_HC2HC)
	($(PRELUDE_COMMANDS_RDFT); $(TWOVERS) $(GEN_HC2HC) $(FLAGS_HB2) -n $* -dif -name hb2_$* -include "rdft/scalar/hb.h") | $(ADD_DATE) | $(INDENT) >$@

r2cbIII_%.c:  $(CODELET_DEPS) $(GEN_R2CB)
	($(PRELUDE_COMMANDS_RDFT); $(TWOVERS) $(GEN_R2CB) $(FLAGS_R2CB) -n $* -name r2cbIII_$* -dft-III -include "rdft/scalar/r2cbIII.h") | $(ADD_DATE) | $(INDENT) >$@

hc2cb_%.c:  $(CODELET_DEPS) $(GEN_HC2C)
	($(PRELUDE_COMMANDS_RDFT); $(TWOVERS) $(GEN_HC2C) $(FLAGS_HC2CB) -n $* -dif -name hc2cb_$* -include "rdft/scalar/hc2cb.h") | $(ADD_DATE) | $(INDENT) >$@

hc2cb2_%.c:  $(CODELET_DEPS) $(GEN_HC2C)
	($(PRELUDE_COMMANDS_RDFT); $(TWOVERS) $(GEN_HC2C) $(FLAGS_HC2CB2) -n $* -dif -name hc2cb2_$* -include "rdft/scalar/hc2cb.h") | $(ADD_DATE) | $(INDENT) >$@

hc2cbdft_%.c:  $(CODELET_DEPS) $(GEN_HC2CDFT)
	($(PRELUDE_COMMANDS_RDFT); $(TWOVERS) $(GEN_HC2CDFT) $(FLAGS_HC2CB) -n $* -dif -name hc2cbdft_$* -include "rdft/scalar/hc2cb.h") | $(ADD_DATE) | $(INDENT) >$@

hc2cbdft2_%.c:  $(CODELET_DEPS) $(GEN_HC2CDFT)
	($(PRELUDE_COMMANDS_RDFT); $(TWOVERS) $(GEN_HC2CDFT) $(FLAGS_HC2CB) -n $* -dif -name hc2cbdft2_$* -include "rdft/scalar/hc2cb.h") | $(ADD_DATE) | $(INDENT) >$@

endif # MAINTAINER_MODE
