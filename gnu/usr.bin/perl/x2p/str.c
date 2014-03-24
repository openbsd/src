/*    str.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1999,
 *    2001, 2002, 2005 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#include "EXTERN.h"
#include "a2p.h"
#include "util.h"

void
str_numset(STR *str, double num)
{
    str->str_nval = num;
    str->str_pok = 0;		/* invalidate pointer */
    str->str_nok = 1;		/* validate number */
}

char *
str_2ptr(STR *str)
{
    char *s;

    if (!str)
	return (char *)"";	/* probably safe - won't be written to */
    GROWSTR(&(str->str_ptr), &(str->str_len), 24);
    s = str->str_ptr;
    if (str->str_nok) {
	sprintf(s,"%.20g",str->str_nval);
	while (*s) s++;
    }
    *s = '\0';
    str->str_cur = s - str->str_ptr;
    str->str_pok = 1;
#ifdef DEBUGGING
    if (debug & 32)
	fprintf(stderr,"0x%lx ptr(%s)\n",(unsigned long)str,str->str_ptr);
#endif
    return str->str_ptr;
}

void
str_sset(STR *dstr, STR *sstr)
{
    if (!sstr)
	str_nset(dstr,No,0);
    else if (sstr->str_nok)
	str_numset(dstr,sstr->str_nval);
    else if (sstr->str_pok)
	str_nset(dstr,sstr->str_ptr,sstr->str_cur);
    else
	str_nset(dstr,"",0);
}

void
str_nset(STR *str, const char *ptr, int len)
{
    GROWSTR(&(str->str_ptr), &(str->str_len), len + 1);
    memcpy(str->str_ptr,ptr,len);
    str->str_cur = len;
    *(str->str_ptr+str->str_cur) = '\0';
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
}

void
str_set(STR *str, const char *ptr)
{
    int len;

    if (!ptr)
	ptr = "";
    len = strlen(ptr);
    GROWSTR(&(str->str_ptr), &(str->str_len), len + 1);
    memcpy(str->str_ptr,ptr,len+1);
    str->str_cur = len;
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
}

void
str_ncat(STR *str, const char *ptr, int len)
{
    if (!(str->str_pok))
	str_2ptr(str);
    GROWSTR(&(str->str_ptr), &(str->str_len), str->str_cur + len + 1);
    memcpy(str->str_ptr+str->str_cur, ptr, len);
    str->str_cur += len;
    *(str->str_ptr+str->str_cur) = '\0';
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
}

void
str_scat(STR *dstr, STR *sstr)
{
    if (!(sstr->str_pok))
	str_2ptr(sstr);
    if (sstr)
	str_ncat(dstr,sstr->str_ptr,sstr->str_cur);
}

void
str_cat(STR *str, const char *ptr)
{
    int len;

    if (!ptr)
	return;
    if (!(str->str_pok))
	str_2ptr(str);
    len = strlen(ptr);
    GROWSTR(&(str->str_ptr), &(str->str_len), str->str_cur + len + 1);
    memcpy(str->str_ptr+str->str_cur, ptr, len+1);
    str->str_cur += len;
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
}

STR *
str_new(int len)
{
    STR *str;
    
    if (freestrroot) {
	str = freestrroot;
	freestrroot = str->str_link.str_next;
    }
    else {
	str = (STR *) safemalloc(sizeof(STR));
	memset((char*)str,0,sizeof(STR));
    }
    if (len)
	GROWSTR(&(str->str_ptr), &(str->str_len), len + 1);
    return str;
}

/* make str point to what nstr did */

void
str_free(STR *str)
{
    if (!str)
	return;
    if (str->str_len)
	str->str_ptr[0] = '\0';
    str->str_cur = 0;
    str->str_nok = 0;
    str->str_pok = 0;
    str->str_link.str_next = freestrroot;
    freestrroot = str;
}

int
str_len(STR *str)
{
    if (!str)
	return 0;
    if (!(str->str_pok))
	str_2ptr(str);
    if (str->str_len)
	return str->str_cur;
    else
	return 0;
}

char *
str_gets(STR *str, FILE *fp)
{
#if defined(USE_STDIO_PTR) && defined(STDIO_PTR_LVALUE) && defined(STDIO_CNT_LVALUE)
    /* Here is some breathtakingly efficient cheating */

    char *bp;		/* we're going to steal some values */
    int cnt;		/*  from the stdio struct and put EVERYTHING */
    STDCHAR *ptr;	/*   in the innermost loop into registers */
    char newline = '\n';	/* (assuming at least 6 registers) */
    int i;
    int bpx;

#if defined(VMS)
    /* An ungetc()d char is handled separately from the regular
     * buffer, so we getc() it back out and stuff it in the buffer.
     */
    i = getc(fp);
    if (i == EOF) return NULL;
    *(--((*fp)->_ptr)) = (unsigned char) i;
    (*fp)->_cnt++;
#endif

    cnt = FILE_cnt(fp);			/* get count into register */
    str->str_nok = 0;			/* invalidate number */
    str->str_pok = 1;			/* validate pointer */
    if (str->str_len <= cnt)		/* make sure we have the room */
	GROWSTR(&(str->str_ptr), &(str->str_len), cnt+1);
    bp = str->str_ptr;			/* move these two too to registers */
    ptr = (STDCHAR*)FILE_ptr(fp);
    for (;;) {
	while (--cnt >= 0) {
	    if ((*bp++ = *ptr++) == newline) {
		if (bp <= str->str_ptr || bp[-2] != '\\')
		    goto thats_all_folks;
		else {
		    line++;
		    bp -= 2;
		}
	    }
	}
	
	FILE_cnt(fp) = cnt;		/* deregisterize cnt and ptr */
	FILE_ptr(fp) = ptr;
	i = getc(fp);		/* get more characters */
	cnt = FILE_cnt(fp);
	ptr = (STDCHAR*)FILE_ptr(fp);		/* reregisterize cnt and ptr */

	bpx = bp - str->str_ptr;	/* prepare for possible relocation */
	GROWSTR(&(str->str_ptr), &(str->str_len), str->str_cur + cnt + 1);
	bp = str->str_ptr + bpx;	/* reconstitute our pointer */

	if (i == newline) {		/* all done for now? */
	    *bp++ = i;
	    goto thats_all_folks;
	}
	else if (i == EOF)		/* all done for ever? */
	    goto thats_all_folks;
	*bp++ = i;			/* now go back to screaming loop */
    }

thats_all_folks:
    FILE_cnt(fp) = cnt;			/* put these back or we're in trouble */
    FILE_ptr(fp) = ptr;
    *bp = '\0';
    str->str_cur = bp - str->str_ptr;	/* set length */

#else /* USE_STDIO_PTR && STDIO_PTR_LVALUE && STDIO_CNT_LVALUE */
    /* The big, slow, and stupid way */

    static char buf[4192];

    if (fgets(buf, sizeof buf, fp) != NULL)
	str_set(str, buf);
    else
	str_set(str, No);

#endif /* USE_STDIO_PTR && STDIO_PTR_LVALUE && STDIO_CNT_LVALUE */

    return str->str_cur ? str->str_ptr : NULL;
}

STR *
str_make(const char *s)
{
    STR *str = str_new(0);

    str_set(str,s);
    return str;
}

