/*	$OpenBSD: iopctl.c,v 1.9 2007/10/16 20:19:27 sobrado Exp $	*/
/*	$NetBSD: iopctl.c,v 1.12 2002/01/04 10:17:20 ad Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/device.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include <dev/i2o/i2o.h>
#include <dev/i2o/iopio.h>

const char	*class2str(int);
void	getparam(int, int, void *, int);
int	gettid(char **);
int	main(int, char *[]);
int	show(const char *, const char *, ...);
void	i2ostrvis(const void *, int, void *, int);
void	usage(void);

void	reconfig(char **);
void	showdevid(char **);
void	showddmid(char **);
void	showlct(char **);
void	showstatus(char **);
void	showtidmap(char **);

struct {
	int	class;
	const char	*caption;
} const i2oclass[] = {
	{ I2O_CLASS_EXECUTIVE, "executive" },
	{ I2O_CLASS_DDM, "device driver module" },
	{ I2O_CLASS_RANDOM_BLOCK_STORAGE, "random block storage" },
	{ I2O_CLASS_SEQUENTIAL_STORAGE,	"sequential storage" },
	{ I2O_CLASS_LAN, "LAN port" },
	{ I2O_CLASS_WAN, "WAN port" },
	{ I2O_CLASS_FIBRE_CHANNEL_PORT,	"fibrechannel port" },
	{ I2O_CLASS_FIBRE_CHANNEL_PERIPHERAL, "fibrechannel peripheral" },
	{ I2O_CLASS_SCSI_PERIPHERAL, "SCSI peripheral" },
	{ I2O_CLASS_ATE_PORT, "ATE port" },
	{ I2O_CLASS_ATE_PERIPHERAL, "ATE peripheral" },
	{ I2O_CLASS_FLOPPY_CONTROLLER, "floppy controller" },
	{ I2O_CLASS_FLOPPY_DEVICE, "floppy device" },
	{ I2O_CLASS_BUS_ADAPTER_PORT, "bus adapter port" },
};

struct {
	const char	*label;
	int	takesargs;
	void	(*func)(char **);
} const cmdtab[] = {
	{ "reconfig", 0, reconfig },
	{ "showddmid", 1, showddmid },
	{ "showdevid", 1, showdevid },
	{ "showlct", 0, showlct },
	{ "showstatus",	0, showstatus },
	{ "showtidmap",	0, showtidmap },
};

int	fd;
char	buf[32768];
struct	i2o_status status;

int
main(int argc, char *argv[])
{
	int ch, i;
	const char *dv;
	struct iovec iov;

	dv = "/dev/iop0";

	while ((ch = getopt(argc, argv, "f:")) != -1) {
		switch (ch) {
		case 'f':
			dv = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (argv[optind] == NULL)
		usage();

	if ((fd = open(dv, O_RDWR)) < 0)
		err(EXIT_FAILURE, "%s", dv);

	iov.iov_base = &status;
	iov.iov_len = sizeof(status);
	if (ioctl(fd, IOPIOCGSTATUS, &iov) < 0)
		err(EXIT_FAILURE, "IOPIOCGSTATUS");

	for (i = 0; i < sizeof(cmdtab) / sizeof(cmdtab[0]); i++)
		if (strcmp(argv[optind], cmdtab[i].label) == 0) {
			if (cmdtab[i].takesargs == 0 &&
			    argv[optind + 1] != NULL)
				usage();
			(*cmdtab[i].func)(argv + optind + 1);
			break;
		}

	if (i == sizeof(cmdtab) / sizeof(cmdtab[0]))
		errx(EXIT_FAILURE, "unknown command ``%s''", argv[optind]);

	close(fd);
	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

void
usage(void)
{
	extern const char *__progname;

	(void)fprintf(stderr, "usage: %s [-f device] command [tid]\n",
	    __progname);
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}

int
show(const char *hdr, const char *fmt, ...)
{
	int i;
	va_list va;

	for (i = printf("%s", hdr); i < 25; i++)
		putchar(' ');
	va_start(va, fmt);
	i += vprintf(fmt, va);
	va_end(va);
	putchar('\n');
	return (i);
}

const char *
class2str(int class)
{
	int i;

	for (i = 0; i < sizeof(i2oclass) / sizeof(i2oclass[0]); i++)
		if (class == i2oclass[i].class)
			return (i2oclass[i].caption);

	return ("unknown");
}

void
getparam(int tid, int group, void *pbuf, int pbufsize)
{
	struct ioppt pt;
	struct i2o_util_params_op mb;
	struct i2o_reply *rf;
	struct {
		struct	i2o_param_op_list_header olh;
		struct	i2o_param_op_all_template oat;
	} __packed req;

	mb.msgflags = I2O_MSGFLAGS(i2o_util_params_op);
	mb.msgfunc = I2O_MSGFUNC(tid, I2O_UTIL_PARAMS_GET);
	mb.flags = 0;

	req.olh.count = htole16(1);
	req.olh.reserved = htole16(0);
	req.oat.operation = htole16(I2O_PARAMS_OP_FIELD_GET);
	req.oat.fieldcount = htole16(0xffff);
	req.oat.group = htole16(group);

	pt.pt_msg = &mb;
	pt.pt_msglen = sizeof(mb);
	pt.pt_reply = buf;
	pt.pt_replylen = sizeof(buf);
	pt.pt_timo = 10000;
	pt.pt_nbufs = 2;

	pt.pt_bufs[0].ptb_data = &req;
	pt.pt_bufs[0].ptb_datalen = sizeof(req);
	pt.pt_bufs[0].ptb_out = 1;

	pt.pt_bufs[1].ptb_data = pbuf;
	pt.pt_bufs[1].ptb_datalen = pbufsize;
	pt.pt_bufs[1].ptb_out = 0;

	if (ioctl(fd, IOPIOCPT, &pt) < 0)
		err(EXIT_FAILURE, "IOPIOCPT");

	rf = (struct i2o_reply *)buf;
	if (rf->reqstatus != 0)
		errx(EXIT_FAILURE, "I2O_UTIL_PARAMS_GET failed (%d)",
		    ((struct i2o_reply *)buf)->reqstatus);
	if ((rf->msgflags & I2O_MSGFLAGS_FAIL) != 0)
		errx(EXIT_FAILURE, "I2O_UTIL_PARAMS_GET failed (FAIL)");
}

void
showlct(char **argv)
{
	struct iovec iov;
	struct i2o_lct *lct;
	struct i2o_lct_entry *ent;
	u_int32_t classid, usertid;
	int i, nent;
	char ident[sizeof(ent->identitytag) * 4 + 1];

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	if (ioctl(fd, IOPIOCGLCT, &iov) < 0)
		err(EXIT_FAILURE, "IOPIOCGLCT");

	lct = (struct i2o_lct *)buf;
	ent = lct->entry;
	nent = ((letoh16(lct->tablesize) << 2) -
	    sizeof(struct i2o_lct) + sizeof(struct i2o_lct_entry)) /
	    sizeof(struct i2o_lct_entry);

	for (i = 0; i < nent; i++, ent++) {
		classid = letoh32(ent->classid);
		usertid = letoh32(ent->usertid);

		show("lct entry", "%d", i);
		show("entry size", "%d bytes", letoh16(ent->entrysize) << 2);
		show("local tid", "%d", letoh16(ent->localtid) & 4095);
		show("change indicator", "%d", letoh32(ent->changeindicator));
		show("flags", "%x", letoh32(ent->deviceflags));
		show("class id", "%x (%s)", classid & 4095,
		    class2str(classid & 4095));
		show("version", "%x", (classid >> 12) & 15);
		show("organisation id", "%x", classid >> 16);
		show("subclass info", "%x", letoh32(ent->subclassinfo));
		show("user tid", "%d", usertid & 4095);
		show("parent tid", "%d", (usertid >> 12) & 4095);
		show("bios info", "%d", (usertid >> 24) & 255);
		i2ostrvis(ent->identitytag, sizeof(ent->identitytag), ident,
		    sizeof(ident));
		show("identity tag", "<%s>", ident);
		show("event caps", "%x", letoh32(ent->eventcaps));

		if (i != nent - 1)
			printf("\n");
	}
}

void
showstatus(char **argv)
{
	char ident[sizeof(status.productid) + 1];
	u_int32_t segnumber;

	i2ostrvis(status.productid, sizeof(status.productid),
	    ident, sizeof(ident));

	segnumber = letoh32(status.segnumber);
	show("organization id", "%d", letoh16(status.orgid));
	show("iop id", "%d", letoh32(status.iopid) & 4095);
	show("host unit id", "%d", (letoh32(status.iopid) >> 16));
	show("segment number", "%d", segnumber & 4095);
	show("i2o version", "%d", (segnumber >> 12) & 15);
	show("iop state", "%d", (segnumber >> 16) & 255);
	show("messenger type", "%d", segnumber >> 24);
	show("inbound frame sz", "%d", letoh32(status.inboundmframesize));
	show("init code", "%d", status.initcode);
	show("max inbound queue depth", "%d",
	    letoh32(status.maxinboundmframes));
	show("inbound queue depth", "%d",
	    letoh32(status.currentinboundmframes));
	show("max outbound queue depth", "%d",
	    letoh32(status.maxoutboundmframes));
	show("product id string", "<%s>", ident);
	show("expected lct size", "%d", letoh32(status.expectedlctsize));
	show("iop capabilities", "0x%08x", letoh32(status.iopcaps));
	show("desired priv mem sz", "0x%08x",
	    letoh32(status.desiredprivmemsize));
	show("current priv mem sz", "0x%08x",
	    letoh32(status.currentprivmemsize));
	show("current priv mem base", "0x%08x",
	    letoh32(status.currentprivmembase));
	show("desired priv io sz", "0x%08x",
	    letoh32(status.desiredpriviosize));
	show("current priv io sz", "0x%08x",
	    letoh32(status.currentpriviosize));
	show("current priv io base", "0x%08x",
	    letoh32(status.currentpriviobase));
}

void
showddmid(char **argv)
{
	struct {
		struct	i2o_param_op_results pr;
		struct	i2o_param_read_results prr;
		struct	i2o_param_ddm_identity di;
		char padding[128];
	} __packed p;
	char ident[128];

	getparam(gettid(argv), I2O_PARAM_DDM_IDENTITY, &p, sizeof(p));

	show("ddm tid", "%d", letoh16(p.di.ddmtid) & 4095);
	i2ostrvis(p.di.name, sizeof(p.di.name), ident, sizeof(ident));
	show("module name", "%s", ident);
	i2ostrvis(p.di.revlevel, sizeof(p.di.revlevel), ident, sizeof(ident));
	show("module revision", "%s", ident);
	show("serial # format", "%d", p.di.snformat);
	show("serial #", "%08x%08x%08x", *(u_int32_t *)&p.di.serialnumber[0],
	    *(u_int32_t *)&p.di.serialnumber[4],
	    *(u_int32_t *)&p.di.serialnumber[8]);
}

void
showdevid(char **argv)
{
	struct {
		struct	i2o_param_op_results pr;
		struct	i2o_param_read_results prr;
		struct	i2o_param_device_identity di;
		char padding[128];
	} __packed p;
	char ident[128];

	getparam(gettid(argv), I2O_PARAM_DEVICE_IDENTITY, &p, sizeof(p));

	show("class id", "%d (%s)", letoh32(p.di.classid) & 4095,
	    class2str(letoh32(p.di.classid) & 4095));
	show("owner tid", "%d", letoh32(p.di.ownertid) & 4095);
	show("parent tid", "%d", letoh32(p.di.parenttid) & 4095);

	i2ostrvis(p.di.vendorinfo, sizeof(p.di.vendorinfo), ident,
	    sizeof(ident));
	show("vendor", "<%s>", ident);

	i2ostrvis(p.di.productinfo, sizeof(p.di.productinfo), ident,
	    sizeof(ident));
	show("product", "<%s>", ident);

	i2ostrvis(p.di.description, sizeof(p.di.description), ident,
	    sizeof(ident));
	show("description", "<%s>", ident);

	i2ostrvis(p.di.revlevel, sizeof(p.di.revlevel), ident, sizeof(ident));
	show("revision level", "<%s>", ident);
}

void
reconfig(char **argv)
{

	if (ioctl(fd, IOPIOCRECONFIG))
		err(EXIT_FAILURE, "IOPIOCRECONFIG");
}

void
showtidmap(char **argv)
{
	struct iovec iov;
	struct iop_tidmap *it;
	int nent;

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	if (ioctl(fd, IOPIOCGTIDMAP, &iov) < 0)
		err(EXIT_FAILURE, "IOPIOCGTIDMAP");

	nent = iov.iov_len / sizeof(*it);
	it = (struct iop_tidmap *)buf;

	for (; nent--; it++)
		if ((it->it_flags & IT_CONFIGURED) != 0)
			printf("%s\ttid %d\n", it->it_dvname, it->it_tid);
}

void
i2ostrvis(const void *srcv, int slen, void *dstv, int dlen)
{
	const char *src = srcv;
	char *dst = dstv;
	int hc, lc, i, nit;

	dlen--;
	lc = 0;
	hc = 0;
	i = 0;

	/*
	 * DPT use NUL as a space, whereas AMI use it as a terminator.  The
	 * spec has nothing to say about it.  Since AMI fields are usually
	 * filled with junk after the terminator, ...
	 */
	nit = (letoh16(status.orgid) != I2O_ORG_DPT);

	while (slen-- != 0 && dlen-- != 0) {
		if (nit && *src == '\0')
			break;
		else if (*src <= 0x20 || *src >= 0x7f) {
			if (hc)
				dst[i++] = ' ';
		} else {
			hc = 1;
			dst[i++] = *src;
			lc = i;
		}
		src++;
	}

	dst[lc] = '\0';
}

int
gettid(char **argv)
{
	char *argp;
	int tid;

	if (argv[1] != NULL)
		usage();

	tid = (int)strtol(argv[0], &argp, 0);
	if (*argp != '\0')
		usage();

	return (tid);
}
