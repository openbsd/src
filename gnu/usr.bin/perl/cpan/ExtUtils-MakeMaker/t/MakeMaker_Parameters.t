#!/usr/bin/perl -w

# Things like the CPAN shell rely on the "MakeMaker Parameters" section of the
# Makefile to learn a module's dependencies so we'd damn well better test it.

BEGIN {
    unshift @INC, 't/lib';
}

use strict;
use warnings;

use ExtUtils::MakeMaker;
use Test::More;

my $mm = bless {}, "MM";

sub extract_params {
    my $text = join "\n", @_;

    $text =~ s{^\s* \# \s+ MakeMaker\ Parameters: \s*\n}{}x;
    $text =~ s{^#}{}gms;
    $text =~ s{\n}{,\n}g;

    no strict 'subs';
    return { eval "$text" };
}

sub test_round_trip {
    my $args = shift;
    my $want = @_ ? shift : $args;

    my $have = extract_params($mm->_MakeMaker_Parameters_section($args));

    local $Test::Builder::Level = $Test::Builder::Level + 1;
    is_deeply $have, $want or diag explain $have, "\n", $want;
}

is join("", $mm->_MakeMaker_Parameters_section()), <<'EXPECT', "nothing";
#   MakeMaker Parameters:
EXPECT

test_round_trip({ NAME => "Foo" });
test_round_trip({ NAME => "Foo", PREREQ_PM => { "Foo::Bar" => 0 } });
test_round_trip({ NAME => "Foo", PREREQ_PM => { "Foo::Bar" => 1.23 } });

# Test the special case for BUILD_REQUIRES
{
    my $have = {
        NAME                => "Foo",
        PREREQ_PM           => { "Foo::Bar" => 1.23 },
        BUILD_REQUIRES      => { "Baz"      => 0.12 },
    };

    my $want = {
        NAME                => "Foo",
        PREREQ_PM           => {
            "Foo::Bar"  => 1.23,
            "Baz"       => 0.12,
        },
        BUILD_REQUIRES      => { "Baz"      => 0.12 },
    };

    test_round_trip( $have, $want );
}

done_testing();

