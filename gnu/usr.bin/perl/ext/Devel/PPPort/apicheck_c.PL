################################################################################
#
#  apicheck_c.PL -- generate apicheck.c
#
################################################################################
#
#  $Revision: 1.1.1.1 $
#  $Author: millert $
#  $Date: 2005/01/15 21:16:45 $
#
################################################################################
#
#  Version 3.x, Copyright (C) 2004, Marcus Holland-Moritz.
#  Version 2.x, Copyright (C) 2001, Paul Marquess.
#  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

$out = 'apicheck.c';
print "creating $out\n";
system $^X, 'parts/apicheck.pl', $out
    and die "couldn't create $out\n";
