/*	$OpenBSD: privsep_pcap.c,v 1.4 2004/04/14 09:14:19 otto Exp $ */

/*
 * Copyright (c) 2004 Can Erkin Acar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pcap-int.h>
#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "privsep.h"

/*
 * privileged part of priv_pcap_setfilter, compile the filter
 * expression, and return it to the child. Note that we fake an hpcap
 * and use it to capture the error messages, and pass the error back
 * to client.
 */
int
setfilter(int bpfd, int sock, char *filter)
{
	struct bpf_program fcode;
	int oflag, snap, link;
	u_int32_t netmask;
	pcap_t hpcap;

	must_read(sock, &oflag, sizeof(oflag));
	must_read(sock, &netmask, sizeof(netmask));
	must_read(sock, &snap, sizeof(snap));
	must_read(sock, &link, sizeof(link));

	if (snap < 0) {
		snprintf(hpcap.errbuf, PCAP_ERRBUF_SIZE, "invalid snaplen");
		goto err;
	}

	/* fake hpcap, it only needs errbuf, snaplen, and linktype to
	 * compile a filter expression */
	/* XXX messing with pcap internals */
	hpcap.snapshot = snap;
	hpcap.linktype = link;
	if (pcap_compile(&hpcap, &fcode, filter, oflag, netmask))
		goto err;

	/* write the filter */
	must_write(sock, &fcode.bf_len, sizeof(fcode.bf_len));
	if (fcode.bf_len > 0)
		must_write(sock, fcode.bf_insns,
		    fcode.bf_len * sizeof(struct bpf_insn));
	else {
		write_string(sock, "Invalid filter size");
		return (1);
	}

	/* if bpf descriptor is open, set the filter XXX check oflag? */
	if (bpfd >= 0 && ioctl(bpfd, BIOCSETF, &fcode))
		return 1;

	pcap_freecode(&fcode);

	return (0);

 err:
	fcode.bf_len = 0;
	must_write(sock, &fcode.bf_len, sizeof(fcode.bf_len));

	/* write back the error string */
	write_string(sock, hpcap.errbuf);

	return (1);
}

/*
 * filter is compiled and set in the parent, get the compiled output,
 * and set it locally, for filtering dumps etc.
 */
struct bpf_program *
priv_pcap_setfilter(pcap_t *hpcap, int oflag, u_int32_t netmask)
{
	int snap, link;
	struct bpf_program *fcode = NULL;
	char *ebuf = pcap_geterr(hpcap);

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion", __func__);

	snap = pcap_snapshot(hpcap);
	link = pcap_datalink(hpcap);

	fcode = calloc(1, sizeof(*fcode));
	if (fcode == NULL) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "out of memory");
		return (NULL);
	}

	write_command(priv_fd, PRIV_SETFILTER);

	/* send oflag, netmask, snaplen and linktype */
	must_write(priv_fd, &oflag, sizeof(oflag));
	must_write(priv_fd, &netmask, sizeof(netmask));
	must_write(priv_fd, &snap, sizeof(snap));
	must_write(priv_fd, &link, sizeof(link));

	/* receive compiled filter */
	must_read(priv_fd, &fcode->bf_len, sizeof(fcode->bf_len));
	if (fcode->bf_len <= 0) {
		int len;
		len = read_string(priv_fd, ebuf, PCAP_ERRBUF_SIZE, __func__);
		if (len == 0)
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "pcap compile error");
		goto err;
	}

	fcode->bf_insns = calloc(fcode->bf_len, sizeof(struct bpf_insn));
	if (fcode->bf_insns == NULL) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "out of memory");
		goto err;
	}

	must_read(priv_fd, fcode->bf_insns,
		  fcode->bf_len * sizeof(struct bpf_insn));

	pcap_setfilter(hpcap, fcode);

	return (fcode);

 err:
	if (fcode) {
		if (fcode->bf_insns)
			free(fcode->bf_insns);
		free(fcode);
	}
	return (NULL);
}


