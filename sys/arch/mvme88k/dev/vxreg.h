/*	$OpenBSD: vxreg.h,v 1.7 2004/05/25 21:21:24 miod Exp $ */

/*
 * Copyright (c) 1999 Steve Murphree, Jr. All rights reserved.
 *
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
 *	This product includes software developed by Dale Rahn.
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

/* IPC - Intelligent Peripheral Controller */

struct vxreg {
/*0x0*/volatile u_short ipc_addrh;	 /* IPC addr reg, most significant */
/*0x2*/volatile u_short ipc_addrl;	 /* IPC addr reg, least significant */
/*0x4*/volatile u_char  ipc_amr;	  /* IPC address modifier reg */
/*0x5*/volatile u_char  unused1;
/*0x6*/volatile u_short ipc_cr;	 /* IPC control reg */
/*0x8*/volatile u_short ipc_sr;	 /* IPC status reg */
/*0xA*/volatile u_char  ipc_mdbp;	/* IPC model data byte pointer */
/*0xB*/volatile u_char  reserved3;
/*0xC*/volatile u_char  ipc_avrp;	/* IPC abort vector reg pointer */
/*0xD*/volatile u_char  unused2;
/*0xE*/volatile u_short ipc_tas;		 /* IPC test and set reg */
};

#define IPC_CR_SYSFI          0x1000   /* inhibit sysfail */
#define IPC_CR_ATTEN          0x2000   /* attention bit */
#define IPC_CR_RESET          0x4000   /* reset bit */
#define IPC_CR_BUSY           0x8000   /* busy bit */

#define IPC_SR_BUSERROR       0x4000   /* bus error */
#define IPC_SR_ERROR          0x8000   /* general error */
#define IPC_SR_INVAL          0xFFFF   /* invalid command */

#define IPC_TAS_COMPLETE      0x1000
#define IPC_TAS_VALID_STATUS  0x2000
#define IPC_TAS_VALID_CMD     0x4000
#define IPC_TAS_TAS           0x8000

#define IPC_CSR_CREATE        0x0001
#define IPC_CSR_DELETE        0x0002

#define CSW_OFFSET            0x0010

#define CMD_INIT              0x0000
#define CMD_READW             0x0001
#define CMD_WRITEW            0x0002
#define CMD_OPEN              0x0003
#define CMD_IOCTL             0x0004
#define CMD_CLOSE             0x0005
#define CMD_EVENT             0x0006
#define CMD_PROCESSED         0x00FF

#define IOCTL_LDOPEN          0x4400
#define IOCTL_LDCLOSE         0x4401
#define IOCTL_LDCHG           0x4402
#define IOCTL_LDGETT          0x4408
#define IOCTL_LDSETT          0x4409
#define IOCTL_TCGETA          0x5401   /* get dev termio struct */
#define IOCTL_TCSETA          0x5402   /* set dev termio struct */
#define IOCTL_TCSETAW         0x5403   /* set dev termio struct - wait */
#define IOCTL_TCSETAF         0x5404   /* set dev termio struct - wait - flush */
#define IOCTL_TCSBRK          0x5405   /* transmit a break seq */
#define IOCTL_TCXONC          0x5406   /* sus or res, xon or xoff, RTS or DTR */
#define IOCTL_TCFLSH          0x5407   /* Flush */
#define IOCTL_TCSETHW         0x5440   /* enable/disable HW handshake */
#define IOCTL_TCGETHW         0x5441   /* get current HW handshake info */
#define IOCTL_TCGETDL         0x5442   /* get daownloadable addr and mem size */
#define IOCTL_TCDLOAD         0x5443   /* download code/data to mem */
#define IOCTL_TCLINE          0x5444   /* copy line discipline */
#define IOCTL_TCEXEC          0x5445   /* exec code in local mem */
#define IOCTL_TCGETVR         0x5446   /* get version and revison of firmware */
#define IOCTL_TCGETDF         0x5447   /* get default termio stuct */
#define IOCTL_TCSETDF         0x5448   /* set default termio stuct */
#define IOCTL_TCGETSYM        0x5449   /* get firmware symbol table */
#define IOCTL_TCWHAT          0x544A   /* get all SCSI IDs of FW files */
#define IOCTL_TIOGETP         0x7408   /* get devs curr termio struct by sgttyb */
#define IOCTL_TIOSETP         0x7409   /* set devs curr termio struct by sgttyb */

