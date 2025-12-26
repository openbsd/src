#!./perl
#
#  Copyright (c) 1995-2000, Raphael Manfredi
#
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#
# Original Author: Ulrich Pfeifer
# (C) Copyright 1997, Universitat Dortmund, all rights reserved.
#

use strict;
use warnings;

use Storable qw(store retrieve);
use Test::More;

use File::Spec;

plan(tests => 8);

*GLOB = *GLOB; # peacify -w
my $bad = ['foo', \*GLOB,  'bar'];
my $result;

eval {$result = store ($bad , "store$$")};
is($result, undef);
isnt($@, '');

$Storable::forgive_me=1;

my $devnull = File::Spec->devnull;

open(SAVEERR, ">&STDERR");
open(STDERR, '>', $devnull) or
    ( print SAVEERR "Unable to redirect STDERR: $!\n" and exit(1) );

eval {$result = store ($bad , "store$$")};

open(STDERR, ">&SAVEERR");

isnt($result, undef);
is($@, '');

my $ret = retrieve("store$$");
isnt($ret, undef);
is($ret->[0], 'foo');
is($ret->[2], 'bar');
is(ref $ret->[1], 'SCALAR');


END { 1 while unlink "store$$" }
