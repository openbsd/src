/*
 *    Copyright (c) 1999 Olaf Flebbe o.flebbe@gmx.de
 *    
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/* This is C++ Code !! */

#include <e32std.h>
#include <stdlib.h>
#include <estlib.h>
#include <string.h>

extern "C" { 


/* Workaround for defect strtoul(). Values with leading + are zero */

unsigned long int epoc_strtoul(const char *nptr, char **endptr,
			       int base) {
  if (nptr && *nptr == '+')
    nptr++;
  return strtoul( nptr, endptr, base);
}

void epoc_gcvt( double x, int digits, unsigned char *buf) {
    TRealFormat trel;

    trel.iPlaces = digits;
    trel.iPoint = TChar( '.');

    TPtr result( buf, 80);

    result.Num( x, trel);
    result.Append( TChar( 0));
  }
}


