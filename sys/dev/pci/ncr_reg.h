/*	$OpenBSD: ncr_reg.h,v 1.6 1997/10/11 09:12:20 pefo Exp $	*/
/*	$NetBSD: ncr_reg.h,v 1.10 1997/01/10 05:57:14 perry Exp $	*/

/**************************************************************************
**
**  Device driver for the   NCR 53C810   PCI-SCSI-Controller.
**
**  FreeBSD / NetBSD / OpenBSD
**
**-------------------------------------------------------------------------
**
**  Written for 386bsd and FreeBSD by
**	wolf@cologne.de		Wolfgang Stanglmeier
**	se@mi.Uni-Koeln.de	Stefan Esser
**
**  Ported to NetBSD by
**	mycroft@gnu.ai.mit.edu
**
**-------------------------------------------------------------------------
**
** Copyright (c) 1994 Wolfgang Stanglmeier.  All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
***************************************************************************
*/

#ifndef __NCR_REG_H__
#define __NCR_REG_H__

/*==========================================================
**
**	OS dependencies.
**
**==========================================================
*/

#if defined(__NetBSD__) || defined (__OpenBSD__)
	#define INT8      int8_t
	#define U_INT8    u_int8_t
	#define INT16     int16_t
	#define U_INT16   u_int16_t
	#define INT32     int32_t
	#define U_INT32   u_int32_t
	#define TIMEOUT   (void*)
#else  /* __NetBSD__ || __OpenBSD__ */
	#define INT8      char
	#define U_INT8    u_char
	#define INT16     short
	#define U_INT16   u_short
	#define INT32     int32
	#define U_INT32   u_int32
	#define TIMEOUT   (timeout_func_t)
#endif /* __NetBSD__ || __OpenBSD__ */

/*-----------------------------------------------------------------
**
**	The ncr 53c810 register structure.
**
**-----------------------------------------------------------------
*/

