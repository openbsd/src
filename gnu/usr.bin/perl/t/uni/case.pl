use File::Spec;

require "test.pl";

sub unidump {
    join " ", map { sprintf "%04X", $_ } unpack "U*", $_[0];
}

sub casetest {
    my ($base, $spec, $func) = @_;
    my $file = File::Spec->catfile(File::Spec->catdir(File::Spec->updir,
						      "lib", "unicore", "To"),
				   "$base.pl");
    my $simple = do $file;
    my %simple;
    for my $i (split(/\n/, $simple)) {
	my ($k, $v) = split(' ', $i);
	$simple{$k} = $v;
    }
    my %seen;

    for my $i (sort keys %simple) {
	$seen{$i}++;
    }
    print "# ", scalar keys %simple, " simple mappings\n";

    my $both;

    for my $i (sort keys %$spec) {
	if (++$seen{$i} == 2) {
	    warn sprintf "$base: $i seen twice\n";
	    $both++;
	}
    }
    print "# ", scalar keys %$spec, " special mappings\n";

    exit(1) if $both;

    my %none;
    for my $i (map { ord } split //,
	       "\e !\"#\$%&'()+,-./0123456789:;<=>?\@[\\]^_{|}~\b") {
	next if pack("U0U", $i) =~ /\w/;
	$none{$i}++ unless $seen{$i};
    }
    print "# ", scalar keys %none, " noncase mappings\n";

    my $tests = 
	(scalar keys %simple) +
	(scalar keys %$spec) +
	(scalar keys %none);
    print "1..$tests\n";

    my $test = 1;

    for my $i (sort keys %simple) {
	my $w = $simple{$i};
	my $c = pack "U0U", hex $i;
	my $d = $func->($c);
	my $e = unidump($d);
	print $d eq pack("U0U", hex $simple{$i}) ?
	    "ok $test # $i -> $w\n" : "not ok $test # $i -> $e ($w)\n";
	$test++;
    }

    for my $i (sort keys %$spec) {
	my $w = unidump($spec->{$i});
	my $u = unpack "U0U", $i;
	my $h = sprintf "%04X", $u;
	my $c = chr($u); $c .= chr(0x100); chop $c;
	my $d = $func->($c);
	my $e = unidump($d);
	if (ord "A" == 193) { # EBCDIC
	    # We need to a little bit of remapping.
	    #
	    # For example, in titlecase (ucfirst) mapping
	    # of U+0149 the Unicode mapping is U+02BC U+004E.
	    # The 4E is N, which in EBCDIC is 2B--
	    # and the ucfirst() does that right.
	    # The problem is that our reference
	    # data is in Unicode code points.
	    #
	    # The Right Way here would be to use, say,
	    # Encode, to remap the less-than 0x100 code points,
	    # but let's try to be Encode-independent here. 
	    #
	    # These are the titlecase exceptions:
	    #
	    #         Unicode   Unicode+EBCDIC  
	    #
	    # 0149 -> 02BC 004E (02BC 002B)
	    # 01F0 -> 004A 030C (00A2 030C)
	    # 1E96 -> 0048 0331 (00E7 0331)
	    # 1E97 -> 0054 0308 (00E8 0308)
	    # 1E98 -> 0057 030A (00EF 030A)
	    # 1E99 -> 0059 030A (00DF 030A)
	    # 1E9A -> 0041 02BE (00A0 02BE)
	    #
	    # The uppercase exceptions are identical.
	    #
	    # The lowercase has one more:
	    #
	    #         Unicode   Unicode+EBCDIC  
	    #
	    # 0130 -> 0069 0307 (00D1 0307)
	    #
	    if ($i =~ /^(0130|0149|01F0|1E96|1E97|1E98|1E99|1E9A)$/) {
		$e =~ s/004E/002B/; # N
		$e =~ s/004A/00A2/; # J
		$e =~ s/0048/00E7/; # H
		$e =~ s/0054/00E8/; # T
		$e =~ s/0057/00EF/; # W
		$e =~ s/0059/00DF/; # Y
		$e =~ s/0041/00A0/; # A
		$e =~ s/0069/00D1/; # i
	    }
	    # We have to map the output, not the input, because
	    # pack/unpack U has been EBCDICified, too, it would
	    # just undo our remapping.
	}
	print $w eq $e ?
	    "ok $test # $i -> $w\n" : "not ok $test # $h -> $e ($w)\n";
	$test++;
    }

    for my $i (sort { $a <=> $b } keys %none) {
	my $w = $i = sprintf "%04X", $i;
	my $c = pack "U0U", hex $i;
	my $d = $func->($c);
	my $e = unidump($d);
	print $d eq $c ?
	    "ok $test # $i -> $w\n" : "not ok $test # $i -> $e ($w)\n";
	$test++;
    }
}

1;
