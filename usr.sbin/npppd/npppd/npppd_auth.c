/* $OpenBSD: npppd_auth.c,v 1.6 2010/07/02 21:20:57 yasuoka Exp $ */

/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/**@file authentication realm */
/* $Id: npppd_auth.c,v 1.6 2010/07/02 21:20:57 yasuoka Exp $ */
/* I hope to write the source code in npppd-independent as possible. */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <event.h>
#include <stdarg.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>

#include "debugutil.h"
#include "hash.h"
#include "slist.h"
#include "npppd_local.h"
#include "npppd_auth.h"
#include "config_helper.h"
#include "net_utils.h"
#include "csvreader.h"

#include "npppd_auth_local.h"

/**
 * Create a npppd_auth_base object.
 * @param auth_type	the authentication type.
 *	specify {@link ::NPPPD_AUTH_TYPE_LOCAL} to authenticate by the local
 *	file, or specify {@link ::NPPPD_AUTH_TYPE_RADIUS} for RADIUS
 *	authentication.
 * @param label		the configuration label
 * @param _npppd	the parent {@link ::npppd} object
 * @see	::NPPPD_AUTH_TYPE_LOCAL
 * @see	::NPPPD_AUTH_TYPE_RADIUS
 * @return The pointer to the {@link ::npppd_auth_base} object will be returned
 * in case success otherwise NULL will be returned.
 */
npppd_auth_base *
npppd_auth_create(int auth_type, const char *label, void *_npppd)
{
	npppd_auth_base *base;

	NPPPD_AUTH_ASSERT(label != NULL);

	switch (auth_type) {
	case NPPPD_AUTH_TYPE_LOCAL:
		if ((base = malloc(sizeof(npppd_auth_local))) != NULL) {
			memset(base, 0, sizeof(npppd_auth_local));
			base->type = NPPPD_AUTH_TYPE_LOCAL;
			base->users_hash = NULL;
			base->strip_nt_domain = 1;
			base->strip_atmark_realm = 0;
			strlcpy(base->label, label, sizeof(base->label));
			base->npppd = _npppd;

			return base;
		}
		break;

#ifdef USE_NPPPD_RADIUS
	case NPPPD_AUTH_TYPE_RADIUS:
		if ((base = malloc(sizeof(npppd_auth_radius))) != NULL) {
			memset(base, 0, sizeof(npppd_auth_radius));
			base->type = NPPPD_AUTH_TYPE_RADIUS;
			base->strip_nt_domain = 0;
			strlcpy(base->label, label, sizeof(base->label));
			base->npppd = _npppd;

			return base;
		}
		break;
#endif

	default:
		NPPPD_AUTH_ASSERT(0);
		break;
	}

	return NULL;
}

/**
 * Call this function to make the object unusable.
 * <p>
 * {@link ::npppd_auth_base} objects is refered by the {@link ::npppd_ppp}
 * object.   After this funcation is called, npppd will disconnect the PPP
 * links that refers the object, it will call {@link ::npppd_auth_destroy()}
 * when all the references to the object are released.</p>
 */
void
npppd_auth_dispose(npppd_auth_base *base)
{

	base->disposing = 1;

	if (base->users_hash != NULL)
		hash_delete_all(base->users_hash, 1);

	return;
}

/** Destroy the {@link ::npppd_auth_base} object.  */
void
npppd_auth_destroy(npppd_auth_base *base)
{

	if (base->disposing == 0)
		npppd_auth_dispose(base);

	if (base->users_hash != NULL) {
		hash_free(base->users_hash);
		base->users_hash = NULL;
	}

	npppd_auth_base_log(base, LOG_INFO, "Finalized");

	switch(base->type) {
	case NPPPD_AUTH_TYPE_LOCAL:
		memset(base, 0, sizeof(npppd_auth_local));
		break;

	case NPPPD_AUTH_TYPE_RADIUS:
		memset(base, 0, sizeof(npppd_auth_local));
		break;
	}
	free(base);

	return;
}

