#!/usr/bin/perl -w
# Generate Fortran 2003 interfaces from a sequence of C function declarations
# of the form (one per line):
#     extern <type> <name>(...args...)
#     extern <type> <name>(...args...)
#     ...
# with no line breaks within a given function.  (It's too much work to
# write a general parser, since we just have to handle FFTW's header files.)

sub canonicalize_type {
    my($type);
    ($type) = @_;
    $type =~ s/ +/ /g;
    $type =~ s/^ //;
    $type =~ s/ $//;
    $type =~ s/([^\* ])\*/$1 \*/g;
    return $type;
}

# C->Fortran map of supported return types
%return_types = (
    "int" => "integer(C_INT)",
    "ptrdiff_t" => "integer(C_INTPTR_T)",
    "size_t" => "integer(C_SIZE_T)",
    "double" => "real(C_DOUBLE)",
    "float" => "real(C_FLOAT)",
    "long double" => "real(C_LONG_DOUBLE)",
    "__float128" => "real(16)",
    "fftw_plan" => "type(C_PTR)",
    "fftwf_plan" => "type(C_PTR)",
    "fftwl_plan" => "type(C_PTR)",
    "fftwq_plan" => "type(C_PTR)",
    "void *" => "type(C_PTR)",
    "char *" => "type(C_PTR)",
    "double *" => "type(C_PTR)",
    "float *" => "type(C_PTR)",
    "long double *" => "type(C_PTR)",
    "__float128 *" => "type(C_PTR)",
    "fftw_complex *" => "type(C_PTR)",
    "fftwf_complex *" => "type(C_PTR)",
    "fftwl_complex *" => "type(C_PTR)",
    "fftwq_complex *" => "type(C_PTR)",
    );

# C->Fortran map of supported argument types
%arg_types = (
    "int" => "integer(C_INT), value",
    "unsigned" => "integer(C_INT), value",
    "size_t" => "integer(C_SIZE_T), value",
    "ptrdiff_t" => "integer(C_INTPTR_T), value",

    "fftw_r2r_kind" => "integer(C_FFTW_R2R_KIND), value",
    "fftwf_r2r_kind" => "integer(C_FFTW_R2R_KIND), value",
    "fftwl_r2r_kind" => "integer(C_FFTW_R2R_KIND), value",
    "fftwq_r2r_kind" => "integer(C_FFTW_R2R_KIND), value",

    "double" => "real(C_DOUBLE), value",
    "float" => "real(C_FLOAT), value",
    "long double" => "real(C_LONG_DOUBLE), value",
    "__float128" => "real(16), value",

    "fftw_complex" => "complex(C_DOUBLE_COMPLEX), value",
    "fftwf_complex" => "complex(C_DOUBLE_COMPLEX), value",
    "fftwl_complex" => "complex(C_LONG_DOUBLE), value",
    "fftwq_complex" => "complex(16), value",

    "fftw_plan" => "type(C_PTR), value",
    "fftwf_plan" => "type(C_PTR), value",
    "fftwl_plan" => "type(C_PTR), value",
    "fftwq_plan" => "type(C_PTR), value",
    "const fftw_plan" => "type(C_PTR), value",
    "const fftwf_plan" => "type(C_PTR), value",
    "const fftwl_plan" => "type(C_PTR), value",
    "const fftwq_plan" => "type(C_PTR), value",

    "const int *" => "integer(C_INT), dimension(*), intent(in)",
    "ptrdiff_t *" => "integer(C_INTPTR_T), intent(out)",
    "const ptrdiff_t *" => "integer(C_INTPTR_T), dimension(*), intent(in)",

    "const fftw_r2r_kind *" => "integer(C_FFTW_R2R_KIND), dimension(*), intent(in)",
    "const fftwf_r2r_kind *" => "integer(C_FFTW_R2R_KIND), dimension(*), intent(in)",
    "const fftwl_r2r_kind *" => "integer(C_FFTW_R2R_KIND), dimension(*), intent(in)",
    "const fftwq_r2r_kind *" => "integer(C_FFTW_R2R_KIND), dimension(*), intent(in)",

    "double *" => "real(C_DOUBLE), dimension(*), intent(out)",
    "float *" => "real(C_FLOAT), dimension(*), intent(out)",
    "long double *" => "real(C_LONG_DOUBLE), dimension(*), intent(out)",
    "__float128 *" => "real(16), dimension(*), intent(out)",

    "fftw_complex *" => "complex(C_DOUBLE_COMPLEX), dimension(*), intent(out)",
    "fftwf_complex *" => "complex(C_FLOAT_COMPLEX), dimension(*), intent(out)",
    "fftwl_complex *" => "complex(C_LONG_DOUBLE_COMPLEX), dimension(*), intent(out)",
    "fftwq_complex *" => "complex(16), dimension(*), intent(out)",

    "const fftw_iodim *" => "type(fftw_iodim), dimension(*), intent(in)",
    "const fftwf_iodim *" => "type(fftwf_iodim), dimension(*), intent(in)",
    "const fftwl_iodim *" => "type(fftwl_iodim), dimension(*), intent(in)",
    "const fftwq_iodim *" => "type(fftwq_iodim), dimension(*), intent(in)",

    "const fftw_iodim64 *" => "type(fftw_iodim64), dimension(*), intent(in)",
    "const fftwf_iodim64 *" => "type(fftwf_iodim64), dimension(*), intent(in)",
    "const fftwl_iodim64 *" => "type(fftwl_iodim64), dimension(*), intent(in)",
    "const fftwq_iodim64 *" => "type(fftwq_iodim64), dimension(*), intent(in)",

    "void *" => "type(C_PTR), value",
    "FILE *" => "type(C_PTR), value",

    "const char *" => "character(C_CHAR), dimension(*), intent(in)",

    "fftw_write_char_func" => "type(C_FUNPTR), value",
    "fftwf_write_char_func" => "type(C_FUNPTR), value",
    "fftwl_write_char_func" => "type(C_FUNPTR), value",
    "fftwq_write_char_func" => "type(C_FUNPTR), value",
    "fftw_read_char_func" => "type(C_FUNPTR), value",
    "fftwf_read_char_func" => "type(C_FUNPTR), value",
    "fftwl_read_char_func" => "type(C_FUNPTR), value",
    "fftwq_read_char_func" => "type(C_FUNPTR), value",

    # Although the MPI standard defines this type as simply "integer",
    # if we use integer without a 'C_' kind in a bind(C) interface then
    # gfortran complains.  Instead, since MPI also requires the C type
    # MPI_Fint to match Fortran integers, we use the size of this type
    # (extracted by configure and substituted by the Makefile).
    "MPI_Comm" => "integer(C_MPI_FINT), value"
    );

