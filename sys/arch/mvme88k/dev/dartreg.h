/*	$OpenBSD: dartreg.h,v 1.5 2004/04/15 12:34:42 miod Exp $	*/

#define MAXPORTS	2		/* max count of PORTS/DUART */

#define MVME188_DUART	0xfff82000	/* M68692 DUART chip */

#define  A_PORT   0  /* flag for port a */
#define  B_PORT   1  /* flag for port b */

/* the access to the same command register must be delayed,
   because the chip has some hardware problems in this case */
#define DELAY_CR   do { volatile int i; for (i = 0; i < 250; ++i); } while (0)

/*********************** MC68681 DEFINITIONS ************************/
#define PORTOFFSET	0x10	/* offset for port b address space */

/* mode register 1: MR1x operations */
#define RXRTS        0x80  /* enable receiver RTS */
#define PAREN        0x00  /* with parity */
#define PARDIS       0x10  /* no parity */
#define EVENPAR      0x00  /* even parity */
#define ODDPAR       0x04  /* odd parity */
#define CL5          0x00  /* 5 bits per char */
#define CL6          0x01  /* 6 bits per char */
#define CL7          0x02  /* 7 bits per char */
#define CL8          0x03  /* 8 bits per char */
#define PARMODEMASK  0x18  /* parity mode mask */
#define PARTYPEMASK  0x04  /* parity type mask */
#define CLMASK       0x03  /* character length mask */

/* mode register 2: MR2x operations */
#define TXRTS        0x20  /* enable transmitter RTS */
#define TXCTS        0x10  /* enable transmitter CTS */
#define SB2          0x0f  /* 2 stop bits */
#define SB1          0x07  /* 1 stop bit */
#define SB1L5        0x00  /* 1 stop bit at 5 bits per character */

#define SBMASK       0x0f  /* stop bit mask */

/* clock-select register: CSRx operations */
#define NOBAUD       -1    /* 50 and 200 baud are not possible */
/* they are not in Baud register set 2 */
#define BD75         0x00  /* 75 baud */
#define BD110        0x11  /* 110 baud */
#define BD134        0x22  /* 134.5 baud */
#define BD150        0x33  /* 150 baud */
#define BD300        0x44  /* 300 baud */
#define BD600        0x55  /* 600 baud */
#define BD1200       0x66  /* 1200 baud */
#define BD1800       0xaa  /* 1800 baud */
#define BD2400       0x88  /* 2400 baud */
#define BD4800       0x99  /* 4800 baud */
#define BD9600       0xbb  /* 9600 baud */
#define BD19200      0xcc  /* 19200 baud */

#define DEFBAUD      BD9600   /* default value if baudrate is not possible */


/* channel command register: CRx operations */
#define MRRESET      0x10  /* reset mr pointer to mr1 */
#define RXRESET      0x20  /* reset receiver */
#define TXRESET      0x30  /* reset transmitter */
#define ERRRESET     0x40  /* reset error status */
#define BRKINTRESET  0x50  /* reset channel's break interrupt */
#define BRKSTART     0x60  /* start break */
#define BRKSTOP      0x70  /* stop break */
#define TXDIS        0x08  /* disable transmitter */
#define TXEN         0x04  /* enable transmitter */
#define RXDIS        0x02  /* disable receiver */
#define RXEN         0x01  /* enable receiver */

/* status register: SRx status */
#define RBRK         0x80  /* received break */
#define FRERR        0x40  /* frame error */
#define PERR         0x20  /* parity error */
#define ROVRN        0x10  /* receiver overrun error */
#define TXEMT        0x08  /* transmitter empty */
#define TXRDY        0x04  /* transmitter ready */
#define FFULL        0x02  /* receiver FIFO full */
#define RXRDY        0x01  /* receiver ready */

/* output port configuration register: OPCR operations */
#define OPSET        0x00  /* set all op lines to op function */
#define OPSETTO      0x04  /* use OP3 for timer output */

/* output port register: OP operations */
#define OPDTRB       0x20  /* DTR line output b on the VME188, 181, 141 */
#define OPDTRA       0x04  /* DTR line output a */
#define OPRTSB       0x02  /* RTS line output b */
#define OPRTSA       0x01  /* RTS line output a */

/* auxiliary control register: ACR operations */
#define BDSET1       0x00  /* baudrate generator set 1 */
#define BDSET2       0x80  /* baudrate generator set 2 */
#define CCLK1        0x60  /* timer clock: external rate.  TA */
#define CCLK16       0x30  /* counter clock: x1 clk divided by 16 */
#define SLCTIM       0x7800/* timer count to get 60 Hz time slice (16.6ms ticks) */
#define IPDCDIB      0x08  /* IP3 change == DCD input on port B */
#define IPDCDIA      0x04  /* IP2 change == DCD input on port A */

/* input port change register: IPCR operations */
#define IPCRDCDB     0x80  /* IP3 change == DCD change on port B */
#define IPCRDCDA     0x40  /* IP2 change == DCD change on port A */

/* Defines for mvme335 */
#define IPDCDB       0x20  /* DCD line input b */
#define IPDCDA       0x10  /* DCD line input a */

#define IPDSRB       0x08  /* DSR line input b */
#define IPDSRA       0x04  /* DSR line input a */
#define IPCTSB       0x02  /* CTS line input b */
#define IPCTSA       0x01  /* CTS line input a */

/* interrupt status and mask register: ISR status and IMR mask */
#define IIPCHG       0x80  /* input port change */
#define IBRKB        0x40  /* delta break b */
#define IRXRDYB      0x20  /* receiver ready b */
#define ITXRDYB      0x10  /* transmitter ready b */
#define ITIMER       0x08  /* Enable timer interrupts. */
#define IBRKA        0x04  /* delta break a */
#define IRXRDYA      0x02  /* receiver ready a */
#define ITXRDYA      0x01  /* transmitter ready a */

