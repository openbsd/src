/*	$NetBSD: netif.c,v 1.1.1.1.2.1 1995/10/12 22:47:57 chuck Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon W. Ross
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
#include <sys/socket.h>
#include <string.h>
#include <time.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>

#include "stand.h"
#include "net.h"
#include "netif.h"
#include "config.h"

static struct netif netif_prom;
void machdep_common_ether __P((u_char *));

#ifdef NETIF_DEBUG
int netif_debug;
#endif

struct iodesc sockets[SOPEN_MAX];

struct iodesc *
socktodesc(sock)
	int sock;
{
	if (sock != 0) {
		return(NULL);
	}
	return (sockets);
}

int
netif_open(machdep_hint)
	void *machdep_hint;
{
	struct saioreq *si;
	struct iodesc *io;
	int error;

	/* find a free socket */
	io = sockets;
	if (io->io_netif) {
#ifdef	DEBUG
		printf("netif_open: device busy\n");
#endif
		return (-1);
	}
	bzero(io, sizeof(*io));

	if ((netif_prom.devdata = le_init(io)) == NULL) {
		printf("le_init failed\n");
		return(-1);
	}

	io->io_netif = &netif_prom;

	return(0);
}

int
netif_close(fd)
	int fd;
{
	struct iodesc *io;
	struct netif *ni;

	if (fd != 0) {
		errno = EBADF;
		return(-1);
	}

	io = sockets;
	ni = io->io_netif;
	if (ni != NULL) {
		le_end(ni);
		ni->devdata = NULL;
		io->io_netif = NULL;
	}
	return(0);
}

/*
 * Send a packet.  The ether header is already there.
 * Return the length sent (or -1 on error).
 */
int
netif_put(desc, pkt, len)
	struct iodesc *desc;
	void *pkt;
	size_t len;
{

#ifdef NETIF_DEBUG
	if (netif_debug) {
		struct ether_header *eh;

		printf("netif_put: desc=0x%x pkt=0x%x len=%d\n",
			   desc, pkt, len);
		eh = pkt;
		printf("dst: %s ", ether_sprintf(eh->ether_dhost));
		printf("src: %s ", ether_sprintf(eh->ether_shost));
		printf("type: 0x%x\n", eh->ether_type & 0xFFFF);
	}
#endif

	return(le_put(desc, pkt, len));
}

/*
 * Receive a packet, including the ether header.
 * Return the total length received (or -1 on error).
 */
int
netif_get(desc, pkt, maxlen, timo)
	struct iodesc *desc;
	void *pkt;
	size_t maxlen;
	time_t timo;
{
	struct saioreq *si;
	struct saif *sif;
	char *dmabuf;
	int tick0, tmo_ticks;
	int len;

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("netif_get: pkt=0x%x, maxlen=%d, tmo=%d\n",
			   pkt, maxlen, timo);
#endif

	len = le_get(desc, pkt, maxlen, timo);

#ifdef NETIF_DEBUG
	if (netif_debug) {
		struct ether_header *eh = pkt;

		printf("dst: %s ", ether_sprintf(eh->ether_dhost));
		printf("src: %s ", ether_sprintf(eh->ether_shost));
		printf("type: 0x%x\n", eh->ether_type & 0xFFFF);
	}
#endif

	return len;
}

/* the rest of this file imported from le_poll.c */

/*
 * Copyright (c) 1993 Adam Glass
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
 *      This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "if_lereg.h"

struct {
	struct	lereg1 *sc_r1;  /* LANCE registers */
	struct	lereg2 *sc_r2;  /* RAM */
	int	next_rmd;
	int	next_tmd;
} le_softc; 

int le_debug = 0;

/*
 * init le device.   return 0 on failure, 1 if ok.
 */

void * 
le_init(io)
	struct iodesc *io;
{
    u_long *eram = (u_long *) ERAM_ADDR;

    if (le_debug)
	printf("le: le_init called\n");
    machdep_common_ether(io->myea);
    bzero(&le_softc, sizeof(le_softc));
    le_softc.sc_r1 = (struct lereg1 *) LANCE_REG_ADDR;
    le_softc.sc_r2 = (struct lereg2 *) (*eram - (1024*1024));
    le_reset(io->io_netif, io->myea);

    return(&le_softc);
}

/*
 * close device
 * XXX le_softc
 */

void le_end(nif)

    struct netif *nif;
{
    struct lereg1 *ler1 = le_softc.sc_r1;

    if (le_debug)
	printf("le: le_end called\n");
    ler1->ler1_rap = LE_CSR0;
    ler1->ler1_rdp = LE_C0_STOP;
}


/*
 * reset device
 * XXX le_softc
 */

