#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Test::More;

BEGIN {
    our $hasgr;
    eval { my @n = getgrgid 0 };
    $hasgr = 1 unless $@ && $@ =~ /unimplemented/;
    unless ($hasgr) { plan skip_all => "no getgrgid"; }
    use Config;
    $hasgr = 0 unless $Config{'i_grp'} eq 'define';
    unless ($hasgr) { plan skip_all => "no grp.h"; }
}

BEGIN {
    our @grent = getgrgid 0; # This is the function getgrgid.
    unless (@grent) { plan skip_all => "no gid 0"; }
}

BEGIN {
    plan tests => 5;
    use_ok('User::grent');
}

can_ok(__PACKAGE__, 'getgrgid');

my $grent = getgrgid 0;

is( $grent->name, $grent[0],    'name matches core getgrgid' );

is( $grent->passwd, $grent[1],  '   passwd' );

is( $grent->gid, $grent[2],     '   gid' );


# Testing pretty much anything else is unportable.

