/*	$OpenBSD: conf.c,v 1.4 1998/11/20 07:38:30 niklas Exp $	*/
/*	$EOM: conf.c,v 1.10 1998/11/20 07:19:21 niklas Exp $	*/

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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "conf.h"
#include "log.h"

/*
 * Radix-64 Encoding.
 */

const u_int8_t bin2asc[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const u_int8_t asc2bin[] =
{
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255,  62, 255, 255, 255,  63,
   52,  53,  54,  55,  56,  57,  58,  59,
   60,  61, 255, 255, 255, 255, 255, 255,
  255,   0,   1,   2,   3,   4,   5,   6,
    7,   8,   9,  10,  11,  12,  13,  14,
   15,  16,  17,  18,  19,  20,  21,  22,
   23,  24,  25, 255, 255, 255, 255, 255,
  255,  26,  27,  28,  29,  30,  31,  32,
   33,  34,  35,  36,  37,  38,  39,  40,
   41,  42,  43,  44,  45,  46,  47,  48,
   49,  50,  51, 255, 255, 255, 255, 255
};

struct conf_binding {
  LIST_ENTRY (conf_binding) link;
  char *section;
  char *tag;
  char *value;
};

char *conf_path = CONFIG_FILE;
LIST_HEAD (conf_bindings, conf_binding) conf_bindings;

static off_t conf_sz;
static char *conf_addr;

/*
 * Insert a tag-value combination from LINE (the equal sign is at POS)
 * into SECTION of our configuration database.
 * XXX Should really be a hash table implementation.
 */
static void
conf_set (char *section, char *line, int pos)
{
  struct conf_binding *node;
  int i;

  node = malloc (sizeof *node);
  if (!node)
    log_fatal ("conf_set: out of memory");
  node->section = section;
  node->tag = line;
  for (i = 0; line[i] && i < pos; i++)
    ;
  line[i] = '\0';
  if (conf_get_str (section, line))
    {
      log_print ("conf_set: duplicate tag [%s]:%s, ignoring...\n", section,
		 line);
      return;
    }
  node->value = line + pos + 1 + strspn (line + pos + 1, " \t");
  LIST_INSERT_HEAD (&conf_bindings, node, link);
  log_debug (LOG_MISC, 70, "(%s,%s)->%s", node->section, node->tag,
	     node->value);
}

/*
 * Parse the line LINE of SZ bytes.  Skip Comments, recognize section
 * headers and feed tag-value pairs into our configuration database.
 */
static void
conf_parse_line (char *line, size_t sz)
{
  char *cp = line;
  int i;
  static char *section = 0;
  static int ln = 0;

  ln++;
  for (i = 0; line[i]; i++)
    if (!isprint (*cp))
      {
	log_print ("conf_parse_line: %d:"
		   "ignoring line %d with non-printable characters", ln);
	return;
      }

  /* Lines starting with '#' or ';' are comments.  */
  if (*line == '#' || *line == ';')
    return;

  /* '[section]' parsing...  */
  if (*line == '[')
    {
      for (i = 1; i < sz; i++)
	if (line[i] == ']')
	  break;
      if (i == sz)
	{
	  log_print ("conf_parse_line: %d:"
		     "non-matched ']', ignoring until next section", ln);
	  section = 0;
	  return;
	}
      section = malloc (i);
      strncpy (section, line + 1, i - 1);
      section[i - 1] = '\0';
      return;
    }

  /* Deal with assignments.  */
  for (i = 0; i < sz; i++)
    if (cp[i] == '=')
      {
	/* If no section, we are ignoring the lines.  */
	if (!section)
	  {
	    log_print ("conf_parse_line: %d: ignoring line due to no section",
		       ln);
	    return;
	  }
	conf_set (section, line, i);
	return;
      }

  /* Other non-empty lines are wierd.  */
  i = strspn (line, " \t");
  if (line[i])
    log_print ("conf_parse_line: %d: syntax error", ln);

  return;
}

/* Parse the mapped configuration file.  */
static void
conf_parse (void)
{
  char *cp = conf_addr;
  char *conf_end = conf_addr + conf_sz;
  char *line;

  line = cp;
  while (cp < conf_end)
    {
      if (*cp == '\n')
	{
	  /* Check for escaped newlines.  */
	  if (cp > conf_addr && *(cp - 1) == '\\')
	    *(cp - 1) = *cp = ' ';
	  else
	    {
	      *cp = '\0';
	      conf_parse_line (line, cp - line);
	      line = cp + 1;
	    }
	}
      cp++;
    }
  if (cp != line)
    log_print ("conf_parse: last line non-terminated, ignored.");
}

/* Open the config file and map it into our address space, then parse it.  */
void
conf_init (void)
{
  int fd;
  struct stat st;

  /*
   * Start by freeing potential existing configuration.
   *
   * XXX One could envision doing this late, surviving failures with just
   * a warning log message that the new configuration did not get read
   * and that the former one persists.
   */
  if (conf_addr)
    {
      while (LIST_FIRST (&conf_bindings))
	LIST_REMOVE (LIST_FIRST (&conf_bindings), link);
      free (conf_addr);
    }

  fd = open (conf_path, O_RDONLY);
  if (fd == -1)
    log_fatal ("open (\"%s\", O_RDONLY)", conf_path);
  if (fstat (fd, &st) == -1)
    log_fatal ("fstat (%d, &st)", fd);
  conf_sz = st.st_size;
  conf_addr = malloc (conf_sz);
  if (!conf_addr)
    log_fatal ("malloc (%d)", conf_sz);
  /* XXX I assume short reads won't happen here.  */
  if (read (fd, conf_addr, conf_sz) != conf_sz)
    log_fatal ("read (%d, %p, %d)", fd, conf_addr, conf_sz);
  close (fd);

  LIST_INIT (&conf_bindings);
  conf_parse ();
}

/* Return the numeric value denoted by TAG in section SECTION.  */
int
conf_get_num (char *section, char *tag)
{
  char *value = conf_get_str (section, tag);

  if (value)
      return atoi (value);
  return 0;
}

/* Validate X according to the range denoted by TAG in section SECTION.  */
int
conf_match_num (char *section, char *tag, int x)
{
  char *value = conf_get_str (section, tag);
  int val, min, max, n;

  if (!value)
    return 0;
  n = sscanf (value, "%d,%d:%d", &val, &min, &max);
  switch (n)
    {
    case 1:
      log_debug (LOG_MISC, 90, "conf_match_num: %s:%s %d==%d?", section, tag,
		 val, x);
      return x == val;
    case 3:
      log_debug (LOG_MISC, 90, "conf_match_num: %s:%s %d<=%d<=%d?", section,
		 tag, min, x, max);
      return min <= x && max >= x;
    default:
      log_error ("conf_match_num: section %s tag %s: invalid number spec %s",
		 section, tag, value);
    }
  return 0;
}

/* Return the string value denoted by TAG in section SECTION.  */
char *
conf_get_str (char *section, char *tag)
{
  struct conf_binding *cb;

  for (cb = LIST_FIRST (&conf_bindings); cb; cb = LIST_NEXT (cb, link))
    if (strcasecmp (section, cb->section) == 0
	&& strcasecmp (tag, cb->tag) == 0)
      {
	log_debug (LOG_MISC, 60, "conf_get_str: (%s, %s) -> %s", section,
		   tag, cb->value);
	return cb->value;
      }
  log_debug (LOG_MISC, 60,
	     "conf_get_str: configuration value not found (%s, %s)", section,
	     tag);
  return 0;
}

struct conf_list *
conf_get_list (char *section, char *tag)
{
  char *liststr = 0, *p, *field;
  struct conf_list *list = 0;
  struct conf_list_node *node;

  list = malloc (sizeof *list);
  if (!list)
    goto cleanup;
  TAILQ_INIT (&list->fields);
  list->cnt = 0;
  liststr = conf_get_str (section, tag);
  if (!liststr)
    goto cleanup;
  liststr = strdup (liststr);
  if (!liststr)
    goto cleanup;
  p = liststr;
  while ((field = strsep (&p, ", \t")) != NULL)
    {
      if (*field == '\0')
	{
	  log_print ("conf_get_list: empty field, ignoring...");
	  continue;
	}
      list->cnt++;
      node = malloc (sizeof *node);
      if (!node)
	goto cleanup;
      node->field = field;
      TAILQ_INSERT_TAIL (&list->fields, node, link);
    }
  return list;

 cleanup:
  if (list)
    conf_free_list (list);
  if (liststr)
    free (liststr);
  return 0;
}

struct conf_list *
conf_get_tag_list (char *section)
{
  struct conf_list *list = 0;
  struct conf_list_node *node;
  struct conf_binding *cb;

  list = malloc (sizeof *list);
  if (!list)
    goto cleanup;
  TAILQ_INIT (&list->fields);
  list->cnt = 0;
  for (cb = LIST_FIRST (&conf_bindings); cb; cb = LIST_NEXT (cb, link))
    if (strcasecmp (section, cb->section) == 0)
      {
	list->cnt++;
	node = malloc (sizeof *node);
	if (!node)
	  goto cleanup;
	node->field = cb->tag;
	TAILQ_INSERT_TAIL (&list->fields, node, link);
      }
  return list;

 cleanup:
  if (list)
    conf_free_list (list);
  return 0;
}

/* Decode a PEM encoded buffer.  */ 
int
conf_decode_base64(u_int8_t *out, u_int32_t *len, u_char *buf)
{
  u_int32_t c = 0;
  u_int8_t c1, c2, c3, c4;

  while (*buf)
    {
      if (*buf > 127 || (c1 = asc2bin[*buf]) == 255)
	return 0;
      buf++;

      if (*buf > 127 || (c2 = asc2bin[*buf]) == 255)
	return 0;
      buf++;

      if (*buf == '=')
	{
	  c3 = c4 = 0;
	  c++;

	  /* Check last four bit */
	  if (c2 & 0xF)
	    return 0;

	  if (!strcmp (buf, "=="))
	    buf++;
	  else
	    return 0;
	}
      else if (*buf > 127 || (c3 = asc2bin[*buf]) == 255)
	return 0;
      else
	{
	  if (*++buf == '=')
	    {
	      c4 = 0;
	      c += 2;
	      
	      /* Check last two bit */
	      if (c3 & 3)
		return 0;

	      if (strcmp(buf, "="))
		return 0;

	    } 
	  else if (*buf > 127 || (c4 = asc2bin[*buf]) == 255)
	      return 0;
	  else 
	      c += 3;
	}

      buf++;
      *out++ = (c1 << 2) | (c2 >> 4);
      *out++ = (c2 << 4) | (c3 >> 2);
      *out++ = (c3 << 6) | c4;
    }

  *len = c;
  return 1;

}

/* Read a line from a stream to the buffer.  */
int
conf_get_line (FILE *stream, char *buf, u_int32_t len)
{
  char c;

  while (len-- > 1)
    {
      c = fgetc (stream);
      if (c == '\n')
	{
	  *buf = 0;
	  return 1;
	}
      else if (c == EOF)
	break;

      *buf++ = c;
    }

  *buf = 0;
  return 0;
}

void
conf_free_list (struct conf_list *list)
{
  while (TAILQ_FIRST (&list->fields))
    TAILQ_REMOVE (&list->fields, TAILQ_FIRST (&list->fields), link);
  free (list);
}
