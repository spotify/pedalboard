#! /usr/bin/perl -w

$program = "./bench";
$default_options = "";
$verbose = 0;
$paranoid = 0;
$exhaustive = 0;
$patient = 0;
$estimate = 0;
$wisdom = 0;
$validate_wisdom = 0;
$threads_callback = 0;
$nthreads = 1;
$rounds = 0;
$maxsize = 60000;
$maxcount = 100;
$do_0d = 0;
$do_1d = 0;
$do_2d = 0;
$do_random = 0;
$keepgoing = 0;
$flushcount = 42;

$mpi = 0;
$mpi_transposed_in = 0;
$mpi_transposed_out = 0;

sub make_options {
    my $options = $default_options;
    $options = "--verify-rounds=$rounds $options" if $rounds;
    $options = "--verbose=$verbose $options" if $verbose;
    $options = "-o paranoid $options" if $paranoid;
    $options = "-o exhaustive $options" if $exhaustive;
    $options = "-o patient $options" if $patient;
    $options = "-o estimate $options" if $estimate;
    $options = "-o wisdom $options" if $wisdom;
    $options = "-o threads_callback $options" if $threads_callback;
    $options = "-o nthreads=$nthreads $options" if ($nthreads > 1);
    $options = "-obflag=30 $options" if $mpi_transposed_in;
    $options = "-obflag=31 $options" if $mpi_transposed_out;
    return $options;
}

@list_of_problems = ();

sub run_bench {
    my $options = shift;
    my $problist = shift;

    print "Executing \"$program $options $problist\"\n"
        if $verbose;

    system("$program $options $problist");
    $exit_value  = $? >> 8;
    $signal_num  = $? & 127;
    $dumped_core = $? & 128;

    if ($signal_num == 1) {
        print "hangup\n";
        exit 0;
    }
    if ($signal_num == 2) {
        print "interrupted\n";
        exit 0;
    }
    if ($signal_num == 9) {
        print "killed\n";
        exit 0;
    }

    if ($exit_value != 0 || $dumped_core || $signal_num) {
        print "FAILED $program: $problist\n";
        if ($signal_num) { print "received signal $signal_num\n"; }
        exit 1 unless $keepgoing;
    }
}

sub flush_problems {
    my $options = shift;
    my $problist = "";

    if ($#list_of_problems >= 0) {
	for (@list_of_problems) {
	    $problist = "$problist --verify '$_'";
	}

        if ($validate_wisdom) {
            # start with a fresh wisdom state
            unlink("wis.dat");
        }

        run_bench($options, $problist);

        if ($validate_wisdom) {
            # run again and validate that we can the problem in wisdom-only mode
            print "Executing again in wisdom-only mode\n"
                if $verbose;
            run_bench("$options -owisdom-only", $problist);
        }
	@list_of_problems = ();
    }
}

