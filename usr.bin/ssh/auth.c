/* $OpenBSD: auth.c,v 1.111 2015/05/01 04:17:51 djm Exp $ */
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
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <login_cap.h>
#include <paths.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "xmalloc.h"
#include "match.h"
#include "groupaccess.h"
#include "log.h"
#include "buffer.h"
#include "misc.h"
#include "servconf.h"
#include "key.h"
#include "hostfile.h"
#include "auth.h"
#include "auth-options.h"
#include "canohost.h"
#include "uidswap.h"
#include "packet.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "authfile.h"
#include "monitor_wrap.h"
#include "authfile.h"
#include "ssherr.h"
#include "compat.h"

/* import */
extern ServerOptions options;
extern int use_privsep;

/* Debugging messages */
Buffer auth_debug;
int auth_debug_init;

/*
 * Check if the user is allowed to log in via ssh. If user is listed
 * in DenyUsers or one of user's groups is listed in DenyGroups, false
 * will be returned. If AllowUsers isn't empty and user isn't listed
 * there, or if AllowGroups isn't empty and one of user's groups isn't
 * listed there, false will be returned.
 * If the user's shell is not executable, false will be returned.
 * Otherwise true is returned.
 */
int
allowed_user(struct passwd * pw)
{
	struct stat st;
	const char *hostname = NULL, *ipaddr = NULL;
	u_int i;

	/* Shouldn't be called if pw is NULL, but better safe than sorry... */
	if (!pw || !pw->pw_name)
		return 0;

	/*
	 * Deny if shell does not exist or is not executable unless we
	 * are chrooting.
	 */
	if (options.chroot_directory == NULL ||
	    strcasecmp(options.chroot_directory, "none") == 0) {
		char *shell = xstrdup((pw->pw_shell[0] == '\0') ?
		    _PATH_BSHELL : pw->pw_shell); /* empty = /bin/sh */

		if (stat(shell, &st) != 0) {
			logit("User %.100s not allowed because shell %.100s "
			    "does not exist", pw->pw_name, shell);
			free(shell);
			return 0;
		}
		if (S_ISREG(st.st_mode) == 0 ||
		    (st.st_mode & (S_IXOTH|S_IXUSR|S_IXGRP)) == 0) {
			logit("User %.100s not allowed because shell %.100s "
			    "is not executable", pw->pw_name, shell);
			free(shell);
			return 0;
		}
		free(shell);
	}

	if (options.num_deny_users > 0 || options.num_allow_users > 0 ||
	    options.num_deny_groups > 0 || options.num_allow_groups > 0) {
		hostname = get_canonical_hostname(options.use_dns);
		ipaddr = get_remote_ipaddr();
	}

	/* Return false if user is listed in DenyUsers */
	if (options.num_deny_users > 0) {
		for (i = 0; i < options.num_deny_users; i++)
			if (match_user(pw->pw_name, hostname, ipaddr,
			    options.deny_users[i])) {
				logit("User %.100s from %.100s not allowed "
				    "because listed in DenyUsers",
				    pw->pw_name, hostname);
				return 0;
			}
	}
	/* Return false if AllowUsers isn't empty and user isn't listed there */
	if (options.num_allow_users > 0) {
		for (i = 0; i < options.num_allow_users; i++)
			if (match_user(pw->pw_name, hostname, ipaddr,
			    options.allow_users[i]))
				break;
		/* i < options.num_allow_users iff we break for loop */
		if (i >= options.num_allow_users) {
			logit("User %.100s from %.100s not allowed because "
			    "not listed in AllowUsers", pw->pw_name, hostname);
			return 0;
		}
	}
	if (options.num_deny_groups > 0 || options.num_allow_groups > 0) {
		/* Get the user's group access list (primary and supplementary) */
		if (ga_init(pw->pw_name, pw->pw_gid) == 0) {
			logit("User %.100s from %.100s not allowed because "
			    "not in any group", pw->pw_name, hostname);
			return 0;
		}

		/* Return false if one of user's groups is listed in DenyGroups */
		if (options.num_deny_groups > 0)
			if (ga_match(options.deny_groups,
			    options.num_deny_groups)) {
				ga_free();
				logit("User %.100s from %.100s not allowed "
				    "because a group is listed in DenyGroups",
				    pw->pw_name, hostname);
				return 0;
			}
		/*
		 * Return false if AllowGroups isn't empty and one of user's groups
		 * isn't listed there
		 */
		if (options.num_allow_groups > 0)
			if (!ga_match(options.allow_groups,
			    options.num_allow_groups)) {
				ga_free();
				logit("User %.100s from %.100s not allowed "
				    "because none of user's groups are listed "
				    "in AllowGroups", pw->pw_name, hostname);
				return 0;
			}
		ga_free();
	}
	/* We found no reason not to let this user try to log on... */
	return 1;
}

