/*	$OpenBSD: asn.c,v 1.7 1999/04/19 20:56:57 niklas Exp $	*/
/*	$EOM: asn.c,v 1.25 1999/04/18 15:17:22 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 * Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
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
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>
#include <unistd.h>

#include "sysdep.h"

#include "log.h"
#include "asn.h"
#include "gmp_util.h"

struct asn_handler table[] = {
  {
    TAG_INTEGER, asn_free_integer, asn_get_encoded_len_integer,
    asn_decode_integer, asn_encode_integer
  },
  {
    TAG_OBJECTID, asn_free_objectid, asn_get_encoded_len_objectid,
    asn_decode_objectid, asn_encode_objectid
  },
  {
    TAG_SEQUENCE, asn_free_sequence, asn_get_encoded_len_sequence,
    asn_decode_sequence, asn_encode_sequence
  },
  {
    TAG_SET, asn_free_sequence, asn_get_encoded_len_sequence,
    asn_decode_sequence, asn_encode_sequence
  },
  {
    TAG_UTCTIME, asn_free_string, asn_get_encoded_len_string,
    asn_decode_string, asn_encode_string
  },
  {
    TAG_BITSTRING, asn_free_string, asn_get_encoded_len_string,
    asn_decode_string, asn_encode_string
  },
  {
    TAG_OCTETSTRING, asn_free_string, asn_get_encoded_len_string,
    asn_decode_string, asn_encode_string
  },
  {
    TAG_BOOL, asn_free_string, asn_get_encoded_len_string, asn_decode_string,
    asn_encode_string
  },
  {
    TAG_PRINTSTRING, asn_free_string, asn_get_encoded_len_string,
    asn_decode_string, asn_encode_string
  },
  {
    TAG_RAW, asn_free_raw, asn_get_encoded_len_raw, asn_decode_raw,
    asn_encode_raw
  },
  {
    TAG_NULL, asn_free_null, asn_get_encoded_len_null, asn_decode_null,
    asn_encode_null
  },
  { TAG_ANY, asn_free_null, 0, asn_decode_any, 0 },
  { TAG_STOP, 0, 0, 0, 0 }
};

int
asn_get_from_file (char *name, u_int8_t **asn, u_int32_t *asnlen)
{
  int fd, res = 0;
  struct stat st;

  if (stat (name, &st) == -1)
    {
      log_error ("asn_get_from_file: failed to state %s", name);
      return 0;
    }

  *asnlen = st.st_size;

  if ((fd = open (name, O_RDONLY)) == -1)
    {
      log_error ("asn_get_from_file: failed to open %s", name);
      return 0;
    }

  *asn = malloc (st.st_size);
  if (!*asn)
    {
      log_print ("asn_get_from_file: malloc (%d) failed", st.st_size);
      res = 0;
      goto done;
    }

  if (read (fd, *asn, st.st_size) != st.st_size
      || asn_get_len (*asn) != *asnlen)
    {
      log_print ("x509_asn_obtain: asn file ended early");
      free (*asn);
      res = 0;
      goto done;
    }

  res = 1;

 done:
  close (fd);

  return res;
}

struct norm_type *
asn_template_clone (struct norm_type *obj, int constructed)
{
  struct norm_type *p;
  u_int32_t i;

  if (!constructed)
    {
      p = malloc (sizeof (struct norm_type));
      if (!p)
	{
	  log_error ("asn_template_clone: malloc (%d) failed",
		     sizeof (struct norm_type));
	  return 0;
	}

      memcpy (p, obj, sizeof (struct norm_type));

      obj = p;
    }

  if (obj->type != TAG_SEQUENCE && obj->type != TAG_SET)
    {
      obj->len = 0;
      obj->data = 0;
    }
  else if (obj->type == TAG_SEQUENCE || obj->type == TAG_SET)
    {
      p = obj;
      obj = obj->data;
      i = 0;
      while (obj[i++].type != TAG_STOP);

      p->data = malloc (i * sizeof (struct norm_type));
      if (!p->data)
	{
	  log_error ("asn_template_clone: malloc (%d) failed",
		     i * sizeof (struct norm_type));
	  return 0;
	}
      memcpy (p->data, obj, i * sizeof (struct norm_type));
      obj = p->data;

      i = 0;
      while (obj[i].type != TAG_STOP)
	{
	  obj[i].len = 0;
	  if (!asn_template_clone (&obj[i], 1))
	    return 0;

	  i++;
	}
    }

  return obj;
}

/* Associates a human readable name to an OBJECT IDENTIFIER.  */
char *
asn_parse_objectid (struct asn_objectid *table, char *id)
{
  u_int32_t len = 0;
  char *p = 0;
  static char buf[LINE_MAX];

  if (!id)
    return 0;

  while (table->name)
    {
      if (!strcmp (table->objectid, id))
	return table->name;
      if (!strncmp (table->objectid, id, strlen (table->objectid))
	  && strlen (table->objectid) > len)
	{
	  len = strlen (table->objectid);
	  p = table->name;
	}

      table++;
    }

  if (len == 0)
    return 0;

  strncpy (buf, p, sizeof (buf) - 1);
  buf[sizeof (buf) - 1] = 0;
  strncat (buf + strlen (buf), id + len, sizeof (buf) - 1 - strlen (buf));
  buf[sizeof (buf) - 1] = 0;

  return buf;
}

