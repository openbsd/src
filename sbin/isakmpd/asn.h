/*	$OpenBSD: asn.h,v 1.2 1998/11/15 00:43:49 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
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
 *	This product includes software developed by Ericsson Radio Systems.
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
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _ASN_H_
#define _ASN_H_

/* Very very simple module for compiling ASN.1 BER encoding */

enum asn_classes {
  UNIVERSAL = 0,
  APPLICATION = 1,
  CONTEXT = 2,
  PRIVATE = 3
};

#define TAG_EXPLICIT	4
#define TAG_EXPSHIFTS	5

#define ISEXPLICIT(x) ((x)->class & TAG_EXPLICIT)
#define ADD_EXP(x,y)	((x << TAG_EXPSHIFTS) | TAG_EXPLICIT | (y))
#define GET_EXP(x)	((x)->class >>  TAG_EXPSHIFTS)

enum asn_tags {
  TAG_BOOL = 1,
  TAG_INTEGER = 2,
  TAG_BITSTRING = 3,
  TAG_OCTETSTRING = 4,
  TAG_NULL = 5,
  TAG_OBJECTID = 6,		/* Internal Representation as ASCII String */
  TAG_SEQUENCE = 16,
  TAG_SET = 17,
  TAG_PRINTSTRING = 19,
  TAG_UTCTIME = 23,		/* Represenation as ASCII String */
  TAG_STOP = -1,       		/* None official ASN tag, indicates end */
  TAG_RAW = -2,			/* Placeholder for something we cant handle */
  TAG_ANY = -3,			/* Either we can handle it or it is RAW */
};

struct norm_type {
  enum asn_tags type;
  enum asn_classes class;
  const char *name;
  u_int32_t len;
  void *data;
  u_int32_t flags;
};

struct asn_objectid {
  char *name;
  char *objectid;
};

struct asn_handler {
  enum asn_tags type;
  void (*free) (struct norm_type *);
  u_int32_t (*get_encoded_len) (struct norm_type *, u_int8_t *type);
  u_int8_t *(*decode) (u_int8_t *, u_int32_t, struct norm_type *);
  u_int8_t *(*encode) (struct norm_type *, u_int8_t *);
};

#define ASN_FLAG_ZEROORMORE	0x0001

/* Construct a Sequence */
#define SEQ(x,y) {TAG_SEQUENCE, UNIVERSAL, x, 0, y}
#define SEQOF(x,y) {TAG_SEQUENCE, UNIVERSAL, x, 0, y, ASN_FLAG_ZEROORMORE}
#define SET(x,y) {TAG_SET, UNIVERSAL, x, 0, y}
#define SETOF(x,y) {TAG_SET, UNIVERSAL, x, 0, y, ASN_FLAG_ZEROORMORE}

#define TAG_TYPE(x) ((enum asn_tags)((x)[0] & 0x1f))

/* Tag modifiers */
#define ASN_CONSTRUCTED	0x20	/* Constructed object type */

/* Length modifiers */
#define ASN_LONG_FORM  	0x80	/* Number of length octets */

/* Function prototypes */

u_int8_t *asn_encode_integer (struct norm_type *, u_int8_t *);
u_int8_t *asn_decode_integer (u_int8_t *, u_int32_t, struct norm_type *);
u_int32_t asn_get_encoded_len_integer (struct norm_type *, u_int8_t *);
void asn_free_integer (struct norm_type *);

u_int8_t *asn_encode_string (struct norm_type *, u_int8_t *);
u_int8_t *asn_decode_string (u_int8_t *, u_int32_t, struct norm_type *);
u_int32_t asn_get_encoded_len_string (struct norm_type *, u_int8_t *);
void asn_free_string (struct norm_type *);

u_int8_t *asn_encode_objectid (struct norm_type *, u_int8_t *);
u_int8_t *asn_decode_objectid (u_int8_t *, u_int32_t, struct norm_type *);
u_int32_t asn_get_encoded_len_objectid (struct norm_type *, u_int8_t *);
void asn_free_objectid (struct norm_type *);

u_int8_t *asn_encode_raw (struct norm_type *, u_int8_t *);
u_int8_t *asn_decode_raw (u_int8_t *, u_int32_t, struct norm_type *);
u_int32_t asn_get_encoded_len_raw (struct norm_type *, u_int8_t *);
void asn_free_raw (struct norm_type *);

u_int8_t *asn_encode_null (struct norm_type *, u_int8_t *);
u_int8_t *asn_decode_null (u_int8_t *, u_int32_t, struct norm_type *);
u_int32_t asn_get_encoded_len_null (struct norm_type *, u_int8_t *);
void asn_free_null (struct norm_type *);

u_int8_t *asn_encode_sequence (struct norm_type *, u_int8_t *);
u_int8_t *asn_decode_sequence (u_int8_t *, u_int32_t, struct norm_type *);
u_int32_t asn_get_encoded_len_sequence (struct norm_type *, u_int8_t *);
void asn_free_sequence (struct norm_type *);

u_int8_t *asn_decode_any (u_int8_t *, u_int32_t, struct norm_type *);

void asn_free (struct norm_type *);

int asn_get_from_file (char *, u_int8_t **, u_int32_t *);
struct norm_type *asn_template_clone (struct norm_type *, int);

u_int32_t asn_sizeinoctets (u_int32_t);
u_int32_t asn_get_len (u_int8_t *);
u_int32_t asn_get_data_len (struct norm_type *, u_int8_t **, u_int8_t **);
u_int32_t asn_get_encoded_len (struct norm_type *, u_int32_t, u_int8_t *);

char *asn_parse_objectid (struct asn_objectid *, char *);
struct norm_type *asn_decompose (char *, struct norm_type *);
#endif /* _ASN_H_ */
