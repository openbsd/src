/* $NetBSD: iic.h,v 1.1 1996/04/19 19:52:46 mark Exp $ */

/*
 * Copyright (c) 1996 Mark Brinicombe.
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * iic.h
 *
 * header file for IIC
 *
 * Created      : 15/04/96
 */

/*
 * IIC bus driver attach arguments
 */

#ifdef _KERNEL

struct iicbus_attach_args {
	u_int	ib_addr;	/* i/o address */
	void	*ib_aux;	/* driver specific */
};

#define iic_bitdelay 10

#define IIC_WRITE	0x00
#define IIC_READ	0x01

/* Prototypes for assembly functions */

void iic_set_state		__P((int data, int clock));
void iic_set_state_and_ack	__P((int data, int clock));
void iic_delay			__P((int delay));

/* Prototype for kernel interface to IIC bus */

int iic_control __P((u_char address, u_char *buffer, int count));

#endif /* _KERNEL */

/* End of iic.h */
