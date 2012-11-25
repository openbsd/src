/*	$OpenBSD: netdev.c,v 1.4 2012/11/25 14:10:47 miod Exp $ */

/*
 * Copyright (c) 1993 Paul Kranenburg
 * All rights reserved.
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <machine/prom.h>
#include <string.h>

#include "stand.h"
#include "tftpfs.h"

#include "libsa.h"

struct bugdev_softc {
	short	clun;
	short	dlun;
} bugdev_softc[1];

int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;
{
	struct bugdev_softc *pp = &bugdev_softc[0];

	pp->clun = (short)bugargs.ctrl_lun;
	pp->dlun = (short)bugargs.dev_lun;

	f->f_devdata = (void *)pp;
	f->f_dev = &devsw[0];
	*file = (char *)fname;
	return (0);
}

#define	NFR_TIMEOUT	5

int
net_strategy(devdata, func, nblk, size, buf, rsize)
	void *devdata;
	int func;
	daddr32_t nblk;
	size_t size;
	void *buf;
	size_t *rsize;
{
	struct bugdev_softc *pp = (struct bugdev_softc *)devdata;
	struct mvmeprom_netfread nfr;
	int attempts;

	for (attempts = 0; attempts < 10; attempts++) {
		nfr.ctrl = pp->clun;
		nfr.dev = pp->dlun;
		nfr.status = 0;
		nfr.addr = (u_long)buf;
		nfr.bytes = 0;
		nfr.blk = nblk;
		nfr.timeout = NFR_TIMEOUT;
		mvmeprom_netfread(&nfr);
	
		if (rsize) {
			*rsize = nfr.bytes;
		}

		if (nfr.status == 0)
			return (0);
	}

	return (EIO);
}

int
net_open(struct open_file *f, ...)
{
	va_list ap;
	struct mvmeprom_netcfig ncfg;
	struct mvmeprom_netfopen nfo;
	struct mvmeprom_ncp ncp;
#if 0
	struct mvmeprom_netctrl nctrl;
#endif
	struct bugdev_softc *pp = (struct bugdev_softc *)f->f_devdata;
	char *filename;
	const char *failure = NULL;

	va_start(ap, f);
	filename = va_arg(ap, char *);
	va_end(ap);

	/*
	 * It seems that, after loading tftpboot from the network, the BUG
	 * will reset its current network settings before giving us control
	 * of the system.  This causes any network parameter not stored in
	 * the NIOT area to be lost.  However, unless we force the `always
	 * send a reverse arp request' setting is set, the BUG will `believe'
	 * it doesn't need to send any (because it had to in order to load
	 * this code), and will fail to connect to the tftp server.
	 *
	 * Unfortunately, updating the in-memory network configuration to
	 * force reverse arp requests to be sent always doesn't work, even
	 * after issueing a `reset device' command.
	 *
	 * The best we can do is recognize this situation, warn the user
	 * and return to the BUG.
	 */

	bzero(&ncp, sizeof ncp);
	ncfg.ctrl = pp->clun;
	ncfg.dev = pp->dlun;
	ncfg.ncp_addr = (u_long)&ncp;
	ncfg.flags = NETCFIG_READ;
	if (mvmeprom_netcfig(&ncfg) == 0 && ncp.magic == NETCFIG_MAGIC) {
		if (ncp.rarp_control != 'A' && ncp.client_ip == 0) {
#if 0
			ncp.rarp_control = 'A';
			ncp.update_control = 'Y';

			bzero(&ncp, sizeof ncp);
			ncfg.ctrl = pp->clun;
			ncfg.dev = pp->dlun;
			ncfg.ncp_addr = (u_long)&ncp;
			ncfg.flags = NETCFIG_WRITE;
		
			if (mvmeprom_netcfig(&ncfg) == 0) {
				bzero(&nctrl, sizeof nctrl);
				nctrl.ctrl = pp->clun;
				nctrl.dev = pp->dlun;
				nctrl.cmd = NETCTRLCMD_RESET;
				if (mvmeprom_netctrl(&nctrl) != 0)
					failure = "reset network interface";
			} else
				failure = "update NIOT configuration";
#else
			printf("Invalid network configuration\n"
			    "Please update the NIOT parameters and set\n"
			    "``BOOTP/RARP Request Control: Always/When-Needed (A/W)'' to `A'\n");
			_rtt();
#endif
		}
	} else
		failure = "read NIOT configuration";

	if (failure != NULL)
		printf("failed to %s (0x%x), "
		    "hope RARP is set to `A'lways\n", failure, ncfg.status);

	nfo.ctrl = pp->clun;
	nfo.dev = pp->dlun;
	nfo.status = 0;
	strlcpy(nfo.filename, filename, sizeof nfo.filename);
	mvmeprom_netfopen(&nfo);
	
#ifdef DEBUG
	printf("tftp open(%s): 0x%x\n", filename, nfo.status);
#endif
	return (nfo.status);
}

int
net_close(f)
	struct open_file *f;
{
	return (0);
}

int
net_ioctl(f, cmd, data)
	struct open_file *f;
	u_long cmd;
	void *data;
{
	return (EIO);
}