/** Reload the configuration */
int
npppd_auth_reload(npppd_auth_base *base)
{
	const char *val;

	val = npppd_auth_config_str(base, "name");
	if (val == NULL)
		/* use the label if .name is not defined. */
		strlcpy(base->name, npppd_auth_default_label(base),
		    sizeof(base->name));
	else
		strlcpy(base->name, val, sizeof(base->name));

	if ((val = npppd_auth_config_str(base, "pppsuffix")) != NULL)
		strlcpy(base->pppsuffix, val, sizeof(base->pppsuffix));
	else
		base->pppsuffix[0] = '\0';

	if ((val = npppd_auth_config_str(base, "pppprefix")) != NULL)
		strlcpy(base->pppprefix, val, sizeof(base->pppprefix));
	else
		base->pppprefix[0] = '\0';

	base->eap_capable =
	    npppd_auth_config_str_equal(base, "eap_capable", "true", 1);
	base->strip_nt_domain =
	    npppd_auth_config_str_equal(base, "strip_nt_domain", "true", 1);
	base->strip_atmark_realm =
	    npppd_auth_config_str_equal(base, "strip_atmark_realm", "true", 0);

	base->has_acctlist = 0;
	base->acctlist_ready = 0;
	base->radius_ready = 0;
	if ((val = npppd_auth_config_str(base, "acctlist")) != NULL) {
		strlcpy(base->acctlist_path, val, sizeof(base->acctlist_path));
		if (base->users_hash == NULL) {
			if ((base->users_hash = hash_create(
			    (int (*)(const void *, const void *))strcmp,
				str_hash, 1021)) == NULL) {
				npppd_auth_base_log(base,
				    LOG_WARNING, "hash_create() failed: %m.");
				goto fail;
			}
		}
		base->reloadable = NPPPD_DEFAULT_AUTH_LOCAL_RELOADABLE;
		base->has_acctlist = 1;
		if (npppd_auth_reload_acctlist(base) != 0)
			goto fail;

	} else {
		if (base->type == NPPPD_AUTH_TYPE_LOCAL) {
			npppd_auth_base_log(base,
			    LOG_WARNING, "missing acctlist property.");
			goto fail;
		}
	}

	switch (base->type) {
#ifdef USE_NPPPD_RADIUS
	case NPPPD_AUTH_TYPE_RADIUS:
		if (npppd_auth_radius_reload(base) != 0)
			goto fail;
		break;
#endif
	}
	base->initialized = 1;

	return 0;

fail:
	base->initialized = 0;
	base->has_acctlist = 0;
	base->acctlist_ready = 0;
	base->radius_ready = 0;

	return 1;
}

/**
 * This function gets specified user's password. The value 0 is returned
 * if the call succeeds.
 *
 * @param	username	username which gets the password
 * @param	password	buffers which stores the password
 *				Specify NULL if you want to known the length of
 *				the password only.
 * @param	lppassword	pointer which indicates the length of
 *				the buffer which stores the password.
 * @return A value 1 is returned if user is unknown. A value 2 is returned
 *				if password buffer is sufficient. A negative value is
 *				returned if other error occurred.
 */
int
npppd_auth_get_user_password(npppd_auth_base *base,
    const char *username, char *password, int *plpassword)
{
	int sz, lpassword;
	npppd_auth_user *user;

	NPPPD_AUTH_ASSERT(base != NULL);
	NPPPD_AUTH_DBG((base, LOG_DEBUG, "%s(%s)", __func__, username));

	if (base->has_acctlist == 0 || base->acctlist_ready == 0)
		return -1;

	if (base->reloadable != 0)
		npppd_auth_reload_acctlist(base);

	if ((user = npppd_auth_find_user(base, username)) == NULL)
		return 1;

	if (password == NULL && plpassword == NULL)
		return 0;
	if (plpassword == NULL)
		return -1;
	lpassword = strlen(user->password) + 1;
	sz = *plpassword;
	*plpassword = lpassword;
	if (password == NULL)
		return 0;
	if (sz < lpassword)
		return 2;

	strlcpy(password, user->password, sz);
	return 0;
}

