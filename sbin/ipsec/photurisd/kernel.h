/*
 * Copyright 1997,1998 Niels Provos <provos@physnet.uni-hamburg.de>
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
/* $Id: kernel.h,v 1.6 1998/05/18 21:25:33 provos Exp $ */
/*
 * kernel.h: 
 * security paramter index creation.
 */
 
#ifndef _KERNEL_H_
#define _KERNEL_H_

#undef EXTERN
#ifdef _KERNEL_C_
#define EXTERN

#define ESP_OLD   0x01
#define ESP_NEW   0x02
#define AH_OLD    0x04
#define AH_NEW    0x08

#define XF_ENC    0x10
#define XF_AUTH   0x20


int kernel_xf_set(struct encap_msghdr *em);
int kernel_xf_read(struct encap_msghdr *em, int msglen);

int kernel_ah(attrib_t *ob, struct spiob *SPI, u_int8_t *secrets);
int kernel_esp(attrib_t *ob, attrib_t *ob2, struct spiob *SPI, 
	       u_int8_t *secrets);

int kernel_group_spi(char *address, u_int8_t *spi);

int kernel_enable_spi(in_addr_t isrc, in_addr_t ismask, 
		      in_addr_t idst, in_addr_t idmask, 
		      char *address, u_int8_t *spi, int proto, int flags);
int kernel_disable_spi(in_addr_t isrc, in_addr_t ismask, 
		       in_addr_t idst, in_addr_t idmask, 
		       char *address, u_int8_t *spi, int proto, int flags);
int kernel_delete_spi(char *address, u_int8_t *spi, int proto);

int kernel_request_sa(struct encap_msghdr *em);
#else
#define EXTERN extern
#endif

EXTERN int kernel_get_offset(int id);
EXTERN int kernel_valid(int encoff, int authoff);

EXTERN u_int32_t kernel_reserve_spi( char *srcaddress, int options);
EXTERN u_int32_t kernel_reserve_single_spi(char *srcaddress, u_int32_t spi, 
					   int proto);

EXTERN int kernel_insert_spi(struct stateob *st, struct spiob *SPI);
EXTERN int kernel_unlink_spi(struct spiob *ospi);
EXTERN int init_kernel(void);
EXTERN int kernel_get_socket(void);
EXTERN void kernel_set_socket_policy(int sd);
EXTERN void kernel_handle_notify(int sd);
EXTERN void kernel_notify_result(struct stateob *, struct spiob *, int);

#endif /* _KERNEL_H */
