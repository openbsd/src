/*	$OpenBSD: identity.c,v 1.8 2002/12/06 02:17:42 deraadt Exp $	*/

/*
 * Copyright 1997-2000 Niels Provos <provos@citi.umich.edu>
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
 * identity.c:
 * handling identity choices and creation of the before mentioned.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: identity.c,v 1.8 2002/12/06 02:17:42 deraadt Exp $";
#endif

#define _IDENTITY_C_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <md5.h>
#include <ssl/bn.h>
#include <sha1.h>
#include "config.h"
#include "photuris.h"
#include "state.h"
#include "attributes.h"
#include "modulus.h"
#include "exchange.h"
#include "identity.h"
#include "buffer.h"
#include "scheme.h"
#include "log.h"

#ifdef NEED_STRSEP
#include "strsep.h"
#endif

static struct identity *idob = NULL;

static union {
     MD5_CTX md5ctx;
     SHA1_CTX sha1ctx;
} Ctx, Ctx2;

/* Identity transforms */
/* XXX - argh, cast the funtions */

static struct idxform idxform[] = {
     { HASH_MD5, 5, MD5_SIZE, (void *)&Ctx.md5ctx,
       sizeof(MD5_CTX), (void *)&Ctx2.md5ctx,
       (void (*)(void *))MD5Init,
       (void (*)(void *, unsigned char *, unsigned int))MD5Update,
       (void (*)(unsigned char *, void *))MD5Final },
     { HASH_SHA1, 6, SHA1_SIZE, (void *)&Ctx.sha1ctx,
       sizeof(SHA1_CTX), (void *)&Ctx2.sha1ctx,
       (void (*)(void *))SHA1Init,
       (void (*)(void *, unsigned char *, unsigned int))SHA1Update,
       (void (*)(unsigned char *, void *))SHA1Final },
};

