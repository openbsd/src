/* magmareg.h
 *
 *  Copyright (c) 1998 Iain Hibbert
 *  All rights reserved.
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
 *	This product includes software developed by Iain Hibbert
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
 *
 */

#ifdef MAGMA_DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

/*  The mapping of minor device number -> card and port is done as
 * follows by default:
 *
 *  +---+---+---+---+---+---+---+---+
 *  | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
 *  +---+---+---+---+---+---+---+---+
 *    |   |   |   |   |   |   |   |
 *    |   |   |   |   +---+---+---+---> port number
 *    |   |   |   |
 *    |   |   |   +-------------------> dialout (on tty ports)
 *    |   |   |
 *    |   |   +-----------------------> unused
 *    |   |
 *    +---+---------------------------> card number
 *
 */

#define MAGMA_MAX_CARDS		4
#define MAGMA_MAX_TTY		16
#define MAGMA_MAX_BPP		2
#define MAGMA_MAX_CD1400	4
#define MAGMA_MAX_CD1190	2

#define MAGMA_CARD(x)	((minor(x) >> 6) & 0x03)
#define MAGMA_PORT(x)	(minor(x) & 0x0f)

#define MTTY_DIALOUT(x) (minor(x) & 0x10)

/*
 * Supported Card Types
 */
struct magma_board_info {
	char *mb_name;			/* cardname to match against */
	char *mb_realname;		/* english card name */
	int mb_nser;			/* number of serial ports */
	int mb_npar;			/* number of parallel ports */
	int mb_ncd1400;			/* number of CD1400 chips */
	int mb_svcackr;			/* svcackr offset */
	int mb_svcackt;			/* svcackt offset */
	int mb_svcackm;			/* svcackm offset */
	int mb_cd1400[MAGMA_MAX_CD1400];/* cd1400 chip register offsets */
	int mb_ncd1190;			/* number of CD1190 chips */
	int mb_cd1190[MAGMA_MAX_CD1190];/* cd1190 chip register offsets */
};

/*
 * cd1400 chip data
 */
struct cd1400 {
	__volatile u_char *cd_reg;	/* chip registers */
	int cd_chiprev;			/* chip revision */
	int cd_clock;			/* clock speed in Mhz */
	int cd_parmode;			/* parallel mode operation */
};

/*
 * cd1190 chip data
 */
struct cd1190 {
	__volatile u_char *cd_reg;	/* chip registers */
	int cd_chiprev;			/* chip revision */
};

/* software state for each card */
struct magma_softc {
	struct device ms_dev;		/* required. must be first in softc */

	/* cd1400 chip info */
	int ms_ncd1400;
	struct cd1400 ms_cd1400[MAGMA_MAX_CD1400];
	__volatile u_char *ms_svcackr;	/* CD1400 service acknowledge receive */
	__volatile u_char *ms_svcackt;	/* CD1400 service acknowledge transmit */
	__volatile u_char *ms_svcackm;	/* CD1400 service acknowledge modem */

	/* cd1190 chip info */
	int ms_ncd1190;
	struct cd1190 ms_cd1190[MAGMA_MAX_CD1190];

	struct magma_board_info *ms_board;	/* what am I? */

	struct mtty_softc *ms_mtty;
	struct mbpp_softc *ms_mbpp;

	struct intrhand ms_hardint;	/* hard interrupt handler */
	struct intrhand ms_softint;	/* soft interrupt handler */
};

#define MTTY_RBUF_SIZE		(2 * 512)
#define MTTY_RX_FIFO_THRESHOLD	6
#define MTTY_RX_DTR_THRESHOLD	9

struct mtty_port {
	struct cd1400 *mp_cd1400;	/* ptr to chip */
	int mp_channel;			/* and channel */
	struct tty *mp_tty;

	int mp_openflags;	/* default tty flags */
	int mp_flags;		/* port flags */
	int mp_carrier;		/* state of carrier */

	u_char *mp_rbuf;	/* ring buffer start */
	u_char *mp_rend;	/* ring buffer end */
	u_char *mp_rget;	/* ring buffer read pointer */
	u_char *mp_rput;	/* ring buffer write pointer */

	u_char *mp_txp;		/* transmit character pointer */
	int mp_txc;		/* transmit character counter */
};

#define MTTYF_CARRIER_CHANGED	(1<<0)
#define MTTYF_SET_BREAK		(1<<1)
#define MTTYF_CLR_BREAK		(1<<2)
#define MTTYF_DONE		(1<<3)
#define MTTYF_STOP		(1<<4)
#define MTTYF_RING_OVERFLOW	(1<<5)

struct mtty_softc {
	struct device ms_dev;		/* device info */
	int ms_nports;			/* tty ports */
	struct mtty_port ms_port[MAGMA_MAX_TTY];
};

#define MBPP_RX_FIFO_THRESHOLD	25

struct mbpp_port {
	struct cd1400 *mp_cd1400;	/* for LC2+1Sp card */
	struct cd1190 *mp_cd1190;	/* all the others   */

	int mp_flags;

	struct bpp_param mp_param;
#define mp_burst mp_param.bp_burst
#define mp_timeout mp_param.bp_timeout
#define mp_delay mp_param.bp_delay

	u_char *mp_ptr;		/* pointer to io data */
	int mp_cnt;		/* count of io chars */
};

#define MBPPF_OPEN	(1<<0)
#define MBPPF_TIMEOUT	(1<<1)
#define MBPPF_UIO	(1<<2)
#define MBPPF_DELAY	(1<<3)
#define MBPPF_WAKEUP	(1<<4)

struct mbpp_softc {
	struct device ms_dev;		/* device info */
	int ms_nports;			/* parallel ports */
	struct mbpp_port ms_port[MAGMA_MAX_BPP];
};

/*
 * useful macros
 */
#define SET(t, f)	((t) |= (f))
#define CLR(t, f)	((t) &= ~(f))
#define ISSET(t, f)	((t) & (f))

/* internal function prototypes */

int cd1400_compute_baud __P((speed_t, int, int *, int *));
__inline void cd1400_write_ccr __P((struct cd1400 *, u_char));
__inline u_char cd1400_read_reg __P((struct cd1400 *, int));
__inline void cd1400_write_reg __P((struct cd1400 *, int, u_char));
void cd1400_enable_transmitter __P((struct cd1400 *, int));

int magma_match __P((struct device *, void *, void *));
void magma_attach __P((struct device *, struct device *, void *));
int magma_hard __P((void *));
int magma_soft __P((void *));

int mtty_match __P((struct device *, void *, void *));
void mtty_attach __P((struct device *, struct device *, void *));
int mtty_modem_control __P((struct mtty_port *, int, int));
int mtty_param __P((struct tty *, struct termios *));
void mtty_start __P((struct tty *));

int mbpp_match __P((struct device *, void *, void *));
void mbpp_attach __P((struct device *, struct device *, void *));
int mbpp_rw __P((dev_t, struct uio *));
void mbpp_timeout __P((void *));
void mbpp_start __P((void *));
int mbpp_send __P((struct mbpp_port *, caddr_t, int));
int mbpp_recv __P((struct mbpp_port *, caddr_t, int));
int mbpp_hztoms __P((int));
int mbpp_mstohz __P((int));