while (<>) {
    next if /^ *$/;
    if (/^ *extern +([a-zA-Z_0-9 ]+[ \*]) *([a-zA-Z_0-9]+) *\((.*)\) *$/) {
	$ret = &canonicalize_type($1);
	$name = $2;

	$args = $3;
	$args =~ s/^ *void *$//;

	$bad = ($ret ne "void") && !exists($return_types{$ret});	
	foreach $arg (split(/ *, */, $args)) {
	    $arg =~ /^([a-zA-Z_0-9 ]+[ \*]) *([a-zA-Z_0-9]+) *$/;
	    $argtype = &canonicalize_type($1);
	    $bad = 1 if !exists($arg_types{$argtype});
	}
	if ($bad) {
	    print "! Unable to generate Fortran interface for $name\n";
	    next;
	}

	# any function taking an MPI_Comm arg needs a C wrapper (grr).
	if ($args =~ /MPI_Comm/) {
	    $cname = $name . "_f03";
	}
	else {
	    $cname = $name;
	}

	# Fortran has a 132-character line-length limit by default (grr)
	$len = 0;

	print "    "; $len = $len + length("    ");
	if ($ret eq "void") {
	    $kind = "subroutine"
	}
	else {
	    print "$return_types{$ret} ";
	    $len = $len + length("$return_types{$ret} ");
	    $kind = "function"
	}
	print "$kind $name("; $len = $len + length("$kind $name(");
	$len0 = $len;
	
	$argnames = $args;
	$argnames =~ s/([a-zA-Z_0-9 ]+[ \*]) *([a-zA-Z_0-9]+) */$2/g;
	$comma = "";
	foreach $argname (split(/ *, */, $argnames)) {
	    if ($len + length("$comma$argname") + 3 > 132) {
		printf ", &\n%*s", $len0, "";
		$len = $len0;
		$comma = "";
	    }
	    print "$comma$argname";
	    $len = $len + length("$comma$argname");
	    $comma = ",";
	}
	print ") "; $len = $len + 2;

	if ($len + length("bind(C, name='$cname')") > 132) {
	    printf "&\n%*s", $len0 - length("$name("), "";
	}
	print "bind(C, name='$cname')\n";

	print "      import\n";
	foreach $arg (split(/ *, */, $args)) {
	    $arg =~ /^([a-zA-Z_0-9 ]+[ \*]) *([a-zA-Z_0-9]+) *$/;
	    $argtype = &canonicalize_type($1);
	    $argname = $2;
	    $ftype = $arg_types{$argtype};

	    # Various special cases for argument types:
	    if ($name =~ /_flops$/ && $argtype eq "double *") {
		$ftype = "real(C_DOUBLE), intent(out)" 
	    }
	    if ($name =~ /_execute/ && ($argname eq "ri" ||
					$argname eq "ii" || 
					$argname eq "in")) {
		$ftype =~ s/intent\(out\)/intent(inout)/;
	    }

	    print "      $ftype :: $argname\n"
	}

	print "    end $kind $name\n";
	print "    \n";
    }
}
