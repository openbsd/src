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
 *
 * $OpenBSD: auth.h,v 1.9 2001/01/18 16:59:59 markus Exp $
 */
#ifndef AUTH_H
#define AUTH_H

typedef struct Authctxt Authctxt;
struct Authctxt {
	int success;
	int postponed;
	int valid;
	int attempt;
	int failures;
	char *user;
	char *service;
	struct passwd *pw;
	char *style;
};

void	do_authentication(void);
void	do_authentication2(void);

Authctxt *authctxt_new(void);
void	auth_log(Authctxt *authctxt, int authenticated, char *method, char *info);
void	userauth_reply(Authctxt *authctxt, int authenticated);
int	auth_root_allowed(void);

int	auth2_challenge(Authctxt *authctxt, char *devs);

int	allowed_user(struct passwd * pw);

char	*get_challenge(Authctxt *authctxt, char *devs);
int	verify_response(Authctxt *authctxt, char *response);

struct passwd * auth_get_user(void);
struct passwd * pwcopy(struct passwd *pw);

#define AUTH_FAIL_MAX 6
#define AUTH_FAIL_LOG (AUTH_FAIL_MAX/2)
#define AUTH_FAIL_MSG "Too many authentication failures for %.100s"

#endif