/* Retrieves the pointer to a data type referenced by the path name.  */
struct norm_type *
asn_decompose (char *path, struct norm_type *obj)
{
  char *p, *p2, *tmp;
  int counter;

  if (!strcasecmp (path, obj->name))
      return obj->data;

  p = path = strdup (path);
  p2 = strsep (&p, ".");

  if (strcasecmp (p2, obj->name) || !p)
    goto fail;

  while (p)
    {
      obj = obj->data;
      if (!obj)
	break;

      p2 = strsep (&p, ".");

      /*
       * For SEQUENCE OF or SET OF, we want to be able to say
       * AttributeValueAssertion[1] for the 2nd value.
       */
      tmp = strchr (p2, '[');
      if (tmp)
	{
	  counter = atoi (tmp+1);
	  *tmp = 0;
	}
      else
	counter = 0;

      /* Find the tag.  */
      while (obj->type != TAG_STOP)
	{
	  if (!strcasecmp (p2, obj->name) && counter-- == 0)
	    break;
	  obj++;
	}

      if (obj->type == TAG_STOP)
	goto fail;

      if (!p)
	goto done;

      if (obj->type != TAG_SEQUENCE && obj->type != TAG_SET)
	    goto fail;
    }

 done:
  free (path);
  return obj;

 fail:
  free (path);
  return 0;
}

/* Gets an entry from the ASN.1 tag switch table.  */
struct asn_handler *
asn_get (enum asn_tags type)
{
  struct asn_handler *h = table;

  while (h->type != TAG_STOP)
    if (h->type  == type)
      return h;
    else
      h++;

  return 0;
}

/*
 * For the long form of BER encoding we need to know in how many
 * octets the length can be encoded.
 */
u_int32_t
asn_sizeinoctets (u_int32_t len)
{
  u_int32_t log = 0;

  while (len)
    {
      log++;
      len >>= 8;
    }

  return log;
}

