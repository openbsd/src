/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
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
/* $Id: kernel.h,v 1.2 1997/07/23 12:28:52 provos Exp $ */
/*
 * kernel.h: 
 * security paramter index creation.
 */
 
#ifndef _KERNEL_H_
#define _KERNEL_H_

#undef EXTERN
#ifdef _KERNEL_C_
#define EXTERN

int kernel_xf_set(struct encap_msghdr *em);
int kernel_xf_read(struct encap_msghdr *em, int msglen);

int kernel_des(char *srcaddress, char *dstaddress, 
	       u_int8_t *spi, u_int8_t *secret, int tunnel);
int kernel_md5(char *srcaddress, char *dstaddress, 
	       u_int8_t *spi, u_int8_t *secret, int tunnel);

int kernel_group_spi(char *address, u_int8_t *spi);

int kernel_enable_spi(in_addr_t isrc, in_addr_t ismask, 
		      in_addr_t idst, in_addr_t idmask, 
		      char *address, u_int8_t *spi, int proto, int flags);
int kernel_disable_spi(in_addr_t isrc, in_addr_t ismask, 
		       in_addr_t idst, in_addr_t idmask, 
		       char *address, u_int8_t *spi, int proto, int flags);
int kernel_delete_spi(char *address, u_int8_t *spi, int proto);

#else
#define EXTERN extern
#endif

EXTERN u_int32_t kernel_reserve_spi( char *srcaddress);

EXTERN int kernel_insert_spi(struct spiob *SPI);
EXTERN int kernel_unlink_spi(struct spiob *ospi);
EXTERN int init_kernel(void);
EXTERN int kernel_get_socket(void);

#endif /* _KERNEL_H */
