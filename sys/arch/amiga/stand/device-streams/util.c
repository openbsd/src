/*	$OpenBSD: util.c,v 1.3 2001/09/05 22:32:38 deraadt Exp $	*/

/* -------------------------------------------------- 
 |  NAME
 |    util
 |  PURPOSE
 |    provide some standard useful utility functions.
 |  NOTES
 | 
 |  COPYRIGHT
 |    Copyright (C) 1993  Christian E. Hopps
 |
 |    This program is free software; you can redistribute it and/or modify
 |    it under the terms of the GNU General Public License as published by
 |    the Free Software Foundation; either version 2 of the License, or
 |    (at your option) any later version.
 |
 |    This program is distributed in the hope that it will be useful,
 |    but WITHOUT ANY WARRANTY; without even the implied warranty of
 |    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 |    GNU General Public License for more details.
 |
 |    You should have received a copy of the GNU General Public License
 |    along with this program; if not, write to the Free Software
 |    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 |    
 |  HISTORY
 |         chopps - Oct 9, 1993: Created.
 +--------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <error.h>
#include <ctype.h>
#include <stdarg.h>
#include "util.h"
#include "string.h"

/* Utility functions */

/* string functions.
 */
int
string_to_number(char *s, unsigned long *num)
{
    char *ns;
    int base, errsave = errno;
    unsigned long res;
    int len;
    s = stpblk(s);
    ns = s;
    len = strlen (s);
    if((!strnicmp(s,"0x",2))) {			  /* check 0[xX]nnnnnnnn */
	/* Hex number */
	s += 2;
	ns += 2;
	base = 16;
    } else if((!strnicmp(s,"x",1)) ||		  /* check [xX]nnnnnnnn */
	      (s[0] == '$')) {			  /* check $nnnnnnnn */
	/* Hex number */
	s++;
	ns++;
	base = 16;
    } else if((len && tolower (s[len-1]) == 'h') || /* check nnnnnnnn[Hh] */
	      (len>1 && tolower (s[len-2]) == 'h')) {
		  
	/* Hex number */
	base = 16;
    } else if((len && tolower (s[len-1]) == 'o') || /* check nnnnnnnnnn[Oo] */
	      (len>1 && tolower (s[len-2]) == 'o')) {
	/* octal */
	base = 8;
    } else if( (len && tolower (s[len-1]) == 'b') || /* check nnnnnnnnnn[Bb] */
	      (len>1 && tolower (s[len-2]) == 'b')) {
	/* binary */
	base = 2;
    } else {
	/* assume decimal */
	base = 10;
    }
    errno = 0;
    len = strlen (s);
    res = strtoul(s,&ns,base);
    if(ns == s || (res == 0xffffffff && errno == ERANGE)) {
	errno = errsave;
	return(0);				  /* failed */
    } else {
	errno = errsave;
	if ((len && s[len-1] == 'M') ||
	    (len>1 && s[len-2] == 'M')) {
	    /* result should be in Megabytes */
	    if (res <= 0xfff) {
		res <<= 20;
	    } else {
		errno = ERANGE;
		return (0);
	    }
	} else if ((len && tolower (s[len-1]) == 'k') ||
		   (len>1 && tolower (s[len-2]) == 'k')) {
	    /* result should be in kilobytes */
	    if (res <= 0x3ffffff) {
		res <<= 10;
	    } else {
		errno = ERANGE;
		return (0);
	    }		
	}
	*num = res;
    }
    return(ns - s);				  /* it worked */
}

char *
stripws (char *s)
{
    while (isspace (*s)) {
	s++;
    }
    return (s);
}

/* string = fgetline(fileptr)  :: replacement for fgets. no length limits. */
/* -------------------------                                              */
/* fgetline function returns a dynamic string of any length.  The string is */
/* the next line from ``fp'' arg.  Returns NULL for failure or EOF the */
/* reason can be detirmened by feof() and errno.  On an error that is not */
/* EOF will flush the buffer to EOL if possible.  The returned string has */
/* the newline stripped. */

/* sorry about the asm like comments I wrote this for a school project and */
/* the prof is decidedly in favor of verbosity.  I think the code is clear */
/* enough alone, and most of these comments clutter the clarity.  Oh well.*/

