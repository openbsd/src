/*	$OpenBSD: sti.c,v 1.63 2011/08/18 20:02:58 miod Exp $	*/

/*
 * Copyright (c) 2000-2003 Michael Shalayeff
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * TODO:
 *	call sti procs asynchronously;
 *	implement console scroll-back;
 *	X11 support.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <uvm/uvm.h>

#include <machine/bus.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>

#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>

#include "sti.h"

struct cfdriver sti_cd = {
	NULL, "sti", DV_DULL
};

int	sti_alloc_attr(void *, int, int, int, long *);
int	sti_copycols(void *, int, int, int, int);
int	sti_copyrows(void *, int, int, int);
int	sti_cursor(void *, int, int, int);
int	sti_erasecols(void *, int, int, int, long);
int	sti_eraserows(void *, int, int, long);
int	sti_mapchar(void *, int, u_int *);
int	sti_putchar(void *, int, int, u_int, long);
void	sti_unpack_attr(void *, long, int *, int *, int *);

struct wsdisplay_emulops sti_emulops = {
	sti_cursor,
	sti_mapchar,
	sti_putchar,
	sti_copycols,
	sti_erasecols,
	sti_copyrows,
	sti_eraserows,
	sti_alloc_attr,
	sti_unpack_attr
};

int	sti_alloc_screen(void *, const struct wsscreen_descr *, void **, int *,
	    int *, long *);
void	sti_free_screen(void *, void *);
int	sti_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t sti_mmap(void *, off_t, int);
int	sti_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);

const struct wsdisplay_accessops sti_accessops = {
	sti_ioctl,
	sti_mmap,
	sti_alloc_screen,
	sti_free_screen,
	sti_show_screen,
	NULL			/* load_font */
};

enum sti_bmove_funcs {
	bmf_clear, bmf_copy, bmf_invert, bmf_underline
};

void	sti_bmove(struct sti_screen *, int, int, int, int, int, int,
	    enum sti_bmove_funcs);
int	sti_init(struct sti_screen *, int);
#define	STI_TEXTMODE	0x01
#define	STI_CLEARSCR	0x02
int	sti_inqcfg(struct sti_screen *, struct sti_inqconfout *);
int	sti_setcment(struct sti_screen *, u_int, u_char, u_char, u_char);

struct sti_screen *
	sti_attach_screen(struct sti_softc *, int);
void	sti_describe_screen(struct sti_softc *, struct sti_screen *);
void	sti_end_attach_screen(struct sti_softc *, struct sti_screen *, int);
int	sti_fetchfonts(struct sti_screen *, struct sti_inqconfout *, u_int32_t,
	    u_int);
void	sti_region_setup(struct sti_screen *);
int	sti_rom_setup(struct sti_rom *, bus_space_tag_t, bus_space_tag_t,
	    bus_space_handle_t, bus_addr_t *, u_int);
int	sti_screen_setup(struct sti_screen *, int);

#if NSTI_PCI > 0
#define	STI_ENABLE_ROM(sc) \
do { \
	if ((sc) != NULL && (sc)->sc_enable_rom != NULL) \
		(*(sc)->sc_enable_rom)(sc); \
} while (0)
#define	STI_DISABLE_ROM(sc) \
do { \
	if ((sc) != NULL && (sc)->sc_disable_rom != NULL) \
		(*(sc)->sc_disable_rom)(sc); \
} while (0)
#else
#define	STI_ENABLE_ROM(sc)		do { /* nothing */ } while (0)
#define	STI_DISABLE_ROM(sc)		do { /* nothing */ } while (0)
#endif

/* Macros to read larger than 8 bit values from byte roms */
#define	parseshort(o) \
	((bus_space_read_1(memt, romh, (o) + 3) <<  8) | \
	 (bus_space_read_1(memt, romh, (o) + 7)))
#define	parseword(o) \
	((bus_space_read_1(memt, romh, (o) +  3) << 24) | \
	 (bus_space_read_1(memt, romh, (o) +  7) << 16) | \
	 (bus_space_read_1(memt, romh, (o) + 11) <<  8) | \
	 (bus_space_read_1(memt, romh, (o) + 15)))

