#!/usr/bin/perl -w

BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir 't' if -d 't';
        unshift @INC, '../lib';
    }
}

use strict;
use lib "BUNDLE";
use Test::More tests => 6; 

use_ok( 'CPAN::FirstTime' );
can_ok( 'CPAN::Mirrored::By', 'new', 'continent', 'country', 'url' );
my $cmb = CPAN::Mirrored::By->new();
isa_ok( $cmb, 'CPAN::Mirrored::By' );

@$cmb = qw( continent country url );
is( $cmb->continent(), 'continent',
    'continent() should return continent entry' );
is( $cmb->country(), 'country', 'country() should return country entry' );
is( $cmb->url(), 'url', 'url() should return url entry' );

__END__
# Local Variables:
# mode: cperl
# cperl-indent-level: 4
# End:
