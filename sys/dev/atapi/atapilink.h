/*	$OpenBSD: atapilink.h,v 1.5 1996/08/07 01:56:29 downsj Exp $	*/

/*
 * Copyright (c) 1996 Manuel Bouyer.  All rights reserved.
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
 *  This product includes software developed by Manuel Bouyer.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/* #undef ATAPI_DEBUG */
/* #undef ATAPI_DEBUG_PROBE */

struct bus_link {
	u_int8_t type;  
#define DRIVE	0
#define BUS	1
	caddr_t	wdc_softc;
	caddr_t	atapibus_softc;
	struct	wdc_link *ctlr_link;
	u_int8_t ctrl;
};

struct atapi_identify {
	struct	config_s {
		u_int8_t cmd_drq_rem;
#define ATAPI_PACKET_SIZE_MASK	0x02
#define ATAPI_PACKET_SIZE_12	0x00
#define ATAPI_PACKET_SIZE_16	0x01

#define ATAPI_DRQ_MASK		0x60
#define ATAPI_MICROPROCESSOR_DRQ 0x00
#define ATAPI_INTERRUPT_DRQ	0x20
#define ATAPI_ACCELERATED_DRQ	0x40

#define ATAPI_REMOVABLE		0x80

		u_int8_t device_type;
#define ATAPI_DEVICE_TYPE_MASK	0x1f
#define ATAPI_DEVICE_TYPE_DAD	0x00	/* direct access device */
					/* 0x1-0x4 reserved */
#define ATAPI_DEVICE_TYPE_CD	0x05	/* CD-ROM */
					/* 0x6 reserved */
#define ATAPI_DEVICE_TYPE_OMD	0x07	/* optical memory device */
					/* 0x8-0x1e reserved */
#define ATAPI_DEVICE_TYPE_UNKNOWN 0x1f

#define ATAPI_GC_PROTOCOL_MASK	0xc0	/* mask of protocol bits */
					/* 0x00 and 0x01 are ATA */
#define ATAPI_GC_PROTO_TYPE_ATAPI 0x80
#define ATAPI_GC_PROTO_TYPE_RESERVED 0xc0
	} config;				/* general configuration */

	u_int8_t	cylinders[2];
	u_int8_t	reserved1[2];
	u_int8_t	heads[2];
	u_int8_t	unf_bytes_per_track[2];
	u_int8_t	unf_bytes_per_sector[2];
	u_int8_t	sectors_per_track[2];
	u_int8_t	reserved2[6];
	char		serial_number[20];
	u_int8_t	buffer_type[2];
	u_int8_t	buffer_size[2];
	u_int8_t	ECC_bytes_available[2];
	char		firmware_revision[8];
	char		model[40];
	u_int8_t	sector_count[2];
	u_int8_t	double_word[2];		/* == 0 for CD-ROMs */

	struct capabilities_s {
		u_int8_t vendor;
		u_int8_t capflags;
#define ATAPI_CAP_DMA			0x01	/* DMA supported */
#define ATAPI_CAP_LBA			0x02	/* LBA supported */
#define ATAPI_IORDY_DISABLE		0x04	/* IORDY can be disabled */
#define ATAPI_IORDY			0x08	/* IORDY supported */
	} capabilities;

	u_int8_t	reserved3[2];
	u_int8_t	PIO_cycle_timing[2];
	u_int8_t	DMA_cycle_timing[2];
	u_int8_t	validity[2]; /* of words 54-58, 64-70 in this table */

#define ATAPI_VALID_FIRST	0x0	/* == 1 => words 54-58 are valid */
#define ATAPI_VALID_SECOND	0x1	/* == 1 => words 64-70 are valid */

	u_int8_t	current_chs[6];	/* cylinder/head/sector */
	u_int8_t	current_capacity[4];
	u_int8_t	reserved4[2];
	u_int8_t	user_addressable_sectors[4];
	u_int8_t	singleword_DMA_mode[2];

#define ATAPI_SW_DMA_MODE_AVAIL	0x00ff	/* Mode 0 is supported */
#define ATAPI_SW_DMA_MODE_ACTIVE 0xff00	/* which mode is active */

	u_int8_t	multiword_DMA_mode[2];

#define ATAPI_MW_DMA_MODE_AVAIL	0x00ff	/* Mode 0 is supported */
#define ATAPI_MW_DMA_MODE_ACTIVE 0xff00	/* which mode is active */

	u_int8_t	enhanced_PIO_mode[2];

#define ATAPI_ENHANCED_PIO_AVAIL 0x0001	/* PIO Mode 3 is supported */

	u_int8_t	blind_PIO_minimum_cycles[2];
	u_int8_t	mw_dma_tct[2]; /* multi-word DMA transfer cycle time */
	u_int8_t	min_PIO_tct_no_flow_control[2];
	u_int8_t	min_PIO_tct_with_flow_control[2];
	u_int8_t	reserved5[4];
	u_int8_t	reserved6[114];
	u_int8_t	vendor[64];	/* vendor unique */
	u_int8_t	reserved7[192];
};

