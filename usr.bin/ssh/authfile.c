/*

authfile.c

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Mon Mar 27 03:52:05 1995 ylo

This file contains functions for reading and writing identity files, and
for reading the passphrase from the user.

*/

#include "includes.h"
RCSID("$Id: authfile.c,v 1.2 1999/09/26 21:02:15 deraadt Exp $");

#include <gmp.h>
#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "cipher.h"
#include "ssh.h"

/* Version identification string for identity files. */
#define AUTHFILE_ID_STRING "SSH PRIVATE KEY FILE FORMAT 1.1\n"

/* Saves the authentication (private) key in a file, encrypting it with
   passphrase.  The identification of the file (lowest 64 bits of n)
   will precede the key to provide identification of the key without
   needing a passphrase. */

int save_private_key(const char *filename, const char *passphrase,
		     RSAPrivateKey *key, const char *comment, 
		     RandomState *state)
{
  Buffer buffer, encrypted;
  char buf[100], *cp;
  int f, i;
  CipherContext cipher;
  int cipher_type;

  /* If the passphrase is empty, use SSH_CIPHER_NONE to ease converting to
     another cipher; otherwise use SSH_AUTHFILE_CIPHER. */
  if (strcmp(passphrase, "") == 0)
    cipher_type = SSH_CIPHER_NONE;
  else
    cipher_type = SSH_AUTHFILE_CIPHER;

  /* This buffer is used to built the secret part of the private key. */
  buffer_init(&buffer);
  
  /* Put checkbytes for checking passphrase validity. */
  buf[0] = random_get_byte(state);
  buf[1] = random_get_byte(state);
  buf[2] = buf[0];
  buf[3] = buf[1];
  buffer_append(&buffer, buf, 4);

  /* Store the private key (n and e will not be stored because they will
     be stored in plain text, and storing them also in encrypted format
     would just give known plaintext). */
  buffer_put_mp_int(&buffer, &key->d);
  buffer_put_mp_int(&buffer, &key->u);
  buffer_put_mp_int(&buffer, &key->p);
  buffer_put_mp_int(&buffer, &key->q);

  /* Pad the part to be encrypted until its size is a multiple of 8. */
  while (buffer_len(&buffer) % 8 != 0)
    buffer_put_char(&buffer, 0);

  /* This buffer will be used to contain the data in the file. */
  buffer_init(&encrypted);

  /* First store keyfile id string. */
  cp = AUTHFILE_ID_STRING;
  for (i = 0; cp[i]; i++)
    buffer_put_char(&encrypted, cp[i]);
  buffer_put_char(&encrypted, 0);

  /* Store cipher type. */
  buffer_put_char(&encrypted, cipher_type);
  buffer_put_int(&encrypted, 0);  /* For future extension */

  /* Store public key.  This will be in plain text. */
  buffer_put_int(&encrypted, key->bits);
  buffer_put_mp_int(&encrypted, &key->n);
  buffer_put_mp_int(&encrypted, &key->e);
  buffer_put_string(&encrypted, comment, strlen(comment));

  /* Allocate space for the private part of the key in the buffer. */
  buffer_append_space(&encrypted, &cp, buffer_len(&buffer));

  cipher_set_key_string(&cipher, cipher_type, passphrase, 1);
  cipher_encrypt(&cipher, (unsigned char *)cp, 
		 (unsigned char *)buffer_ptr(&buffer),
		 buffer_len(&buffer));
  memset(&cipher, 0, sizeof(cipher));

  /* Destroy temporary data. */
  memset(buf, 0, sizeof(buf));
  buffer_free(&buffer);

  /* Write to a file. */
  f = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0600);
  if (f < 0)
    return 0;

  if (write(f, buffer_ptr(&encrypted), buffer_len(&encrypted)) != 
      buffer_len(&encrypted))
    {
      debug("Write to key file %.200s failed: %.100s", filename,
	    strerror(errno));
      buffer_free(&encrypted);
      close(f);
      remove(filename);
      return 0;
    }
  close(f);
  buffer_free(&encrypted);
  return 1;
}

/* Loads the public part of the key file.  Returns 0 if an error
   was encountered (the file does not exist or is not readable), and
   non-zero otherwise. */

