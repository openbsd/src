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
/*
 * attributes.h:
 * attributes for a security association
 */

#ifndef _ATTRIBUTES_H_
#define _ATTRIBUTES_H_

#undef EXTERN
#ifdef _ATTRIBUTES_C_
#define EXTERN
#else
#define EXTERN extern
#endif

#define AT_ID		1
#define AT_ENC		2
#define AT_AUTH		4

#define AT_PAD		0
#define AT_AH_ATTRIB	1
#define AT_ESP_ATTRIB	2
#define AT_HMAC		254

/* XXX - Only for the moment */
#define DH_G_2_MD5          2
#define DH_G_3_MD5          3
#define DH_G_2_DES_MD5      4    
#define DH_G_5_MD5          5
#define DH_G_3_DES_MD5      6
#define DH_G_VAR_MD5        7
#define DH_G_2_3DES_SHA1    8
#define DH_G_5_DES_MD5      10
#define DH_G_3_3DES_SHA1    12
#define DH_G_VAR_DES_MD5    14
#define DH_G_5_3DES_SHA1    20
#define DH_G_VAR_3DES_SHA1  28

typedef struct _attribute_list {
     struct _attribute_list *next;
     char *address;
     in_addr_t netmask;
     u_int8_t *attributes;
     u_int16_t attribsize;
} attribute_list;

typedef struct _attrib_t {
     struct _attrib_t *next;
     u_int16_t id;		/* Photuris Attribute ID */
     int type;			/* Type of attribute: ident, enc, auth */
     int klen;			/* required key length */
} attrib_t;

#define ATTRIBHASHMOD		17

EXTERN void putattrib(attrib_t *attrib);
EXTERN attrib_t *getattrib(u_int8_t id);
EXTERN void clearattrib(void);

EXTERN void get_attrib_section(u_int8_t *, u_int16_t, u_int8_t **, u_int16_t *,
			       u_int8_t);

EXTERN int isinattrib(u_int8_t *attributes, u_int16_t attribsize, 
		      u_int8_t attribute);
EXTERN int isattribsubset(u_int8_t *set, u_int16_t setsize, 
			  u_int8_t *subset, u_int16_t subsetsize);
EXTERN attribute_list *attrib_new(void);
EXTERN int attrib_insert(attribute_list *);
EXTERN int attrib_unlink(attribute_list *);
EXTERN int attrib_value_reset(attribute_list *);
EXTERN attribute_list *attrib_find(char *);
EXTERN void attrib_cleanup(void);

#endif /* ATTRIBUTES_H */