void
auth_info(Authctxt *authctxt, const char *fmt, ...)
{
	va_list ap;
        int i;

	free(authctxt->info);
	authctxt->info = NULL;

	va_start(ap, fmt);
	i = vasprintf(&authctxt->info, fmt, ap);
	va_end(ap);

	if (i < 0 || authctxt->info == NULL)
		fatal("vasprintf failed");
}

void
auth_log(Authctxt *authctxt, int authenticated, int partial,
    const char *method, const char *submethod)
{
	void (*authlog) (const char *fmt,...) = verbose;
	char *authmsg;

	if (use_privsep && !mm_is_monitor() && !authctxt->postponed)
		return;

	/* Raise logging level */
	if (authenticated == 1 ||
	    !authctxt->valid ||
	    authctxt->failures >= options.max_authtries / 2 ||
	    strcmp(method, "password") == 0)
		authlog = logit;

	if (authctxt->postponed)
		authmsg = "Postponed";
	else if (partial)
		authmsg = "Partial";
	else
		authmsg = authenticated ? "Accepted" : "Failed";

	authlog("%s %s%s%s for %s%.100s from %.200s port %d %s%s%s",
	    authmsg,
	    method,
	    submethod != NULL ? "/" : "", submethod == NULL ? "" : submethod,
	    authctxt->valid ? "" : "invalid user ",
	    authctxt->user,
	    get_remote_ipaddr(),
	    get_remote_port(),
	    compat20 ? "ssh2" : "ssh1",
	    authctxt->info != NULL ? ": " : "",
	    authctxt->info != NULL ? authctxt->info : "");
	free(authctxt->info);
	authctxt->info = NULL;
}

void
auth_maxtries_exceeded(Authctxt *authctxt)
{
	error("maximum authentication attempts exceeded for "
	    "%s%.100s from %.200s port %d %s",
	    authctxt->valid ? "" : "invalid user ",
	    authctxt->user,
	    get_remote_ipaddr(),
	    get_remote_port(),
	    compat20 ? "ssh2" : "ssh1");
	packet_disconnect("Too many authentication failures");
	/* NOTREACHED */
}

/*
 * Check whether root logins are disallowed.
 */
int
auth_root_allowed(const char *method)
{
	switch (options.permit_root_login) {
	case PERMIT_YES:
		return 1;
	case PERMIT_NO_PASSWD:
		if (strcmp(method, "password") != 0)
			return 1;
		break;
	case PERMIT_FORCED_ONLY:
		if (forced_command) {
			logit("Root login accepted for forced command.");
			return 1;
		}
		break;
	}
	logit("ROOT LOGIN REFUSED FROM %.200s", get_remote_ipaddr());
	return 0;
}


/*
 * Given a template and a passwd structure, build a filename
 * by substituting % tokenised options. Currently, %% becomes '%',
 * %h becomes the home directory and %u the username.
 *
 * This returns a buffer allocated by xmalloc.
 */
char *
expand_authorized_keys(const char *filename, struct passwd *pw)
{
	char *file, ret[PATH_MAX];
	int i;

	file = percent_expand(filename, "h", pw->pw_dir,
	    "u", pw->pw_name, (char *)NULL);

	/*
	 * Ensure that filename starts anchored. If not, be backward
	 * compatible and prepend the '%h/'
	 */
	if (*file == '/')
		return (file);

	i = snprintf(ret, sizeof(ret), "%s/%s", pw->pw_dir, file);
	if (i < 0 || (size_t)i >= sizeof(ret))
		fatal("expand_authorized_keys: path too long");
	free(file);
	return (xstrdup(ret));
}

char *
authorized_principals_file(struct passwd *pw)
{
	if (options.authorized_principals_file == NULL)
		return NULL;
	return expand_authorized_keys(options.authorized_principals_file, pw);
}