u_int8_t *
asn_format_header (struct norm_type *obj, u_int8_t *asn, u_int8_t **data)
{
  u_int8_t *buf = 0, *erg;
  u_int8_t type;
  u_int16_t len_off, len;
  struct asn_handler *h;

  h = asn_get (obj->type);
  if (!h)
    return 0;

  if (asn)
    buf = asn;

  /* We only do low tags at the moment.  */
  len_off = 1;

  len = h->get_encoded_len (obj, &type);

  if (!buf)
    {
      buf = malloc (len);
      if (!buf)
	{
	  log_error ("asn_format_header: malloc (%d) failed", len);
	  return 0;
	}
    }

  if (type != ASN_LONG_FORM)
    {
      len -= len_off + 1;
      buf[len_off] = len;

      *data = buf + len_off + 1;
    }
  else
    {
      u_int16_t tmp;
      int octets = asn_sizeinoctets (len);

      len -= len_off + 1 + octets;
      *data = buf + len_off + 1 + octets;

      buf[len_off] = octets | ASN_LONG_FORM;

      tmp = len;
      while (--octets >= 0)
	{
	  buf[len_off + 1 + octets] = tmp;
	  tmp >>= 8;
	}
    }

  if (ISEXPLICIT (obj))
    {
      /* Explicit tagging adds an outer layer.  */
      struct norm_type tmp = {obj->type, obj->class&0x3, 0, 0, obj->data};

      /* XXX Force the class to be CONTEXT.  */
      buf[0] = GET_EXP (obj) | (((enum asn_classes)CONTEXT & 0x3) << 6)
	| ASN_CONSTRUCTED;
      erg = asn_format_header (&tmp, *data, data);

      if (erg && (obj->type == TAG_SEQUENCE || obj->type == TAG_SET))
	erg[0] |= ASN_CONSTRUCTED;
    }
  else
    /* XXX Low tags only.  */
    buf[0] = obj->type | (obj->class << 6);

  return buf;
}

u_int32_t
asn_get_encoded_len (struct norm_type *obj, u_int32_t len, u_int8_t *type)
{
  u_int32_t len_off = 1;

  if (len <= 127)
    {
      /* Short form */
      len = len + 1 + len_off;
      if (type)
	*type = 0;
    }
  else
    {
      /* Long Form */
      len = len + asn_sizeinoctets (len) + 1 + len_off;
      if (type)
	*type = ASN_LONG_FORM;
    }

  if (obj && ISEXPLICIT (obj))
    len = asn_get_encoded_len (0, len, 0);

  return len;
}

/* Tries to decode an ANY tag, if we cant handle it we just raw encode it.  */
u_int8_t *
asn_decode_any (u_int8_t *asn, u_int32_t asnlen, struct norm_type *obj)
{
  struct asn_handler *h;
  enum asn_tags type;

  type = TAG_TYPE (asn);
  if (type == TAG_SEQUENCE || type == TAG_SET)
    type = TAG_RAW;

  h = asn_get (type);
  if (!h)
    {
      type = TAG_RAW;
      h = asn_get (type);
    }

  obj->type = type;
  return h->decode (asn, asnlen, obj);
}

u_int32_t
asn_get_encoded_len_integer (struct norm_type *obj, u_int8_t *type)
{
  u_int16_t len_off;
  u_int32_t len = obj->len;
  u_int32_t tmp;
  mpz_t a;

  /* XXX We only do low tags at the moment.  */
  len_off = 1;

  obj->len = len = mpz_sizeinoctets ((mpz_ptr) obj->data);
  mpz_init_set (a, (mpz_ptr) obj->data);

  if (len > 1)
    mpz_fdiv_q_2exp (a, a, (len - 1) << 3);

  tmp = mpz_fdiv_r_ui (a, a, 256);
  mpz_clear (a);

  /*
   * We only need to encode positive integers, ASN.1 defines
   * negative integers to have the msb set, so if data[0] has
   * msb set we need to introduce a zero octet.
   */
  if (tmp & 0x80)
    len++;

  return asn_get_encoded_len (obj, len, type);
}

/*
 * Encode an integer value.
 * Input = obj, output = asn or return value.
 */
u_int8_t *
asn_encode_integer (struct norm_type *obj, u_int8_t *asn)
{
  u_int8_t *buf, *data;
  u_int32_t len;

  buf = asn_format_header (obj, asn, &data);

  if (!buf)
    return 0;

  len = mpz_sizeinoctets ((mpz_ptr) obj->data);
  mpz_getraw (data, (mpz_ptr) obj->data, len);

  /* XXX We only deal with unsigned integers at the moment.  */
  if (data[0] & 0x80)
    {
      memmove (data + 1, data, len);
      data[0] = 0;
    }

  return buf;
}