/* interrupts from port a or b */
#define AINTPORT  ( IRXRDYA | ITXRDYA )
#define BINTPORT  ( IRXRDYB | ITXRDYB )

/* HW write register index for ut_wr_regs[] */
#define MR1A         0  /* mode register 1 a */
#define CSRA         1  /* clock-select register a*/
#define CRA          2  /* command register a */
#define TBA          3  /* transmitter buffer a */
#define ACR          4  /* auxiliary control register*/
#define IMR          5  /* interrupt mask register */
#define CTUR         6  /* counter/timer upper reg */
#define CTLR         7  /* counter/timer lower reg */
#define MR1B         8  /* mode register 1 b */
#define CSRB         9  /* clock-select register b*/
#define CRB          10 /* command register b */
#define TBB          11 /* transmitter buffer b */
#define IVR          12 /* interrupt vector register */
#define OPCR         13 /* output port config reg */
#define OPRSET       14 /* output port: bit set cmd */
#define OPRRESET     15 /* output port: bit reset cmd */
#define MR2A         16 /* mode register 2 a */
#define MR2B         17 /* mode register 2 b */
#define MAXREG       18 /* max count of registers */

/*
 *	Structure of MC68681 hardware read registers.
 */
struct dart_rd_reg
{
	volatile unsigned int	rd_mra;     /* mode register a */
	volatile unsigned int	rd_sra;     /* status register a*/
	volatile unsigned int	dummy5;     /* do not access */
	volatile unsigned int	rd_rba;     /* receiver buffer a */
	volatile unsigned int	rd_ipcr;    /* input port change register */
	volatile unsigned int	rd_isr;     /* interrupt status register */
	volatile unsigned int	rd_cur;     /* current MSB of counter */
	volatile unsigned int	rd_clr;	    /* current LSB of counter */
	volatile unsigned int	rd_mrb;     /* mode register b */
	volatile unsigned int	rd_srb;     /* status register b*/
	volatile unsigned int	dummyxb;    /* do not access */
	volatile unsigned int	rd_rbb;     /* receiver buffer b */
	volatile unsigned int	rd_ivr;     /* interrupt vector register */
	volatile unsigned int	rd_ip;      /* input port (unlatched) */
	volatile unsigned int	rd_ctstart; /* start-counter command */
	volatile unsigned int	rd_ctstop;  /* stop-counter command */
};

/*
 *	Structure of MC68681 hardware write registers.
 */
struct dart_wr_reg
{
	volatile unsigned int	wr_mra;     /* mode register a */
	volatile unsigned int	wr_csra;    /* clock-select register a*/
	volatile unsigned int	wr_cra;     /* command register a */
	volatile unsigned int	wr_tba;     /* transmitter buffer a */
	volatile unsigned int	wr_acr;     /* auxiliary control register*/
	volatile unsigned int	wr_imr;     /* interrupt mask register */
	volatile unsigned int	wr_ctur;    /* counter/timer upper reg */
	volatile unsigned int	wr_ctlr;    /* counter/timer lower reg */
	volatile unsigned int	wr_mrb;     /* mode register b */
	volatile unsigned int	wr_csrb;    /* clock-select register b*/
	volatile unsigned int	wr_crb;     /* command register b */
	volatile unsigned int	wr_tbb;     /* transmitter buffer b */
	volatile unsigned int	wr_ivr;     /* interrupt vector register */
	volatile unsigned int	wr_opcr;    /* output port config reg */
	volatile unsigned int	wr_oprset;  /* output port: bit set cmd */
	volatile unsigned int	wr_oprreset;/* output port: bit reset cmd */
};

union dartreg {
	struct dart_rd_reg	read;		/* read registers */
	struct dart_wr_reg	write;		/* write registers */
};

/*
 *	Structure of MC68681 hardware port dependent read registers.
 */
struct dart_prd_reg
{
	volatile unsigned int	rd_mr;		/* mode register */
	volatile unsigned int	rd_sr;		/* status register */
	volatile unsigned int	dummy5;
	volatile unsigned int	rd_rb;		/* receiver buffer */
	volatile unsigned int	dummy9;
	volatile unsigned int	dummy11;
	volatile unsigned int	dummy13;
	volatile unsigned int	dummy15;
};

/*
 *	Structure of MC68681 hardware port dependent write registers.
 */
struct dart_pwr_reg
{
	volatile unsigned int	wr_mr;		/* mode register */
	volatile unsigned int	wr_csr;		/* clock-select register */
	volatile unsigned int	wr_cr;		/* command register */
	volatile unsigned int	wr_tb;		/* transmitter buffer */
	volatile unsigned int	dummy9;
	volatile unsigned int	dummy11;
	volatile unsigned int	dummy13;
	volatile unsigned int	dummy15;
};

union dart_pt_io {
	struct dart_prd_reg	read;		/* read registers */
	struct dart_pwr_reg	write;		/* write registers */
};

/*
 *	Structure of saved registers
 */
struct dart_sv_reg
{
	volatile unsigned char	sv_mr1[MAXPORTS];	/* mode register 1 a */
	volatile unsigned char	sv_mr2[MAXPORTS];	/* mode register 2 a */
	volatile unsigned char	sv_csr[MAXPORTS];	/* clock-select register a*/
	volatile unsigned char	sv_cr[MAXPORTS];	/* command register a */
	volatile unsigned char	sv_acr;		/* auxiliary control register*/
	volatile unsigned char	sv_imr;		/* interrupt mask register */
	volatile unsigned char	sv_ivr;		/* interrupt vector register */
};
