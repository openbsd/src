/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * RSA-based authentication.  This code determines whether to admit a login
 * based on RSA authentication.  This file also contains functions to check
 * validity of the host key.
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
RCSID("$OpenBSD: auth-options.c,v 1.3 2000/09/07 20:27:49 deraadt Exp $");

#include "ssh.h"
#include "packet.h"
#include "xmalloc.h"
#include "match.h"

/* Flags set authorized_keys flags */
int no_port_forwarding_flag = 0;
int no_agent_forwarding_flag = 0;
int no_x11_forwarding_flag = 0;
int no_pty_flag = 0;

/* "command=" option. */
char *forced_command = NULL;

/* "environment=" options. */
struct envstring *custom_environment = NULL;

/* return 1 if access is granted, 0 if not. side effect: sets key option flags */
int
auth_parse_options(struct passwd *pw, char *options, unsigned long linenum)
{
	const char *cp;
	if (!options)
		return 1;
	while (*options && *options != ' ' && *options != '\t') {
		cp = "no-port-forwarding";
		if (strncmp(options, cp, strlen(cp)) == 0) {
			packet_send_debug("Port forwarding disabled.");
			no_port_forwarding_flag = 1;
			options += strlen(cp);
			goto next_option;
		}
		cp = "no-agent-forwarding";
		if (strncmp(options, cp, strlen(cp)) == 0) {
			packet_send_debug("Agent forwarding disabled.");
			no_agent_forwarding_flag = 1;
			options += strlen(cp);
			goto next_option;
		}
		cp = "no-X11-forwarding";
		if (strncmp(options, cp, strlen(cp)) == 0) {
			packet_send_debug("X11 forwarding disabled.");
			no_x11_forwarding_flag = 1;
			options += strlen(cp);
			goto next_option;
		}
		cp = "no-pty";
		if (strncmp(options, cp, strlen(cp)) == 0) {
			packet_send_debug("Pty allocation disabled.");
			no_pty_flag = 1;
			options += strlen(cp);
			goto next_option;
		}
		cp = "command=\"";
		if (strncmp(options, cp, strlen(cp)) == 0) {
			int i;
			options += strlen(cp);
			forced_command = xmalloc(strlen(options) + 1);
			i = 0;
			while (*options) {
				if (*options == '"')
					break;
				if (*options == '\\' && options[1] == '"') {
					options += 2;
					forced_command[i++] = '"';
					continue;
				}
				forced_command[i++] = *options++;
			}
			if (!*options) {
				debug("%.100s, line %lu: missing end quote",
				      SSH_USER_PERMITTED_KEYS, linenum);
				packet_send_debug("%.100s, line %lu: missing end quote",
						  SSH_USER_PERMITTED_KEYS, linenum);
				continue;
			}
			forced_command[i] = 0;
			packet_send_debug("Forced command: %.900s", forced_command);
			options++;
			goto next_option;
		}
		cp = "environment=\"";
		if (strncmp(options, cp, strlen(cp)) == 0) {
			int i;
			char *s;
			struct envstring *new_envstring;
			options += strlen(cp);
			s = xmalloc(strlen(options) + 1);
			i = 0;
			while (*options) {
				if (*options == '"')
					break;
				if (*options == '\\' && options[1] == '"') {
					options += 2;
					s[i++] = '"';
					continue;
				}
				s[i++] = *options++;
			}
			if (!*options) {
				debug("%.100s, line %lu: missing end quote",
				      SSH_USER_PERMITTED_KEYS, linenum);
				packet_send_debug("%.100s, line %lu: missing end quote",
						  SSH_USER_PERMITTED_KEYS, linenum);
				continue;
			}
			s[i] = 0;
			packet_send_debug("Adding to environment: %.900s", s);
			debug("Adding to environment: %.900s", s);
			options++;
			new_envstring = xmalloc(sizeof(struct envstring));
			new_envstring->s = s;
			new_envstring->next = custom_environment;
			custom_environment = new_envstring;
			goto next_option;
		}
		cp = "from=\"";
		if (strncmp(options, cp, strlen(cp)) == 0) {
			int mname, mip;
			char *patterns = xmalloc(strlen(options) + 1);
			int i;
			options += strlen(cp);
			i = 0;
			while (*options) {
				if (*options == '"')
					break;
				if (*options == '\\' && options[1] == '"') {
					options += 2;
					patterns[i++] = '"';
					continue;
				}
				patterns[i++] = *options++;
			}
			if (!*options) {
				debug("%.100s, line %lu: missing end quote",
				    SSH_USER_PERMITTED_KEYS, linenum);
				packet_send_debug("%.100s, line %lu: missing end quote",
				    SSH_USER_PERMITTED_KEYS, linenum);
				continue;
			}
			patterns[i] = 0;
			options++;
			/*
			 * Deny access if we get a negative
			 * match for the hostname or the ip
			 * or if we get not match at all
			 */
			mname = match_hostname(get_canonical_hostname(),
			    patterns, strlen(patterns));
			mip = match_hostname(get_remote_ipaddr(),
			    patterns, strlen(patterns));
			xfree(patterns);
			if (mname == -1 || mip == -1 ||
			    (mname != 1 && mip != 1)) {
				log("Authentication tried for %.100s with correct key but not from a permitted host (host=%.200s, ip=%.200s).",
				    pw->pw_name, get_canonical_hostname(),
				    get_remote_ipaddr());
				packet_send_debug("Your host '%.200s' is not permitted to use this key for login.",
				get_canonical_hostname());
				/* key invalid for this host, reset flags */
				no_agent_forwarding_flag = 0;
				no_port_forwarding_flag = 0;
				no_pty_flag = 0;
				no_x11_forwarding_flag = 0;
				while (custom_environment) {
					struct envstring *ce = custom_environment;
					custom_environment = ce->next;
					xfree(ce->s);
					xfree(ce);
				}
				if (forced_command) {
					xfree(forced_command);
					forced_command = NULL;
				}
				/* deny access */
				return 0;
			}
			/* Host name matches. */
			goto next_option;
		}
next_option:
		/*
		 * Skip the comma, and move to the next option
		 * (or break out if there are no more).
		 */
		if (!*options)
			fatal("Bugs in auth-options.c option processing.");
		if (*options == ' ' || *options == '\t')
			break;		/* End of options. */
		if (*options != ',')
			goto bad_option;
		options++;
		/* Process the next option. */
	}
	/* grant access */
	return 1;

bad_option:
	log("Bad options in %.100s file, line %lu: %.50s",
	    SSH_USER_PERMITTED_KEYS, linenum, options);
	packet_send_debug("Bad options in %.100s file, line %lu: %.50s",
	    SSH_USER_PERMITTED_KEYS, linenum, options);
	/* deny access */
	return 0;
}
