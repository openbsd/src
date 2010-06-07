/*	$OpenBSD: eeprom.h,v 1.12 2010/06/07 19:43:49 miod Exp $	*/

/*
 * Copyright (c) 1995 Theo de Raadt
 * All rights reserved.
 * Portions Copyright (c) 1997, Jason Downs.  All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This structure defines the contents of the EEPROM for all Sun
 * Workstations with the SunMon monitor.
 *
 * The EEPROM is divided into a diagnostic section, a reserved section,
 * a ROM section, and a software section (defined in detail elsewhere).
 */

/*
 * Note that most places where the PROM stores a "true/false" flag,
 * the true value is 0x12 and false is the usual zero.  Such flags
 * all take the values EE_TRUE or EE_FALSE so this file does not
 * need to define so many value macros.
 */
#define EE_TRUE		0x12
#define EE_FALSE	0x00

struct ee_keymap {
	u_char	keymap[128];	/* PROM/EEPROM are 7 bit */
};

#define PW_SIZE	8			/* max size of a password entry */

struct password_inf {
	u_short	bad_counter;		/* illegal password count */
	char	pw_mode;		/* mode */
	char	pw_bytes[PW_SIZE];	/* password */
};
#define NON_SECURE_MODE		0x00	/* non-secure */
#define COMMAND_SECURE_MODE	0x01	/* command secure */
#define FULLY_SECURE_MODE	0x5e	/* fully secure */
					/* 0, 2-0x5d, 0x5f-0xff: non secure*/


#define MAX_SLOTS	12
#define CONFIG_SIZE	16	/* bytes of config data for each slot */

struct eeprom {
	struct ee_diag {
		u_int	eed_test;	/* 0x000: diagnostic test write area */
		u_short	eed_wrcnt[3];	/* 0x004: diag area write count (3 copies) */
		short	eed_nu1;

		u_char	eed_chksum[3];	/* 0x00c: diag area checksum (3 copies) */
		char	eed_nu2;
		time_t	eed_hwupdate;	/* 0x010: date of last hardware update */

		char	eed_memsize;	/* 0x014: MB's of memory in system */
		char	eed_memtest;	/* 0x015: MB's of memory to test */

		char	eed_scrsize;	/* 0x016: screen size */
#define	EED_SCR_1152X900	0x00
#define	EED_SCR_1024X1024	0x12
#define EED_SCR_1600X1280	0x13
#define EED_SCR_1440X1440	0x14
#define EED_SCR_640X480		0x15
#define EED_SCR_1280X1024	0x16

		char	eed_dogaction;	/* 0x017: watchdog reset action */
#define	EED_DOG_MONITOR		0x00	/* return to monitor command level */
#define	EED_DOG_REBOOT		0x12	/* perform power on reset and reboot */

		char	eed_defboot;	/* 0x018: default boot? */
#define	EED_DEFBOOT		0x00	/* do default boot */
#define	EED_NODEFBOOT		0x12	/* don't do default boot */

		char	eed_bootdev[2];	/* 0x019: boot device name (e.g. xy, ie) */

		u_char	eed_bootctrl;	/* 0x01b: controller number */
		u_char	eed_bootunit;	/* 0x01c: unit number */
		u_char	eed_bootpart;	/* 0x01d: partition number */

		char	eed_kbdtype;	/* 0x01e: non-Sun keyboard type - for OEM's */
#define	EED_KBD_SUN	0		/* one of the Sun keyboards */

		char	eed_console;	/* 0x01f: console device */
#define	EED_CONS_BW	0x00		/* use b&w monitor */
#define	EED_CONS_TTYA	0x10		/* use tty A port */
#define	EED_CONS_TTYB	0x11		/* use tty B port */
#define	EED_CONS_COLOR	0x12		/* use color monitor */
#define EED_CONS_P4	0x20		/* use the P4 monitor */

		char	eed_showlogo;	/* 0x020: display Sun logo? */
#define	EED_LOGO	0x00
#define	EED_NOLOGO	0x12

		char	eed_keyclick;	/* 0x021: keyboard click? */
#define	EED_NOKEYCLICK	0x00
#define	EED_KEYCLICK	0x12

		char	eed_diagdev[2];	/* 0x022: boot device name (e.g. xy, ie) */
		u_char	eed_diagctrl;	/* 0x024: controller number */
		u_char	eed_diagunit;	/* 0x025: unit number */
		u_char	eed_diagpart;	/* 0x026: partition number */
#define	EED_WOB	0x12			/* bring system up white on black */

		u_char	eed_videobg;
		char	eed_diagpath[40]; /* 0x028: boot path of diagnostic */
#define EED_TERM_34x80	00
#define EED_TERM_48x120	0x12		/* for large scrn size 1600x1280 */
		u_char	eed_colsize;	/* 0x050: number of columns */
		u_char	eed_rowsize;	/* 0x051: number of rows */

		char	eed_nu5[6];