int
init_identities(char *name, struct identity *root)
{
     FILE *fp;
     char *p, *p2, *file = secret_file;
     struct identity *tmp, **ob;
     struct passwd *pwd;
     struct stat sb;
     int type;

     if (name != NULL) {
	  ob = (struct identity **)&root->object;
	  file = name;
     } else
	  ob = &idob;

     if (lstat(file, &sb) == -1) {
	  log_error("lstat() on %s in init_identities()", file);
	  return -1;
     }
     if (((sb.st_mode & S_IFMT) & ~S_IFREG)) {
	  log_print("no regular file %s in init_identities()", file);
	  return -1;
     }
     fp = fopen(file, "r");
     if (fp == (FILE *) NULL)
     {
	  log_error("no hash secrets file %s", file);
	  return -1;
     }

#ifdef DEBUG
     if (name == NULL)
	  printf("[Reading identities + secrets]\n");
#endif

     while(fgets(buffer, BUFFER_SIZE,fp)) {
	  p=buffer;
	  while(isspace(*p))  /* Get rid of leading spaces */
	       p++;
	  if(*p == '#')       /* Ignore comments */
	       continue;
	  if(!strlen(p))
	       continue;

	  if (!strncmp(p, IDENT_LOCAL, strlen(IDENT_LOCAL))) {
	       type = ID_LOCAL;
	       p += strlen(IDENT_LOCAL);
	  } else if (!strncmp(p, IDENT_LOCALPAIR, strlen(IDENT_LOCALPAIR))) {
	       type = ID_LOCALPAIR;
	       p += strlen(IDENT_LOCALPAIR);
	  } else if (!strncmp(p, IDENT_REMOTE, strlen(IDENT_REMOTE))) {
	       type = ID_REMOTE;
	       p += strlen(IDENT_REMOTE);
	  } else if (!strncmp(p, IDENT_LOOKUP, strlen(IDENT_LOOKUP))) {
	       type = ID_LOOKUP;
	       p += strlen(IDENT_LOOKUP);
	  } else {
	       log_print("Unknown tag %s in %s", p, file);
	       continue;
	  }
		
	  if ((tmp = identity_new()) == NULL) {
	       log_print("identity_new() in init_identities()");
	       continue;
	  }

	  p2 = p;
	  if (!isspace(*p2))
	       continue;
	
	  /* Tokens are braced with "token" */
	  if((p=strsep(&p2, "\"\'")) == NULL ||
	     (p=strsep(&p2, "\"\'")) == NULL)
	       continue;

	  tmp->type = type;
	  tmp->tag = strdup(p);
	  tmp->root = root;

	  switch(type) {
	  case ID_LOCAL:
	  case ID_REMOTE:
	       if (type == ID_REMOTE) {
		    /* Search for duplicates */
		    if (identity_find(idob, tmp->tag, ID_REMOTE) != NULL) {
			 log_print("Duplicate id \"%s\" found in %s",
				   tmp->tag, name != NULL ? name : "root");
			 identity_value_reset(tmp);
			 continue;
		    }
	       }
	       /* Tokens are braced with "token" */
	       if((p=strsep(&p2, "\"\'")) == NULL ||
		  (p=strsep(&p2, "\"\'")) == NULL) {
		    identity_value_reset(tmp);
		    continue;
	       }
	       tmp->object = strdup(p);
	       break;
	  case ID_LOCALPAIR:
	       /* Tokens are braced with "token" */
	       if((p=strsep(&p2, "\"\'")) == NULL ||
		  (p=strsep(&p2, "\"\'")) == NULL) {
		    identity_value_reset(tmp);
		    continue;
	       }
	       tmp->pairid = strdup(p);
	       /* Tokens are braced with "token" */
	       if((p=strsep(&p2, "\"\'")) == NULL ||
		  (p=strsep(&p2, "\"\'")) == NULL) {
		    identity_value_reset(tmp);
		    continue;
	       }
	       tmp->object = strdup(p);
	       break;
	  case ID_LOOKUP:
	       if (name != NULL) {
		    log_print("lookup in user file %s in init_identities()",
			      name);
		    continue;
	       }
	       while(isspace(*p2)) p2++;

	       while(isspace(p2[strlen(p2)-1]))
		    p2[strlen(p2)-1] = 0;

	       if ((pwd = getpwnam(p2)) == NULL) {
		    log_error("getpwnam() in init_identities()");
		    identity_value_reset(tmp);
		    continue;
	       } else {
		    char *dir = calloc(strlen(PHOTURIS_USER_SECRET)+
				       strlen(pwd->pw_dir) + 2,
				       sizeof(char));

		    /* This is the user name */
		    tmp->pairid = strdup(p2);

		    if (dir == NULL) {
			 log_error("calloc() in init_identities()");
			 identity_value_reset(tmp);
			 continue;
		    }
		    sprintf(dir,"%s/%s", pwd->pw_dir, PHOTURIS_USER_SECRET);
		    if (init_identities(dir, (struct identity *)tmp) == -1) {
			 free(dir);
			 identity_value_reset(tmp);
			 continue;
		    }

		    free(dir);
	       }
	       break;
	  }
	  identity_insert(ob, tmp);
     }
     fclose(fp);

     return 0;
}

/*
 * Get shared symmetric keys and identity, put the values in
 * the state object. If a SPI User ident is given, we look up
 * the matching remote secret.
 */

