/* All the asi of the series5 machine.  */
#ifndef _SERIES5_ASI_H_
#define _SERIES5_ASI_H_

/* Defined by the uP.  */
#define ASI_UP		0x08	/* User program.  */
#define ASI_SP		0x09	/* Supervisor program.  */
#define ASI_UD		0x0a	/* User data.  */
#define ASI_SD		0x0b	/* Supervisor data.  */

/* Diagnostic.  */
#define ASI_CTEST	0x40	/* Cache test operation base ASI.  */

/* MMU spaces.  */
#define ASI_MMCR	0x80	/* Memory management control reg, 32-bit, RW */
#define ASI_FCR		0x81	/* Fault cause register, 32-bit, RW.  */
#define ASI_FVAR	0x82	/* Fault virtual address reg, 32-bit, RO.  */
#define ASI_PDBA	0x83	/* Page directory base register, 32-bit, RW  */
#define ASI_TIR		0x84	/* Test information register, 32-bit, RO.  */
#define ASI_FTIR	0x85	/* FTLB test info reg, 32-bit, RO.  */

/* TLB spaces.  */
#define ASI_GTLB_INV	0x85	/* Invalidate GTLB, WO.  */
#define ASI_FTLB_INV	0x86	/* Invalidate FTLB, WO.  */
#define ASI_FGTLB_INV	0x87	/* Invalidate FTLB and GTLB, WO.  */

#define ASI_CTAG	0x90	/* Cache tag operations base ASI, WO.  */
#define ASI_CTAG_INV	0x94	/* Cache tag flash invalidation, WO.  */

#define ASI_FGTLB_VALI	0xa8	/* Write FTLB & instruction GTLB entry, WO.  */
#define ASI_FGTLB_VALD	0xaa	/* Write FTLB & data GTLB entry, WO.  */
#define ASI_GTLB_RDI	0xb1	/* Read instruction GTLB entry, WO.  */
#define ASI_GTLB_RDD	0xb3	/* Read data GTLB entry, WO.  */

#define ASI_BID		0xc0	/* Board id register, 16-bit, RO.  */
#define ASI_LED		0xc1	/* Board led register, 16-bit, RO.  */

/* ASI spaces for the SIC registers.  */
#define ASI_DIR		0xc8	/* Device id register, 8-bit, RW.  */
#define ASI_IXR		0xc9	/* Interrupt transmit reg, 32-bit, RW.  */
#define ASI_ITXC	0xca	/* Interrupt transmit ctrl reg, 32-Bit, RW.  */
#define ASI_IPR		0xcb	/* Interrupt priority register, 32-bit, RW.  */
#define ASI_ACK_IPV	0xcc	/* Ack int. pending vector reg, 32-Bit, RW.  */
#define ASI_IRXC	0xcd	/* Interrupt receiver ctrl reg, 32-bit, RW.  */
#define ASI_NOACK_IPV	0xce	/* Noack int. pending vector, 32-bit, RO.  */

#define ASI_CRDAT	0xcf	/* CRDAT bus state, 32-bit, RO.  */

#define ASI_FPAR	0xd0	/* Fault physical address reg, 32-bit, RO  */
#define ASI_FES		0xd1	/* Fault ECC syndrome register, 16-bit, RO  */
#define ASI_FTOR	0xd3	/* Fault time out register, 32-bit, RO.  */
#define ASI_FTSR	0xd4	/* Fault time out space reg, 16-bit, RO.  */

#define ASI_TRIGGER	0xd8	/* See diag.h.  */

#define ASI_DGRAM	0xe0	/* Diagnostic RAM, 2kb, RW.  */

#define ASI_KDT		0xf4	/* Diagnostic transaction space.  */
#define ASI_CLR_CORRUPT	0xf8	/* Corrupt block RAM bit clear, WO.  */
#define ASI_SET_CORRUPT	0xf9	/* Corrupt block RAM bit set, WO.  */
#endif /* _SERIES5_ASI_H_ */
