/*	$OpenBSD: prom.h,v 1.5 2004/01/24 21:10:31 miod Exp $ */
/*
 * Copyright (c) 2001 Steve Murphree, Jr.
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __MACHINE_PROM_H__
#define __MACHINE_PROM_H__

#define MVMEPROM_INCHR		0x0000
#define MVMEPROM_INSTAT		0x0001
#define MVMEPROM_INLN		0x0002
#define MVMEPROM_READSTR	0x0003
#define MVMEPROM_READLN		0x0004
#define MVMEPROM_CHKBRK		0x0005
#define MVMEPROM_DSKRD		0x0010
#define MVMEPROM_DSKWR		0x0011
#define MVMEPROM_DSKCFIG	0x0012
#define MVMEPROM_DSKFMT		0x0014
#define MVMEPROM_DSKCTRL	0x0015
#define MVMEPROM_NETRD		0x0018
#define MVMEPROM_NETWR		0x0019
#define MVMEPROM_NETCFIG	0x001a
#define MVMEPROM_NETFOPEN	0x001b
#define MVMEPROM_NETFREAD	0x001c
#define MVMEPROM_NETCTRL	0x001d
#define MVMEPROM_OUTCHR		0x0020
#define MVMEPROM_OUTSTR		0x0021
#define MVMEPROM_OUTLN		0x0022
#define MVMEPROM_WRITE		0x0023
#define MVMEPROM_WRITELN	0x0024
#define MVMEPROM_WRITDLN	0x0025
#define MVMEPROM_PCRLF		0x0026
#define MVMEPROM_ERASLN		0x0027
#define MVMEPROM_WRITD		0x0028
#define MVMEPROM_SNDBRK		0x0029
#define MVMEPROM_DELAY		0x0043
#define MVMEPROM_RTC_TM		0x0050
#define MVMEPROM_RTC_DT		0x0051
#define MVMEPROM_RTC_DSP	0x0052
#define MVMEPROM_RTC_RD		0x0053
#define MVMEPROM_REDIR		0x0060
#define MVMEPROM_REDIR_I	0x0061
#define MVMEPROM_REDIR_O	0x0062
#define MVMEPROM_EXIT		0x0063
#define MVMEPROM_RETURN		MVMEPROM_EXIT
#define MVMEPROM_BINDEC		0x0064
#define MVMEPROM_CHANGEV	0x0067
#define MVMEPROM_STRCMP		0x0068
#define MVMEPROM_MUL32		0x0069
#define MVMEPROM_DIV32		0x006a
#define MVMEPROM_CHKSUM		0x006b
#define MVMEPROM_BRD_ID		0x0070
#define MVMEPROM_ENVIRON	0x0071
#define MVMEPROM_PFLASH		0x0073
#define MVMEPROM_DIAGFCN	0x0074
#define MVMEPROM_SIOPEPS	0x0090
#define MVMEPROM_FORKMPU	0x0100
#define MVMEPROM_FORKMPUR	0x0101
#define MVMEPROM_IDELMPU	0x0110
#define MVMEPROM_IOINQ		0x0120
#define MVMEPROM_IOINFORM	0x0124
#define MVMEPROM_IOCONFIG	0x0128
#define MVMEPROM_IODELETE	0x012c
#define MVMEPROM_SYMBOLTA	0x0130
#define MVMEPROM_SYMBOLTD	0x0131

#define NETCTRLCMD_GETETHER	1
#define ENVIRONCMD_WRITE	1
#define ENVIRONCMD_READ		2
#define ENVIRONTYPE_EOL		0
#define ENVIRONTYPE_START	1
#define ENVIRONTYPE_DISKBOOT	2
#define ENVIRONTYPE_ROMBOOT	3
#define ENVIRONTYPE_NETBOOT	4
#define ENVIRONTYPE_MEMSIZE	5

#define NETSTATUS_SUCCESS	0x00
#define NETSTATUS_MISALNG	0x01
#define NETSTATUS_BUFFLMT	0x02
#define NETSTATUS_BADLEN	0x03
#define NETSTATUS_INITABRT	0x04
#define NETSTATUS_TXABRT	0x05
#define NETSTATUS_PCIADDRERR	0x06
#define NETSTATUS_NOPORT	0x07
#define NETSTATUS_ILLIPL	0x08
#define NETSTATUS_USERABRT	0x09
#define NETSTATUS_TIMEOUT	0x0A
#define NETSTATUS_SYSERR	0x10
#define NETSTATUS_TXBABBLE	0x11
#define NETSTATUS_TXCOL		0x12
#define NETSTATUS_TXSTOPPED	0x13
#define NETSTATUS_TXUNDERFL	0x14
#define NETSTATUS_TXLATECOL	0x15
#define NETSTATUS_TXLOSTCARR	0x16
#define NETSTATUS_TXLINKFAIL	0x17
#define NETSTATUS_TXNOCARR	0x18
#define NETSTATUS_TXTOPHY	0x19
#define NETSTATUS_RXCRCERR	0x20
#define NETSTATUS_RXOVERFL	0x21
#define NETSTATUS_RXFRAMEERR	0x22
#define NETSTATUS_RXLDFNS	0x23
#define NETSTATUS_RXFDCOL	0x24
#define NETSTATUS_RXRUNTFRAME	0x25
#define NETSTATUS_TXTONORM	0x28
#define NETSTATUS_TXTOSETUP	0x29
#define NETSTATUS_SROMERR	0x30

#define NETCTRLCMD_INIT		0
#define NETCTRLCMD_GETETHER	1
#define NETCTRLCMD_TX		2
#define NETCTRLCMD_RX		3
#define NETCTRLCMD_FLUSH	4
#define NETCTRLCMD_RESET	5

#define NETCFG_FLAG_RD		0
#define NETCFG_FLAG_WR		1
#define NETCFG_FLAG_WRNV	2

#ifndef LOCORE
extern struct bugenviron bugenviron;
extern int bugenv_init;

#define BUG_ENV_END		0
#define BUG_STARTUP_PARAM	1
struct bug_startup {
	char s_mode;
	char s_menu;
	char s_remotestart;
	char s_probe;
	char s_negsysfail;
	char s_resetscsi;
	char s_nocfblk;
	char s_scsisync;
};

#define BUG_AUTOBOOT_INFO	2
struct bug_autoboot {
	char b_enable;
	char b_poweruponly;
	char b_clun;
	char b_dlun;
	char b_delay;
	char b_string[22]; /* 0x15 + 0x1 */
};

