/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: clnt_perror.c,v 1.15 2002/09/10 05:39:07 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * clnt_perror.c
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>

static char *auth_errmsg(enum auth_stat stat);
#define CLNT_PERROR_BUFLEN 256

static char *buf;

static char *
_buf()
{

	if (buf == NULL)
		buf = (char *)malloc(CLNT_PERROR_BUFLEN);
	return (buf);
}

/*
 * Print reply error info
 */
char *
clnt_sperror(rpch, s)
	CLIENT *rpch;
	char *s;
{
	char *err, *str = _buf(), *strstart;
	struct rpc_err e;
	int ret, len = CLNT_PERROR_BUFLEN;

	strstart = str;
	if (str == NULL)
		return (0);
	CLNT_GETERR(rpch, &e);

	ret = snprintf(str, len, "%s: %s", s, clnt_sperrno(e.re_status));
	if (ret == -1)
		ret = 0;
	str += ret;
	len -= ret;
	if (str > strstart + CLNT_PERROR_BUFLEN)
		goto truncated;

	switch (e.re_status) {
	case RPC_SUCCESS:
	case RPC_CANTENCODEARGS:
	case RPC_CANTDECODERES:
	case RPC_TIMEDOUT:
	case RPC_PROGUNAVAIL:
	case RPC_PROCUNAVAIL:
	case RPC_CANTDECODEARGS:
	case RPC_SYSTEMERROR:
	case RPC_UNKNOWNHOST:
	case RPC_UNKNOWNPROTO:
	case RPC_PMAPFAILURE:
	case RPC_PROGNOTREGISTERED:
	case RPC_FAILED:
		break;

	case RPC_CANTSEND:
	case RPC_CANTRECV:
		snprintf(str, len, "; errno = %s\n", strerror(e.re_errno));
		break;

	case RPC_VERSMISMATCH:
		snprintf(str, len, "; low version = %u, high version = %u\n",
		    e.re_vers.low, e.re_vers.high);
		break;

	case RPC_AUTHERROR:
		ret = snprintf(str, len, "; why = ");
		if (ret == -1)
			ret = 0;
		str += ret;
		len -= ret;
		if (str > strstart + CLNT_PERROR_BUFLEN)
			goto truncated;
		err = auth_errmsg(e.re_why);
		if (err != NULL) {
			ret = snprintf(str, len, "%s\n", err);
		} else {
			ret = snprintf(str, len,
			    "(unknown authentication error - %d)\n",
			    (int) e.re_why);
		}
		break;

	case RPC_PROGVERSMISMATCH:
		snprintf(str, len, "; low version = %u, high version = %u\n",
		    e.re_vers.low, e.re_vers.high);
		break;

	default:	/* unknown */
		snprintf(str, len, "; s1 = %u, s2 = %u\n",
		    e.re_lb.s1, e.re_lb.s2);
		break;
	}
	strstart[CLNT_PERROR_BUFLEN-2] = '\0';
	strlcat(strstart, "\n", CLNT_PERROR_BUFLEN);
	return (strstart);

truncated:
	snprintf(strstart + CLNT_PERROR_BUFLEN - 5, 5, "...\n");
	return (strstart);
}

void
clnt_perror(rpch, s)
	CLIENT *rpch;
	char *s;
{
	(void) fprintf(stderr, "%s", clnt_sperror(rpch, s));
}

static const char *const rpc_errlist[] = {
	"RPC: Success",				/*  0 - RPC_SUCCESS */
	"RPC: Can't encode arguments",		/*  1 - RPC_CANTENCODEARGS */
	"RPC: Can't decode result",		/*  2 - RPC_CANTDECODERES */
	"RPC: Unable to send",			/*  3 - RPC_CANTSEND */
	"RPC: Unable to receive",		/*  4 - RPC_CANTRECV */
	"RPC: Timed out",			/*  5 - RPC_TIMEDOUT */
	"RPC: Incompatible versions of RPC",	/*  6 - RPC_VERSMISMATCH */
	"RPC: Authentication error",		/*  7 - RPC_AUTHERROR */
	"RPC: Program unavailable",		/*  8 - RPC_PROGUNAVAIL */
	"RPC: Program/version mismatch",	/*  9 - RPC_PROGVERSMISMATCH */
	"RPC: Procedure unavailable",		/* 10 - RPC_PROCUNAVAIL */
	"RPC: Server can't decode arguments",	/* 11 - RPC_CANTDECODEARGS */
	"RPC: Remote system error",		/* 12 - RPC_SYSTEMERROR */
	"RPC: Unknown host",			/* 13 - RPC_UNKNOWNHOST */
	"RPC: Port mapper failure",		/* 14 - RPC_PMAPFAILURE */
	"RPC: Program not registered",		/* 15 - RPC_PROGNOTREGISTERED */
	"RPC: Failed (unspecified error)",	/* 16 - RPC_FAILED */
	"RPC: Unknown protocol"			/* 17 - RPC_UNKNOWNPROTO */
};


/*
 * This interface for use by clntrpc
 */
char *
clnt_sperrno(stat)
	enum clnt_stat stat;
{
	unsigned int errnum = stat;

	if (errnum < (sizeof(rpc_errlist)/sizeof(rpc_errlist[0])))
		return (char *)rpc_errlist[errnum];

	return ("RPC: (unknown error code)");
}

void
clnt_perrno(num)
	enum clnt_stat num;
{
	(void) fprintf(stderr, "%s\n", clnt_sperrno(num));
}


char *
clnt_spcreateerror(s)
	char *s;
{
	char *str = _buf();

	if (str == NULL)
		return (0);

	switch (rpc_createerr.cf_stat) {
	case RPC_PMAPFAILURE:
		(void) snprintf(str, CLNT_PERROR_BUFLEN, "%s: %s - %s\n", s,
		    clnt_sperrno(rpc_createerr.cf_stat),
		    clnt_sperrno(rpc_createerr.cf_error.re_status));
		break;

	case RPC_SYSTEMERROR:
		(void) snprintf(str, CLNT_PERROR_BUFLEN, "%s: %s - %s\n", s,
		    clnt_sperrno(rpc_createerr.cf_stat),
		    strerror(rpc_createerr.cf_error.re_errno));
		break;

	default:
		(void) snprintf(str, CLNT_PERROR_BUFLEN, "%s: %s\n", s,
		    clnt_sperrno(rpc_createerr.cf_stat));
		break;
	}
	str[CLNT_PERROR_BUFLEN-2] = '\n';
	str[CLNT_PERROR_BUFLEN-1] = '\0';
	return (str);
}

void
clnt_pcreateerror(s)
	char *s;
{
	(void) fprintf(stderr, "%s", clnt_spcreateerror(s));
}

static const char *const auth_errlist[] = {
	"Authentication OK",			/* 0 - AUTH_OK */
	"Invalid client credential",		/* 1 - AUTH_BADCRED */
	"Server rejected credential",		/* 2 - AUTH_REJECTEDCRED */
	"Invalid client verifier", 		/* 3 - AUTH_BADVERF */
	"Server rejected verifier", 		/* 4 - AUTH_REJECTEDVERF */
	"Client credential too weak",		/* 5 - AUTH_TOOWEAK */
	"Invalid server verifier",		/* 6 - AUTH_INVALIDRESP */
	"Failed (unspecified error)"		/* 7 - AUTH_FAILED */
};

static char *
auth_errmsg(stat)
	enum auth_stat stat;
{
	unsigned int errnum = stat;

	if (errnum < (sizeof(auth_errlist)/sizeof(auth_errlist[0])))
		return (char *)auth_errlist[errnum];

	return (NULL);
}
