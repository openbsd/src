#!./perl

use strict;
use warnings;

use Scalar::Util ();
use List::Util ();
use List::Util::XS ();
use Test::More tests => 2;

is( $Scalar::Util::VERSION, $List::Util::VERSION, "VERSION mismatch");
my $has_xs = eval { Scalar::Util->import('dualvar'); 1 };
my $xs_version = $has_xs ? $List::Util::VERSION : undef;
is( $List::Util::XS::VERSION, $xs_version, "XS VERSION");

