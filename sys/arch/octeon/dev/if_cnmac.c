/*	$OpenBSD: if_cnmac.c,v 1.3 2011/06/19 02:01:23 yasuoka Exp $	*/

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
#include <sys/cdefs.h>

/*
 * XXXSEIL
 * If no free send buffer is available, free all the sent buffer and bail out.
 */
#define OCTEON_ETH_SEND_QUEUE_CHECK

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/conf.h>
#include <sys/stdint.h> /* uintptr_t */
#include <sys/sysctl.h>
#include <sys/syslog.h>
#ifdef MBUF_TIMESTAMP
#include <sys/time.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/route.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/endian.h>
#include <machine/octeonvar.h>
#include <machine/octeon_model.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <octeon/dev/cn30xxasxreg.h>
#include <octeon/dev/cn30xxciureg.h>
#include <octeon/dev/cn30xxnpireg.h>
#include <octeon/dev/cn30xxgmxreg.h>
#include <octeon/dev/cn30xxipdreg.h>
#include <octeon/dev/cn30xxpipreg.h>
#include <octeon/dev/cn30xxpowreg.h>
#include <octeon/dev/cn30xxfaureg.h>
#include <octeon/dev/cn30xxfpareg.h>
#include <octeon/dev/cn30xxbootbusreg.h>
#include <octeon/dev/cn30xxfpavar.h>
#include <octeon/dev/cn30xxgmxvar.h>
#include <octeon/dev/cn30xxfauvar.h>
#include <octeon/dev/cn30xxpowvar.h>
#include <octeon/dev/cn30xxipdvar.h>
#include <octeon/dev/cn30xxpipvar.h>
#include <octeon/dev/cn30xxpkovar.h>
#include <octeon/dev/cn30xxasxvar.h>
#include <octeon/dev/cn30xxsmivar.h>
#include <octeon/dev/iobusvar.h>
#include <octeon/dev/if_cnmacvar.h>

#ifndef SET
#define	SET(t, f)	((t) |= (f))
#define	ISSET(t, f)	((t) & (f))
#define	CLR(t, f)	((t) &= ~(f))
#endif

#ifdef OCTEON_ETH_DEBUG
#define	OCTEON_ETH_KASSERT(x)	KASSERT(x)
#define	OCTEON_ETH_KDASSERT(x)	KDASSERT(x)
#else
#define	OCTEON_ETH_KASSERT(x)
#define	OCTEON_ETH_KDASSERT(x)
#endif

/*
 * Set the PKO to think command buffers are an odd length.  This makes it so we
 * never have to divide a comamnd across two buffers.
 */
#define OCTEON_POOL_NWORDS_CMD	\
	    (((uint32_t)OCTEON_POOL_SIZE_CMD / sizeof(uint64_t)) - 1)
#define FPA_COMMAND_BUFFER_POOL_NWORDS	OCTEON_POOL_NWORDS_CMD	/* XXX */

#if NBPFILTER > 0
#define	OCTEON_ETH_TAP(ifp, m, dir) \
	do { \
		/* Pass this up to any BPF listeners. */ \
		if ((ifp)->if_bpf) \
			bpf_mtap((ifp)->if_bpf, (m), (dir)); \
	} while (0/* CONSTCOND */)
#else
#define	OCTEON_ETH_TAP(ifp, m, dir)
#endif /* NBPFILTER > 0 */

static void		octeon_eth_buf_init(struct octeon_eth_softc *);

static int	octeon_eth_match(struct device *, void *, void *);
static void	octeon_eth_attach(struct device *, struct device *, void *);
static void	octeon_eth_pip_init(struct octeon_eth_softc *);
static void	octeon_eth_ipd_init(struct octeon_eth_softc *);
static void	octeon_eth_pko_init(struct octeon_eth_softc *);
static void	octeon_eth_asx_init(struct octeon_eth_softc *);
static void	octeon_eth_smi_init(struct octeon_eth_softc *);

static void	octeon_eth_board_mac_addr(uint8_t *, size_t, int);

static int	octeon_eth_mii_readreg(struct device *, int, int);
static void	octeon_eth_mii_writereg(struct device *, int, int, int);
static void	octeon_eth_mii_statchg(struct device *);

static int	octeon_eth_mediainit(struct octeon_eth_softc *);
static void	octeon_eth_mediastatus(struct ifnet *, struct ifmediareq *);
static int	octeon_eth_mediachange(struct ifnet *);

static void	octeon_eth_send_queue_flush_prefetch(struct octeon_eth_softc *);
static void	octeon_eth_send_queue_flush_fetch(struct octeon_eth_softc *);
static void	octeon_eth_send_queue_flush(struct octeon_eth_softc *);
static void	octeon_eth_send_queue_flush_sync(struct octeon_eth_softc *);
static int	octeon_eth_send_queue_is_full(struct octeon_eth_softc *);
static void	octeon_eth_send_queue_add(struct octeon_eth_softc *,
			    struct mbuf *, uint64_t *);
static void	octeon_eth_send_queue_del(struct octeon_eth_softc *,
			    struct mbuf **, uint64_t **);
static int	octeon_eth_buf_free_work(struct octeon_eth_softc *,
			    uint64_t *, uint64_t);
static void	octeon_eth_buf_ext_free_m(caddr_t, u_int, void *);
static void	octeon_eth_buf_ext_free_ext(caddr_t, u_int, void *);

static int	octeon_eth_ioctl(struct ifnet *, u_long, caddr_t);
static void	octeon_eth_watchdog(struct ifnet *);
static int	octeon_eth_init(struct ifnet *);
static int	octeon_eth_stop(struct ifnet *, int);
static void	octeon_eth_start(struct ifnet *);

static int	octeon_eth_send_cmd(struct octeon_eth_softc *, uint64_t,
			    uint64_t);
static uint64_t	octeon_eth_send_makecmd_w1(int, paddr_t);
static uint64_t octeon_eth_send_makecmd_w0(uint64_t, uint64_t, size_t,
			    int);
static int	octeon_eth_send_makecmd_gbuf(struct octeon_eth_softc *,
			    struct mbuf *, uint64_t *, int *);
static int	octeon_eth_send_makecmd(struct octeon_eth_softc *,
			    struct mbuf *, uint64_t *, uint64_t *, uint64_t *);
static int	octeon_eth_send_buf(struct octeon_eth_softc *,
			    struct mbuf *, uint64_t *);
static int	octeon_eth_send(struct octeon_eth_softc *,
			    struct mbuf *);

static int	octeon_eth_reset(struct octeon_eth_softc *);
static int	octeon_eth_configure(struct octeon_eth_softc *);
static int	octeon_eth_configure_common(struct octeon_eth_softc *);

static void	octeon_eth_tick_free(void *arg);
static void	octeon_eth_tick_misc(void *);

static int	octeon_eth_recv_mbuf(struct octeon_eth_softc *,
			    uint64_t *, struct mbuf **);
static int	octeon_eth_recv_check_code(struct octeon_eth_softc *,
			    uint64_t);
#if 0 /* not used */
static int      octeon_eth_recv_check_jumbo(struct octeon_eth_softc *,
			    uint64_t);
#endif
static int	octeon_eth_recv_check_link(struct octeon_eth_softc *,
			    uint64_t);
static int	octeon_eth_recv_check(struct octeon_eth_softc *,
			    uint64_t);
static int	octeon_eth_recv(struct octeon_eth_softc *, uint64_t *);
static void		octeon_eth_recv_intr(void *, uint64_t *);

