/*
 * Memory map for PCC2 chip found in mvme1x7 boards.
 *
 * PCCchip2 control and status register can be accessed as bytes (8 bits),
 * two-bytes (16 bits), or four-bytes (32 bits).
 */

struct pcc2reg {
	volatile u_char		pcc2_chipid;
	volatile u_char		pcc2_chiprev;
	volatile u_char		pcc2_gcr;
	volatile u_char		pcc2_vbr;		/* vector base reg. */
	volatile u_long		pcc2_t1cmp;		/* timer1 compare reg */
	volatile u_long		pcc2_t1cntr;		/* timer1 counter reg */
	volatile u_long		pcc2_t2cmp;		/* timer2 compare reg */
	volatile u_long		pcc2_t2cntr;		/* timer2 counter reg */
	volatile u_char		pcc2_pscntreg;		/* prescalar count reg */
	volatile u_char		pcc2_psclkadj;		/* clock adjust reg */
	volatile u_char		pcc2_t2ctl;		/* timer2 control */
	volatile u_char		pcc2_t1ctl;		/* timer1 control */
	volatile u_char		pcc2_gpiirq;		/* GPIO intr ctl */
	volatile u_char		pcc2_gpiopctl;		/* GPIO pin control */ 
	volatile u_char		pcc2_t2irq;		/* Timer2 intr ctl */
	volatile u_char		pcc2_t1irq;		/* Timer1 intr ctl */
	volatile u_char		pcc2_sccerrstat;	/* SCC error status */
	volatile u_char		pcc2_sccmoirq;		/* Modem intr control */
	volatile u_char		pcc2_scctxirq;		/* Tx intr control */
	volatile u_char		pcc2_sccrxirq;		/* Rx intr control */
	volatile u_int		:24;
	volatile u_char		pcc2_sccmopiack;	/* modem PIACK */
	volatile u_char		:8;
	volatile u_char		pcc2_scctxpiack;	/* Tx PIACK */
	volatile u_char		:8;
	volatile u_char		pcc2_sccrxpiack;	/* Rx PIACK */
	volatile u_char		pcc2_lancerrstat;	/* LANC error status */
	volatile u_char		:8;
	volatile u_char		pcc2_lancirq;		/* LANC intr control */
	volatile u_char		pcc2_lancerrirq;	/* LANC err intr ctl */
	volatile u_char		pcc2_scsierrstat;	/* SCSI err status */
	volatile u_char		:8;
	volatile u_char		:8;
	volatile u_char		pcc2_scsiirq;		/* SCSI intr control */
	volatile u_char		pcc2_packirq;		/* printer ACK intr */
	volatile u_char		pcc2_pfltirq;		/* printer FAULT intr */
	volatile u_char		pcc2_pselirq;		/* printer SEL intr */
	volatile u_char		pcc2_ppeirq;		/* printer PE intr */
	volatile u_char		pcc2_pbusyirq;		/* printer BUSY intr */
	volatile u_char		:8;
	volatile u_char		pcc2_pstat;		/* printer status reg */
	volatile u_char		pcc2_pctl;		/* printer port ctl */
	volatile u_short	pcc2_chipspeed;		/* chip speed (factory testing only) */
	volatile u_short	pcc2_pdata;		/* printer data */
	volatile u_int		:16;
	volatile u_char		pcc2_ipl;		/* interrupt IPL */
	volatile u_char		pcc2_imask;		/* intr mask level */
};

/*
 * Vaddrs for interrupt mask and pri registers
 */
extern volatile u_char *pcc2intr_mask;
extern volatile u_char *pcc2intr_ipl;

extern volatile struct pcc2reg *pcc2addr;

#define PCC2_BASE_ADDR		0xFFF42000		/* base address */
#define PCC2_SIZE		0x1000			/* size */

#define PCC2_CHIP_ID		0x20
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
 * Timer control regs
 */

#define	PCC2_TICTL_CEN			0x01
#define PCC2_TICTL_COC			0x02
#define PCC2_TICTL_COVF			0x04
#define PCC2_TTCTL_OVF_MASK		(1 << 4)	/* overflow bits mask */

/* GPIO interrupt control */

#define PCC2_GPIIRQ_PLTY		0x80
#define PCC2_GPIIRQ_EL			0x40
#define PCC2_GPIIRQ_INT			0x20
#define PCC2_GPIIRQ_IEN			0x10
#define PCC2_GPIIRQ_ICLR		0x08
#define PCC2_GPIIRQ_IL			0x07		/* IL2-IL0 */

/* GPIO Pin Control Register */

#define PCC2_GPIOPCTL_GPI		0x04
#define PCC2_GPIOPCTL_GPOE		0x02
#define PCC2_GPIOPCTL_GPO		0x01

/* Tick Timer Interrupt Control Register */

#define PCC2_TTIRQ_INT			0x20
#define PCC2_TTIRQ_IEN			0x10
#define PCC2_TTIRQ_ICLR			0x08
#define PCC2_TTIRQ_IL			0x07		/* mask for IL2-IL0 */

/* SCC Error Status Register */

#define PCC2_SCCERRSTAT_RTRY		0x10
#define PCC2_SCCERRSTAT_PRTY		0x08
#define PCC2_SCCERRSTAT_EXT		0x04
#define PCC2_SCCERRSTAT_LTO		0x02
#define PCC2_SCCERRSTAT_SCLR		0x01

/* SCC Modem Interrupt Control Register */

#define PCC2_SCCMOIRQ_IRQ		0x20
#define PCC2_SCCMOIRQ_IEN		0x10
#define PCC2_SCCMOIRQ_AVEC		0x08
#define PCC2_SCCMOIRQ_IL		0x07		/* int level mask */

/* SCC Tx Interrupt Control Register */

#define PCC2_SCCTXIRQ_IRQ		0x20
#define PCC2_SCCTXIRQ_IEN		0x10
#define PCC2_SCCTXIRQ_AVEC		0x08
#define PCC2_SCCTXIRQ_IL		0x07

/* SCC Tx Interrupt Control Register */

#define PCC2_SCCRXIRQ_SNOOP		(1 << 6)
#define PCC2_SCCRXIRQ_IRQ		0x20
#define PCC2_SCCRXIRQ_IEN		0x10
#define PCC2_SCCRXIRQ_AVEC		0x08
#define PCC2_SCCRXIRQ_IL		0x07

/* SCSI Interrupt Control Register */

#define PCC2_SCSIIRQ_IEN		0x10

/* Interrupt Priority Level Register */

#define PCC2_IPL_IPL			0x07

/* Interrupt Mask Level Register */

#define PCC2_IMASK_MSK			0x07

#define PCC2_IRQ_IPL		0x07
#define PCC2_IRQ_ICLR		0x08
#define PCC2_IRQ_IEN		0x10
#define PCC2_IRQ_INT		0x20

#define PCC2_IEERR_SCLR		0x01
