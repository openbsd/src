/*
 *    Copyright (c) 1999 Olaf Flebbe o.flebbe@gmx.de
 *    
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/* This is indeed C++ Code !! */

#include <e32std.h>

extern "C" { 

epoc_spawn( char *cmd, char *cmdline) {
  RProcess p;
  TRequestStatus status;
  TInt rc;

  rc = p.Create( _L( cmd), _L( cmdline));
  if (rc != KErrNone)
    return -1;

  p.Resume();
  
  p.Logon( status);
  User::WaitForRequest( status);
  if (status!=KErrNone) {
    return -1;
  }
  return 0;
}

}