/* privileged part of priv_pcap_live */
int
pcap_live(const char *device, int snaplen, int promisc)
{
	char		bpf[sizeof "/dev/bpf0000000000"];
	struct ifreq	ifr;
	unsigned	v;
	int		fd = -1;
	int		n = 0;

	if (device == NULL || snaplen <= 0)
		goto error;

	do {
		snprintf(bpf, sizeof(bpf), "/dev/bpf%d", n++);
		fd = open(bpf, O_RDONLY);
	} while (fd < 0 && errno == EBUSY);

	if (fd < 0)
		goto error;

	v = 32768;	/* XXX this should be a user-accessible hook */
	ioctl(fd, BIOCSBLEN, &v);

	strlcpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
	if (ioctl(fd, BIOCSETIF, &ifr) < 0)
		goto error;

	if (promisc)
		/* this is allowed to fail */
		ioctl(fd, BIOCPROMISC, NULL);

	/* lock the descriptor */
	if (ioctl(fd, BIOCLOCK, NULL) < 0)
		goto error;

	return (fd);

 error:
	if (fd >= 0)
		close(fd);

	return (-1);
}


/*
 * XXX reimplement pcap_open_live with privsep, this is the
 * unprivileged part.
 */
pcap_t *
priv_pcap_live(const char *dev, int slen, int prom, int to_ms, char *ebuf)
{
	int fd, err;
	struct bpf_version bv;
	u_int v;
	pcap_t *p;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion", __func__);

	if (dev == NULL)
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "No interface specified");

	p = (pcap_t *)malloc(sizeof(*p));
	if (p == NULL) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "malloc: %s",
		    pcap_strerror(errno));
		return (NULL);
	}

	bzero(p, sizeof(*p));

	write_command(priv_fd, PRIV_OPEN_BPF);
	must_write(priv_fd, &slen, sizeof(int));
	must_write(priv_fd, &prom, sizeof(int));
	write_string(priv_fd, dev);

	fd = receive_fd(priv_fd);
	must_read(priv_fd, &err, sizeof(int));
	if (fd < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "Failed to open bpf device for %s: %s",
		    dev, strerror(err));
		goto bad;
	}

	/* fd is locked, can only use 'safe' ioctls */

	if (ioctl(fd, BIOCVERSION, &bv) < 0) {
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

	p->fd = fd;
	p->snapshot = slen;

	/* Get the data link layer type. */
	if (ioctl(fd, BIOCGDLT, &v) < 0) {
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

	/* set timeout */
	if (to_ms != 0) {
		struct timeval to;
		to.tv_sec = to_ms / 1000;
		to.tv_usec = (to_ms * 1000) % 1000000;
		if (ioctl(p->fd, BIOCSRTIMEOUT, &to) < 0) {
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "BIOCSRTIMEOUT: %s",
			    pcap_strerror(errno));
			goto bad;
		}
	}

	if (ioctl(fd, BIOCGBLEN, &v) < 0) {
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
		close(fd);
	free(p);
	return (NULL);
}



/*
 * reimplement pcap_open_offline with privsep, this is the
 * unprivileged part.
 * XXX merge with above?
 */
static void
swap_hdr(struct pcap_file_header *hp)
{
	hp->version_major = swap16(hp->version_major);
	hp->version_minor = swap16(hp->version_minor);
	hp->thiszone = swap32(hp->thiszone);
	hp->sigfigs = swap32(hp->sigfigs);
	hp->snaplen = swap32(hp->snaplen);
	hp->linktype = swap32(hp->linktype);
}

pcap_t *
priv_pcap_offline(const char *fname, char *errbuf)
{
	register pcap_t *p;
	register FILE *fp;
	struct pcap_file_header hdr;
	int linklen, err;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion", __func__);

	p = (pcap_t *)malloc(sizeof(*p));
	if (p == NULL) {
		strlcpy(errbuf, "out of swap", PCAP_ERRBUF_SIZE);
		return (NULL);
	}

	memset((char *)p, 0, sizeof(*p));

	if (fname[0] == '-' && fname[1] == '\0') {
		p->fd = -1;
		fp = stdin;
	} else {
		write_command(priv_fd, PRIV_OPEN_DUMP);
		p->fd = receive_fd(priv_fd);
		must_read(priv_fd, &err, sizeof(int));
		if (p->fd < 0) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "Failed to open input file %s: %s",
			    fname, strerror(err));
			goto bad;
		}

		fp = fdopen(p->fd, "r");
		if (fp == NULL) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE, "%s: %s", fname,
			    pcap_strerror(errno));
			goto bad;
		}
	}
	if (fread((char *)&hdr, sizeof(hdr), 1, fp) != 1) {
		snprintf(errbuf, PCAP_ERRBUF_SIZE, "fread: %s",
		    pcap_strerror(errno));
		goto bad;
	}

	if (hdr.magic != TCPDUMP_MAGIC) {
		if (swap32(hdr.magic) != TCPDUMP_MAGIC) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "bad dump file format");
			goto bad;
		}
		p->sf.swapped = 1;
		swap_hdr(&hdr);
	}
	if (hdr.version_major < PCAP_VERSION_MAJOR) {
		snprintf(errbuf, PCAP_ERRBUF_SIZE, "archaic file format");
		goto bad;
	}

	p->tzoff = hdr.thiszone;
	p->snapshot = hdr.snaplen;
	p->linktype = hdr.linktype;
	p->sf.rfile = fp;
	p->bufsize = hdr.snaplen;

	/* Align link header as required for proper data alignment */
	/* XXX should handle all types */
	switch (p->linktype) {

	case DLT_EN10MB:
		linklen = 14;
		break;

	case DLT_FDDI:
		linklen = 13 + 8;	/* fddi_header + llc */
		break;

	case DLT_NULL:
	default:
		linklen = 0;
		break;
	}

	if (p->bufsize < 0)
		p->bufsize = BPF_MAXBUFSIZE;
	p->sf.base = (u_char *)malloc(p->bufsize + BPF_ALIGNMENT);
	if (p->sf.base == NULL) {
		strlcpy(errbuf, "out of swap", PCAP_ERRBUF_SIZE);
		goto bad;
	}
	p->buffer = p->sf.base + BPF_ALIGNMENT - (linklen % BPF_ALIGNMENT);
	p->sf.version_major = hdr.version_major;
	p->sf.version_minor = hdr.version_minor;