#define IPC_EIO               5     /* I/O error */
#define IPC_ENXIO             6     /* no such device or address */
#define IPC_ENOMEM            12    /* not enough space */
#define IPC_EEXIST            17    /* device or address exists */
#define IPC_EINVAL            22    /* invalid caommand argument */

/*
 *	Index into c_cc[VNCC];
 */
#define  VVINTR   0  /* ISIG */
#define  VVQUIT   1  /* ISIG */
#define  VVERASE  2  /* ICANON */
#define  VVKILL   3  /* ICANON */
#define  VVEOF    4  /* ICANON */
#define  VVEOL    5  /* ICANON */
#define  VVSWTCH  6

/*
 * Input flags - software input processing
 */
#define  VIGNBRK  0000001   /* ignore BREAK condition */
#define  VBRKINT  0000002   /* map BREAK to SIGINTR */
#define  VIGNPAR  0000004   /* ignore (discard) parity errors */
#define  VPARMRK  0000010   /* mark parity and framing errors */
#define  VINPCK   0000020   /* enable checking of parity errors */
#define  VISTRIP  0000040   /* strip 8th bit off chars */
#define  VINLCR   0000100   /* map NL into CR */
#define  VIGNCR   0000200   /* ignore CR */
#define  VICRNL   0000400   /* map CR to NL (ala CRMOD) */
#define  VIUCLC   0001000   /* translate upper to lower case */
#define  VIXON    0002000   /* enable output flow control */
#define  VIXANY   0004000   /* any char will restart after stop */
#define  VIXOFF   0010000   /* enable input flow control */

/*
 * Output flags - software output processing
 */
#define  VOPOST   0000001   /* enable following output processing */
#define  VOLCUC   0000002   /* translate lower case to upper case */
#define  VONLCR   0000004   /* map NL to CR-NL (ala CRMOD) */
#define  VOCRNL   0000010   /* map CR to NL */
#define  VONOCR   0000020   /* No CR output at column 0 */
#define  VONLRET  0000040   /* NL performs the CR function */
#define  VOFILL   0000100
#define  VOFDEL   0000200
#define  VOXTABS  0014000   /* expand tabs to spaces */

/*
 * Control flags - hardware control of terminal
 */

#define  VCBAUD   0000017   /* baud rate */
#define  VB0      0000000   /* hang up */
#define  VB50     0000001
#define  VB75     0000002
#define  VB110    0000003
#define  VB134    0000004
#define  VB150    0000005
#define  VB200    0000006
#define  VB300    0000007
#define  VB600    0000010
#define  VB1200   0000011
#define  VB1800   0000012
#define  VB2400   0000013
#define  VB4800   0000014
#define  VB9600   0000015
#define  VB19200  0000016
#define  VB38400  0000017
#define  VEXTA    0000016
#define  VEXTB    0000017
#define  VCSIZE   0000060   /* character size mask */
#define  VCS5     0000000   /* 5 bits (pseudo) */
#define  VCS6     0000020   /* 6 bits */
#define  VCS7     0000040   /* 7 bits */
#define  VCS8     0000060   /* 8 bits */
#define  VCSTOPB  0000100   /* send 2 stop bits */
#define  VCREAD   0000200   /* enable receiver */
#define  VPARENB  0000400   /* parity enable */
#define  VPARODD  0001000   /* odd parity, else even */
#define  VHUPCL   0002000   /* hang up on last close */
#define  VCLOCAL  0004000   /* ignore modem status lines */

/*
 * "Local" flags - dumping ground for other state
 *
 * Warning: some flags in this structure begin with
 * the letter "I" and look like they belong in the
 * input flag.
 */

