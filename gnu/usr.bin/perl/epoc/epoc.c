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

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

int 
do_aspawn( pTHX_ SV *really,SV **mark,SV **sp) {
  return do_spawn( really, mark, sp);
}

int
do_spawn (pTHX_ SV *really,SV **mark,SV **sp)
{
    dTHR;
    int  rc;
    char **a,*cmd,**ptr, *cmdline, **argv, *p2; 
    STRLEN n_a;
    size_t len = 0;

    if (sp<=mark)
      return -1;
    
    a=argv=ptr=(char**) malloc ((sp-mark+3)*sizeof (char*));
    
    while (++mark <= sp) {
      if (*mark)
	*a = SvPVx(*mark, n_a);
      else
	*a = "";
      len += strlen( *a) + 1;
      a++;
    }
    *a = Nullch;

    if (!(really && *(cmd = SvPV(really, n_a)))) {
      cmd = argv[0];
      argv++;
    }
      
    cmdline = (char * ) malloc( len + 1);
    cmdline[ 0] = '\0';
    while (*argv != NULL) {
      strcat( cmdline, *argv++);
      strcat( cmdline, " ");
    }

    for (p2=cmd; *p2 != '\0'; p2++) {
      /* Change / to \ */
      if ( *p2 == '/') 
	*p2 = '\\';
    }
    rc = epoc_spawn( cmd, cmdline);
    free( ptr);
    free( cmdline);
    
    return rc;
}

 
#endif