/* device driver context */
static struct	octeon_eth_softc *octeon_eth_gsc[GMX_PORT_NUNITS];
static void	*octeon_eth_pow_recv_ih;

/* sysctl'able parameters */
int		octeon_eth_param_pko_cmd_w0_n2 = 1;
int		octeon_eth_param_pip_dyn_rs = 1;
int		octeon_eth_param_redir = 0;
int		octeon_eth_param_pktbuf = 0;
int		octeon_eth_param_rate = 0;
int		octeon_eth_param_intr = 0;

struct cfattach cnmac_ca = {sizeof(struct octeon_eth_softc),
    octeon_eth_match, octeon_eth_attach, NULL, NULL};

struct cfdriver cnmac_cd = {NULL, "cnmac", DV_IFNET};

#ifdef OCTEON_ETH_DEBUG

static const struct octeon_evcnt_entry octeon_evcnt_entries[] = {
#define	_ENTRY(name, type, parent, descr) \
	OCTEON_EVCNT_ENTRY(struct octeon_eth_softc, name, type, parent, descr)
	_ENTRY(rx,			MISC, NULL, "rx"),
	_ENTRY(rxint,			INTR, NULL, "rx intr"),
	_ENTRY(rxrs,			MISC, NULL, "rx dynamic short"),
	_ENTRY(rxbufpkalloc,		MISC, NULL, "rx buf pkt alloc"),
	_ENTRY(rxbufpkput,		MISC, NULL, "rx buf pkt put"),
	_ENTRY(rxbufwqalloc,		MISC, NULL, "rx buf wqe alloc"),
	_ENTRY(rxbufwqput,		MISC, NULL, "rx buf wqe put"),
	_ENTRY(rxerrcode,		MISC, NULL, "rx code error"),
	_ENTRY(rxerrfix,		MISC, NULL, "rx fixup error"),
	_ENTRY(rxerrjmb,		MISC, NULL, "rx jmb error"),
	_ENTRY(rxerrlink,		MISC, NULL, "rx link error"),
	_ENTRY(rxerroff,		MISC, NULL, "rx offload error"),
	_ENTRY(rxonperrshort,		MISC, NULL, "rx onp fixup short error"),
	_ENTRY(rxonperrpreamble,	MISC, NULL, "rx onp fixup preamble error"),
	_ENTRY(rxonperrcrc,		MISC, NULL, "rx onp fixup crc error"),
	_ENTRY(rxonperraddress,		MISC, NULL, "rx onp fixup address error"),
	_ENTRY(rxonponp,		MISC, NULL, "rx onp fixup onp packets"),
	_ENTRY(rxonpok,			MISC, NULL, "rx onp fixup success packets"),
	_ENTRY(tx,			MISC, NULL, "tx"),
	_ENTRY(txadd,			MISC, NULL, "tx add"),
	_ENTRY(txbufcballoc,		MISC, NULL, "tx buf cb alloc"),
	_ENTRY(txbufcbget,		MISC, NULL, "tx buf cb get"),
	_ENTRY(txbufgballoc,		MISC, NULL, "tx buf gb alloc"),
	_ENTRY(txbufgbget,		MISC, NULL, "tx buf gb get"),
	_ENTRY(txbufgbput,		MISC, NULL, "tx buf gb put"),
	_ENTRY(txdel,			MISC, NULL, "tx del"),
	_ENTRY(txerr,			MISC, NULL, "tx error"),
	_ENTRY(txerrcmd,		MISC, NULL, "tx cmd error"),
	_ENTRY(txerrgbuf,		MISC, NULL, "tx gbuf error"),
	_ENTRY(txerrlink,		MISC, NULL, "tx link error"),
	_ENTRY(txerrmkcmd,		MISC, NULL, "tx makecmd error"),
#undef	_ENTRY
};
#endif

/* XXX board-specific */
static const int	octeon_eth_phy_table[] = {
#if defined __seil5__
	0x04, 0x01, 0x02
#else
	0x02, 0x03, 0x22
#endif
};

/* ---- buffer management */

static const struct octeon_eth_pool_param {
	int			poolno;
	size_t			size;
	size_t			nelems;
} octeon_eth_pool_params[] = {
#define	_ENTRY(x)	{ OCTEON_POOL_NO_##x, OCTEON_POOL_SIZE_##x, OCTEON_POOL_NELEMS_##x }
	_ENTRY(PKT),
	_ENTRY(WQE),
	_ENTRY(CMD),
	_ENTRY(SG)
#undef	_ENTRY
};
struct cn30xxfpa_buf	*octeon_eth_pools[8/* XXX */];
#define	octeon_eth_fb_pkt	octeon_eth_pools[OCTEON_POOL_NO_PKT]
#define	octeon_eth_fb_wqe	octeon_eth_pools[OCTEON_POOL_NO_WQE]
#define	octeon_eth_fb_cmd	octeon_eth_pools[OCTEON_POOL_NO_CMD]
#define	octeon_eth_fb_sg	octeon_eth_pools[OCTEON_POOL_NO_SG]

static void
octeon_eth_buf_init(struct octeon_eth_softc *sc)
{
	static int once;
	int i;
	const struct octeon_eth_pool_param *pp;
	struct cn30xxfpa_buf *fb;

	if (once == 1)
		return;
	once = 1;

	for (i = 0; i < (int)nitems(octeon_eth_pool_params); i++) {
		pp = &octeon_eth_pool_params[i];
		cn30xxfpa_buf_init(pp->poolno, pp->size, pp->nelems, &fb);
		octeon_eth_pools[i] = fb;
	}
}

/* ---- autoconf */

static int
octeon_eth_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = (struct cfdata *)match;
	struct cn30xxgmx_attach_args *ga = aux;

	if (strcmp(cf->cf_driver->cd_name, ga->ga_name) != 0) {
		return 0;
	}
	return 1;
}

