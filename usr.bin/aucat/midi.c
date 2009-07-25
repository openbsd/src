/*	$OpenBSD: midi.c,v 1.1 2009/07/25 08:44:27 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
/*
 * TODO
 *
 * use abuf->duplex to implement bidirectionnal sockets
 * that don't receive what they send
 *
 * use shadow variables in the midi merger
 *
 * make output and input identical when only one
 * input is used (fix running status)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "abuf.h"
#include "aproc.h"
#include "midi.h"

/*
 * input data rate is XFER / TIMO (in bytes per microsecond),
 * it must be slightly larger than the MIDI standard 3125 bytes/s
 */ 
#define MIDITHRU_XFER 340
#define MIDITHRU_TIMO 100000

struct aproc *thrubox = NULL;

unsigned voice_len[] = { 3, 3, 3, 3, 2, 2, 3 };
unsigned common_len[] = { 0, 2, 3, 2, 0, 0, 1, 1 };

void
thru_flush(struct aproc *p, struct abuf *ibuf, struct abuf *obuf)
{
	unsigned ocount, itodo;
	unsigned char *odata, *idata;

	itodo = ibuf->mused;
	idata = ibuf->mdata;
	DPRINTFN(4, "thru_flush: mused = %u\n", itodo);
	while (itodo > 0) {
		if (!ABUF_WOK(obuf)) {
			abuf_rdiscard(obuf, obuf->used);
			DPRINTFN(2, "thru_flush: discarded %u\n", obuf->used);
			if (p->u.thru.owner == ibuf)
				p->u.thru.owner = NULL;
			return;
		}
		odata = abuf_wgetblk(obuf, &ocount, 0);
		if (ocount > itodo)
			ocount = itodo;
		memcpy(odata, idata, ocount);
		abuf_wcommit(obuf, ocount);
		itodo -= ocount;
		idata += ocount;
	}
	ibuf->mused = 0;
	p->u.thru.owner = ibuf;
}

void
thru_rt(struct aproc *p, struct abuf *ibuf, struct abuf *obuf, unsigned c)
{
	unsigned ocount;
	unsigned char *odata;

	DPRINTFN(4, "thru_rt:\n");
	if (!ABUF_WOK(obuf)) {
		DPRINTFN(2, "thru_rt: discarded %u\n", obuf->used);
		abuf_rdiscard(obuf, obuf->used);
		if (p->u.thru.owner == ibuf)
			p->u.thru.owner = NULL;
	}
	odata = abuf_wgetblk(obuf, &ocount, 0);
	odata[0] = c;
	abuf_wcommit(obuf, 1);
}


void
thru_bcopy(struct aproc *p, struct abuf *ibuf, struct abuf *obuf, unsigned todo)
{
	unsigned char *idata;
	unsigned c, icount, ioffs;

	idata = NULL;
	icount = ioffs = 0;
	for (;;) {
		if (icount == 0) {
			if (todo == 0)
				break;
			idata = abuf_rgetblk(ibuf, &icount, ioffs);
			if (icount > todo)
				icount = todo;
			if (icount == 0)
				break;
			todo -= icount;
			ioffs += icount;
		}
		c = *idata++;
		icount--;
		if (c < 0x80) {
			if (ibuf->mindex == 0 && ibuf->mstatus) {
				ibuf->mdata[ibuf->mused++] = ibuf->mstatus;
				ibuf->mindex++;
			}
			ibuf->mdata[ibuf->mused++] = c;
			ibuf->mindex++;
			if (ibuf->mindex == ibuf->mlen) {
				thru_flush(p, ibuf, obuf);
				if (ibuf->mstatus >= 0xf0)
					ibuf->mstatus = 0;
				ibuf->mindex = 0;
			}
			if (ibuf->mused == MDATA_NMAX) {
				if (ibuf->mused == ibuf->mindex ||
				    p->u.thru.owner == ibuf)
					thru_flush(p, ibuf, obuf);
				else
					ibuf->mused = 0;
			}
		} else if (c < 0xf8) {
			if (ibuf->mused == ibuf->mindex ||
			    p->u.thru.owner == ibuf) {
				thru_flush(p, ibuf, obuf);
			} else
				ibuf->mused = 0;
			ibuf->mdata[0] = c;
			ibuf->mused = 1;
			ibuf->mlen = (c >= 0xf0) ? 
			    common_len[c & 7] :
			    voice_len[(c >> 4) & 7];
			if (ibuf->mlen == 1) {
				thru_flush(p, ibuf, obuf);
				ibuf->mindex = 0;
				ibuf->mstatus = 0;
				ibuf->mlen = 0;
			} else { 
				ibuf->mstatus = c;
				ibuf->mindex = 1;
			}
		} else {
			thru_rt(p, ibuf, obuf, c);
		}
	}
}

int
thru_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *i, *inext;
	unsigned todo;

	DPRINTFN(3, "thru_in: %s\n", p->name);

	if (!ABUF_ROK(ibuf))
		return 0;
	if (ibuf->mtickets == 0) {
		DPRINTFN(2, "thru_in: out of tickets\n");
		return 0;
	}
	todo = ibuf->used;
	if (todo > ibuf->mtickets)
		todo = ibuf->mtickets;
	ibuf->mtickets -= todo;
	for (i = LIST_FIRST(&p->obuflist); i != NULL; i = inext) {
		inext = LIST_NEXT(i, oent);
		if (ibuf->duplex == i)
			continue;
		thru_bcopy(p, ibuf, i, todo);
		(void)abuf_flush(i);
	}
	abuf_rdiscard(ibuf, todo);
	return 1;
}

int
thru_out(struct aproc *p, struct abuf *obuf)
{
	return 0;
}

void
thru_eof(struct aproc *p, struct abuf *ibuf)
{
	DPRINTF("thru_eof: %s: eof\n", p->name);
}

void
thru_hup(struct aproc *p, struct abuf *obuf)
{
	DPRINTF("thru_hup: %s: detached\n", p->name);
}

void
thru_newin(struct aproc *p, struct abuf *ibuf)
{
	ibuf->mused = 0;
	ibuf->mlen = 0;
	ibuf->mindex = 0;
	ibuf->mstatus = 0;
	ibuf->mtickets = MIDITHRU_XFER;
}

void
thru_done(struct aproc *p)
{
	timo_del(&p->u.thru.timo);
}

struct aproc_ops thru_ops = {
	"thru",
	thru_in,
	thru_out,
	thru_eof,
	thru_hup,
	thru_newin,
	NULL, /* newout */
	NULL, /* ipos */
	NULL, /* opos */
	thru_done
};

void
thru_cb(void *addr)
{
	struct aproc *p = (struct aproc *)addr;
	struct abuf *i, *inext;
	unsigned tickets;

	timo_add(&p->u.thru.timo, MIDITHRU_TIMO);
	
	for (i = LIST_FIRST(&p->ibuflist); i != NULL; i = inext) {
		inext = LIST_NEXT(i, ient);
		tickets = i->mtickets;
		i->mtickets = MIDITHRU_XFER;
		if (tickets == 0)
			abuf_run(i);
	}
}

struct aproc *
thru_new(char *name)
{
	struct aproc *p;

	p = aproc_new(&thru_ops, name);
	p->u.thru.owner = NULL;
	timo_set(&p->u.thru.timo, thru_cb, p);
	timo_add(&p->u.thru.timo, MIDITHRU_TIMO);
	return p;
}