		struct eed_tty_def {	/* 0x058: tty port defaults */
			char	eet_sel_baud;	/* user specifies baud rate */
#define	EET_DEFBAUD	0x00
#define	EET_SELBAUD	0x12
			u_char	eet_hi_baud;	/* upper byte of baud rate */
			u_char	eet_lo_baud;	/* lower byte of baud rate */
			u_char	eet_rtsdtr;	/* flag for dtr and rts */
#define NO_RTSDTR	0x12
			char	eet_unused[4];
		} eed_ttya_def, eed_ttyb_def;

		char	eed_banner[80];	/* 0x068: replacement banner */
		/* last two chars must be \r\n (XXX - why not \0?) */

		u_short	eed_pattern;	/* 0x0b8: test pattern - must contain 0xAA55 */
		short	eed_nu6;
		struct eed_conf {	/* 0x0bc: system configuration, by slot */
			union {
				struct eec_gen {
					u_char	eec_type;	/* board type code */
					char	eec_size[CONFIG_SIZE - 1];
				} eec_gen;

				char	conf_byte[CONFIG_SIZE];
				u_char	eec_type;	/* type of this board */
#define	EEC_TYPE_NONE	0	/* no board this slot */
#define	EEC_TYPE_CPU	0x01	/* cpu */
				struct eec_cpu {
					u_char	eec_type;		/* board type */
					u_char	eec_cpu_memsize;	/* MB's on cpu */
					int	eec_cpu_unused:6;
					int	eec_cpu_dcp:1;		/* dcp? */
					int	eec_cpu_68881:1;	/* 68881? */
					u_char	eec_cpu_cachesize;	/* KB's cache */
				} eec_cpu;
				struct eec_cpu_alt {
					u_char	eec_type;	/* board type */
					u_char	memsize;	/* MB's on cpu */
					u_char	option;	/* option flags */
#define CPU_HAS_DCP	0x02
#define CPU_HAS_68881	0x01
					u_char	cachesize;	/* KB's in cache */
				} eec_cpu_alt;

#define	EEC_TYPE_MEM	0x02	/* memory board */
				struct eec_mem {
					u_char	eec_type;	/* board type */
					u_char	eec_mem_size;	/* MB's on card */
				} eec_mem;

#define	EEC_TYPE_COLOR	0x03	/* color frame buffer */
				struct eec_color {
					u_char	eec_type;	/* board type */
					char	eec_color_type;
#define	EEC_COLOR_TYPE_CG2	2	/* cg2 color board */
#define	EEC_COLOR_TYPE_CG3	3	/* cg3 color board */
#define	EEC_COLOR_TYPE_CG5	5	/* cg5 color board */
				} eec_color;

#define	EEC_TYPE_BW	0x04	/* b&w frame buffer */

#define	EEC_TYPE_FPA	0x05	/* floating point accelerator */

#define	EEC_TYPE_DISK	0x06	/* SMD disk controller */
				struct eec_disk {
					u_char	eec_type;	/* board type */
					char	eec_disk_type;	/* controller type */
#define EEC_DISK_TYPE_X450	1
#define EEC_DISK_TYPE_X451	2
					char	eec_disk_ctlr;	/* controller number */
					char	eec_disk_disks;	/* number of disks */
					char	eec_disk_cap[4];	/* capacity */
#define EEC_DISK_NONE	0
#define EEC_DISK_130	1
#define EEC_DISK_280	2
#define EEC_DISK_380	3
#define EEC_DISK_575	4
#define EEC_DISK_900	5
				} eec_disk;

#define	EEC_TYPE_TAPE	0x07	/* 1/2" tape controller */
				struct eec_tape {
					u_char	eec_type;	/* board type */
					char	eec_tape_type;	/* controller type */
#define	EEC_TAPE_TYPE_XT	1	/* Xylogics 472 */
#define	EEC_TAPE_TYPE_MT	2	/* TapeMaster */
					char	eec_tape_ctlr;	/* controller number */
					char	eec_tape_drives;	/* number of drives */
				} eec_tape;

#define	EEC_TYPE_ETHER	0x08	/* Ethernet controller */

#define	EEC_TYPE_TTY	0x09	/* terminal multiplexer */
				struct eec_tty {
					u_char	eec_type;	/* board type */
					char	eec_tty_lines;	/* number of lines */
#define MAX_TTY_LINES	16
					char	manufacturer;	/* code for maker */
#define EEC_TTY_UNKNOWN	0
#define EEC_TTY_SYSTECH	1
#define EEC_TTY_SUN	2
				} eec_tty;

#define	EEC_TYPE_GP	0x0a	/* graphics processor/buffer */
				struct eec_gp {
					u_char	eec_type;	/* board type */
					char	eec_gp_type;
#define EEC_GP_TYPE_GPPLUS	1	/* GP+ graphic processor board */
#define EEC_GP_TYPE_GP2		2	/* GP2 graphic processor board */
				} eec_gp;

#define	EEC_TYPE_DCP	0x0b	/* DCP ??? XXX */

#define	EEC_TYPE_SCSI	0x0c	/* SCSI controller */
				struct eec_scsi {
					u_char	eec_type;	/* board type */
					char	eec_scsi_type;	/* host adaptor type */
#define EEC_SCSI_SUN2	2
#define EEC_SCSI_SUN3	3
					char	eec_scsi_tapes;	/* number of tapes */
					char	eec_scsi_disks;	/* number of disks */
					char	eec_scsi_tape_type;
#define EEC_SCSI_SYSG	1
#define EEC_SCSI_MT02	2
					char	eec_scsi_disk_type;
#define EEC_SCSI_MD21	1
#define EEC_SCSI_ADAPT	2
					char	eec_scsi_driv_code[2];
#define EEC_SCSI_D71	1
#define EEC_SCSI_D141	2
#define EEC_SCSI_D327	3
				} eec_scsi;
#define EEC_TYPE_IPC	0x0d

#define EEC_TYPE_GB	0x0e

#define EEC_TYPE_SCSI375	0x0f

#define EEC_TYPE_MAPKIT	0x10
				struct eec_mapkit {
					u_char	eec_type;	/* board type */
					char	eec_mapkit_hw;	/* whether INI */
#define EEC_TYPE_MAPKIT_INI	1
				} eec_mapkit;

#define EEC_TYPE_CHANNEL	0x11
#define EEC_TYPE_ALM2		0x12


