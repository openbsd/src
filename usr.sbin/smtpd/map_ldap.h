/*
 * XXX
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>


#define MAX_LDAP_IDENTIFIER		 32
#define MAX_LDAP_URL		 	 256
#define MAX_LDAP_USERNAME      	 256
#define MAX_LDAP_PASSWORD      	 256
#define MAX_LDAP_BASELEN      	 128
#define MAX_LDAP_FILTERLEN     	 1024
#define MAX_LDAP_FIELDLEN      	 128

struct ldap_conf;

extern TAILQ_HEAD(ldap_confs, ldap_conf) ldap_confs;

struct ldap_conf {
	char					identifier[MAX_LDAP_IDENTIFIER];
	char					url[MAX_LDAP_URL];
	char					username[MAX_LDAP_USERNAME];
	char					password[MAX_LDAP_PASSWORD];
	char					m_ldapbasedn[MAX_LDAP_BASELEN];
	char					m_ldapfilter[MAX_LDAP_FILTERLEN];
	char					m_ldapattr[MAX_LDAP_FIELDLEN];
	TAILQ_ENTRY(ldap_conf)	entry;
};

struct ldaphandle {
	void				*aldap;
	struct ldap_conf	*conf;
};
