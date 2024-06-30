AM_CPPFLAGS = -I $(top_srcdir)
SUBDIRS = 

noinst_LTLIBRARIES = libreodft.la

# no longer used due to numerical problems
EXTRA_DIST = reodft11e-r2hc.c redft00e-r2hc.c rodft00e-r2hc.c

libreodft_la_SOURCES = conf.c reodft.h reodft010e-r2hc.c	\
reodft11e-radix2.c reodft11e-r2hc-odd.c redft00e-r2hc-pad.c	\
rodft00e-r2hc-pad.c reodft00e-splitradix.c
# redft00e-r2hc.c rodft00e-r2hc.c reodft11e-r2hc.c