				struct eec_generic_net {
#define EEC_TYPE_GENERIC_NET  0x13
					u_char	eec_devtype;	/* board type */
					u_char	eec_mem_type;	/* Memory type */
					u_char	gn_nu1;
					u_char	gn_nu2;
#define ID_VME		0x0	/* Data buffers are in VME space(Shared) */
#define ID_DVMA		0x1	/* or DVMA buffer memory type */
					char	*dev_reg_ptr;
				} eec_generic_net;


#define	EEC_TYPE_END	0xff	/* end of card cage */
			} eec_un;
		} eed_conf[MAX_SLOTS + 1];
#define EEPROM_TABLE  0x58	/* 1 indicates alternate key table. */
		u_char	which_kbt;	/* 0x18C: which keytable? */

		/*
		 * Note. The following contains the full keyboard id
		 * returned by the type4 transmit layout command. It is not
		 * necessarily anything to do with locale of operation.
		 */
		u_char	ee_kb_type;
		u_char	ee_kb_id;	/* 0x18E: keyboard id in case of EEPROM table */
#define EEPROM_LOGO 0x12
		u_char	otherlogo;	/* 0x18F: True if eeprom logo  needed */
		struct ee_keymap ee_keytab_lc[1];	/* 0x190: */
		struct ee_keymap ee_keytab_uc[1];	/* 0x210: */
		u_char	ee_logo[64][8];	/* 0x290: A 64X64 bit space for custom/OEM designed logo icon */
		struct password_inf ee_password_inf;	/* 0x490: reserved */
		char	eed_resv[0x500 - 0x49c];	/* 0x49c: reserved */
	} ee_diag;

	struct ee_resv {	/* reserved area of EEPROM */
		u_short	eev_wrcnt[3];	/* 0x500: write count (3 copies) */
		short	eev_nu1;
		u_char	eev_chksum[3];	/* 0x508: reserved area checksum (3 copies) */
		char	eev_nu2;
		char	eev_resv[0x600 - 0x50c];	/* 0x50c: */
	} ee_resv;

	struct ee_rom {		/* ROM area of EEPROM */
		u_short	eer_wrcnt[3];	/* 0x600: write count (3 copies) */
		short	eer_nu1;
		u_char	eer_chksum[3];	/* 0x608: ROM area checksum (3 copies) */
		char	eer_nu2;
		char	eer_resv[0x700 - 0x60c];	/* 0x60c: */
	} ee_rom;

	/*
	 * The following area is reserved for software (i.e. UNIX).
	 * The actual contents of this area are defined elsewhere.
	 */
	struct ee_softresv {	/* software area of EEPROM */
		u_short	ees_wrcnt[3];	/* 0x700: write count (3 copies) */
		short	ees_nu1;
		u_char	ees_chksum[3];	/* 0x708: software area checksum (3 copies) */
		char	ees_nu2;
		char	ees_resv[0x800 - 0x70c];	/* 0x70c: */
	} ee_soft;
};

#define EEPROM_BASE     0xffd04000

/*
 * The size of the eeprom on machines with the old clock is 2k.  However,
 * on machines with the new clock (and the `eeprom' in the nvram area)
 * there are only 2040 bytes available. (???).  Since we really only
 * care about the `diagnostic' area, we'll use its size when dealing
 * with the eeprom in general.
 */
#define EEPROM_SIZE             0x500

#ifdef _KERNEL
extern	char *eeprom_va;
int	eeprom_uio(struct uio *);
#endif /* _KERNEL */
