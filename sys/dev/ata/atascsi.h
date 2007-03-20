/*	$OpenBSD: atascsi.h,v 1.17 2007/03/20 14:10:53 dlg Exp $ */

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

/*
 * ATA commands
 */

#define ATA_C_IDENTIFY		0xec
#define ATA_C_FLUSH_CACHE	0xe7
#define ATA_C_FLUSH_CACHE_EXT	0xea /* lba48 */
#define ATA_C_READDMA		0xc8
#define ATA_C_WRITEDMA		0xca
#define ATA_C_READDMA_EXT	0x25
#define ATA_C_WRITEDMA_EXT	0x35
#define ATA_C_PACKET		0xa0

struct ata_identify {
	u_int16_t	config;		/*   0 */
	u_int16_t	ncyls;		/*   1 */
	u_int16_t	reserved1;	/*   2 */
	u_int16_t	nheads;		/*   3 */
	u_int16_t	track_size;	/*   4 */
	u_int16_t	sector_size;	/*   5 */
	u_int16_t	nsectors;	/*   6 */
	u_int16_t	reserved2[3];	/*   7 vendor unique */
	u_int8_t	serial[20];	/*  10 */
	u_int16_t	buffer_type;	/*  20 */
	u_int16_t	buffer_size;	/*  21 */
	u_int16_t	ecc;		/*  22 */
	u_int8_t	firmware[8];	/*  23 */
	u_int8_t	model[40];	/*  27 */
	u_int16_t	multi;		/*  47 */
	u_int16_t	dwcap;		/*  48 */
	u_int16_t	cap;		/*  49 */
	u_int16_t	reserved3;	/*  50 */
	u_int16_t	piomode;	/*  51 */
	u_int16_t	dmamode;	/*  52 */
	u_int16_t	validinfo;	/*  53 */
	u_int16_t	curcyls;	/*  54 */
	u_int16_t	curheads;	/*  55 */
	u_int16_t	cursectrk;	/*  56 */
	u_int16_t	curseccp[2];	/*  57 */
	u_int16_t	mult2;		/*  59 */
	u_int16_t	addrsec[2];	/*  60 */
	u_int16_t	worddma;	/*  62 */
	u_int16_t	dworddma;	/*  63 */
	u_int16_t	advpiomode;	/*  64 */
	u_int16_t	minmwdma;	/*  65 */
	u_int16_t	recmwdma;	/*  66 */
	u_int16_t	minpio;		/*  67 */
	u_int16_t	minpioflow;	/*  68 */
	u_int16_t	reserved4[2];	/*  69 */
	u_int16_t	typtime[2];	/*  71 */
	u_int16_t	reserved5[2];	/*  73 */
	u_int16_t	qdepth;		/*  75 */
	u_int16_t	satacap;	/*  76 */
	u_int16_t	reserved6;	/*  77 */
	u_int16_t	satafsup;	/*  78 */
	u_int16_t	satafen;	/*  79 */
	u_int16_t	majver;		/*  80 */
	u_int16_t	minver;		/*  81 */
	u_int16_t	cmdset82;	/*  82 */
	u_int16_t	cmdset83;	/*  83 */
	u_int16_t	cmdset84;	/*  84 */
	u_int16_t	features85;	/*  85 */
	u_int16_t	features86;	/*  86 */
	u_int16_t	features87;	/*  87 */
	u_int16_t	ultradma;	/*  88 */
	u_int16_t	erasetime;	/*  89 */
	u_int16_t	erasetimex;	/*  90 */
	u_int16_t	apm;		/*  91 */
	u_int16_t	masterpw;	/*  92 */
	u_int16_t	hwreset;	/*  93 */
	u_int16_t	acoustic;	/*  94 */
	u_int16_t	stream_min;	/*  95 */
	u_int16_t	stream_xfer_d;	/*  96 */
	u_int16_t	stream_lat;	/*  97 */
	u_int16_t	streamperf[2];	/*  98 */
	u_int16_t	addrsecxt[4];	/* 100 */
	u_int16_t	stream_xfer_p;	/* 104 */
	u_int16_t	padding1;	/* 105 */
	u_int16_t	phys_sect_sz;	/* 106 */
	u_int16_t	seek_delay;	/* 107 */
	u_int16_t	naa_ieee_oui;	/* 108 */
	u_int16_t	ieee_oui_uid;	/* 109 */
	u_int16_t	uid_mid;	/* 110 */
	u_int16_t	uid_low;	/* 111 */
	u_int16_t	resv_wwn[4];	/* 112 */
	u_int16_t	incits;		/* 116 */
	u_int16_t	words_lsec[2];	/* 117 */
	u_int16_t	cmdset119;	/* 119 */
	u_int16_t	features120;	/* 120 */
	u_int16_t	padding2[6];
	u_int16_t	rmsn;		/* 127 */
	u_int16_t	securestatus;	/* 128 */
	u_int16_t	vendor[31];	/* 129 */
	u_int16_t	padding3[16];	/* 160 */
	u_int16_t	curmedser[30];	/* 176 */
	u_int16_t	sctsupport;	/* 206 */
	u_int16_t	padding4[48];	/* 207 */
	u_int16_t	integrity;	/* 255 */
} __packed;

/*
 * ATA interface
 */

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
#define H2D_DEVICE_LBA			0x40
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
	u_int8_t		*packetcmd;
	u_int8_t		tag;

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
#define ATA_F_PIO			(1<<4)
#define ATA_F_PACKET			(1<<5)
	volatile int		state;
#define ATA_S_SETUP			0
#define ATA_S_PENDING			1
#define ATA_S_COMPLETE			2
#define ATA_S_ERROR			3
#define ATA_S_TIMEOUT			4
#define ATA_S_ONCHIP			5
#define ATA_S_PUT			6

	void			*atascsi_private;

	void			(*ata_put_xfer)(struct ata_xfer *);
};

#define ATA_QUEUED		0
#define ATA_COMPLETE		1
#define ATA_ERROR		2

/*
 * atascsi
 */

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

struct atascsi	*atascsi_attach(struct device *, struct atascsi_attach_args *);
int		atascsi_detach(struct atascsi *);

int		atascsi_probe_dev(struct atascsi *, int);
int		atascsi_detach_dev(struct atascsi *, int);