u_int8_t *
asn_decode_integer (u_int8_t *asn, u_int32_t asnlen, struct norm_type *obj)
{
  u_int8_t *data;
  u_int32_t len;
  mpz_ptr p;

  if (asnlen < asn_get_len (asn))
    {
      log_print ("asn_decode_integer: ASN.1 content is bigger than buffer");
      return 0;
    }

  len = asn_get_data_len (obj, &asn, &data);

  if (TAG_TYPE (asn) != TAG_INTEGER)
    {
      log_print ("asn_decode_integer: expected tag type INTEGER, got %d",
		 TAG_TYPE (asn));
      return 0;
    }

  p = malloc (sizeof *p);
  if (!p)
    {
      log_error ("asn_decode_integer: malloc (%d) failed", sizeof *p);
      return 0;
    }

  mpz_init (p);
  mpz_setraw (p, data, len);

  obj->len = len;
  obj->data = p;

  return data + len;
}

void
asn_free_integer (struct norm_type *obj)
{
  if (obj->data)
    {
      mpz_clear ((mpz_ptr) obj->data);
      free (obj->data);
    }
}


u_int32_t
asn_get_encoded_len_string (struct norm_type *obj, u_int8_t *type)
{
  return asn_get_encoded_len (obj, obj->len, type);
}

/*
 * Encode a String
 * Input = obj, output = asn or return value.
 */
u_int8_t *
asn_encode_string (struct norm_type *obj, u_int8_t *asn)
{
  u_int8_t *buf, *data;

  buf = asn_format_header (obj, asn, &data);

  if (!buf)
    return 0;

  memcpy (data, obj->data, obj->len);

  return buf;
}

u_int8_t *
asn_decode_string (u_int8_t *asn, u_int32_t asnlen, struct norm_type *obj)
{
  u_int8_t *data;
  u_int32_t len;

  obj->len = len = asn_get_data_len (obj, &asn, &data);

  if (TAG_TYPE (asn) != obj->type)
    {
      log_print ("asn_decode_string: expected tag type STRING(%d), got %d",
		 obj->type, TAG_TYPE (asn));
      return 0;
    }

  if (asnlen < asn_get_len (asn))
    {
      log_print ("asn_decode_string: ASN.1 content is bigger than buffer");
      return 0;
    }

  obj->data = malloc (obj->len + 1);
  if (!obj->data)
    {
      log_error ("asn_decode_string: malloc (%d) failed", obj->len + 1);
      return 0;
    }
  memcpy ((char *)obj->data, data, obj->len);

  /*
   * Encode a terminating '0', this is irrelevant for OCTET strings
   * but nice for printable strings which do not include the terminating
   * zero.
   */
  ((char *)obj->data)[obj->len] = 0;

  return data + len;
}

void
asn_free_string (struct norm_type *obj)
{
  if (obj->data)
    free (obj->data);
}


u_int32_t
asn_get_encoded_len_objectid (struct norm_type *obj, u_int8_t *type)
{
  u_int16_t len_off;
  u_int32_t len;
  u_int32_t tmp;
  char *buf, *buf2;

  /* XXX We only do low tags at the moment.  */
  len_off = 1;

  /* The first two numbers are encoded together.  */
  buf = obj->data;
  tmp = strtol (buf, &buf2, 10);
  buf = buf2;
  tmp = strtol (buf, &buf2, 10);
  buf = buf2;

  len = 1;
  while (*buf)
    {
      tmp = strtol (buf, &buf2, 10);
      if (buf == buf2)
	break;

      buf = buf2;
      do {
	tmp >>= 7;
	len++;
      } while (tmp);
    }

  /* The first two IDs are encoded as one octet.  */
  obj->len = len - 1;

  return asn_get_encoded_len (obj, len, type);
}

/*
 * Encode an Object Identifier
 * Input = obj, output = asn or return value.
 */
