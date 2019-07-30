/* $OpenBSD: auth2-kbdint.c,v 1.8 2017/05/30 14:29:59 markus Exp $ */
/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/types.h>

#include "xmalloc.h"
#include "packet.h"
#include "key.h"
#include "hostfile.h"
#include "auth.h"
#include "log.h"
#include "buffer.h"
#include "misc.h"
#include "servconf.h"

/* import */
extern ServerOptions options;

static int
userauth_kbdint(struct ssh *ssh)
{
	int authenticated = 0;
	char *lang, *devs;

	lang = packet_get_string(NULL);
	devs = packet_get_string(NULL);
	packet_check_eom();

	debug("keyboard-interactive devs %s", devs);

	if (options.challenge_response_authentication)
		authenticated = auth2_challenge(ssh, devs);

	free(devs);
	free(lang);
	return authenticated;
}

Authmethod method_kbdint = {
	"keyboard-interactive",
	userauth_kbdint,
	&options.kbd_interactive_authentication
};
