/*	$OpenBSD: rcsnum.c,v 1.44 2007/05/26 00:36:04 krw Exp $	*/
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

#include <ctype.h>
#include <string.h>

#include "cvs.h"

static void	 rcsnum_setsize(RCSNUM *, u_int);
static char	*rcsnum_itoa(u_int16_t, char *, size_t);

int rcsnum_flags;

/*
 * rcsnum_alloc()
 *
 * Allocate an RCS number structure and return a pointer to it.
 */
RCSNUM *
rcsnum_alloc(void)
{
	RCSNUM *rnp;

	rnp = xmalloc(sizeof(*rnp));
	rnp->rn_len = 0;
	rnp->rn_id = NULL;

	return (rnp);
}

/*
 * rcsnum_parse()
 *
 * Parse a string specifying an RCS number and return the corresponding RCSNUM.
 */
RCSNUM *
rcsnum_parse(const char *str)
{
	char *ep;
	RCSNUM *num;

	num = rcsnum_alloc();
	if (rcsnum_aton(str, &ep, num) < 0 || *ep != '\0') {
		rcsnum_free(num);
		num = NULL;
		if (*ep != '\0')
			rcs_errno = RCS_ERR_BADNUM;
	}

	return (num);
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
		xfree(rn->rn_id);
	xfree(rn);
}

/*
 * rcsnum_tostr()
 *
 * Format the RCS number <nump> into a human-readable dot-separated
 * representation and store the resulting string in <buf>, which is of size
 * <blen>.
 * Returns a pointer to the start of <buf>.  On failure <buf> is set to
 * an empty string.
 */
char *
rcsnum_tostr(const RCSNUM *nump, char *buf, size_t blen)
{
	u_int i;
	char tmp[8];

	if (nump == NULL || nump->rn_len == 0) {
		buf[0] = '\0';
		return (buf);
	}

	if (strlcpy(buf, rcsnum_itoa(nump->rn_id[0], buf, blen), blen) >= blen)
		fatal("rcsnum_tostr: truncation");
	for (i = 1; i < nump->rn_len; i++) {
		const char *str;

		str = rcsnum_itoa(nump->rn_id[i], tmp, sizeof(tmp));
		if (strlcat(buf, ".", blen) >= blen ||
		    strlcat(buf, str, blen) >= blen)
			fatal("rcsnum_tostr: truncation");
	}

	return (buf);
}

static char *
rcsnum_itoa(u_int16_t num, char *buf, size_t len)
{
	u_int16_t i;
	char *p;

	if (num == 0)
		return "0";

	p = buf + len - 1;
	i = num;
	bzero(buf, len);
	while (i) {
		*--p = '0' + (i % 10);
		i  /= 10;
	}
	return (p);
}

/*
 * rcsnum_cpy()
 *
 * Copy the number stored in <nsrc> in the destination <ndst> up to <depth>
 * numbers deep.  If <depth> is 0, there is no depth limit.
 */
