/*	$OpenBSD: pcap-bpf.c,v 1.19 2006/01/16 22:45:57 reyk Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1998
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <sys/param.h>			/* optionally get BSD define */
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pcap-int.h"

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "gencode.h"

int
pcap_stats(pcap_t *p, struct pcap_stat *ps)
{
	struct bpf_stat s;

	if (ioctl(p->fd, BIOCGSTATS, (caddr_t)&s) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCGSTATS: %s",
		    pcap_strerror(errno));
		return (-1);
	}

	ps->ps_recv = s.bs_recv;
	ps->ps_drop = s.bs_drop;
	return (0);
}

int
pcap_read(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	int cc;
	int n = 0;
	register u_char *bp, *ep;

 again:
	/*
	 * Has "pcap_breakloop()" been called?
	 */
	if (p->break_loop) {
		/*
		 * Yes - clear the flag that indicates that it
		 * has, and return -2 to indicate that we were
		 * told to break out of the loop.
		 */
		p->break_loop = 0;
		return (-2);
	}

	cc = p->cc;
	if (p->cc == 0) {
		cc = read(p->fd, (char *)p->buffer, p->bufsize);
		if (cc < 0) {
			/* Don't choke when we get ptraced */
			switch (errno) {

			case EINTR:
				goto again;

			case EWOULDBLOCK:
				return (0);
#if defined(sun) && !defined(BSD)
			/*
			 * Due to a SunOS bug, after 2^31 bytes, the kernel
			 * file offset overflows and read fails with EINVAL.
			 * The lseek() to 0 will fix things.
			 */
			case EINVAL:
				if (lseek(p->fd, 0L, SEEK_CUR) +
				    p->bufsize < 0) {
					(void)lseek(p->fd, 0L, SEEK_SET);
					goto again;
				}
				/* FALLTHROUGH */
#endif
			}
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "read: %s",
			    pcap_strerror(errno));
			return (-1);
		}
		bp = p->buffer;
	} else
		bp = p->bp;

	/*
	 * Loop through each packet.
	 */
#define bhp ((struct bpf_hdr *)bp)
	ep = bp + cc;
	while (bp < ep) {
		register int caplen, hdrlen;

		/*
		 * Has "pcap_breakloop()" been called?
		 * If so, return immediately - if we haven't read any
		 * packets, clear the flag and return -2 to indicate
		 * that we were told to break out of the loop, otherwise
		 * leave the flag set, so that the *next* call will break
		 * out of the loop without having read any packets, and
		 * return the number of packets we've processed so far.
		 */
		if (p->break_loop) {
			if (n == 0) {
				p->break_loop = 0;
				return (-2);
			} else {
				p->bp = bp;
				p->cc = ep - bp;
				return (n);
			}
		}

		caplen = bhp->bh_caplen;
		hdrlen = bhp->bh_hdrlen;
		/*
		 * XXX A bpf_hdr matches a pcap_pkthdr.
		 */
		(*callback)(user, (struct pcap_pkthdr*)bp, bp + hdrlen);
		bp += BPF_WORDALIGN(caplen + hdrlen);
		if (++n >= cnt && cnt > 0) {
			p->bp = bp;
			p->cc = ep - bp;
			return (n);
		}
	}
#undef bhp
	p->cc = 0;
	return (n);
}

int
pcap_inject(pcap_t *p, const void *buf, size_t len)
{
	return (write(p->fd, buf, len));
}

static __inline int
bpf_open(pcap_t *p, char *errbuf)
{
	int fd;
	int n = 0;
	char device[sizeof "/dev/bpf0000000000"];

	/*
	 * Go through all the minors and find one that isn't in use.
	 */
	do {
		(void)snprintf(device, sizeof device, "/dev/bpf%d", n++);
		fd = open(device, O_RDWR);
		if (fd < 0 && errno == EACCES)
			fd = open(device, O_RDONLY);
	} while (fd < 0 && errno == EBUSY);

	/*
	 * XXX better message for all minors used
	 */
	if (fd < 0)
		snprintf(errbuf, PCAP_ERRBUF_SIZE, "%s: %s",
		    device, pcap_strerror(errno));

	return (fd);
}

pcap_t *
pcap_open_live(const char *device, int snaplen, int promisc, int to_ms,
    char *ebuf)
{
	int fd;
	struct ifreq ifr;
	struct bpf_version bv;
	u_int v;
	pcap_t *p;
	struct bpf_dltlist bdl;

	bzero(&bdl, sizeof(bdl));
	p = (pcap_t *)malloc(sizeof(*p));
	if (p == NULL) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "malloc: %s",
		    pcap_strerror(errno));
		return (NULL);
	}
	bzero(p, sizeof(*p));
	fd = bpf_open(p, ebuf);
	if (fd < 0)
		goto bad;

	p->fd = fd;
	p->snapshot = snaplen;

	if (ioctl(fd, BIOCVERSION, (caddr_t)&bv) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "BIOCVERSION: %s",
		    pcap_strerror(errno));
		goto bad;
	}
	if (bv.bv_major != BPF_MAJOR_VERSION ||
	    bv.bv_minor < BPF_MINOR_VERSION) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "kernel bpf filter out of date");
		goto bad;
	}
