/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Rhosts or /etc/hosts.equiv authentication combined with RSA host
 * authentication.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
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

#include "includes.h"
RCSID("$OpenBSD: auth-rh-rsa.c,v 1.15 2000/09/07 20:27:49 deraadt Exp $");

#include "packet.h"
#include "ssh.h"
#include "xmalloc.h"
#include "uidswap.h"
#include "servconf.h"

#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include "key.h"
#include "hostfile.h"

/*
 * Tries to authenticate the user using the .rhosts file and the host using
 * its host key.  Returns true if authentication succeeds.
 */

int
auth_rhosts_rsa(struct passwd *pw, const char *client_user, RSA *client_host_key)
{
	extern ServerOptions options;
	const char *canonical_hostname;
	HostStatus host_status;
	Key *client_key, *found;

	debug("Trying rhosts with RSA host authentication for %.100s", client_user);

	if (client_host_key == NULL)
		return 0;

	/* Check if we would accept it using rhosts authentication. */
	if (!auth_rhosts(pw, client_user))
		return 0;

	canonical_hostname = get_canonical_hostname();

	debug("Rhosts RSA authentication: canonical host %.900s", canonical_hostname);

	/* wrap the RSA key into a 'generic' key */
	client_key = key_new(KEY_RSA);
	BN_copy(client_key->rsa->e, client_host_key->e);
	BN_copy(client_key->rsa->n, client_host_key->n);
	found = key_new(KEY_RSA);

	/* Check if we know the host and its host key. */
	host_status = check_host_in_hostfile(SSH_SYSTEM_HOSTFILE, canonical_hostname,
	    client_key, found);

	/* Check user host file unless ignored. */
	if (host_status != HOST_OK && !options.ignore_user_known_hosts) {
		struct stat st;
		char *user_hostfile = tilde_expand_filename(SSH_USER_HOSTFILE, pw->pw_uid);
		/*
		 * Check file permissions of SSH_USER_HOSTFILE, auth_rsa()
		 * did already check pw->pw_dir, but there is a race XXX
		 */
		if (options.strict_modes &&
		    (stat(user_hostfile, &st) == 0) &&
		    ((st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
		     (st.st_mode & 022) != 0)) {
			log("Rhosts RSA authentication refused for %.100s: bad owner or modes for %.200s",
			    pw->pw_name, user_hostfile);
		} else {
			/* XXX race between stat and the following open() */
			temporarily_use_uid(pw->pw_uid);
			host_status = check_host_in_hostfile(user_hostfile, canonical_hostname,
			    client_key, found);
			restore_uid();
		}
		xfree(user_hostfile);
	}
	key_free(client_key);
	key_free(found);

	if (host_status != HOST_OK) {
		debug("Rhosts with RSA host authentication denied: unknown or invalid host key");
		packet_send_debug("Your host key cannot be verified: unknown or invalid host key.");
		return 0;
	}
	/* A matching host key was found and is known. */

	/* Perform the challenge-response dialog with the client for the host key. */
	if (!auth_rsa_challenge_dialog(client_host_key)) {
		log("Client on %.800s failed to respond correctly to host authentication.",
		    canonical_hostname);
		return 0;
	}
	/*
	 * We have authenticated the user using .rhosts or /etc/hosts.equiv,
	 * and the host using RSA. We accept the authentication.
	 */

	verbose("Rhosts with RSA host authentication accepted for %.100s, %.100s on %.700s.",
	   pw->pw_name, client_user, canonical_hostname);
	packet_send_debug("Rhosts with RSA host authentication accepted.");
	return 1;
}
