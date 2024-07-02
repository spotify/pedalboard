# This file contains a standard list of RDFT SIMD codelets.  It is
# included by common/Makefile to generate the C files with the actual
# codelets in them.  It is included by {sse,sse2,...}/Makefile to
# generate and compile stub files that include common/*.c

# You can customize FFTW for special needs, e.g. to handle certain
# sizes more efficiently, by adding new codelets to the lists of those
# included by default.  If you change the list of codelets, any new
# ones you added will be automatically generated when you run the
# bootstrap script (see "Generating your own code" in the FFTW
# manual).

HC2CFDFTV = hc2cfdftv_2.c hc2cfdftv_4.c hc2cfdftv_6.c hc2cfdftv_8.c	\
hc2cfdftv_10.c hc2cfdftv_12.c hc2cfdftv_16.c hc2cfdftv_32.c		\
hc2cfdftv_20.c

HC2CBDFTV = hc2cbdftv_2.c hc2cbdftv_4.c hc2cbdftv_6.c hc2cbdftv_8.c	\
hc2cbdftv_10.c hc2cbdftv_12.c hc2cbdftv_16.c hc2cbdftv_32.c		\
hc2cbdftv_20.c

###########################################################################
SIMD_CODELETS = $(HC2CFDFTV) $(HC2CBDFTV)
