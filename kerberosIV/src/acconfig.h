/* $KTH: acconfig.h,v 1.106 2000/11/05 17:14:13 joda Exp $ */

@BOTTOM@

/* This for compat with heimdal (or something) */
#define KRB_PUT_INT(f, t, l, s) krb_put_int((f), (t), (l), (s))

#define HAVE_KRB_ENABLE_DEBUG 1

#define HAVE_KRB_DISABLE_DEBUG 1

#define HAVE_KRB_GET_OUR_IP_FOR_REALM 1

/*
 * Set ORGANIZATION to be the desired organization string printed
 * by the 'kinit' program.  It may have spaces.
 */
#define ORGANIZATION "eBones International"

/* ftp stuff -------------------------------------------------- */

#define KERBEROS

/* telnet stuff ----------------------------------------------- */

/* define this if you have kerberos 4 */
#undef KRB4

/* define this if you want encryption */
#undef ENCRYPTION

/* define this if you want authentication */
#undef AUTHENTICATION

#if defined(ENCRYPTION) && !defined(AUTHENTICATION)
#define AUTHENTICATION 1
#endif

/* Set this if you want des encryption */
#undef DES_ENCRYPTION

/* Set this to the default system lead string for telnetd 
 * can contain %-escapes: %s=sysname, %m=machine, %r=os-release
 * %v=os-version, %t=tty, %h=hostname, %d=date and time
 */
#undef USE_IM

/* define this if you want diagnostics in telnetd */
#undef DIAGNOSTICS

/* define this if you want support for broken ENV_{VALUE,VAR} systems  */
#undef ENV_HACK

/*  */
#undef OLD_ENVIRON

/* Used with login -p */
#undef LOGIN_ARGS

/* set this to a sensible login */
#ifndef LOGIN_PATH
#define LOGIN_PATH BINDIR "/login"
#endif


/* ------------------------------------------------------------ */

/* Temporary fixes for krb_{rd,mk}_safe */
#define DES_QUAD_GUESS 0
#define DES_QUAD_NEW 1
#define DES_QUAD_OLD 2

/*
 * All these are system-specific defines that I would rather not have at all.
 */

/*
 * AIX braindamage!
 */
#if _AIX
#define _ALL_SOURCE
/* XXX this is gross, but kills about a gazillion warnings */
struct ether_addr;
struct sockaddr;
struct sockaddr_dl;
struct sockaddr_in;
#endif

#if defined(__sgi) || defined(sgi)
#if defined(__SYSTYPE_SVR4) || defined(_SYSTYPE_SVR4)
#define IRIX 5
#else
#define IRIX 4
#endif
#endif

/* IRIX 4 braindamage */
#if IRIX == 4 && !defined(__STDC__)
#define __STDC__ 0
#endif

/*
 * Defining this enables lots of useful (and used) extensions on
 * glibc-based systems such as Linux
 */

#define _GNU_SOURCE

/* some strange OS/2 stuff.  From <d96-mst@nada.kth.se> */

#ifdef __EMX__
#define _EMX_TCPIP
#define MAIL_USE_SYSTEM_LOCK
#endif

#ifdef ROKEN_RENAME
#include "roken_rename.h"
#endif