static void
octeon_eth_attach(struct device *parent, struct device *self, void *aux)
{
	struct octeon_eth_softc *sc = (void *)self;
	struct cn30xxgmx_attach_args *ga = aux;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	uint8_t enaddr[ETHER_ADDR_LEN];

	printf("\n");

	sc->sc_regt = ga->ga_regt;
	sc->sc_dmat = ga->ga_dmat;
	sc->sc_port = ga->ga_portno;
	sc->sc_port_type = ga->ga_port_type;
	sc->sc_gmx = ga->ga_gmx;
	sc->sc_gmx_port = ga->ga_gmx_port;

	sc->sc_init_flag = 0;

	/*
	 * XXX
	 * Setting PIP_IP_OFFSET[OFFSET] to 8 causes panic ... why???
	 */
	sc->sc_ip_offset = 0/* XXX */;

	octeon_eth_board_mac_addr(enaddr, sizeof(enaddr), sc->sc_port);
	printf("%s: Ethernet address %s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(enaddr));

	/*
	 * live lock control notifications.
	 * XXX: use sysctl ???
	 */

	octeon_eth_gsc[sc->sc_port] = sc;

	SIMPLEQ_INIT(&sc->sc_sendq);
	sc->sc_soft_req_thresh = 15/* XXX */;
	sc->sc_ext_callback_cnt = 0;

	cn30xxgmx_stats_init(sc->sc_gmx_port);

	timeout_set(&sc->sc_tick_misc_ch, octeon_eth_tick_misc, sc);
	timeout_set(&sc->sc_tick_free_ch, octeon_eth_tick_free, sc);

	cn30xxfau_op_init(&sc->sc_fau_done,
	    OCTEON_CVMSEG_ETHER_OFFSET(sc->sc_port, csm_ether_fau_done),
	    OCT_FAU_REG_ADDR_END - (8 * (sc->sc_port + 1))/* XXX */);
	cn30xxfau_op_set_8(&sc->sc_fau_done, 0);

	octeon_eth_pip_init(sc);
	octeon_eth_ipd_init(sc);
	octeon_eth_pko_init(sc);
	octeon_eth_asx_init(sc);
	octeon_eth_smi_init(sc);

	sc->sc_gmx_port->sc_ipd = sc->sc_ipd;
	sc->sc_gmx_port->sc_port_asx = sc->sc_asx;
	sc->sc_gmx_port->sc_port_mii = &sc->sc_mii;
	sc->sc_gmx_port->sc_port_ac = &sc->sc_arpcom;

	/* XXX */
	sc->sc_pow = &cn30xxpow_softc;

	octeon_eth_mediainit(sc);

	strncpy(ifp->if_xname, sc->sc_dev.dv_xname, sizeof(ifp->if_xname));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = octeon_eth_ioctl;
	ifp->if_start = octeon_eth_start;
	ifp->if_watchdog = octeon_eth_watchdog;
	ifp->if_stop = octeon_eth_stop; /* XXX */
	IFQ_SET_MAXLEN(&ifp->if_snd, max(GATHER_QUEUE_SIZE, IFQ_MAXLEN));
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capabilities = 0; /* XXX */

	cn30xxgmx_set_mac_addr(sc->sc_gmx_port, enaddr);
	cn30xxgmx_set_filter(sc->sc_gmx_port);

	if_attach(ifp);

	memcpy(sc->sc_arpcom.ac_enaddr, enaddr, ETHER_ADDR_LEN);
	ether_ifattach(ifp);

	/* XXX */
	sc->sc_rate_recv_check_link_cap.tv_sec = 1;
	sc->sc_rate_recv_check_jumbo_cap.tv_sec = 1;
	sc->sc_rate_recv_check_code_cap.tv_sec = 1;

#if 1
	octeon_eth_buf_init(sc);
#endif

	if (octeon_eth_pow_recv_ih == NULL)
		octeon_eth_pow_recv_ih = cn30xxpow_intr_establish(OCTEON_POW_GROUP_PIP,
		    IPL_NET, octeon_eth_recv_intr, NULL, NULL, sc->sc_dev.dv_xname);

#if 0
	/* Make sure the interface is shutdown during reboot. */
	sc->sc_sdhook = shutdownhook_establish(octeon_eth_shutdown, sc);
	if (sc->sc_sdhook == NULL)
		printf("%s: WARNING: unable to establish shutdown hook\n",
		    sc->sc_dev.dv_xname);
#endif

	OCTEON_EVCNT_ATTACH_EVCNTS(sc, octeon_evcnt_entries,
	    sc->sc_dev.dv_xname);
}

/* ---- submodules */

/* XXX */
static void
octeon_eth_pip_init(struct octeon_eth_softc *sc)
{
	struct cn30xxpip_attach_args pip_aa;

	pip_aa.aa_port = sc->sc_port;
	pip_aa.aa_regt = sc->sc_regt;
	pip_aa.aa_tag_type = POW_TAG_TYPE_ORDERED/* XXX */;
	pip_aa.aa_receive_group = OCTEON_POW_GROUP_PIP;
	pip_aa.aa_ip_offset = sc->sc_ip_offset;
	cn30xxpip_init(&pip_aa, &sc->sc_pip);
}

/* XXX */
static void
octeon_eth_ipd_init(struct octeon_eth_softc *sc)
{
	struct cn30xxipd_attach_args ipd_aa;

	ipd_aa.aa_port = sc->sc_port;
	ipd_aa.aa_regt = sc->sc_regt;
	ipd_aa.aa_first_mbuff_skip = 184/* XXX */;
	ipd_aa.aa_not_first_mbuff_skip = 0/* XXX */;
	cn30xxipd_init(&ipd_aa, &sc->sc_ipd);
}

/* XXX */
static void
octeon_eth_pko_init(struct octeon_eth_softc *sc)
{
	struct cn30xxpko_attach_args pko_aa;

	pko_aa.aa_port = sc->sc_port;
	pko_aa.aa_regt = sc->sc_regt;
	pko_aa.aa_cmdptr = &sc->sc_cmdptr;
	pko_aa.aa_cmd_buf_pool = OCTEON_POOL_NO_CMD;
	pko_aa.aa_cmd_buf_size = OCTEON_POOL_NWORDS_CMD;
	cn30xxpko_init(&pko_aa, &sc->sc_pko);
}

/* XXX */
static void
octeon_eth_asx_init(struct octeon_eth_softc *sc)
{
	struct cn30xxasx_attach_args asx_aa;

	asx_aa.aa_port = sc->sc_port;
	asx_aa.aa_regt = sc->sc_regt;
	cn30xxasx_init(&asx_aa, &sc->sc_asx);
}

static void
octeon_eth_smi_init(struct octeon_eth_softc *sc)
{
	struct cn30xxsmi_attach_args smi_aa;

	smi_aa.aa_port = sc->sc_port;
	smi_aa.aa_regt = sc->sc_regt;
	cn30xxsmi_init(&smi_aa, &sc->sc_smi);
	cn30xxsmi_set_clock(sc->sc_smi, 0x1464ULL); /* XXX */
}

/* ---- XXX */

#define	ADDR2UINT64(u, a) \
	do { \
		u = \
		    (((uint64_t)a[0] << 40) | ((uint64_t)a[1] << 32) | \
		     ((uint64_t)a[2] << 24) | ((uint64_t)a[3] << 16) | \
		     ((uint64_t)a[4] <<  8) | ((uint64_t)a[5] <<  0)); \
	} while (0)
#define	UINT642ADDR(a, u) \
	do { \
		a[0] = (uint8_t)((u) >> 40); a[1] = (uint8_t)((u) >> 32); \
		a[2] = (uint8_t)((u) >> 24); a[3] = (uint8_t)((u) >> 16); \
		a[4] = (uint8_t)((u) >>  8); a[5] = (uint8_t)((u) >>  0); \
	} while (0)

static void
octeon_eth_board_mac_addr(uint8_t *enaddr, size_t size, int port)
{
	uint64_t addr;
	int i;

	/* XXX read a mac_dsc tuple from EEPROM */
	for (i = 0; i < size; i++)
		enaddr[i] = i;

	ADDR2UINT64(addr, enaddr);
	addr += port;
	UINT642ADDR(enaddr, addr);
}

/* ---- media */

static int
octeon_eth_mii_readreg(struct device *self, int phy_no, int reg)
{
	struct octeon_eth_softc *sc = (struct octeon_eth_softc *)self;
	int phy_addr = octeon_eth_phy_table[phy_no];

	if (sc->sc_port >= (int)nitems(octeon_eth_phy_table) ||
	    phy_no != sc->sc_port) {
		log(LOG_ERR,
		    "mii read address is mismatch, phy number %d.\n", phy_no);
		return -1;
	}
	return cn30xxsmi_read(sc->sc_smi, phy_addr, reg);
}

static void
octeon_eth_mii_writereg(struct device *self, int phy_no, int reg, int value)
{
	struct octeon_eth_softc *sc = (struct octeon_eth_softc *)self;
	int phy_addr = octeon_eth_phy_table[phy_no];

	if (sc->sc_port >= (int)nitems(octeon_eth_phy_table) ||
	    phy_no != sc->sc_port) {
		log(LOG_ERR,
		    "mii write address is mismatch, phy number %d.\n", phy_no);
		return;
	}
	cn30xxsmi_write(sc->sc_smi, phy_addr, reg, value);
}

static void
octeon_eth_mii_statchg(struct device *self)
{
	struct octeon_eth_softc *sc = (struct octeon_eth_softc *)self;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	cn30xxpko_port_enable(sc->sc_pko, 0);
	cn30xxgmx_port_enable(sc->sc_gmx_port, 0);

	octeon_eth_reset(sc);

	if (ISSET(ifp->if_flags, IFF_RUNNING))
		cn30xxgmx_set_filter(sc->sc_gmx_port);

	cn30xxpko_port_enable(sc->sc_pko, 1);
	cn30xxgmx_port_enable(sc->sc_gmx_port, 1);
}

static int
octeon_eth_mediainit(struct octeon_eth_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = octeon_eth_mii_readreg;
	sc->sc_mii.mii_writereg = octeon_eth_mii_writereg;
	sc->sc_mii.mii_statchg = octeon_eth_mii_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, octeon_eth_mediachange,
	    octeon_eth_mediastatus);

	mii_attach(&sc->sc_dev, &sc->sc_mii,
	    0xffffffff, sc->sc_port, MII_OFFSET_ANY, MIIF_DOPAUSE);

	/* XXX */
	if (LIST_FIRST(&sc->sc_mii.mii_phys) != NULL) {
		/* XXX */
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER | IFM_AUTO);
	} else {
		/* XXX */
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER | IFM_NONE,
		    MII_MEDIA_NONE, NULL);
		/* XXX */
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER | IFM_NONE);
	}

	return 0;
}

