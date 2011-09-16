/*      $OpenBSD: mtpr.h,v 1.7 2011/09/16 17:20:07 miod Exp $     */
/*      $NetBSD: mtpr.h,v 1.12 1999/06/06 19:06:29 ragge Exp $     */

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
 * All rights reserved.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

 /* All bugs are subject to removal without further notice */

#ifndef	_MACHINE_MTPR_H_
#define	_MACHINE_MTPR_H_

/******************************************************************************

  Processor register numbers in the VAX		/IC

******************************************************************************/


#define PR_KSP     0 /* Kernel Stack Pointer */
#define PR_ESP     1 /* Executive Stack Pointer */
#define PR_SSP     2 /* Supervisor Stack Pointer */
#define PR_USP     3 /* User Stack Pointer */
#define PR_ISP     4 /* Interrupt Stack Pointer */

#define PR_P0BR    8 /* P0 Base Register */
#define PR_P0LR    9 /* P0 Length Register */
#define PR_P1BR   10 /* P1 Base Register */
#define PR_P1LR   11 /* P1 Length Register */
#define PR_SBR    12 /* System Base Register */
#define PR_SLR    13 /* System Limit Register */
#define PR_PCBB   16 /* Process Control Block Base */
#define PR_SCBB   17 /* System Control Block Base */ 
#define PR_IPL    18 /* Interrupt Priority Level */
#define PR_ASTLVL 19 /* AST Level */
#define PR_SIRR   20 /* Software Interrupt Request */
#define PR_SISR   21 /* Software Interrupt Summary */
#define	PR_IPIR	  22 /* KA820 Interprocessor register */
#define PR_MCSR   23 /* Machine Check Status Register 11/750 */
#define PR_ICCS   24 /* Interval Clock Control */
#define PR_NICR   25 /* Next Interval Count */
#define PR_ICR    26 /* Interval Count */
#define PR_TODR   27 /* Time Of Year (optional) */
#define	PR_CSRS	  28 /* Console Storage R/S */
#define	PR_CSRD	  29 /* Console Storage R/D */
#define	PR_CSTS	  30 /* Console Storage T/S */
#define	PR_CSTD	  31 /* Console Storage T/D */
#define PR_RXCS   32 /* Console Receiver C/S */
#define PR_RXDB   33 /* Console Receiver D/B */
#define PR_TXCS   34 /* Console Transmit C/S */
#define PR_TXDB   35 /* Console Transmit D/B */
#define PR_TBDR   36 /* Translation Buffer Group Disable Register 11/750 */
#define PR_CADR   37 /* Cache Disable Register 11/750 */
#define PR_MCESR  38 /* Machine Check Error Summary Register 11/750 */
#define PR_CAER   39 /* Cache Error Register 11/750 */
#define PR_ACCS   40 /* Accelerator control register */
#define PR_SAVISP 41 /* Console Saved ISP */
#define PR_SAVPC  42 /* Console Saved PC */
#define PR_SAVPSL 43 /* Console Saved PSL */
#define PR_WCSA   44 /* WCS Address */
#define PR_WCSB   45 /* WCS Data */
#define PR_SBIFS  48 /* SBI Fault/Status */
#define PR_SBIS   49 /* SBI Silo */
#define PR_SBISC  50 /* SBI Silo Comparator */
#define PR_SBIMT  51 /* SBI Silo Maintenance */
#define PR_SBIER  52 /* SBI Error Register */
#define PR_SBITA  53 /* SBI Timeout Address Register */
#define PR_SBIQC  54 /* SBI Quadword Clear */
#define PR_IUR    55 /* Initialize Unibus Register 11/750 */
#define PR_MAPEN  56 /* Memory Management Enable */
#define PR_TBIA   57 /* Trans. Buf. Invalidate All */
#define PR_TBIS   58 /* Trans. Buf. Invalidate Single */
#define PR_TBDATA 59 /* Translation Buffer Data */
#define PR_MBRK   60 /* Microprogram Break */
#define PR_PMR    61 /* Performance Monitor Enable */
#define PR_SID    62 /* System ID Register */
#define PR_TBCHK  63 /* Translation Buffer Check */