int
get_secrets(struct stateob *st, int mode)
{
     u_int8_t local_ident[MAX_IDENT];
     u_int8_t local_secret[MAX_IDENT_SECRET];
     u_int8_t remote_secret[MAX_IDENT_SECRET];

     struct identity *id, *root = idob;

     local_ident[0] = '\0';
     local_secret[0] = '\0';
     remote_secret[0] = '\0';

     /*
      * Remote secret first, if we find the remote secret in
      * a user secret file, we restrict our local searches
      * to that tree.
      */

     if(st->uSPIident != NULL && st->uSPIsecret == NULL &&
	(mode & ID_REMOTE)) {
	  int skip;

	  if (st->uSPIident[0] == 255 && st->uSPIident[1] == 255)
	       skip = 8;
	  else if (st->uSPIident[0] == 255)
	       skip = 4;
	  else
	       skip = 2;

	  id = identity_find(root, st->uSPIident+skip, ID_REMOTE);
	  if (id != NULL) {
               strncpy(remote_secret, id->object, MAX_IDENT_SECRET-1);
               remote_secret[MAX_IDENT_SECRET-1] = '\0';

	       if (id->root)
		    root = (struct identity *)id->root->object;
	  }
     }

     if (st->user != NULL &&
	 (id = identity_find(idob, st->user, ID_LOOKUP)) != NULL) {
	  /* User keying */
	  id = identity_find((struct identity *)id->object, NULL, ID_LOCAL);
     } else
	  id = NULL;

     if (id == NULL) {
	  /* Host keying */
	  id = identity_find(root, NULL, ID_LOCAL);
     }

      if (id != NULL && (mode & (ID_LOCAL|ID_LOCALPAIR))) {
	  /* Namespace: root->tag + user->tag */
	  if (id->root) {
	       strncpy(local_ident, id->root->tag, MAX_IDENT-1);
	       local_ident[MAX_IDENT-1] = '\0';
	  }
	  strncpy(local_ident+strlen(local_ident), id->tag,
		  MAX_IDENT-1-strlen(local_ident));
	  local_ident[MAX_IDENT_SECRET-1] = '\0';

	  strncpy(local_secret, id->object, MAX_IDENT_SECRET-1);
	  local_secret[MAX_IDENT_SECRET-1] = '\0';
     }
     if (st->uSPIident != NULL && st->oSPIident == NULL &&
	 (mode & (ID_LOCAL|ID_LOCALPAIR))) {
	  int skip;
	  if (st->uSPIident[0] == 255 && st->uSPIident[1] == 255)
	       skip = 8;
	  else if (st->uSPIident[0] == 255)
	       skip = 4;
	  else
	       skip = 2;

	  id = identity_find(root, st->uSPIident+skip, ID_LOCALPAIR);
	  if (id != NULL) {
	       local_ident[0] = '\0';
	       /* Namespace: root->tag + user->tag */
	       if (id->root) {
		    strncpy(local_ident, id->root->tag, MAX_IDENT-1);
		    local_ident[MAX_IDENT-1] = '\0';
	       }
	       strncpy(local_ident+strlen(local_ident), id->pairid,
		       MAX_IDENT-1-strlen(local_ident));
               local_ident[MAX_IDENT-1] = '\0';

               strncpy(local_secret, id->object, MAX_IDENT_SECRET-1);
               local_secret[MAX_IDENT_SECRET-1] = '\0';
	  }
     }
	
     if(strlen(remote_secret) == 0 && (mode & ID_REMOTE)) {
	  log_print("Can't find remote secret for %s in get_secrets()",
		st->uSPIident+2);
	  return -1;
     }

     if (strlen(local_ident) == 0 && (mode & (ID_LOCAL|ID_LOCALPAIR)) ) {
	  log_print("Can't find local identity in get_secrets()");
	  return -1;
     }

     if(st->oSPIident == NULL && (mode & (ID_LOCAL|ID_LOCALPAIR))) {
	  st->oSPIident = calloc(2+strlen(local_ident)+1,sizeof(u_int8_t));
	  if(st->oSPIident == NULL)
	       return -1;
	  strcpy(st->oSPIident+2,local_ident);
	  st->oSPIident[0] = ((strlen(local_ident)+1) >> 5) & 0xFF;
	  st->oSPIident[1] = ((strlen(local_ident)+1) << 3) & 0xFF;

	  st->oSPIsecretsize = strlen(local_secret);
	  st->oSPIsecret = calloc(st->oSPIsecretsize,sizeof(u_int8_t));
	  if(st->oSPIsecret == NULL)
	       return -1;
	  strncpy(st->oSPIsecret, local_secret, st->oSPIsecretsize);
     }
     if(st->uSPIident != NULL && st->uSPIsecret == NULL &&
	(mode & ID_REMOTE)) {
	  st->uSPIsecretsize = strlen(remote_secret);
          st->uSPIsecret = calloc(st->uSPIsecretsize,sizeof(u_int8_t));
          if(st->uSPIsecret == NULL)
               return -1;
          strncpy(st->uSPIsecret, remote_secret, st->uSPIsecretsize);
     }
     return 0;
}