#define  VISIG    0000001   /* enable signals INTR, QUIT, [D]SUSP */
#define  VICANON  0000002   /* canonicalize input lines */
#define  VXCASE   0000004   /* canonical upper/lower case */
#define  VECHO    0000010   /* enable echoing */
#define  VECHOE   0000020   /* visually erase chars */
#define  VECHOK   0000040   /* echo NL after line kill */
#define  VECHONL  0000100   /* echo NL even if ECHO is off */
#define  VNOFLSH  0000200   /* don't flush after interrupt */


#define  VNCC     9  /* 18 bytes */
struct termio {
	volatile unsigned short c_iflag;
	volatile unsigned short c_oflag;
	volatile unsigned short c_cflag;
	volatile unsigned short c_lflag;
	volatile char           c_line;
	volatile unsigned char  c_cc[VNCC];
};

struct vx_sgttyb {      /* 6 bytes */
	volatile char  sg_ispeed;
	volatile char  sg_ospeed;
	volatile char  sg_erase;
	volatile char  sg_kill;
	volatile short  sg_flags;
};

struct termcb {      /* 6 bytes */
	volatile char  st_flgs;
	volatile char  st_termt;
	volatile char  st_crow;
	volatile char  st_ccol;
	volatile char  st_vrow;
	volatile char  st_lrow;
};

struct ctdesc {
	unsigned short csw,
	resv;
	unsigned long  magic,
	lcnt,
	fatal,
	error,
	faddr,
	expdata,
	readdata;
};

struct dl_info {     /* 18 bytes */
	volatile unsigned long  host_addr;
	volatile unsigned long  ipc_addr;
	volatile unsigned long  count;
	volatile unsigned long  extra_long;
	volatile unsigned short extra_short;
};

struct packet {      /* 68 bytes */
	volatile u_long  link;       /* was eyecatcher */
	volatile u_char  command_pipe_number;
	volatile u_char  status_pipe_number;
	volatile char    filler0[4];
	volatile short   command;
	volatile char    filler1[1];
	volatile char    command_dependent;
	volatile char    filler2[1];
	volatile char	 interrupt_level;	/* init only */
	volatile u_char  device_number;
	volatile char    filler3[1];
	volatile u_short ioctl_cmd_h;
	volatile u_short ioctl_cmd_l;
#define	init_info_ptr_h	ioctl_cmd_h
#define	init_info_ptr_l	ioctl_cmd_l
	volatile u_short ioctl_arg_h;
	volatile u_short ioctl_arg_l;
	volatile u_short ioctl_mode_h;
	volatile u_short ioctl_mode_l;
#define	interrupt_vec	ioctl_mode_l
	volatile char    filler4[6];
	volatile u_short error_h;
	volatile u_short error_l;
	volatile short   event_code;
	volatile char    filler5[6];
	union {
		struct  termio  tio;
		struct  termcb  tcb;
		struct  vx_sgttyb  sgt;
		struct  dl_info dl;
		long    param;
	} pb;
	short   reserved;    /* for alignment */
} packet;

struct envelope {	      /* 12 bytes */
	volatile u_long          link;
	volatile u_long          packet_ptr;
	volatile char            valid_flag;
	volatile char            reserved1;
	volatile char            reserved[2];
};

struct channel {        /* 24 bytes */
	volatile u_short           command_pipe_head_ptr_h;
	volatile u_short           command_pipe_head_ptr_l;
	volatile u_short           command_pipe_tail_ptr_h;
	volatile u_short           command_pipe_tail_ptr_l;
	volatile u_short           status_pipe_head_ptr_h;
	volatile u_short           status_pipe_head_ptr_l;
	volatile u_short           status_pipe_tail_ptr_h;
	volatile u_short           status_pipe_tail_ptr_l;
	volatile char              interrupt_level;
	volatile char              interrupt_vec;
	volatile char              channel_priority;
	volatile char              address_modifier;
	volatile char              channel_number;
	volatile char              valid;
	volatile char              datasize;
	volatile char              reserved;
};

#define WRING_DATA_SIZE 4096   /* for a total struct size of 4104 (4K + 6 + 2 bytes) */
#define WRING_BUF_SIZE WRING_DATA_SIZE
struct wring {
	volatile unsigned short reserved;
	volatile unsigned short put;
	volatile unsigned short get;
	volatile char           data[WRING_BUF_SIZE];
   char                    res[2]; /* for alignment */
};

