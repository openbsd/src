/*	$OpenBSD: if_ie.c,v 1.6 2003/01/28 01:37:52 jason Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#define ETHER_MIN_LEN   64
#define ETHER_MAX_LEN   1518
#define ETHER_CRC_LEN	4

#define NTXBUF	1
#define NRXBUF	16
#define IE_RBUF_SIZE	ETHER_MAX_LEN

#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"
#include "netif.h"
#include "config.h"

#include "i82586.h"
#include "if_iereg.h"

int     ie_debug = 0;

void ie_stop(struct netif *);
void ie_end(struct netif *);
void ie_error(struct netif *, char *, volatile struct iereg *);
int ie_get(struct iodesc *, void *, size_t, time_t);
void ie_init(struct iodesc *, void *);
int ie_match(struct netif *, void *);
int ie_poll(struct iodesc *, void *, int);
int ie_probe(struct netif *, void *);
int ie_put(struct iodesc *, void *, size_t);
void ie_reset(struct netif *, u_char *);

struct netif_stats ie_stats;

struct netif_dif ie0_dif = {
	0,			/* unit */
	1,			/* nsel */
	&ie_stats,
	0,
	0,
};

struct netif_driver ie_driver = {
	"ie",			/* netif_bname */
	ie_match,		/* match */
	ie_probe,		/* probe */
	ie_init,		/* init */
	ie_get,			/* get */
	ie_put,			/* put */
	ie_end,			/* end */
	&ie0_dif,		/* netif_ifs */
	1,			/* netif_nifs */
};

struct ie_configuration {
	u_int   phys_addr;
	int     used;
} ie_config[] = {
	{ INTEL_REG_ADDR, 0 }
};

int     nie_config = sizeof(ie_config) / (sizeof(ie_config[0]));

struct {
	struct iereg *sc_reg;	/* IE registers */
	struct iemem *sc_mem;	/* RAM */
}       ie_softc;

int
ie_match(nif, machdep_hint)
	struct netif *nif;
	void   *machdep_hint;
{
	char   *name;
	int     i, val = 0;

	name = machdep_hint;
	if (name && !bcmp(ie_driver.netif_bname, name, 2))
		val += 10;
	for (i = 0; i < nie_config; i++) {
		if (ie_config[i].used)
			continue;
		if (ie_debug)
			printf("ie%d: ie_match --> %d\n", i, val + 1);
		ie_config[i].used++;
		return (val + 1);
	}
	if (ie_debug)
		printf("ie%d: ie_match --> 0\n", i);
	return (0);
}

int
ie_probe(nif, machdep_hint)
	struct netif *nif;
	void   *machdep_hint;
{

	/* the set unit is the current unit */
	if (ie_debug)
		printf("ie%d: ie_probe called\n", nif->nif_unit);
	return (0);
/*	return (1);*/
}

void
ie_error(nif, str, ier)
	struct netif *nif;
	char   *str;
	volatile struct iereg *ier;
{
	panic("ie%d: unknown error", nif->nif_unit);
}

ieack(ier, iem)
	volatile struct iereg *ier;
	struct iemem *iem;
{
	/* ack the `interrupt' */
	iem->im_scb.ie_command = iem->im_scb.ie_status & IE_ST_WHENCE;
	ier->ie_attention = 1;	/* chan attention! */
	while (iem->im_scb.ie_command)
		;
}

