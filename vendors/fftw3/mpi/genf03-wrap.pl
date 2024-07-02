#!/usr/bin/perl -w
# Generate Fortran 2003 wrappers (which translate MPI_Comm from f2c) from
# function declarations of the form (one per line):
#     extern <type> fftw_mpi_<name>(...args...)
#     extern <type> fftw_mpi_<name>(...args...)
#     ...
# with no line breaks within a given function.  (It's too much work to
# write a general parser, since we just have to handle FFTW's header files.)
# Each declaration has at least one MPI_Comm argument.

sub canonicalize_type {
    my($type);
    ($type) = @_;
    $type =~ s/ +/ /g;
    $type =~ s/^ //;
    $type =~ s/ $//;
    $type =~ s/([^\* ])\*/$1 \*/g;
    $type =~ s/double/R/;
    $type =~ s/fftw_([A-Za-z0-9_]+)/X(\1)/;
    return $type;
}

while (<>) {
    next if /^ *$/;
    if (/^ *extern +([a-zA-Z_0-9 ]+[ \*]) *fftw_mpi_([a-zA-Z_0-9]+) *\((.*)\) *$/) {
	$ret = &canonicalize_type($1);
	$name = $2;

	$args = $3;

	
	print "\n$ret XM(${name}_f03)(";

	$comma = "";
	foreach $arg (split(/ *, */, $args)) {
            $arg =~ /^([a-zA-Z_0-9 ]+[ \*]) *([a-zA-Z_0-9]+) *$/;
            $argtype = &canonicalize_type($1);
            $argname = $2;
	    print $comma;
	    if ($argtype eq "MPI_Comm") {
		print "MPI_Fint f_$argname";
	    }
	    else {
		print "$argtype $argname";
	    }
	    $comma = ", ";
        }
	print ")\n{\n";

	print "     MPI_Comm ";
	$comma = "";
	foreach $arg (split(/ *, */, $args)) {
            $arg =~ /^([a-zA-Z_0-9 ]+[ \*]) *([a-zA-Z_0-9]+) *$/;
            $argtype = &canonicalize_type($1);
            $argname = $2;
	    if ($argtype eq "MPI_Comm") {
		print "$comma$argname";
		$comma = ", ";
	    }
        }
	print ";\n\n";

	foreach $arg (split(/ *, */, $args)) {
            $arg =~ /^([a-zA-Z_0-9 ]+[ \*]) *([a-zA-Z_0-9]+) *$/;
            $argtype = &canonicalize_type($1);
            $argname = $2;
            if ($argtype eq "MPI_Comm") {
                print "     $argname = MPI_Comm_f2c(f_$argname);\n";
            }
        }

	$argnames = $args;
	$argnames =~ s/([a-zA-Z_0-9 ]+[ \*]) *([a-zA-Z_0-9]+) */$2/g;
	print "     ";
	print "return " if ($ret ne "void");
	print "XM($name)($argnames);\n}\n";
    }
}
