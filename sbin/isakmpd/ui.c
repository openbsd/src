/*	$OpenBSD: ui.c,v 1.12 2000/02/25 17:23:42 niklas Exp $	*/
/*	$EOM: ui.c,v 1.37 2000/02/20 19:58:42 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "sysdep.h"

#include "conf.h"
#include "connection.h"
#include "doi.h"
#include "exchange.h"
#include "isakmp.h"
#include "log.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"
#include "ui.h"
#include "util.h"

#define BUF_SZ 256

char *ui_fifo = FIFO;
int ui_socket;

/* Create and open the FIFO used for user control.  */
void
ui_init ()
{
  struct stat st;

  /* -f- means control messages comes in via stdin.  */
  if (strcmp (ui_fifo, "-") == 0)
    ui_socket = 0;
  else
    {
      /* Don't overwrite a file, i.e '-f /etc/isakmpd/isakmpd.conf'.  */
      if (lstat (ui_fifo, &st) == 0)
	if ((st.st_mode & S_IFMT) == S_IFREG)
	  {
	    errno = EEXIST;
	    log_fatal ("could not create FIFO \"%s\"", ui_fifo);
	  }

      /* No need to know about errors.  */
      unlink (ui_fifo);
      if (mkfifo (ui_fifo, 0600) == -1)
	log_fatal ("mkfifo");

      /* XXX Is O_RDWR needed on some OSes?  Photurisd seems to imply that.  */
      ui_socket = open (ui_fifo, O_RDONLY | O_NONBLOCK, 0);
      if (ui_socket == -1)
	log_fatal (ui_fifo);
    }
}

/*
 * Setup a phase 2 connection.
 * XXX Maybe phase 1 works too, but teardown won't work then, fix?
 */
static void
ui_connect (char *cmd)
{
  char name[81];

  if (sscanf (cmd, "c %80s", name) != 1)
    {
      log_print ("ui_connect: command \"%s\" malformed", cmd);
      return;
    }
  connection_setup (name);
}

/* Tear down a phase 2 connection.  */
static void
ui_teardown (char *cmd)
{
  char name[81];
  struct sa *sa;

  if (sscanf (cmd, "t %80s", name) != 1)
    {
      log_print ("ui_teardown: command \"%s\" malformed", cmd);
      return;
    }
  connection_teardown (name);
  while ((sa = sa_lookup_by_name (name, 2)) != 0)
    sa_delete (sa, 1);
}

/*
 * Call the configuration API.
 * XXX Error handling!  How to do multi-line transactions?  Too short arbitrary
 * limit on the parameters?
 */
static void
ui_config (char *cmd)
{
  char subcmd[81], section[81], tag[81], value[81];
  int override, trans = 0;

  if (sscanf (cmd, "C %80s", subcmd) != 1)
    goto fail;

  trans = conf_begin ();
  if (strcasecmp (subcmd, "set") == 0)
    {
      if (sscanf (cmd, "C %*s [%80[^]]]:%80[^=]=%80s %d", section, tag, value,
		  &override) != 4)
	goto fail;
      conf_set (trans, section, tag, value, override);
    }
  else if (strcasecmp (cmd, "rm") == 0)
    {
      if (sscanf (cmd, "C %*s [%80[^]]]:%80s", section, tag) != 2)
	goto fail;
      conf_remove (trans, section, tag);
    }
  else if (strcasecmp (cmd, "rms") == 0)
    {
      if (sscanf (cmd, "C %*s [%80[^]]]", section) != 1)
	goto fail;
      conf_remove_section (trans, section);
    }
  else
    goto fail;

  conf_end (trans, 1);
  return;

    fail:
  if (trans)
    conf_end (trans, 0);
  log_print ("ui_config: command \"%s\" malformed", cmd);
}

