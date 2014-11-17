#!/pro/bin/perl

use strict;
use warnings;

BEGIN {
    use Test::More;
    my $tests = 9;
    unless ($ENV{PERL_CORE}) {
	require Test::NoWarnings;
	Test::NoWarnings->import ();
	$tests++;
	}

    plan tests => $tests;

    use_ok ("Config::Perl::V");
    }

ok (my $conf = Config::Perl::V::myconfig,	"Read config");
ok (exists $conf->{$_},	"Has $_ entry") for qw( build environment config inc );
is (lc $conf->{build}{osname}, lc $conf->{config}{osname}, "osname");

SKIP: {
    # Test that the code that shells out to perl -V and parses the output
    # gives the same results as the code that calls Config::* routines directly.
    defined &Config::compile_date or
	skip "This perl doesn't provide perl -V in the Config module", 2;
    eval q{no warnings "redefine"; sub Config::compile_date { return undef }};
    is (Config::compile_date (), undef, "Successfully overriden compile_date");
    is_deeply (Config::Perl::V::myconfig, $conf,
	"perl -V parsing code produces same result as the Config module");
    }