#define	PR_PAMACC 64 /* Physical Address Memory Map Access (KA86) */
#define	PR_PAMLOC 65 /* Physical Address Memory Map Location (KA86) */
#define PR_CSWP   66 /* Cache Sweep (KA86) */
#define PR_MDECC  67 /* MBOX Data Ecc Register (KA86) */
#define PR_MENA   68 /* MBOX Error Enable Register (KA86) */
#define PR_MDCTL  69 /* MBOX Data Control Register (KA86) */
#define PR_MCCTL  70 /* MBOX Mcc Control Register (KA86) */
#define PR_MERG   71 /* MBOX Error Generator Register (KA86) */
#define PR_CRBT   72 /* Console Reboot (KA86) */
#define PR_DFI    73 /* Diagnostic Fault Insertion Register (KA86) */
#define PR_EHSR   74 /* Error Handling Status Register (KA86) */
#define PR_STXCS  76 /* Console Storage C/S (KA86) */
#define PR_STXDB  77 /* Console Storage D/B (KA86) */
#define PR_ESPA   78 /* EBOX Scratchpad Address (KA86) */
#define PR_ESPD   79 /* EBOX Scratchpad Data (KA86) */

#define	PR_RXCS1  80 /* Serial-Line Unit 1 Receive CSR (KA820) */
#define	PR_RXDB1  81 /* Serial-Line Unit 1 Receive Data Buffer (KA820) */
#define	PR_TXCS1  82 /* Serial-Line Unit 1 Transmit CSR (KA820) */
#define	PR_TXDB1  83 /* Serial-Line Unit 1 Transmit Data Buffer (KA820) */
#define	PR_RXCS2  84 /* Serial-Line Unit 2 Receive CSR (KA820) */
#define	PR_RXDB2  85 /* Serial-Line Unit 2 Receive Data Buffer (KA820) */
#define	PR_TXCS2  86 /* Serial-Line Unit 2 Transmit CSR (KA820) */
#define	PR_TXDB2  87 /* Serial-Line Unit 2 Transmit Data Buffer (KA820) */
#define	PR_RXCS3  88 /* Serial-Line Unit 3 Receive CSR (KA820) */
#define	PR_RXDB3  89 /* Serial-Line Unit 3 Receive Data Buffer (KA820) */
#define	PR_TXCS3  90 /* Serial-Line Unit 3 Transmit CSR (KA820) */
#define	PR_TXDB3  91 /* Serial-Line Unit 3 Transmit Data Buffer (KA820) */
#define	PR_RXCD	  92 /* Receive Console Data from another cpu (KA820) */
#define	PR_CACHEX 93 /* Cache invalidate Register (KA820) */
#define	PR_BINID  94 /* VAXBI node ID Register (KA820) */
#define	PR_BISTOP 95 /* VAXBI Stop Register (KA820) */

#define PR_BCBTS  113 /* Backup Cache Tag Store (KA670) */
#define PR_BCP1TS 114 /* Primary Tag Store 1st half (KA670) */
#define PR_BCP2TS 115 /* Primary Tag Store 2st half (KA670) */
#define PR_BCRFR  116 /* Refresh Register (KA670) */
#define PR_BCIDX  117 /* Index Register (KA670) */
#define PR_BCSTS  118 /* Status (KA670) */
#define PR_BCCTL  119 /* Control Register (KA670) */
#define PR_BCERR  120 /* Error Address (KA670) */
#define PR_BCFBTS 121 /* Flush backup tag store (KA670) */
#define PR_BCFPTS 122 /* Flush primary tag store (KA670) */

#define	PR_VINTSR 123 /* vector i/f error status (KA43/KA46) */
#define PR_PCTAG  124 /* primary cache tag store (KA43/KA46) */
#define PR_PCIDX  125 /* primary cache index (KA43/KA46) */
#define PR_PCERR  126 /* primary cache error address (KA43/KA46) */
#define PR_PCSTS  127 /* primary cache status (KA43/KA46) */

/* Definitions for AST */
#define	AST_NO	  4
#define	AST_OK	  3

#ifndef	_LOCORE

#define mtpr(val,reg)                                   \
{                                                       \
        __asm__ __volatile ("mtpr %0,%1"                    \
                        : /* No output */               \
                        : "g" (val), "g" (reg));        \
}

#define mfpr(reg)                                       \
({                                                      \
        register int val;                               \
        __asm__ __volatile ("mfpr %1,%0"                    \
                        : "=g" (val)                    \
                        : "g" (reg));                   \
        val;                                            \
})
#endif	/* _LOCORE */

#endif /* _MACHINE_MTPR_H_ */
