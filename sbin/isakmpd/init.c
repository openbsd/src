/*	$OpenBSD: init.c,v 1.20 2002/08/07 13:19:20 ho Exp $	*/
/*	$EOM: init.c,v 1.25 2000/03/30 14:27:24 ho Exp $	*/

/*
 * Copyright (c) 1998, 1999, 2000 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2000 Angelos D. Keromytis.  All rights reserved.
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

/* XXX This file could easily be built dynamically instead.  */

#include "sysdep.h"

#include "app.h"
#include "cert.h"
#include "conf.h"
#include "connection.h"
#include "doi.h"
#include "exchange.h"
#include "init.h"
#include "ipsec.h"
#include "isakmp_doi.h"
#include "libcrypto.h"
#include "log.h"
#include "math_group.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"
#include "udp.h"
#include "ui.h"
#include "util.h"

#ifdef USE_POLICY
#include "policy.h"
#endif

void
init (void)
{
  log_init ();
  app_init ();
  doi_init ();
  exchange_init ();
  group_init ();
  ipsec_init ();
  isakmp_doi_init ();
  libcrypto_init ();

  tzset ();

  timer_init ();

  /* The following group are depending on timer_init having run.  */
  conf_init ();
  connection_init ();

#ifdef USE_POLICY
  /* policy_init depends on conf_init having run.  */
  policy_init ();
#endif

  /* Depends on conf_init and policy_init having run */
  cert_init ();
  crl_init ();

  sa_init ();
  transport_init ();
  udp_init ();
  ui_init ();
}

/* Reinitialize, either after a SIGHUP reception or by FIFO UI cmd.  */
void
reinit (void)
{
  log_print ("reinitializing daemon");

  /*
   * XXX Remove all(/some?) pending exchange timers? - they may not be
   *     possible to complete after we've re-read the config file.
   *     User-initiated SIGHUP's maybe "authorizes" a wait until
   *     next connection-check.
   * XXX This means we discard exchange->last_msg, is this really ok?
   */

  /* Reinitialize PRNG if we are in deterministic mode.  */
  if (regrand)
    srandom (seed);

  /* Reread config file.  */
  conf_reinit ();

  /* Set timezone */
  tzset ();

#ifdef USE_POLICY
  /* Reread the policies.  */
  policy_init ();
#endif

  /* Reinitialize certificates */
  cert_init ();
  crl_init ();

  /* Reinitialize our connection list.  */
  connection_reinit ();

  /*
   * Rescan interfaces.
   */
  transport_reinit ();

  /*
   * XXX "These" (non-existant) reinitializations should not be done.
   *   cookie_reinit ();
   *   ui_reinit ();
   *   sa_reinit ();
   */
}
