################################################################################
#
#  apicheck_c.PL -- generate apicheck.c
#
################################################################################
#
#  $Revision: 1.2 $
#  $Author: millert $
#  $Date: 2009/10/12 18:24:26 $
#
################################################################################
#
#  Version 3.x, Copyright (C) 2004-2009, Marcus Holland-Moritz.
#  Version 2.x, Copyright (C) 2001, Paul Marquess.
#  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

use strict;

my $out = 'apicheck.c';
my @api = map { /^--api=(\w+)$/ ? ($1) : () } @ARGV;
print "creating $out", (@api ? " (@api)" : ''), "\n";
system $^X, 'parts/apicheck.pl', @api, $out
    and die "couldn't create $out\n";

