/*	$OpenBSD: pcctworeg.h,v 1.2 1998/12/15 05:52:30 smurph Exp $ */

/*
 * Memory map for PCC2 chip found in mvme1x7 boards.
 *
 * PCCchip2 control and status register can be accessed as bytes (8 bits),
 * two-bytes (16 bits), or four-bytes (32 bits).
 */

struct pcctworeg {
	volatile u_char     pcc2_chipid;
	volatile u_char     pcc2_chiprev;
	volatile u_char     pcc2_genctl;
	volatile u_char     pcc2_vecbase;	/* irq vector base */
	volatile u_long     pcc2_t1cmp;		/* timer1 compare */
	volatile u_long     pcc2_t1count;	/* timer1 count */
	volatile u_long     pcc2_t2cmp;		/* timer2 compare */
	volatile u_long     pcc2_t2count;	/* timer2 count */
	volatile u_char     pcc2_pscalecnt;	/* timer prescaler counter */
	volatile u_char     pcc2_pscaleadj;	/* timer prescaler adjust */
	volatile u_char     pcc2_t2ctl;		/* timer2 ctrl reg */
	volatile u_char     pcc2_t1ctl;		/* timer1 ctrl reg */
	volatile u_char     pcc2_gpioirq;	/* gpio irq */
	volatile u_char     pcc2_gpio;		/* gpio i/o */
	volatile u_char     pcc2_t2irq;
	volatile u_char     pcc2_t1irq;
	volatile u_char     pcc2_sccerr;
	volatile u_char     pcc2_sccirq;
	volatile u_char     pcc2_scctx;
	volatile u_char     pcc2_sccrx;
	volatile u_char     :8;
	volatile u_char     :8;
	volatile u_char     :8;
	volatile u_char     pcc2_sccmoiack;
	volatile u_char     :8;
	volatile u_char     pcc2_scctxiack;
	volatile u_char     :8;
	volatile u_char     pcc2_sccrxiack;
	volatile u_char     pcc2_ieerr;
	volatile u_char     :8;
	volatile u_char     pcc2_ieirq;
	volatile u_char     pcc2_iefailirq;
	volatile u_char     pcc2_ncrerr;
	volatile u_char     :8;
	volatile u_char     :8;
	volatile u_char     pcc2_ncrirq;
	volatile u_char     pcc2_prtairq;
	volatile u_char     pcc2_prtfirq;
	volatile u_char     pcc2_prtsirq;
	volatile u_char     pcc2_prtpirq;
	volatile u_char     pcc2_prtbirq;
	volatile u_char     :8;
	volatile u_char     pcc2_prtstat;
	volatile u_char     pcc2_prtctl;
	volatile u_short    pcc2_speed;		/* DO NOT USE */
	volatile u_short    pcc2_prtdat;
	volatile u_short    :16;
	volatile u_char     pcc2_ipl;
	volatile u_char     pcc2_mask;
};
#define PCC2_PCC2CHIP_OFF	0x42000
#define PCC2_CHIPID		0x20
#define PCC2_BASE_ADDR		0xFFF42000		/* base address */
#define PCC2_SIZE		0x1000			/* size */
#define PCC2_CHIP_REV		0x00

/* General  Control Register */

#define PCC2_DR0		0x80
#define PCC2_C040		0x04
#define PCC2_MIEN		0x02
#define PCC2_FAST		0x01

/* Top 4 bits of the PCC2 VBR. Will be the top 4 bits of the vector */

#define	PCC2_VECT		0x50

/* Bottom 4 bits of the vector returned during IACK cycle */
#define PPBSY			0x00 				/* lowest */
#define PPSE			0x01
#define PPSEL			0x02
#define PPFLT			0x03
#define PPACK			0x04
#define SCSIIRQ			0x05
#define LANCERR			0x06
#define LANCIRQ			0x07
#define TIMER1IRQ		0x08
#define TIMER2IRQ		0x09
#define GPIOIRQ			0x0a
#define SRXEIRQ			0x0c
#define SMOIRQ			0x0d
#define STxIRQ			0x0e
#define SRxIRQ			0x0f


/*
 * Vaddrs for interrupt mask and pri registers
 */
extern volatile u_char *pcc2intr_mask;
extern volatile u_char *pcc2intr_ipl;

/*
 * points to system's PCCTWO. This is not active until the pcctwo0
 * device has been attached.
 */
extern struct pcctworeg *sys_pcc2;

/*
 * We lock off our interrupt vector at 0x50.
 */
#define PCC2_VECBASE		0x50
#define PCC2_NVEC		16

/*
 * Vectors we use
 */
#define PCC2V_NCR		0x05
#define PCC2V_IEFAIL		0x06
#define PCC2V_IE		0x07
#define PCC2V_TIMER2		0x08
#define PCC2V_TIMER1		0x09
#define PCC2V_GPIO		0x0a
#define PCC2V_SCC_RXE		0x0c
#define PCC2V_SCC_M		0x0d
#define PCC2V_SCC_TX		0x0e
#define PCC2V_SCC_RX		0x0f

#define PCC2_TCTL_CEN		0x01
#define PCC2_TCTL_COC		0x02
#define PCC2_TCTL_COVF		0x04
#define PCC2_TCTL_OVF		0xf0

#define	PCC2_TICTL_CEN			0x01
#define PCC2_TICTL_COC			0x02
#define PCC2_TICTL_COVF			0x04
#define PCC2_TTCTL_OVF_MASK		(1 << 4)	/* overflow bits mask */

#define PCC2_GPIO_PLTY		0x80
#define PCC2_GPIO_EL		0x40

#define PCC2_GPIOCR_OE		0x2
#define PCC2_GPIOCR_O		0x1

#define PCC2_SCC_AVEC		0x08
#define PCC2_SCCRX_INHIBIT	(0 << 6)
#define PCC2_SCCRX_SNOOP	(1 << 6)
#define PCC2_SCCRX_INVAL	(2 << 6)
#define PCC2_SCCRX_RESV		(3 << 6)

#define pcc2_timer_us2lim(us)	(us)		/* timer increments in "us" */

#define PCC2_IRQ_IPL		0x07
#define PCC2_IRQ_ICLR		0x08
#define PCC2_IRQ_IEN		0x10
#define PCC2_IRQ_INT		0x20

/* Tick Timer Interrupt Control Register */

#define PCC2_TTIRQ_INT			0x20
#define PCC2_TTIRQ_IEN			0x10
#define PCC2_TTIRQ_ICLR			0x08
#define PCC2_TTIRQ_IL			0x07		/* mask for IL2-IL0 */

#define PCC2_IEERR_SCLR		0x01

#define PCC2_GENCTL_FAST	0x01
#define PCC2_GENCTL_IEN		0x02
#define PCC2_GENCTL_C040	0x03

#define PCC2_SC_INHIBIT		(0 << 6)
#define PCC2_SC_SNOOP		(1 << 6)
#define PCC2_SC_INVAL		(2 << 6)
#define PCC2_SC_RESV		(3 << 6)
