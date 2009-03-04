/*	$OpenBSD: pcctworeg.h,v 1.9 2009/03/04 19:35:52 miod Exp $ */

/*
 * Memory map for PCC2 chip found in MVME1x7 boards.
 *
 * PCCchip2 control and status register can be accessed as bytes (8 bits),
 * two-bytes (16 bits), or four-bytes (32 bits).
 */

#define	PCC2_BASE		0xfff42000
#define PCC2_SIZE		0x0040

#define	PCCTWO_CHIPID		0x0000
#define	PCCTWO_CHIPREV		0x0001
#define	PCCTWO_GENCTL		0x0002
#define	PCCTWO_VECBASE		0x0003
#define	PCCTWO_T1CMP		0x0004
#define	PCCTWO_T1COUNT		0x0008
#define	PCCTWO_T2CMP		0x000c
#define	PCCTWO_T2COUNT		0x0010
#define	PCCTWO_PSCALECNT	0x0014
#define	PCCTWO_PSCALEADJ	0x0015
#define	PCCTWO_T2CTL		0x0016
#define	PCCTWO_T1CTL		0x0017
#define	PCCTWO_GPIO_ICR		0x0018
#define	PCCTWO_GPIO_PCR		0x0019
#define	PCCTWO_T2ICR		0x001a
#define	PCCTWO_T1ICR		0x001b
#define	PCCTWO_SCCERR		0x001c
#define	PCCTWO_SCCICR		0x001d
#define	PCCTWO_SCCTX		0x001e
#define	PCCTWO_SCCRX		0x001f
#define	PCCTWO_SCCMOIACK	0x0023
#define	PCCTWO_SCCTXIACK	0x0025
#define	PCCTWO_SCCRXIACK	0x0027
#define	PCCTWO_IEERR		0x0028
#define	PCCTWO_IEICR		0x002a
#define	PCCTWO_IEBERR		0x002b
#define	PCCTWO_SCSIERR		0x002c
#define	PCCTWO_SCSIICR		0x002f
#define	PCCTWO_PRTICR		0x0030
#define	PCCTWO_PTRFICR		0x0031
#define	PCCTWO_PTRSICR		0x0032
#define	PCCTWO_PTRPICR		0x0033
#define	PCCTWO_PRTBICR		0x0034
#define	PCCTWO_PRTSTATUS	0x0036
#define	PCCTWO_PRTCTL		0x0037
#define	PCCTWO_SPEED		0x0038
#define	PCCTWO_PRTDATA		0x003a
/* The following registers are not valid on MVME197 */
#define	PCCTWO_IPL		0x003e
#define	PCCTWO_MASK		0x003f

#define PCC2_ID			0x20	/* value at CHIPID */

/* General Control Register */
#define PCC2_DR0		0x80
#define PCC2_C040		0x04
#define PCC2_MIEN		0x02
#define PCC2_FAST		0x01

/* Top 4 bits of the PCC2 VBR. Will be the top 4 bits of the vector */
#define	PCC2_VECT		0x50

/* Bottom 4 bits of the vector returned during IACK cycle */
#define PCC2V_PPBUSY		0x00 				/* lowest */
#define PCC2V_PPPE		0x01
#define PCC2V_PPSELECT		0x02
#define PCC2V_PPFAULT		0x03
#define PCC2V_PPACK		0x04
#define PCC2V_SCSI		0x05
#define PCC2V_IEFAIL		0x06
#define PCC2V_IE		0x07
#define PCC2V_TIMER2		0x08
#define PCC2V_TIMER1		0x09
#define PCC2V_GPIO		0x0a
#define PCC2V_SCC_RXE		0x0c
#define PCC2V_SCC_M		(PCC2V_SCC_RXE + 1)
#define PCC2V_SCC_TX		(PCC2V_SCC_M + 1)
#define PCC2V_SCC_RX		(PCC2V_SCC_TX + 1)

/*
 * Vaddrs for interrupt mask and pri registers
 */
extern u_int8_t *volatile pcc2intr_mask;
extern u_int8_t *volatile pcc2intr_ipl;

/*
 * We lock off our interrupt vector at 0x50.
 */
#define PCC2_VECBASE		0x50
#define PCC2_NVEC		0x10

#define PCC2_TCTL_CEN		0x01
#define PCC2_TCTL_COC		0x02
#define PCC2_TCTL_COVF		0x04
#define PCC2_TCTL_OVF		0xf0
#define PCC2_TCTL_OVF_SHIFT	4

#define PCC2_GPIO_PLTY		0x80
#define PCC2_GPIO_EL		0x40

#define PCC2_GPIOCR_OE		0x2
#define PCC2_GPIOCR_O		0x1

#define PCC2_SCC_AVEC		0x08

#define PCC2_SC_INHIBIT		(0 << 6)
#define PCC2_SC_SNOOP		(1 << 6)
#define PCC2_SC_INVAL		(2 << 6)
#define PCC2_SC_RESV		(3 << 6)

#define pcc2_timer_us2lim(us)	(us)		/* timer increments in "us" */

#define PCC2_IRQ_IPL		0x07
#define PCC2_IRQ_ICLR		0x08
#define PCC2_IRQ_IEN		0x10
#define PCC2_IRQ_INT		0x20

/* Tick Timer Interrupt Control Register */
#define PCC2_TTIRQ_INT		0x20
#define PCC2_TTIRQ_IEN		0x10
#define PCC2_TTIRQ_ICLR		0x08
#define PCC2_TTIRQ_IL		0x07		/* mask for IL2-IL0 */

#define PCC2_IEERR_SCLR		0x01

#define PCC2_GENCTL_FAST	0x01
#define PCC2_GENCTL_IEN		0x02
#define PCC2_GENCTL_C040	0x03
