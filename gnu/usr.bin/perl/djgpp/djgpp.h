#ifndef PERL_DJGPP_DJGPP_H
#define PERL_DJGPP_DJGPP_H

#include <libc/stubs.h>
#include <io.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libc/file.h>
#include <process.h>
#include <fcntl.h>
#include <glob.h>
#include <sys/fsext.h>
#include <crt0.h>
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

FILE *
djgpp_popen (const char *cm, const char *md);

int
djgpp_pclose (FILE *pp);

int
do_aspawn (pTHX_ SV *really,SV **mark,SV **sp);

int
do_spawn2 (pTHX_ char *cmd,int execf);

int
do_spawn (pTHX_ char *cmd);

bool
Perl_do_exec (pTHX_ const char *cmd);

void
Perl_init_os_extras(pTHX);

char
*djgpp_pathexp (const char *p);

void
Perl_DJGPP_init (int *argcp,char ***argvp);

int
djgpp_fflush (FILE *fp);

/* DJGPP utility functions without prototypes? */

int _is_unixy_shell(char *s);

#endif