/* return ok if key exists in sysfile or userfile */
HostStatus
check_key_in_hostfiles(struct passwd *pw, Key *key, const char *host,
    const char *sysfile, const char *userfile)
{
	char *user_hostfile;
	struct stat st;
	HostStatus host_status;
	struct hostkeys *hostkeys;
	const struct hostkey_entry *found;

	hostkeys = init_hostkeys();
	load_hostkeys(hostkeys, host, sysfile);
	if (userfile != NULL) {
		user_hostfile = tilde_expand_filename(userfile, pw->pw_uid);
		if (options.strict_modes &&
		    (stat(user_hostfile, &st) == 0) &&
		    ((st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
		    (st.st_mode & 022) != 0)) {
			logit("Authentication refused for %.100s: "
			    "bad owner or modes for %.200s",
			    pw->pw_name, user_hostfile);
			auth_debug_add("Ignored %.200s: bad ownership or modes",
			    user_hostfile);
		} else {
			temporarily_use_uid(pw);
			load_hostkeys(hostkeys, host, user_hostfile);
			restore_uid();
		}
		free(user_hostfile);
	}
	host_status = check_key_in_hostkeys(hostkeys, key, &found);
	if (host_status == HOST_REVOKED)
		error("WARNING: revoked key for %s attempted authentication",
		    found->host);
	else if (host_status == HOST_OK)
		debug("%s: key for %s found at %s:%ld", __func__,
		    found->host, found->file, found->line);
	else
		debug("%s: key for host %s not found", __func__, host);

	free_hostkeys(hostkeys);

	return host_status;
}

/*
 * Check a given path for security. This is defined as all components
 * of the path to the file must be owned by either the owner of
 * of the file or root and no directories must be group or world writable.
 *
 * XXX Should any specific check be done for sym links ?
 *
 * Takes a file name, its stat information (preferably from fstat() to
 * avoid races), the uid of the expected owner, their home directory and an
 * error buffer plus max size as arguments.
 *
 * Returns 0 on success and -1 on failure
 */
int
auth_secure_path(const char *name, struct stat *stp, const char *pw_dir,
    uid_t uid, char *err, size_t errlen)
{
	char buf[PATH_MAX], homedir[PATH_MAX];
	char *cp;
	int comparehome = 0;
	struct stat st;

	if (realpath(name, buf) == NULL) {
		snprintf(err, errlen, "realpath %s failed: %s", name,
		    strerror(errno));
		return -1;
	}
	if (pw_dir != NULL && realpath(pw_dir, homedir) != NULL)
		comparehome = 1;

	if (!S_ISREG(stp->st_mode)) {
		snprintf(err, errlen, "%s is not a regular file", buf);
		return -1;
	}
	if ((stp->st_uid != 0 && stp->st_uid != uid) ||
	    (stp->st_mode & 022) != 0) {
		snprintf(err, errlen, "bad ownership or modes for file %s",
		    buf);
		return -1;
	}

	/* for each component of the canonical path, walking upwards */
	for (;;) {
		if ((cp = dirname(buf)) == NULL) {
			snprintf(err, errlen, "dirname() failed");
			return -1;
		}
		strlcpy(buf, cp, sizeof(buf));

		if (stat(buf, &st) < 0 ||
		    (st.st_uid != 0 && st.st_uid != uid) ||
		    (st.st_mode & 022) != 0) {
			snprintf(err, errlen,
			    "bad ownership or modes for directory %s", buf);
			return -1;
		}

		/* If are past the homedir then we can stop */
		if (comparehome && strcmp(homedir, buf) == 0)
			break;

		/*
		 * dirname should always complete with a "/" path,
		 * but we can be paranoid and check for "." too
		 */
		if ((strcmp("/", buf) == 0) || (strcmp(".", buf) == 0))
			break;
	}
	return 0;
}

/*
 * Version of secure_path() that accepts an open file descriptor to
 * avoid races.
 *
 * Returns 0 on success and -1 on failure
 */
static int
secure_filename(FILE *f, const char *file, struct passwd *pw,
    char *err, size_t errlen)
{
	struct stat st;

	/* check the open file to avoid races */
	if (fstat(fileno(f), &st) < 0) {
		snprintf(err, errlen, "cannot stat file %s: %s",
		    file, strerror(errno));
		return -1;
	}
	return auth_secure_path(file, &st, pw->pw_dir, pw->pw_uid, err, errlen);
}

static FILE *
auth_openfile(const char *file, struct passwd *pw, int strict_modes,
    int log_missing, char *file_type)
{
	char line[1024];
	struct stat st;
	int fd;
	FILE *f;

	if ((fd = open(file, O_RDONLY|O_NONBLOCK)) == -1) {
		if (log_missing || errno != ENOENT)
			debug("Could not open %s '%s': %s", file_type, file,
			   strerror(errno));
		return NULL;
	}

	if (fstat(fd, &st) < 0) {
		close(fd);
		return NULL;
	}
	if (!S_ISREG(st.st_mode)) {
		logit("User %s %s %s is not a regular file",
		    pw->pw_name, file_type, file);
		close(fd);
		return NULL;
	}
	unset_nonblock(fd);
	if ((f = fdopen(fd, "r")) == NULL) {
		close(fd);
		return NULL;
	}
	if (strict_modes &&
	    secure_filename(f, file, pw, line, sizeof(line)) != 0) {
		fclose(f);
		logit("Authentication refused: %s", line);
		auth_debug_add("Ignored %s: %s", file_type, line);
		return NULL;
	}

	return f;
}


FILE *
auth_openkeyfile(const char *file, struct passwd *pw, int strict_modes)
{
	return auth_openfile(file, pw, strict_modes, 1, "authorized keys");
}

FILE *
auth_openprincipals(const char *file, struct passwd *pw, int strict_modes)
{
	return auth_openfile(file, pw, strict_modes, 0,
	    "authorized principals");
}

struct passwd *
getpwnamallow(const char *user)
{
	extern login_cap_t *lc;
	auth_session_t *as;
	struct passwd *pw;
	struct connection_info *ci = get_connection_info(1, options.use_dns);

	ci->user = user;
	parse_server_match_config(&options, ci);

	pw = getpwnam(user);
	if (pw == NULL) {
		logit("Invalid user %.100s from %.100s",
		    user, get_remote_ipaddr());
		return (NULL);
	}
	if (!allowed_user(pw))
		return (NULL);
	if ((lc = login_getclass(pw->pw_class)) == NULL) {
		debug("unable to get login class: %s", user);
		return (NULL);
	}
	if ((as = auth_open()) == NULL || auth_setpwd(as, pw) != 0 ||
	    auth_approval(as, lc, pw->pw_name, "ssh") <= 0) {
		debug("Approval failure for %s", user);
		pw = NULL;
	}
	if (as != NULL)
		auth_close(as);
	if (pw != NULL)
		return (pwcopy(pw));
	return (NULL);
}

/* Returns 1 if key is revoked by revoked_keys_file, 0 otherwise */
int
auth_key_is_revoked(Key *key)
{
	char *fp = NULL;
	int r;

	if (options.revoked_keys_file == NULL)
		return 0;
	if ((fp = sshkey_fingerprint(key, options.fingerprint_hash,
	    SSH_FP_DEFAULT)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		error("%s: fingerprint key: %s", __func__, ssh_err(r));
		goto out;
	}

	r = sshkey_check_revoked(key, options.revoked_keys_file);
	switch (r) {
	case 0:
		break; /* not revoked */
	case SSH_ERR_KEY_REVOKED:
		error("Authentication key %s %s revoked by file %s",
		    sshkey_type(key), fp, options.revoked_keys_file);
		goto out;
	default:
		error("Error checking authentication key %s %s in "
		    "revoked keys file %s: %s", sshkey_type(key), fp,
		    options.revoked_keys_file, ssh_err(r));
		goto out;
	}

	/* Success */
	r = 0;

 out:
	free(fp);
	return r == 0 ? 0 : 1;
}

void
auth_debug_add(const char *fmt,...)
{
	char buf[1024];
	va_list args;

	if (!auth_debug_init)
		return;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	buffer_put_cstring(&auth_debug, buf);
}

void
auth_debug_send(void)
{
	char *msg;

	if (!auth_debug_init)
		return;
	while (buffer_len(&auth_debug)) {
		msg = buffer_get_string(&auth_debug, NULL);
		packet_send_debug("%s", msg);
		free(msg);
	}
}

void
auth_debug_reset(void)
{
	if (auth_debug_init)
		buffer_clear(&auth_debug);
	else {
		buffer_init(&auth_debug);
		auth_debug_init = 1;
	}
}

struct passwd *
fakepw(void)
{
	static struct passwd fake;

	memset(&fake, 0, sizeof(fake));
	fake.pw_name = "NOUSER";
	fake.pw_passwd =
	    "$2a$06$r3.juUaHZDlIbQaO2dS9FuYxL1W9M81R1Tc92PoSNmzvpEqLkLGrK";
	fake.pw_gecos = "NOUSER";
	fake.pw_uid = (uid_t)-1;
	fake.pw_gid = (gid_t)-1;
	fake.pw_class = "";
	fake.pw_dir = "/nonexist";
	fake.pw_shell = "/nonexist";

	return (&fake);
}
