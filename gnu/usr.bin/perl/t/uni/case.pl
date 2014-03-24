require "test.pl";
use strict;
use warnings;

sub unidump {
    join " ", map { sprintf "%04X", $_ } unpack "U*", $_[0];
}

sub casetest {
    my ($already_run, $base, @funcs) = @_;

    my %spec;

    # For each provided function run it, and run a version with some extra
    # characters afterwards. Use a recycling symbol, as it doesn't change case.
    # $already_run is the number of extra tests the caller has run before this
    # call.
    my $ballast = chr (0x2672) x 3;
    @funcs = map {my $f = $_;
		  ($f,
		   sub {my $r = $f->($_[0] . $ballast); # Add it before
			$r =~ s/$ballast\z//so # Remove it afterwards
			    or die "'$_[0]' to '$r' mangled";
			$r; # Result with $ballast removed.
		    },
		   )} @funcs;

    use Unicode::UCD 'prop_invmap';

    # Get the case mappings
    my ($invlist_ref, $invmap_ref, undef, $default) = prop_invmap($base);
    my %simple;

    for my $i (0 .. @$invlist_ref - 1 - 1) {
        next if $invmap_ref->[$i] == $default;

        # Add simple mappings to the simples test list
        if (! ref $invmap_ref->[$i]) {

            # The returned map needs to have adjustments made.  Each
            # subsequent element of the range requires adjustment of +1 from
            # the previous element
            my $adjust = 0;
            for my $k ($invlist_ref->[$i] .. $invlist_ref->[$i+1] - 1) {
                $simple{$k} = $invmap_ref->[$i] + $adjust++;
            }
        }
        else {  # The return is a list of the characters mapped-to.
                # prop_invmap() guarantees a single element in the range in
                # this case, so no adjustments are needed.
            $spec{$invlist_ref->[$i]} = pack "U0U*" , @{$invmap_ref->[$i]};
        }
    }

    my %seen;

    for my $i (sort keys %simple) {
	$seen{$i}++;
    }
    print "# ", scalar keys %simple, " simple mappings\n";

    for my $i (sort keys %spec) {
	if (++$seen{$i} == 2) {
	    warn sprintf "$base: $i seen twice\n";
	}
    }
    print "# ", scalar keys %spec, " special mappings\n";

    my %none;
    for my $i (map { ord } split //,
	       "\e !\"#\$%&'()+,-./0123456789:;<=>?\@[\\]^_{|}~\b") {
	next if pack("U0U", $i) =~ /\w/;
	$none{$i}++ unless $seen{$i};
    }
    print "# ", scalar keys %none, " noncase mappings\n";

    my $tests = 
        $already_run +
	((scalar keys %simple) +
	 (scalar keys %spec) +
	 (scalar keys %none)) * @funcs;

    my $test = $already_run + 1;

    for my $i (sort keys %simple) {
	my $w = $simple{$i};
	my $c = pack "U0U", $i;
	foreach my $func (@funcs) {
	    my $d = $func->($c);
	    my $e = unidump($d);
	    is( $d, pack("U0U", $simple{$i}), "$i -> $e ($w)" );
	}
    }

    for my $i (sort keys %spec) {
	my $w = unidump($spec{$i});
	my $h = sprintf "%04X", $i;
	my $c = chr($i); $c .= chr(0x100); chop $c;
	foreach my $func (@funcs) {
	    my $d = $func->($c);
	    my $e = unidump($d);
            is( $w, $e, "$h -> $e ($w)" );
	}
    }

    for my $i (sort { $a <=> $b } keys %none) {
	my $c = pack "U0U", $i;
	my $w = $i = sprintf "%04X", $i;
	foreach my $func (@funcs) {
	    my $d = $func->($c);
	    my $e = unidump($d);
            is( $d, $c, "$i -> $e ($w)" );
	}
    }

    done_testing();
}

1;