struct at_dev_link {
	void	*device_softc;
	u_int8_t drive;
	u_int8_t openings;
	struct	atapi_identify id;  
	struct	bus_link *bus;
	u_int16_t flags;
#define ADEV_REMOVABLE		0x0001	/* media is removable */
#define ADEV_MEDIA_LOADED	0x0002	/* device figures are still valid */  
#define ADEV_WAITING		0x0004	/* a process is waiting for this */
#define ADEV_OPEN		0x0008	/* at least 1 open session */
#define ADEV_EJECTING		0x0010	/* eject on close */
#define ACAP_DRQ_MPROC		0x0000	/* microprocessor DRQ */
#define ACAP_DRQ_INTR		0x0100	/* interrupt DRQ */
#define ACAP_DRQ_ACCEL		0x0200	/* accelerated DRQ */
#define ACAP_LEN 		0x0400	/* 16 bit commands */
	u_int8_t quirks;		/* per-device oddities */
#define ADEV_CDROM		0x01	/* device is a CD-ROM */
#define ADEV_LITTLETOC		0x02	/* Audio TOC uses wrong byte order */
	void	(*start) __P((void *));	/* device start routine */
	int	(*done) __P((void *));	/* device done routine */
};

struct atapi_command_packet {
	void	*ad_link;
	void	*command;
	char	cmd_store[16];
	int	command_size;
	struct	buf *bp; 
	void	*databuf;
	int	data_size;
	int	flags;	/* handle B_READ/B_WRITE mask 0x00f00000 */
			/* controller flags maks 0x0000000f */
			/* ATAPI flags mask 0x000000f0 */
			/* Capabilities flags 0x00000f00 */
	u_int8_t drive;
	u_int16_t status;
#define STATUS_MASK	0xff
#define NO_ERROR	0x00
#define ERROR		0x01
#define MEDIA_CHANGE	0x02
#define END_OF_MEDIA	0x03
#define NOT_READY	0x10
#define UNIT_ATTENTION	0x20
#define RETRY		0x40
#define ITSDONE		0x100
	u_int8_t error;
	u_int8_t retries;
#define ATAPI_NRETRIES 5
	LIST_ENTRY(atapi_command_packet) free_list;
};

int	wdc_atapi_get_params __P((struct bus_link *, u_int8_t,
	    struct atapi_identify *)); 
void	wdc_atapi_send_command_packet __P((struct bus_link *,
	    struct atapi_command_packet*));

#define A_POLLED	0x10
#define A_NOSLEEP	0x20
#define A_SILENT	0x40

void	atapi_done __P((struct atapi_command_packet *));
struct	atapi_command_packet *atapi_get_pkt __P((struct at_dev_link *, int));
void	atapi_free_pkt __P((struct atapi_command_packet *));

/*
 * Functions used for reading and writing 2, 3, and 4 byte values
 * in ATAPI commands.
 */

static __inline void _lto2b __P((u_int32_t val, u_int8_t *bytes));
static __inline void _lto3b __P((u_int32_t val, u_int8_t *bytes));
static __inline void _lto4b __P((u_int32_t val, u_int8_t *bytes));
static __inline u_int32_t _2btol __P((u_int8_t *bytes));
static __inline u_int16_t _2btos __P((u_int8_t *bytes));
static __inline u_int32_t _3btol __P((u_int8_t *bytes));
static __inline u_int32_t _4btol __P((u_int8_t *bytes));

static __inline void _lto2l __P((u_int32_t val, u_int8_t *bytes));
static __inline void _lto3l __P((u_int32_t val, u_int8_t *bytes));
static __inline void _lto4l __P((u_int32_t val, u_int8_t *bytes));
static __inline u_int32_t _2ltol __P((u_int8_t *bytes));
static __inline u_int32_t _3ltol __P((u_int8_t *bytes));
static __inline u_int32_t _4ltol __P((u_int8_t *bytes));

static __inline void
_lto2b(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = (val >> 8) & 0xff;
	bytes[1] = val & 0xff;
}

static __inline void
_lto3b(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = (val >> 16) & 0xff;
	bytes[1] = (val >> 8) & 0xff;
	bytes[2] = val & 0xff;
}

static __inline void
_lto4b(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = (val >> 24) & 0xff;
	bytes[1] = (val >> 16) & 0xff;
	bytes[2] = (val >> 8) & 0xff;
	bytes[3] = val & 0xff;
}

static __inline u_int32_t
_2btol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = (bytes[0] << 8) |
	     bytes[1];
	return (rv);
}

static __inline u_int16_t
_2btos(bytes)
	u_int8_t *bytes;
{
	register u_int16_t rv;

	rv = (bytes[0] << 8) |
	     bytes[1];
	return (rv);
}

static __inline u_int32_t
_3btol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = (bytes[0] << 16) |
	     (bytes[1] << 8) |
	     bytes[2];
	return (rv);
}

static __inline u_int32_t
_4btol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = (bytes[0] << 24) |
	     (bytes[1] << 16) |
	     (bytes[2] << 8) |
	     bytes[3];
	return (rv);
}

static __inline void
_lto2l(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = val & 0xff;
	bytes[1] = (val >> 8) & 0xff;
}

static __inline void
_lto3l(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = val & 0xff;
	bytes[1] = (val >> 8) & 0xff;
	bytes[2] = (val >> 16) & 0xff;
}

static __inline void
_lto4l(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = val & 0xff;
	bytes[1] = (val >> 8) & 0xff;
	bytes[2] = (val >> 16) & 0xff;
	bytes[3] = (val >> 24) & 0xff;
}

static __inline u_int32_t
_2ltol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = bytes[0] |
	     (bytes[1] << 8);
	return (rv);
}

static __inline u_int32_t
_3ltol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = bytes[0] |
	     (bytes[1] << 8) |
	     (bytes[2] << 16);
	return (rv);
}

static __inline u_int32_t
_4ltol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = bytes[0] |
	     (bytes[1] << 8) |
	     (bytes[2] << 16) |
	     (bytes[3] << 24);
	return (rv);
}