int
sti_attach_common(struct sti_softc *sc, bus_space_tag_t iot,
    bus_space_tag_t memt, bus_space_handle_t romh, u_int codebase)
{
	struct sti_rom *rom;
	int rc;

	rom = (struct sti_rom *)malloc(sizeof(*rom), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (rom == NULL) {
		printf("cannot allocate rom data\n");
		return (ENOMEM);
	}

	rom->rom_softc = sc;
	rc = sti_rom_setup(rom, iot, memt, romh, sc->bases, codebase);
	if (rc != 0) {
		free(rom, M_DEVBUF);
		return (rc);
	}

	sc->sc_rom = rom;

	sti_describe(sc);

	sc->sc_scr = sti_attach_screen(sc,
	    sc->sc_flags & STI_CONSOLE ?  0 : STI_CLEARSCR);
	if (sc->sc_scr == NULL)
		rc = ENOMEM;

	return (rc);
}

struct sti_screen *
sti_attach_screen(struct sti_softc *sc, int flags)
{
	struct sti_screen *scr;
	int rc;

	scr = (struct sti_screen *)malloc(sizeof(*scr), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (scr == NULL) {
		printf("cannot allocate screen data\n");
		return (NULL);
	}

	scr->scr_rom = sc->sc_rom;
	rc = sti_screen_setup(scr, flags);
	if (rc != 0) {
		free(scr, M_DEVBUF);
		return (NULL);
	}

	sti_describe_screen(sc, scr);

	return (scr);
}

int
sti_rom_setup(struct sti_rom *rom, bus_space_tag_t iot, bus_space_tag_t memt,
    bus_space_handle_t romh, bus_addr_t *bases, u_int codebase)
{
	struct sti_dd *dd;
	int error, size, i;

	STI_ENABLE_ROM(rom->rom_softc);

	rom->iot = iot;
	rom->memt = memt;
	rom->romh = romh;
	rom->bases = bases;

	/*
	 * Get ROM header and code function pointers.
	 */

	dd = &rom->rom_dd;
	rom->rom_devtype = bus_space_read_1(memt, romh, 3);
	if (rom->rom_devtype == STI_DEVTYPE1) {
		dd->dd_type      = bus_space_read_1(memt, romh, 0x03);
		dd->dd_nmon      = bus_space_read_1(memt, romh, 0x07);
		dd->dd_grrev     = bus_space_read_1(memt, romh, 0x0b);
		dd->dd_lrrev     = bus_space_read_1(memt, romh, 0x0f);
		dd->dd_grid[0]   = parseword(0x10);
		dd->dd_grid[1]   = parseword(0x20);
		dd->dd_fntaddr   = parseword(0x30) & ~3;
		dd->dd_maxst     = parseword(0x40);
		dd->dd_romend    = parseword(0x50) & ~3;
		dd->dd_reglst    = parseword(0x60) & ~3;
		dd->dd_maxreent  = parseshort(0x70);
		dd->dd_maxtimo   = parseshort(0x78);
		dd->dd_montbl    = parseword(0x80) & ~3;
		dd->dd_udaddr    = parseword(0x90) & ~3;
		dd->dd_stimemreq = parseword(0xa0);
		dd->dd_udsize    = parseword(0xb0);
		dd->dd_pwruse    = parseshort(0xc0);
		dd->dd_bussup    = bus_space_read_1(memt, romh, 0xcb);
		dd->dd_ebussup   = bus_space_read_1(memt, romh, 0xcf);
		dd->dd_altcodet  = bus_space_read_1(memt, romh, 0xd3);
		dd->dd_eddst[0]  = bus_space_read_1(memt, romh, 0xd7);
		dd->dd_eddst[1]  = bus_space_read_1(memt, romh, 0xdb);
		dd->dd_eddst[2]  = bus_space_read_1(memt, romh, 0xdf);
		dd->dd_cfbaddr   = parseword(0xe0) & ~3;

		codebase <<= 2;
		dd->dd_pacode[0x0] = parseword(codebase + 0x000) & ~3;
		dd->dd_pacode[0x1] = parseword(codebase + 0x010) & ~3;
		dd->dd_pacode[0x2] = parseword(codebase + 0x020) & ~3;
		dd->dd_pacode[0x3] = parseword(codebase + 0x030) & ~3;
		dd->dd_pacode[0x4] = parseword(codebase + 0x040) & ~3;
		dd->dd_pacode[0x5] = parseword(codebase + 0x050) & ~3;
		dd->dd_pacode[0x6] = parseword(codebase + 0x060) & ~3;
		dd->dd_pacode[0x7] = parseword(codebase + 0x070) & ~3;
		dd->dd_pacode[0x8] = parseword(codebase + 0x080) & ~3;
		dd->dd_pacode[0x9] = parseword(codebase + 0x090) & ~3;
		dd->dd_pacode[0xa] = parseword(codebase + 0x0a0) & ~3;
		dd->dd_pacode[0xb] = parseword(codebase + 0x0b0) & ~3;
		dd->dd_pacode[0xc] = parseword(codebase + 0x0c0) & ~3;
		dd->dd_pacode[0xd] = parseword(codebase + 0x0d0) & ~3;
		dd->dd_pacode[0xe] = parseword(codebase + 0x0e0) & ~3;
		dd->dd_pacode[0xf] = parseword(codebase + 0x0f0) & ~3;
	} else {	/* STI_DEVTYPE4 */
		bus_space_read_raw_region_4(memt, romh, 0, (u_int8_t *)dd,
		    sizeof(*dd));
		/* fix pacode... */
		bus_space_read_raw_region_4(memt, romh, codebase,
		    (u_int8_t *)dd->dd_pacode, sizeof(dd->dd_pacode));
	}

	STI_DISABLE_ROM(rom->rom_softc);

#ifdef STIDEBUG
	printf("dd:\n"
	    "devtype=%x, rev=%x;%d, altt=%x, gid=%08x%08x, font=%x, mss=%x\n"
	    "end=%x, regions=%x, msto=%x, timo=%d, mont=%x, user=%x[%x]\n"
	    "memrq=%x, pwr=%d, bus=%x, ebus=%x, cfb=%x\n"
	    "code=",
	    dd->dd_type & 0xff, dd->dd_grrev, dd->dd_lrrev, dd->dd_altcodet,
	    dd->dd_grid[0], dd->dd_grid[1], dd->dd_fntaddr, dd->dd_maxst,
	    dd->dd_romend, dd->dd_reglst, dd->dd_maxreent, dd->dd_maxtimo,
	    dd->dd_montbl, dd->dd_udaddr, dd->dd_udsize, dd->dd_stimemreq,
	    dd->dd_pwruse, dd->dd_bussup, dd->dd_ebussup, dd->dd_cfbaddr);
	printf("%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
	    dd->dd_pacode[0x0], dd->dd_pacode[0x1], dd->dd_pacode[0x2],
	    dd->dd_pacode[0x3], dd->dd_pacode[0x4], dd->dd_pacode[0x5],
	    dd->dd_pacode[0x6], dd->dd_pacode[0x7], dd->dd_pacode[0x8],
	    dd->dd_pacode[0x9], dd->dd_pacode[0xa], dd->dd_pacode[0xb],
	    dd->dd_pacode[0xc], dd->dd_pacode[0xd], dd->dd_pacode[0xe],
	    dd->dd_pacode[0xf]);
#endif

	/*
	 * Figure out how much bytes we need for the STI code.
	 * Note there could be fewer than STI_END entries pointer
	 * entries populated, especially on older devices.
	 */

	for (i = STI_END; !dd->dd_pacode[i]; i--)
		;
	size = dd->dd_pacode[i] - dd->dd_pacode[STI_BEGIN];
	if (rom->rom_devtype == STI_DEVTYPE1)
		size = (size + 3) / 4;
	if (size == 0) {
		printf(": no code for the requested platform\n");
		return (EINVAL);
	}

	if (!(rom->rom_code = uvm_km_alloc(kernel_map, round_page(size)))) {
		printf(": cannot allocate %u bytes for code\n", size);
		return (ENOMEM);
	}
#ifdef STIDEBUG
	printf("code=0x%x[%x]\n", rom->rom_code, size);
#endif

	/*
	 * Copy code into memory and make it executable.
	 */

	STI_ENABLE_ROM(rom->rom_softc);

	if (rom->rom_devtype == STI_DEVTYPE1) {
		u_int8_t *p = (u_int8_t *)rom->rom_code;
		u_int32_t addr, eaddr;

		for (addr = dd->dd_pacode[STI_BEGIN], eaddr = addr + size * 4;
		    addr < eaddr; addr += 4 )
			*p++ = bus_space_read_4(memt, romh, addr) & 0xff;

	} else	/* STI_DEVTYPE4 */
		bus_space_read_raw_region_4(memt, romh,
		    dd->dd_pacode[STI_BEGIN], (u_int8_t *)rom->rom_code,
		    size);

	STI_DISABLE_ROM(rom->rom_softc);

	if ((error = uvm_map_protect(kernel_map, rom->rom_code,
	    rom->rom_code + round_page(size), UVM_PROT_RX, FALSE))) {
		printf(": uvm_map_protect failed (%d)\n", error);
		uvm_km_free(kernel_map, rom->rom_code, round_page(size));
		return (error);
	}

	/*
	 * Setup code function pointers.
	 */

#define	O(i) \
	(dd->dd_pacode[(i)] == 0 ? 0 : \
	    (rom->rom_code + (dd->dd_pacode[(i)] - dd->dd_pacode[0]) / \
	      (rom->rom_devtype == STI_DEVTYPE1? 4 : 1)))

	rom->init	= (sti_init_t)	O(STI_INIT_GRAPH);
	rom->mgmt	= (sti_mgmt_t)	O(STI_STATE_MGMT);
	rom->unpmv	= (sti_unpmv_t)	O(STI_FONT_UNPMV);
	rom->blkmv	= (sti_blkmv_t)	O(STI_BLOCK_MOVE);
	rom->test	= (sti_test_t)	O(STI_SELF_TEST);
	rom->exhdl	= (sti_exhdl_t)	O(STI_EXCEP_HDLR);
	rom->inqconf	= (sti_inqconf_t)O(STI_INQ_CONF);
	rom->scment	= (sti_scment_t)O(STI_SCM_ENT);
	rom->dmac	= (sti_dmac_t)	O(STI_DMA_CTRL);
	rom->flowc	= (sti_flowc_t)	O(STI_FLOW_CTRL);
	rom->utiming	= (sti_utiming_t)O(STI_UTIMING);
	rom->pmgr	= (sti_pmgr_t)	O(STI_PROC_MGR);
	rom->util	= (sti_util_t)	O(STI_UTIL);

#undef	O

	/*
	 * Set colormap entry is not implemented until 8.04, so force
	 * a NULL pointer here.
	 */
	if (dd->dd_grrev < STI_REVISION(8,4)) {
		rom->scment = NULL;
	}

	return (0);
}

/*
 * Map all regions.
 */
void
sti_region_setup(struct sti_screen *scr)
{
	struct sti_rom *rom = scr->scr_rom;
	bus_space_tag_t memt = rom->memt;
	bus_space_handle_t romh = rom->romh;
	bus_addr_t *bases = rom->bases;
	struct sti_dd *dd = &rom->rom_dd;
	struct sti_cfg *cc = &scr->scr_cfg;
	bus_space_handle_t bh;
	struct sti_region regions[STI_REGION_MAX], *r;
	u_int regno, regcnt;
	bus_addr_t addr;

#ifdef STIDEBUG
	printf("stiregions @%p:\n", dd->dd_reglst);
#endif

	/*
	 * Read the region information.
	 */

	STI_ENABLE_ROM(rom->rom_softc);

	if (rom->rom_devtype == STI_DEVTYPE1) {
		for (regno = 0; regno < STI_REGION_MAX; regno++)
			*(u_int *)(regions + regno) =
			    parseword(dd->dd_reglst + regno * 0x10);
	} else {
		bus_space_read_raw_region_4(memt, romh, dd->dd_reglst,
		    (u_int8_t *)regions, sizeof regions);
	}

	STI_DISABLE_ROM(rom->rom_softc);

	/*
	 * Count them.
	 */

	for (regcnt = 0, r = regions; regcnt < STI_REGION_MAX; regcnt++, r++)
		if (r->last)
			break;
	regcnt++;

	/*
	 * Map them.
	 */

	for (regno = 0, r = regions; regno < regcnt; regno++, r++) {
		if (r->length == 0)
			continue;

		/*
		 * Assume an existing mapping exists.
		 */
		addr = bases[regno] + (r->offset << PGSHIFT);

#ifdef STIDEBUG
		printf("%08x @ 0x%08x%s%s%s%s\n",
		    r->length << PGSHIFT, addr, r->sys_only ? " sys" : "",
		    r->cache ? " cache" : "", r->btlb ? " btlb" : "",
		    r->last ? " last" : "");
#endif

		/*
		 * Region #0 is always the rom, and it should have been
		 * mapped already.
		 * XXX This expects a 1:1 mapping...
		 */
		if (regno == 0 && romh == bases[0]) {
			cc->regions[0] = addr;
			continue;
		}

		if (bus_space_map(memt, addr, r->length << PGSHIFT,
		    r->cache ? BUS_SPACE_MAP_CACHEABLE : 0, &bh)) {
#ifdef STIDEBUG
			printf("already mapped region\n");
#endif
		} else {
			/* XXX should use bus_space_vaddr */
			addr = (bus_addr_t)bh;
			if (regno == 1) {
				scr->fbaddr = addr;
				scr->fblen = r->length << PGSHIFT;
			}
		}

		cc->regions[regno] = addr;
	}

#ifdef STIDEBUG
	/*
	 * Make sure we'll trap accessing unmapped regions
	 */
	for (regno = 0; regno < STI_REGION_MAX; regno++)
		if (cc->regions[regno] == 0)
		    cc->regions[regno] = 0x81234567;
#endif
}

int
sti_screen_setup(struct sti_screen *scr, int flags)
{
	struct sti_rom *rom = scr->scr_rom;
	bus_space_tag_t memt = rom->memt;
	bus_space_handle_t romh = rom->romh;
	struct sti_dd *dd = &rom->rom_dd;
	struct sti_cfg *cc = &scr->scr_cfg;
	struct sti_inqconfout cfg;
	struct sti_einqconfout ecfg;
	int error, i;
	int geometry_kluge = 0;
	u_int fontindex = 0;

	bzero(cc, sizeof (*cc));
	cc->ext_cfg = &scr->scr_ecfg;
	bzero(cc->ext_cfg, sizeof(*cc->ext_cfg));

	if (dd->dd_stimemreq) {
		scr->scr_ecfg.addr =
		    malloc(dd->dd_stimemreq, M_DEVBUF, M_NOWAIT);
		if (!scr->scr_ecfg.addr) {
			printf("cannot allocate %d bytes for STI\n",
			    dd->dd_stimemreq);
			return (ENOMEM);
		}
	}

	sti_region_setup(scr);

	if ((error = sti_init(scr, 0))) {
		printf(": can not initialize (%d)\n", error);
		goto fail;
	}

	bzero(&cfg, sizeof(cfg));
	bzero(&ecfg, sizeof(ecfg));
	cfg.ext = &ecfg;
	if ((error = sti_inqcfg(scr, &cfg))) {
		printf(": error %d inquiring config\n", error);
		goto fail;
	}

	/*
	 * Older (rev 8.02) boards report wrong offset values,
	 * similar to the displayable area size, at least in m68k mode.
	 * Attempt to detect this and adjust here.
	 */
	if (cfg.owidth == cfg.width &&
	    cfg.oheight == cfg.height)
		geometry_kluge = 1;

	if (geometry_kluge) {
		scr->scr_cfg.oscr_width = cfg.owidth =
		    cfg.fbwidth - cfg.width;
		scr->scr_cfg.oscr_height = cfg.oheight =
		    cfg.fbheight - cfg.height;
	}

	/*
	 * Save a few fields for sti_describe_screen() later
	 */
	scr->fbheight = cfg.fbheight;
	scr->fbwidth = cfg.fbwidth;
	scr->oheight = cfg.oheight;
	scr->owidth = cfg.owidth;
	bcopy(cfg.name, scr->name, sizeof(scr->name));

	if ((error = sti_init(scr, STI_TEXTMODE | flags))) {
		printf(": can not initialize (%d)\n", error);
		goto fail;
	}
#ifdef STIDEBUG
	printf("conf: bpp=%d planes=%d attr=%b\n"
	    "crt=0x%x:0x%x:0x%x hw=0x%x:0x%x:0x%x\n", cfg.bpp,
	    cfg.planes, cfg.attributes, STI_INQCONF_BITS,
	    ecfg.crt_config[0], ecfg.crt_config[1], ecfg.crt_config[2],
	    ecfg.crt_hw[0], ecfg.crt_hw[1], ecfg.crt_hw[2]);
#endif
	scr->scr_bpp = cfg.bppu;

	/*
	 * Although scr->scr_ecfg.current_monitor is not filled by
	 * sti_init() as expected, we can nevertheless walk the monitor
	 * list, if there is any, and if we find a mode matching our
	 * resolution, pick its font index.
	 */
	if (dd->dd_montbl != 0) {
		STI_ENABLE_ROM(rom->rom_softc);

		for (i = 0; i < dd->dd_nmon; i++) {
			u_int offs = dd->dd_montbl + 8 * i;
			u_int32_t m[2];
			sti_mon_t mon = (void *)m;
			if (rom->rom_devtype == STI_DEVTYPE1) {
				m[0] = parseword(4 * offs);
				m[1] = parseword(4 * (offs + 4));
			} else {
				bus_space_read_raw_region_4(memt, romh, offs,
				    (u_int8_t *)mon, sizeof(*mon));
			}

			if (mon->width == scr->scr_cfg.scr_width &&
			    mon->height == scr->scr_cfg.scr_height) {
				fontindex = mon->font;
				break;
			}
		}

		STI_DISABLE_ROM(rom->rom_softc);

#ifdef STIDEBUG
		printf("font index: %d\n", fontindex);
#endif
	}

	if ((error = sti_fetchfonts(scr, &cfg, dd->dd_fntaddr, fontindex))) {
		printf(": cannot fetch fonts (%d)\n", error);
		goto fail;
	}

	/*
	 * setup screen descriptions:
	 *	figure number of fonts supported;
	 *	allocate wscons structures;
	 *	calculate dimensions.
	 */

	strlcpy(scr->scr_wsd.name, "std", sizeof(scr->scr_wsd.name));
	scr->scr_wsd.ncols = cfg.width / scr->scr_curfont.width;
	scr->scr_wsd.nrows = cfg.height / scr->scr_curfont.height;
	scr->scr_wsd.textops = &sti_emulops;
	scr->scr_wsd.fontwidth = scr->scr_curfont.width;
	scr->scr_wsd.fontheight = scr->scr_curfont.height;
	scr->scr_wsd.capabilities = 0;

	scr->scr_scrlist[0] = &scr->scr_wsd;
	scr->scr_screenlist.nscreens = 1;
	scr->scr_screenlist.screens =
	    (const struct wsscreen_descr **)scr->scr_scrlist;

	return (0);

fail:
	/* XXX free resources */
	if (scr->scr_ecfg.addr != NULL) {
		free(scr->scr_ecfg.addr, M_DEVBUF);
		scr->scr_ecfg.addr = NULL;
	}

	return (ENXIO);
}

void
sti_describe_screen(struct sti_softc *sc, struct sti_screen *scr)
{
	struct sti_font *fp = &scr->scr_curfont;

	printf("%s: %s, %dx%d frame buffer, %dx%dx%d display\n",
	    sc->sc_dev.dv_xname, scr->name, scr->fbwidth, scr->fbheight,
	    scr->scr_cfg.scr_width, scr->scr_cfg.scr_height, scr->scr_bpp);

	printf("%s: %dx%d font type %d, %d bpc, charset %d-%d\n",
	    sc->sc_dev.dv_xname, fp->width, fp->height,
	    fp->type, fp->bpc, fp->first, fp->last);
}

void
sti_describe(struct sti_softc *sc)
{
	struct sti_rom *rom = sc->sc_rom;
	struct sti_dd *dd = &rom->rom_dd;

	printf(": rev %d.%02d;%d, ID 0x%08X%08X\n",
	    dd->dd_grrev >> 4, dd->dd_grrev & 0xf,
	    dd->dd_lrrev, dd->dd_grid[0], dd->dd_grid[1]);

	if (sc->sc_scr != NULL)
		sti_describe_screen(sc, sc->sc_scr);
}

/*
 * Final part of attachment. On hppa where we use the PDC console
 * during autoconf, this has to be postponed until autoconf has
 * completed.
 */
void
sti_end_attach(void *v)
{
	struct sti_softc *sc = (struct sti_softc *)v;

	if (sc->sc_scr != NULL)
		sti_end_attach_screen(sc, sc->sc_scr,
		    sc->sc_flags & STI_CONSOLE ? 1 : 0);
}

void
sti_end_attach_screen(struct sti_softc *sc, struct sti_screen *scr, int console)
{
	struct wsemuldisplaydev_attach_args waa;

	scr->scr_wsmode = WSDISPLAYIO_MODE_EMUL;

	waa.console = console;
	waa.scrdata = &scr->scr_screenlist;
	waa.accessops = &sti_accessops;
	waa.accesscookie = scr;
	waa.defaultscreens = 0;

	/* attach as console if required */
	if (console && !ISSET(sc->sc_flags, STI_ATTACHED)) {
		long defattr;

		sti_alloc_attr(scr, 0, 0, 0, &defattr);
		wsdisplay_cnattach(&scr->scr_wsd, scr,
		    0, scr->scr_wsd.nrows - 1, defattr);
		sc->sc_flags |= STI_ATTACHED;
	}

	config_found(&sc->sc_dev, &waa, wsemuldisplaydevprint);
}

u_int
sti_rom_size(bus_space_tag_t memt, bus_space_handle_t romh)
{
	int devtype;
	u_int romend;

	devtype = bus_space_read_1(memt, romh, 3);
	if (devtype == STI_DEVTYPE4) {
		bus_space_read_raw_region_4(memt, romh, 0x18,
		    (u_int8_t *)&romend, 4);
	} else {
		romend = parseword(0x50);
	}

	return (round_page(romend));
}

int
sti_fetchfonts(struct sti_screen *scr, struct sti_inqconfout *cfg,
    u_int32_t baseaddr, u_int fontindex)
{
	struct sti_rom *rom = scr->scr_rom;
	bus_space_tag_t memt = rom->memt;
	bus_space_handle_t romh = rom->romh;
	struct sti_font *fp = &scr->scr_curfont;
	u_int32_t addr;
	int size;
#ifdef notyet
	int uc;
	struct {
		struct sti_unpmvflags flags;
		struct sti_unpmvin in;
		struct sti_unpmvout out;
	} a;
#endif

	/*
	 * Get the first PROM font in memory
	 */

	STI_ENABLE_ROM(rom->rom_softc);

rescan:
	addr = baseaddr;
	do {
		if (rom->rom_devtype == STI_DEVTYPE1) {
			fp->first  = parseshort(addr + 0x00);
			fp->last   = parseshort(addr + 0x08);
			fp->width  = bus_space_read_1(memt, romh,
			    addr + 0x13);
			fp->height = bus_space_read_1(memt, romh,
			    addr + 0x17);
			fp->type   = bus_space_read_1(memt, romh,
			    addr + 0x1b);
			fp->bpc    = bus_space_read_1(memt, romh,
			    addr + 0x1f);
			fp->next   = parseword(addr + 0x20);
			fp->uheight= bus_space_read_1(memt, romh,
			    addr + 0x33);
			fp->uoffset= bus_space_read_1(memt, romh,
			    addr + 0x37);
		} else { /* STI_DEVTYPE4 */
			bus_space_read_raw_region_4(memt, romh, addr,
			    (u_int8_t *)fp, sizeof(struct sti_font));
		}

#ifdef STIDEBUG
		STI_DISABLE_ROM(rom->rom_softc);
		printf("font@%p: %d-%d, %dx%d, type %d, next %x\n",
		    addr, fp->first, fp->last, fp->width, fp->height, fp->type,
		    fp->next);
		STI_ENABLE_ROM(rom->rom_softc);
#endif

		if (fontindex == 0) {
			size = sizeof(struct sti_font) +
			    (fp->last - fp->first + 1) * fp->bpc;
			if (rom->rom_devtype == STI_DEVTYPE1)
				size *= 4;
			scr->scr_romfont = malloc(size, M_DEVBUF, M_NOWAIT);
			if (scr->scr_romfont == NULL)
				return (ENOMEM);

			bus_space_read_raw_region_4(memt, romh, addr,
			    (u_int8_t *)scr->scr_romfont, size);

			break;
		}

		addr = baseaddr + fp->next;
		fontindex--;
	} while (fp->next != 0);

	/*
	 * If our font index was bogus, we did not find the expected font.
	 * In this case, pick the first one and be done with it.
	 */
	if (fp->next == 0 && scr->scr_romfont == NULL) {
		fontindex = 0;
		goto rescan;
	}

	STI_DISABLE_ROM(rom->rom_softc);

#ifdef notyet
	/*
	 * If there is enough room in the off-screen framebuffer memory,
	 * display all the characters there in order to display them
	 * faster with blkmv operations rather than unpmv later on.
	 */
	if (size <= cfg->fbheight *
	    (cfg->fbwidth - cfg->width - cfg->owidth)) {
		bzero(&a, sizeof(a));
		a.flags.flags = STI_UNPMVF_WAIT;
		a.in.fg_colour = STI_COLOUR_WHITE;
		a.in.bg_colour = STI_COLOUR_BLACK;
		a.in.font_addr = scr->scr_romfont;

		scr->scr_fontmaxcol = cfg->fbheight / fp->height;
		scr->scr_fontbase = cfg->width + cfg->owidth;
		for (uc = fp->first; uc <= fp->last; uc++) {
			a.in.x = ((uc - fp->first) / scr->scr_fontmaxcol) *
			    fp->width + scr->scr_fontbase;
			a.in.y = ((uc - fp->first) % scr->scr_fontmaxcol) *
			    fp->height;
			a.in.index = uc;

			(*scr->unpmv)(&a.flags, &a.in, &a.out, &scr->scr_cfg);
			if (a.out.errno) {
#ifdef STIDEBUG
				printf("sti_unpmv %d returned %d\n",
				    uc, a.out.errno);
#endif
				return (0);
			}
		}

		free(scr->scr_romfont, M_DEVBUF);
		scr->scr_romfont = NULL;
	}
#endif

	return (0);
}

/*
 * Wrappers around STI code pointers
 */

int
sti_init(struct sti_screen *scr, int mode)
{
	struct sti_rom *rom = scr->scr_rom;
	struct {
		struct sti_initflags flags;
		struct sti_initin in;
		struct sti_einitin ein;
		struct sti_initout out;
	} a;

	bzero(&a, sizeof(a));

	a.flags.flags = STI_INITF_WAIT | STI_INITF_CMB | STI_INITF_EBET |
	    (mode & STI_TEXTMODE ? STI_INITF_TEXT | STI_INITF_PBET |
	     STI_INITF_PBETI | STI_INITF_ICMT : 0) |
	    (mode & STI_CLEARSCR ? STI_INITF_CLEAR : 0);
	a.in.text_planes = 1;
	a.in.ext_in = &a.ein;
#ifdef STIDEBUG
	printf("sti_init,%p(%x, %p, %p, %p)\n",
	    rom->init, a.flags.flags, &a.in, &a.out, &scr->scr_cfg);
#endif
	(*rom->init)(&a.flags, &a.in, &a.out, &scr->scr_cfg);
	if (a.out.text_planes != a.in.text_planes)
		return (-1);	/* not colliding with sti errno values */
	return (a.out.errno);
}

int
sti_inqcfg(struct sti_screen *scr, struct sti_inqconfout *out)
{
	struct sti_rom *rom = scr->scr_rom;
	struct {
		struct sti_inqconfflags flags;
		struct sti_inqconfin in;
	} a;

	bzero(&a, sizeof(a));

	a.flags.flags = STI_INQCONFF_WAIT;
	(*rom->inqconf)(&a.flags, &a.in, out, &scr->scr_cfg);

	return out->errno;
}

void
sti_bmove(struct sti_screen *scr, int x1, int y1, int x2, int y2, int h, int w,
    enum sti_bmove_funcs f)
{
	struct sti_rom *rom = scr->scr_rom;
	struct {
		struct sti_blkmvflags flags;
		struct sti_blkmvin in;
		struct sti_blkmvout out;
	} a;

	bzero(&a, sizeof(a));

	a.flags.flags = STI_BLKMVF_WAIT;
	switch (f) {
	case bmf_clear:
		a.flags.flags |= STI_BLKMVF_CLR;
		a.in.bg_colour = STI_COLOUR_BLACK;
		break;
	case bmf_underline:
	case bmf_copy:
		a.in.fg_colour = STI_COLOUR_WHITE;
		a.in.bg_colour = STI_COLOUR_BLACK;
		break;
	case bmf_invert:
		a.flags.flags |= STI_BLKMVF_COLR;
		a.in.fg_colour = STI_COLOUR_BLACK;
		a.in.bg_colour = STI_COLOUR_WHITE;
		break;
	}
	a.in.srcx = x1;
	a.in.srcy = y1;
	a.in.dstx = x2;
	a.in.dsty = y2;
	a.in.height = h;
	a.in.width = w;

	(*rom->blkmv)(&a.flags, &a.in, &a.out, &scr->scr_cfg);
#ifdef STIDEBUG
	if (a.out.errno)
		printf("sti_blkmv returned %d\n", a.out.errno);
#endif
}

int
sti_setcment(struct sti_screen *scr, u_int i, u_char r, u_char g, u_char b)
{
	struct sti_rom *rom = scr->scr_rom;
	struct {
		struct sti_scmentflags flags;
		struct sti_scmentin in;
		struct sti_scmentout out;
	} a;

	bzero(&a, sizeof(a));

	a.flags.flags = STI_SCMENTF_WAIT;
	a.in.entry = i;
	a.in.value = (r << 16) | (g << 8) | b;

	(*rom->scment)(&a.flags, &a.in, &a.out, &scr->scr_cfg);

	return a.out.errno;
}

/*
 * wsdisplay accessops
 */

int
sti_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct sti_screen *scr = (struct sti_screen *)v;
	struct sti_rom *rom = scr->scr_rom;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_cmap *cmapp;
	u_int mode, idx, count;
	int i, ret;

	ret = 0;
	switch (cmd) {
	case WSDISPLAYIO_SMODE:
		mode = *(u_int *)data;
		if (scr->scr_wsmode == WSDISPLAYIO_MODE_EMUL &&
		    mode == WSDISPLAYIO_MODE_DUMBFB)
			ret = sti_init(scr, 0);
		else if (scr->scr_wsmode == WSDISPLAYIO_MODE_DUMBFB &&
		    mode == WSDISPLAYIO_MODE_EMUL)
			ret = sti_init(scr, STI_TEXTMODE);
		scr->scr_wsmode = mode;
		break;

	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_STI;
		break;

	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = scr->scr_cfg.scr_height;
		wdf->width  = scr->scr_cfg.scr_width;
		wdf->depth  = scr->scr_bpp;
		if (rom->scment == NULL)
			wdf->cmsize = 0;
		else
			wdf->cmsize = STI_NCMAP;
		break;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = scr->scr_cfg.fb_width;
		break;

	case WSDISPLAYIO_GETCMAP:
		if (rom->scment == NULL)
			return ENODEV;
		cmapp = (struct wsdisplay_cmap *)data;
		idx = cmapp->index;
		count = cmapp->count;
		if (idx >= STI_NCMAP || idx + count > STI_NCMAP)
			return EINVAL;
		if ((ret = copyout(&scr->scr_rcmap[idx], cmapp->red, count)))
			break;
		if ((ret = copyout(&scr->scr_gcmap[idx], cmapp->green, count)))
			break;
		if ((ret = copyout(&scr->scr_bcmap[idx], cmapp->blue, count)))
			break;
		break;

	case WSDISPLAYIO_PUTCMAP:
		if (rom->scment == NULL)
			return ENODEV;
		cmapp = (struct wsdisplay_cmap *)data;
		idx = cmapp->index;
		count = cmapp->count;
		if (idx >= STI_NCMAP || idx + count > STI_NCMAP)
			return EINVAL;
		if ((ret = copyin(cmapp->red, &scr->scr_rcmap[idx], count)))
			break;
		if ((ret = copyin(cmapp->green, &scr->scr_gcmap[idx], count)))
			break;
		if ((ret = copyin(cmapp->blue, &scr->scr_bcmap[idx], count)))
			break;
		for (i = idx + count - 1; i >= idx; i--)
			if ((ret = sti_setcment(scr, i, scr->scr_rcmap[i],
			    scr->scr_gcmap[i], scr->scr_bcmap[i]))) {
#ifdef STIDEBUG
				printf("sti_ioctl: "
				    "sti_setcment(%d, %u, %u, %u): %d\n", i,
				    (u_int)scr->scr_rcmap[i],
				    (u_int)scr->scr_gcmap[i],
				    (u_int)scr->scr_bcmap[i]);
#endif
				ret = EINVAL;
				break;
			}
		break;

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
		break;

	default:
		return (-1);		/* not supported yet */
	}

	return (ret);
}

paddr_t
sti_mmap(void *v, off_t offset, int prot)
{
#if 0
	struct sti_screen *scr = (struct sti_screen *)v;
#endif

	/* XXX not finished */
	return -1;
}

int
sti_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *cxp, int *cyp, long *defattr)
{
	struct sti_screen *scr = (struct sti_screen *)v;

	if (scr->scr_nscreens > 0)
		return ENOMEM;

	*cookiep = scr;
	*cxp = 0;
	*cyp = 0;
	sti_alloc_attr(scr, 0, 0, 0, defattr);
	scr->scr_nscreens++;
	return 0;
}