struct ncr_reg {
/*00*/  U_INT8    nc_scntl0;    /* full arb., ena parity, par->ATN  */

/*01*/  U_INT8    nc_scntl1;    /* no reset                         */
        #define   ISCON   0x10  /* connected to scsi		    */
        #define   CRST    0x08  /* force reset                      */

/*02*/  U_INT8    nc_scntl2;    /* no disconnect expected           */
	#define   SDU     0x80  /* cmd: disconnect will raise error */
	#define   CHM     0x40  /* sta: chained mode                */
	#define   WSS     0x08  /* sta: wide scsi send           [W]*/
	#define   WSR     0x01  /* sta: wide scsi received       [W]*/

/*03*/  U_INT8    nc_scntl3;    /* cnf system clock dependent       */
	#define   EWS     0x08  /* cmd: enable wide scsi         [W]*/

/*04*/  U_INT8    nc_scid;	/* cnf host adapter scsi address    */
	#define   RRE     0x40  /* r/w:e enable response to resel.  */
	#define   SRE     0x20  /* r/w:e enable response to select  */

/*05*/  U_INT8    nc_sxfer;	/* ### Sync speed and count         */

/*06*/  U_INT8    nc_sdid;	/* ### Destination-ID               */

/*07*/  U_INT8    nc_gpreg;	/* ??? IO-Pins                      */

/*08*/  U_INT8    nc_sfbr;	/* ### First byte in phase          */

/*09*/  U_INT8    nc_socl;
	#define   CREQ	  0x80	/* r/w: SCSI-REQ                    */
	#define   CACK	  0x40	/* r/w: SCSI-ACK                    */
	#define   CBSY	  0x20	/* r/w: SCSI-BSY                    */
	#define   CSEL	  0x10	/* r/w: SCSI-SEL                    */
	#define   CATN	  0x08	/* r/w: SCSI-ATN                    */
	#define   CMSG	  0x04	/* r/w: SCSI-MSG                    */
	#define   CC_D	  0x02	/* r/w: SCSI-C_D                    */
	#define   CI_O	  0x01	/* r/w: SCSI-I_O                    */

/*0a*/  U_INT8    nc_ssid;

/*0b*/  U_INT8    nc_sbcl;

/*0c*/  U_INT8    nc_dstat;
        #define   DFE     0x80  /* sta: dma fifo empty              */
        #define   MDPE    0x40  /* int: master data parity error    */
        #define   BF      0x20  /* int: script: bus fault           */
        #define   ABRT    0x10  /* int: script: command aborted     */
        #define   SSI     0x08  /* int: script: single step         */
        #define   SIR     0x04  /* int: script: interrupt instruct. */
        #define   IID     0x01  /* int: script: illegal instruct.   */

/*0d*/  U_INT8    nc_sstat0;
        #define   ILF     0x80  /* sta: data in SIDL register lsb   */
        #define   ORF     0x40  /* sta: data in SODR register lsb   */
        #define   OLF     0x20  /* sta: data in SODL register lsb   */
        #define   AIP     0x10  /* sta: arbitration in progress     */
        #define   LOA     0x08  /* sta: arbitration lost            */
        #define   WOA     0x04  /* sta: arbitration won             */
        #define   IRST    0x02  /* sta: scsi reset signal           */
        #define   SDP     0x01  /* sta: scsi parity signal          */

/*0e*/  U_INT8    nc_sstat1;
	#define   FF3210  0xf0	/* sta: bytes in the scsi fifo      */

/*0f*/  U_INT8    nc_sstat2;
        #define   ILF1    0x80  /* sta: data in SIDL register msb[W]*/
        #define   ORF1    0x40  /* sta: data in SODR register msb[W]*/
        #define   OLF1    0x20  /* sta: data in SODL register msb[W]*/
        #define   LDSC    0x02  /* sta: disconnect & reconnect      */

/*10*/  U_INT32   nc_dsa;	/* --> Base page                    */

/*14*/  U_INT8    nc_istat;	/* --> Main Command and status      */
        #define   CABRT   0x80  /* cmd: abort current operation     */
        #define   SRST    0x40  /* mod: reset chip                  */
        #define   SIGP    0x20  /* r/w: message from host to ncr    */
        #define   SEM     0x10  /* r/w: message between host + ncr  */
        #define   CON     0x08  /* sta: connected to scsi           */
        #define   INTF    0x04  /* sta: int on the fly (reset by wr)*/
        #define   SIP     0x02  /* sta: scsi-interrupt              */
        #define   DIP     0x01  /* sta: host/script interrupt       */

/*15*/  U_INT8    nc_15_;
/*16*/	U_INT8	  nc_16_;
/*17*/  U_INT8    nc_17_;

/*18*/	U_INT8	  nc_ctest0;
/*19*/  U_INT8    nc_ctest1;

/*1a*/  U_INT8    nc_ctest2;
	#define   CSIGP   0x40

/*1b*/  U_INT8    nc_ctest3;
        #define   FLF     0x08  /* cmd: flush dma fifo              */
        #define   CLF	  0x04	/* cmd: clear dma fifo		    */
        #define   FM      0x02  /* mod: fetch pin mode              */
        #define   WRIE    0x01  /* mod: write and invalidate enable */

/*1c*/  U_INT32   nc_temp;	/* ### Temporary stack              */

/*20*/	U_INT8	  nc_dfifo;
/*21*/  U_INT8    nc_ctest4;
        #define   BDIS    0x80  /* mod: burst disable               */
        #define   MPEE    0x08  /* mod: master parity error enable  */

/*22*/  U_INT8    nc_ctest5;
/*23*/  U_INT8    nc_ctest6;

/*24*/  U_INT32   nc_dbc;	/* ### Byte count and command       */
/*28*/  U_INT32   nc_dnad;	/* ### Next command register        */
/*2c*/  U_INT32   nc_dsp;	/* --> Script Pointer               */
/*30*/  U_INT32   nc_dsps;	/* --> Script pointer save/opcode#2 */
/*34*/  U_INT32   nc_scratcha;  /* ??? Temporary register a         */

/*38*/  U_INT8    nc_dmode;
        #define   BL_2    0x80  /* mod: burst length shift value +2 */
        #define   BL_1    0x40  /* mod: burst length shift value +1 */
        #define   ERL     0x08  /* mod: enable read line            */
        #define   ERMP    0x04  /* mod: enable read multiple        */
        #define   BOF     0x02  /* mod: burst op code fetch         */

/*39*/  U_INT8    nc_dien;
/*3a*/  U_INT8    nc_dwt;

/*3b*/  U_INT8    nc_dcntl;	/* --> Script execution control     */
        #define   CLSE    0x80  /* mod: cache line size enable      */
        #define   PFF     0x40  /* cmd: pre-fetch flush             */
        #define   PFEN    0x20  /* mod: pre-fetch enable            */
        #define   SSM     0x10  /* mod: single step mode            */
        #define   IRQM    0x08  /* mod: irq mode (1 = totem pole !) */
        #define   STD     0x04  /* cmd: start dma mode              */
        #define   IRQD    0x02  /* mod: irq disable                 */
	#define	  NOCOM   0x01	/* cmd: protect sfbr while reselect */

/*3c*/  U_INT32   nc_adder;

/*40*/  U_INT16   nc_sien;	/* -->: interrupt enable            */
/*42*/  U_INT16   nc_sist;	/* <--: interrupt status            */
        #define   STO     0x0400/* sta: timeout (select)            */
        #define   GEN     0x0200/* sta: timeout (general)           */
        #define   HTH     0x0100/* sta: timeout (handshake)         */
        #define   MA      0x80  /* sta: phase mismatch              */
        #define   CMP     0x40  /* sta: arbitration complete        */
        #define   SEL     0x20  /* sta: selected by another device  */
        #define   RSL     0x10  /* sta: reselected by another device*/
        #define   SGE     0x08  /* sta: gross error (over/underflow)*/
        #define   UDC     0x04  /* sta: unexpected disconnect       */
        #define   RST     0x02  /* sta: scsi bus reset detected     */
        #define   PAR     0x01  /* sta: scsi parity error           */

/*44*/  U_INT8    nc_slpar;
/*45*/  U_INT8    nc_swide;
/*46*/  U_INT8    nc_macntl;
/*47*/  U_INT8    nc_gpcntl;
/*48*/  U_INT8    nc_stime0;    /* cmd: timeout for select&handshake*/
/*49*/  U_INT8    nc_stime1;    /* cmd: timeout user defined        */
/*4a*/  U_INT16   nc_respid;    /* sta: Reselect-IDs                */

/*4c*/  U_INT8    nc_stest0;

/*4d*/  U_INT8    nc_stest1;
	#define   DBLEN   0x08	/* clock doubler running		*/
	#define   DBLSEL  0x04	/* clock doubler selected		*/

/*4e*/  U_INT8    nc_stest2;
	#define   ROF     0x40	/* reset scsi offset (after gross error!) */
	#define   EXT     0x02  /* extended filtering                     */

/*4f*/  U_INT8    nc_stest3;
	#define   TE     0x80	/* c: tolerAnt enable */
	#define   CSF    0x02	/* c: clear scsi fifo */

/*50*/  U_INT16   nc_sidl;	/* Lowlevel: latched from scsi data */
/*52*/  U_INT16   nc_52_;
/*54*/  U_INT16   nc_sodl;	/* Lowlevel: data out to scsi data  */
/*56*/  U_INT16   nc_56_;
/*58*/  U_INT16   nc_sbdl;	/* Lowlevel: data from scsi data    */
/*5a*/  U_INT16   nc_5a_;
/*5c*/  U_INT8    nc_scr0;	/* Working register B               */
/*5d*/  U_INT8    nc_scr1;	/*                                  */
/*5e*/  U_INT8    nc_scr2;	/*                                  */
/*5f*/  U_INT8    nc_scr3;	/*                                  */
/*60*/
};