static void
octeon_eth_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct octeon_eth_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_active = (sc->sc_mii.mii_media_active & ~IFM_ETH_FMASK) |
	    sc->sc_gmx_port->sc_port_flowflags;
}

static int
octeon_eth_mediachange(struct ifnet *ifp)
{
	struct octeon_eth_softc *sc = ifp->if_softc;

	mii_mediachg(&sc->sc_mii);

	return 0;
}

/* ---- send buffer garbage collection */

static void
octeon_eth_send_queue_flush_prefetch(struct octeon_eth_softc *sc)
{
	OCTEON_ETH_KASSERT(sc->sc_prefetch == 0);
	cn30xxfau_op_inc_fetch_8(&sc->sc_fau_done, 0);
	sc->sc_prefetch = 1;
}

static void
octeon_eth_send_queue_flush_fetch(struct octeon_eth_softc *sc)
{
#ifndef  OCTEON_ETH_DEBUG
	if (!sc->sc_prefetch)
		return;
#endif
	OCTEON_ETH_KASSERT(sc->sc_prefetch == 1);
	sc->sc_hard_done_cnt = cn30xxfau_op_inc_read_8(&sc->sc_fau_done);
	OCTEON_ETH_KASSERT(sc->sc_hard_done_cnt <= 0);
	sc->sc_prefetch = 0;
}

static void
octeon_eth_send_queue_flush(struct octeon_eth_softc *sc)
{
	const int64_t sent_count = sc->sc_hard_done_cnt;
	int i;

	OCTEON_ETH_KASSERT(sc->sc_flush == 0);
	OCTEON_ETH_KASSERT(sent_count <= 0);

	for (i = 0; i < 0 - sent_count; i++) {
		struct mbuf *m;
		uint64_t *gbuf;

		octeon_eth_send_queue_del(sc, &m, &gbuf);

		cn30xxfpa_buf_put_paddr(octeon_eth_fb_sg, CKSEG0_TO_PHYS(gbuf));
		OCTEON_EVCNT_INC(sc, txbufgbput);

		m_freem(m);
	}

	cn30xxfau_op_inc_fetch_8(&sc->sc_fau_done, i);
	sc->sc_flush = i;
}

static void
octeon_eth_send_queue_flush_sync(struct octeon_eth_softc *sc)
{
	if (sc->sc_flush == 0)
		return;

	OCTEON_ETH_KASSERT(sc->sc_flush > 0);

	/* XXX */
	cn30xxfau_op_inc_read_8(&sc->sc_fau_done);
	sc->sc_soft_req_cnt -= sc->sc_flush;
	OCTEON_ETH_KASSERT(sc->sc_soft_req_cnt >= 0);
	/* XXX */

	sc->sc_flush = 0;
}

static int
octeon_eth_send_queue_is_full(struct octeon_eth_softc *sc)
{
#ifdef OCTEON_ETH_SEND_QUEUE_CHECK
	int64_t nofree_cnt;

	nofree_cnt = sc->sc_soft_req_cnt + sc->sc_hard_done_cnt; 

	if (__predict_false(nofree_cnt == GATHER_QUEUE_SIZE - 1)) {
		octeon_eth_send_queue_flush(sc);
		OCTEON_EVCNT_INC(sc, txerrgbuf);
		octeon_eth_send_queue_flush_sync(sc);
		return 1;
	}

#endif
	return 0;
}

/*
 * (Ab)use m_nextpkt and m_paddr to maintain mbuf chain and pointer to gather
 * buffer.  Other mbuf members may be used by m_freem(), so don't touch them!
 */

struct _send_queue_entry {
	union {
		struct mbuf _sqe_s_mbuf;
		struct {
			char _sqe_s_entry_pad[offsetof(struct mbuf, m_nextpkt)];
			SIMPLEQ_ENTRY(_send_queue_entry) _sqe_s_entry_entry;
		} _sqe_s_entry;
		struct {
			char _sqe_s_gbuf_pad[offsetof(struct mbuf, M_dat.MH.MH_pkthdr.rcvif)];
			uint64_t *_sqe_s_gbuf_gbuf;
		} _sqe_s_gbuf;
	} _sqe_u;
#define	_sqe_entry	_sqe_u._sqe_s_entry._sqe_s_entry_entry
#define	_sqe_gbuf	_sqe_u._sqe_s_gbuf._sqe_s_gbuf_gbuf
};

static void
octeon_eth_send_queue_add(struct octeon_eth_softc *sc, struct mbuf *m,
    uint64_t *gbuf)
{
	struct _send_queue_entry *sqe = (struct _send_queue_entry *)m;

	OCTEON_ETH_KASSERT(m->m_flags & M_PKTHDR);

	sqe->_sqe_gbuf = gbuf;
	SIMPLEQ_INSERT_TAIL(&sc->sc_sendq, sqe, _sqe_entry);

	if (m->m_ext.ext_free != NULL)
		sc->sc_ext_callback_cnt++;

	OCTEON_EVCNT_INC(sc, txadd);
}

static void
octeon_eth_send_queue_del(struct octeon_eth_softc *sc, struct mbuf **rm,
    uint64_t **rgbuf)
{
	struct _send_queue_entry *sqe;

	sqe = SIMPLEQ_FIRST(&sc->sc_sendq);
	OCTEON_ETH_KASSERT(sqe != NULL);
	SIMPLEQ_REMOVE_HEAD(&sc->sc_sendq, _sqe_entry);

	*rm = (void *)sqe;
	*rgbuf = sqe->_sqe_gbuf;

	if ((*rm)->m_ext.ext_free != NULL) {
		sc->sc_ext_callback_cnt--;
		OCTEON_ETH_KASSERT(sc->sc_ext_callback_cnt >= 0);
	}

	OCTEON_EVCNT_INC(sc, txdel);
}