#if 0
	/* Just use the kernel default */
	v = 32768;	/* XXX this should be a user-accessible hook */
	/* Ignore the return value - this is because the call fails on
	 * BPF systems that don't have kernel malloc.  And if the call
	 * fails, it's no big deal, we just continue to use the standard
	 * buffer size.
	 */
	(void) ioctl(fd, BIOCSBLEN, (caddr_t)&v);
#endif

	(void)strlcpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
	if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "%s: %s",
		    device, pcap_strerror(errno));
		goto bad;
	}
	/* Get the data link layer type. */
	if (ioctl(fd, BIOCGDLT, (caddr_t)&v) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "BIOCGDLT: %s",
		    pcap_strerror(errno));
		goto bad;
	}
#if _BSDI_VERSION - 0 >= 199510
	/* The SLIP and PPP link layer header changed in BSD/OS 2.1 */
	switch (v) {

	case DLT_SLIP:
		v = DLT_SLIP_BSDOS;
		break;

	case DLT_PPP:
		v = DLT_PPP_BSDOS;
		break;
	}
#endif
	p->linktype = v;

	/*
	 * We know the default link type -- now determine all the DLTs
	 * this interface supports.  If this fails with EINVAL, it's
	 * not fatal; we just don't get to use the feature later.
	 */
	if (ioctl(fd, BIOCGDLTLIST, (caddr_t)&bdl) == 0) {
		bdl.bfl_list = (u_int *) calloc(bdl.bfl_len + 1, sizeof(u_int));
		if (bdl.bfl_list == NULL) {
			(void)snprintf(ebuf, PCAP_ERRBUF_SIZE, "malloc: %s",
			    pcap_strerror(errno));
			goto bad;
		}

		if (ioctl(fd, BIOCGDLTLIST, (caddr_t)&bdl) < 0) {
			(void)snprintf(ebuf, PCAP_ERRBUF_SIZE,
			    "BIOCGDLTLIST: %s", pcap_strerror(errno));
			goto bad;
		}
		p->dlt_count = bdl.bfl_len;
		p->dlt_list = bdl.bfl_list;
	} else {
		if (errno != EINVAL) {
			(void)snprintf(ebuf, PCAP_ERRBUF_SIZE,
			    "BIOCGDLTLIST: %s", pcap_strerror(errno));
			goto bad;
		}
	}

	/* set timeout */
	if (to_ms != 0) {
		struct timeval to;
		to.tv_sec = to_ms / 1000;
		to.tv_usec = (to_ms * 1000) % 1000000;
		if (ioctl(p->fd, BIOCSRTIMEOUT, (caddr_t)&to) < 0) {
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "BIOCSRTIMEOUT: %s",
			    pcap_strerror(errno));
			goto bad;
		}
	}
	if (promisc)
		/* set promiscuous mode, okay if it fails */
		(void)ioctl(p->fd, BIOCPROMISC, NULL);

	if (ioctl(fd, BIOCGBLEN, (caddr_t)&v) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "BIOCGBLEN: %s",
		    pcap_strerror(errno));
		goto bad;
	}
	p->bufsize = v;
	p->buffer = (u_char *)malloc(p->bufsize);
	if (p->buffer == NULL) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "malloc: %s",
		    pcap_strerror(errno));
		goto bad;
	}

	return (p);
 bad:
	if (fd >= 0)
		(void)close(fd);
	free(bdl.bfl_list);
	free(p);
	return (NULL);
}

int
pcap_setfilter(pcap_t *p, struct bpf_program *fp)
{
	int buflen;
	/*
	 * It looks that BPF code generated by gen_protochain() is not
	 * compatible with some of kernel BPF code (for example BSD/OS 3.1).
	 * Take a safer side for now.
	 */
	if (no_optimize || (p->sf.rfile != NULL)){
		if (p->fcode.bf_insns != NULL)
			pcap_freecode(&p->fcode);
		buflen = sizeof(*fp->bf_insns) * fp->bf_len;
		p->fcode.bf_len = fp->bf_len;
		p->fcode.bf_insns = malloc(buflen);
		if (p->fcode.bf_insns == NULL) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "malloc: %s",
			    pcap_strerror(errno));
			return (-1);
		}
		memcpy(p->fcode.bf_insns, fp->bf_insns, buflen);
	} else if (ioctl(p->fd, BIOCSETF, (caddr_t)fp) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCSETF: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	return (0);
}

int
pcap_set_datalink(pcap_t *p, int dlt)
{
	int i;

	if (p->dlt_count == 0) {
		/*
		 * We couldn't fetch the list of DLTs, or we don't
		 * have a "set datalink" operation, which means
		 * this platform doesn't support changing the
		 * DLT for an interface.  Check whether the new
		 * DLT is the one this interface supports.
		 */
		if (p->linktype != dlt)
			goto unsupported;

		/*
		 * It is, so there's nothing we need to do here.
		 */
		return (0);
	}
	for (i = 0; i < p->dlt_count; i++)
		if (p->dlt_list[i] == dlt)
			break;
	if (i >= p->dlt_count)
		goto unsupported;
	if (ioctl(p->fd, BIOCSDLT, &dlt) == -1) {
		(void) snprintf(p->errbuf, sizeof(p->errbuf),
		    "Cannot set DLT %d: %s", dlt, strerror(errno));
		return (-1);
	}
	p->linktype = dlt;
	return (0);

unsupported:
	(void) snprintf(p->errbuf, sizeof(p->errbuf),
	    "DLT %d is not one of the DLTs supported by this device",
	    dlt);
	return (-1);
}

