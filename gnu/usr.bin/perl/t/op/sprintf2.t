#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}   

plan tests => 7 + 256;

is(
    sprintf("%.40g ",0.01),
    sprintf("%.40g", 0.01)." ",
    q(the sprintf "%.<number>g" optimization)
);
is(
    sprintf("%.40f ",0.01),
    sprintf("%.40f", 0.01)." ",
    q(the sprintf "%.<number>f" optimization)
);
{
	chop(my $utf8_format = "%-3s\x{100}");
	is(
		sprintf($utf8_format, "\xe4"),
		"\xe4  ",
		q(width calculation under utf8 upgrade)
	);
}

# Used to mangle PL_sv_undef
fresh_perl_is(
    'print sprintf "xxx%n\n"; print undef',
    'Modification of a read-only value attempted at - line 1.',
    { switches => [ '-w' ] },
    q(%n should not be able to modify read-only constants),
);

# check %NNN$ for range bounds, especially negative 2's complement

{
    my ($warn, $bad) = (0,0);
    local $SIG{__WARN__} = sub {
	if ($_[0] =~ /uninitialized/) {
	    $warn++
	}
	else {
	    $bad++
	}
    };
    my $result = sprintf join('', map("%$_\$s%" . ~$_ . '$s', 1..20)),
	qw(a b c d);
    is($result, "abcd", "only four valid values");
    is($warn, 36, "expected warnings");
    is($bad,   0, "unexpected warnings");
}

{
    foreach my $ord (0 .. 255) {
	my $bad = 0;
	local $SIG{__WARN__} = sub {
	    unless ($_[0] =~ /^Invalid conversion in sprintf/ ||
		    $_[0] =~ /^Use of uninitialized value in sprintf/) {
		warn $_[0];
		$bad++;
	    }
	};
	my $r = eval {sprintf '%v' . chr $ord};
	is ($bad, 0, "pattern '%v' . chr $ord");
    }
}