int load_public_key(const char *filename, RSAPublicKey *pub, 
		    char **comment_return)
{
  int f, i;
  unsigned long len;
  Buffer buffer;
  char *cp;

  /* Read data from the file into the buffer. */
  f = open(filename, O_RDONLY);
  if (f < 0)
    return 0;

  len = lseek(f, (off_t)0L, 2);
  lseek(f, (off_t)0L, 0);
  
  buffer_init(&buffer);
  buffer_append_space(&buffer, &cp, len);

  if (read(f, cp, len) != len)
    {
      debug("Read from key file %.200s failed: %.100s", filename, 
	    strerror(errno));
      buffer_free(&buffer);
      close(f);
      return 0;
    }
  close(f);

  /* Check that it is at least big enought to contain the ID string. */
  if (len < strlen(AUTHFILE_ID_STRING) + 1)
    {
      debug("Bad key file %.200s.", filename);
      buffer_free(&buffer);
      return 0;
    }

  /* Make sure it begins with the id string.  Consume the id string from
     the buffer. */
  for (i = 0; i < (unsigned int)strlen(AUTHFILE_ID_STRING) + 1; i++)
    if (buffer_get_char(&buffer) != (unsigned char)AUTHFILE_ID_STRING[i])
      {
	debug("Bad key file %.200s.", filename);
	buffer_free(&buffer);
	return 0;
      }

  /* Skip cipher type and reserved data. */
  (void)buffer_get_char(&buffer); /* cipher type */
  (void)buffer_get_int(&buffer); /* reserved */

  /* Read the public key from the buffer. */
  pub->bits = buffer_get_int(&buffer);
  mpz_init(&pub->n);
  buffer_get_mp_int(&buffer, &pub->n);
  mpz_init(&pub->e);
  buffer_get_mp_int(&buffer, &pub->e);
  if (comment_return)
    *comment_return = buffer_get_string(&buffer, NULL);
  /* The encrypted private part is not parsed by this function. */

  buffer_free(&buffer);
  
  return 1;
}

/* Loads the private key from the file.  Returns 0 if an error is encountered
   (file does not exist or is not readable, or passphrase is bad).
   This initializes the private key. */

int load_private_key(const char *filename, const char *passphrase,
		     RSAPrivateKey *prv, char **comment_return)
{
  int f, i, check1, check2, cipher_type;
  unsigned long len;
  Buffer buffer, decrypted;
  char *cp;
  CipherContext cipher;

  /* Read the file into the buffer. */
  f = open(filename, O_RDONLY);
  if (f < 0)
    return 0;

  len = lseek(f, (off_t)0L, 2);
  lseek(f, (off_t)0L, 0);
  
  buffer_init(&buffer);
  buffer_append_space(&buffer, &cp, len);

  if (read(f, cp, len) != len)
    {
      debug("Read from key file %.200s failed: %.100s", filename,
	    strerror(errno));
      buffer_free(&buffer);
      close(f);
      return 0;
    }
  close(f);

  /* Check that it is at least big enought to contain the ID string. */
  if (len < strlen(AUTHFILE_ID_STRING) + 1)
    {
      debug("Bad key file %.200s.", filename);
      buffer_free(&buffer);
      return 0;
    }

  /* Make sure it begins with the id string.  Consume the id string from
     the buffer. */
  for (i = 0; i < (unsigned int)strlen(AUTHFILE_ID_STRING) + 1; i++)
    if (buffer_get_char(&buffer) != (unsigned char)AUTHFILE_ID_STRING[i])
      {
	debug("Bad key file %.200s.", filename);
	buffer_free(&buffer);
	return 0;
      }

  /* Read cipher type. */
  cipher_type = buffer_get_char(&buffer);
  (void)buffer_get_int(&buffer);  /* Reserved data. */

  /* Read the public key from the buffer. */
  prv->bits = buffer_get_int(&buffer);
  mpz_init(&prv->n);
  buffer_get_mp_int(&buffer, &prv->n);
  mpz_init(&prv->e);
  buffer_get_mp_int(&buffer, &prv->e);
  if (comment_return)
    *comment_return = buffer_get_string(&buffer, NULL);
  else
    xfree(buffer_get_string(&buffer, NULL));

  /* Check that it is a supported cipher. */
  if ((cipher_mask() & (1 << cipher_type)) == 0)
    {
      debug("Unsupported cipher %.100s used in key file %.200s.",
	    cipher_name(cipher_type), filename);
      buffer_free(&buffer);
      goto fail;
    }

  /* Initialize space for decrypted data. */
  buffer_init(&decrypted);
  buffer_append_space(&decrypted, &cp, buffer_len(&buffer));
      
  /* Rest of the buffer is encrypted.  Decrypt it using the passphrase. */
  cipher_set_key_string(&cipher, cipher_type, passphrase, 0);
  cipher_decrypt(&cipher, (unsigned char *)cp,
		 (unsigned char *)buffer_ptr(&buffer),
		 buffer_len(&buffer));

  buffer_free(&buffer);

  check1 = buffer_get_char(&decrypted);
  check2 = buffer_get_char(&decrypted);
  if (check1 != buffer_get_char(&decrypted) ||
      check2 != buffer_get_char(&decrypted))
    {
      if (strcmp(passphrase, "") != 0)
	debug("Bad passphrase supplied for key file %.200s.", filename);
      /* Bad passphrase. */
      buffer_free(&decrypted);
    fail:
      mpz_clear(&prv->n);
      mpz_clear(&prv->e);
      if (comment_return)
	xfree(*comment_return);
      return 0;
    }

  /* Read the rest of the private key. */
  mpz_init(&prv->d);
  buffer_get_mp_int(&decrypted, &prv->d);
  mpz_init(&prv->u);
  buffer_get_mp_int(&decrypted, &prv->u);
  mpz_init(&prv->p);
  buffer_get_mp_int(&decrypted, &prv->p);
  mpz_init(&prv->q);
  buffer_get_mp_int(&decrypted, &prv->q);
  
  buffer_free(&decrypted);

  return 1;
}
