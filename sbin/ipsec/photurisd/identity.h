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
/* $Id */
/*
 * identity.h:
 * identity for a security association
 */

#ifndef _IDENTITY_H_
#define _IDENTITY_H_
#include "state.h"

struct identity {
     struct identity *next;
     struct identity *root;
     int type;
     char *tag;
     char *pairid;
     void *object;
};

enum hashes {
     HASH_MD5 = 0,
     HASH_SHA1 };

struct idxform {
     enum hashes type;		/* Type of the transform */
     int id;			/* Photuris Attribute ID */
     u_int8_t hashsize;		/* Size of the hash */
     void *ctx;			/* Pointer to a context */
     int ctxsize;
     void *ctx2;		/* Pointer to a 2nd context for speedup */
     void (*Init)(void *);
     void (*Update)(void *, unsigned char *, unsigned int);
     void (*Final)(unsigned char *, void *);
};

#undef EXTERN
#ifdef _IDENTITY_C_
#define EXTERN

char *secret_file = NULL;

#else
#define EXTERN extern

extern char *secret_file;
#endif

#define ID_LOCAL         1
#define ID_LOCALPAIR     2
#define ID_REMOTE        4
#define ID_LOOKUP        8

#define IDENT_LOCAL      "identity local"
#define IDENT_LOCALPAIR  "identity pair local"
#define IDENT_REMOTE     "identity remote"
#define IDENT_LOOKUP     "identity lookup"

#define MAX_IDENT        120
#define MAX_IDENT_SECRET 120

#define MD5_SIZE         16
#define SHA1_SIZE        20

#define HASH_MAX         20      /* Keep this uptodate with hashsizes */

int init_identities(char *name, struct identity *ob);
int identity_insert(struct identity **idob, struct identity *ob);
int identity_unlink(struct identity **idob, struct identity *ob);
struct identity *identity_new(void);
struct identity *identity_root(void);
int identity_value_reset(struct identity *ob);
struct identity *identity_find(struct identity *ob, char *id, int type);
void identity_cleanup(struct identity **idob);

int get_secrets(struct stateob *st, int mode);
int choose_identity(struct stateob *st, u_int8_t *packet, u_int16_t *size,
		     u_int8_t *attributes, u_int16_t attribsize);
u_int16_t get_identity_verification_size(struct stateob *st, u_int8_t *choice);
int create_identity_verification(struct stateob *st, u_int8_t *buffer, 
				 u_int8_t *packet, u_int16_t size);
int  verify_identity_verification(struct stateob *st, u_int8_t *buffer,
				  u_int8_t *packet, u_int16_t size);

struct idxform *get_hash_id(int id);
struct idxform *get_hash(enum hashes hashtype);
int create_verification_key(struct stateob *, u_int8_t *, u_int16_t *, int);

int idsign(struct stateob *, struct idxform *, u_int8_t *, 
	   u_int8_t *, u_int16_t);
int idverify(struct stateob *, struct idxform *, u_int8_t *, 
	     u_int8_t *, u_int16_t);
#endif
