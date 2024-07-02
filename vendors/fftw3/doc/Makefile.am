SUBDIRS = FAQ

info_TEXINFOS = fftw3.texi
fftw3_TEXINFOS = acknowledgements.texi cindex.texi fftw3.texi findex.texi install.texi intro.texi legacy-fortran.texi license.texi modern-fortran.texi mpi.texi other.texi reference.texi threads.texi tutorial.texi upgrading.texi version.texi rfftwnd.pdf rfftwnd.eps

DVIPS = dvips -Pwww

EQN_IMAGES = equation-dft.png equation-dht.png equation-idft.png	\
equation-redft00.png equation-redft01.png equation-redft10.png		\
equation-redft11.png equation-rodft00.png equation-rodft01.png		\
equation-rodft10.png equation-rodft11.png

EXTRA_DIST = f77_wisdom.f fftw3.pdf html rfftwnd.fig rfftwnd.eps	\
rfftwnd.pdf rfftwnd-for-html.png $(EQN_IMAGES)

html: $(fftw3_TEXINFOS) $(EQN_IMAGES) rfftwnd-for-html.png
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) \
		--html --number-sections -o html fftw3.texi
	for i in $(EQN_IMAGES); do cp -f ${srcdir}/$$i html; done
	cp -f ${srcdir}/rfftwnd-for-html.png html

maintainer-clean-local:
	rm -rf html

if MAINTAINER_MODE
# generate the figure for the manual and distribute the binaries, so that
# people don't need to have fig2dev installed.
rfftwnd.eps: rfftwnd.fig
	fig2dev -L eps -m .7 ${srcdir}/rfftwnd.fig rfftwnd.eps

rfftwnd-for-html.png: rfftwnd.fig
	fig2dev -L png -m 1 ${srcdir}/rfftwnd.fig rfftwnd-for-html.png

rfftwnd.pdf: rfftwnd.fig
	fig2dev -L pdf -m .7 ${srcdir}/rfftwnd.fig rfftwnd.pdf

endif