void
sti_free_screen(void *v, void *cookie)
{
	struct sti_screen *scr = (struct sti_screen *)v;

	scr->scr_nscreens--;
}

int
sti_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
#if 0
	struct sti_screen *scr = (struct sti_screen *)v;
#endif

	return 0;
}

/*
 * wsdisplay emulops
 */

int
sti_cursor(void *v, int on, int row, int col)
{
	struct sti_screen *scr = (struct sti_screen *)v;
	struct sti_font *fp = &scr->scr_curfont;

	sti_bmove(scr,
	    col * fp->width, row * fp->height,
	    col * fp->width, row * fp->height,
	    fp->height, fp->width, bmf_invert);

	return 0;
}

/*
 * ISO 8859-1 part of Unicode to HP Roman font index conversion array.
 */
static const u_int8_t
sti_unitoroman[0x100 - 0xa0] = {
	0xa0, 0xb8, 0xbf, 0xbb, 0xba, 0xbc,    0, 0xbd,
	0xab,    0, 0xf9, 0xfb,    0, 0xf6,    0, 0xb0,
	
	0xb3, 0xfe,    0,    0, 0xa8, 0xf3, 0xf4, 0xf2,
	   0,    0, 0xfa, 0xfd, 0xf7, 0xf8,    0, 0xb9,

	0xa1, 0xe0, 0xa2, 0xe1, 0xd8, 0xd0, 0xd3, 0xb4,
	0xa3, 0xdc, 0xa4, 0xa5, 0xe6, 0xe5, 0xa6, 0xa7,

	0xe3, 0xb6, 0xe8, 0xe7, 0xdf, 0xe9, 0xda,    0,
	0xd2, 0xad, 0xed, 0xae, 0xdb, 0xb1, 0xf0, 0xde,

	0xc8, 0xc4, 0xc0, 0xe2, 0xcc, 0xd4, 0xd7, 0xb5,
	0xc9, 0xc5, 0xc1, 0xcd, 0xd9, 0xd5, 0xd1, 0xdd,

	0xe4, 0xb7, 0xca, 0xc6, 0xc2, 0xea, 0xce,    0,
	0xd6, 0xcb, 0xc7, 0xc3, 0xcf, 0xb2, 0xf1, 0xef
};