static int
octeon_eth_buf_free_work(struct octeon_eth_softc *sc, uint64_t *work,
    uint64_t word2)
{
	/* XXX when jumbo frame */
	if (ISSET(word2, PIP_WQE_WORD2_IP_BUFS)) {
		paddr_t addr;
		paddr_t start_buffer;

		addr = CKSEG0_TO_PHYS(work[3] & PIP_WQE_WORD3_ADDR);
		start_buffer = addr & ~(2048 - 1);

		cn30xxfpa_buf_put_paddr(octeon_eth_fb_pkt, start_buffer);
		OCTEON_EVCNT_INC(sc, rxbufpkput);
	}

	cn30xxfpa_buf_put_paddr(octeon_eth_fb_wqe, CKSEG0_TO_PHYS(work));
	OCTEON_EVCNT_INC(sc, rxbufwqput);

	return 0;
}

static void
octeon_eth_buf_ext_free_m(caddr_t buf, u_int size, void *arg)
{
	uint64_t *work = (void *)arg;
#ifdef OCTEON_ETH_DEBUG
	struct octeon_eth_softc *sc = (void *)(uintptr_t)work[0];
#endif
	int s = splnet();

	OCTEON_EVCNT_INC(sc, rxrs);

	cn30xxfpa_buf_put_paddr(octeon_eth_fb_wqe, CKSEG0_TO_PHYS(work));
	OCTEON_EVCNT_INC(sc, rxbufwqput);

	splx(s);
}

static void
octeon_eth_buf_ext_free_ext(caddr_t buf, u_int size,
    void *arg)
{
	uint64_t *work = (void *)arg;
#ifdef OCTEON_ETH_DEBUG
	struct octeon_eth_softc *sc = (void *)(uintptr_t)work[0];
#endif
	int s = splnet();

	cn30xxfpa_buf_put_paddr(octeon_eth_fb_wqe, CKSEG0_TO_PHYS(work));
	OCTEON_EVCNT_INC(sc, rxbufwqput);

	cn30xxfpa_buf_put_paddr(octeon_eth_fb_pkt, CKSEG0_TO_PHYS(buf));
	OCTEON_EVCNT_INC(sc, rxbufpkput);

	splx(s);
}

/* ---- ifnet interfaces */

static int
octeon_eth_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct octeon_eth_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			octeon_eth_init(ifp);
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_arpcom, ifa);
#endif
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				octeon_eth_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				octeon_eth_stop(ifp, 0);
		}
		break;

	case SIOCSIFMEDIA:
		/* Flow control requires full-duplex mode. */
		if (IFM_SUBTYPE(ifr->ifr_media) == IFM_AUTO ||
		    (ifr->ifr_media & IFM_FDX) == 0) {
			ifr->ifr_media &= ~IFM_ETH_FMASK;
		}
		if (IFM_SUBTYPE(ifr->ifr_media) != IFM_AUTO) {
			if ((ifr->ifr_media & IFM_ETH_FMASK) == IFM_FLOW) {
				ifr->ifr_media |=
				    IFM_ETH_TXPAUSE | IFM_ETH_RXPAUSE;
			}
			sc->sc_gmx_port->sc_port_flowflags = 
				ifr->ifr_media & IFM_ETH_FMASK;
		}
		/* FALLTHROUGH */
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data);
	}

	if (error == ENETRESET) {
		if (ISSET(ifp->if_flags, IFF_RUNNING))
			cn30xxgmx_set_filter(sc->sc_gmx_port);
		error = 0;
	}

	octeon_eth_start(ifp);

	splx(s);
	return (error);
}

/* ---- send (output) */

static uint64_t
octeon_eth_send_makecmd_w0(uint64_t fau0, uint64_t fau1, size_t len, int segs)
{
	return cn30xxpko_cmd_word0(
		OCT_FAU_OP_SIZE_64,		/* sz1 */
		OCT_FAU_OP_SIZE_64,		/* sz0 */
		1, fau1, 1, fau0,		/* s1, reg1, s0, reg0 */
		0,				/* le */
		octeon_eth_param_pko_cmd_w0_n2,	/* n2 */
		1, 0,				/* q, r */
		(segs == 1) ? 0 : 1,		/* g */
		0, 0, 1,			/* ipoffp1, ii, df */
		segs, (int)len);		/* segs, totalbytes */
}

static uint64_t 
octeon_eth_send_makecmd_w1(int size, paddr_t addr)
{
	return cn30xxpko_cmd_word1(
		0, 0,				/* i, back */
		FPA_GATHER_BUFFER_POOL,		/* pool */
		size, addr);			/* size, addr */
}

/* TODO: use bus_dma(9) */

#define KVTOPHYS(addr)	if_cnmac_kvtophys((vaddr_t)(addr))
paddr_t if_cnmac_kvtophys(vaddr_t);

paddr_t
if_cnmac_kvtophys(vaddr_t kva)
{
	if (IS_XKPHYS(kva))
		return XKPHYS_TO_PHYS(kva);
	else if (kva >= CKSEG0_BASE && kva < CKSEG0_BASE + CKSEG_SIZE)
		return CKSEG0_TO_PHYS(kva);
	else if (kva >= CKSEG1_BASE && kva < CKSEG1_BASE + CKSEG_SIZE)
		return CKSEG1_TO_PHYS(kva);

	printf("kva %p is not be able to convert physical address\n", kva);
	panic("if_cnmac_kvtophys");
}

static int
octeon_eth_send_makecmd_gbuf(struct octeon_eth_softc *sc, struct mbuf *m0,
    uint64_t *gbuf, int *rsegs)
{
	struct mbuf *m;
	int segs = 0;
	uint32_t laddr, rlen, nlen;

	for (m = m0; m != NULL; m = m->m_next) {

		if (__predict_false(m->m_len == 0))
			continue;

#if 0	
		OCTEON_ETH_KASSERT(((uint32_t)m->m_data & (PAGE_SIZE - 1))
		   == (kvtophys((vaddr_t)m->m_data) & (PAGE_SIZE - 1)));
#endif

		/*
		 * aligned 4k
		 */
		laddr = (uintptr_t)m->m_data & (PAGE_SIZE - 1);

		if (laddr + m->m_len > PAGE_SIZE) {
			/* XXX */
			rlen = PAGE_SIZE - laddr;
			nlen = m->m_len - rlen;
			*(gbuf + segs) = octeon_eth_send_makecmd_w1(rlen,
			    KVTOPHYS(m->m_data));
			segs++;
			if (segs > 63) {
				return 1;
			}
			/* XXX */
		} else {
			rlen = 0;
			nlen = m->m_len;
		}

		*(gbuf + segs) = octeon_eth_send_makecmd_w1(nlen,
		    KVTOPHYS((caddr_t)m->m_data + rlen));
		segs++;
		if (segs > 63) {
			return 1;
		}
	}

	OCTEON_ETH_KASSERT(m == NULL);

	*rsegs = segs;

	return 0;
}

