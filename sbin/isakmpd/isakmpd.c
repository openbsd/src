/*	$Id: isakmpd.c,v 1.1.1.1 1998/11/15 00:03:49 niklas Exp $	*/

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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app.h"
#include "conf.h"
#include "init.h"
#include "log.h"
#include "sysdep.h"
#include "timer.h"
#include "transport.h"
#include "udp.h"
#include "ui.h"

extern char *optarg;
extern int optind;

/*
 * Set if -d is given, currently just for running in the foreground and log
 * to stderr instead of syslog.
 */
int debug = 0;

/*
 * Use -r seed to initalize random numbers to a deterministic sequence.
 */
extern int regrand;

/*
 * If we receive a SIGHUP signal, this flag gets set to show we need to
 * reconfigure ASAP.
 */
static int sighupped = 0;

static void
usage ()
{
  fprintf (stderr,
	   "usage: %s [-d] [-c config-file] [-D class=level] [-f fifo] [-n]\n"
	   "          [-p listen-port] [-P local-port] [-r seed]\n",
	   sysdep_progname ());
  exit (1);
}

static void
parse_args (int argc, char *argv[])
{
  int ch, cls, level;

  while ((ch = getopt (argc, argv, "c:dD:f:np:P:r:")) != -1) {
    switch (ch) {
    case 'c':
      conf_path = optarg;
      break;
    case 'd':
      debug++;
      break;
    case 'D':
      if (sscanf (optarg, "%d=%d", &cls, &level) != 2)
	log_print ("parse_args: -D argument unparseable: %s", optarg);
      else
	log_debug_cmd (cls, level);
      break;
    case 'f':
      ui_fifo = optarg;
      break;
    case 'n':
      app_none++;
      break;
    case 'p':
      udp_default_port = atoi (optarg);
      break;
    case 'P':
      udp_bind_port = atoi (optarg);
      break;
    case 'r':
      srandom (strtoul (optarg, NULL, 0));
      regrand = 1;
      break;
    case '?':
    default:
      usage ();
    }
  }
  argc -= optind;
  argv += optind;
}

/* Reinitialize after a SIGHUP reception.  */
static void
reinit (void)
{
  /* Reread config file.  */
  conf_init ();

  /* XXX Rescan interfaces.  */

  sighupped = 0;
}

static void
sighup (int sig)
{
  sighupped = 1;
}

int
main (int argc, char *argv[])
{
  fd_set rfds, wfds;
  int n, m;
  struct timeval tv, *timeout = &tv;

  parse_args (argc, argv);
  init ();
  if (!debug)
    {
      if (daemon (0, 0))
	log_fatal ("daemon");
      /* Switch to syslog.  */
      log_to (0);
    }

  /* Reinitialize on HUP reception.  */
  signal (SIGHUP, sighup);

  while (1)
    {
      /* If someone has sent SIGHUP to us, reconfigure.  */
      if (sighupped)
	reinit ();

      /* Setup the descriptors to look for incoming messages at.  */
      FD_ZERO (&rfds);
      n = transport_fd_set (&rfds);
      FD_SET (ui_socket, &rfds);
      if (ui_socket + 1 > n)
	n = ui_socket + 1;

      /*
       * XXX Some day we might want to deal with an abstract application
       * class instead, with many instantiations possible.
       */
      if (!app_none)
	{
	  FD_SET (app_socket, &rfds);
	  if (app_socket + 1 > n)
	    n = app_socket + 1;
	}

      /* Setup the descriptors that have pending messages to send.  */
      FD_ZERO (&wfds);
      m = transport_pending_wfd_set (&wfds);
      if (m > n)
	n = m;

      /* Find out when the next timed event is.  */
      timer_next_event (&timeout);

      n = select (n, &rfds, &wfds, 0, timeout);
      if (n == -1)
	{
	  log_error ("select");
	  /*
	   * In order to give the unexpected error condition time to resolve
	   * without letting this process eat up all available CPU we sleep
	   * for a short while.
	   */
	  sleep (1);
	}
      else if (n)
	{
	  transport_handle_messages (&rfds);
	  transport_send_messages (&wfds);
	  if (FD_ISSET (ui_socket, &rfds))
	    ui_handler ();
	  if (!app_none && FD_ISSET (app_socket, &rfds))
	    app_handler ();
	}
      timer_handle_expirations ();
    }
}