int
sti_mapchar(void *v, int uni, u_int *index)
{
	struct sti_screen *scr = (struct sti_screen *)v;
	struct sti_font *fp = &scr->scr_curfont;
	int c;

	switch (fp->type) {
	case STI_FONT_HPROMAN8:
		if (uni >= 0x80 && uni < 0xa0)
			c = -1;
		else if (uni >= 0xa0 && uni < 0x100) {
			c = (int)sti_unitoroman[uni - 0xa0];
			if (c == 0)
				c = -1;
		} else
			c = uni;
		break;
	default:
		c = uni;
		break;
	}

	if (c == -1 || c < fp->first || c > fp->last) {
		*index = ' ';
		return (0);
	}

	*index = c;
	return (5);
}

int
sti_putchar(void *v, int row, int col, u_int uc, long attr)
{
	struct sti_screen *scr = (struct sti_screen *)v;
	struct sti_rom *rom = scr->scr_rom;
	struct sti_font *fp = &scr->scr_curfont;

	if (scr->scr_romfont != NULL) {
		/*
		 * Font is in memory, use unpmv
		 */
		struct {
			struct sti_unpmvflags flags;
			struct sti_unpmvin in;
			struct sti_unpmvout out;
		} a;

		bzero(&a, sizeof(a));

		a.flags.flags = STI_UNPMVF_WAIT;
		/* XXX does not handle text attributes */
		a.in.fg_colour = STI_COLOUR_WHITE;
		a.in.bg_colour = STI_COLOUR_BLACK;
		a.in.x = col * fp->width;
		a.in.y = row * fp->height;
		a.in.font_addr = scr->scr_romfont;
		a.in.index = uc;

		(*rom->unpmv)(&a.flags, &a.in, &a.out, &scr->scr_cfg);
	} else {
		/*
		 * Font is in frame buffer, use blkmv
		 */
		struct {
			struct sti_blkmvflags flags;
			struct sti_blkmvin in;
			struct sti_blkmvout out;
		} a;

		bzero(&a, sizeof(a));

		a.flags.flags = STI_BLKMVF_WAIT;
		/* XXX does not handle text attributes */
		a.in.fg_colour = STI_COLOUR_WHITE;
		a.in.bg_colour = STI_COLOUR_BLACK;

		a.in.srcx = ((uc - fp->first) / scr->scr_fontmaxcol) *
		    fp->width + scr->scr_fontbase;
		a.in.srcy = ((uc - fp->first) % scr->scr_fontmaxcol) *
		    fp->height;
		a.in.dstx = col * fp->width;
		a.in.dsty = row * fp->height;
		a.in.height = fp->height;
		a.in.width = fp->width;

		(*rom->blkmv)(&a.flags, &a.in, &a.out, &scr->scr_cfg);
	}

	return 0;
}