static int
octeon_eth_send_makecmd(struct octeon_eth_softc *sc, struct mbuf *m,
    uint64_t *gbuf, uint64_t *rpko_cmd_w0, uint64_t *rpko_cmd_w1)
{
	uint64_t pko_cmd_w0, pko_cmd_w1;
	int segs;
	int result = 0;

	if (octeon_eth_send_makecmd_gbuf(sc, m, gbuf, &segs)) {
		log(LOG_WARNING, "%s: there are a lot of number of segments"
		    " of transmission data", sc->sc_dev.dv_xname);
		result = 1;
		goto done;
	}

	/*
	 * segs == 1	-> link mode (single continuous buffer)
	 *		   WORD1[size] is number of bytes pointed by segment
	 *
	 * segs > 1	-> gather mode (scatter-gather buffer)
	 *		   WORD1[size] is number of segments
	 */
	pko_cmd_w0 = octeon_eth_send_makecmd_w0(sc->sc_fau_done.fd_regno,
	    0, m->m_pkthdr.len, segs);
	pko_cmd_w1 = octeon_eth_send_makecmd_w1(
	    (segs == 1) ? m->m_pkthdr.len : segs,
	    (segs == 1) ? 
		KVTOPHYS(m->m_data) :
		CKSEG0_TO_PHYS(gbuf));

	*rpko_cmd_w0 = pko_cmd_w0;
	*rpko_cmd_w1 = pko_cmd_w1;

done:
	return result;
}

static int
octeon_eth_send_cmd(struct octeon_eth_softc *sc, uint64_t pko_cmd_w0,
    uint64_t pko_cmd_w1)
{
	uint64_t *cmdptr;
	int result = 0;

	cmdptr = (uint64_t *)PHYS_TO_CKSEG0(sc->sc_cmdptr.cmdptr);
	cmdptr += sc->sc_cmdptr.cmdptr_idx;

	OCTEON_ETH_KASSERT(cmdptr != NULL);

	*cmdptr++ = pko_cmd_w0;
	*cmdptr++ = pko_cmd_w1;

	OCTEON_ETH_KASSERT(sc->sc_cmdptr.cmdptr_idx + 2 <= FPA_COMMAND_BUFFER_POOL_NWORDS - 1);

	if (sc->sc_cmdptr.cmdptr_idx + 2 == FPA_COMMAND_BUFFER_POOL_NWORDS - 1) {
		paddr_t buf;

		buf = cn30xxfpa_buf_get_paddr(octeon_eth_fb_cmd);
		if (buf == 0) {
			log(LOG_WARNING,
			    "%s: can not allocate command buffer from free pool allocator\n",
			    sc->sc_dev.dv_xname);
			result = 1;
			goto done;
		}
		OCTEON_EVCNT_INC(sc, txbufcbget);
		*cmdptr++ = buf;
		sc->sc_cmdptr.cmdptr = (uint64_t)buf;
		sc->sc_cmdptr.cmdptr_idx = 0;
	} else {
		sc->sc_cmdptr.cmdptr_idx += 2;
	}

	cn30xxpko_op_doorbell_write(sc->sc_port, sc->sc_port, 2);

done:
	return result;
}

static int
octeon_eth_send_buf(struct octeon_eth_softc *sc, struct mbuf *m,
    uint64_t *gbuf)
{
	int result = 0, error;
	uint64_t pko_cmd_w0, pko_cmd_w1;

	error = octeon_eth_send_makecmd(sc, m, gbuf, &pko_cmd_w0, &pko_cmd_w1);
	if (error != 0) {
		/* already logging */
		OCTEON_EVCNT_INC(sc, txerrmkcmd);
		result = error;
		goto done;
	}

	error = octeon_eth_send_cmd(sc, pko_cmd_w0, pko_cmd_w1);
	if (error != 0) {
		/* already logging */
		OCTEON_EVCNT_INC(sc, txerrcmd);
		result = error;
	}

done:
	return result;
}

static int
octeon_eth_send(struct octeon_eth_softc *sc, struct mbuf *m)
{
	paddr_t gaddr = 0;
	uint64_t *gbuf = NULL;
	int result = 0, error;

	OCTEON_EVCNT_INC(sc, tx);

	gaddr = cn30xxfpa_buf_get_paddr(octeon_eth_fb_sg);
	if (gaddr == 0) {
		log(LOG_WARNING,
		    "%s: can not allocate gather buffer from free pool allocator\n",
		    sc->sc_dev.dv_xname);
		OCTEON_EVCNT_INC(sc, txerrgbuf);
		result = 1;
		goto done;
	}
	OCTEON_EVCNT_INC(sc, txbufgbget);

	gbuf = (uint64_t *)(uintptr_t)PHYS_TO_CKSEG0(gaddr);

	OCTEON_ETH_KASSERT(gbuf != NULL);

	error = octeon_eth_send_buf(sc, m, gbuf);
	if (error != 0) {
		/* already logging */
		cn30xxfpa_buf_put_paddr(octeon_eth_fb_sg, gaddr);
		OCTEON_EVCNT_INC(sc, txbufgbput);
		result = error;
		goto done;
	}

	octeon_eth_send_queue_add(sc, m, gbuf);

done:
	return result;
}

static void
octeon_eth_start(struct ifnet *ifp)
{
	struct octeon_eth_softc *sc = ifp->if_softc;
	struct mbuf *m;

	/*
	 * performance tuning
	 * presend iobdma request 
	 */
	octeon_eth_send_queue_flush_prefetch(sc);

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		goto last;

	/* XXX assume that OCTEON doesn't buffer packets */
	if (__predict_false(!cn30xxgmx_link_status(sc->sc_gmx_port))) {
		/* dequeue and drop them */
		while (1) {
			IFQ_DEQUEUE(&ifp->if_snd, m);
			if (m == NULL)
				break;
#if 0
#ifdef DDB
			m_print(m, "cd", printf);
#endif
			printf("%s: drop\n", sc->sc_dev.dv_xname);
#endif
			m_freem(m);
			IF_DROP(&ifp->if_snd);
			OCTEON_EVCNT_INC(sc, txerrlink);
		}
		goto last;
	}

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m);
		if (__predict_false(m == NULL))
			break;

		octeon_eth_send_queue_flush_fetch(sc); /* XXX */

		/*
		 * XXXSEIL
		 * If no free send buffer is available, free all the sent buffer
		 * and bail out.
		 */
		if (octeon_eth_send_queue_is_full(sc)) {
			return;
		}
		/* XXX */

		IFQ_DEQUEUE(&ifp->if_snd, m);

		OCTEON_ETH_TAP(ifp, m, BPF_DIRECTION_OUT);

		/* XXX */
		if (sc->sc_soft_req_cnt > sc->sc_soft_req_thresh)
			octeon_eth_send_queue_flush(sc);
		if (octeon_eth_send(sc, m)) {
			ifp->if_oerrors++;
			m_freem(m);
			log(LOG_WARNING,
		  	  "%s: failed in the transmission of the packet\n",
		    	  sc->sc_dev.dv_xname);
			OCTEON_EVCNT_INC(sc, txerr);
		} else {
			sc->sc_soft_req_cnt++;
		}
		if (sc->sc_flush)
			octeon_eth_send_queue_flush_sync(sc);
		/* XXX */

		/*
		 * send next iobdma request 
		 */
		octeon_eth_send_queue_flush_prefetch(sc);
	}

/*
 * XXXSEIL
 * Don't schedule send-buffer-free callout every time - those buffers are freed
 * by "free tick".  This makes some packets like NFS slower, but it normally
 * doesn't happen on SEIL.
 */
#ifdef OCTEON_ETH_USENFS
	if (__predict_false(sc->sc_ext_callback_cnt > 0)) {
		int timo;

		/* ??? */
		timo = hz - (100 * sc->sc_ext_callback_cnt);
		if (timo < 10)
			timo = 10;
		callout_schedule(&sc->sc_tick_free_ch, timo);
	}