/**
 * This function gets specified users' Framed-IP-{Address,Netmask}.
 * The value 0 is returned if the call succeeds.
 * <p>
 * Because authentication database is updated at any time, the password is
 * possible to be inconsistent if this function is not called immediately
 * after authentication. So this function is called immediately after
 * authentication. </p>
 * @param	username	username which gets the password
 * @param	ip4address	pointer which indicates struct in_addr which
 *						stores the Framed-IP-Address
 * @param	ip4netmask	pointer which indicates struct in_addr which
 *						stores Framed-IP-Netmask
 */
int
npppd_auth_get_framed_ip(npppd_auth_base *base, const char *username,
    struct in_addr *ip4address, struct in_addr *ip4netmask)
{
	npppd_auth_user *user;

	NPPPD_AUTH_ASSERT(base != NULL);
	NPPPD_AUTH_DBG((base, LOG_DEBUG, "%s(%s)", __func__, username));
	if (base->has_acctlist == 0 || base->acctlist_ready == 0)
		return -1;

	if ((user = npppd_auth_find_user(base, username)) == NULL)
		return 1;

	if (user->framed_ip_address.s_addr != 0) {
		*ip4address = user->framed_ip_address;
		if (ip4netmask != NULL)
			*ip4netmask = user->framed_ip_netmask;

		return 0;
	}

	return 1;
}

/**
 * Retribute "Calling-Number" attribute of the user from the realm.
 *
 * @param username	Username.
 * @param number	Pointer to the space for the Calling-Number.  This
 *	can be NULL in case retributing the Calling-Number only.
 * @param plnumber	Pointer to the length of the space for the
 *	Calling-Number.
 * @return 0 if the Calling-Number attribute is successfully retributed.
 *	1 if the user has no Calling-Number attribute.  return -1 if the realm
 *	doesn't have user attributes or other errors.   return 2 if the space
 *	is not enough.
 */
int
npppd_auth_get_calling_number(npppd_auth_base *base, const char *username,
    char *number, int *plnumber)
{
	int lcallnum, sz;
	npppd_auth_user *user;

	if (base->has_acctlist == 0 || base->acctlist_ready == 0)
		return -1;

	if ((user = npppd_auth_find_user(base, username)) == NULL)
		return 1;

	if (number == NULL && plnumber == NULL)
		return 0;
	if (plnumber == NULL)
		return -1;
	lcallnum = strlen(user->calling_number) + 1;
	sz = *plnumber;
	*plnumber = lcallnum;
	if (sz < lcallnum)
		return 2;
	strlcpy(number, user->calling_number, sz);

	return 0;
}

int
npppd_auth_get_type(npppd_auth_base *base)
{
	return base->type;
}

int
npppd_auth_is_usable(npppd_auth_base *base)
{
    	return (base->initialized != 0 && base->disposing == 0)? 1 : 0;
}

int
npppd_auth_is_ready(npppd_auth_base *base)
{
	if (!npppd_auth_is_usable(base))
		return 0;

	switch(base->type) {
	case NPPPD_AUTH_TYPE_LOCAL:
		return (base->acctlist_ready != 0)? 1 : 0;
		/* NOTREACHED */

	case NPPPD_AUTH_TYPE_RADIUS:
		return (base->acctlist_ready != 0 ||
		    base->radius_ready != 0)? 1 : 0;
		/* NOTREACHED */
	}
	NPPPD_AUTH_ASSERT(0);

    	return 0;
}

int
npppd_auth_is_disposing(npppd_auth_base *base)
{
	return (base->disposing != 0)? 1 : 0;
}

int
npppd_auth_is_eap_capable(npppd_auth_base *base)
{
	return (base->eap_capable != 0)? 1 : 0;
}

const char *
npppd_auth_get_label(npppd_auth_base *base)
{
	return base->label;
}

const char *
npppd_auth_get_name(npppd_auth_base *base)
{
	return base->name;
}

const char *
npppd_auth_get_suffix(npppd_auth_base *base)
{
	return base->pppsuffix;
}

