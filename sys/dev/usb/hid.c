/*	$OpenBSD: hid.c,v 1.19 2007/09/09 01:00:35 fgsch Exp $ */
/*	$NetBSD: hid.c,v 1.23 2002/07/11 21:14:25 augustss Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/hid.c,v 1.11 1999/11/17 22:33:39 n_hibma Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/hid.h>

#ifdef UHIDEV_DEBUG
#define DPRINTF(x)	do { if (uhidevdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (uhidevdebug>(n)) printf x; } while (0)
extern int uhidevdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

void hid_clear_local(struct hid_item *);

#define MAXUSAGE 256
struct hid_data {
	u_char *start;
	u_char *end;
	u_char *p;
	struct hid_item cur;
	int32_t usages[MAXUSAGE];
	int nu;
	int minset;
	int multi;
	int multimax;
	enum hid_kind kind;
};

void
hid_clear_local(struct hid_item *c)
{

	DPRINTFN(5,("hid_clear_local\n"));
	c->usage = 0;
	c->usage_minimum = 0;
	c->usage_maximum = 0;
	c->designator_index = 0;
	c->designator_minimum = 0;
	c->designator_maximum = 0;
	c->string_index = 0;
	c->string_minimum = 0;
	c->string_maximum = 0;
	c->set_delimiter = 0;
}

struct hid_data *
hid_start_parse(void *d, int len, enum hid_kind kind)
{
	struct hid_data *s;

	s = malloc(sizeof *s, M_TEMP, M_WAITOK|M_ZERO);
	if (s == NULL)
		panic("hid_start_parse");

	s->start = s->p = d;
	s->end = (char *)d + len;
	s->kind = kind;
	return (s);
}

void
hid_end_parse(struct hid_data *s)
{

	while (s->cur.next != NULL) {
		struct hid_item *hi = s->cur.next->next;
		free(s->cur.next, M_TEMP);
		s->cur.next = hi;
	}
	free(s, M_TEMP);
}

int
hid_get_item(struct hid_data *s, struct hid_item *h)
{
	struct hid_item *c = &s->cur;
	unsigned int bTag, bType, bSize;
	u_int32_t oldpos;
	u_char *data;
	int32_t dval;
	u_char *p;
	struct hid_item *hi;
	int i;
	enum hid_kind retkind;

 top:
	DPRINTFN(5,("hid_get_item: multi=%d multimax=%d\n",
		    s->multi, s->multimax));
	if (s->multimax != 0) {
		if (s->multi < s->multimax) {
			c->usage = s->usages[min(s->multi, s->nu-1)];
			s->multi++;
			*h = *c;
			c->loc.pos += c->loc.size;
			h->next = NULL;
			DPRINTFN(5,("return multi\n"));
			return (1);
		} else {
			c->loc.count = s->multimax;
			s->multimax = 0;
			s->nu = 0;
			hid_clear_local(c);
		}
	}
	for (;;) {
		p = s->p;
		if (p >= s->end)
			return (0);

		bSize = *p++;
		if (bSize == 0xfe) {
			/* long item */
			bSize = *p++;
			bSize |= *p++ << 8;
			bTag = *p++;
			data = p;
			p += bSize;
			bType = 0xff; /* XXX what should it be */
		} else {
			/* short item */
			bTag = bSize >> 4;
			bType = (bSize >> 2) & 3;
			bSize &= 3;
			if (bSize == 3) bSize = 4;
			data = p;
			p += bSize;
		}
		s->p = p;
		switch(bSize) {
		case 0:
			dval = 0;
			break;
		case 1:
			dval = /*(int8_t)*/ *data++;
			break;
		case 2:
			dval = *data++;
			dval |= *data++ << 8;
			dval = /*(int16_t)*/ dval;
			break;
		case 4:
			dval = *data++;
			dval |= *data++ << 8;
			dval |= *data++ << 16;
			dval |= *data++ << 24;
			break;
		default:
			printf("BAD LENGTH %d\n", bSize);
			continue;
		}

		DPRINTFN(5,("hid_get_item: bType=%d bTag=%d dval=%d\n",
			 bType, bTag, dval));
		switch (bType) {
		case 0:			/* Main */
			switch (bTag) {
			case 8:		/* Input */
				retkind = hid_input;
			ret:
				if (s->kind != retkind) {
					s->minset = 0;
					s->nu = 0;
					hid_clear_local(c);
					continue;
				}
				c->kind = retkind;
				c->flags = dval;
				if (c->flags & HIO_VARIABLE) {
					s->multimax = c->loc.count;
					s->multi = 0;
					c->loc.count = 1;
					if (s->minset) {
						for (i = c->usage_minimum;
						     i <= c->usage_maximum;
						     i++) {
							s->usages[s->nu] = i;
							if (s->nu < MAXUSAGE-1)
								s->nu++;
						}
						s->minset = 0;
					}
					goto top;
				} else {
					c->usage = c->_usage_page; /* XXX */
					*h = *c;
					h->next = NULL;
					c->loc.pos +=
					    c->loc.size * c->loc.count;
					s->minset = 0;
					s->nu = 0;
					hid_clear_local(c);
					return (1);
				}
			case 9:		/* Output */
				retkind = hid_output;
				goto ret;
			case 10:	/* Collection */
				c->kind = hid_collection;
				c->collection = dval;
				c->collevel++;
				*h = *c;
				hid_clear_local(c);
				s->nu = 0;
				return (1);
			case 11:	/* Feature */
				retkind = hid_feature;
				goto ret;
			case 12:	/* End collection */
				c->kind = hid_endcollection;
				c->collevel--;
				*h = *c;
				s->nu = 0;
				return (1);
			default:
				printf("Main bTag=%d\n", bTag);
				break;
			}
			break;
		case 1:		/* Global */
			switch (bTag) {
			case 0:
				c->_usage_page = dval << 16;
				break;
			case 1:
				c->logical_minimum = dval;
				break;
			case 2:
				c->logical_maximum = dval;
				break;
			case 3:
				c->physical_maximum = dval;
				break;
			case 4:
				c->physical_maximum = dval;
				break;
			case 5:
				c->unit_exponent = dval;
				break;
			case 6:
				c->unit = dval;
				break;
			case 7:
				c->loc.size = dval;
				break;
			case 8:
				c->report_ID = dval;
				c->loc.pos = 0;
				break;
			case 9:
				c->loc.count = dval;
				break;
			case 10: /* Push */
				hi = malloc(sizeof *hi, M_TEMP, M_WAITOK);
				*hi = s->cur;
				c->next = hi;
				break;
			case 11: /* Pop */
				hi = c->next;
				oldpos = c->loc.pos;
				s->cur = *hi;
				c->loc.pos = oldpos;
				free(hi, M_TEMP);
				break;
			default:
				printf("Global bTag=%d\n", bTag);
				break;
			}
			break;
		case 2:		/* Local */
			switch (bTag) {
			case 0:
				if (bSize == 1)
					dval = c->_usage_page | (dval&0xff);
				else if (bSize == 2)
					dval = c->_usage_page | (dval&0xffff);
				c->usage = dval;
				if (s->nu < MAXUSAGE)
					s->usages[s->nu++] = dval;
				/* else XXX */
				break;
			case 1:
				s->minset = 1;
				if (bSize == 1)
					dval = c->_usage_page | (dval&0xff);
				else if (bSize == 2)
					dval = c->_usage_page | (dval&0xffff);
				c->usage_minimum = dval;
				break;
			case 2:
				if (bSize == 1)
					dval = c->_usage_page | (dval&0xff);
				else if (bSize == 2)
					dval = c->_usage_page | (dval&0xffff);
				c->usage_maximum = dval;
				break;
			case 3:
				c->designator_index = dval;
				break;
			case 4:
				c->designator_minimum = dval;
				break;
			case 5:
				c->designator_maximum = dval;
				break;
			case 7:
				c->string_index = dval;
				break;
			case 8:
				c->string_minimum = dval;
				break;
			case 9:
				c->string_maximum = dval;
				break;
			case 10:
				c->set_delimiter = dval;
				break;
			default:
				printf("Local bTag=%d\n", bTag);
				break;
			}
			break;
		default:
			printf("default bType=%d\n", bType);
			break;
		}
	}
}

