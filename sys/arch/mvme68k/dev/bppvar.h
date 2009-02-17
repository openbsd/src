/*	$OpenBSD: bppvar.h,v 1.1 2009/02/17 22:28:40 miod Exp $	*/

/*
 * Copyright (c) 2008, 2009  Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies. And
 * I won't mind if you keep the disclaimer below.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Register and structure definitions for MVME boards using the Motorola
 * Buffered Pipe Protocol.
 */

/*
 * Register layout (CSR space)
 */

#define	CSR_ADDR			0x00	/* 32 */
#define	CSR_ADDR_MOD			0x04	/* 8 */
#define	CSR_DATA_WIDTH			0x05	/* 8 */
/* IPC control register */
#define	CSR_CONTROL			0x06	/* 16 */
#define	CSR_STATUS			0x08	/* 8 */
#define	CSR_DIAG			0x09	/* 8 */
#define	CSR_TAS				0x0e	/* 16 */

/* CSR bits */
#define	CSR_BUSY			0x8000	/* firmware busy */
#define	CSR_RESET			0x4000	/* reset board */
#define	CSR_ATTENTION			0x2000	/* interrupt firmware */
#define	CSR_INHIBIT_SYSFAIL		0x1000

/* TAS bits */
#define	TAS_TAS				0x8000	/* test and set bit */
#define	TAS_VALID_COMMAND		0x4000
#define	TAS_VALID_STATUS		0x2000
#define	TAS_COMMAND_COMPLETE		0x1000
#define	TAS_COMMAND_MASK		0x0fff

/* TAS commands */
#define	COMMAND_CHANNEL_CREATE		0x001
#define	COMMAND_CHANNEL_DELETE		0x002
#define	COMMAND_DUMP_MEMORY		0x101

/* command status values */
#define	STATUS_OK			0x00
#define	STATUS_INVALID_COMMAND		0x01
#define	STATUS_READ_CHANNEL_HEADER	0x02
#define	STATUS_WRITE_CHANNEL_HEADER	0x03
#define	STATUS_DMA_READ			0x04
#define	STATUS_DMA_WRITE		0x05
#define	STATUS_NO_MORE_CHANNELS		0x06
#define	STATUS_NO_SUCH_CHANNEL		0x07
/* catastrophic errors start here... */
#define	STATUS_LOCAL_ADDRESS_ERROR	0xaa
#define	STATUS_ACFAIL			0xac
#define	STATUS_LOCAL_BUS_ERROR		0xbb
#define	STATUS_CONFIDENCE_FAILED	0xcc
#define	STATUS_UNEXPECTED_ERROR		0xee
#define	STATUS_ERRNO			0x100	/* not from status reg */

/*
 * BPP Channel header
 */

struct bpp_channel {
	uint32_t	ch_cmd_head;	/* command pipe head */
	uint32_t	ch_cmd_tail;	/* command pipe tail */
	uint32_t	ch_status_head;	/* status pipe head */
	uint32_t	ch_status_tail;	/* status pipe tail */
	uint8_t		ch_ipl;		/* channel interrupt level */
	uint8_t		ch_ivec;	/* channel interrupt vector */
	uint8_t		ch_priority;	/* channel priority */
#define	BPP_PRIORITY_HIGHEST	0x00
#define	BPP_PRIORITY_LOWEST	0xff
	uint8_t		ch_addrmod;	/* envelope memory address modifier */
	uint8_t		ch_num;		/* set by firmware */
	uint8_t		ch_valid;	/* set by firmware */
	uint8_t		ch_buswidth;	/* unused */
	uint8_t		reserved;
};

/*
 * BPP Envelope structure
 */

struct bpp_envelope {
	uint32_t	env_link;	/* next envelope */
	uint32_t	env_pkt;	/* command packet physical address */
	uint8_t		env_valid;
#define	ENVELOPE_INVALID	0x00
#define	ENVELOPE_VALID		0x01	/* any nonzero value will do */
	uint8_t		reserved[3];
	/*
	 * The following is not part of the envelope as defined by the
	 * firmware, but is used to page the envelope to a 68040/88200
	 * cache line boundary.
	 */
	uint32_t	pad;
	/* total size 0x10 bytes */
};

/*
 * Data bus width
 */

#define	MEMT_D16	1
#define	MEMT_D32	2

/*
 * The following defines ought to be in a MI vme header file...
 */

/*
 * VME addressing modes
 */

#define	ADRM_STD_S_P		0x3e	/* standard supervisory program */
#define	ADRM_STD_S_D		0x3d	/* standard supervisory data */
#define	ADRM_STD_N_P		0x3a	/* standard normal program */
#define	ADRM_STD_N_D		0x39	/* standard normal data */
#define	ADRM_SHT_S_IO		0x2d	/* short supervisory I/O */
#define	ADRM_SHT_N_IO		0x29	/* short normal I/O */
#define	ADRM_EXT_S_P		0x0e	/* extended supervisory program */
#define	ADRM_EXT_S_D		0x0d	/* extended supervisory data */
#define	ADRM_EXT_N_P		0x0a	/* extended normal program */
#define	ADRM_EXT_N_D		0x09	/* extended normal data */
#define	ADRM_EXT_S_BM		0x0f	/* extended supervisory block mode */
#define	ADRM_EXT_S_D64		0x0c	/* extended supervisory D64 mode */

/*
 * Per channel information
 */

struct bpp_chan {
	struct bpp_channel	*ch;		/* bpp communication channel */

	struct bpp_envelope	*envcmd;	/* command tail envelope */
	struct bpp_envelope	*envsts;	/* status head envelope */
};

/*
 * Device superset
 */

struct bpp_softc {
	struct device		 sc_dev;

	bus_space_tag_t		 sc_iot;	/* CSR registers access */
	bus_space_handle_t	 sc_ioh;

	struct bpp_envelope	*sc_env_free;	/* head of free envelope list */

	/* channel function pointers */
	paddr_t	(*bpp_chan_pa)(struct bpp_softc *, struct bpp_channel *);
	void	(*bpp_chan_sync)(struct bpp_softc *, struct bpp_channel *, int);

	/* envelope function pointers */
	paddr_t	(*bpp_env_pa)(struct bpp_softc *, struct bpp_envelope *);
	struct bpp_envelope *(*bpp_env_va)(struct bpp_softc *, paddr_t);
	void	(*bpp_env_sync)(struct bpp_softc *, struct bpp_envelope *, int);
};

void	bpp_attach(struct bpp_softc *, bus_space_tag_t, bus_space_handle_t);
void	bpp_attention(struct bpp_softc *);
int	bpp_create_channel(struct bpp_softc *, struct bpp_chan *,
	    int, int, int);
int	bpp_dequeue_envelope(struct bpp_softc *, struct bpp_chan *, paddr_t *);
struct bpp_envelope *
	bpp_get_envelope(struct bpp_softc *);
void	bpp_initialize_envelopes(struct bpp_softc *, struct bpp_envelope *,
	    uint);
void	bpp_put_envelope(struct bpp_softc *, struct bpp_envelope *);
void	bpp_queue_envelope(struct bpp_softc *, struct bpp_chan *,
	    struct bpp_envelope *, paddr_t);
int	bpp_reset(struct bpp_softc *);
