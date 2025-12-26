#!./perl
#
#  Copyright (c) 2002 Slaven Rezic
#
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

use strict;
use warnings;
use Test::More tests => 1;

my @warns;
$SIG{__WARN__} = sub { push @warns, shift };
$SIG{__DIE__}  = sub { require Carp; warn Carp::longmess(); warn "Evil die!" };

require Storable;

Storable::dclone({foo => "bar"});

is(join("", @warns), "", "__DIE__ is not evil here");