void le_reset(nif, myea)
     struct netif *nif;
     u_char *myea;
{
    struct lereg1 *ler1 = le_softc.sc_r1;
    struct lereg2 *ler2 = le_softc.sc_r2;
    unsigned int a;
    int timo = 100000, stat, i;

    if (le_debug)
	printf("le: le_reset called\n");
    ler1->ler1_rap = LE_CSR0;
    ler1->ler1_rdp = LE_C0_STOP;	/* do nothing until we are finished */

    bzero(ler2, sizeof(*ler2));

    ler2->ler2_mode = LE_MODE_NORMAL;
    ler2->ler2_padr[0] = myea[1];
    ler2->ler2_padr[1] = myea[0];
    ler2->ler2_padr[2] = myea[3];
    ler2->ler2_padr[3] = myea[2];
    ler2->ler2_padr[4] = myea[5];
    ler2->ler2_padr[5] = myea[4];


    ler2->ler2_ladrf0 = 0;
    ler2->ler2_ladrf1 = 0;

    a = (u_int)ler2->ler2_rmd;
    ler2->ler2_rlen =  LE_RLEN | (a >> 16);
    ler2->ler2_rdra = a & LE_ADDR_LOW_MASK; 

    a = (u_int)ler2->ler2_tmd;
    ler2->ler2_tlen = LE_TLEN | (a >> 16);
    ler2->ler2_tdra = a & LE_ADDR_LOW_MASK;

    ler1->ler1_rap = LE_CSR1;
    a = (u_int)ler2;
    ler1->ler1_rdp = a & LE_ADDR_LOW_MASK;
    ler1->ler1_rap = LE_CSR2;
    ler1->ler1_rdp = a >> 16;

    for (i = 0; i < LERBUF; i++) {
	a = (u_int)&ler2->ler2_rbuf[i];
	ler2->ler2_rmd[i].rmd0 = a & LE_ADDR_LOW_MASK;
	ler2->ler2_rmd[i].rmd1_bits = LE_R1_OWN;
	ler2->ler2_rmd[i].rmd1_hadr = a >> 16;
	ler2->ler2_rmd[i].rmd2 = -LEMTU;
	ler2->ler2_rmd[i].rmd3 = 0;
    }
    for (i = 0; i < LETBUF; i++) {
	a = (u_int)&ler2->ler2_tbuf[i];
	ler2->ler2_tmd[i].tmd0 = a & LE_ADDR_LOW_MASK;
	ler2->ler2_tmd[i].tmd1_bits = 0;
	ler2->ler2_tmd[i].tmd1_hadr = a >> 16;
	ler2->ler2_tmd[i].tmd2 = 0;
	ler2->ler2_tmd[i].tmd3 = 0;
    }

    ler1->ler1_rap = LE_CSR3;
    ler1->ler1_rdp = LE_C3_BSWP;

    ler1->ler1_rap = LE_CSR0;
    ler1->ler1_rdp = LE_C0_INIT;
    do {
	if (--timo == 0) {
	    printf("le: init timeout, stat = 0x%x\n", stat);
	    break;
	}
	stat = ler1->ler1_rdp;
    } while ((stat & LE_C0_IDON) == 0);
    
    ler1->ler1_rdp = LE_C0_IDON;
    le_softc.next_rmd = 0;
    le_softc.next_tmd = 0;
    ler1->ler1_rap = LE_CSR0;
    ler1->ler1_rdp = LE_C0_STRT;
}

/*
 * le_error
 * XXX le_softc
 */

void le_error(nif, str, vler1)
     struct netif *nif;
     char *str;
     volatile void *vler1;
{
     volatile struct lereg1 *ler1 = vler1;
    /* ler1->ler1_rap = LE_CSRO done in caller */
    if (ler1->ler1_rdp & LE_C0_BABL)
	panic("le: been babbling, found by '%s'\n", str);
    if (ler1->ler1_rdp & LE_C0_CERR) {
	ler1->ler1_rdp = LE_C0_CERR;
    }
    if (ler1->ler1_rdp & LE_C0_MISS) {
	ler1->ler1_rdp = LE_C0_MISS;
    }
    if (ler1->ler1_rdp & LE_C0_MERR) { 
	printf("le: memory error in '%s'\n", str);
	panic("memory error");
    }
}

/*
 * put a packet
 * XXX le_softc
 */

