#!perl
use strict;
use warnings;

use Config;
use Test::More
    $Config{'extensions'} =~ /\bOpcode\b/
        ? (tests => 2)
        : (skip_all => "no Opcode extension");
use Safe;

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