const char *
npppd_auth_username_for_auth(npppd_auth_base *base, const char *username,
    char *username_buffer)
{
	const char *u0;
	char *atmark, *u1;

	u0 = NULL;
	if (base->strip_nt_domain != 0) {
		if ((u0 = strchr(username, '\\')) != NULL)
			u0++;
	}
	if (u0 == NULL)
		u0 = username;
	u1 = username_buffer;
	if (username_buffer != u0)
		memmove(username_buffer, u0, MIN(strlen(u0) + 1,
		    MAX_USERNAME_LENGTH));
	if (base->strip_atmark_realm != 0) {
		if ((atmark = strrchr(u1, '@')) != NULL)
			*atmark = '\0';
	}

	return username_buffer;
}

/***********************************************************************
 * Account list related functions
 ***********************************************************************/
/** Reload the account list */
static int
npppd_auth_reload_acctlist(npppd_auth_base *base)
{
	CSVREADER_STATUS status;
	int linno, ncols, usersz, nuser, eof, off;
	const char **cols, *passwd, *callnum;
	char line[8192];
	csvreader *csv;
	npppd_auth_user *user;
	struct in_addr ip4, ip4mask;
	slist users;
	FILE *file;
	struct stat st;

	if (base->acctlist_ready != 0 && lstat(base->acctlist_path, &st) == 0) {
		if (st.st_mtime == base->last_load)
			return 0;
		base->last_load = st.st_mtime;
	}

	slist_init(&users);
	csv = NULL;
	if ((file = priv_fopen(base->acctlist_path)) == NULL) {
		/* hash is empty if file is not found. */
		if (errno == ENOENT)
			hash_delete_all(base->users_hash, 1);
		npppd_auth_base_log(base,
		    (errno == ENOENT)? LOG_DEBUG : LOG_ERR,
		    "Open %s failed: %m", base->acctlist_path);
		return 0;
	}
	if ((csv = csvreader_create()) == NULL) {
		npppd_auth_base_log(base, LOG_ERR,
		    "Loading a account list failed: csvreader_create(): %m");
		goto fail;
	}

	for (linno = 0, eof = 0; !eof;) {
		ip4.s_addr = 0;
		ip4mask.s_addr = 0xffffffffL;
		if (fgets(line, sizeof(line), file) != NULL) {
			int linelen;

			linelen = strlen(line);
			if (linelen <= 0) {
				npppd_auth_base_log(base, LOG_ERR,
				    "Loading a account list failed: lineno=%d "
				    "line too short", linno + 1);
				goto fail;
			}
			if (line[linelen - 1] != '\n' && !feof(file)) {
				npppd_auth_base_log(base, LOG_ERR,
				    "Loading a account list failed: lineno=%d "
				    "line too long", linno + 1);
				goto fail;
			}

			status = csvreader_parse(csv, line);
		} else {
			if (!feof(file)) {
				npppd_auth_base_log(base, LOG_ERR,
				    "Loading a account list failed: %m");
				goto fail;
			}
			status = csvreader_parse_flush(csv);
			eof = 1;
		}
		if (status != CSVREADER_NO_ERROR) {
			if (status == CSVREADER_OUT_OF_MEMORY)
				npppd_auth_base_log(base, LOG_ERR,
				    "Loading a account list failed: %m");
			else
				npppd_auth_base_log(base, LOG_ERR,
				    "Loading a account list "
				    "failed: lineno=%d parse error", linno);
			goto fail;
		}
		ncols = csvreader_get_number_of_column(csv);
		if ((cols = csvreader_get_column(csv)) == NULL)
			continue;
		linno++; /* count up here because line number is treated as CSV. */
		if (linno == 1) {
			/* skip a title line */
			continue;
		}
		if (ncols < 1) {
			npppd_auth_base_log(base, LOG_ERR,
			    "account list lineno=%d has only %d fields.",
			    linno, ncols);
			continue;
		}
		if (strlen(cols[0]) <= 0)
			continue;	/* skip if the user-name is empty */
		if (ncols >= 3) {
			if (*cols[2] != '\0' && inet_aton(cols[2], &ip4) != 1) {
				npppd_auth_base_log(base, LOG_ERR,
				    "account list lineno=%d parse error: "
				    "invalid 'Framed-IP-Address' field: %s",
				    linno, cols[2]);
				continue;
			}
		}
		if (ncols >= 4) {
			if ((*cols[3] != '\0' &&
			    inet_aton(cols[3], &ip4mask) != 1) ||
			    netmask2prefixlen(htonl(ip4mask.s_addr)) < 0) {
				npppd_auth_base_log(base, LOG_ERR,
				    "account list lineno=%d parse error: "
				    "invalid 'Framed-IP-Netmask' field: %s",
				    linno, cols[3]);
				continue;
			}
		}

		passwd = "";
		if (cols[1] != NULL)
			passwd = cols[1];
		callnum = "";
		if (ncols >= 6 && cols[5] != NULL)
			callnum = cols[5];

		usersz = sizeof(npppd_auth_user);
		usersz += strlen(cols[0]) + 1;
		usersz += strlen(passwd) + 1;
		usersz += strlen(callnum) + 1;
		if ((user = malloc(usersz)) == NULL) {
			npppd_auth_base_log(base, LOG_ERR,
			    "Loading a account list failed: %m");
			goto fail;
		}
		memset(user, 0, usersz);

		off = 0;

		user->username = user->space + off;
		off += strlcpy(user->username, cols[0], usersz - off);
		++off;

		user->password = user->space + off;
		off += strlcpy(user->password, passwd, usersz - off);
		++off;

		user->calling_number = user->space + off;
		strlcpy(user->calling_number, callnum, usersz - off);

		user->framed_ip_address = ip4;
		user->framed_ip_netmask = ip4mask;

		slist_add(&users, user);
	}
	hash_delete_all(base->users_hash, 1);

	nuser = 0;
	for (slist_itr_first(&users); slist_itr_has_next(&users);) {
		user = slist_itr_next(&users);
		if (hash_lookup(base->users_hash, user->username) != NULL) {
			npppd_auth_base_log(base, LOG_WARNING,
			    "Record for user='%s' is redefined, the first "
			    "record will be used.",  user->username);
			free(user);
			goto next_user;
		}
		if (hash_insert(base->users_hash, user->username, user) != 0) {
			npppd_auth_base_log(base, LOG_ERR,
			    "Loading a account list failed: hash_insert(): %m");
			goto fail;
		}
		nuser++;
next_user:
		slist_itr_remove(&users);
	}
	slist_fini(&users);
	csvreader_destroy(csv);

	fclose(file);
	npppd_auth_base_log(base, LOG_INFO,
	    "Loaded users from='%s' successfully.  %d users",
	    base->acctlist_path, nuser);
	base->acctlist_ready = 1;

	return 0;
fail:
	fclose(file);
	if (csv != NULL)
		csvreader_destroy(csv);
	hash_delete_all(base->users_hash, 1);
	for (slist_itr_first(&users); slist_itr_has_next(&users);) {
		user = slist_itr_next(&users);
		free(user);
	}
	slist_fini(&users);

	return 1;
}

