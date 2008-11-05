/*	$OpenBSD: dns.c,v 1.2 2008/11/05 12:14:45 sobrado Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <err.h>
#include <event.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "smtpd.h"

struct mxrecord {
	char hostname[MAXHOSTNAMELEN];
	u_int16_t priority;
};

static void mxsort(struct mxrecord *, size_t);
size_t getmxbyname(char *, char ***);


/* bubble sort MX records by priority */
static void
mxsort(struct mxrecord *array, size_t len)
{
	u_int32_t i;
	u_int32_t j;
	struct mxrecord store;

	for (i = j = 0; i < len - 1; ++i) {
		for (j = i + 1; j < len; ++j) {
			if (array[i].priority > array[j].priority) {
				store = array[i];
				array[i] = array[j];
				array[j] = store;
			}
		}
	}
}

size_t
getmxbyname(char *name, char ***result)
{
	union {
		u_int8_t bytes[PACKETSZ];
		HEADER header;
	} answer;
	u_int32_t i, j;
	int ret;
	u_int8_t *sp;
	u_int8_t *endp;
	u_int8_t *ptr;
	u_int16_t qdcount;
	u_int8_t expbuf[PACKETSZ];
	u_int16_t type;
	u_int16_t n;
	u_int16_t priority, tprio;
	size_t mxnb;
	struct mxrecord mxarray[MXARRAYSIZE];
	size_t chunklen;

	ret = res_query(name, C_IN, T_MX, (u_int8_t *)&answer.bytes,
		sizeof answer);
	if (ret == -1)
		return 0;

	/* sp stores start of dns packet,
	 * endp stores end of dns packet,
	 */
	sp = (u_int8_t *)&answer.bytes;
	endp = sp + ret;

	/* skip header */
	ptr = sp + HFIXEDSZ;

	for (qdcount = ntohs(answer.header.qdcount);
	     qdcount--;
	     ptr += ret + QFIXEDSZ) {
		ret = dn_skipname(ptr, endp);
		if (ret == -1)
			return 0;
	}

	mxnb = 0;
	for (; ptr < endp;) {
		memset(expbuf, 0, sizeof expbuf);
		ret = dn_expand(sp, endp, ptr, expbuf, sizeof expbuf);
		if (ret == -1)
			break;
		ptr += ret;

		GETSHORT(type, ptr);
		ptr += sizeof(u_int16_t) + sizeof(u_int32_t);
		GETSHORT(n, ptr);

		if (type != T_MX) {
			ptr += n;
			continue;
		}

		GETSHORT(priority, ptr);
		ret = dn_expand(sp, endp, ptr, expbuf, sizeof expbuf);
		if (ret == -1)
			return 0;
		ptr += ret;

		if (mxnb < sizeof(mxarray) / sizeof(struct mxrecord)) {
			if (strlcpy(mxarray[mxnb].hostname, expbuf,
				MAXHOSTNAMELEN) >= MAXHOSTNAMELEN)
				return 0;
			mxarray[mxnb].priority = priority;
			if (tprio < priority)
				tprio = priority;
		}
		else {
			for (i = j = 0;
				i < sizeof(mxarray) / sizeof(struct mxrecord);
				++i) {
				if (tprio < mxarray[i].priority) {
					tprio = mxarray[i].priority;
					j = i;
				}
			}

			if (mxarray[j].priority > priority) {
				if (strlcpy(mxarray[j].hostname, expbuf,
					MAXHOSTNAMELEN) >= MAXHOSTNAMELEN)
					return 0;
				mxarray[j].priority = priority;
			}
		}
		++mxnb;
	}

	if (mxnb == 0)
		return 0;

	if (mxnb > sizeof(mxarray) / sizeof(struct mxrecord))
		mxnb = sizeof(mxarray) / sizeof(struct mxrecord);

	/* Rearrange MX records by priority */
	mxsort((struct mxrecord *)&mxarray, mxnb);

	chunklen = 0;
	for (i = 0; i < mxnb; ++i)
		chunklen += strlen(mxarray[i].hostname) + 1;
	chunklen += ((mxnb + 1) * sizeof(char *));

	*result = calloc(1, chunklen);
	if (*result == NULL) {
		err(1, "calloc");
	}

	ptr = (u_int8_t *)*result + (mxnb + 1) * sizeof(char *);
	for (i = 0; i < mxnb; ++i) {
		strlcpy(ptr, mxarray[i].hostname, MAXHOSTNAMELEN);
		(*result)[i] = ptr;
		ptr += strlen(mxarray[i].hostname) + 1;
	}
	(*result)[i] = NULL;

	return mxnb;
}
