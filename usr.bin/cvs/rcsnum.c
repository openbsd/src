/*	$OpenBSD: rcsnum.c,v 1.5 2004/12/10 19:19:11 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rcs.h"
#include "log.h"



/*
 * rcsnum_alloc()
 *
 * Allocate an RCS number structure.
 */
RCSNUM*
rcsnum_alloc(void)
{
	RCSNUM *rnp;

	rnp = (RCSNUM *)malloc(sizeof(*rnp));
	if (rnp == NULL) {
		cvs_log(LP_ERR, "failed to allocate RCS number");
		return (NULL);
	}
	rnp->rn_len = 0;
	rnp->rn_id = NULL;

	return (rnp);
}


/*
 * rcsnum_free()
 *
 * Free an RCSNUM structure previously allocated with rcsnum_alloc().
 */

void
rcsnum_free(RCSNUM *rn)
{
	if (rn->rn_id != NULL)
		free(rn->rn_id);
	free(rn);
}


/*
 * rcsnum_tostr()
 * Returns a pointer to the start of <buf> on success, or NULL on failure.
 */
char*
rcsnum_tostr(const RCSNUM *nump, char *buf, size_t blen)
{
	u_int i;
	char tmp[8];

	if (nump->rn_len == 0) {
		buf[0] = '\0';
		return (buf);
	}

	snprintf(buf, blen, "%u", nump->rn_id[0]);
	for (i = 1; i < nump->rn_len; i++) {
		snprintf(tmp, sizeof(tmp), ".%u", nump->rn_id[i]);
		strlcat(buf, tmp, blen);
	}

	return (buf);
}


/*
 * rcsnum_cpy()
 *
 * Copy the number stored in <nsrc> in the destination <ndst> up to <depth>
 * numbers deep.
 * Returns 0 on success, or -1 on failure.
 */
int
rcsnum_cpy(const RCSNUM *nsrc, RCSNUM *ndst, u_int depth)
{
	u_int len;
	size_t sz;
	void *tmp;

	len = nsrc->rn_len;
	if ((depth != 0) && (len > depth))
		len = depth;
	sz = len * sizeof(u_int16_t);

	tmp = realloc(ndst->rn_id, sz);
	if (tmp == NULL) {
		cvs_log(LP_ERR, "failed to reallocate RCSNUM");
		return (-1);
	}

	ndst->rn_id = (u_int16_t *)tmp;
	ndst->rn_len = len;
	memcpy(ndst->rn_id, nsrc->rn_id, sz);
	return (0);
}


/*
 * rcsnum_cmp()
 *
 * Compare the two numbers <n1> and <n2>. Returns -1 if <n1> is larger than
 * <n2>, 0 if they are both the same, and 1 if <n2> is larger than <n1>.
 * The <depth> argument specifies how many numbers deep should be checked for
 * the result.  A value of 0 means that the depth will be the minimum of the
 * two numbers.
 */
int
rcsnum_cmp(const RCSNUM *n1, const RCSNUM *n2, u_int depth)
{
	int res;
	u_int i;
	size_t slen;

	slen = MIN(n1->rn_len, n2->rn_len);
	if ((depth != 0) && (slen > depth))
		slen = depth;

	for (i = 0; i < slen; i++) {
		res = n1->rn_id[i] - n2->rn_id[i];
		if (res < 0)
			return (1);
		else if (res > 0)
			return (-1);
	}

	if (n1->rn_len > n2->rn_len)
		return (-1);
	else if (n2->rn_len > n1->rn_len)
		return (1);

	return (0);
}


/*
 * rcsnum_aton()
 *
 * Translate the string <str> containing a sequence of digits and periods into
 * its binary representation, which is stored in <nump>.  The address of the
 * first byte not part of the number is stored in <ep> on return, if it is not
 * NULL.
 * Returns 0 on success, or -1 on failure.
 */
int
rcsnum_aton(const char *str, char **ep, RCSNUM *nump)
{
	const char *sp;
	void *tmp;

	if (nump->rn_id == NULL) {
		nump->rn_id = (u_int16_t *)malloc(sizeof(u_int16_t));
		if (nump->rn_id == NULL)
			return (-1);
	}

	nump->rn_len = 0;
	nump->rn_id[nump->rn_len] = 0;

	for (sp = str;; sp++) {
		if (!isdigit(*sp) && (*sp != '.'))
			break;

		if (*sp == '.') {
			nump->rn_len++;
			tmp = realloc(nump->rn_id,
			    (nump->rn_len + 1) * sizeof(u_int16_t));
			if (tmp == NULL) {
				free(nump->rn_id);
				nump->rn_len = 0;
				nump->rn_id = NULL;
				return (-1);
			}
			nump->rn_id = (u_int16_t *)tmp;
			nump->rn_id[nump->rn_len] = 0;
			continue;
		}

		nump->rn_id[nump->rn_len] *= 10;
		nump->rn_id[nump->rn_len] += *sp - 0x30;
	}

	if (ep != NULL)
		*(const char **)ep = sp;

	nump->rn_len++;
	return (nump->rn_len);
}