int
sti_copycols(void *v, int row, int srccol, int dstcol, int ncols)
{
	struct sti_screen *scr = (struct sti_screen *)v;
	struct sti_font *fp = &scr->scr_curfont;

	sti_bmove(scr,
	    srccol * fp->width, row * fp->height,
	    dstcol * fp->width, row * fp->height,
	    fp->height, ncols * fp->width, bmf_copy);

	return 0;
}

int
sti_erasecols(void *v, int row, int startcol, int ncols, long attr)
{
	struct sti_screen *scr = (struct sti_screen *)v;
	struct sti_font *fp = &scr->scr_curfont;

	sti_bmove(scr,
	    startcol * fp->width, row * fp->height,
	    startcol * fp->width, row * fp->height,
	    fp->height, ncols * fp->width, bmf_clear);

	return 0;
}

int
sti_copyrows(void *v, int srcrow, int dstrow, int nrows)
{
	struct sti_screen *scr = (struct sti_screen *)v;
	struct sti_font *fp = &scr->scr_curfont;

	sti_bmove(scr, 0, srcrow * fp->height, 0, dstrow * fp->height,
	    nrows * fp->height, scr->scr_cfg.scr_width, bmf_copy);

	return 0;
}

int
sti_eraserows(void *v, int srcrow, int nrows, long attr)
{
	struct sti_screen *scr = (struct sti_screen *)v;
	struct sti_font *fp = &scr->scr_curfont;

	sti_bmove(scr, 0, srcrow * fp->height, 0, srcrow * fp->height,
	    nrows * fp->height, scr->scr_cfg.scr_width, bmf_clear);

	return 0;
}