void
ie_reset(nif, myea)
	struct netif *nif;
	u_char *myea;
{
	volatile struct iereg *ier = ie_softc.sc_reg;
	struct iemem *iem = ie_softc.sc_mem;
	int     timo = 10000, stat, i;
	volatile int t;
	u_int   a;

	if (ie_debug)
		printf("ie%d: ie_reset called\n", nif->nif_unit);

	/*printf("ier %x iem %x\n", ier, iem);*/

	*(u_char *)0xfff4202a = 0x40;

	bzero(iem, sizeof(*iem));
	iem->im_scp.scp_sysbus = 0;
	iem->im_scp.scp_iscp_low = (int) &iem->im_iscp & 0xffff;
	iem->im_scp.scp_iscp_high = (int) &iem->im_iscp >> 16;

	iem->im_iscp.iscp_scboffset = (int) &iem->im_scb - (int) iem;
	iem->im_iscp.iscp_busy = 1;
	iem->im_iscp.iscp_base_low = (int) iem & 0xffff;
	iem->im_iscp.iscp_base_high = (int) iem >> 16;

	/*
	 * completely and utterly unlike what i expected, the
	 * "write" order is:
	 * 1st:	d15-d0 -> high address
	 * 2nd:	d31-d16 -> low address
	 */

	/* reset chip */
	a = IE_PORT_RESET;
	ier->ie_porthigh = a & 0xffff;
	t = 0;
	t = 1;
	ier->ie_portlow = a >> 16;
	for (t = timo; t--;)
		;

	/* set new SCP pointer */
	a = (int) &iem->im_scp | IE_PORT_NEWSCP;
	ier->ie_porthigh = a & 0xffff;
	t = 0;
	t = 1;
	ier->ie_portlow = a >> 16;
	for (t = timo; t--;)
		;

	ier->ie_attention = 1;	/* chan attention! */
	for (t = timo * 10; t--;)
		;

	/* send CONFIGURE command */
	iem->im_scb.ie_command = IE_CU_START;
	iem->im_scb.ie_command_list = (int) &iem->im_cc - (int) iem;
	iem->im_cc.com.ie_cmd_status = 0;
	iem->im_cc.com.ie_cmd_cmd = IE_CMD_CONFIG | IE_CMD_LAST;
	iem->im_cc.com.ie_cmd_link = 0xffff;
	iem->im_cc.ie_config_count = 0x0c;
	iem->im_cc.ie_fifo = 8;
	iem->im_cc.ie_save_bad = 0x40;
	iem->im_cc.ie_addr_len = 0x2e;
	iem->im_cc.ie_priority = 0;
	iem->im_cc.ie_ifs = 0x60;
	iem->im_cc.ie_slot_low = 0;
	iem->im_cc.ie_slot_high = 0xf2;
	iem->im_cc.ie_promisc = 0;
	iem->im_cc.ie_crs_cdt = 0;
	iem->im_cc.ie_min_len = 64;
	iem->im_cc.ie_junk = 0xff;

	ier->ie_attention = 1;	/* chan attention! */
	for (t = timo * 10; t--;)
		;

	ieack(ier, iem);

	/*printf("ic %x\n", &iem->im_ic);*/
	/* send IASETUP command */
	iem->im_scb.ie_command = IE_CU_START;
	iem->im_scb.ie_command_list = (int) &iem->im_ic - (int) iem;
	iem->im_ic.com.ie_cmd_status = 0;
	iem->im_ic.com.ie_cmd_cmd = IE_CMD_IASETUP | IE_CMD_LAST;
	iem->im_ic.com.ie_cmd_link = 0xffff;
	bcopy(myea, (void *)&iem->im_ic.ie_address, sizeof iem->im_ic.ie_address);

	ier->ie_attention = 1;	/* chan attention! */
	for (t = timo * 10; t--;)
		;

	ieack(ier, iem);

	/* setup buffers */

	for (i = 0; i < NRXBUF; i++) {
		iem->im_rfd[i].ie_fd_next = (int) &iem->im_rfd[(i+1) % NRXBUF] -
		    (int) iem;
		iem->im_rbd[i].ie_rbd_next = (int) &iem->im_rbd[(i+1) % NRXBUF] -
		    (int) iem;
		a = (int) &iem->im_rxbuf[i * IE_RBUF_SIZE];
		iem->im_rbd[i].ie_rbd_buffer_low = a & 0xffff;
		iem->im_rbd[i].ie_rbd_buffer_high = a >> 16;
		iem->im_rbd[i].ie_rbd_length = IE_RBUF_SIZE;
	}
	iem->im_rfd[NRXBUF-1].ie_fd_last |= IE_FD_LAST;
	iem->im_rbd[NRXBUF-1].ie_rbd_length |= IE_RBD_LAST;
	iem->im_rfd[0].ie_fd_buf_desc = (int) &iem->im_rbd[0] - (int) iem;

	/*printf("rfd[0] %x rbd[0] %x buf[0] %x\n", &iem->im_rfd, &iem->im_rbd,
	    &iem->im_rxbuf);*/

	/* send receiver start command */
	iem->im_scb.ie_command = IE_RU_START;
	iem->im_scb.ie_command_list = 0;
	iem->im_scb.ie_recv_list = (int) &iem->im_rfd[0] - (int) iem;
	ier->ie_attention = 1;	/* chan attention! */
	while (iem->im_scb.ie_command)
		;

	ieack(ier, iem);
}

