FFTW is a free collection of fast C routines for computing the
Discrete Fourier Transform in one or more dimensions.  It includes
complex, real, symmetric, and parallel transforms, and can handle
arbitrary array sizes efficiently.  FFTW is typically faster than
other publically-available FFT implementations, and is even
competitive with vendor-tuned libraries.  (See our web page
http://fftw.org/ for extensive benchmarks.)  To achieve this
performance, FFTW uses novel code-generation and runtime
self-optimization techniques (along with many other tricks).

The doc/ directory contains the manual in texinfo, PDF, info, and HTML
formats.  Frequently asked questions and answers can be found in the
doc/FAQ/ directory in ASCII and HTML.

For a quick introduction to calling FFTW, see the "Tutorial" section
of the manual.

INSTALLATION
------------

INSTALLATION FROM AN OFFICIAL RELEASE:

Please read chapter 10 "Installation and Customization" of the manual.
In short:

     ./configure
     make
     make install

INSTALLATION FROM THE GIT REPOSITORY:

First, install these programs:

  ocaml, ocamlbuild, autoconf, automake, indent, and libtool.

You also need the ocaml Num library, which was standard in Ocaml but
was removed without warning in OCaml 4.06.0 (3 Nov 2017).  On Fedora
30, try installing the ocaml-num-devel package.

Then, execute

    sh bootstrap.sh
    make
    
The bootstrap.sh script runs configure directly, but if you need to
re-run configure, you must pass the --enable-maintainer-mode flag:

    ./configure --enable-maintainer-mode [OTHER CONFIGURE FLAGS]

Alternatively, you can run

    sh mkdist.sh

which will run the entire bootstrapping process and generate
.tar.gz files similar to those for official releases.

CONTACTS
--------

FFTW was written by Matteo Frigo and Steven G. Johnson.  You can
contact them at fftw@fftw.org.  The latest version of FFTW,
benchmarks, links, and other information can be found at the FFTW home
page (http://www.fftw.org).  You can also sign up to the fftw-announce
Google group to receive (infrequent) updates and information about new
releases.