#define BUG_ROMBOOT_INFO	3
struct bug_romboot {
	char r_enable;
	char r_poweruponly;
	char r_bootvme;
	char r_delay;
	unsigned r_start;
	unsigned r_end;
};

#define BUG_NETBOOT_INFO	4
struct bug_netboot {
	char n_enable;
	char n_poweruponly;
	char n_clun;
	char n_dlun;
	char n_delay;
	char *n_param;
};

#define BUG_MEMORY_INFO		5
struct bug_memory {
	char m_sizeenable;
	unsigned m_start;
	unsigned m_end;
};

struct bugenviron {
        struct bug_startup  s;
        struct bug_autoboot b;
	struct bug_romboot  r;
	struct bug_netboot  n;
        struct bug_memory   m;
};

#define bug_localmemsize()      (bugenviron.m.m_end - bugenviron.m.m_start)
#define bug_localmemstart()	(bugenviron.m.m_start)
#define bug_localmemend()	(bugenviron.m.m_end)

struct mvmeprom_netio {
	u_char	clun;
	u_char	dlun;
	u_short	status;
	void	*addr;
	u_long	tlen;
	u_long	offset;
	u_long	ttime;
	u_long	tbytes;
	char 	filename[64];
};

struct mvmeprom_netfopen {
	u_char	clun;
	u_char	dlun;
	u_short	status;
	char 	filename[64];
};

struct mvmeprom_netfread {
	u_char	clun;
	u_char	dlun;
	u_short	status;
	void	*addr;
	u_short	bytes;
	u_short	blk;
	u_long	timeout;
};

struct mvmeprom_netctrl {
	u_char	clun;
	u_char	dlun;
	u_short	status;
	u_long	cmd;
	void	*addr;
	u_long	len;
	u_long	flags;
};