int
ie_poll(desc, pkt, len)
	struct iodesc *desc;
	void   *pkt;
	int     len;
{
	volatile struct iereg *ier = ie_softc.sc_reg;
	struct iemem *iem = ie_softc.sc_mem;
	u_char *p = pkt;
	static int slot;
	int     length = 0;
	u_int   a;
	u_short status;

	asm(".word	0xf518\n");
	status = iem->im_rfd[slot].ie_fd_status;
	if (status & IE_FD_BUSY)
		return (0);

	/* printf("slot %d: %x\n", slot, status); */
	if ((status & (IE_FD_COMPLETE | IE_FD_OK)) == (IE_FD_COMPLETE | IE_FD_OK)) {
		if (status & IE_FD_OK) {
			length = iem->im_rbd[slot].ie_rbd_actual & 0x3fff;
			if (length > len)
				length = len;
			bcopy((void *)&iem->im_rxbuf[slot * IE_RBUF_SIZE],
			    pkt, length);

			iem->im_rfd[slot].ie_fd_status = 0;
			iem->im_rfd[slot].ie_fd_last |= IE_FD_LAST;
			iem->im_rfd[(slot+NRXBUF-1)%NRXBUF].ie_fd_last &=
			    ~IE_FD_LAST;
			iem->im_rbd[slot].ie_rbd_actual = 0;
			iem->im_rbd[slot].ie_rbd_length |= IE_RBD_LAST;
			iem->im_rbd[(slot+NRXBUF-1)%NRXBUF].ie_rbd_length &=
			    ~IE_RBD_LAST;
			/*printf("S%d\n", slot);*/

		} else {
			printf("shit\n");
		}
		slot++;
		/* should move descriptor onto end of queue... */
	}
	if ((iem->im_scb.ie_status & IE_RU_READY) == 0) {
		printf("RR\n");

		for (slot = 0; slot < NRXBUF; slot++) {
			iem->im_rbd[slot].ie_rbd_length &= ~IE_RBD_LAST;
			iem->im_rfd[slot].ie_fd_last &= ~IE_FD_LAST;
		}
		iem->im_rbd[NRXBUF-1].ie_rbd_length |= IE_RBD_LAST;
		iem->im_rfd[NRXBUF-1].ie_fd_last |= IE_FD_LAST;

		iem->im_rfd[0].ie_fd_buf_desc = (int)&iem->im_rbd[0] - (int)iem;

		iem->im_scb.ie_command = IE_RU_START;
		iem->im_scb.ie_command_list = 0;
		iem->im_scb.ie_recv_list = (int)&iem->im_rfd[0] - (int)iem;
		ier->ie_attention = 1;	/* chan attention! */
		while (iem->im_scb.ie_command)
			;
		slot = 0;
	}
	slot = slot % NRXBUF;
	return (length);
}

