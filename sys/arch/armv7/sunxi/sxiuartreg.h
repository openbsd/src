#define	SXIUART_RBR	0x00
#define	SXIUART_THR	0x00
#define SXIUART_DLL	0x00
#define SXIUART_DLH	0x04
#define SXIUART_IER	0x04
#define SXIUART_IIR	0x08
#define SXIUART_FCR	0x08
#define SXIUART_LCR	0x0c
#define SXIUART_MCR	0x10
#define SXIUART_LSR	0x14
#define SXIUART_MSR	0x18
#define SXIUART_SCH	0x1c
#define SXIUART_USR	0x7c
#define SXIUART_TFL	0x80
#define SXIUART_RFL	0x84
#define SXIUART_HALT	0xa4

/* interrupt enable register */
#define IER_PTIME	0x80
#define	IER_EDSSI	0x08
#define IER_EMSC	IER_EDSSI
#define	IER_ELSI	0x04
#define IER_ERLS	IER_ELSI
#define	IER_ETBEI	0x02
#define IER_ETXRDY	IER_ETBEI
#define	IER_ERBFI	0x01
#define IER_ERXRDY	IER_ERBFI

/* interrupt identification register */
#define	IIR_IMASK	0x0f
#define	IIR_RXTOUT	0x0c
#define	IIR_BUSY	0x07
#define	IIR_RLS		0x06
#define	IIR_RXRDY	0x04
#define	IIR_TXRDY	0x02
#define	IIR_MLSC	0x00
#define	IIR_NOPEND	0x01
#define	IIR_FIFO_MASK	0xc0

/* fifo control register */
#define	FIFO_RXT3	0xc0	/* FIFOSIZE-2 chars in rxfifo */
#define	FIFO_RXT2	0x80	/* rxfifo 1/2 full */
#define	FIFO_RXT1	0x40	/* rxfifo 1/4 full */
#define	FIFO_RXT0	0x00	/* 1char in rxfifo */
#define FIFO_TXT3	0x30	/* txfifo 1/2 full */
#define FIFO_TXT2	0x20	/* txfifo 1/4 full */
#define FIFO_TXT1	0x10	/* 2chars in txfifo */
#define FIFO_TXT0	0x00	/* txfifo empty */
#define	DMAM		0x08
#define	XFIFOR		0x04	/* txfifo reset */
#define	RFIFOR		0x02	/* rxfifo reset */
#define	FIFOE		0x01	/* enable, resets fifos when changed */

/* line control register */
#define	LCR_DLAB	0x80
#define	LCR_SBREAK	0x40
#define	LCR_PZERO	0x38
#define	LCR_PONE	0x28
#define	LCR_PEVEN	0x18
#define	LCR_PODD	0x08
#define	LCR_PNONE	0x00
#define	LCR_PENAB	0x08
#define	LCR_STOPB	0x04
#define	LCR_8BITS	0x03
#define	LCR_7BITS	0x02
#define	LCR_6BITS	0x01
#define	LCR_5BITS	0x00

/* modem control register */
#define MCR_SIRE	0x40
#define	MCR_AFCE	0x20
#define	MCR_LOOPBACK	0x10
#define	MCR_IENABLE	0x08 /* XXX */
#define	MCR_DRS		0x04 /* XXX */
#define	MCR_RTS		0x02
#define	MCR_DTR		0x01

/* line status register */
#define	LSR_RCV_FIFO	0x80
#define	LSR_TSRE	0x40
#define	LSR_TEMT	LSR_TSRE
#define	LSR_TXRDY	0x20
#define	LSR_THRE	LSR_TXRDY
#define	LSR_BI		0x10
#define	LSR_FE		0x08
#define	LSR_PE		0x04
#define	LSR_OE		0x02
#define	LSR_RXRDY	0x01
#define	LSR_RCV_MASK	0x1f

/* modem status register */
/* All deltas are from the last read of the MSR. */
#define	MSR_DCD		0x80
#define	MSR_RI		0x40
#define	MSR_DSR		0x20
#define	MSR_CTS		0x10
#define	MSR_DDCD	0x08
#define	MSR_TERI	0x04
#define	MSR_DDSR	0x02
#define	MSR_DCTS	0x01

/* uart status register */
#define	USR_RFF		0x10
#define	USR_RFNE	0x08
#define	USR_TFE		0x04
#define	USR_TFNF	0x02
#define	USR_BUSY	0x01