struct mvmeprom_netparam {
	u_long	ver;
	void *	nodeaddr;
	void *	loadaddr;
	void *	execaddr;
	u_long	delay;
	u_long	length;
	u_long	offset;
	void *	traceaddr;
	u_long	client_ip;
	u_long	server_ip;
	u_long	subnet;
	u_long	bcast;
	u_long	gateway_ip;
	u_char	rarp_retry;
	u_char	tftp_retry;
	u_char	rarp_cntl;
	u_char	update_cntl;
	char 	filename[64];
	char 	args[64];
};

struct mvmeprom_netcfg {
	u_char	clun;
	u_char	dlun;
	u_short	status;
	struct mvmeprom_netparam *netparam;
	u_long	flag;
};

struct prom_environ_hdr {
	u_char	type;
	u_char	len;
};

struct mvmeprom_brdid {
	u_long	eye_catcher;		/* "BDID" */ 
	u_char	rev;
	u_char	month;
	u_char	day;
	u_char	year;
	u_short	size;			/* BID packet length */
	u_short	rsv1;
	u_short	model;			/* e.g. 1603, 1604 */
	u_short	suffix;			/* e.g. AT */
	u_long	options;		/* Board options */
	u_short	ctrlun;			/* boot clun */
	u_short	devlun;                 /* boot dlun */
	u_short	devtype;		/* boot device type */
	u_short	devnum;			/* boot device number */
	u_long	opt2;			/* reserved */
	u_char	version[4];
	/* the folowing are CNFG values */
	u_char	board_serial[12];	/* SBC serial number */
	u_char	board_id[16];		/* SBC id */
	u_char	pwa_id[16];		/* printed wiring assembly id */
	u_char	old_speed[4];		/* old cpu speed field */
	u_char	etheraddr[6];		/* mac address, all zero if no ether */
	u_char	fill[2];		
	u_char	scsi_id[2];		/* local SCSI id */
	u_char	speed[3];		/* cpu speed */
	u_char	bus_speed[3];		/* pci bus speed */
	u_char	sys_serial[16];		/* system serial (user)*/
	u_char	sys_id[31];		/* system id (user)*/
	u_char	license_id[9];		/* license ID (for AIX)*/
};

struct mvmeprom_time {
        u_char	year_BCD;
        u_char	month_BCD;
        u_char	day_BCD;
        u_char	wday_BCD;
        u_char	hour_BCD;
        u_char	min_BCD;
        u_char	sec_BCD;
        u_char	cal_BCD;
};

struct mvmeprom_dskio {
	u_char	ctrl_lun;
	u_char	dev_lun;
	u_short	status;
	void	*pbuffer;
	u_long	blk_num;
	u_short	blk_cnt;
	u_char	flag;
#define BUG_FILE_MARK	0x80
#define IGNORE_FILENUM	0x02
#define END_OF_FILE	0x01
	u_char	addr_mod;
};
#define MVMEPROM_BLOCK_SIZE	256

struct mvmeprom_args {
        u_int	dev_lun;
        u_int	ctrl_lun;
        u_int	flags;
        u_int	ctrl_addr;
        u_int	entry;
        u_int	conf_blk;
        char	*arg_start;
        char	*arg_end;
	char	*nbarg_start;
	char	*nbarg_end;
	u_int	cputyp;
};

#endif

#define MVMEPROM_REG_CTRLLUN	"3"
#define MVMEPROM_REG_DEVLUN	"4"
#define MVMEPROM_REG_SCSUPP	"5"
#define MVMEPROM_REG_CTRLADDR	"6"
#define MVMEPROM_REG_ENTRY	"7"
#define MVMEPROM_REG_IPA	"8"
#define MVMEPROM_REG_ARGSTART	"9"
#define MVMEPROM_REG_ARGEND	"10"
#define MVMEPROM_REG_NBARGSTART	"11"
#define MVMEPROM_REG_NBARGEND	"12"

#ifndef RB_NOSYM
#define RB_NOSYM 0x4000
#endif
#endif /* __MACHINE_PROM_H__ */
