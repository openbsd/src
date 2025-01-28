#!perl

BEGIN {
    require Config;
    import Config;
    if ($Config{'extensions'} !~ /\bOpcode\b/) {
        print "1..0\n";
        exit 0;
    }
}

use strict;
use warnings;
use Test::More;
use Safe;
plan(tests => 2);

$SIG{$_} = $_ for keys %SIG;
my %saved_SIG = %SIG;

my $compartment = Safe->new;
my $rv = $compartment->reval(q| return 1+1 |);

my $success = 0;
for (keys %saved_SIG) {
    $success++ if $SIG{$_} and $SIG{$_} eq $saved_SIG{$_};
}

# This tests for: https://rt.cpan.org/Ticket/Display.html?id=112092
is( $success, scalar(keys %saved_SIG),
    '%SIG content restored after compartmentized evaluation' );

my ($signame, ) = keys %SIG; # just pick any signal name
is( defined $compartment->reval(qq| return \$SIG{$signame} |),
    defined undef,
    '%SIG values undefined inside compartment' );
