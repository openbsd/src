#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}
use strict;

require './test.pl';

plan(2);

like(runperl(switches => ['-Irun/flib', '-Mbroken'], stderr => 1),
     qr/^Global symbol "\$x" requires explicit package name at run\/flib\/broken.pm line 6\./,
     "Ensure -Irun/flib produces correct filename in warnings");

like(runperl(switches => ['-Irun/flib/', '-Mbroken'], stderr => 1),
     qr/^Global symbol "\$x" requires explicit package name at run\/flib\/broken.pm line 6\./,
     "Ensure -Irun/flib/ produces correct filename in warnings");
