#!perl

use Config;
use Test::More
    $Config{'extensions'} =~ /\bOpcode\b/
        && eval { require version; delete $INC{"version.pm"}; 1 }
    ? (tests => 4)
    : (skip_all => "no Opcode extension or can't load version.pm");

use strict;
use Safe;

my $c = Safe->new;
$c->permit(qw(require caller entereval unpack rand));
my $r = $c->reval(q{ use version; 1 });
ok( defined $r, "Can load version.pm in a Safe compartment" ) or diag $@;

$r = $c->reval(q{ version->new(1.2) });
is(ref $r, "Safe::Root0::version", "version objects rerooted");
$r or diag $@;

# Does this test really belong here?  We are testing the "loading" of
# a perl version number.
# This should died because of strictures under 5.12+ and because of the
# perl version in 5.10-.
ok !$c->reval(q{use 5.012; $undeclared; 1}),
   'reval does not prevent use 5.012 from enabling strict';

# "use Tie::Scalar" depends on UNIVERSAL::import as Tie::Scalar does not have
# its own import method.
$r = $c->reval(q{ use Tie::Scalar; 1 });
ok( defined $r, "Can load Tie::Scalar.pm in a Safe compartment" ) or diag $@;
