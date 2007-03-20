/*	$OpenBSD: atascsi.h,v 1.10 2007/03/20 05:33:02 pascoe Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

struct atascsi;
struct ata_xfer;

struct atascsi_methods {
	int			(*probe)(void *, int);
	struct ata_xfer *	(*ata_get_xfer)(void *, int );
	int			(*ata_cmd)(struct ata_xfer *);
};

struct atascsi_attach_args {
	void			*aaa_cookie;

	struct atascsi_methods	*aaa_methods;
	void			(*aaa_minphys)(struct buf *);
	int			aaa_nports;
	int			aaa_ncmds;
};

struct ata_port {
	struct atascsi		*ap_as;
	int			ap_port;
	int			ap_type;
#define ATA_PORT_T_NONE			0
#define ATA_PORT_T_DISK			1
#define ATA_PORT_T_ATAPI		2
};

/* These offsets correspond with the layout of the SATA Host to Device
 * and Device to Host FISes.  Do not change them. */
enum ata_register_map {
	REGS_TYPE = 0,
	FIS_TYPE = 0,
#define REGS_TYPE_REG_H2D		0x27
#define REGS_TYPE_REG_D2H		0x34
#define REGS_TYPE_SDB_D2H		0xA1
	H2D_DEVCTL_OR_COMMAND = 1,
#define H2D_DEVCTL_OR_COMMAND_DEVCTL	0x00
#define H2D_DEVCTL_OR_COMMAND_COMMAND	0x80
#define H2D_DEVCTL_OR_COMMAND_PMULT_MSK	0x0f
	D2H_INTERRUPT = 1,
#define D2H_INTERRUPT_INTERRUPT		0x40
	H2D_COMMAND = 2,
	D2H_STATUS = 2,
	H2D_FEATURES = 3,
	D2H_ERROR = 3,
	SECTOR_NUM = 4,
	LBA_LOW = 4,
	CYL_LOW = 5,
	LBA_MID = 5,
	CYL_HIGH = 6,
	LBA_HIGH = 6,
	DEVICE_HEAD = 7,
	DEVICE = 7,
	SECTOR_NUM_EXP = 8,
	LBA_LOW_EXP = 8,
	CYL_LOW_EXP = 9,
	LBA_MID_EXP = 9,
	CYL_HIGH_EXP = 10,
	LBA_HIGH_EXP = 10,
	FEATURES_EXP = 11,
	SECTOR_COUNT = 12,
	SECTOR_COUNT_EXP = 13,
	RESERVED_14 = 14,
	CONTROL = 15,
	RESERVED_16 = 16,
	RESERVED_17 = 17,
	RESERVED_18 = 18,
	RESERVED_19 = 19,
	MAX_ATA_REGS = 20
};

struct ata_regs {
	u_int8_t		regs[MAX_ATA_REGS];
};

struct ata_cmd {
	struct ata_regs		*tx;
	struct ata_regs		rx_err;

	u_int8_t		st_bmask;
	u_int8_t		st_pmask;
};

struct ata_xfer {
	struct ata_cmd		cmd;
	u_int8_t		*data;
	size_t			datalen;
	size_t			resid;

	void			(*complete)(struct ata_xfer *);
	struct timeout		stimeout;
	u_int			timeout;

	int			flags;
#define ATA_F_READ			(1<<0)
#define ATA_F_WRITE			(1<<1)
#define ATA_F_NOWAIT			(1<<2)
#define ATA_F_POLL			(1<<3)
	volatile int		state;
#define ATA_S_SETUP			0
#define ATA_S_PENDING			1
#define ATA_S_COMPLETE			2
#define ATA_S_ERROR			3
#define ATA_S_PUT			6

	void			*atascsi_private;

	void			(*ata_put_xfer)(struct ata_xfer *);
};

#define ATA_QUEUED		0
#define ATA_COMPLETE		1
#define ATA_ERROR		2

struct atascsi	*atascsi_attach(struct device *, struct atascsi_attach_args *);
int		atascsi_detach(struct atascsi *);

int		atascsi_probe_dev(struct atascsi *, int);
int		atascsi_detach_dev(struct atascsi *, int);