#endif

last:
	octeon_eth_send_queue_flush_fetch(sc);
}

static void
octeon_eth_watchdog(struct ifnet *ifp)
{
	struct octeon_eth_softc *sc = ifp->if_softc;

	printf("%s: device timeout\n", sc->sc_dev.dv_xname);

	octeon_eth_configure(sc);

	SET(ifp->if_flags, IFF_RUNNING);
	CLR(ifp->if_flags, IFF_OACTIVE);
	ifp->if_timer = 0;

	octeon_eth_start(ifp);
}

static int
octeon_eth_init(struct ifnet *ifp)
{
	struct octeon_eth_softc *sc = ifp->if_softc;

	/* XXX don't disable commonly used parts!!! XXX */
	if (sc->sc_init_flag == 0) {
		/* Cancel any pending I/O. */
		octeon_eth_stop(ifp, 0);

		/* Initialize the device */
		octeon_eth_configure(sc);

		cn30xxpko_enable(sc->sc_pko);
		cn30xxipd_enable(sc->sc_ipd);

		sc->sc_init_flag = 1;
	} else {
		cn30xxgmx_port_enable(sc->sc_gmx_port, 1);
	}
	octeon_eth_mediachange(ifp);

	cn30xxgmx_set_filter(sc->sc_gmx_port);

	timeout_add_sec(&sc->sc_tick_misc_ch, 1);
	timeout_add_sec(&sc->sc_tick_free_ch, 1);

	SET(ifp->if_flags, IFF_RUNNING);
	CLR(ifp->if_flags, IFF_OACTIVE);

	return 0;
}

static int
octeon_eth_stop(struct ifnet *ifp, int disable)
{
	struct octeon_eth_softc *sc = ifp->if_softc;

	timeout_del(&sc->sc_tick_misc_ch);
	timeout_del(&sc->sc_tick_free_ch);
	timeout_del(&sc->sc_resume_ch);

	mii_down(&sc->sc_mii);

	cn30xxgmx_port_enable(sc->sc_gmx_port, 0);

	/* Mark the interface as down and cancel the watchdog timer. */
	CLR(ifp->if_flags, IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	return 0;
}

/* ---- misc */

#define PKO_INDEX_MASK	((1ULL << 12/* XXX */) - 1)

static int
octeon_eth_reset(struct octeon_eth_softc *sc)
{
	cn30xxgmx_reset_speed(sc->sc_gmx_port);
	cn30xxgmx_reset_flowctl(sc->sc_gmx_port);
	cn30xxgmx_reset_timing(sc->sc_gmx_port);
	cn30xxgmx_reset_board(sc->sc_gmx_port);

	return 0;
}

static int
octeon_eth_configure(struct octeon_eth_softc *sc)
{
	cn30xxgmx_port_enable(sc->sc_gmx_port, 0);

	octeon_eth_reset(sc);

	octeon_eth_configure_common(sc);

	cn30xxpko_port_config(sc->sc_pko);
	cn30xxpko_port_enable(sc->sc_pko, 1);
	cn30xxpip_port_config(sc->sc_pip);

	cn30xxgmx_tx_stats_rd_clr(sc->sc_gmx_port, 1);
	cn30xxgmx_rx_stats_rd_clr(sc->sc_gmx_port, 1);

	cn30xxgmx_port_enable(sc->sc_gmx_port, 1);

	return 0;
}

static int
octeon_eth_configure_common(struct octeon_eth_softc *sc)
{
	static int once;

	if (once == 1)
		return 0;
	once = 1;

#if 0
	octeon_eth_buf_init(sc);
#endif

	cn30xxipd_config(sc->sc_ipd);
	cn30xxpko_config(sc->sc_pko);

	cn30xxpow_config(sc->sc_pow, OCTEON_POW_GROUP_PIP);

	return 0;
}

static int
octeon_eth_recv_mbuf(struct octeon_eth_softc *sc, uint64_t *work,
    struct mbuf **rm)
{
	struct mbuf *m;
	void (*ext_free)(caddr_t, u_int, void *);
	void *ext_buf;
	size_t ext_size;
	void *data;
	uint64_t word1 = work[1];
	uint64_t word2 = work[2];
	uint64_t word3 = work[3];

	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL)
		return 1;
	OCTEON_ETH_KASSERT(m != NULL);

	if ((word2 & PIP_WQE_WORD2_IP_BUFS) == 0) {
		/* Dynamic short */
		ext_free = octeon_eth_buf_ext_free_m;
		ext_buf = &work[4];
		ext_size = 96;

		data = &work[4 + sc->sc_ip_offset / sizeof(uint64_t)];
	} else {
		vaddr_t addr;
		vaddr_t start_buffer;

		addr = PHYS_TO_CKSEG0(word3 & PIP_WQE_WORD3_ADDR);
		start_buffer = addr & ~(2048 - 1);

		ext_free = octeon_eth_buf_ext_free_ext;
		ext_buf = (void *)start_buffer;
		ext_size = 2048;

		data = (void *)addr;
	}

	/* embed sc pointer into work[0] for _ext_free evcnt */
	work[0] = (uintptr_t)sc;

	MEXTADD(m, ext_buf, ext_size, 0, ext_free, work);
	OCTEON_ETH_KASSERT(ISSET(m->m_flags, M_EXT));

	m->m_data = data;
	m->m_len = m->m_pkthdr.len = (word1 & PIP_WQE_WORD1_LEN) >> 48;
	m->m_pkthdr.rcvif = &sc->sc_arpcom.ac_if;
#if 0
	/*
	 * not readonly buffer
	 */
	m->m_flags |= M_EXT_RW;
#endif

	*rm = m;

	OCTEON_ETH_KASSERT(*rm != NULL);

	return 0;
}

static int
octeon_eth_recv_check_code(struct octeon_eth_softc *sc, uint64_t word2)
{
	uint64_t opecode = word2 & PIP_WQE_WORD2_NOIP_OPECODE;

	if (__predict_true(!ISSET(word2, PIP_WQE_WORD2_NOIP_RE)))
		return 0;

	/* this error is harmless */
	if (opecode == PIP_OVER_ERR)
		return 0;

	return 1;
}

#if 0 /* not used */
static int
octeon_eth_recv_check_jumbo(struct octeon_eth_softc *sc, uint64_t word2)
{
	if (__predict_false((word2 & PIP_WQE_WORD2_IP_BUFS) > (1ULL << 56)))
		return 1;
	return 0;
}
#endif

static int
octeon_eth_recv_check_link(struct octeon_eth_softc *sc, uint64_t word2)
{
	if (__predict_false(!cn30xxgmx_link_status(sc->sc_gmx_port)))
		return 1;
	return 0;
}