void
rcsnum_cpy(const RCSNUM *nsrc, RCSNUM *ndst, u_int depth)
{
	u_int len;

	len = nsrc->rn_len;
	if (depth != 0 && len > depth)
		len = depth;

	rcsnum_setsize(ndst, len);
	/* Overflow checked in rcsnum_setsize(). */
	(void)memcpy(ndst->rn_id, nsrc->rn_id,
	    len * sizeof(*(nsrc->rn_id)));
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
rcsnum_cmp(RCSNUM *n1, RCSNUM *n2, u_int depth)
{
	int res;
	u_int i;
	size_t slen;

	if (!rcsnum_differ(n1, n2))
		return (0);

	slen = MIN(n1->rn_len, n2->rn_len);
	if (depth != 0 && slen > depth)
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
	u_int32_t val;
	const char *sp;
	void *tmp;
	char *s;

	if (nump->rn_id == NULL)
		nump->rn_id = xmalloc(sizeof(*(nump->rn_id)));

	nump->rn_len = 0;
	nump->rn_id[0] = 0;

	for (sp = str;; sp++) {
		if (!isdigit(*sp) && (*sp != '.'))
			break;

		if (*sp == '.') {
			if (nump->rn_len >= RCSNUM_MAXLEN - 1) {
				rcs_errno = RCS_ERR_BADNUM;
				goto rcsnum_aton_failed;
			}

			nump->rn_len++;
			tmp = xrealloc(nump->rn_id,
			    nump->rn_len + 1, sizeof(*(nump->rn_id)));
			nump->rn_id = tmp;
			nump->rn_id[nump->rn_len] = 0;
			continue;
		}

		val = (nump->rn_id[nump->rn_len] * 10) + (*sp - '0');
		if (val > RCSNUM_MAXNUM)
			fatal("RCSNUM overflow!");

		nump->rn_id[nump->rn_len] = val;
	}

	if (ep != NULL)
		*(const char **)ep = sp;

	/*
	 * Handle "magic" RCS branch numbers.
	 *
	 * What are they?
	 *
	 * Magic branch numbers have an extra .0. at the second farmost
	 * rightside of the branch number, so instead of having an odd
	 * number of dot-separated decimals, it will have an even number.
	 *
	 * Now, according to all the documentation I've found on the net
	 * about this, cvs does this for "efficiency reasons", I'd like
	 * to hear one.
	 *
	 * We just make sure we remove the .0. from in the branch number.
	 *
	 * XXX - for compatibility reasons with GNU cvs we _need_
	 * to skip this part for the 'log' command, apparently it does
	 * show the magic branches for an unknown and probably
	 * completely insane and not understandable reason in that output.
	 *
	 */
	if (nump->rn_len > 2 && nump->rn_id[nump->rn_len - 1] == 0
	    && !(rcsnum_flags & RCSNUM_NO_MAGIC)) {
		/*
		 * Look for ".0.x" at the end of the branch number.
		 */
		if ((s = strrchr(str, '.')) != NULL) {
			s--;
			while (*s != '.')
				s--;

			/*
			 * If we have a "magic" branch, adjust it
			 * so the .0. is removed.
			 */
			if (!strncmp(s, RCS_MAGIC_BRANCH,
			    strlen(RCS_MAGIC_BRANCH))) {
				nump->rn_id[nump->rn_len - 1] =
				    nump->rn_id[nump->rn_len];
				nump->rn_len--;
			}
		}
	}

	/* We can't have a single-digit rcs number. */
	if (nump->rn_len == 0) {
		tmp = xrealloc(nump->rn_id,
		    nump->rn_len + 1, sizeof(*(nump->rn_id)));
		nump->rn_id = tmp;
		nump->rn_id[nump->rn_len + 1] = 0;
		nump->rn_len++;
	}

	nump->rn_len++;
	return (nump->rn_len);

rcsnum_aton_failed:
	nump->rn_len = 0;
	xfree(nump->rn_id);
	nump->rn_id = NULL;
	return (-1);
}

/*
 * rcsnum_inc()
 *
 * Increment the revision number specified in <num>.
 * Returns a pointer to the <num> on success, or NULL on failure.
 */
RCSNUM *
rcsnum_inc(RCSNUM *num)
{
	if (num->rn_id[num->rn_len - 1] == RCSNUM_MAXNUM)
		return (NULL);
	num->rn_id[num->rn_len - 1]++;
	return (num);
}

/*
 * rcsnum_dec()
 *
 * Decreases the revision number specified in <num>, if doing so will not
 * result in an ending value below 1. E.g. 4.2 will go to 4.1 but 4.1 will
 * be returned as 4.1.
 */
RCSNUM *
rcsnum_dec(RCSNUM *num)
{
	/* XXX - Is it an error for the number to be 0? */
	if (num->rn_id[num->rn_len - 1] <= 1)
		return (num);
	num->rn_id[num->rn_len - 1]--;
	return (num);
}

/*
 * rcsnum_revtobr()
 *
 * Retrieve the branch number associated with the revision number <num>.
 * If <num> is a branch revision, the returned value will be the same
 * number as the argument.
 */
RCSNUM *
rcsnum_revtobr(const RCSNUM *num)
{
	RCSNUM *brnum;

	if (num->rn_len < 2)
		return (NULL);

	brnum = rcsnum_alloc();
	rcsnum_cpy(num, brnum, 0);

	if (!RCSNUM_ISBRANCH(brnum))
		brnum->rn_len--;

	return (brnum);
}

/*
 * rcsnum_brtorev()
 *
 * Retrieve the initial revision number associated with the branch number <num>.
 * If <num> is a revision number, an error will be returned.
 */
RCSNUM *
rcsnum_brtorev(const RCSNUM *brnum)
{
	RCSNUM *num;

	if (!RCSNUM_ISBRANCH(brnum)) {
		return (NULL);
	}

	num = rcsnum_alloc();
	rcsnum_setsize(num, brnum->rn_len + 1);
	rcsnum_cpy(brnum, num, brnum->rn_len);
	num->rn_id[num->rn_len++] = 1;

	return (num);
}

static void
rcsnum_setsize(RCSNUM *num, u_int len)
{
	void *tmp;

	tmp = xrealloc(num->rn_id, len, sizeof(*(num->rn_id)));
	num->rn_id = tmp;
	num->rn_len = len;
}

int
rcsnum_differ(RCSNUM *r1, RCSNUM *r2)
{
	int i, len;

	if (r1->rn_len != r2->rn_len)
		return (1);

	len = MIN(r1->rn_len, r2->rn_len);
	for (i = 0; i < len; i++) {
		if (r1->rn_id[i] != r2->rn_id[i])
			return (1);
	}

	return (0);
}