static npppd_auth_user *
npppd_auth_find_user(npppd_auth_base *base, const char *username)
{
	int lsuffix, lusername;
	const char *un;
	char buf[MAX_USERNAME_LENGTH];
	hash_link *hl;

	un = username;
	lsuffix = strlen(base->pppsuffix);
	if (lsuffix > 0) {
		/* Strip the suffix */
		lusername = strlen(username);
		NPPPD_AUTH_ASSERT(lusername + 1 < sizeof(buf));
		if (lusername + 1 >= sizeof(buf))
			return NULL;
		memcpy(buf, username, lusername - lsuffix);
		buf[lusername - lsuffix] = '\0';
		un = buf;
	}

	if ((hl = hash_lookup(base->users_hash, un)) == NULL)
		return NULL;

	return hl->item;
}

#ifdef USE_NPPPD_RADIUS
/***********************************************************************
 * RADIUS
 ***********************************************************************/

static int
radius_server_address_load(radius_req_setting *radius, int idx,
    const char *address)
{
	struct addrinfo *ai;
	struct sockaddr_in *sin;

	memset(&radius->server[idx], 0, sizeof(radius->server[0]));

	if (addrport_parse(address, IPPROTO_TCP, &ai) !=0)
		return 1;

	switch (ai->ai_family) {
	default:
		freeaddrinfo(ai);
		return 1;
	case AF_INET:
	case AF_INET6:
		break;
	}

	sin = (struct sockaddr_in *)(ai->ai_addr);
	if (sin->sin_port == 0)
		sin->sin_port = htons(DEFAULT_RADIUS_AUTH_PORT);
	memcpy(&radius->server[idx].peer, ai->ai_addr,
	    MIN(sizeof(radius->server[idx].peer), ai->ai_addrlen));

	freeaddrinfo(ai);
	radius->server[idx].enabled = 1;

	return 0;
}

