################################################################################
#
#  apicheck_c.PL -- generate apicheck.c
#
################################################################################
#
#  $Revision: 1.1.1.2 $
#  $Author: millert $
#  $Date: 2006/03/28 18:47:56 $
#
################################################################################
#
#  Version 3.x, Copyright (C) 2004-2005, Marcus Holland-Moritz.
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