/*-----------------------------------------------------------
**
**	Utility macros for the script.
**
**-----------------------------------------------------------
*/

#define REGJ(p,r) (offsetof(struct ncr_reg, p ## r))
#define REG(r) REGJ (nc_, r)

#ifndef TARGET_MODE
#define TARGET_MODE 0
#endif

typedef U_INT32 ncrcmd;

#if BYTE_ORDER == BIG_ENDIAN
#define	SCR_BO(x)	(((x) >> 24) | (((x) >> 8) & 0xff00) | \
			 ((x) << 24) | (((x) & 0xff00) << 8))
#else
#define	SCR_BO(x)	(x)
#endif
/*-----------------------------------------------------------
**
**	SCSI phases
**
**-----------------------------------------------------------
*/

#define	SCR_DATA_OUT	0x00000000
#define	SCR_DATA_IN	0x01000000
#define	SCR_COMMAND	0x02000000
#define	SCR_STATUS	0x03000000
#define SCR_ILG_OUT	0x04000000
#define SCR_ILG_IN	0x05000000
#define SCR_MSG_OUT	0x06000000
#define SCR_MSG_IN      0x07000000

/*-----------------------------------------------------------
**
**	Data transfer via SCSI.
**
**-----------------------------------------------------------
**
**	MOVE_ABS (LEN)
**	<<start address>>
**
**	MOVE_IND (LEN)
**	<<dnad_offset>>
**
**	MOVE_TBL
**	<<dnad_offset>>
**
**-----------------------------------------------------------
*/

