/*	$Id: x509test.c,v 1.1.1.1 1998/11/15 00:03:50 niklas Exp $	*/

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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <gmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "conf.h"
#include "asn.h"
#include "asn_useful.h"
#include "pkcs.h"
#include "x509.h"
#include "log.h"

u_int32_t file_sz;

#define LINECOL(x,y)   (x) = strsep (&(y), "\n\r"); \
  (x) = strchr ((x), ':') + 1; \
  while (isspace((x)[0])) (x)++; \


u_int8_t *
open_file (char *name)
{
  int fd;
  struct stat st;
  u_int8_t *addr;

  if (stat (name, &st) == -1)
    log_fatal ("stat (\"%s\", &st)", name);
  file_sz = st.st_size;
  fd = open (name, O_RDONLY);
  if (fd == -1)
    log_fatal ("open (\"%s\", O_RDONLY)", name);
  addr = mmap (0, file_sz, PROT_READ | PROT_WRITE, MAP_FILE | MAP_PRIVATE,
		    fd, 0);
  if (!addr)
    log_fatal ("mmap (0, %d, PROT_READ | PROT_WRITE, MAP_FILE | MAP_PRIVATE,"
	       "%d, 0)", file_sz, fd);
  close (fd);

  return addr;
}

int
main (void)
{
  struct rsa_private_key priv;
  struct x509_certificate cert;
  FILE *fd;
  char *p, *p2;
  u_int8_t *addr, *asn;
  u_int32_t asnlen, len;

  addr = open_file ("isakmpd_key");
  if (!pkcs_private_key_from_asn (&priv, addr, asn_get_len (addr)))
    {
      munmap (addr, file_sz);
      exit (1);
    }
  munmap (addr, file_sz);

  addr = open_file ("isakmpd_key.pub");
  if (!pkcs_public_key_from_asn (&cert.key, addr, asn_get_len (addr)))
    {
      munmap (addr, file_sz);
      exit (1);
    }
  munmap (addr, file_sz);

  cert.signaturetype = strdup (ASN_ID_MD5WITHRSAENC);
  cert.issuer1.type = strdup (ASN_ID_COUNTRY_NAME);
  cert.issuer2.type = strdup (ASN_ID_ORGANIZATION_NAME);
  cert.subject1.type = strdup (ASN_ID_COUNTRY_NAME);
  cert.subject2.type = strdup (ASN_ID_ORGANIZATION_NAME);

  addr = open_file ("certificate.txt");
  p = addr;

  LINECOL (p2, p); cert.version = atoi (p2);
  LINECOL (p2, p); cert.serialnumber = atoi (p2);
  LINECOL (p2, p); cert.issuer1.val = strdup (p2);
  LINECOL (p2, p); cert.issuer2.val = strdup (p2);
  LINECOL (p2, p); cert.subject1.val = strdup (p2);
  LINECOL (p2, p); cert.subject2.val = strdup (p2);
  LINECOL (p2, p); cert.start = strdup (p2);
  LINECOL (p2, p); cert.end = strdup (p2);
  munmap (addr, file_sz);

  /* XXX - just put any IP number in there - XXX */
  cert.extension.type = strdup (ASN_ID_SUBJECT_ALT_NAME);
  cert.extension.val = p = malloc (8);
  /* XXX - this could also be encoded as norm_type, but time is lacking */
  p[0] = 0x30; p[1] = 0x06; p[2] = 0x87; p[3] = 0x04;
  memset (p + 4, 0, 4);

  printf ("Encoding Certificiate: ");
  if (!x509_encode_certificate(&cert, &asn, &asnlen))
    printf ("FAILED ");
  else
    printf ("OKAY ");
  printf ("\n");

  printf ("Creating Signature: ");
  if (!x509_create_signed (asn, asnlen, &priv, &addr, &len))
    printf ("FAILED ");
  else
    printf ("OKAY ");
  printf ("\n");

  printf ("Validate SIGNED: ");
  if (!x509_validate_signed (addr, len, &cert.key, &asn, &asnlen))
    printf ("FAILED ");
  else
    printf ("OKAY ");
  printf ("\n");

  fd = fopen ("cert.asn", "w");
  fwrite (addr, len, 1, fd);
  fclose (fd);

  free (addr);

  return 1;
}
