/* $OpenBSD: pop_root.c,v 1.4 2005/01/18 19:13:27 otto Exp $ */

/*
 * Main daemon code: invokes the actual POP handling routines. Most calls
 * to functions in other source files are done as a non-root user (either
 * POP_USER or the authenticated user). Depending on compile-time options
 * in params.h, the following files may contain code executed as root:
 *
 * startup.c		if supporting command line options (POP_OPTIONS)
 * standalone.c		if not running via an inetd clone (POP_STANDALONE)
 * virtual.c		if supporting virtual domains (POP_VIRTUAL)
 * auth_passwd.c	if using passwd or *BSD (AUTH_PASSWD && !VIRTUAL_ONLY)
 */

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "params.h"
#include "protocol.h"
#include "pop_auth.h"
#include "pop_trans.h"
#if POP_VIRTUAL
#include "virtual.h"
#endif

#if !VIRTUAL_ONLY
extern struct passwd *auth_userpass(char *user, char *pass, int *known);
#endif

/* POP_USER's pw_uid and pw_gid, other fields may not be valid */
static struct passwd pop_pw;

static int known;
static char *user;
static char *spool, *mailbox;

int log_error(char *s)
{
	syslog(SYSLOG_PRI_ERROR, "%s: %m", s);
	return 1;
}

static int set_user(struct passwd *pw)
{
	gid_t groups[2];

	if (!pw->pw_uid) return 1;

	groups[0] = groups[1] = pw->pw_gid;
	if (setgroups(1, groups)) return log_error("setgroups");
	if (setegid(pw->pw_gid)) return log_error("setegid");
	if (setgid(pw->pw_gid)) return log_error("setgid");
	if (seteuid(pw->pw_uid)) return log_error("seteuid");
	if (setuid(pw->pw_uid)) return log_error("setuid");

	return 0;
}

static int drop_root(void)
{
	tzset();
	openlog(SYSLOG_IDENT, SYSLOG_OPTIONS | LOG_NDELAY, SYSLOG_FACILITY);

	if (chroot(POP_CHROOT)) return log_error("chroot");
	if (chdir("/")) return log_error("chdir");

	return set_user(&pop_pw);
}

/*
 * Attempts to read until EOF, and returns the number of bytes read.
 * We don't expect any signals, so even EINTR is considered an error.
 */
static int read_loop(int fd, char *buffer, int count)
{
	int offset, block;

	offset = 0;
	while (count > 0) {
		block = read(fd, &buffer[offset], count);

		if (block < 0) return block;
		if (!block) return offset;

		offset += block;
		count -= block;
	}

	return offset;
}

/*
 * The root-privileged part of the AUTHORIZATION state handling: reads
 * the authentication data obtained over POP from its end of the pipe,
 * attempts authentication, and, if successful, drops privilege to the
 * authenticated user. Returns one of the AUTH_* result codes.
 */
static int do_root_auth(int channel)
{
	static char auth[AUTH_BUFFER_SIZE + 2];
	char *pass;
	struct passwd *pw;

	known = 0;
#if POP_VIRTUAL
	virtual_domain = NULL;
#endif

/* The POP client could have sent extra commands without waiting for
 * successful authentication. We're passing them into the TRANSACTION
 * state if we ever get there. */
	if (read_loop(channel, (char *)&pop_buffer, sizeof(pop_buffer)) !=
	    sizeof(pop_buffer)) return AUTH_NONE;

/* Now, the authentication data. */
	memset(auth, 0, sizeof(auth));	/* Ensure NUL termination */
	if (read_loop(channel, auth, AUTH_BUFFER_SIZE) < 0) {
		memset(auth, 0, sizeof(auth));
		return AUTH_NONE;
	}

	user = auth;
	pass = &user[strlen(user) + 1];

	pw = NULL;
#if POP_VIRTUAL
	if (!(pw = virtual_userpass(user, pass, &known)) && virtual_domain) {
		memset(pass, 0, strlen(pass));
		return AUTH_FAILED;
	}
#endif
#if VIRTUAL_ONLY
	if (!pw) {
		memset(pass, 0, strlen(pass));
		return AUTH_FAILED;
	}
#else
	if (!pw && !(pw = auth_userpass(user, pass, &known))) {
		memset(pass, 0, strlen(pass));
		return AUTH_FAILED;
	}
#endif
	if (!*pass) return AUTH_FAILED;
	memset(pass, 0, strlen(pass));
	if (!*user) return AUTH_FAILED;

#if VIRTUAL_ONLY
	if (!virtual_domain) return AUTH_FAILED;
#endif

	if (set_user(pw)) return AUTH_FAILED;

#if POP_VIRTUAL
	if (virtual_domain) {
		spool = virtual_spool;
		mailbox = user;

		return AUTH_OK;
	}
#endif

#if VIRTUAL_ONLY
	/* never reached */
	return AUTH_FAILED;
#else
#ifdef MAIL_SPOOL_PATH
	spool = MAIL_SPOOL_PATH;
	mailbox = user;
#else
	spool = pw->pw_dir;
	mailbox = HOME_MAILBOX_NAME;
#endif

	return AUTH_OK;
#endif
}

int do_pop_startup(void)
{
	struct passwd *pw;

	umask(077);
	signal(SIGPIPE, SIG_IGN);

	openlog(SYSLOG_IDENT, SYSLOG_OPTIONS, SYSLOG_FACILITY);

	errno = 0;
	if (!(pw = getpwnam(POP_USER))) {
		syslog(SYSLOG_PRI_ERROR, "getpwnam(\"" POP_USER "\"): %s",
			errno ? strerror(errno) : "No such user");
		return 1;
	}
	memset(pw->pw_passwd, 0, strlen(pw->pw_passwd));
	endpwent();
	pop_pw.pw_uid = pw->pw_uid;
	pop_pw.pw_gid = pw->pw_gid;

#if POP_VIRTUAL
	if (virtual_startup()) return 1;
#endif

	return 0;
}

int do_pop_session(void)
{
	int channel[2];
	int result, status;

/* For SIGCHLD, default action is to ignore the signal. {SIGCHLD, SIG_IGN}
 * may be invalid (POSIX) or may enable a different behavior (SUSv2), none
 * of which are any good for us. */
	signal(SIGCHLD, SIG_DFL);

	if (pipe(channel)) return log_error("pipe");

	switch (fork()) {
	case -1:
		return log_error("fork");

	case 0:
		if (close(channel[0])) return log_error("close");
		if (drop_root()) return 1;
		return do_pop_auth(channel[1]);
	}

	if (close(channel[1]))
		result = AUTH_NONE;
	else
		result = do_root_auth(channel[0]);

	if (wait(&status) < 0)
		status = 1;
	else
	if (WIFEXITED(status))
		status = WEXITSTATUS(status);
	else
		status = 1;

	if (result == AUTH_OK) {
		if (close(channel[0])) return log_error("close");
		log_pop_auth(result, user);
		return do_pop_trans(spool, mailbox);
	}

	if (drop_root()) return 1;
	log_pop_auth(result, known ? user : NULL);

#ifdef AUTH_FAILED_MESSAGE
	if (result == AUTH_FAILED) pop_reply("-ERR %s", AUTH_FAILED_MESSAGE);
#else
	if (result == AUTH_FAILED) pop_reply_error();
#endif

	return status;
}

#if !POP_STANDALONE && !POP_OPTIONS
int main(void)
{
	if (do_pop_startup()) return 1;
	return do_pop_session();
}
#endif
