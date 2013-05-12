/*	$OpenBSD: devopen.c,v 1.1 2013/05/12 10:43:45 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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


#include <sys/param.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"
#include "netif.h"
#include "config.h"

/*
 * A replacement for libsa devopen() to recognize an Ethernet interface
 * name, followed by a digit and a colon, as an optional prefix.
 */
int
devopen(struct open_file *f, const char *fname, char **file)
{
	struct devsw *dp;
	const char *prefix;
	char *colon;

	prefix = NULL;
	*file = (char *)fname;

	colon = strchr(fname, ':');
	if (colon != NULL) {
		int i;

		for (i = 0; i < n_netif_drivers; i++) {
			struct netif_driver *drv = netif_drivers[i];

			/* out of lazyness, only allow one digit for the
			   interface number */
			if (colon != fname + strlen(drv->netif_bname) + 1)
				continue;
			if (bcmp(fname, drv->netif_bname,
			    strlen(drv->netif_bname)) != 0)
				continue;

			prefix = fname;
			*file = colon + 1;
		}
	}

	dp = &devsw[0];
	f->f_dev = dp;
	return (*dp->dv_open)(f, prefix);
}