int le_put(desc, pkt, len)
     struct iodesc *desc;
     void *pkt;
     int len;
{
    volatile struct lereg1 *ler1 = le_softc.sc_r1;
    volatile struct lereg2 *ler2 = le_softc.sc_r2;
    volatile struct letmd *tmd;
    int timo = 100000, stat, i;
    unsigned int a;

    ler1->ler1_rap = LE_CSR0;
    if (ler1->ler1_rdp & LE_C0_ERR)
	le_error(desc->io_netif, "le_put(way before xmit)", ler1);
    tmd = &ler2->ler2_tmd[le_softc.next_tmd];
    while(tmd->tmd1_bits & LE_T1_OWN) {
	printf("le: output buffer busy\n");
    }
    bcopy(pkt, ler2->ler2_tbuf[le_softc.next_tmd], len);
    if (len < 64) 
	tmd->tmd2 = -64;
    else 
	tmd->tmd2 = -len;
    tmd->tmd3 = 0;
    if (ler1->ler1_rdp & LE_C0_ERR)
	le_error(desc->io_netif, "le_put(before xmit)", ler1);
    tmd->tmd1_bits = LE_T1_STP | LE_T1_ENP | LE_T1_OWN;
    a = (u_int)&ler2->ler2_tbuf[le_softc.next_tmd];
    tmd->tmd0 = a & LE_ADDR_LOW_MASK;
    tmd->tmd1_hadr = a >> 16;
    ler1->ler1_rdp = LE_C0_TDMD;
    if (ler1->ler1_rdp & LE_C0_ERR)
	le_error(desc->io_netif, "le_put(after xmit)", ler1);
    do {
	if (--timo == 0) {
	    printf("le: transmit timeout, stat = 0x%x\n", stat);
	    if (ler1->ler1_rdp & LE_C0_ERR)
		le_error(desc->io_netif, "le_put(timeout)", ler1);
	    break;
	}
	stat = ler1->ler1_rdp;
    } while ((stat & LE_C0_TINT) == 0);
    ler1->ler1_rdp = LE_C0_TINT;
    if (ler1->ler1_rdp & LE_C0_ERR) {
      if ((ler1->ler1_rdp & (LE_C0_BABL|LE_C0_CERR|LE_C0_MISS|LE_C0_MERR)) !=
	  LE_C0_CERR)
	printf("le_put: xmit error, buf %d\n", le_softc.next_tmd);
      le_error(desc->io_netif, "le_put(xmit error)", ler1);
    }
    le_softc.next_tmd = 0;
/*	(le_softc.next_tmd == (LETBUF - 1)) ? 0 : le_softc.next_tmd + 1;*/
    if (tmd->tmd1_bits & LE_T1_ERR) {
      printf("le: transmit error, error = 0x%x\n", tmd->tmd3);
      return -1;
    }
    if (le_debug) {
	printf("le: le_put() successful: sent %d\n", len);
	printf("le: le_put(): tmd1_bits: %x tmd3: %x\n", 
	       (unsigned int) tmd->tmd1_bits,
	       (unsigned int) tmd->tmd3);
    }
    return len;
}


/*
 * le_get
 */

int le_get(desc, pkt, len, timeout)
     struct iodesc *desc;
     void *pkt;
     int len;
     time_t timeout;
{
    time_t t;
    int cc;

    t = getsecs();
    cc = 0;
    while (((getsecs() - t) < timeout) && !cc) {
	cc = le_poll(desc, pkt, len);
    }
    return cc;
}


/*
 * le_poll
 * XXX softc
 */

int le_poll(desc, pkt, len)
     struct iodesc *desc;
     void *pkt;
     int len;
{
    struct lereg1 *ler1 = le_softc.sc_r1;
    struct lereg2 *ler2 = le_softc.sc_r2;
    unsigned int a;
    int length;
    struct lermd *rmd;


    ler1->ler1_rap = LE_CSR0;
    if ((ler1->ler1_rdp & LE_C0_RINT) != 0)
      ler1->ler1_rdp = LE_C0_RINT;
    rmd = &ler2->ler2_rmd[le_softc.next_rmd];
    if (rmd->rmd1_bits & LE_R1_OWN) {
        return(0);
    }
    if (ler1->ler1_rdp & LE_C0_ERR)
	le_error(desc->io_netif, "le_poll", ler1);
    if (rmd->rmd1_bits & LE_R1_ERR) {
	printf("le_poll: rmd status 0x%x\n", rmd->rmd1_bits);
	length = 0;
	goto cleanup;
    }
    if ((rmd->rmd1_bits & (LE_R1_STP|LE_R1_ENP)) != (LE_R1_STP|LE_R1_ENP))
	panic("le_poll: chained packet\n");

    length = rmd->rmd3;
    if (length >= LEMTU) {
	length = 0;
	panic("csr0 when bad things happen: %x\n", ler1->ler1_rdp);
	goto cleanup;
    }
    if (!length) goto cleanup;
    length -= 4;
    if (length > 0) {

      /*
       * if buffer is smaller than the packet truncate it.
       * (is this wise?)
       */
      if (length > len) 
	length = len;

      bcopy((void *)&ler2->ler2_rbuf[le_softc.next_rmd], pkt, length);
    }

 cleanup: 
    a = (u_int)&ler2->ler2_rbuf[le_softc.next_rmd];
    rmd->rmd0 = a & LE_ADDR_LOW_MASK;
    rmd->rmd1_hadr = a >> 16;
    rmd->rmd2 = -LEMTU;
    le_softc.next_rmd =
	(le_softc.next_rmd == (LERBUF - 1)) ? 0 : (le_softc.next_rmd + 1);
    rmd->rmd1_bits = LE_R1_OWN;
    return length;
}
