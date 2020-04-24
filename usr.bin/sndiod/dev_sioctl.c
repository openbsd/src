/*	$OpenBSD: dev_sioctl.c,v 1.5 2020/04/24 11:33:28 ratchov Exp $	*/
/*
 * Copyright (c) 2014-2020 Alexandre Ratchov <alex@caoua.org>
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
#include <sys/time.h>
#include <sys/types.h>

#include <poll.h>
#include <sndio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "abuf.h"
#include "defs.h"
#include "dev.h"
#include "dsp.h"
#include "file.h"
#include "dev_sioctl.h"
#include "utils.h"

void dev_sioctl_ondesc(void *, struct sioctl_desc *, int);
void dev_sioctl_onval(void *, unsigned int, unsigned int);
int dev_sioctl_pollfd(void *, struct pollfd *);
int dev_sioctl_revents(void *, struct pollfd *);
void dev_sioctl_in(void *);
void dev_sioctl_out(void *);
void dev_sioctl_hup(void *);

struct fileops dev_sioctl_ops = {
	"sioctl",
	dev_sioctl_pollfd,
	dev_sioctl_revents,
	dev_sioctl_in,
	dev_sioctl_out,
	dev_sioctl_hup
};

void
dev_sioctl_ondesc(void *arg, struct sioctl_desc *desc, int val)
{
#define GROUP_PREFIX		"hw"
	char group_buf[CTL_NAMEMAX], *group;
	struct dev *d = arg;
	int addr;

	if (desc == NULL) {
		dev_ctlsync(d);
		return;
	}

	addr = CTLADDR_END + desc->addr;
	dev_rmctl(d, addr);

	/*
	 * prefix group names we use (currently "app") with "hw/"
	 * to ensure that all controls have unique names when multiple
	 * sndiod's are chained
	 */
	if (strcmp(desc->group, "app") == 0) {
		group = group_buf;
		if (snprintf(group_buf, CTL_NAMEMAX, GROUP_PREFIX "/%s",
		    desc->group) >= CTL_NAMEMAX)
			return;
	} else
		group = desc->group;

	dev_addctl(d, group, desc->type, addr,
	    desc->node0.name, desc->node0.unit, desc->func,
	    desc->node1.name, desc->node1.unit, desc->maxval, val);
}

void
dev_sioctl_onval(void *arg, unsigned int addr, unsigned int val)
{
	struct dev *d = arg;
	struct ctl *c;

	addr += CTLADDR_END;

	dev_log(d);
	log_puts(": onctl: addr = ");
	log_putu(addr);
	log_puts(", val = ");
	log_putu(val);
	log_puts("\n");

	for (c = d->ctl_list; c != NULL; c = c->next) {
		if (c->addr != addr)
			continue;
		ctl_log(c);
		log_puts(": new value -> ");
		log_putu(val);
		log_puts("\n");
		c->val_mask = ~0U;
		c->curval = val;
	}
}

/*
 * open the control device.
 */
void
dev_sioctl_open(struct dev *d)
{
	if (d->sioctl.hdl == NULL) {
		/*
		 * At this point there are clients, for instance if we're
		 * called by dev_reopen() but the control device couldn't
		 * be opened. In this case controls have changed (thoseof
		 * old device are just removed) so we need to notify clients.
		 */
		dev_ctlsync(d);
		return;
	}
	sioctl_ondesc(d->sioctl.hdl, dev_sioctl_ondesc, d);
	sioctl_onval(d->sioctl.hdl, dev_sioctl_onval, d);
}

/*
 * close the control device.
 */
void
dev_sioctl_close(struct dev *d)
{
	struct ctl *c, **pc;

	/* remove controls */
	pc = &d->ctl_list;
	while ((c = *pc) != NULL) {
		if (c->addr >= CTLADDR_END) {
			c->refs_mask &= ~CTL_DEVMASK;
			if (c->refs_mask == 0) {
				*pc = c->next;
				xfree(c);
				continue;
			}
			c->type = CTL_NONE;
			c->desc_mask = ~0;
		}
		pc = &c->next;
	}
	dev_ctlsync(d);
}

int
dev_sioctl_pollfd(void *arg, struct pollfd *pfd)
{
	struct dev *d = arg;
	struct ctl *c;
	int events = 0;

	for (c = d->ctl_list; c != NULL; c = c->next) {
		if (c->dirty)
			events |= POLLOUT;
	}
	return sioctl_pollfd(d->sioctl.hdl, pfd, events);
}

int
dev_sioctl_revents(void *arg, struct pollfd *pfd)
{
	struct dev *d = arg;

	return sioctl_revents(d->sioctl.hdl, pfd);
}

void
dev_sioctl_in(void *arg)
{
}

void
dev_sioctl_out(void *arg)
{
	struct dev *d = arg;
	struct ctl *c;
	int cnt;

	/*
	 * for each dirty ctl, call sioctl_setval() and dev_unref(). As
	 * dev_unref() may destroy the ctl_list, we must call it after
	 * we've finished iterating on it.
	 */
	cnt = 0;
	for (c = d->ctl_list; c != NULL; c = c->next) {
		if (!c->dirty)
			continue;
		if (!sioctl_setval(d->sioctl.hdl,
			c->addr - CTLADDR_END, c->curval)) {
			ctl_log(c);
			log_puts(": set failed\n");
			break;
		}
		if (log_level >= 2) {
			ctl_log(c);
			log_puts(": changed\n");
		}
		c->dirty = 0;
		cnt++;
	}
	while (cnt-- > 0)
		dev_unref(d);
}

void
dev_sioctl_hup(void *arg)
{
	struct dev *d = arg;

	dev_sioctl_close(d);
	file_del(d->sioctl.file);
	sioctl_close(d->sioctl.hdl);
	d->sioctl.hdl = NULL;
}
