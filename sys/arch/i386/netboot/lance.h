/*	$NetBSD: lance.h,v 1.3 1994/10/27 04:21:16 cgd Exp $	*/

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 */

/* RAP functions as a select for RDP */
#define RDP_CSR0	0
#define RDP_CSR1	1
#define RDP_CSR2	2
#define RDP_CSR3	3

/* contents of csr0 */
#define CSR_ERR		0x8000
#define CSR_BABL	0x4000
#define CSR_CERR	0x2000
#define CSR_MISS	0x1000
#define CSR_MERR	0x0800
#define CSR_RINT	0x0400
#define CSR_TINT	0x0200
#define CSR_IDON	0x0100
#define CSR_INTR	0x0080
#define CSR_INEA	0x0040
#define CSR_RXON	0x0020
#define CSR_TXON	0x0010
#define CSR_TDMD	0x0008
#define CSR_STOP	0x0004
#define CSR_STRT	0x0002
#define CSR_INIT	0x0001

/* csr1 contains low 16 bits of address of Initialization Block */

/* csr2 contains in low byte high 8 bits of address of InitBlock */

/* contents of csr3 */
#define CSR3_BSWP	0x04	/* byte swap (for big endian) */
#define CSR3_ACON	0x02	/* ALE control */
#define CSR3_BCON	0x01	/* byte control */

/*
 * The initialization block
 */
typedef struct {
    u_short	ib_mode;	/* modebits, see below */
    char	ib_padr[6];	/* physical 48bit Ether-address */
    u_short	ib_ladrf[4];	/* 64bit hashtable for "logical" addresses */
    u_short	ib_rdralow;	/* low 16 bits of Receiver Descr. Ring addr */
    u_char	ib_rdrahigh;	/* high 8 bits of Receiver Descr. Ring addr */
    u_char	ib_rlen;	/* upper 3 bits are 2log Rec. Ring Length */
    u_short	ib_tdralow;	/* low 16 bits of Transm. Descr. Ring addr */
    u_char	ib_tdrahigh;	/* high 8 bits of Transm. Descr. Ring addr */
    u_char	ib_tlen;	/* upper 3 bits are 2log Transm. Ring Length */
} initblock_t;

/* bits in mode */
#define IB_PROM		0x8000
#define IB_INTL		0x0040
#define IB_DRTY		0x0020
#define IB_COLL		0x0010
#define IB_DTCR		0x0008
#define IB_LOOP		0x0004
#define IB_DTX		0x0002
#define IB_DRX		0x0001

/*
 * A receive message descriptor entry
 */
typedef struct {
    u_short	rmd_ladr;	/* low 16 bits of bufaddr */
    char	rmd_hadr;	/* high 8 bits of bufaddr */
    char	rmd_flags; 	/* see below */
    short	rmd_bcnt;	/* two's complement of buffer byte count */
    u_short	rmd_mcnt;	/* message byte count */
} rmde_t;

/* bits in flags */
#define RMD_OWN		0x80
#define RMD_ERR		0x40
#define RMD_FRAM	0x20
#define RMD_OFLO	0x10
#define RMD_CRC		0x08
#define RMD_BUFF	0x04
#define RMD_STP		0x02
#define RMD_ENP		0x01

/*
 * A transmit message descriptor entry
 */
typedef struct {
    u_short	tmd_ladr;	/* low 16 bits of bufaddr */
    u_char	tmd_hadr;	/* high 8 bits of bufaddr */
    u_char	tmd_flags;	/* see below */
    short	tmd_bcnt;	/* two's complement of buffer byte count */
    u_short	tmd_err;	/* more error bits + TDR */
} tmde_t;

/* bits in flags */
#define TMD_OWN		0x80
#define TMD_ERR		0x40
#define TMD_MORE	0x10
#define TMD_ONE		0x08
#define TMD_DEF		0x04
#define TMD_STP		0x02
#define TMD_ENP		0x01

/* bits in tmd_err */
#define TMDE_BUFF	0x8000
#define TMDE_UFLO	0x4000
#define TMDE_LCOL	0x1000
#define TMDE_LCAR	0x0800
#define TMDE_RTRY	0x0400
#define TMDE_TDR	0x003F	/* mask for TDR */