static int
octeon_eth_recv_check(struct octeon_eth_softc *sc, uint64_t word2)
{
	if (__predict_false(octeon_eth_recv_check_link(sc, word2)) != 0) {
		if (ratecheck(&sc->sc_rate_recv_check_link_last,
		    &sc->sc_rate_recv_check_link_cap))
			log(LOG_DEBUG,
			    "%s: link is not up, the packet was dropped\n",
			    sc->sc_dev.dv_xname);
		OCTEON_EVCNT_INC(sc, rxerrlink);
		return 1;
	}

#if 0 /* XXX Performance tunig (Jumbo-frame is not supported yet!) */
	if (__predict_false(octeon_eth_recv_check_jumbo(sc, word2)) != 0) {
		/* XXX jumbo frame */
		if (ratecheck(&sc->sc_rate_recv_check_jumbo_last,
		    &sc->sc_rate_recv_check_jumbo_cap))
			log(LOG_DEBUG,
			    "jumbo frame was received\n");
		OCTEON_EVCNT_INC(sc, rxerrjmb);
		return 1;
	}
#endif

	if (__predict_false(octeon_eth_recv_check_code(sc, word2)) != 0) {
		if ((word2 & PIP_WQE_WORD2_NOIP_OPECODE) == PIP_WQE_WORD2_RE_OPCODE_LENGTH) {
			/* no logging */
			/* XXX inclement special error count */
		} else if ((word2 & PIP_WQE_WORD2_NOIP_OPECODE) == 
				PIP_WQE_WORD2_RE_OPCODE_PARTIAL) {
			/* not an erorr. it's because of overload */
		}
		else {
			if (ratecheck(&sc->sc_rate_recv_check_code_last,
			    &sc->sc_rate_recv_check_code_cap)) 
				log(LOG_WARNING,
				    "%s: the reception error had occured, "
				    "the packet was dropped (error code = %lld)\n",
				    sc->sc_dev.dv_xname, word2 & PIP_WQE_WORD2_NOIP_OPECODE);
		}
		OCTEON_EVCNT_INC(sc, rxerrcode);
		return 1;
	}

	return 0;
}

static int
octeon_eth_recv(struct octeon_eth_softc *sc, uint64_t *work)
{
	int result = 0;
	struct ifnet *ifp;
	struct mbuf *m;
	uint64_t word2;

	/* XXX */
	/*
 	 * performance tuning
	 * presend iobdma request
	 */
	if (sc->sc_soft_req_cnt > sc->sc_soft_req_thresh) {
		octeon_eth_send_queue_flush_prefetch(sc);
	}
	/* XXX */

	OCTEON_ETH_KASSERT(sc != NULL);
	OCTEON_ETH_KASSERT(work != NULL);

	OCTEON_EVCNT_INC(sc, rx);

	word2 = work[2];
	ifp = &sc->sc_arpcom.ac_if;

	OCTEON_ETH_KASSERT(ifp != NULL);

	if (__predict_false(octeon_eth_recv_check(sc, word2) != 0)) {
		ifp->if_ierrors++;
		result = 1;
		octeon_eth_buf_free_work(sc, work, word2);
		goto drop;
	}

	if (__predict_false(octeon_eth_recv_mbuf(sc, work, &m) != 0)) {
		ifp->if_ierrors++;
		result = 1;
		octeon_eth_buf_free_work(sc, work, word2);
		goto drop;
	}

	/* work[0] .. work[3] may not be valid any more */

	OCTEON_ETH_KASSERT(m != NULL);

	cn30xxipd_offload(word2, m->m_data, &m->m_pkthdr.csum_flags);

	/* XXX */
	if (sc->sc_soft_req_cnt > sc->sc_soft_req_thresh) {
		octeon_eth_send_queue_flush_fetch(sc);
		octeon_eth_send_queue_flush(sc);
	}
	/* XXX */

	OCTEON_ETH_TAP(ifp, m, BPF_DIRECTION_IN);

	/* XXX */
	if (sc->sc_flush)
		octeon_eth_send_queue_flush_sync(sc);
	/* XXX */

	ether_input_mbuf(ifp, m);

	return 0;

drop:
	/* XXX */
	if (sc->sc_soft_req_cnt > sc->sc_soft_req_thresh) {
		octeon_eth_send_queue_flush_fetch(sc);
	}
	/* XXX */

	return result;
}

static void
octeon_eth_recv_intr(void *data, uint64_t *work)
{
	struct octeon_eth_softc *sc;
	int port;

	OCTEON_ETH_KASSERT(work != NULL);

	port = (work[1] & PIP_WQE_WORD1_IPRT) >> 42;

	OCTEON_ETH_KASSERT(port < GMX_PORT_NUNITS);

	sc = octeon_eth_gsc[port];

	OCTEON_ETH_KASSERT(sc != NULL);
	OCTEON_ETH_KASSERT(port == sc->sc_port);

	/* XXX process all work queue entries anyway */

	(void)octeon_eth_recv(sc, work);
}

/* ---- tick */

/*
 * octeon_eth_tick_free
 *
 * => garbage collect send gather buffer / mbuf
 * => called at softclock
 */
static void
octeon_eth_tick_free(void *arg)
{
	struct octeon_eth_softc *sc = arg;
	int timo;
	int s;

	s = splnet();
	/* XXX */
	if (sc->sc_soft_req_cnt > 0) {
		octeon_eth_send_queue_flush_prefetch(sc);
		octeon_eth_send_queue_flush_fetch(sc);
		octeon_eth_send_queue_flush(sc);
		octeon_eth_send_queue_flush_sync(sc);
	}
	/* XXX */

	/* XXX ??? */
	timo = hz - (100 * sc->sc_ext_callback_cnt);
	if (timo < 10)
		 timo = 10;
	timeout_add_msec(&sc->sc_tick_free_ch, 1000 * timo / hz);
	/* XXX */
	splx(s);
}

/*
 * octeon_eth_tick_misc
 *
 * => collect statistics
 * => check link status
 * => called at softclock
 */
static void
octeon_eth_tick_misc(void *arg)
{
	struct octeon_eth_softc *sc = arg;
	struct ifnet *ifp;
	u_quad_t iqdrops, delta;
	int s;

	s = splnet();

	ifp = &sc->sc_arpcom.ac_if;

	iqdrops = ifp->if_iqdrops;
	cn30xxgmx_stats(sc->sc_gmx_port);
#ifdef OCTEON_ETH_DEBUG
	delta = ifp->if_iqdrops - iqdrops;
	printf("%s: %qu packets dropped at GMX FIFO\n",
			ifp->if_xname, delta);
#endif
	cn30xxpip_stats(sc->sc_pip, ifp, sc->sc_port);
	delta = ifp->if_iqdrops - iqdrops;
#ifdef OCTEON_ETH_DEBUG
	printf("%s: %qu packets dropped at PIP + GMX FIFO\n",
			ifp->if_xname, delta);
#endif

	mii_tick(&sc->sc_mii);

#ifdef OCTEON_ETH_FIXUP_ODD_NIBBLE_DYNAMIC
	if (sc->sc_gmx_port->sc_proc_nibble_by_soft &&
	    sc->sc_gmx_port->sc_even_nibble_cnt > PROC_NIBBLE_SOFT_THRESHOLD) {
#ifdef OCTEON_ETH_DEBUG
		log(LOG_DEBUG, "%s: even nibble preamble count %d\n",
		    sc->sc_dev.dv_xname, sc->sc_gmx_port->sc_even_nibble_cnt);
#endif
		if (OCTEON_ETH_FIXUP_ODD_NIBBLE_MODEL_P(sc) &&
		    OCTEON_ETH_FIXUP_ODD_NIBBLE_DYNAMIC_SPEED_P(sc->sc_gmx_port, ifp)) {
			log(LOG_NOTICE, 
			    "%s: the preamble processing is switched to hardware\n", 
			    sc->sc_dev.dv_xname);
		}
		sc->sc_gmx_port->sc_proc_nibble_by_soft = 0;
		octeon_eth_mii_statchg((struct device *)sc);
		sc->sc_gmx_port->sc_even_nibble_cnt = 0;
	}
#endif
	splx(s);

	timeout_add_sec(&sc->sc_tick_misc_ch, 1);
}
