/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
RCSID("$KTH: otp_md.c,v 1.9 1999/12/02 16:58:44 joda Exp $");
#endif
#include "otp_locl.h"

#include "otp_md.h"
#include <md4.h>
#include <md5.h>
#include <sha.h>

/*
 * Compress len bytes from md into key
 */

static void
compressmd (OtpKey key, unsigned char *md, size_t len)
{
  u_char *p = key;

  memset (p, 0, OTPKEYSIZE);
  while(len) {
    *p++ ^= *md++;
    *p++ ^= *md++;
    *p++ ^= *md++;
    *p++ ^= *md++;
    len -= 4;
    if (p == key + OTPKEYSIZE)
      p = key;
  }
}

static int
otp_md_init (OtpKey key,
	     char *pwd,
	     char *seed,
	     void (*init)(void *),
	     void (*update)(void *, void *, size_t),
	     void (*finito)(void *, void *),
	     void *arg,
	     unsigned char *res,
	     size_t ressz)
{
  char *p;
  int len;

  len = strlen(pwd) + strlen(seed);
  p = malloc (len + 1);
  if (p == NULL)
    return -1;
  strcpy (p, seed);
  strlwr (p);
  strcat (p, pwd);
  (*init)(arg);
  (*update)(arg, p, len);
  (*finito)(arg, res);
  free (p);
  compressmd (key, res, ressz);
  return 0;
}

static int
otp_md_next (OtpKey key,
	     void (*init)(void *),
	     void (*update)(void *, void *, size_t),
	     void (*finito)(void *, void *),
	     void *arg,
	     unsigned char *res,
	     size_t ressz)
{
  (*init)(arg);
  (*update)(arg, key, OTPKEYSIZE);
  (*finito)(arg, res);
  compressmd (key, res, ressz);
  return 0;
}

static int
otp_md_hash (char *data,
	     size_t len,
	     void (*init)(void *),
	     void (*update)(void *, void *, size_t),
	     void (*finito)(void *, void *),
	     void *arg,
	     unsigned char *res,
	     size_t ressz)
{
  (*init)(arg);
  (*update)(arg, data, len);
  (*finito)(arg, res);
  return 0;
}

int
otp_md4_init (OtpKey key, char *pwd, char *seed)
{
  unsigned char res[16];
  struct md4 md4;

  return otp_md_init (key, pwd, seed,
		      (void (*)(void *))md4_init,
		      (void (*)(void *, void *, size_t))md4_update, 
		      (void (*)(void *, void *))md4_finito,
		      &md4, res, sizeof(res));
}

int
otp_md4_hash (char *data,
	      size_t len,
	      unsigned char *res)
{
  struct md4 md4;

  return otp_md_hash (data, len,
		      (void (*)(void *))md4_init,
		      (void (*)(void *, void *, size_t))md4_update, 
		      (void (*)(void *, void *))md4_finito,
		      &md4, res, 16);
}

int
otp_md4_next (OtpKey key)
{
  unsigned char res[16];
  struct md4 md4;

  return otp_md_next (key, 
		      (void (*)(void *))md4_init, 
		      (void (*)(void *, void *, size_t))md4_update, 
		      (void (*)(void *, void *))md4_finito,
		      &md4, res, sizeof(res));
}


int
otp_md5_init (OtpKey key, char *pwd, char *seed)
{
  unsigned char res[16];
  struct md5 md5;

  return otp_md_init (key, pwd, seed, 
		      (void (*)(void *))md5_init, 
		      (void (*)(void *, void *, size_t))md5_update, 
		      (void (*)(void *, void *))md5_finito,
		      &md5, res, sizeof(res));
}

int
otp_md5_hash (char *data,
	      size_t len,
	      unsigned char *res)
{
  struct md5 md5;

  return otp_md_hash (data, len,
		      (void (*)(void *))md5_init,
		      (void (*)(void *, void *, size_t))md5_update, 
		      (void (*)(void *, void *))md5_finito,
		      &md5, res, 16);
}

int
otp_md5_next (OtpKey key)
{
  unsigned char res[16];
  struct md5 md5;

  return otp_md_next (key, 
		      (void (*)(void *))md5_init, 
		      (void (*)(void *, void *, size_t))md5_update, 
		      (void (*)(void *, void *))md5_finito,
		      &md5, res, sizeof(res));
}

/* 
 * For histerical reasons, in the OTP definition it's said that the
 * result from SHA must be stored in little-endian order.  See
 * draft-ietf-otp-01.txt.
 */

static void
sha_finito_little_endian (struct sha *m, void *res)
{
  unsigned char tmp[20];
  unsigned char *p = res;
  int j;

  sha_finito (m, tmp);
  for (j = 0; j < 20; j += 4) {
    p[j]   = tmp[j+3];
    p[j+1] = tmp[j+2];
    p[j+2] = tmp[j+1];
    p[j+3] = tmp[j];
  }
}

int
otp_sha_init (OtpKey key, char *pwd, char *seed)
{
  unsigned char res[20];
  struct sha sha;

  return otp_md_init (key, pwd, seed, 
		      (void (*)(void *))sha_init, 
		      (void (*)(void *, void *, size_t))sha_update, 
		      (void (*)(void *, void *))sha_finito_little_endian,
		      &sha, res, sizeof(res));
}

int
otp_sha_hash (char *data,
	      size_t len,
	      unsigned char *res)
{
  struct sha sha;

  return otp_md_hash (data, len,
		      (void (*)(void *))sha_init,
		      (void (*)(void *, void *, size_t))sha_update, 
		      (void (*)(void *, void *))sha_finito_little_endian,
		      &sha, res, 20);
}

int
otp_sha_next (OtpKey key)
{
  unsigned char res[20];
  struct sha sha;

  return otp_md_next (key, 
		      (void (*)(void *))sha_init, 
		      (void (*)(void *, void *, size_t))sha_update, 
		      (void (*)(void *, void *))sha_finito_little_endian,
		      &sha, res, sizeof(res));
}