u_int8_t *
asn_encode_objectid (struct norm_type *obj, u_int8_t *asn)
{
  u_int8_t *buf, *data;
  char *enc, *enc2;
  u_int32_t tmp, tmp2;
  int flag = 0;

  buf = asn_format_header (obj, asn, &data);

  if (!buf)
    return 0;

  enc = obj->data;
  while (*enc)
    {
      /* First two IDs are encoded as one octet.  */
      if (flag == 0)
	{
	  tmp = strtol (enc, &enc2, 10);
	  if (enc == enc2)
	    return 0;
	  enc = enc2;
	  tmp2 = strtol (enc, &enc2, 10) + 40 * tmp;
	  flag = 1;
	}
      else
	  tmp2 = strtol (enc, &enc2, 10);

      if (enc == enc2)
	break;

      /* Reverse the digits to base-128.  */
      tmp = 0;
      do {
	tmp <<= 7;
	tmp += tmp2 & 0x7f;
	tmp2 >>= 7;
      } while (tmp2);

      enc = enc2;
      do {
	/* If the next octet still belongs to the data set MSB.  */
	*data++ = (tmp & 0x7f) | ( tmp > 127 ? 0x80 : 0);
	tmp >>= 7;
      } while (tmp);
    }

  return buf;
}

u_int8_t *
asn_decode_objectid (u_int8_t *asn, u_int32_t asnlen, struct norm_type *obj)
{
  u_int8_t *data;
  u_int32_t len, c, tmp;
  int flag = 0;
  void *new_buf;

  len = asn_get_data_len (obj, &asn, &data);

  if (TAG_TYPE (asn) != TAG_OBJECTID)
    {
      log_print ("asn_decode_objectid: expected tag type OBJECTID, got %d",
		 TAG_TYPE (asn));
      return 0;
    }

  if (asnlen < asn_get_len (asn))
    {
      log_print ("asn_decode_objectid: ASN.1 content is bigger than buffer");
      return 0;
    }

  obj->data = 0;
  obj->len = 0;
  while (len > 0)
    {
      tmp = 0;
      do
	{
	  tmp <<= 7;
	  tmp += *data & 0x7f;
	}
      while (len-- > 0 && (*data++ & 0x80));

      if (flag == 0)
	  c = snprintf (0, 0, "%d %d ", tmp / 40, tmp % 40) + 1;
      else
	  c = snprintf (0, 0, "%d ", tmp) + 1;

      new_buf = realloc (obj->data, obj->len + c);
      if (!new_buf)
	{
	  log_error ("asn_decode_objectid: realloc (%p, %d) failed", obj->data,
		     obj->len + c);
	  free (obj->data);
	  obj->data = 0;
	  return 0;
	}
      obj->data = new_buf;

      if (flag == 0)
	{
	  sprintf (obj->data + obj->len, "%d %d ", tmp/40, tmp % 40);
	  flag = 1;
	}
      else
	sprintf (obj->data + obj->len, "%d ", tmp);
	
      obj->len = strlen (obj->data);
    }

  if (obj->data)
    ((char *)obj->data)[obj->len - 1] = 0;

  return data;
}

void
asn_free_objectid (struct norm_type *obj)
{
  if (obj->data)
    free (obj->data);
}


u_int32_t
asn_get_encoded_len_raw (struct norm_type *obj, u_int8_t *type)
{
  if (type)
    {
      if (obj->len > 127)
	*type = ASN_LONG_FORM;
      else
	*type = 0;
    }

  return obj->len;
}

u_int8_t *
asn_encode_raw (struct norm_type *obj, u_int8_t *asn)
{
  u_int8_t *buf = 0;

  if (obj->len == 0)
    return asn;

  if (asn)
    buf = asn;

  if (!buf)
    {
      buf = malloc (obj->len);
      if (!buf)
	{
	  log_error ("asn_encode_raw: malloc (%d) failed", obj->len);
	  return 0;
	}
    }

  memcpy (buf, obj->data, obj->len);

  return buf;
}

u_int8_t *
asn_decode_raw (u_int8_t *asn, u_int32_t asnlen, struct norm_type *obj)
{
  obj->len = asn_get_len (asn);
  if (asnlen < obj->len)
    {
      log_print ("asn_decode_raw: ASN.1 content is bigger than buffer");
      return 0;
    }

  obj->data = malloc (obj->len);
  if (!obj->data)
    {
      log_error ("asn_decode_raw: malloc (%d) failed", obj->len);
      return 0;
    }

  memcpy (obj->data, asn, obj->len);

  return asn + obj->len;
}

void
asn_free_raw (struct norm_type *obj)
{
  if (obj->data)
    free (obj->data);
}

