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

void
Perl_epoc_init(int *argcp, char ***argvp) {
  int i;
  int truecount=0;
  char **lastcp = (*argvp);
  char *ptr;

#if 0
  epoc_spawn_posix_server();
#endif
  for (i=0; i< *argcp; i++) {
    if ((*argvp)[i]) {
      if (*((*argvp)[i]) == '<') {
	if (strlen((*argvp)[i]) > 1) {
	  ptr =((*argvp)[i])+1;
	} else {
	  i++;
	  ptr = ((*argvp)[i]);
	}
	freopen(  ptr, "r", stdin);
      } else if (*((*argvp)[i]) == '>') {
	if (strlen((*argvp)[i]) > 1) {
	  ptr =((*argvp)[i])+1;
	} else {
	  i++;
	  ptr = ((*argvp)[i]);
	}
	freopen(  ptr, "w", stdout);
      } else if ((*((*argvp)[i]) == '2') && (*(((*argvp)[i])+1) == '>')) {
	if (strcmp( (*argvp)[i], "2>&1") == 0) {
	  dup2( fileno( stdout), fileno( stderr));
	} else {
          if (strlen((*argvp)[i]) > 2) {
            ptr =((*argvp)[i])+2;
	  } else {
	    i++;
	    ptr = ((*argvp)[i]);
	  }
	  freopen(  ptr, "w", stderr);
	}
      } else {
	*lastcp++ = (*argvp)[i];
	truecount++;
      }
    } 
  }
  *argcp=truecount;
      

}


#ifdef __MARM__
/* Symbian forgot to include __fixunsdfi into the MARM euser.lib */
/* This is from libgcc2.c , gcc-2.7.2.3                          */

typedef unsigned int UQItype	__attribute__ ((mode (QI)));
typedef 	 int SItype	__attribute__ ((mode (SI)));
typedef unsigned int USItype	__attribute__ ((mode (SI)));
typedef		 int DItype	__attribute__ ((mode (DI)));
typedef unsigned int UDItype	__attribute__ ((mode (DI)));

typedef 	float SFtype	__attribute__ ((mode (SF)));
typedef		float DFtype	__attribute__ ((mode (DF)));



extern DItype __fixunssfdi (SFtype a);
extern DItype __fixunsdfdi (DFtype a);


USItype
__fixunsdfsi (a)
     DFtype a;
{
  if (a >= - (DFtype) (- 2147483647L  -1) )
    return (SItype) (a + (- 2147483647L  -1) ) - (- 2147483647L  -1) ;
  return (SItype) a;
}

#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

int 
do_spawn( char *cmd) {
    dTHXo;
    char *argv0, *ptr;
    char *cmdptr = cmd;
    int ret;
    
    argv0 = ptr = malloc( strlen(cmd) + 1);

    while (*cmdptr && !isSPACE( *cmdptr)) {
      *ptr = *cmdptr;
      if (*ptr == '/') {
	*ptr = '\\';
      }
      ptr++; cmdptr++;
    }
    while (*cmdptr && isSPACE( *cmdptr)) {
      cmdptr++;
    }
    *ptr = '\0';
    ret = epoc_spawn( argv0, cmdptr);
    free( argv0);
    return ret;
}

int
do_aspawn ( void *vreally, void **vmark, void **vsp) {

    dTHXo;

    SV *really = (SV*)vreally;
    SV **mark = (SV**)vmark;
    SV **sp = (SV**)vsp;

    char **argv;
    char *str;
    char *p2, **ptr;
    char *cmd, *cmdline;


    int  rc;
    int index = 0;
    int len = 0;

    if (sp<=mark)
      return -1;
    
    ptr = argv =(char**) malloc ((sp-mark+3)*sizeof (char*));
    
    while (++mark <= sp) {
      if (*mark && (str = SvPV_nolen(*mark)))
	argv[index] = str;
      else
	argv[index] = "";
      
      len += strlen(argv[ index++]) + 1;
    }
    argv[index++] = 0;

    cmd = strdup((const char*)(really ? SvPV_nolen(really) : argv[0]));

    for (p2=cmd; *p2 != '\0'; p2++) {
      /* Change / to \ */
      if ( *p2 == '/') 
	*p2 = '\\';
    }
      
    cmdline = (char * ) malloc( len + 1);
    cmdline[ 0] = '\0';
    while (*argv != NULL) {
      strcat( cmdline, *ptr++);
      strcat( cmdline, " ");
    }
    
    free( argv);

    rc = epoc_spawn( cmd, cmdline);
    free( cmdline);
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
	XSRETURN(1);
    }
    free( buffer);
    XSRETURN_UNDEF;
}
  

void
Perl_init_os_extras(void)
{ 
  dTHXo;
  char *file = __FILE__;
  newXS("EPOC::getcwd", epoc_getcwd, file);
}

void
Perl_my_setenv(pTHX_ char *nam,char *val) {
  setenv( nam, val, 1);
}
