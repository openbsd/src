/*	$OpenBSD: field.c,v 1.2 1998/11/15 00:43:53 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niklas Hallqvist.  All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "field.h"
#include "log.h"
#include "util.h"

static char *field_debug_raw (u_int8_t *, size_t, struct constant_map **);
static char *field_debug_num (u_int8_t *, size_t, struct constant_map **);
static char *field_debug_mask (u_int8_t *, size_t, struct constant_map **);
static char *field_debug_ign (u_int8_t *, size_t, struct constant_map **);
static char *field_debug_cst (u_int8_t *, size_t, struct constant_map **);

/* Contents must match the enum in struct field.  */
static char *(*decode_field[]) (u_int8_t *, size_t, struct constant_map **) = {
  field_debug_raw,
  field_debug_num,
  field_debug_mask,
  field_debug_ign,
  field_debug_cst
};

/*
 * Return a string showing the hexadecimal contents of the LEN-sized buffer
 * BUF.  MAPS should be zero and is only here because the API requires it.
 */
static char *
field_debug_raw (u_int8_t *buf, size_t len, struct constant_map **maps)
{
  char *retval, *p;

  if (len == 0)
    return 0;
  retval = malloc (3 + len * 2);
  if (!retval)
    return 0;
  strcpy (retval, "0x");
  p = retval + 2;
  while (len--)
    {
      sprintf (p, "%02x", *buf++);
      p += 2;
    }
  return retval;
}

/*
 * Convert the unsigned LEN-sized number at BUF of network byteorder to a
 * 32-bit unsigned integer of host byteorder pointed to by VAL.
 */
static int
extract_val (u_int8_t *buf, size_t len, u_int32_t *val)
{
  switch (len)
    {
    case 1:
      *val = *buf;
      break;
    case 2:
      *val = decode_16 (buf);
      break;
    case 4:
      *val = decode_32 (buf);
      break;
    default:
      return -1;
    }
  return 0;
}

/*
 * Return a textual representation of the unsigned number pointed to by BUF
 * which is LEN octets long.  MAPS should be zero and is only here because
 * the API requires it.
 */
static char *
field_debug_num (u_int8_t *buf, size_t len, struct constant_map **maps)
{
  char *retval;
  u_int32_t val;

  if (extract_val (buf, len, &val))
    return 0;
  asprintf (&retval, "%u", val);
  return retval;
}

/*
 * Return the symbolic names of the flags pointed to by BUF which is LEN
 * octets long, using the constant maps MAPS.
 */
static char *
field_debug_mask (u_int8_t *buf, size_t len, struct constant_map **maps)
{
  u_int32_t val;
  u_int32_t bit;
  char *retval, *new_buf, *name;
  size_t buf_sz;

  if (extract_val (buf, len, &val))
    return 0;

  /* Size for brackets, two spaces and a NUL terminator.  */
  buf_sz = 5;
  retval = malloc (buf_sz);
  if (!retval)
    return 0;

  strcpy (retval, "[ ");
  for (bit = 1; bit; bit <<= 1)
    {
      if (val & bit)
	{
	  name = constant_name_maps (maps, bit);
	  buf_sz += strlen (name);
	  new_buf = realloc (retval, buf_sz);
	  if (!new_buf)
	    {
	      free (retval);
	      return 0;
	    }
	  retval = new_buf;
	  strcat (retval, name);
	  strcat (retval, " ");
	}
    }
  strcat (retval, "]");
  return retval;
}

/*
 * Just a dummy needed to skip the unused LEN sized space at BUF.  MAPS
 * should be zero and is only here because the API requires it.
 */
static char *
field_debug_ign (u_int8_t *buf, size_t len, struct constant_map **maps)
{
  return 0;
}

/*
 * Return the symbolic name of a constant pointed to by BUF which is LEN
 * octets long, using the constant maps MAPS.
 */
static char *
field_debug_cst (u_int8_t *buf, size_t len, struct constant_map **maps)
{
  u_int32_t val;

  if (extract_val (buf, len, &val))
    return 0;

  return strdup (constant_name_maps (maps, val));
}

/* Pretty-print a field from BUF as described by F.  */
void
field_dump_field (struct field *f, u_int8_t *buf)
{
  char *value;

  value = decode_field[(int)f->type] (buf + f->offset, f->len, f->maps);
  if (value)
    {
      log_debug (LOG_MESSAGE, 70, "%s: %s", f->name, value);
      free (value);
    }
}

/* Pretty-print all the fields of BUF as described in FIELDS.  */
void
field_dump_payload (struct field *fields, u_int8_t *buf)
{
  struct field *field;

  for (field = fields; field->name; field++)
    field_dump_field (field, buf);
}

/* Return the numeric value of the field F of BUF.  */
u_int32_t
field_get_num (struct field *f, u_int8_t *buf)
{
  u_int32_t val;

  if (extract_val(buf + f->offset, f->len, &val))
    return 0;
  return val;
}

/* Stash the number VAL into BUF's field F.  */
void
field_set_num (struct field *f, u_int8_t *buf, u_int32_t val)
{
  switch (f->len)
    {
    case 1:
      buf[f->offset] = val;
      break;
    case 2:
      encode_16 (buf + f->offset, val);
      break;
    case 4:
      encode_32 (buf + f->offset, val);
      break;
    }
}

/* Stash BUF's raw field F into VAL.  */
void
field_get_raw (struct field *f, u_int8_t *buf, u_int8_t *val)
{
  memcpy (val, buf + f->offset, f->len);
}

/* Stash the buffer VAL into BUF's field F.  */
void
field_set_raw (struct field *f, u_int8_t *buf, u_int8_t *val)
{
  memcpy (buf + f->offset, val, f->len);
}
