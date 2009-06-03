/*	$OpenBSD: clnt_perror.c,v 1.22 2009/06/03 03:34:00 schwarze Exp $ */
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

static char buf[CLNT_PERROR_BUFLEN];

/*
 * Print reply error info
 */
char *
clnt_sperror(CLIENT *rpch, char *s)
{
	char *err, *str = buf;
	struct rpc_err e;
	int ret, len = CLNT_PERROR_BUFLEN;

	CLNT_GETERR(rpch, &e);

	ret = snprintf(str, len, "%s: %s", s, clnt_sperrno(e.re_status));
	if (ret == -1)
		ret = 0;
	else if (ret >= len)
		goto truncated;
	str += ret;
	len -= ret;

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
		ret = snprintf(str, len, "; errno = %s", strerror(e.re_errno));
		if (ret == -1 || ret >= len)
			goto truncated;
		break;

	case RPC_VERSMISMATCH:
		ret = snprintf(str, len,
		    "; low version = %u, high version = %u",
		    e.re_vers.low, e.re_vers.high);
		if (ret == -1 || ret >= len)
			goto truncated;
		break;

	case RPC_AUTHERROR:
		ret = snprintf(str, len, "; why = ");
		if (ret == -1)
			ret = 0;
		else if (ret >= len)
			goto truncated;
		str += ret;
		len -= ret;
		err = auth_errmsg(e.re_why);
		if (err != NULL) {
			ret = snprintf(str, len, "%s", err);
			if (ret == -1 || ret >= len)
				goto truncated;
		} else {
			ret = snprintf(str, len,
			    "(unknown authentication error - %d)",
			    (int) e.re_why);
			if (ret == -1 || ret >= len)
				goto truncated;
		}
		break;

	case RPC_PROGVERSMISMATCH:
		ret = snprintf(str, len,
		    "; low version = %u, high version = %u",
		    e.re_vers.low, e.re_vers.high);
		if (ret == -1 || ret >= len)
			goto truncated;
		break;

	default:	/* unknown */
		ret = snprintf(str, len, "; s1 = %u, s2 = %u",
		    e.re_lb.s1, e.re_lb.s2);
		if (ret == -1 || ret >= len)
			goto truncated;
		break;
	}
	if (strlcat(buf, "\n", CLNT_PERROR_BUFLEN) >= CLNT_PERROR_BUFLEN)
		goto truncated;
	return (buf);

truncated:
	snprintf(buf + CLNT_PERROR_BUFLEN - 5, 5, "...\n");
	return (buf);
}

void
clnt_perror(CLIENT *rpch, char *s)
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
clnt_sperrno(enum clnt_stat stat)
{
	unsigned int errnum = stat;

	if (errnum < (sizeof(rpc_errlist)/sizeof(rpc_errlist[0])))
		return (char *)rpc_errlist[errnum];

	return ("RPC: (unknown error code)");
}

void
clnt_perrno(enum clnt_stat num)
{
	(void) fprintf(stderr, "%s\n", clnt_sperrno(num));
}


char *
clnt_spcreateerror(char *s)
{
	switch (rpc_createerr.cf_stat) {
	case RPC_PMAPFAILURE:
		(void) snprintf(buf, CLNT_PERROR_BUFLEN, "%s: %s - %s\n", s,
		    clnt_sperrno(rpc_createerr.cf_stat),
		    clnt_sperrno(rpc_createerr.cf_error.re_status));
		break;

	case RPC_SYSTEMERROR:
		(void) snprintf(buf, CLNT_PERROR_BUFLEN, "%s: %s - %s\n", s,
		    clnt_sperrno(rpc_createerr.cf_stat),
		    strerror(rpc_createerr.cf_error.re_errno));
		break;

	default:
		(void) snprintf(buf, CLNT_PERROR_BUFLEN, "%s: %s\n", s,
		    clnt_sperrno(rpc_createerr.cf_stat));
		break;
	}
	buf[CLNT_PERROR_BUFLEN-2] = '\n';
	buf[CLNT_PERROR_BUFLEN-1] = '\0';
	return (buf);
}

void
clnt_pcreateerror(char *s)
{
	fprintf(stderr, "%s", clnt_spcreateerror(s));
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
auth_errmsg(enum auth_stat stat)
{
	unsigned int errnum = stat;

	if (errnum < (sizeof(auth_errlist)/sizeof(auth_errlist[0])))
		return (char *)auth_errlist[errnum];

	return (NULL);
}