/** reload the configuration of RADIUS authentication realm */
static int
npppd_auth_radius_reload(npppd_auth_base *base)
{
	npppd_auth_radius *_this = (npppd_auth_radius *)base;
	int i, n;
	const char *val;
	char *tok, *buf0, buf[NPPPD_CONFIG_BUFSIZ], logbuf[BUFSIZ];
	char label[256];

#define	VAL_SEP		" \t\r\n"
	n = 0;
	_this->rad_setting.timeout =
	    npppd_auth_config_int(base, "timeout", DEFAULT_RADIUS_AUTH_TIMEOUT);
	_this->rad_setting.curr_server = 0;

	if ((val = npppd_auth_config_str(base, "server_list")) != NULL) {
		strlcpy(buf, val, sizeof(buf));
		buf0 = buf;
		while ((tok = strsep(&buf0, VAL_SEP)) != NULL) {
			if (tok[0] == '\0')
				continue;
			snprintf(label, sizeof(label), "server.%s.address",tok);
			if ((val = npppd_auth_config_str(base, label)) == NULL)
				goto fail;
			if (radius_server_address_load(&_this->rad_setting, n,
			    val) != 0) {
				npppd_auth_base_log(base, LOG_INFO,
				    "parse error at %s", label);
				goto fail;
			}
			snprintf(label, sizeof(label), "server.%s.secret",
			    tok);
			if ((val = npppd_auth_config_str(base, label)) != NULL)
				strlcpy(_this->rad_setting.server[n].secret,
				    val, sizeof(_this->rad_setting
					.server[n].secret));
			else
				_this->rad_setting.server[n].secret[0] = '\0';
			if (n != 0)
				strlcat(logbuf, " ", sizeof(logbuf));
			n++;
		}
	} else if ((val = npppd_auth_config_str(base, "server.address"))
	    != NULL) {
		if (radius_server_address_load(&_this->rad_setting, n, val)
		    != 0) {
			npppd_auth_base_log(base, LOG_INFO,
			    "parse error at %s", label);
			goto fail;
		}
		if ((val = npppd_auth_config_str(base, "server.secret"))!= NULL)
			strlcpy(_this->rad_setting.server[n].secret, val,
			    sizeof(_this->rad_setting.server[n].secret));
		else
			_this->rad_setting.server[n].secret[0] = '\0';
		n++;
	}
	for (i = n; i < countof(_this->rad_setting.server); i++) {
		memset(&_this->rad_setting.server[i], 0,
		    sizeof(_this->rad_setting.server[0]));
	}
	for (i = 0; i < countof(_this->rad_setting.server); i++) {
		if (_this->rad_setting.server[i].enabled)
			base->radius_ready = 1;
	}

	npppd_auth_base_log(base, LOG_INFO,
	    "Loaded configuration timeout=%d nserver=%d",
	    _this->rad_setting.timeout, n);

	return 0;
fail:
	npppd_auth_destroy(base);

	return 1;
}