#define SCR_MOVE_ABS(l) ((0x08000000 ^ (TARGET_MODE << 1ul)) | (l))
#define SCR_MOVE_IND(l) ((0x28000000 ^ (TARGET_MODE << 1ul)) | (l))
#define SCR_MOVE_TBL     (0x18000000 ^ (TARGET_MODE << 1ul))

struct scr_tblmove {
        U_INT32  size;
        U_INT32  addr;
};

/*-----------------------------------------------------------
**
**	Selection
**
**-----------------------------------------------------------
**
**	SEL_ABS | SCR_ID (0..7)     [ | REL_JMP]
**	<<alternate_address>>
**
**	SEL_TBL | << dnad_offset>>  [ | REL_JMP]
**	<<alternate_address>>
**
**-----------------------------------------------------------
*/

#define	SCR_SEL_ABS	0x40000000
#define	SCR_SEL_ABS_ATN	0x41000000
#define	SCR_SEL_TBL	0x42000000
#define	SCR_SEL_TBL_ATN	0x43000000

struct scr_tblsel {
        U_INT8  sel_0;
        U_INT8  sel_sxfer;
        U_INT8  sel_id;
        U_INT8  sel_scntl3;
};

#define SCR_JMP_REL     0x04000000
#define SCR_ID(id)	(((U_INT32)(id)) << 16)

/*-----------------------------------------------------------
**
**	Waiting for Disconnect or Reselect
**
**-----------------------------------------------------------
**
**	WAIT_DISC
**	dummy: <<alternate_address>>
**
**	WAIT_RESEL
**	<<alternate_address>>
**
**-----------------------------------------------------------
*/

#define	SCR_WAIT_DISC	0x48000000
#define SCR_WAIT_RESEL  0x50000000

/*-----------------------------------------------------------
**
**	Bit Set / Reset
**
**-----------------------------------------------------------
**
**	SET (flags {|.. })
**
**	CLR (flags {|.. })
**
**-----------------------------------------------------------
*/

#define SCR_SET(f)     (0x58000000 | (f))
#define SCR_CLR(f)     (0x60000000 | (f))

#define	SCR_CARRY	0x00000400
#define	SCR_TRG		0x00000200
#define	SCR_ACK		0x00000040
#define	SCR_ATN		0x00000008




/*-----------------------------------------------------------
**
**	Memory to memory move
**
**-----------------------------------------------------------
**
**	COPY (bytecount)
**	<< source_address >>
**	<< destination_address >>
**
**-----------------------------------------------------------
*/

#define SCR_COPY(n) (0xc0000000 | (n))

