/*	$OpenBSD: yplib.c,v 1.2 2000/03/01 22:10:12 todd Exp $	*/
/*	$NetBSD: yplib.c,v 1.1.1.1 1995/10/08 23:08:48 gwr Exp $	*/

/*
 * Copyright (c) 1992, 1993 Theo de Raadt <deraadt@fsa.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This file provides "stubs" for all the YP library functions.
 * It is not needed unless you pull in things that call YP, and
 * if you use all the get* files here then the YP stuff should
 * not get dragged in.  But if it does, one can use this.
 *
 * This was copied from:
 *      lib/libc/yp/yplib.c
 * (and then completely gutted! 8^)
 */

#include <sys/types.h>

/* #include <rpcsvc/yp_prot.h> */
#define YP_TRUE	 	((long)1)	/* general purpose success code */
#define YP_FALSE 	((long)0)	/* general purpose failure code */

/* #include <rpcsvc/ypclnt.h> */
#define YPERR_DOMAIN	3		/* can't bind to a server for domain */
#define YPERR_YPERR 	6		/* some internal YP server or client error */
#define YPERR_YPBIND	10		/* can't communicate with ypbind */
#define YPERR_NODOM 	12		/* local domain name not set */

#ifndef NULL
#define NULL (void*)0
#endif


static char _yp_domain[256];

int
_yp_dobind(dom, ypdb)
	const char *dom;
	void **ypdb;
{
	return YPERR_YPBIND;
}

int
yp_bind(dom)
	const char     *dom;
{
	return _yp_dobind(dom, NULL);
}

void
yp_unbind(dom)
	const char     *dom;
{
}

int
yp_match(indomain, inmap, inkey, inkeylen, outval, outvallen)
	const char     *indomain;
	const char     *inmap;
	const char     *inkey;
	int             inkeylen;
	char          **outval;
	int            *outvallen;
{
	*outval = NULL;
	*outvallen = 0;

	return YPERR_DOMAIN;
}

int
yp_get_default_domain(domp)
	char **domp;
{
	*domp = NULL;
	if (_yp_domain[0] == '\0')
		if (getdomainname(_yp_domain, sizeof(_yp_domain)))
			return YPERR_NODOM;
	*domp = _yp_domain;
	return 0;
}

int
yp_first(indomain, inmap, outkey, outkeylen, outval, outvallen)
	const char     *indomain;
	const char     *inmap;
	char          **outkey;
	int            *outkeylen;
	char          **outval;
	int            *outvallen;
{

	*outkey = *outval = NULL;
	*outkeylen = *outvallen = 0;

	return YPERR_DOMAIN;
}

int
yp_next(indomain, inmap, inkey, inkeylen, outkey, outkeylen, outval, outvallen)
	const char     *indomain;
	const char     *inmap;
	const char     *inkey;
	int             inkeylen;
	char          **outkey;
	int            *outkeylen;
	char          **outval;
	int            *outvallen;
{
	*outkey = *outval = NULL;
	*outkeylen = *outvallen = 0;

	return YPERR_DOMAIN;
}

int
yp_all(indomain, inmap, incallback)
	const char     *indomain;
	const char     *inmap;
	void *incallback;
{
	return YPERR_DOMAIN;
}

int
yp_order(indomain, inmap, outorder)
	const char     *indomain;
	const char     *inmap;
	int            *outorder;
{
	return YPERR_DOMAIN;
}

int
yp_master(indomain, inmap, outname)
	const char     *indomain;
	const char     *inmap;
	char          **outname;
{
	return YPERR_DOMAIN;
}

int
yp_maplist(indomain, outmaplist)
	const char     *indomain;
	struct ypmaplist **outmaplist;
{
	return YPERR_DOMAIN;
}

char *
yperr_string(incode)
	int             incode;
{
	static char     err[80];

	if (incode == 0)
		return "Success";

	sprintf(err, "YP FAKE error %d\n", incode);
	return err;
}

int
ypprot_err(incode)
	unsigned int    incode;
{
	switch (incode) {
	case YP_TRUE:	/* success */
		return 0;
	case YP_FALSE:	/* failure */
		return YPERR_YPBIND;
	}
	return YPERR_YPERR;
}

int
_yp_check(dom)
	char          **dom;
{
	char           *unused;

	if (_yp_domain[0] == '\0')
		if (yp_get_default_domain(&unused))
			return 0;

	if (dom)
		*dom = _yp_domain;

	if (yp_bind(_yp_domain) == 0)
		return 1;
	return 0;
}