#define RRING_DATA_SIZE 2048   /* for a total struct size of 2054 (2K + 6 + 2 bytes) */
#define RRING_BUF_SIZE RRING_DATA_SIZE
struct rring {
	volatile unsigned short reserved;
	volatile unsigned short put;
	volatile unsigned short get;
	volatile char           data[RRING_BUF_SIZE];
   char                    res[2]; /* for alignment */
};

#define EOFRAME  0xA
#define DELIMITER  0x1

struct init_info {      /* 88 bytes */
	volatile u_short        write_ring_ptr_h;
	volatile u_short        write_ring_ptr_l;
	volatile u_short        read_ring_ptr_h;
	volatile u_short        read_ring_ptr_l;
	volatile unsigned short write_ring_size;
	volatile unsigned short read_ring_size;
	volatile struct termio  def_termio;
	volatile unsigned short reserved1;
	volatile unsigned short reserved2;
	volatile unsigned short reserved3;
	volatile unsigned short reserved4;
	volatile char           init_data[56];
};

/* IPC event codes */
#define  E_INTR      0x0001
#define  E_QUIT      0x0002
#define  E_HUP       0x0004
#define  E_DCD       0x0008
#define  E_DSR       0x0010
#define  E_CTS       0x0020
#define  E_LOST_DCD  0x0040
#define  E_LOST_DSR  0x0080
#define  E_LOST_CTS  0x0100
#define  E_PR_FAULT  0x0200
#define  E_PR_POUT   0x0400
#define  E_PR_SELECT 0x0800
#define  E_SWITCH    0x4000
#define  E_BREAK     0x8000

/*
 * All structures must reside in dual port user memory.
 * ($FFxx0100 to $FFxxFFF0)
 * All structures must be word aligned (see byte counts above)
 *
 *       +--------------------------------+
 *       |  IPC Control/Status Register   | $FFxx0000
 *       |          (16 bytes)            |
 *       |--------------------------------|
 *       |  Confidence Test Descriptor    | $FFxx0010
 *       |          (32 bytes)            |
 *       |--------------------------------|
 *       |          Dump Area             | $FFxx0030
 *       |         (208 bytes)            |
 *       |--------------------------------|
 *       |                                | $FFxx0100
 *       |                                |
 *       :          User Space            :
 *       :                                :
 *       :        (65,264 bytes)          :
 *       |                                |
 *       |                                |
 *       |--------------------------------|
 *       |  Interrupt Vector Registers    | $FFxxFFF0
 *       |          (16 bytes)            |
 *       +--------------------------------+
 */

#define	NVXPORTS	9

#define  NENVELOPES           30
#define  NPACKETS             NENVELOPES
#define  USER_AREA            (sc->board_addr + 0x0100)
#define  CHANNEL_H            (sc->board_addr + 0x0100)
#define  ENVELOPE_AREA        (CHANNEL_H + sizeof(struct channel))
#define  ENVELOPE_AREA_SIZE   (NENVELOPES * sizeof(struct envelope))
#define  PACKET_AREA          (ENVELOPE_AREA + ENVELOPE_AREA_SIZE)
#define  PACKET_AREA_SIZE     (NPACKETS * sizeof(struct packet))
#define  INIT_INFO_AREA       (PACKET_AREA + PACKET_AREA_SIZE)
#define  INIT_INFO_AREA_SIZE  (NVXPORTS * sizeof(struct init_info))
#define  WRING_AREA           roundup(INIT_INFO_AREA + INIT_INFO_AREA_SIZE, 8)
#define  WRING_AREA_SIZE      (NVXPORTS * sizeof(struct wring))
#define  RRING_AREA           (WRING_AREA + WRING_AREA_SIZE)
#define  RRING_AREA_SIZE      (NVXPORTS * sizeof(struct rring))
#define  USER_AREA_SIZE       (RRING_AREA + RRING_AREA_SIZE - USER_AREA)

#define  LO(x) (u_short)((unsigned long)x & 0x0000FFFF)
#define  HI(x) (u_short)((unsigned long)x >> 16)