/*-----------------------------------------------------------
**
**	Register move and binary operations
**
**-----------------------------------------------------------
**
**	SFBR_REG (reg, op, data)        reg  = SFBR op data
**	<< 0 >>
**
**	REG_SFBR (reg, op, data)        SFBR = reg op data
**	<< 0 >>
**
**	REG_REG  (reg, op, data)        reg  = reg op data
**	<< 0 >>
**
**-----------------------------------------------------------
*/

#define SCR_REG_OFS(ofs) ((ofs) << 16ul)

#define SCR_SFBR_REG(reg,op,data) \
        (0x68000000 | (SCR_REG_OFS(REG(reg))) | (op) | ((data)<<8ul))

#define SCR_REG_SFBR(reg,op,data) \
        (0x70000000 | (SCR_REG_OFS(REG(reg))) | (op) | ((data)<<8ul))

#define SCR_REG_REG(reg,op,data) \
        (0x78000000 | (SCR_REG_OFS(REG(reg))) | (op) | ((data)<<8ul))


#define      SCR_LOAD   0x00000000
#define      SCR_SHL    0x01000000
#define      SCR_OR     0x02000000
#define      SCR_XOR    0x03000000
#define      SCR_AND    0x04000000
#define      SCR_SHR    0x05000000
#define      SCR_ADD    0x06000000
#define      SCR_ADDC   0x07000000

/*-----------------------------------------------------------
**
**	FROM_REG (reg)		  reg  = SFBR
**	<< 0 >>
**
**	TO_REG	 (reg)		  SFBR = reg
**	<< 0 >>
**
**	LOAD_REG (reg, data)	  reg  = <data>
**	<< 0 >>
**
**	LOAD_SFBR(data) 	  SFBR = <data>
**	<< 0 >>
**
**-----------------------------------------------------------
*/

#define	SCR_FROM_REG(reg) \
	SCR_REG_SFBR(reg,SCR_OR,0)

#define	SCR_TO_REG(reg) \
	SCR_SFBR_REG(reg,SCR_OR,0)

#define	SCR_LOAD_REG(reg,data) \
	SCR_REG_REG(reg,SCR_LOAD,data)

#define SCR_LOAD_SFBR(data) \
        (SCR_REG_SFBR (gpreg, SCR_LOAD, data))

/*-----------------------------------------------------------
**
**	Waiting for Disconnect or Reselect
**
**-----------------------------------------------------------
**
**	JUMP            [ | IFTRUE/IFFALSE ( ... ) ]
**	<<address>>
**
**	JUMPR           [ | IFTRUE/IFFALSE ( ... ) ]
**	<<distance>>
**
**	CALL            [ | IFTRUE/IFFALSE ( ... ) ]
**	<<address>>
**
**	CALLR           [ | IFTRUE/IFFALSE ( ... ) ]
**	<<distance>>
**
**	RETURN          [ | IFTRUE/IFFALSE ( ... ) ]
**	<<dummy>>
**
**	INT             [ | IFTRUE/IFFALSE ( ... ) ]
**	<<ident>>
**
**	INT_FLY         [ | IFTRUE/IFFALSE ( ... ) ]
**	<<ident>>
**
**	Conditions:
**	     WHEN (phase)
**	     IF   (phase)
**	     CARRY
**	     DATA (data, mask)
**
**-----------------------------------------------------------
*/

#define SCR_JUMP        0x80080000
#define SCR_JUMPR       0x80880000
#define SCR_CALL        0x88080000
#define SCR_CALLR       0x88880000
#define SCR_RETURN      0x90080000
#define SCR_INT         0x98080000
#define SCR_INT_FLY     0x98180000

#define IFFALSE(arg)   (0x00080000 | (arg))
#define IFTRUE(arg)    (0x00000000 | (arg))

#define WHEN(phase)    (0x00030000 | (phase))
#define IF(phase)      (0x00020000 | (phase))

#define DATA(D)        (0x00040000 | ((D) & 0xff))
#define MASK(D,M)      (0x00040000 | (((M ^ 0xff) & 0xff) << 8ul)|((D) & 0xff))