sub do_problem {
    my $problem = shift;
    my $doablep = shift;
    my $options = &make_options;

    if ($problem =~ /\// && $problem =~ /r/
	&& ($problem =~ /i.*x/
	    || $problem =~ /v/ || $problem =~ /\*/)) {
	return; # cannot do real split inplace-multidimensional or vector
    }

    # in --mpi mode, restrict to problems supported by MPI code
    if ($mpi) {
	if ($problem =~ /\//) { return; } # no split
	if ($problem =~ /\*/) { return; } # no non-contiguous vectors
	if ($problem =~ /r/ && $problem !~ /x/) { return; } # no 1d r2c
	if ($problem =~ /k/ && $problem !~ /x/) { return; } # no 1d r2r
	if ($mpi_transposed_in || $problem =~ /\[/) {
	    if ($problem !~ /x/) { return; } # no 1d transposed_in
	    if ($problem =~ /r/ && $problem !~ /b/) { return; } # only c2r
	}
	if ($mpi_transposed_out || $problem =~ /\]/) {
	    if ($problem !~ /x/) { return; } # no 1d transposed_out
	    if ($problem =~ /r/ && $problem =~ /b/) { return; } # only r2c
	}
    }

    # size-1 redft00 is not defined/doable
    return if ($problem =~ /[^0-9]1e00/);

    if ($doablep) {
	@list_of_problems = ($problem, @list_of_problems);
	&flush_problems($options) if ($#list_of_problems > $flushcount);
    } else {
	print "Executing \"$program $options --can-do $problem\"\n"
	    if $verbose;
	$result=`$program $options --can-do $problem`;
	if ($result ne "#f\n" && $result ne "#f\r\n") {
	    print "FAILED $program: $problem is not undoable\n";
	    exit 1 unless $keepgoing;
	}
    }
}

# given geometry, try both directions and in place/out of place
sub do_geometry {
    my $geom = shift;
    my $doablep = shift;
    do_problem("if$geom", $doablep);
    do_problem("of$geom", $doablep);
    do_problem("ib$geom", $doablep);
    do_problem("ob$geom", $doablep);
    do_problem("//if$geom", $doablep);
    do_problem("//of$geom", $doablep);
    do_problem("//ib$geom", $doablep);
    do_problem("//ob$geom", $doablep);
}

# given size, try all transform kinds (complex, real, etc.)
sub do_size {
    my $size = shift;
    my $doablep = shift;
    do_geometry("c$size", $doablep);
    do_geometry("r$size", $doablep);
}

sub small_0d {
    for ($i = 0; $i <= 16; ++$i) {
	for ($j = 0; $j <= 16; ++$j) {
	    for ($vl = 1; $vl <= 5; ++$vl) {
		my $ivl = $i * $vl;
		my $jvl = $j * $vl;
		do_problem("o1v${i}:${vl}:${jvl}x${j}:${ivl}:${vl}x${vl}:1:1", 1);
		do_problem("i1v${i}:${vl}:${jvl}x${j}:${ivl}:${vl}x${vl}:1:1", 1);
		do_problem("ok1v${i}:${vl}:${jvl}x${j}:${ivl}:${vl}x${vl}:1:1", 1);
		do_problem("ik1v${i}:${vl}:${jvl}x${j}:${ivl}:${vl}x${vl}:1:1", 1);
	    }
	}
    }
}

sub small_1d {
    do_size (0, 0);
    for ($i = 1; $i <= 100; ++$i) {
	do_size ($i, 1);
    }
    do_size (128, 1);
    do_size (256, 1);
    do_size (512, 1);
    do_size (1024, 1);
    do_size (2048, 1);
    do_size (4096, 1);
}

sub small_2d {
    do_size ("0x0", 0);
    for ($i = 1; $i <= 100; ++$i) {
	my $ub = 900/$i;
	$ub = 100 if $ub > 100;
	for ($j = 1; $j <= $ub; ++$j) {
	    do_size ("${i}x${j}", 1);
	}
    }
}

sub rand_small_factors {
    my $l = shift;
    my $n = 1;
    my $maxfactor = 13;
    my $f = int(rand($maxfactor) + 1);
    while ($n * $f < $l) {
	$n *= $f;
	$f = int(rand($maxfactor) + 1);
    };
    return $n;
}

# way too complicated...
sub one_random_test {
    my $q = int(2 + rand($maxsize));
    my $rnk = int(1 + rand(4));
    my $vtype = int(rand(3));
    my $g = int(2 + exp(log($q) / ($rnk + ($vtype > 0))));
    my $first = 1;
    my $sz = "";
    my $is_r2r = shift;
    my @r2r_kinds = ("f", "b", "h",
		     "e00", "e01", "e10", "e11", "o00", "o01", "o10", "o11");

    while ($q > 1 && $rnk > 0) {
	my $r = rand_small_factors(int(rand($g) + 10));
	if ($r > 1) {
	    $sz = "${sz}x" if (!$first);
	    $first = 0;
	    $sz = "${sz}${r}";
	    if ($is_r2r) {
		my $k = $r2r_kinds[int(1 + rand($#r2r_kinds))];
		$sz = "${sz}${k}";
	    }
	    $q = int($q / $r);
	    if ($g > $q) { $g = $q; }
	    --$rnk;
	}
    }
    if ($vtype > 0 && $g > 1) {
	my $v = int(1 + rand($g));
	$sz = "${sz}*${v}" if ($vtype == 1);
	$sz = "${sz}v${v}" if ($vtype == 2);
    }
    if ($mpi) {
	my $stype = int(rand(3));
	$sz = "]${sz}" if ($stype == 1);
	$sz = "[${sz}" if ($stype == 2);
    }
    $sz = "d$sz" if (int(rand(3)) == 0);
    if ($is_r2r) {
	do_problem("ik$sz", 1);
	do_problem("ok$sz", 1);
    }
    else {
	do_size($sz, 1);
    }
}

sub random_tests {
    my $i;
    for ($i = 0; $i < $maxcount; ++$i) {
	&one_random_test(0);
	&one_random_test(1);
    }
}

sub parse_arguments (@)
{
    local (@arglist) = @_;

    while (@arglist)
    {
	if ($arglist[0] eq '-v') { ++$verbose; }
	elsif ($arglist[0] eq '--verbose') { ++$verbose; }
	elsif ($arglist[0] eq '-p') { ++$paranoid; }
	elsif ($arglist[0] eq '--paranoid') { ++$paranoid; }
	elsif ($arglist[0] eq '--exhaustive') { ++$exhaustive; }
	elsif ($arglist[0] eq '--patient') { ++$patient; }
	elsif ($arglist[0] eq '--estimate') { ++$estimate; }
	elsif ($arglist[0] eq '--wisdom') { ++$wisdom; }
        elsif ($arglist[0] eq '--validate-wisdom') { ++$wisdom;  ++$validate_wisdom; }
	elsif ($arglist[0] eq '--threads_callback') { ++$threads_callback; }
	elsif ($arglist[0] =~ /^--nthreads=(.+)$/) { $nthreads = $1; }
	elsif ($arglist[0] eq '-k') { ++$keepgoing; }
	elsif ($arglist[0] eq '--keep-going') { ++$keepgoing; }
	elsif ($arglist[0] =~ /^--verify-rounds=(.+)$/) { $rounds = $1; }
	elsif ($arglist[0] =~ /^--count=(.+)$/) { $maxcount = $1; }
	elsif ($arglist[0] =~ /^-c=(.+)$/) { $maxcount = $1; }
	elsif ($arglist[0] =~ /^--flushcount=(.+)$/) { $flushcount = $1; }
	elsif ($arglist[0] =~ /^--maxsize=(.+)$/) { $maxsize = $1; }

	elsif ($arglist[0] eq '--mpi') { ++$mpi; }
	elsif ($arglist[0] eq '--mpi-transposed-in') {
	    ++$mpi; ++$mpi_transposed_in; }
	elsif ($arglist[0] eq '--mpi-transposed-out') {
	    ++$mpi; ++$mpi_transposed_out; }

	elsif ($arglist[0] eq '-0d') { ++$do_0d; }
	elsif ($arglist[0] eq '-1d') { ++$do_1d; }
	elsif ($arglist[0] eq '-2d') { ++$do_2d; }
	elsif ($arglist[0] eq '-r') { ++$do_random; }
	elsif ($arglist[0] eq '--random') { ++$do_random; }
	elsif ($arglist[0] eq '-a') {
	    ++$do_0d; ++$do_1d; ++$do_2d; ++$do_random;
	}

	else { $program=$arglist[0]; }
	shift (@arglist);
    }
}

# MAIN PROGRAM:

&parse_arguments (@ARGV);

&random_tests if $do_random;
&small_0d if $do_0d;
&small_1d if $do_1d;
&small_2d if $do_2d;

{
    my $options = &make_options;
    &flush_problems($options);
}