/**
 * Get {@link ::radius_req_setting} of specified {@link ::npppd_auth_base}
 * object.
 */
void *
npppd_auth_radius_get_radius_req_setting(npppd_auth_radius *_this)
{
	return &_this->rad_setting;
}

/** This function notifies that RADIUS server failed the request. */
void
npppd_auth_radius_server_failure_notify(npppd_auth_radius *_this,
    struct sockaddr *server, const char *reason)
{
	int i, n;
	radius_req_setting *rad_setting;
	char buf0[BUFSIZ];

	NPPPD_AUTH_ASSERT(_this != NULL);
	NPPPD_AUTH_ASSERT(server != NULL);

	if (reason == NULL)
		reason = "failure";

	rad_setting = &_this->rad_setting;
	if (memcmp(&rad_setting->server[rad_setting->curr_server].peer,
	    server, server->sa_len) == 0) {
		/*
		 * The RADIUS server which request was failed is currently selected,
		 * so next RADIUS server will be selected.
		 */
		for (i = 1; i < countof(rad_setting->server); i++) {
			n = (rad_setting->curr_server + i) %
			    countof(rad_setting->server);
			if (rad_setting->server[n].enabled == 0)
				continue;
			rad_setting->curr_server = n;
			break;
		}
	}

	npppd_auth_base_log(&_this->nar_base, LOG_NOTICE,
	    "server=%s request failure: %s",
		addrport_tostring(server, server->sa_len, buf0, sizeof(buf0)),
		reason);
}
#endif

/***********************************************************************
 * Helper functions
 ***********************************************************************/
/** Log it which starts the label based on this instance. */
static int
npppd_auth_base_log(npppd_auth_base *_this, int prio, const char *fmt, ...)
{
	int status;
	char logbuf[BUFSIZ];
	va_list ap;

	NPPPD_AUTH_ASSERT(_this != NULL);
	va_start(ap, fmt);
	snprintf(logbuf, sizeof(logbuf), "realm name=%s(%s) %s",
	    _this->name, (_this->label[0] == '\0')? "default" : _this->label,
	    fmt);
	status = vlog_printf(prio, logbuf, ap);
	va_end(ap);

	return status;
}

static uint32_t
str_hash(const void *ptr, int sz)
{
	u_int32_t hash = 0;
	int i, len;
	const char *str;

	str = ptr;
	len = strlen(str);
	for (i = 0; i < len; i++)
		hash = hash*0x1F + str[i];
	hash = (hash << 16) ^ (hash & 0xffff);

	return hash % sz;
}

static const char *
npppd_auth_default_label(npppd_auth_base *base)
{
	switch(base->type) {
	case NPPPD_AUTH_TYPE_LOCAL:
		return "local";
	case NPPPD_AUTH_TYPE_RADIUS:
		return "radius";
	}
	NPPPD_AUTH_ASSERT(0);

	return NULL;
}

static inline const char *
npppd_auth_config_prefix(npppd_auth_base *base)
{
	switch(base->type) {
	case NPPPD_AUTH_TYPE_LOCAL:
		return "auth.local.realm";

	case NPPPD_AUTH_TYPE_RADIUS:
		return "auth.radius.realm";

	}
	NPPPD_AUTH_ASSERT(0);

	return NULL;
}

static const char  *
npppd_auth_config_str(npppd_auth_base *base, const char *confKey)
{
	return config_named_prefix_str(((npppd *)base->npppd)->properties,
	    npppd_auth_config_prefix(base), base->label, confKey);
}

static int
npppd_auth_config_int(npppd_auth_base *base, const char *confKey, int defVal)
{
	return config_named_prefix_int(((npppd *)base->npppd)->properties,
	    npppd_auth_config_prefix(base), base->label, confKey, defVal);
}

static int
npppd_auth_config_str_equal(npppd_auth_base *base, const char *confKey,
    const char *confVal, int defVal)
{
	return config_named_prefix_str_equal(((npppd *)base->npppd)->properties,
	    npppd_auth_config_prefix(base), base->label, confKey, confVal,
	    defVal);
}