#define CARRYSET       (0x00200000)

/*-----------------------------------------------------------
**
**	SCSI  constants.
**
**-----------------------------------------------------------
*/

/*
**	Messages
*/
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <scsi/scsi_message.h>

#define	M_COMPLETE	MSG_CMDCOMPLETE
#define	M_EXTENDED	MSG_EXTENDED
#define	M_SAVE_DP	MSG_SAVEDATAPOINTER
#define	M_RESTORE_DP	MSG_RESTOREPOINTERS
#define	M_DISCONNECT	MSG_DISCONNECT
#define	M_ID_ERROR	MSG_INITIATOR_DET_ERR
#define	M_ABORT		MSG_ABORT
#define	M_REJECT	MSG_MESSAGE_REJECT
#define	M_NOOP		MSG_NOOP
#define	M_PARITY	MSG_PARITY_ERROR
#define	M_LCOMPLETE	MSG_LINK_CMD_COMPLETE
#define	M_FCOMPLETE	MSG_LINK_CMD_COMPLETEF
#define	M_RESET		MSG_BUS_DEV_RESET
#define	M_ABORT_TAG	MSG_ABORT_TAG
#define	M_CLEAR_QUEUE	MSG_CLEAR_QUEUE
#define	M_INIT_REC	MSG_INIT_RECOVERY
#define	M_REL_REC	MSG_REL_RECOVERY
#define	M_TERMINATE	MSG_TERM_IO_PROC
#define	M_SIMPLE_TAG	MSG_SIMPLE_Q_TAG
#define	M_HEAD_TAG	MSG_HEAD_OF_Q_TAG
#define	M_ORDERED_TAG	MSG_ORDERED_Q_TAG
#define	M_IGN_RESIDUE	MSG_IGN_WIDE_RESIDUE
#define	M_IDENTIFY   	MSG_IDENTIFY(0, 0)

/* #define	M_X_MODIFY_DP	(0x00) */ 	/* XXX what is this? */
#define	M_X_SYNC_REQ	MSG_EXT_SDTR
#define	M_X_WIDE_REQ	MSG_EXT_WDTR
#else
#define M_COMPLETE      (0x00)
#define M_EXTENDED      (0x01)
#define M_SAVE_DP       (0x02)
#define M_RESTORE_DP    (0x03)
#define M_DISCONNECT    (0x04)
#define M_ID_ERROR      (0x05)
#define M_ABORT         (0x06)
#define M_REJECT        (0x07)
#define M_NOOP          (0x08)
#define M_PARITY        (0x09)
#define M_LCOMPLETE     (0x0a)
#define M_FCOMPLETE     (0x0b)  
#define M_RESET         (0x0c)  
#define M_ABORT_TAG     (0x0d)
#define M_CLEAR_QUEUE   (0x0e)
#define M_INIT_REC      (0x0f)
#define M_REL_REC       (0x10)
#define M_TERMINATE     (0x11)
#define M_SIMPLE_TAG    (0x20)
#define M_HEAD_TAG      (0x21) 
#define M_ORDERED_TAG   (0x22)
#define M_IGN_RESIDUE   (0x23)
#define M_IDENTIFY      (0x80) 

#define M_X_MODIFY_DP   (0x00)
#define M_X_SYNC_REQ    (0x01)
#define M_X_WIDE_REQ    (0x03)
#endif /* __NetBSD__ || __OpenBSD__ */


/*
**	Status
*/

#define	S_GOOD		(0x00)
#define	S_CHECK_COND	(0x02)
#define	S_COND_MET	(0x04)
#define	S_BUSY		(0x08)
#define	S_INT		(0x10)
#define	S_INT_COND_MET	(0x14)
#define	S_CONFLICT	(0x18)
#define	S_TERMINATED	(0x20)
#define	S_QUEUE_FULL	(0x28)
#define	S_ILLEGAL	(0xff)
#define	S_SENSE		(0x80)

#endif /*__NCR_REG_H__*/
