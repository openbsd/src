#!perl -w

use Config;
use Test::More
    $Config{'extensions'} =~ /\bOpcode\b/
        ? (tests => 3)
        : (skip_all => "no Opcode extension");
use Safe;

my $c; my $r;
my $snippet = q{
    my $foo = qr/foo/;
    ref $foo;
};
$c = Safe->new;
$r = $c->reval($snippet);
is( $r, "Safe::Root0::Regexp" );
$r or diag $@;

# once more with the same compartment
# (where DESTROY has been cleaned up)
$r = $c->reval($snippet);
is( $r, "Safe::Root0::Regexp" );
$r or diag $@;

# try with a new compartment
$c = Safe->new;
$r = $c->reval($snippet);
is( $r, "Safe::Root1::Regexp" );
$r or diag $@;