char *fgetline(FILE *fp)
{
    enum local_constants { locbufsize = 40 };
    char *retstr = NULL, *temp;
    char locbuf[locbufsize];
    char locbuflen = 0;
    
    while(1) {                                    /* do forever. */
        while(locbuflen < (locbufsize-1)) {
            int ch = fgetc(fp);                   /* get next character from */
                                                  /* stream.  */

            if(ch == EOF) {                       /* check for end of file. */
                free_string(retstr);              /* free_string retstr */
                                                  /* if EOF. */
                return(NULL);                     /* and return NULL. */
            } else if( ch == '\n' ) {
                locbuf[locbuflen] = 0;            /* got newline null term. */
                temp = concat_strings(retstr,locbuf); /* and concat local */
                                                      /* buffer.  */
                free_string(retstr);
                return(temp);                     /* return new string. */
            } else {
                locbuf[locbuflen++] = ch;         /* add to local buffer */
            }
        }
        /* we need to reset out local buffer. */
        locbuf[locbuflen] = 0;                    /* null terminate. */
        temp = retstr;
        retstr = concat_strings(retstr,locbuf);   /* concatenate locbuf to */
                                                  /* older string.  */
        locbuflen = 0;                            /* zero local buffer. */
        free_string(temp);                        /* free old string. */
        
        if(retstr == NULL) {
            flush_to_eol(fp);                     /* flush to EOL on fail. */
            return(NULL);                         /* and return NULL. */
        }
    }
}

/* flush ``fp'' to end of line, if possible.  returns 0 on success or EOF for */
/* error. */
int flush_to_eol(FILE *fp)
{
    int ch;
    while(EOF != (ch = fgetc(fp))) {		  /* loop until EOF */
        if(ch == '\n') {			  /* if newline, return. */
            return(0);
        }
    }
    return(EOF);
}

/* Concatenate 2 strings into a new one.  Both or either of the inputs */
/* ``before'' and ``after'' can be NULL.  returns NULL for failure */
/* setting errno acordingly. */
char *concat_strings(const char *before, const char *after)
{
    char *string = NULL;
    int len1 = 0, len2 = 0;
    if(before)					  /* if non null */
        len1 = strlen(before);			  /* get length */

    if(after)					  /* if non null */
        len2 = strlen(after);			  /* get length */
    
    string = malloc(len1 + len2 + 1);		  /* allocate storage for */
						  /* new string. */
    if(string) {				  
        memcpy(string,before,len1);		  /* copy ``before'' */
        memcpy(&string[len1],after,len2);	  /* cat ``after'' */
        string[len1+len2] = '\0';		  /* null terminate. */
    }
    return(string);				  /* return string (or NULL) */
}

/* free_string() - frees a string gotten from misc string routines. input */
/* can be NULL. */
void free_string(char *string)
{
    if(string)					  /* if non NULL */
        free(string);				  /* free string. */
}

char *
alloc_string (char *s)
{
    char *d = malloc (strlen (s) + 1);
    if (d) {
	strcpy (d, s);
    }
    return (d);
}

int
ask_bool (int def, int other, char *f, ...)
{
    char buffer[20];
    va_list ap;
    va_start (ap, f);
    vfprintf (mout, f, ap);
    fprintf (mout, "? [%lc%lc]:",toupper (def),tolower (other));
    va_end (ap);
    fflush (mout);
    if (fgets (buffer, 18, min)) {
	char *s = stripws (buffer);
	if (s[0] != 0 && s[0] != '\n') {
	    def = (int) s[0];
	}
    }
    if (buffer[strlen (buffer)-1] != '\n') {
	flush_to_eol (min);
    }
    return (def);
}

void *
zmalloc (size_t b)
{
    void *mem = malloc (b);
    if (mem) {
	memset (mem, 0, b);
    }
    return (mem);
}

void
zfree (void *mem)
{
    if (mem) 
	free (mem);
}

struct Node *
find_name (struct List *l, char *s)
{
    struct Node *n = l->lh_Head;
    while (n->ln_Succ) {
	if (!stricmp (s, n->ln_Name)) {
	    return (n);
	}
	n = n->ln_Succ;
    }
    return (NULL);
}

void
verbose_message (char *f, ...)
{
    if (opt_verbose) {
	va_list ap;
	va_start (ap, f);
	vfprintf (mout, f, ap);
	fprintf (mout, "\n");
	va_end (ap);
    }
}

void
debug_message (char *f, ...)
{
    if (opt_debug) {
	va_list ap;
	va_start (ap, f);
	fprintf (mout, "debug: ");
	vfprintf (mout, f, ap);
        fprintf (mout, "\n");
	va_end (ap);
    }
}

void
verbose_debug_message (char *f, ...)
{
    if (opt_verbose && opt_debug) {
	va_list ap;
	va_start (ap, f);
	fprintf (mout, "debug: ");
	vfprintf (mout, f, ap);
	fprintf (mout, "\n");
	va_end (ap);
    }
}

void
message (char *f, ...)
{
    va_list ap;
    va_start (ap, f);
    vfprintf (mout, f, ap);
    fprintf (mout, "\n");
    va_end (ap);
}

void
warn_message (char *f, ...)
{
    va_list ap;
    va_start (ap, f);
    fprintf (mout, "warn: ");
    vfprintf (mout, f, ap);
    fprintf (mout, "\n");
    va_end (ap);
}

void
vmessage (char *f, va_list ap)
{
    vfprintf (mout, f, ap);
    fprintf (mout, "\n");
}



