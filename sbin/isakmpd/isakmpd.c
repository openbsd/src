/*	$OpenBSD: isakmpd.c,v 1.19 2000/02/19 19:32:53 niklas Exp $	*/
/*	$EOM: isakmpd.c,v 1.44 2000/02/19 07:58:56 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999, 2000 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 1999, 2000 Angelos D. Keromytis.  All rights reserved.
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

#include <errno.h>
#include <sys/param.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sysdep.h"

#include "app.h"
#include "conf.h"
#include "connection.h"
#include "init.h"
#include "log.h"
#include "timer.h"
#include "transport.h"
#include "udp.h"
#include "ui.h"
#include "util.h"
#include "cert.h"

#ifdef USE_POLICY
#include "policy.h"
#endif

/*
 * Set if -d is given, currently just for running in the foreground and log
 * to stderr instead of syslog.
 */
int debug = 0;

/*
 * If we receive a SIGHUP signal, this flag gets set to show we need to
 * reconfigure ASAP.
 */
static int sighupped = 0;

/*
 * If we receive a USR1 signal, this flag gets set to show we need to dump
 * a report over our internal state ASAP.  The file to report to is settable
 * via the -R parameter.
 */
static int sigusr1ed = 0;
static char *report_file = "/var/run/isakmpd.report";

/*
 * If we receive a USR2 signal, this flag gets set to show we need to
 * rehash our SA soft expiration timers to a uniform distribution.
 * XXX Perhaps this is a really bad idea?
 */
static int sigusr2ed = 0;

static void
usage ()
{
  fprintf (stderr,
	   "usage: %s [-d] [-c config-file] [-D class=level] [-f fifo] [-n]\n"
	   "          [-p listen-port] [-P local-port] [-r seed]\n"
	   "          [-R report-file]\n",
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
	{
	    if (sscanf (optarg, "A=%d", &level) == 1)
	      {
		  for (cls = 0; cls < LOG_ENDCLASS; cls++)
		    log_debug_cmd (cls, level);
	      }  
	    else
	      log_print ("parse_args: -D argument unparseable: %s", optarg);
	}
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
      udp_default_port = udp_decode_port (optarg);
      if (!udp_default_port)
	exit (1);
      break;
    case 'P':
      udp_bind_port = udp_decode_port (optarg);
      if (!udp_bind_port)
	exit (1);
      break;
    case 'r':
      srandom (strtoul (optarg, 0, 0));
      regrand = 1;
      break;
    case 'R':
      report_file = optarg;
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
  log_print ("SIGHUP recieved, reinitializing daemon.");

  /* 
   * XXX Remove all(/some?) pending exchange timers? - they may not be 
   *     possible to complete after we've re-read the config file.
   *     User-initiated SIGHUP's maybe "authorizes" a wait until
   *     next connection-check.
   * XXX This means we discard exchange->last_msg, is this really ok?
   */

  /* Reinitialize PRNG if we are in deterministic mode.  */
  if (regrand)
    srandom (strtoul (optarg, 0, 0));

  /* Reread config file. */
  conf_reinit ();

  /* Set timezone */
  tzset ();

#ifdef USE_POLICY
  /* Reread the policies. */
  policy_init ();
#endif

  /* Reinitialize certificates */
  cert_init();

  /* Reinitialize our connection list. */
  connection_reinit ();

  /*
   * XXX Rescan interfaces.
   *   transport_reinit (); 
   *   udp_reinit ();
   */

  /*
   * XXX "These" (non-existant) reinitializations should not be done.
   *   cookie_reinit();
   *   ui_reinit ();
   *   sa_reinit ();
   */

  sighupped = 0;
}

static void
sighup (int sig)
{
  sighupped = 1;
}

/* Report internal state on SIGUSR1.  */
static void
report (void)
{
  FILE *report = fopen (report_file, "w");
  FILE *old;

  if (!report)
    {
      log_error ("fopen (\"%s\", \"w\") failed");
      return;
    }

  /* Divert the log channel to the report file during the report.  */
  old = log_current ();
  log_to (report);
  ui_report ("r");
  log_to (old);
  fclose (report);

  sigusr1ed = 0;
}

static void
sigusr1 (int sig)
{
  sigusr1ed = 1;
}

/* Rehash soft expiration timers on SIGUSR2.  */
static void
rehash_timers (void)
{
#if 0
  /* XXX - not yet */
  log_print ("SIGUSR2 received, rehasing soft expiration timers.");
  
  timer_rehash_timers ();
#endif

  sigusr2ed = 0;
}

static void
sigusr2 (int sig)
{
  sigusr2ed = 1;
}

int
main (int argc, char *argv[])
{
  fd_set *rfds, *wfds;
  int n, m;
  size_t mask_size;
  struct timeval tv, *timeout;

  parse_args (argc, argv);
  init ();
  if (!debug)
    {
      if (daemon (0, 0))
	log_fatal ("main: daemon");
      /* Switch to syslog.  */
      log_to (0);
    }

  /* Reinitialize on HUP reception.  */
  signal (SIGHUP, sighup);

  /* Report state on USR1 reception.  */
  signal (SIGUSR1, sigusr1);

  /* Rehash soft expiration timers on USR2 reception.  */
  signal (SIGUSR2, sigusr2);

  /* Allocate the file descriptor sets just big enough.  */
  n = getdtablesize ();
  mask_size = howmany (n, NFDBITS) * sizeof (fd_mask);
  rfds = (fd_set *)malloc (mask_size);
  if (!rfds)
    log_fatal ("main: malloc (%d)", mask_size);
  wfds = (fd_set *)malloc (mask_size);
  if (!wfds)
    log_fatal ("main: malloc (%d)", mask_size);

  while (1)
    {
      /* If someone has sent SIGHUP to us, reconfigure.  */
      if (sighupped)
	reinit ();

      /* and if someone sent SIGUSR1, do a state report.  */
      if (sigusr1ed)
	report ();

      /* and if someone sent SIGUSR2, do a timer rehash.  */
      if (sigusr2ed)
	rehash_timers ();
      
      /* Setup the descriptors to look for incoming messages at.  */
      memset (rfds, 0, mask_size);
      n = transport_fd_set (rfds);
      FD_SET (ui_socket, rfds);
      if (ui_socket + 1 > n)
	n = ui_socket + 1;

      /*
       * XXX Some day we might want to deal with an abstract application
       * class instead, with many instantiations possible.
       */
      if (!app_none && app_socket >= 0)
	{
	  FD_SET (app_socket, rfds);
	  if (app_socket + 1 > n)
	    n = app_socket + 1;
	}

      /* Setup the descriptors that have pending messages to send.  */
      memset (wfds, 0, mask_size);
      m = transport_pending_wfd_set (wfds);
      if (m > n)
	n = m;

      /* Find out when the next timed event is.  */
      timeout = &tv;
      timer_next_event (&timeout);

      n = select (n, rfds, wfds, 0, timeout);
      if (n == -1)
	{
	  if (errno != EINTR)
	    {
	      log_error ("select");

	      /*
	       * In order to give the unexpected error condition time to
	       * resolve without letting this process eat up all available CPU
	       * we sleep for a short while.
	       */
	      sleep (1);
	    }
	}
      else if (n)
	{
	  transport_handle_messages (rfds);
	  transport_send_messages (wfds);
	  if (FD_ISSET (ui_socket, rfds))
	    ui_handler ();
	  if (!app_none && app_socket >= 0 && FD_ISSET (app_socket, rfds))
	    app_handler ();
	}
      timer_handle_expirations ();
    }
}