int
choose_identity(struct stateob *st, u_int8_t *packet, u_int16_t *size,
		 u_int8_t *attributes, u_int16_t attribsize)
{
     u_int16_t rsize, asize, tmp;
     attrib_t *ob;
     int mode = 0;
     rsize = *size;

     /* XXX - preference of identity choice ? */
     tmp = 0;
     while(attribsize>0) {
	  /* Check if we support this identity choice */
	  if ((ob = getattrib(*attributes)) != NULL &&
	      (ob->type & AT_ID))
	       break;

	  if(attribsize -(*(attributes+1)+2) > attribsize) {
	       attribsize=0;
	       break;
	  }
	  attribsize -= *(attributes+1)+2;
	  attributes += *(attributes+1)+2;
     }

     if(attribsize == 0) {
	  log_print("No identity choice found in offered attributes "
		    "in choose_identity()");
	  return -1;
     }

     if(rsize < *(attributes+1)+2)
	  return -1;

     asize = *(attributes+1)+2;
     rsize -= asize;
     bcopy(attributes, packet, asize);

     /* Now put identity in state object */
     if (st->oSPIidentchoice == NULL) {
	  if ((st->oSPIidentchoice = calloc(asize, sizeof(u_int8_t))) == NULL)
	       return -1;
	  bcopy(attributes, st->oSPIidentchoice, asize);
	  st->oSPIidentchoicesize = asize;
     }

     packet += asize;

     /* Choose identity and secrets for Owner and User */
     if (st->uSPIsecret == NULL && st->uSPIident != NULL)
	  mode |= ID_REMOTE;
     if (st->oSPIsecret == NULL)
	  mode |= ID_LOCAL;
     if(get_secrets(st, mode) == -1)
	  return -1;

     /* oSPIident is varpre already */
     tmp = varpre2octets(st->oSPIident);
     if(rsize < tmp)
	  return -1;

     bcopy(st->oSPIident, packet, tmp);

     *size = asize + tmp;

     return 0;
}


u_int16_t
get_identity_verification_size(struct stateob *st, u_int8_t *choice)
{
     struct idxform *hash;

     if ((hash = get_hash_id(*choice)) == NULL) {
	  log_print("Unknown identity choice: %d\n", *choice);
	  return 0;
     }

     return hash->hashsize+2;
}

/*
 * Gets a hash corresponding with a Photuris ID
 */

struct idxform *get_hash_id(int id)
{
     int i;
     for (i=0; i<sizeof(idxform)/sizeof(idxform[0]); i++)
	  if (id == idxform[i].id)
	       return &idxform[i];
     return NULL;
}

struct idxform *get_hash(enum hashes hashtype)
{
     int i;
     for (i=0; i<sizeof(idxform)/sizeof(idxform[0]); i++)
	  if (hashtype == idxform[i].type)
	       return &idxform[i];
     log_print("Unknown hash type: %d in get_hash()", hashtype);
     return NULL;
}

int
create_verification_key(struct stateob *st, u_int8_t *buffer, u_int16_t *size,
			int owner)
{
     struct idxform *hash;
     int id = owner ? *(st->oSPIidentchoice) : *(st->uSPIidentchoice);

     if ((hash = get_hash_id(id)) == NULL) {
	  log_print("Unknown identity choice %d in create_verification_key", id);
          return -1;
     }

     if (*size < hash->hashsize)
	  return -1;

     hash->Init(hash->ctx);
     if (owner)
	  hash->Update(hash->ctx, st->oSPIsecret, st->oSPIsecretsize);
     else
	  hash->Update(hash->ctx, st->uSPIsecret, st->uSPIsecretsize);

     hash->Update(hash->ctx, st->shared, st->sharedsize);
     hash->Final(buffer, hash->ctx);
     *size = hash->hashsize;

     return 0;
}

int
create_identity_verification(struct stateob *st, u_int8_t *buffer,
			     u_int8_t *packet, u_int16_t size)
{
     int hash_size;
     struct idxform *hash;

     if ((hash = get_hash_id(*(st->oSPIidentchoice))) == NULL) {
	  log_print("Unknown identity choice %d in create_verification_key",
		    *(st->oSPIidentchoice));
          return 0;
     }

     hash_size = idsign(st, hash, buffer+2, packet,size);

     if(hash_size) {
	  /* Create varpre number from digest */
	  buffer[0] = hash_size >> 5 & 0xFF;
	  buffer[1] = hash_size << 3 & 0xFF;

	  if(st->oSPIidentver != NULL)
	       free(st->oSPIidentver);

	  st->oSPIidentver = calloc(hash_size+2,sizeof(u_int8_t));
	  if(st->oSPIidentver == NULL) {
	       log_error("Not enough memory in create_identity_verification()");
	       return 0;
	  }

	  bcopy(buffer, st->oSPIidentver, hash_size+2);
	  st->oSPIidentversize = hash_size+2;

	  state_save_verification(st, st->oSPIidentver, hash_size+2);
     }
     return hash_size+2;
}

