/* $RCSfile: util.h,v $$Revision: 4.1 $$Date: 92/08/07 18:29:30 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	util.h,v $
 */

/* is the string for makedir a directory name or a filename? */

#define fatal Myfatal

#define MD_DIR 0
#define MD_FILE 1

#ifdef SETUIDGID
    int		eaccess();
#endif

char	*getwd();
int	makedir();

char * cpy2 _(( char *to, char *from, int delim ));
char * cpytill _(( char *to, char *from, int delim ));
void croak _(( char *pat, int a1, int a2, int a3, int a4 ));
void growstr _(( char **strptr, int *curlen, int newlen ));
char * instr _(( char *big, char *little ));
void Myfatal ();
char * safecpy _(( char *to, char *from, int len ));
char * savestr _(( char *str ));
void warn ();