int
sti_alloc_attr(void *v, int fg, int bg, int flags, long *pattr)
{
#if 0
	struct sti_screen *scr = (struct sti_screen *)v;
#endif

	*pattr = 0;
	return 0;
}

void
sti_unpack_attr(void *v, long attr, int *fg, int *bg, int *ul)
{
#if 0
	struct sti_screen *scr = (struct sti_screen *)v;
#endif

	*fg = WSCOL_WHITE;
	*bg = WSCOL_BLACK;
	if (ul != NULL)
		*ul = 0;
}

#if NSTI_DIO > 0 || NSTI_SGC > 0

/*
 * Early console support.  Only used on hp300.
 */

int
sti_cnattach(struct sti_rom *rom, struct sti_screen *scr, bus_space_tag_t memt,
    bus_addr_t *bases, u_int codebase)
{
	bus_space_handle_t romh;
	u_int romend;
	int error;
	long defattr;

	if ((error = bus_space_map(memt, bases[0], PAGE_SIZE, 0, &romh)) != 0)
		return (error);

	/*
	 * Compute real PROM size
	 */
	romend = sti_rom_size(memt, romh);

	bus_space_unmap(memt, romh, PAGE_SIZE);

	if ((error = bus_space_map(memt, bases[0], romend, 0, &romh)) != 0)
		return (error);

	bases[0] = romh;
	if (sti_rom_setup(rom, memt, memt, romh, bases, codebase) != 0)
		panic(__func__);
	scr->scr_rom = rom;
	if (sti_screen_setup(scr, STI_CLEARSCR) != 0)
		panic(__func__);

	sti_alloc_attr(scr, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&scr->scr_wsd, scr, 0, 0, defattr);

	return (0);
}

#endif	/* NSTI_SGC > 0 */
