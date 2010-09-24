#!/usr/bin/env perl

use strict;
use warnings;

use lib '../lib';
use Test::Builder;

=head1 NOTES

Must have explicit finalize
Must name nest
Trailing summary test
Pass chunk o'TAP
No builder may have more than one child active
What happens if you call ->finalize with open children

=cut

my $builder = Test::Builder->new;
$builder->plan(tests => 7);
for( 1 .. 3 ) {
    $builder->ok( $_, "We're on $_" );
    $builder->note("We ran $_");
}
{
    my $indented = $builder->child;
    $indented->plan('no_plan');
    for( 1 .. 1+int(rand(5))  ) {
        $indented->ok( 1, "We're on $_" );
    }
    $indented->finalize;
}
for( 7, 8, 9 ) {
    $builder->ok( $_, "We're on $_" );
}