u_int32_t
asn_get_encoded_len_null (struct norm_type *obj, u_int8_t *type)
{
  return asn_get_encoded_len (obj, 0, type);
}

u_int8_t *
asn_encode_null (struct norm_type *obj, u_int8_t *asn)
{
  u_int8_t *buf = 0;

  if (asn)
    buf = asn;

  if (!buf)
    {
      buf = malloc (2);
      if (!buf)
	{
	  log_error ("asn_encode_null: malloc (2) failed");
	  return 0;
	}
    }

  buf[0] = obj->type;
  buf[1] = 0;

  return buf;
}

u_int8_t *
asn_decode_null (u_int8_t *asn, u_int32_t asnlen, struct norm_type *obj)
{
  obj->data = 0;
  obj->len = 0;

  return asn + asn_get_len (asn);
}

void
asn_free_null (struct norm_type *obj)
{
  obj->data = 0;
}

void
asn_free (struct norm_type *obj)
{
  struct asn_handler *h = asn_get (obj->type);

  if (!h)
    log_print ("asn_free: unkown ASN.1 type %d", obj->type);
  else
    h->deallocate (obj);
}

/* Returns the whole length of the BER encoded ASN.1 object.  */
u_int32_t
asn_get_len (u_int8_t *asn)
{
  u_int32_t len;
  u_int8_t *data;
  struct norm_type tmp = { TAG_RAW, UNIVERSAL, 0, 0, 0 };

  len = asn_get_data_len (&tmp, &asn, &data);

  if (!asn)
    return 0;

  return (data - asn) + len;
}

/*
 * Returns the length of the ASN content, and a pointer to the content
 * data itself.
 * For TAG_NULL the data length is zero, so we have to return an error
 * in asn, asn will be NULL in case of error.
 */
u_int32_t
asn_get_data_len (struct norm_type *obj, u_int8_t **asn, u_int8_t **data)
{
  u_int32_t len;
  u_int16_t len_off = 1;
  static struct norm_type tmp = { TAG_RAW, UNIVERSAL, 0, 0, 0 };

  if (obj && ISEXPLICIT (obj))
    {
      if (TAG_TYPE (*asn) != GET_EXP (obj))
	{
	  log_print ("asn_get_data_len: explicit tagging was needed");
	  *asn = 0;
	  return 0;
	}

      asn_get_data_len (&tmp, asn, data);
      *asn = *data;
    }

  if ((*asn)[len_off] & ASN_LONG_FORM)
    {
      int i, octets = (*asn)[len_off] & 0x7f;

      /* XXX We only decode really small lengths.  */
      if (octets > sizeof len)
	{
	  log_print ("asn_get_data_len: long form length %d exceeds "
		     "allowed maximum", octets);
	  *asn = 0;
	  return 0;
	}

      for (len = 0, i = 0; i < octets; i++)
	{
	  len = (len << 8) | (*asn)[len_off + 1 + i];
	}

      if (data)
	*data = *asn + len_off + 1 + octets;
    }
  else
    {
      /* Short form */
      len = (*asn)[len_off];

      if (data)
	*data = *asn + len_off + 1;
    }

  return len;
}

void
asn_free_sequence (struct norm_type *obj)
{
  struct norm_type *in = obj->data;
  struct asn_handler *h;

  if (!in)
    return;

  while (in->type != TAG_STOP)
    {
      h = asn_get (in->type);
      if (!h)
	break;

      h->deallocate (in++);
    }

  free (obj->data);
}

u_int32_t
asn_get_encoded_len_sequence (struct norm_type *seq, u_int8_t *type)
{
  u_int32_t len, i;
  struct asn_handler *h;
  struct norm_type *obj = (struct norm_type *) seq->data;

  /* Get whole data length */
  for (len = 0, i = 0; obj[i].type != TAG_STOP; i++)
    {
      h = asn_get (obj[i].type);
      if (!h)
	{
	  log_print ("asn_encode_sequence: unknown type %d", obj[i].type);
	  break;
	}
      len += h->get_encoded_len (&obj[i], 0);
    }

  return asn_get_encoded_len (seq, len, type);
}

