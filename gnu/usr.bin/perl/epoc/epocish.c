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

#if 1
int
epoc_spawn( char *cmd, char *cmdline) {
  RProcess p;
  TRequestStatus status;
  TInt rc;

  rc = p.Create( _L( cmd), _L( cmdline));
  if (rc != KErrNone) {
    return -1;
  }

  p.Resume();
  
  p.Logon( status);
  User::WaitForRequest( status);
  p.Kill( 0);
  if (status!=KErrNone) {
    return -1;
  }
  return 0;
}
#else 
int
epoc_spawn( char *cmd, char *cmdline) {
  int len = strlen(cmd) + strlen(cmdline) + 4;
  char *n = (char *) malloc( len);
  int r;
  strcpy( n, cmd);
  strcat( n, " ");
  strcat( n, cmdline);
  r = system( n);
  free( n);
  return r;
}
#endif 

/* Workaround for defect strtoul(). Values with leading + are zero */

unsigned long int epoc_strtoul(const char *nptr, char **endptr,
			       int base) {
  if (nptr && *nptr == '+')
    nptr++;
  return strtoul( nptr, endptr, base);
}

/* Workaround for defect atof(), see java defect list for epoc */
double epoc_atof( char* str) {
    TReal64 aRes;
    
    while (TChar( *str).IsSpace()) {
      str++;
    }

    TLex lex( _L( str));
    TInt err = lex.Val( aRes, TChar( '.'));
    return aRes;
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

#if 0
void epoc_spawn_posix_server() {
  SpawnPosixServerThread(); 
}
#endif