int
verify_identity_verification(struct stateob *st, u_int8_t *buffer,
			     u_int8_t *packet, u_int16_t size)
{
     struct idxform *hash;

     if ((hash = get_hash_id(*(st->uSPIidentchoice))) == NULL) {
	  log_print("Unknown identity choice %d in create_verification_key",
		    *(st->uSPIidentchoice));
          return 0;
     }

     if (varpre2octets(buffer) != hash->hashsize +2)
	  return 0;

     state_save_verification(st, buffer, hash->hashsize+2);

     return idverify(st, hash, buffer+2, packet, size);
}


int
idsign(struct stateob *st, struct idxform *hash, u_int8_t *signature,
       u_int8_t *packet, u_int16_t psize)
{
     u_int8_t key[HASH_MAX];
     u_int16_t keylen = HASH_MAX;

     create_verification_key(st, key, &keylen, 1); /* Owner direction */

     hash->Init(hash->ctx);

     /* Our verification key */
     hash->Update(hash->ctx, key, keylen);
     /* Key fill */
     hash->Final(NULL, hash->ctx);

     /*
      * Hash Cookies, type, lifetime + spi fields +
      * SPI owner Identity Choice + Identity
      */
     hash->Update(hash->ctx, packet, IDENTITY_MESSAGE_MIN +
		  st->oSPIidentchoicesize + varpre2octets(st->oSPIident));

     if(st->uSPIident != NULL) {
	  hash->Update(hash->ctx, st->uSPIidentver, st->uSPIidentversize);
     }

     /* Hash attribute choice, padding */
     packet += IDENTITY_MESSAGE_MIN;
     psize -= IDENTITY_MESSAGE_MIN + packet[1] + 2;
     packet += packet[1] + 2;
     psize -= varpre2octets(packet) + 2 + hash->hashsize;
     packet += varpre2octets(packet) + 2 + hash->hashsize;

     hash->Update(hash->ctx, packet, psize);

     /* Our exchange value */
     hash->Update(hash->ctx, st->oSPITBV, 3);
     hash->Update(hash->ctx, st->exchangevalue, st->exchangesize);
     hash->Update(hash->ctx, st->oSPIoattrib, st->oSPIoattribsize);

     /* Their exchange value */
     hash->Update(hash->ctx, st->uSPITBV, 3);
     hash->Update(hash->ctx, st->texchange, st->texchangesize);
     hash->Update(hash->ctx, st->uSPIoattrib, st->uSPIoattribsize);

     /* Responder offered schemes */
     hash->Update(hash->ctx, st->roschemes, st->roschemesize);

     /* Data fill */
     hash->Final(NULL, hash->ctx);

     /* And finally the trailing key */
     hash->Update(hash->ctx, key, keylen);

     hash->Final(signature, hash->ctx);

     return hash->hashsize;
}

