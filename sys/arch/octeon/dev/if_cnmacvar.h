/*	$OpenBSD: if_cnmacvar.h,v 1.6 2015/06/11 12:30:42 jmatthew Exp $	*/

/*
 * Copyright (c) 2007 Internet Initiative Japan, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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

#define IS_MAC_MULTICASTBIT(addr) \
        ((addr)[0] & 0x01)

#define SEND_QUEUE_SIZE		(32)
#define GATHER_QUEUE_SIZE	(1024)
#define FREE_QUEUE_SIZE		GATHER_QUEUE_SIZE
#define RECV_QUEUE_SIZE		(GATHER_QUEUE_SIZE * 2)

#ifdef OCTEON_ETH_FIXUP_ODD_NIBBLE_DYNAMIC
#define PROC_NIBBLE_SOFT_THRESHOLD 2000
#endif

struct _send_queue_entry;
struct cn30xxpow_softc;
struct cn30xxpip_softc;
struct cn30xxipd_softc;
struct cn30xxpko_softc;
struct cn30xxasx_softc;
struct cn30xxsmi_softc;
struct cn30xxgmx_port_softc;
struct cn30xxpow_softc;

extern struct cn30xxpow_softc	cn30xxpow_softc;

struct octeon_eth_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_regt;
	bus_dma_tag_t		sc_dmat;

	bus_dmamap_t		sc_dmap;

	void			*sc_pow_recv_ih;
	struct cn30xxpip_softc	*sc_pip;
	struct cn30xxipd_softc	*sc_ipd;
	struct cn30xxpko_softc	*sc_pko;
	struct cn30xxasx_softc	*sc_asx;
	struct cn30xxsmi_softc	*sc_smi;
	struct cn30xxgmx_softc	*sc_gmx;
	struct cn30xxgmx_port_softc
				*sc_gmx_port;
	struct cn30xxpow_softc
				*sc_pow;

	struct arpcom		sc_arpcom;
	struct mii_data		sc_mii;

	struct timeout		sc_tick_misc_ch;
	struct timeout		sc_tick_free_ch;
	struct timeout		sc_resume_ch;

	int64_t			sc_soft_req_cnt;
	int64_t			sc_soft_req_thresh;
	int64_t			sc_hard_done_cnt;
	int			sc_flush;
	int			sc_prefetch;
	struct mbuf_list	sc_sendq;
	uint64_t		sc_ext_callback_cnt;

	uint32_t		sc_port;
	uint32_t		sc_port_type;
	uint32_t		sc_init_flag;
	int			sc_phy_addr;

	/*
	 * Redirection - received (input) packets are redirected (directly sent)
	 * to another port.  Only meant to test hardware + driver performance.
	 *
	 *  0	- disabled
	 * >0	- redirected to ports that correspond to bits
	 *		0b001 (0x1)	- Port 0
	 *		0b010 (0x2)	- Port 1
	 *		0b100 (0x4)	- Port 2
	 */
	int			sc_redir;

	struct cn30xxfau_desc	sc_fau_done;
	struct cn30xxpko_cmdptr_desc
				sc_cmdptr;

	size_t			sc_ip_offset;

	struct timeval		sc_rate_recv_check_link_last;
	struct timeval		sc_rate_recv_check_link_cap;
	struct timeval		sc_rate_recv_check_jumbo_last;
	struct timeval		sc_rate_recv_check_jumbo_cap;
	struct timeval		sc_rate_recv_check_code_last;
	struct timeval		sc_rate_recv_check_code_cap;
};
