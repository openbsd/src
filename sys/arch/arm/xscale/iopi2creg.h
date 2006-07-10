/*	$OpenBSD: iopi2creg.h,v 1.2 2006/07/10 15:39:56 drahn Exp $	*/
/*	$NetBSD: iopi2creg.h,v 1.2 2005/12/11 12:16:51 christos Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARM_XSCALE_IOPIICREG_H_ 
#define _ARM_XSCALE_IOPIICREG_H_ 

#define	IIC_ICR		0x00	/* i2c control register */
#define	IIC_ISR		0x04	/* i2c status register */
#define	IIC_ISAR	0x08	/* i2c slave address register */
#define	IIC_IDBR	0x0c	/* i2c data buffer register */
#define	IIC_ICCR	0x10	/* i2c clock control register (i80312 only) */
#define	IIC_IBMR	0x14	/* i2c bus monitor register */

#define	IIC_ICR_FM		(1U << 15)	/* fast mode (i80321 only) */
#define	IIC_ICR_RESET		(1U << 14)	/* i2c unit reset */
#define	IIC_ICR_SADIE		(1U << 13)	/* slave addr det int en */
#define	IIC_ICR_ALDIE		(1U << 12)	/* arb loss det int en */
#define	IIC_ICR_SSDIE		(1U << 11)	/* slave stop det in en */
#define	IIC_ICR_BEIE		(1U << 10)	/* bus error int en */
#define	IIC_ICR_IRFIE		(1U << 9)	/* IDBR Rx full int en */
#define	IIC_ICR_ITEIE		(1U << 8)	/* IDBR Tx empty int en */
#define	IIC_ICR_GCD		(1U << 7)	/* general call disable */
#define	IIC_ICR_UE		(1U << 6)	/* i2c unit enable */
#define	IIC_ICR_SCLE		(1U << 5)	/* SCL master enable */
#define	IIC_ICR_MA		(1U << 4)	/* abort as master */
#define	IIC_ICR_TB		(1U << 3)	/* transfer byte */
#define	IIC_ICR_NACK		(1U << 2)	/* 0=ACK, 1=NACK */
#define	IIC_ICR_STOP		(1U << 1)	/* initiate STOP condition */
#define	IIC_ICR_START		(1U << 0)	/* initiate START condition */

#define	IIC_ISR_BED		(1U << 10)	/* bus error detected */
#define	IIC_ISR_SAD		(1U << 9)	/* slave address detected */
#define	IIC_ISR_GCAD		(1U << 8)	/* general call addr detected */
#define	IIC_ISR_IRF		(1U << 7)	/* IDBR Rx full */
#define	IIC_ISR_ITE		(1U << 6)	/* IDBR Tx empty */
#define	IIC_ISR_ALD		(1U << 5)	/* arb loss detected */
#define	IIC_ISR_SSD		(1U << 4)	/* slave STOP detected */
#define	IIC_ISR_IBB		(1U << 3)	/* i2c bus busy */
#define	IIC_ISR_UB		(1U << 2)	/* unit busy */
#define	IIC_ISR_NACK		(1U << 1)	/* NACK received */
#define	IIC_ISR_RW		(1U << 0)	/* 0=mt/sr, 1=mr/st */

#endif /* _ARM_XSCALE_IOPIICREG_H_  */
