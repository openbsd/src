/*	$OpenBSD: connection.c,v 1.1 1999/05/02 05:52:48 niklas Exp $	*/
/*	$EOM: connection.c,v 1.1 1999/05/01 20:21:09 niklas Exp $	*/

/*
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

#include <sys/queue.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#include "conf.h"
#include "connection.h"
#include "log.h"
#include "timer.h"

/* How often should we check that connections we require to be up, are up?  */
#define CHECK_INTERVAL 60

struct connection
{
  TAILQ_ENTRY (connection) link;
  char *name;
  struct event *ev;
};

TAILQ_HEAD (connection_head, connection) connections;

/*
 * This is where we setup all the connections we want there right from the
 * start.
 */
void
connection_init ()
{
  struct conf_list *conns;
  struct conf_list_node *conn;

  TAILQ_INIT (&connections);
  conns = conf_get_list ("Phase 2", "Connections");
  if (conns)
    {
      for (conn = TAILQ_FIRST (&conns->fields); conn;
	   conn = TAILQ_NEXT (conn, link))
	{
	  if (connection_setup (conn->field))
	    log_print ("connection_init: could not setup \"%s\"", conn->field);
	    continue;
	}
      conf_free_list (conns);
    }
}

/* Check the connection in VCONN and schedule another check later.  */
static void
connection_checker (void *vconn)
{
  struct timeval now;
  struct connection *conn = vconn;

  gettimeofday (&now, 0);
  now.tv_sec += conf_get_num ("General", "check-interval", CHECK_INTERVAL);
  conn->ev
    = timer_add_event ("connection_checker", connection_checker, conn, &now);
  if (!conn->ev)
    log_print ("connection_checker: could not add timer event");
  sysdep_connection_check (conn->name);
}

/* Find the connection named NAME.  */
static struct connection *
connection_lookup (char *name)
{
  struct connection *conn;

  for (conn = TAILQ_FIRST (&connections); conn; conn = TAILQ_NEXT (conn, link))
    if (strcasecmp (conn->name, name) == 0)
      return conn;
  return 0;
}

/*
 * Setup NAME to be a connection that should be up "always", i.e. if it dies,
 * for whatever reason, it should be tried to be brought up, over and over
 * again.
 */
int
connection_setup (char *name)
{
  struct connection *conn = 0;
  struct timeval now;

  /* Check for trials to add duplicate connections.  */
  if (connection_lookup (name))
    {
      log_debug (LOG_MISC, 10, "connection_setup: cannot add \"%s\" twice",
		 name);
      return 0;
    }

  conn = calloc (1, sizeof *conn);
  if (!conn)
    {
      log_error ("connection_setup: calloc (1, %d) failed", sizeof *conn);
      goto fail;
    }

  conn->name = strdup (name);
  if (!conn->name)
    {
      log_error ("connection_setup: strdup (\"%s\") failed", name);
      goto fail;
    }

  gettimeofday (&now, 0);
  conn->ev
    = timer_add_event ("connection_checker", connection_checker, conn, &now);
  if (!conn->ev)
    {
      log_print ("connection_setup: could not add timer event");
      goto fail;
    }

  TAILQ_INSERT_TAIL (&connections, conn, link);
  return 0;

 fail:
  if (conn)
    {
      if (conn->name)
	free (conn->name);
      free (conn);
    }
  return -1;
}

/* Remove the connection named NAME.  */
void
connection_teardown (char *name)
{
  struct connection *conn;

  conn = connection_lookup (name);
  if (!conn)
    return;

  TAILQ_REMOVE (&connections, conn, link);
  timer_remove_event (conn->ev);
  free (conn->name);
  free (conn);
}
