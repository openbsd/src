/*
 *    Copyright (c) 1999 Olaf Flebbe o.flebbe@gmx.de
 *    
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/unistd.h>
#include <process.h>


#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

int 
do_spawn( char *cmd) {
    dTHX;
    return system( cmd);
}

int
do_aspawn ( void *vreally, void **vmark, void **vsp) {

    dTHX;

    SV *really = (SV*)vreally;
    SV **mark = (SV**)vmark;
    SV **sp = (SV**)vsp;

    char **argv;
    char *str;
    char *p2, **ptr;
    char *cmd;


    int  rc;
    int index = 0;

    if (sp<=mark)
      return -1;
    
    ptr = argv =(char**) malloc ((sp-mark+3)*sizeof (char*));
    
    while (++mark <= sp) {
      if (*mark && (str = SvPV_nolen(*mark)))
	argv[index] = str;
      else
	argv[index] = "";
    }
    argv[index++] = 0;

    cmd = strdup((const char*)(really ? SvPV_nolen(really) : argv[0]));

    rc = spawnvp( P_WAIT, cmd, argv);
    free( argv);
    free( cmd);

    return rc;
}

static
XS(epoc_getcwd)   /* more or less stolen from win32.c */
{
    dXSARGS;
    /* Make the host for current directory */
    char *buffer; 
    int buflen = 256;

    char *ptr;
    buffer = (char *) malloc( buflen);
    if (buffer == NULL) {
      XSRETURN_UNDEF;
    }
    while ((NULL == ( ptr = getcwd( buffer, buflen))) && (errno == ERANGE)) {
      buflen *= 2;
      if (NULL == realloc( buffer, buflen)) {
	 XSRETURN_UNDEF;
      }
      
    }

    /* 
     * If ptr != Nullch 
     *   then it worked, set PV valid, 
     *   else return 'undef' 
     */

    if (ptr) {
	SV *sv = sv_newmortal();
	char *tptr;

	for (tptr = ptr; *tptr != '\0'; tptr++) {
	  if (*tptr == '\\') {
	    *tptr = '/';
	  }
	}
	sv_setpv(sv, ptr);
	free( buffer);

	EXTEND(SP,1);
	SvPOK_on(sv);
	ST(0) = sv;
#ifndef INCOMPLETE_TAINTS
	SvTAINTED_on(ST(0));
#endif
	XSRETURN(1);
    }
    free( buffer);
    XSRETURN_UNDEF;
}
  

void
Perl_init_os_extras(void)
{ 
  dTHX;
  char *file = __FILE__;
  newXS("EPOC::getcwd", epoc_getcwd, file);
}