#ifdef PCAP_FDDIPAD
	/* XXX what to do with this? */
	/* XXX padding only needed for kernel fcode */
	pcap_fddipad = 0;
#endif

	return (p);
 bad:
	free(p);
	return (NULL);
}


static int
sf_write_header(FILE *fp, int linktype, int thiszone, int snaplen)
{
	struct pcap_file_header hdr;

	hdr.magic = TCPDUMP_MAGIC;
	hdr.version_major = PCAP_VERSION_MAJOR;
	hdr.version_minor = PCAP_VERSION_MINOR;

	hdr.thiszone = thiszone;
	hdr.snaplen = snaplen;
	hdr.sigfigs = 0;
	hdr.linktype = linktype;

	if (fwrite((char *)&hdr, sizeof(hdr), 1, fp) != 1)
		return (-1);

	return (0);
}

pcap_dumper_t *
priv_pcap_dump_open(pcap_t *p, char *fname)
{
	int fd, err;
	FILE *f;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion\n", __func__);

	if (fname[0] == '-' && fname[1] == '\0') {
		f = stdout;
		priv_init_done();
	} else {
		write_command(priv_fd, PRIV_OPEN_OUTPUT);
		fd = receive_fd(priv_fd);
		must_read(priv_fd, &err, sizeof(err));
		if (fd < 0)  {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "Failed to open output file %s: %s",
			    fname, strerror(err));
			return (NULL);
		}
		f = fdopen(fd, "w");
		if (f == NULL) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "%s: %s",
			    fname, pcap_strerror(errno));
			return (NULL);
		}
	}

	(void)sf_write_header(f, p->linktype, p->tzoff, p->snapshot);
	return ((pcap_dumper_t *)f);
}
