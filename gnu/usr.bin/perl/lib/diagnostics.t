#!./perl

BEGIN {
    chdir '..' if -d '../pod' && -d '../t';
    @INC = 'lib';
}

use Test::More tests => 3;

BEGIN { use_ok('diagnostics') }

require base;

eval {
    'base'->import(qw(I::do::not::exist));
};

like( $@, qr/^Base class package "I::do::not::exist" is empty/);

# Test for %.0f patterns in perldiag, added in 5.11.0
close STDERR;
open STDERR, ">", \my $warning
    or die "Couldn't redirect STDERR to var: $!";
warn('gmtime(nan) too large');
like $warning, qr/\(W overflow\) You called/, '%0.f patterns';
