/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/kname_parse.c,v $
 *
 * $Locker:  $
 */

/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"

#define INSTANCE_DOTS_OK        0

/* max size of full name */
#define FULL_SZ (ANAME_SZ + INST_SZ + REALM_SZ)

#define NAME    0		/* which field are we in? */
#define INST    1
#define REALM   2

/*
 * This file contains four routines for handling Kerberos names.
 *
 * kname_parse() breaks a Kerberos name into its name, instance,
 * and realm components.
 *
 * k_isname(), k_isinst(), and k_isrealm() check a given string to see if
 * it's a syntactically legitimate respective part of a Kerberos name,
 * returning 1 if it is, 0 if it isn't.
 *
 * Definition of "syntactically legitimate" names is according to
 * the Project Athena Technical Plan Section E.2.1, page 7 "Specifying
 * names", version dated 21 Dec 1987.
 */

/*
 * kname_parse() takes a Kerberos name "fullname" of the form:
 *
 *		username[.instance][@realm]
 *
 * and returns the three components ("name", "instance", and "realm"
 * in the example above) in the given arguments "np", "ip", and "rp".
 *
 * If successful, it returns KSUCCESS.  If there was an error,
 * KNAME_FMT is returned.
 */

int
kname_parse(np, ip, rp, fullname)
	char *np;
	char *ip;
	char *rp;
	char *fullname;
{
    static char buf[FULL_SZ];
    char *rnext, *wnext;	/* next char to read, write */
    register char c;
    int backslash;
    int field;

    backslash = 0;
    rnext = buf;
    wnext = np;
    field = NAME;

    if (strlen(fullname) > FULL_SZ)
        return KNAME_FMT;
    (void) strcpy(buf, fullname);

    while ((c = *rnext++)) {
        if (backslash) {
            *wnext++ = c;
            backslash = 0;
            continue;
        }
        switch (c) {
        case '\\':
            backslash++;
            break;
        case '.':
            switch (field) {
            case NAME:
                if (wnext == np)
                    return KNAME_FMT;
                *wnext = '\0';
                field = INST;
                wnext = ip;
                break;
            case INST:
#if     INSTANCE_DOTS_OK
                *wnext++ = c;
                break;
#else /* INSTANCE_DOTS_OK */
                return KNAME_FMT; 
#endif /* INSTANCE_DOTS_OK */
                /* break; */
            case REALM:
                *wnext++ = c;
                break;
            default:
                fprintf(stderr, "unknown field value\n");
                exit(1);
            }
            break;
        case '@':
            switch (field) {
            case NAME:
                if (wnext == np)
                    return KNAME_FMT;
                *ip = '\0';
                /* fall through */
            case INST:
                *wnext = '\0';
                field = REALM;
                wnext = rp;
                break;
            case REALM:
                return KNAME_FMT;
            default:
                fprintf(stderr, "unknown field value\n");
                exit(1);
            }
            break;
        default:
            *wnext++ = c;
        }
    }
    *wnext = '\0';
    if ((strlen(np) > ANAME_SZ - 1) ||
        (strlen(ip) > INST_SZ  - 1) ||
        (strlen(rp) > REALM_SZ - 1))
        return KNAME_FMT;
    return KSUCCESS;
}

/*
 * k_isname() returns 1 if the given name is a syntactically legitimate
 * Kerberos name; returns 0 if it's not.
 */

int
k_isname(s)
	char *s;
{
    register char c;
    int backslash = 0;

    if (!*s)
        return 0;
    if (strlen(s) > ANAME_SZ - 1)
        return 0;
    while ((c = *s++)) {
        if (backslash) {
            backslash = 0;
            continue;
        }
        switch(c) {
        case '\\':
            backslash = 1;
            break;
        case '.':
            return 0;
            /* break; */
        case '@':
            return 0;
            /* break; */
        }
    }
    return 1;
}


/*
 * k_isinst() returns 1 if the given name is a syntactically legitimate
 * Kerberos instance; returns 0 if it's not.
 */

int
k_isinst(s)
	char *s;
{
    register char c;
    int backslash = 0;

    if (strlen(s) > INST_SZ - 1)
        return 0;
    while ((c = *s++)) {
        if (backslash) {
            backslash = 0;
            continue;
        }
        switch(c) {
        case '\\':
            backslash = 1;
            break;
        case '.':
#if     INSTANCE_DOTS_OK
            break;
#else /* INSTANCE_DOTS_OK */
            return 0; 
#endif /* INSTANCE_DOTS_OK */
            /* break; */
        case '@':
            return 0;
            /* break; */
        }
    }
    return 1;
}

/*
 * k_isrealm() returns 1 if the given name is a syntactically legitimate
 * Kerberos realm; returns 0 if it's not.
 */

int
k_isrealm(s)
	char *s;
{
    register char c;
    int backslash = 0;

    if (!*s)
        return 0;
    if (strlen(s) > REALM_SZ - 1)
        return 0;
    while ((c = *s++)) {
        if (backslash) {
            backslash = 0;
            continue;
        }
        switch(c) {
        case '\\':
            backslash = 1;
            break;
        case '@':
            return 0;
            /* break; */
        }
    }
    return 1;
}
