/*    str.h
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1999, 2000,
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

struct string {
    char *	str_ptr;	/* pointer to malloced string */
    double	str_nval;	/* numeric value, if any */
    int		str_len;	/* allocated size */
    int		str_cur;	/* length of str_ptr as a C string */
    union {
	STR *str_next;		/* while free, link to next free str */
    } str_link;
    char	str_pok;	/* state of str_ptr */
    char	str_nok;	/* state of str_nval */
};

#define Nullstr Null(STR*)

/* the following macro updates any magic values this str is associated with */

#define STABSET(x) (x->str_link.str_magic && stabset(x->str_link.str_magic,x))

EXT STR **tmps_list;
EXT long tmps_max INIT(-1);

char * str_2ptr ( STR *str );
void str_cat ( STR *str, char *ptr );
void str_free ( STR *str );
char * str_gets ( STR *str, FILE *fp );
int str_len ( STR *str );
STR * str_make ( char *s );
void str_ncat ( STR *str, char *ptr, int len );
STR * str_new ( int len );
void str_nset ( STR *str, char *ptr, int len );
void str_numset ( STR *str, double num );
void str_scat ( STR *dstr, STR *sstr );
void str_set ( STR *str, char *ptr );
void str_sset ( STR *dstr, STR *sstr );