static void
ui_delete (char *cmd)
{
  char cookies_str[ISAKMP_HDR_COOKIES_LEN * 2 + 1];
  char message_id_str[ISAKMP_HDR_MESSAGE_ID_LEN * 2 + 1];
  u_int8_t cookies[ISAKMP_HDR_COOKIES_LEN];
  u_int8_t message_id_buf[ISAKMP_HDR_MESSAGE_ID_LEN];
  u_int8_t *message_id = message_id_buf;
  struct sa *sa;

  if (sscanf (cmd, "d %32s %8s", cookies_str, message_id_str) != 2)
    {
      log_print ("ui_delete: command \"%s\" malformed", cmd);
      return;
    }
  
  if (strcmp (message_id_str, "-") == 0)
    message_id = 0;

  if (hex2raw (cookies_str, cookies, ISAKMP_HDR_COOKIES_LEN) == -1
      || (message_id && hex2raw (message_id_str, message_id_buf,
				 ISAKMP_HDR_MESSAGE_ID_LEN) == -1))
    {
      log_print ("ui_delete: command \"%s\" has bad arguments", cmd);
      return;
    }

  sa = sa_lookup (cookies, message_id);
  if (!sa)
    {
      log_print ("ui_delete: command \"%s\" found no SA", cmd);
      return;
    }
  sa_delete (sa, 1);
}

#ifdef USE_DEBUG
/* Parse the debug command found in CMD.  */
static void
ui_debug (char *cmd)
{
  int cls, level;

  if (sscanf (cmd, "D %d %d", &cls, &level) != 2)
    {
      log_print ("ui_debug: command \"%s\" malformed", cmd);
      return;
    }
  log_debug_cmd (cls, level);
}
#endif /* USE_DEBUG */

/* Report SAs and ongoing exchanges.  */
void
ui_report (char *cmd)
{
  /* XXX Skip 'cmd' as arg? */
  sa_report ();
  exchange_report ();
  transport_report ();
  connection_report ();
  timer_report ();
}

/*
 * Call the relevant command handler based on the first character of the
 * line (the command).
 */
static void
ui_handle_command (char *line)
{
  /* Find out what one-letter command was sent.  */
  switch (line[0])
    {
    case 'c':
      ui_connect (line);
      break;

    case 'C':
      ui_config (line);
      break;

    case 'd':
      ui_delete (line);
      break;

#ifdef USE_DEBUG
    case 'D':
      ui_debug (line);
      break;
#endif

    case 'r':
      ui_report (line);
      break;

    case 't':
      ui_teardown (line);
      break;

    default:
      log_print ("ui_handle_messages: unrecognized command: '%c'", line[0]);
    }
}

/*
 * A half-complex implementation of reading from a file descriptor
 * line by line without resorting to stdio which apparently have
 * troubles with non-blocking fifos.
 */
void
ui_handler ()
{
  static char *buf = 0;
  static char *p;
  static size_t sz;
  static size_t resid;
  size_t n;
  char *new_buf;

  /* If no buffer, set it up.  */
  if (!buf)
    {
      sz = BUF_SZ;
      buf = malloc (sz);
      if (!buf)
	{
	  log_print ("malloc (%d) failed", sz);
	  return;
	}
      p = buf;
      resid = sz;
    }

  /* If no place left in the buffer reallocate twice as large.  */
  if (!resid)
    {
      new_buf = realloc (buf, sz * 2);
      if (!new_buf)
	{
	  log_print ("realloc (%p, %d) failed", buf, sz * 2);
	  free (buf);
	  buf = 0;
	  return;
	}
      buf = new_buf;
      p = buf + sz;
      resid = sz;
      sz *= 2;
    }

  n = read (ui_socket, p, resid);
  if (n == -1)
    {
      log_error ("read (%d, %p, %d)", ui_socket, p, resid);
      return;
    }

  if (!n)
    return;
  resid -= n;
  while (n--)
    {
      /*
       * When we find a newline, cut off the line and feed it to the
       * command processor.  Then move the rest up-front.
       */
      if (*p == '\n')
	{
	  *p = '\0';
	  ui_handle_command (buf);
	  memcpy (buf, p + 1, n);
	  p = buf;
	  resid = sz - n;
	  continue;
	}
      p++;
    }
}