int
idverify(struct stateob *st, struct idxform *hash, u_int8_t *signature,
	 u_int8_t *packet, u_int16_t psize)
{
     u_int8_t digest[HASH_MAX];
     u_int8_t key[HASH_MAX];
     u_int16_t keylen = HASH_MAX;
     struct identity_message *p = (struct identity_message *)packet;

     create_verification_key(st, key, &keylen, 0); /* User direction */

     hash->Init(hash->ctx);

     /* Their verification key */
     hash->Update(hash->ctx, key, keylen);
     /* Key fill */
     hash->Final(NULL, hash->ctx);

     /*
      * Hash Cookies, type, lifetime + spi fields +
      * SPI owner Identity Choice + Identity
      */
     hash->Update(hash->ctx, packet, IDENTITY_MESSAGE_MIN +
		  st->uSPIidentchoicesize + varpre2octets(st->uSPIident));

     /* Determine if the sender knew our secret already */
     if(p->type != IDENTITY_REQUEST) {
	  hash->Update(hash->ctx, st->oSPIidentver, st->oSPIidentversize);
     }

     packet += IDENTITY_MESSAGE_MIN;
     psize -= IDENTITY_MESSAGE_MIN + packet[1] + 2;
     packet += packet[1] + 2;
     psize -= varpre2octets(packet) + 2 + hash->hashsize;
     packet += varpre2octets(packet) + 2 + hash->hashsize;
     hash->Update(hash->ctx, packet, psize);

     /* Their exchange value */
     hash->Update(hash->ctx, st->uSPITBV, 3);
     hash->Update(hash->ctx, st->texchange, st->texchangesize);
     hash->Update(hash->ctx, st->uSPIoattrib, st->uSPIoattribsize);

     /* Our exchange value */
     hash->Update(hash->ctx, st->oSPITBV, 3);
     hash->Update(hash->ctx, st->exchangevalue, st->exchangesize);
     hash->Update(hash->ctx, st->oSPIoattrib, st->oSPIoattribsize);

     /* Responder offered schemes */
     hash->Update(hash->ctx, st->roschemes, st->roschemesize);

     /* Data fill */
     hash->Final(NULL, hash->ctx);

     /* And finally the trailing key */
     hash->Update(hash->ctx, key, keylen);

     hash->Final(digest, hash->ctx);

     return !bcmp(digest, signature, hash->hashsize);
}

/* Functions for handling the linked list of identities */

int
identity_insert(struct identity **idob, struct identity *ob)
{
     struct identity *tmp;

     ob->next = NULL;

     if(*idob == NULL) {
	  *idob = ob;
	  return 1;
     }

     tmp=*idob;
     while(tmp->next!=NULL)
	  tmp = tmp->next;

     tmp->next = ob;
     return 1;
}

int
identity_unlink(struct identity **idob, struct identity *ob)
{
     struct identity *tmp;
     if(*idob == ob) {
	  *idob = ob->next;
	  free(ob);
	  return 1;
     }

     for(tmp=*idob; tmp!=NULL; tmp=tmp->next) {
	  if(tmp->next==ob) {
	       tmp->next=ob->next;
	       free(ob);
	       return 1;
	  }
     }
     return 0;
}

struct identity *
identity_new(void)
{
     struct identity *p;

     if((p = calloc(1, sizeof(struct identity)))==NULL)
	  return NULL;

     return p;
}

int
identity_value_reset(struct identity *ob)
{
     if (ob->tag != NULL)
	  free(ob->tag);
     if (ob->pairid != NULL)
	  free(ob->pairid);
     if (ob->object != NULL)
	  free(ob->object);

     return 1;
}

/*
 * find the state ob with matching address
 */

struct identity *
identity_root(void)
{
     return idob;
}

/* On ID_LOOKUP match pairid, on ID_LOCAL only match type */

struct identity *
identity_find(struct identity *idob, char *id, int type)
{
     struct identity *tmp = idob, *p;
     while(tmp!=NULL) {
          if(((type == ID_LOCAL && id == NULL) ||
	     (type != ID_LOOKUP && !strcmp(id, tmp->tag)) ||
	     (type == ID_LOOKUP && tmp->pairid != NULL && !strcmp(id, tmp->pairid))) &&
	     type == tmp->type)
		    return tmp;
	  if (tmp->type == ID_LOOKUP && tmp->object != NULL) {
	       p = identity_find((struct identity *)tmp->object, id, type);
	       if (p != NULL)
		    return p;
	  }
	  tmp = tmp->next;
     }
     return NULL;
}

void
identity_cleanup(struct identity **root)
{
     struct identity *p;
     struct identity *tmp;

     if (root == NULL)
	  tmp = idob;
     else
	  tmp = *root;

     while(tmp!=NULL) {
	  if (tmp->type == ID_LOOKUP)
	       identity_cleanup((struct identity **)&tmp->object);
	  p = tmp;
	  tmp = tmp->next;
	  identity_value_reset(p);
	  free(p);
     }

     if (root != NULL)
	  *root = NULL;
     else
	  idob = NULL;
}
