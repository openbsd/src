#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $INC{"feature.pm"} = 1; # so we don't attempt to load feature.pm
}

print "1..63\n";

# Can't require test.pl, as we're testing the use/require mechanism here.

my $test = 1;

sub _ok {
    my ($type, $got, $expected, $name) = @_;

    my $result;
    if ($type eq 'is') {
	$result = $got eq $expected;
    } elsif ($type eq 'isnt') {
	$result = $got ne $expected;
    } elsif ($type eq 'like') {
	$result = $got =~ $expected;
    } else {
	die "Unexpected type '$type'$name";
    }
    if ($result) {
	if ($name) {
	    print "ok $test - $name\n";
	} else {
	    print "ok $test\n";
	}
    } else {
	if ($name) {
	    print "not ok $test - $name\n";
	} else {
	    print "not ok $test\n";
	}
	my @caller = caller(2);
	print "# Failed test at $caller[1] line $caller[2]\n";
	print "# Got      '$got'\n";
	if ($type eq 'is') {
	    print "# Expected '$expected'\n";
	} elsif ($type eq 'isnt') {
	    print "# Expected not '$expected'\n";
	} elsif ($type eq 'like') {
	    print "# Expected $expected\n";
	}
    }
    $test = $test + 1;
    $result;
}

sub like ($$;$) {
    _ok ('like', @_);
}
sub is ($$;$) {
    _ok ('is', @_);
}
sub isnt ($$;$) {
    _ok ('isnt', @_);
}

eval "use 5.000";	# implicit semicolon
is ($@, '');

eval "use 5.000;";
is ($@, '');

eval "use 6.000;";
like ($@, qr/Perl v6\.0\.0 required--this is only \Q$^V\E, stopped/);

eval "no 6.000;";
is ($@, '');

eval "no 5.000;";
like ($@, qr/Perls since v5\.0\.0 too modern--this is \Q$^V\E, stopped/);

eval "use 5.6;";
like ($@, qr/Perl v5\.600\.0 required \(did you mean v5\.6\.0\?\)--this is only \Q$^V\E, stopped/);

eval "use 5.8;";
like ($@, qr/Perl v5\.800\.0 required \(did you mean v5\.8\.0\?\)--this is only \Q$^V\E, stopped/);

eval "use 5.9;";
like ($@, qr/Perl v5\.900\.0 required \(did you mean v5\.9\.0\?\)--this is only \Q$^V\E, stopped/);

eval "use 5.10;";
like ($@, qr/Perl v5\.100\.0 required \(did you mean v5\.10\.0\?\)--this is only \Q$^V\E, stopped/);

eval sprintf "use %.6f;", $];
is ($@, '');


eval sprintf "use %.6f;", $] - 0.000001;
is ($@, '');

eval sprintf("use %.6f;", $] + 1);
like ($@, qr/Perl v6.\d+.\d+ required--this is only \Q$^V\E, stopped/);

eval sprintf "use %.6f;", $] + 0.00001;
like ($@, qr/Perl v5.\d+.\d+ required--this is only \Q$^V\E, stopped/);

{ use lib }	# check that subparse saves pending tokens

local $lib::VERSION = 1.0;

eval "use lib 0.9";
is ($@, '');

eval "use lib 1.0";
is ($@, '');

eval "use lib 1.01";
isnt ($@, '');


eval "use lib 0.9 qw(fred)";
is ($@, '');

if ($^O eq 'MacOS') {
    is($INC[0], ":fred:");
} else {
    is($INC[0], "fred");
}

eval "use lib 1.0 qw(joe)";
is ($@, '');


if ($^O eq 'MacOS') {
    is($INC[0], ":joe:");
} else {
    is($INC[0], "joe");
}


eval "use lib 1.01 qw(freda)";
isnt($@, '');

if ($^O eq 'MacOS') {
    isnt($INC[0], ":freda:");
} else {
    isnt($INC[0], "freda");
}

{
    local $lib::VERSION = 35.36;
    eval "use lib v33.55";
    is ($@, '');

    eval "use lib v100.105";
    like ($@, qr/lib version v100.105.0 required--this is only version v35\.360\.0/);

    eval "use lib 33.55";
    is ($@, '');

    eval "use lib 100.105";
    like ($@, qr/lib version 100.105 required--this is only version 35.36/);

    local $lib::VERSION = '35.36';
    eval "use lib v33.55";
    like ($@, '');

    eval "use lib v100.105";
    like ($@, qr/lib version v100.105.0 required--this is only version v35\.360\.0/);

    eval "use lib 33.55";
    is ($@, '');

    eval "use lib 100.105";
    like ($@, qr/lib version 100.105 required--this is only version 35.36/);

    local $lib::VERSION = v35.36;
    eval "use lib v33.55";
    is ($@, '');

    eval "use lib v100.105";
    like ($@, qr/lib version v100.105.0 required--this is only version v35\.36\.0/);

    eval "use lib 33.55";
    is ($@, '');

    eval "use lib 100.105";
    like ($@, qr/lib version 100.105 required--this is only version v35.36/);
}


{
    # Regression test for patch 14937: 
    #   Check that a .pm file with no package or VERSION doesn't core.
    open F, ">xxx$$.pm" or die "Cannot open xxx$$.pm: $!\n";
    print F "1;\n";
    close F;
    eval "use lib '.'; use xxx$$ 3;";
    like ($@, qr/^xxx$$ defines neither package nor VERSION--version check failed at/);
    unlink "xxx$$.pm";
}

my @ver = split /\./, sprintf "%vd", $^V;

foreach my $index (-3..+3) {
    foreach my $v (0, 1) {
	my @parts = @ver;
	if ($index) {
	    if ($index < 0) {
		# Jiggle one of the parts down
		--$parts[-$index - 1];
		if ($parts[-$index - 1] < 0) {
		    # perl's version number ends with '.0'
		    $parts[-$index - 1] = 0;
		    $parts[-$index - 2] -= 2;
		}
	    } else {
		# Jiggle one of the parts up
		++$parts[$index - 1];
	    }
	}
	my $v_version = sprintf "v%d.%d.%d", @parts;
	my $version;
	if ($v) {
	    $version = $v_version;
	} else {
	    $version = $parts[0] + $parts[1] / 1000 + $parts[2] / 1000000;
	}

	eval "use $version";
	if ($index > 0) {
	    # The future
	    like ($@,
		  qr/Perl $v_version required--this is only \Q$^V\E, stopped/,
		  "use $version");
	} else {
	    # The present or past
	    is ($@, '', "use $version");
	}

	eval "no $version";
	if ($index <= 0) {
	    # The present or past
	    like ($@,
		  qr/Perls since $v_version too modern--this is \Q$^V\E, stopped/,
		  "no $version");
	} else {
	    # future
	    is ($@, '', "no $version");
	}
    }
}