int
hid_report_size(void *buf, int len, enum hid_kind k, u_int8_t id)
{
	struct hid_data *d;
	struct hid_item h;
	int lo, hi;

	h.report_ID = 0;
	lo = hi = -1;
	DPRINTFN(2,("hid_report_size: kind=%d id=%d\n", k, id));
	for (d = hid_start_parse(buf, len, k); hid_get_item(d, &h); ) {
		DPRINTFN(2,("hid_report_size: item kind=%d id=%d pos=%d "
			    "size=%d count=%d\n",
			    h.kind, h.report_ID, h.loc.pos, h.loc.size,
			    h.loc.count));
		if (h.report_ID == id && h.kind == k) {
			if (lo < 0) {
				lo = h.loc.pos;
#ifdef DIAGNOSTIC
				if (lo != 0) {
					printf("hid_report_size: lo != 0\n");
				}
#endif
			}
			hi = h.loc.pos + h.loc.size * h.loc.count;
			DPRINTFN(2,("hid_report_size: lo=%d hi=%d\n", lo, hi));
		}
	}
	hid_end_parse(d);
	return ((hi - lo + 7) / 8);
}

int
hid_locate(void *desc, int size, u_int32_t u, u_int8_t id, enum hid_kind k,
	   struct hid_location *loc, u_int32_t *flags)
{
	struct hid_data *d;
	struct hid_item h;

