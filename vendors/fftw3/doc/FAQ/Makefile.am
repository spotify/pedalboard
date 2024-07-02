BFNNCONV_SRC = bfnnconv.pl m-ascii.pl m-html.pl m-info.pl m-lout.pl m-post.pl

FAQ = fftw-faq.ascii fftw-faq.html
EXTRA_DIST = fftw-faq.bfnn $(FAQ) $(BFNNCONV_SRC) html.refs

html.refs2: html.refs
	cp -f ${srcdir}/html.refs html.refs2

fftw-faq.ascii: fftw-faq.html

# generates both fftw-faq.html and fftw-faq.ascii
fftw-faq.html: $(BFNNCONV_SRC) fftw-faq.bfnn html.refs2
	@echo converting...
	perl -I${srcdir} ${srcdir}/bfnnconv.pl < ${srcdir}/fftw-faq.bfnn
	@echo converting again...
	perl -I${srcdir} ${srcdir}/bfnnconv.pl < ${srcdir}/fftw-faq.bfnn
	rm -f fftw-faq.ascii
	mv stdin.ascii fftw-faq.ascii
	rm -rf fftw-faq.html
	mv -f stdin.html fftw-faq.html

faq: $(FAQ)

clean-local:
	rm -f *~ core a.out *.lout *.ps *.info *.ascii *.xrefdb *.post
	rm -rf *.html html.refs2