int
ie_put(desc, pkt, len)
	struct	iodesc *desc;
	void	*pkt;
	size_t	len;
{
	volatile struct iereg *ier = ie_softc.sc_reg;
	struct iemem *iem = ie_softc.sc_mem;
	u_char *p = pkt;
	int     timo = 10000, stat, i;
	volatile int t;
	u_int   a;
	int     xx = 0;

	/* send transmit command */

	while (iem->im_scb.ie_command)
		;

	/* copy data */
	bcopy(p, (void *)&iem->im_txbuf[xx], len);

	if (len < ETHER_MIN_LEN - ETHER_CRC_LEN) {
		bzero((char *)&iem->im_txbuf[xx] + len,
		    ETHER_MIN_LEN - ETHER_CRC_LEN - len);
		len = ETHER_MIN_LEN - ETHER_CRC_LEN;
	}

	/* build transmit descriptor */
	iem->im_xd[xx].ie_xmit_flags = len | IE_XMIT_LAST;
	iem->im_xd[xx].ie_xmit_next = 0xffff;
	a = (int) &iem->im_txbuf[xx];
	iem->im_xd[xx].ie_xmit_buf_low = a & 0xffff;
	iem->im_xd[xx].ie_xmit_buf_high = a >> 16;

	/* transmit command */
	iem->im_xc[xx].com.ie_cmd_status = 0;
	iem->im_xc[xx].com.ie_cmd_cmd = IE_CMD_XMIT | IE_CMD_LAST;
	iem->im_xc[xx].com.ie_cmd_link = 0xffff;
	iem->im_xc[xx].ie_xmit_desc = (int) &iem->im_xd[xx] - (int) iem;
	iem->im_xc[xx].ie_xmit_length = len;
	bcopy(p, (void *)&iem->im_xc[xx].ie_xmit_addr,
	    sizeof iem->im_xc[xx].ie_xmit_addr);

	iem->im_scb.ie_command = IE_CU_START;
	iem->im_scb.ie_command_list = (int) &iem->im_xc[xx] - (int) iem;

	ier->ie_attention = 1;	/* chan attention! */

	if (ie_debug) {
		printf("ie%d: send %d to %x:%x:%x:%x:%x:%x\n",
		    desc->io_netif->nif_unit, len,
		    p[0], p[1], p[2], p[3], p[4], p[5]);
	}
	return (len);
}

int
ie_get(desc, pkt, len, timeout)
	struct	iodesc *desc;
	void	*pkt;
	size_t	len;
	time_t	timeout;
{
	time_t  t;
	int     cc;

	t = getsecs();
	cc = 0;
	while (((getsecs() - t) < timeout) && !cc) {
		cc = ie_poll(desc, pkt, len);
	}
	return (cc);
}
/*
 * init ie device.   return 0 on failure, 1 if ok.
 */
void
ie_init(desc, machdep_hint)
	struct iodesc *desc;
	void   *machdep_hint;
{
	struct netif *nif = desc->io_netif;

	if (ie_debug)
		printf("ie%d: ie_init called\n", desc->io_netif->nif_unit);
	machdep_common_ether(desc->myea);
	bzero(&ie_softc, sizeof(ie_softc));
	ie_softc.sc_reg =
	    (struct iereg *) ie_config[desc->io_netif->nif_unit].phys_addr;
	ie_softc.sc_mem = (struct iemem *) 0xae0000;
	ie_reset(desc->io_netif, desc->myea);
	printf("device: %s%d attached to %s\n", nif->nif_driver->netif_bname,
	    nif->nif_unit, ether_sprintf(desc->myea));
}

void
ie_stop(nif)
	struct netif *nif;
{
	volatile struct iereg *ier = ie_softc.sc_reg;
	struct iemem *iem = ie_softc.sc_mem;
	int     timo = 10000;
	volatile int t;
	u_int   a;

	iem->im_iscp.iscp_busy = 1;
	/* reset chip */
	a = IE_PORT_RESET;
	ier->ie_porthigh = a & 0xffff;
	t = 0;
	t = 1;
	ier->ie_portlow = a >> 16;
	for (t = timo; t--;)
		;

	/* reset chip again */
	a = IE_PORT_RESET;
	ier->ie_porthigh = a & 0xffff;
	t = 0;
	t = 1;
	ier->ie_portlow = a >> 16;
	for (t = timo; t--;)
		;

	/*printf("status %x busy %x\n", iem->im_scb.ie_status,
	    iem->im_iscp.iscp_busy);*/
}

void
ie_end(nif)
	struct netif *nif;
{
	if (ie_debug)
		printf("ie%d: ie_end called\n", nif->nif_unit);

	ie_stop(nif);

	/* *(u_char *) 0xfff42002 = 0; */
}