	h.report_ID = 0;
	DPRINTFN(5,("hid_locate: enter usage=0x%x kind=%d id=%d\n", u, k, id));
	for (d = hid_start_parse(desc, size, k); hid_get_item(d, &h); ) {
		DPRINTFN(5,("hid_locate: usage=0x%x kind=%d id=%d flags=0x%x\n",
			    h.usage, h.kind, h.report_ID, h.flags));
		if (h.kind == k && !(h.flags & HIO_CONST) &&
		    h.usage == u && h.report_ID == id) {
			if (loc != NULL)
				*loc = h.loc;
			if (flags != NULL)
				*flags = h.flags;
			hid_end_parse(d);
			return (1);
		}
	}
	hid_end_parse(d);
	loc->size = 0;
	return (0);
}

u_long
hid_get_data(u_char *buf, struct hid_location *loc)
{
	u_int hpos = loc->pos;
	u_int hsize = loc->size;
	u_int32_t data;
	int i, s;

	DPRINTFN(10, ("hid_get_data: loc %d/%d\n", hpos, hsize));

	if (hsize == 0)
		return (0);

	data = 0;
	s = hpos / 8;
	for (i = hpos; i < hpos+hsize; i += 8)
		data |= buf[i / 8] << ((i / 8 - s) * 8);
	data >>= hpos % 8;
	data &= (1 << hsize) - 1;
	hsize = 32 - hsize;
	/* Sign extend */
	data = ((int32_t)data << hsize) >> hsize;
	DPRINTFN(10,("hid_get_data: loc %d/%d = %lu\n",
		    loc->pos, loc->size, (long)data));
	return (data);
}

int
hid_is_collection(void *desc, int size, u_int8_t id, u_int32_t usage)
{
	struct hid_data *hd;
	struct hid_item hi;
	u_int32_t coll_usage = ~0;

	hd = hid_start_parse(desc, size, hid_none);
	if (hd == NULL)
		return (0);

	DPRINTFN(2,("hid_is_collection: id=%d usage=0x%x\n", id, usage));
	while (hid_get_item(hd, &hi)) {
		DPRINTFN(2,("hid_is_collection: kind=%d id=%d usage=0x%x"
			    "(0x%x)\n",
			    hi.kind, hi.report_ID, hi.usage, coll_usage));
		if (hi.kind == hid_collection &&
		    hi.collection == HCOLL_APPLICATION)
			coll_usage = hi.usage;
		if (hi.kind == hid_endcollection &&
		    coll_usage == usage &&
		    hi.report_ID == id) {
			DPRINTFN(2,("hid_is_collection: found\n"));
			hid_end_parse(hd);
			return (1);
		}
	}
	DPRINTFN(2,("hid_is_collection: not found\n"));
	hid_end_parse(hd);
	return (0);
}