u_int8_t *
asn_encode_sequence (struct norm_type *seq, u_int8_t *asn)
{
  u_int32_t len;
  u_int8_t *erg, *data;
  struct norm_type *obj;
  struct asn_handler *h;
  int i;

  h = asn_get (seq->type);
  if (!h)
    return 0;

  obj = (struct norm_type *) seq->data;

  erg = asn_format_header (seq, asn, &data);
  if (!erg)
      return 0;

  for (i = 0, len = 0; obj[i].type != TAG_STOP; i++)
    {
      h = asn_get (obj[i].type);
      if (!h)
	{
	  log_print ("asn_encode_sequence: unknown ASN.1 tag %d", obj[i].type);
	  return 0;
	}

      /* A structure can be optional, indicated by data == 0.  */
      if (!h->encode (&obj[i], data + len) && obj->data)
	{
	  log_print ("asn_encode_sequence: encoding of %s failed",
		     obj[i].name);
	  return 0;
	}
      len += h->get_encoded_len (&obj[i], 0);
    }

  erg[0] |= ASN_CONSTRUCTED;

  return erg;
}

u_int8_t *
asn_decode_sequence (u_int8_t *asn, u_int32_t asnlen, struct norm_type *obj)
{
  u_int8_t *p, *data;
  u_int32_t len, flags, objects;
  struct asn_handler *h;
  void *new_buf;

  if (asnlen < asn_get_len (asn))
    {
      log_print ("asn_decode_sequence: ASN.1 content is bigger than buffer");
      return 0;
    }

  len = asn_get_data_len (obj, &asn, &data);

  /* XXX An empty sequence is that okay.  */
  if (len == 0)
    return data;

  if (TAG_TYPE (asn) != obj->type)
    {
      log_print ("asn_decode_sequence: expected tag type SEQUENCE/SET, got %d",
		 TAG_TYPE (asn));
      return 0;
    }

  /* Handle dynamic sized sets and sequences.  */
  flags = obj->flags;

  if (flags & ASN_FLAG_ZEROORMORE)
    {
      struct norm_type stop_tag = { TAG_STOP };
      struct norm_type *tmp;

      /* Zero occurences */
      if (len == 0)
	{
	  asn_free (obj);
	  obj->data = 0;
	  return data;
	}

      /* Count number of objects */
      p = data;
      objects = 0;
      while (p < data + len)
	{
	  objects++;
	  p += asn_get_len (p);
	}
      if (p != data + len)
	{
	  log_print ("asn_decode_sequence: SEQ/SET OF too many elements");
	  return 0;
	}

      /*
       * Create new templates for dynamically added objects,
       * the ASN.1 tags SEQUENCE OF and SET OF, specify an unknown
       * number of elements.
       */
      new_buf = realloc (obj->data,
			 (objects + 1) * sizeof (struct norm_type));
      if (!new_buf)
	{
	  log_error ("asn_decode_sequence: realloc (%p, %d) failed", obj->data,
		     (objects + 1) * sizeof (struct norm_type));
	  asn_free (obj);
	  obj->data = 0;
	  return 0;
	}
      obj->data = new_buf;

      tmp = obj->data;

      /* Copy TAG_STOP */
      memcpy (tmp + objects, &stop_tag, sizeof (struct norm_type));
      while (objects-- > 1)
	{
	  memcpy (tmp + objects, tmp, sizeof (struct norm_type));
	  if (!asn_template_clone (tmp + objects, 1))
	    return 0;
	}
    }

  obj = (struct norm_type *) obj->data;

  p = data;
  while (p < data + len)
    {
      if (obj->type == TAG_STOP)
	break;
      h = asn_get (obj->type);
      if (!h)
	{
	  log_print ("asn_decode_sequence: unknown ASN.1 tag %d", obj->type);
	  return 0;
	}

      p = h->decode (p, (data - p) + len, obj++);
      if (!p)
	break;
    }

  if (p < data + len)
      log_print ("asn_decode_sequence: ASN tag was not decoded completely");

  if (!p)
    return 0;

  return data + len;
}
