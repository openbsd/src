/* $OpenBSD: session.c,v 1.315 2019/02/22 03:37:11 djm Exp $ */
/*
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * SSH2 support by Markus Friedl.
 * Copyright (c) 2000, 2001 Markus Friedl.  All rights reserved.
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
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <login_cap.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "xmalloc.h"
#include "ssh.h"
#include "ssh2.h"
#include "sshpty.h"
#include "packet.h"
#include "sshbuf.h"
#include "ssherr.h"
#include "match.h"
#include "uidswap.h"
#include "compat.h"
#include "channels.h"
#include "sshkey.h"
#include "cipher.h"
#include "kex.h"
#include "hostfile.h"
#include "auth.h"
#include "auth-options.h"
#include "authfd.h"
#include "pathnames.h"
#include "log.h"
#include "misc.h"
#include "servconf.h"
#include "sshlogin.h"
#include "serverloop.h"
#include "canohost.h"
#include "session.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "monitor_wrap.h"
#include "sftp.h"
#include "atomicio.h"

#ifdef KRB5
#include <kafs.h>
#endif

#define IS_INTERNAL_SFTP(c) \
	(!strncmp(c, INTERNAL_SFTP_NAME, sizeof(INTERNAL_SFTP_NAME) - 1) && \
	 (c[sizeof(INTERNAL_SFTP_NAME) - 1] == '\0' || \
	  c[sizeof(INTERNAL_SFTP_NAME) - 1] == ' ' || \
	  c[sizeof(INTERNAL_SFTP_NAME) - 1] == '\t'))

/* func */

Session *session_new(void);
void	session_set_fds(struct ssh *, Session *, int, int, int, int, int);
void	session_pty_cleanup(Session *);
void	session_proctitle(Session *);
int	session_setup_x11fwd(struct ssh *, Session *);
int	do_exec_pty(struct ssh *, Session *, const char *);
int	do_exec_no_pty(struct ssh *, Session *, const char *);
int	do_exec(struct ssh *, Session *, const char *);
void	do_login(struct ssh *, Session *, const char *);
void	do_child(struct ssh *, Session *, const char *);
void	do_motd(void);
int	check_quietlogin(Session *, const char *);

static void do_authenticated2(struct ssh *, Authctxt *);

static int session_pty_req(struct ssh *, Session *);

/* import */
extern ServerOptions options;
extern char *__progname;
extern int debug_flag;
extern u_int utmp_len;
extern int startup_pipe;
extern void destroy_sensitive_data(void);
extern struct sshbuf *loginmsg;
extern struct sshauthopt *auth_opts;
extern char *tun_fwd_ifnames; /* serverloop.c */

/* original command from peer. */
const char *original_command = NULL;

/* data */
static int sessions_first_unused = -1;
static int sessions_nalloc = 0;
static Session *sessions = NULL;

#define SUBSYSTEM_NONE			0
#define SUBSYSTEM_EXT			1
#define SUBSYSTEM_INT_SFTP		2
#define SUBSYSTEM_INT_SFTP_ERROR	3

login_cap_t *lc;

static int is_child = 0;
static int in_chroot = 0;

/* File containing userauth info, if ExposeAuthInfo set */
static char *auth_info_file = NULL;

/* Name and directory of socket for authentication agent forwarding. */
static char *auth_sock_name = NULL;
static char *auth_sock_dir = NULL;

/* removes the agent forwarding socket */

static void
auth_sock_cleanup_proc(struct passwd *pw)
{
	if (auth_sock_name != NULL) {
		temporarily_use_uid(pw);
		unlink(auth_sock_name);
		rmdir(auth_sock_dir);
		auth_sock_name = NULL;
		restore_uid();
	}
}

static int
auth_input_request_forwarding(struct ssh *ssh, struct passwd * pw)
{
	Channel *nc;
	int sock = -1;

	if (auth_sock_name != NULL) {
		error("authentication forwarding requested twice.");
		return 0;
	}

	/* Temporarily drop privileged uid for mkdir/bind. */
	temporarily_use_uid(pw);

	/* Allocate a buffer for the socket name, and format the name. */
	auth_sock_dir = xstrdup("/tmp/ssh-XXXXXXXXXX");

	/* Create private directory for socket */
	if (mkdtemp(auth_sock_dir) == NULL) {
		ssh_packet_send_debug(ssh, "Agent forwarding disabled: "
		    "mkdtemp() failed: %.100s", strerror(errno));
		restore_uid();
		free(auth_sock_dir);
		auth_sock_dir = NULL;
		goto authsock_err;
	}

	xasprintf(&auth_sock_name, "%s/agent.%ld",
	    auth_sock_dir, (long) getpid());

	/* Start a Unix listener on auth_sock_name. */
	sock = unix_listener(auth_sock_name, SSH_LISTEN_BACKLOG, 0);

	/* Restore the privileged uid. */
	restore_uid();

	/* Check for socket/bind/listen failure. */
	if (sock < 0)
		goto authsock_err;

	/* Allocate a channel for the authentication agent socket. */
	nc = channel_new(ssh, "auth socket",
	    SSH_CHANNEL_AUTH_SOCKET, sock, sock, -1,
	    CHAN_X11_WINDOW_DEFAULT, CHAN_X11_PACKET_DEFAULT,
	    0, "auth socket", 1);
	nc->path = xstrdup(auth_sock_name);
	return 1;

 authsock_err:
	free(auth_sock_name);
	if (auth_sock_dir != NULL) {
		temporarily_use_uid(pw);
		rmdir(auth_sock_dir);
		restore_uid();
		free(auth_sock_dir);
	}
	if (sock != -1)
		close(sock);
	auth_sock_name = NULL;
	auth_sock_dir = NULL;
	return 0;
}

static void
display_loginmsg(void)
{
	int r;

	if (sshbuf_len(loginmsg) == 0)
		return;
	if ((r = sshbuf_put_u8(loginmsg, 0)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	printf("%s", (char *)sshbuf_ptr(loginmsg));
	sshbuf_reset(loginmsg);
}

static void
prepare_auth_info_file(struct passwd *pw, struct sshbuf *info)
{
	int fd = -1, success = 0;

	if (!options.expose_userauth_info || info == NULL)
		return;

	temporarily_use_uid(pw);
	auth_info_file = xstrdup("/tmp/sshauth.XXXXXXXXXXXXXXX");
	if ((fd = mkstemp(auth_info_file)) == -1) {
		error("%s: mkstemp: %s", __func__, strerror(errno));
		goto out;
	}
	if (atomicio(vwrite, fd, sshbuf_mutable_ptr(info),
	    sshbuf_len(info)) != sshbuf_len(info)) {
		error("%s: write: %s", __func__, strerror(errno));
		goto out;
	}
	if (close(fd) != 0) {
		error("%s: close: %s", __func__, strerror(errno));
		goto out;
	}
	success = 1;
 out:
	if (!success) {
		if (fd != -1)
			close(fd);
		free(auth_info_file);
		auth_info_file = NULL;
	}
	restore_uid();
}

static void
set_fwdpermit_from_authopts(struct ssh *ssh, const struct sshauthopt *opts)
{
	char *tmp, *cp, *host;
	int port;
	size_t i;

	if ((options.allow_tcp_forwarding & FORWARD_LOCAL) != 0) {
		channel_clear_permission(ssh, FORWARD_USER, FORWARD_LOCAL);
		for (i = 0; i < auth_opts->npermitopen; i++) {
			tmp = cp = xstrdup(auth_opts->permitopen[i]);
			/* This shouldn't fail as it has already been checked */
			if ((host = hpdelim(&cp)) == NULL)
				fatal("%s: internal error: hpdelim", __func__);
			host = cleanhostname(host);
			if (cp == NULL || (port = permitopen_port(cp)) < 0)
				fatal("%s: internal error: permitopen port",
				    __func__);
			channel_add_permission(ssh,
			    FORWARD_USER, FORWARD_LOCAL, host, port);
			free(tmp);
		}
	}
	if ((options.allow_tcp_forwarding & FORWARD_REMOTE) != 0) {
		channel_clear_permission(ssh, FORWARD_USER, FORWARD_REMOTE);
		for (i = 0; i < auth_opts->npermitlisten; i++) {
			tmp = cp = xstrdup(auth_opts->permitlisten[i]);
			/* This shouldn't fail as it has already been checked */
			if ((host = hpdelim(&cp)) == NULL)
				fatal("%s: internal error: hpdelim", __func__);
			host = cleanhostname(host);
			if (cp == NULL || (port = permitopen_port(cp)) < 0)
				fatal("%s: internal error: permitlisten port",
				    __func__);
			channel_add_permission(ssh,
			    FORWARD_USER, FORWARD_REMOTE, host, port);
			free(tmp);
		}
	}
}

void
do_authenticated(struct ssh *ssh, Authctxt *authctxt)
{
	setproctitle("%s", authctxt->pw->pw_name);

	auth_log_authopts("active", auth_opts, 0);

	/* setup the channel layer */
	/* XXX - streamlocal? */
	set_fwdpermit_from_authopts(ssh, auth_opts);

	if (!auth_opts->permit_port_forwarding_flag ||
	    options.disable_forwarding) {
		channel_disable_admin(ssh, FORWARD_LOCAL);
		channel_disable_admin(ssh, FORWARD_REMOTE);
	} else {
		if ((options.allow_tcp_forwarding & FORWARD_LOCAL) == 0)
			channel_disable_admin(ssh, FORWARD_LOCAL);
		else
			channel_permit_all(ssh, FORWARD_LOCAL);
		if ((options.allow_tcp_forwarding & FORWARD_REMOTE) == 0)
			channel_disable_admin(ssh, FORWARD_REMOTE);
		else
			channel_permit_all(ssh, FORWARD_REMOTE);
	}
	auth_debug_send(ssh);

	prepare_auth_info_file(authctxt->pw, authctxt->session_info);

	do_authenticated2(ssh, authctxt);

	do_cleanup(ssh, authctxt);
}

/* Check untrusted xauth strings for metacharacters */
static int
xauth_valid_string(const char *s)
{
	size_t i;

	for (i = 0; s[i] != '\0'; i++) {
		if (!isalnum((u_char)s[i]) &&
		    s[i] != '.' && s[i] != ':' && s[i] != '/' &&
		    s[i] != '-' && s[i] != '_')
			return 0;
	}
	return 1;
}

#define USE_PIPES 1
/*
 * This is called to fork and execute a command when we have no tty.  This
 * will call do_child from the child, and server_loop from the parent after
 * setting up file descriptors and such.
 */
int
do_exec_no_pty(struct ssh *ssh, Session *s, const char *command)
{
	pid_t pid;
#ifdef USE_PIPES
	int pin[2], pout[2], perr[2];

	if (s == NULL)
		fatal("do_exec_no_pty: no session");

	/* Allocate pipes for communicating with the program. */
	if (pipe(pin) < 0) {
		error("%s: pipe in: %.100s", __func__, strerror(errno));
		return -1;
	}
	if (pipe(pout) < 0) {
		error("%s: pipe out: %.100s", __func__, strerror(errno));
		close(pin[0]);
		close(pin[1]);
		return -1;
	}
	if (pipe(perr) < 0) {
		error("%s: pipe err: %.100s", __func__,
		    strerror(errno));
		close(pin[0]);
		close(pin[1]);
		close(pout[0]);
		close(pout[1]);
		return -1;
	}
#else
	int inout[2], err[2];

	if (s == NULL)
		fatal("do_exec_no_pty: no session");

	/* Uses socket pairs to communicate with the program. */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, inout) < 0) {
		error("%s: socketpair #1: %.100s", __func__, strerror(errno));
		return -1;
	}
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, err) < 0) {
		error("%s: socketpair #2: %.100s", __func__,
		    strerror(errno));
		close(inout[0]);
		close(inout[1]);
		return -1;
	}
#endif

	session_proctitle(s);

	/* Fork the child. */
	switch ((pid = fork())) {
	case -1:
		error("%s: fork: %.100s", __func__, strerror(errno));
#ifdef USE_PIPES
		close(pin[0]);
		close(pin[1]);
		close(pout[0]);
		close(pout[1]);
		close(perr[0]);
		close(perr[1]);
#else
		close(inout[0]);
		close(inout[1]);
		close(err[0]);
		close(err[1]);
#endif
		return -1;
	case 0:
		is_child = 1;

		/*
		 * Create a new session and process group since the 4.4BSD
		 * setlogin() affects the entire process group.
		 */
		if (setsid() < 0)
			error("setsid failed: %.100s", strerror(errno));

#ifdef USE_PIPES
		/*
		 * Redirect stdin.  We close the parent side of the socket
		 * pair, and make the child side the standard input.
		 */
		close(pin[1]);
		if (dup2(pin[0], 0) < 0)
			perror("dup2 stdin");
		close(pin[0]);

		/* Redirect stdout. */
		close(pout[0]);
		if (dup2(pout[1], 1) < 0)
			perror("dup2 stdout");
		close(pout[1]);

		/* Redirect stderr. */
		close(perr[0]);
		if (dup2(perr[1], 2) < 0)
			perror("dup2 stderr");
		close(perr[1]);
#else
		/*
		 * Redirect stdin, stdout, and stderr.  Stdin and stdout will
		 * use the same socket, as some programs (particularly rdist)
		 * seem to depend on it.
		 */
		close(inout[1]);
		close(err[1]);
		if (dup2(inout[0], 0) < 0)	/* stdin */
			perror("dup2 stdin");
		if (dup2(inout[0], 1) < 0)	/* stdout (same as stdin) */
			perror("dup2 stdout");
		close(inout[0]);
		if (dup2(err[0], 2) < 0)	/* stderr */
			perror("dup2 stderr");
		close(err[0]);
#endif

		/* Do processing for the child (exec command etc). */
		do_child(ssh, s, command);
		/* NOTREACHED */
	default:
		break;
	}

	s->pid = pid;
	/* Set interactive/non-interactive mode. */
	ssh_packet_set_interactive(ssh, s->display != NULL,
	    options.ip_qos_interactive, options.ip_qos_bulk);

#ifdef USE_PIPES
	/* We are the parent.  Close the child sides of the pipes. */
	close(pin[0]);
	close(pout[1]);
	close(perr[1]);

	session_set_fds(ssh, s, pin[1], pout[0], perr[0],
	    s->is_subsystem, 0);
#else
	/* We are the parent.  Close the child sides of the socket pairs. */
	close(inout[0]);
	close(err[0]);

	/*
	 * Enter the interactive session.  Note: server_loop must be able to
	 * handle the case that fdin and fdout are the same.
	 */
	session_set_fds(ssh, s, inout[1], inout[1], err[1],
	    s->is_subsystem, 0);
#endif
	return 0;
}

/*
 * This is called to fork and execute a command when we have a tty.  This
 * will call do_child from the child, and server_loop from the parent after
 * setting up file descriptors, controlling tty, updating wtmp, utmp,
 * lastlog, and other such operations.
 */
int
do_exec_pty(struct ssh *ssh, Session *s, const char *command)
{
	int fdout, ptyfd, ttyfd, ptymaster;
	pid_t pid;

	if (s == NULL)
		fatal("do_exec_pty: no session");
	ptyfd = s->ptyfd;
	ttyfd = s->ttyfd;

	/*
	 * Create another descriptor of the pty master side for use as the
	 * standard input.  We could use the original descriptor, but this
	 * simplifies code in server_loop.  The descriptor is bidirectional.
	 * Do this before forking (and cleanup in the child) so as to
	 * detect and gracefully fail out-of-fd conditions.
	 */
	if ((fdout = dup(ptyfd)) < 0) {
		error("%s: dup #1: %s", __func__, strerror(errno));
		close(ttyfd);
		close(ptyfd);
		return -1;
	}
	/* we keep a reference to the pty master */
	if ((ptymaster = dup(ptyfd)) < 0) {
		error("%s: dup #2: %s", __func__, strerror(errno));
		close(ttyfd);
		close(ptyfd);
		close(fdout);
		return -1;
	}

	/* Fork the child. */
	switch ((pid = fork())) {
	case -1:
		error("%s: fork: %.100s", __func__, strerror(errno));
		close(fdout);
		close(ptymaster);
		close(ttyfd);
		close(ptyfd);
		return -1;
	case 0:
		is_child = 1;

		close(fdout);
		close(ptymaster);

		/* Close the master side of the pseudo tty. */
		close(ptyfd);

		/* Make the pseudo tty our controlling tty. */
		pty_make_controlling_tty(&ttyfd, s->tty);

		/* Redirect stdin/stdout/stderr from the pseudo tty. */
		if (dup2(ttyfd, 0) < 0)
			error("dup2 stdin: %s", strerror(errno));
		if (dup2(ttyfd, 1) < 0)
			error("dup2 stdout: %s", strerror(errno));
		if (dup2(ttyfd, 2) < 0)
			error("dup2 stderr: %s", strerror(errno));

		/* Close the extra descriptor for the pseudo tty. */
		close(ttyfd);

		/* record login, etc. similar to login(1) */
		do_login(ssh, s, command);

		/*
		 * Do common processing for the child, such as execing
		 * the command.
		 */
		do_child(ssh, s, command);
		/* NOTREACHED */
	default:
		break;
	}
	s->pid = pid;

	/* Parent.  Close the slave side of the pseudo tty. */
	close(ttyfd);

	/* Enter interactive session. */
	s->ptymaster = ptymaster;
	ssh_packet_set_interactive(ssh, 1,
	    options.ip_qos_interactive, options.ip_qos_bulk);
	session_set_fds(ssh, s, ptyfd, fdout, -1, 1, 1);
	return 0;
}

/*
 * This is called to fork and execute a command.  If another command is
 * to be forced, execute that instead.
 */
int
do_exec(struct ssh *ssh, Session *s, const char *command)
{
	int ret;
	const char *forced = NULL, *tty = NULL;
	char session_type[1024];

	if (options.adm_forced_command) {
		original_command = command;
		command = options.adm_forced_command;
		forced = "(config)";
	} else if (auth_opts->force_command != NULL) {
		original_command = command;
		command = auth_opts->force_command;
		forced = "(key-option)";
	}
	s->forced = 0;
	if (forced != NULL) {
		s->forced = 1;
		if (IS_INTERNAL_SFTP(command)) {
			s->is_subsystem = s->is_subsystem ?
			    SUBSYSTEM_INT_SFTP : SUBSYSTEM_INT_SFTP_ERROR;
		} else if (s->is_subsystem)
			s->is_subsystem = SUBSYSTEM_EXT;
		snprintf(session_type, sizeof(session_type),
		    "forced-command %s '%.900s'", forced, command);
	} else if (s->is_subsystem) {
		snprintf(session_type, sizeof(session_type),
		    "subsystem '%.900s'", s->subsys);
	} else if (command == NULL) {
		snprintf(session_type, sizeof(session_type), "shell");
	} else {
		/* NB. we don't log unforced commands to preserve privacy */
		snprintf(session_type, sizeof(session_type), "command");
	}

	if (s->ttyfd != -1) {
		tty = s->tty;
		if (strncmp(tty, "/dev/", 5) == 0)
			tty += 5;
	}

	verbose("Starting session: %s%s%s for %s from %.200s port %d id %d",
	    session_type,
	    tty == NULL ? "" : " on ",
	    tty == NULL ? "" : tty,
	    s->pw->pw_name,
	    ssh_remote_ipaddr(ssh),
	    ssh_remote_port(ssh),
	    s->self);

#ifdef GSSAPI
	if (options.gss_authentication) {
		temporarily_use_uid(s->pw);
		ssh_gssapi_storecreds();
		restore_uid();
	}
#endif
	if (s->ttyfd != -1)
		ret = do_exec_pty(ssh, s, command);
	else
		ret = do_exec_no_pty(ssh, s, command);

	original_command = NULL;

	/*
	 * Clear loginmsg: it's the child's responsibility to display
	 * it to the user, otherwise multiple sessions may accumulate
	 * multiple copies of the login messages.
	 */
	sshbuf_reset(loginmsg);

	return ret;
}


/* administrative, login(1)-like work */
void
do_login(struct ssh *ssh, Session *s, const char *command)
{
	socklen_t fromlen;
	struct sockaddr_storage from;
	struct passwd * pw = s->pw;
	pid_t pid = getpid();

	/*
	 * Get IP address of client. If the connection is not a socket, let
	 * the address be 0.0.0.0.
	 */
	memset(&from, 0, sizeof(from));
	fromlen = sizeof(from);
	if (ssh_packet_connection_is_on_socket(ssh)) {
		if (getpeername(ssh_packet_get_connection_in(ssh),
		    (struct sockaddr *)&from, &fromlen) < 0) {
			debug("getpeername: %.100s", strerror(errno));
			cleanup_exit(255);
		}
	}

	/* Record that there was a login on that tty from the remote host. */
	if (!use_privsep)
		record_login(pid, s->tty, pw->pw_name, pw->pw_uid,
		    session_get_remote_name_or_ip(ssh, utmp_len,
		    options.use_dns),
		    (struct sockaddr *)&from, fromlen);

	if (check_quietlogin(s, command))
		return;

	display_loginmsg();

	do_motd();
}

/*
 * Display the message of the day.
 */
void
do_motd(void)
{
	FILE *f;
	char buf[256];

	if (options.print_motd) {
		f = fopen(login_getcapstr(lc, "welcome", "/etc/motd",
		    "/etc/motd"), "r");
		if (f) {
			while (fgets(buf, sizeof(buf), f))
				fputs(buf, stdout);
			fclose(f);
		}
	}
}


/*
 * Check for quiet login, either .hushlogin or command given.
 */
int
check_quietlogin(Session *s, const char *command)
{
	char buf[256];
	struct passwd *pw = s->pw;
	struct stat st;

	/* Return 1 if .hushlogin exists or a command given. */
	if (command != NULL)
		return 1;
	snprintf(buf, sizeof(buf), "%.200s/.hushlogin", pw->pw_dir);
	if (login_getcapbool(lc, "hushlogin", 0) || stat(buf, &st) >= 0)
		return 1;
	return 0;
}

/*
 * Reads environment variables from the given file and adds/overrides them
 * into the environment.  If the file does not exist, this does nothing.
 * Otherwise, it must consist of empty lines, comments (line starts with '#')
 * and assignments of the form name=value.  No other forms are allowed.
 * If whitelist is not NULL, then it is interpreted as a pattern list and
 * only variable names that match it will be accepted.
 */
static void
read_environment_file(char ***env, u_int *envsize,
	const char *filename, const char *whitelist)
{
	FILE *f;
	char *line = NULL, *cp, *value;
	size_t linesize = 0;
	u_int lineno = 0;

	f = fopen(filename, "r");
	if (!f)
		return;

	while (getline(&line, &linesize, f) != -1) {
		if (++lineno > 1000)
			fatal("Too many lines in environment file %s", filename);
		for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '#' || *cp == '\n')
			continue;

		cp[strcspn(cp, "\n")] = '\0';

		value = strchr(cp, '=');
		if (value == NULL) {
			fprintf(stderr, "Bad line %u in %.100s\n", lineno,
			    filename);
			continue;
		}
		/*
		 * Replace the equals sign by nul, and advance value to
		 * the value string.
		 */
		*value = '\0';
		value++;
		if (whitelist != NULL &&
		    match_pattern_list(cp, whitelist, 0) != 1)
			continue;
		child_set_env(env, envsize, cp, value);
	}
	free(line);
	fclose(f);
}

static char **
do_setup_env(struct ssh *ssh, Session *s, const char *shell)
{
	char buf[256];
	size_t n;
	u_int i, envsize;
	char *ocp, *cp, *value, **env, *laddr;
	struct passwd *pw = s->pw;

	/* Initialize the environment. */
	envsize = 100;
	env = xcalloc(envsize, sizeof(char *));
	env[0] = NULL;

#ifdef GSSAPI
	/* Allow any GSSAPI methods that we've used to alter
	 * the childs environment as they see fit
	 */
	ssh_gssapi_do_child(&env, &envsize);
#endif

	/* Set basic environment. */
	for (i = 0; i < s->num_env; i++)
		child_set_env(&env, &envsize, s->env[i].name, s->env[i].val);

	child_set_env(&env, &envsize, "USER", pw->pw_name);
	child_set_env(&env, &envsize, "LOGNAME", pw->pw_name);
	child_set_env(&env, &envsize, "HOME", pw->pw_dir);
	if (setusercontext(lc, pw, pw->pw_uid, LOGIN_SETPATH) < 0)
		child_set_env(&env, &envsize, "PATH", _PATH_STDPATH);
	else
		child_set_env(&env, &envsize, "PATH", getenv("PATH"));

	snprintf(buf, sizeof buf, "%.200s/%.50s", _PATH_MAILDIR, pw->pw_name);
	child_set_env(&env, &envsize, "MAIL", buf);

	/* Normal systems set SHELL by default. */
	child_set_env(&env, &envsize, "SHELL", shell);

	if (getenv("TZ"))
		child_set_env(&env, &envsize, "TZ", getenv("TZ"));
	if (s->term)
		child_set_env(&env, &envsize, "TERM", s->term);
	if (s->display)
		child_set_env(&env, &envsize, "DISPLAY", s->display);
#ifdef KRB5
	if (s->authctxt->krb5_ticket_file)
		child_set_env(&env, &envsize, "KRB5CCNAME",
		    s->authctxt->krb5_ticket_file);
#endif
	if (auth_sock_name != NULL)
		child_set_env(&env, &envsize, SSH_AUTHSOCKET_ENV_NAME,
		    auth_sock_name);


	/* Set custom environment options from pubkey authentication. */
	if (options.permit_user_env) {
		for (n = 0 ; n < auth_opts->nenv; n++) {
			ocp = xstrdup(auth_opts->env[n]);
			cp = strchr(ocp, '=');
			if (*cp == '=') {
				*cp = '\0';
				/* Apply PermitUserEnvironment whitelist */
				if (options.permit_user_env_whitelist == NULL ||
				    match_pattern_list(ocp,
				    options.permit_user_env_whitelist, 0) == 1)
					child_set_env(&env, &envsize,
					    ocp, cp + 1);
			}
			free(ocp);
		}
	}

	/* read $HOME/.ssh/environment. */
	if (options.permit_user_env) {
		snprintf(buf, sizeof buf, "%.200s/.ssh/environment",
		    pw->pw_dir);
		read_environment_file(&env, &envsize, buf,
		    options.permit_user_env_whitelist);
	}

	/* Environment specified by admin */
	for (i = 0; i < options.num_setenv; i++) {
		cp = xstrdup(options.setenv[i]);
		if ((value = strchr(cp, '=')) == NULL) {
			/* shouldn't happen; vars are checked in servconf.c */
			fatal("Invalid config SetEnv: %s", options.setenv[i]);
		}
		*value++ = '\0';
		child_set_env(&env, &envsize, cp, value);
	}

	/* SSH_CLIENT deprecated */
	snprintf(buf, sizeof buf, "%.50s %d %d",
	    ssh_remote_ipaddr(ssh), ssh_remote_port(ssh),
	    ssh_local_port(ssh));
	child_set_env(&env, &envsize, "SSH_CLIENT", buf);

	laddr = get_local_ipaddr(ssh_packet_get_connection_in(ssh));
	snprintf(buf, sizeof buf, "%.50s %d %.50s %d",
	    ssh_remote_ipaddr(ssh), ssh_remote_port(ssh),
	    laddr, ssh_local_port(ssh));
	free(laddr);
	child_set_env(&env, &envsize, "SSH_CONNECTION", buf);

	if (tun_fwd_ifnames != NULL)
		child_set_env(&env, &envsize, "SSH_TUNNEL", tun_fwd_ifnames);
	if (auth_info_file != NULL)
		child_set_env(&env, &envsize, "SSH_USER_AUTH", auth_info_file);
	if (s->ttyfd != -1)
		child_set_env(&env, &envsize, "SSH_TTY", s->tty);
	if (original_command)
		child_set_env(&env, &envsize, "SSH_ORIGINAL_COMMAND",
		    original_command);

	if (debug_flag) {
		/* dump the environment */
		fprintf(stderr, "Environment:\n");
		for (i = 0; env[i]; i++)
			fprintf(stderr, "  %.200s\n", env[i]);
	}
	return env;
}

/*
 * Run $HOME/.ssh/rc, /etc/ssh/sshrc, or xauth (whichever is found
 * first in this order).
 */
static void
do_rc_files(struct ssh *ssh, Session *s, const char *shell)
{
	FILE *f = NULL;
	char cmd[1024];
	int do_xauth;
	struct stat st;

	do_xauth =
	    s->display != NULL && s->auth_proto != NULL && s->auth_data != NULL;

	/* ignore _PATH_SSH_USER_RC for subsystems and admin forced commands */
	if (!s->is_subsystem && options.adm_forced_command == NULL &&
	    auth_opts->permit_user_rc && options.permit_user_rc &&
	    stat(_PATH_SSH_USER_RC, &st) >= 0) {
		snprintf(cmd, sizeof cmd, "%s -c '%s %s'",
		    shell, _PATH_BSHELL, _PATH_SSH_USER_RC);
		if (debug_flag)
			fprintf(stderr, "Running %s\n", cmd);
		f = popen(cmd, "w");
		if (f) {
			if (do_xauth)
				fprintf(f, "%s %s\n", s->auth_proto,
				    s->auth_data);
			pclose(f);
		} else
			fprintf(stderr, "Could not run %s\n",
			    _PATH_SSH_USER_RC);
	} else if (stat(_PATH_SSH_SYSTEM_RC, &st) >= 0) {
		if (debug_flag)
			fprintf(stderr, "Running %s %s\n", _PATH_BSHELL,
			    _PATH_SSH_SYSTEM_RC);
		f = popen(_PATH_BSHELL " " _PATH_SSH_SYSTEM_RC, "w");
		if (f) {
			if (do_xauth)
				fprintf(f, "%s %s\n", s->auth_proto,
				    s->auth_data);
			pclose(f);
		} else
			fprintf(stderr, "Could not run %s\n",
			    _PATH_SSH_SYSTEM_RC);
	} else if (do_xauth && options.xauth_location != NULL) {
		/* Add authority data to .Xauthority if appropriate. */
		if (debug_flag) {
			fprintf(stderr,
			    "Running %.500s remove %.100s\n",
			    options.xauth_location, s->auth_display);
			fprintf(stderr,
			    "%.500s add %.100s %.100s %.100s\n",
			    options.xauth_location, s->auth_display,
			    s->auth_proto, s->auth_data);
		}
		snprintf(cmd, sizeof cmd, "%s -q -",
		    options.xauth_location);
		f = popen(cmd, "w");
		if (f) {
			fprintf(f, "remove %s\n",
			    s->auth_display);
			fprintf(f, "add %s %s %s\n",
			    s->auth_display, s->auth_proto,
			    s->auth_data);
			pclose(f);
		} else {
			fprintf(stderr, "Could not run %s\n",
			    cmd);
		}
	}
}

static void
do_nologin(struct passwd *pw)
{
	FILE *f = NULL;
	char buf[1024], *nl, *def_nl = _PATH_NOLOGIN;
	struct stat sb;

	if (login_getcapbool(lc, "ignorenologin", 0) || pw->pw_uid == 0)
		return;
	nl = login_getcapstr(lc, "nologin", def_nl, def_nl);

	if (stat(nl, &sb) == -1) {
		if (nl != def_nl)
			free(nl);
		return;
	}

	/* /etc/nologin exists.  Print its contents if we can and exit. */
	logit("User %.100s not allowed because %s exists", pw->pw_name, nl);
	if ((f = fopen(nl, "r")) != NULL) {
		while (fgets(buf, sizeof(buf), f))
			fputs(buf, stderr);
		fclose(f);
	}
	exit(254);
}

/*
 * Chroot into a directory after checking it for safety: all path components
 * must be root-owned directories with strict permissions.
 */
static void
safely_chroot(const char *path, uid_t uid)
{
	const char *cp;
	char component[PATH_MAX];
	struct stat st;

	if (!path_absolute(path))
		fatal("chroot path does not begin at root");
	if (strlen(path) >= sizeof(component))
		fatal("chroot path too long");

	/*
	 * Descend the path, checking that each component is a
	 * root-owned directory with strict permissions.
	 */
	for (cp = path; cp != NULL;) {
		if ((cp = strchr(cp, '/')) == NULL)
			strlcpy(component, path, sizeof(component));
		else {
			cp++;
			memcpy(component, path, cp - path);
			component[cp - path] = '\0';
		}
	
		debug3("%s: checking '%s'", __func__, component);

		if (stat(component, &st) != 0)
			fatal("%s: stat(\"%s\"): %s", __func__,
			    component, strerror(errno));
		if (st.st_uid != 0 || (st.st_mode & 022) != 0)
			fatal("bad ownership or modes for chroot "
			    "directory %s\"%s\"",
			    cp == NULL ? "" : "component ", component);
		if (!S_ISDIR(st.st_mode))
			fatal("chroot path %s\"%s\" is not a directory",
			    cp == NULL ? "" : "component ", component);

	}

	if (chdir(path) == -1)
		fatal("Unable to chdir to chroot path \"%s\": "
		    "%s", path, strerror(errno));
	if (chroot(path) == -1)
		fatal("chroot(\"%s\"): %s", path, strerror(errno));
	if (chdir("/") == -1)
		fatal("%s: chdir(/) after chroot: %s",
		    __func__, strerror(errno));
	verbose("Changed root directory to \"%s\"", path);
}

/* Set login name, uid, gid, and groups. */
void
do_setusercontext(struct passwd *pw)
{
	char uidstr[32], *chroot_path, *tmp;

	if (getuid() == 0 || geteuid() == 0) {
		/* Prepare groups */
		if (setusercontext(lc, pw, pw->pw_uid,
		    (LOGIN_SETALL & ~(LOGIN_SETPATH|LOGIN_SETUSER))) < 0) {
			perror("unable to set user context");
			exit(1);
		}

		if (!in_chroot && options.chroot_directory != NULL &&
		    strcasecmp(options.chroot_directory, "none") != 0) {
                        tmp = tilde_expand_filename(options.chroot_directory,
			    pw->pw_uid);
			snprintf(uidstr, sizeof(uidstr), "%llu",
			    (unsigned long long)pw->pw_uid);
			chroot_path = percent_expand(tmp, "h", pw->pw_dir,
			    "u", pw->pw_name, "U", uidstr, (char *)NULL);
			safely_chroot(chroot_path, pw->pw_uid);
			free(tmp);
			free(chroot_path);
			/* Make sure we don't attempt to chroot again */
			free(options.chroot_directory);
			options.chroot_directory = NULL;
			in_chroot = 1;
		}

		/* Set UID */
		if (setusercontext(lc, pw, pw->pw_uid, LOGIN_SETUSER) < 0) {
			perror("unable to set user context (setuser)");
			exit(1);
		}
	} else if (options.chroot_directory != NULL &&
	    strcasecmp(options.chroot_directory, "none") != 0) {
		fatal("server lacks privileges to chroot to ChrootDirectory");
	}

	if (getuid() != pw->pw_uid || geteuid() != pw->pw_uid)
		fatal("Failed to set uids to %u.", (u_int) pw->pw_uid);
}

static void
do_pwchange(Session *s)
{
	fflush(NULL);
	fprintf(stderr, "WARNING: Your password has expired.\n");
	if (s->ttyfd != -1) {
		fprintf(stderr,
		    "You must change your password now and login again!\n");
		execl(_PATH_PASSWD_PROG, "passwd", (char *)NULL);
		perror("passwd");
	} else {
		fprintf(stderr,
		    "Password change required but no TTY available.\n");
	}
	exit(1);
}

static void
child_close_fds(struct ssh *ssh)
{
	extern int auth_sock;

	if (auth_sock != -1) {
		close(auth_sock);
		auth_sock = -1;
	}

	if (ssh_packet_get_connection_in(ssh) ==
	    ssh_packet_get_connection_out(ssh))
		close(ssh_packet_get_connection_in(ssh));
	else {
		close(ssh_packet_get_connection_in(ssh));
		close(ssh_packet_get_connection_out(ssh));
	}
	/*
	 * Close all descriptors related to channels.  They will still remain
	 * open in the parent.
	 */
	/* XXX better use close-on-exec? -markus */
	channel_close_all(ssh);

	/*
	 * Close any extra file descriptors.  Note that there may still be
	 * descriptors left by system functions.  They will be closed later.
	 */
	endpwent();

	/*
	 * Close any extra open file descriptors so that we don't have them
	 * hanging around in clients.  Note that we want to do this after
	 * initgroups, because at least on Solaris 2.3 it leaves file
	 * descriptors open.
	 */
	closefrom(STDERR_FILENO + 1);
}

/*
 * Performs common processing for the child, such as setting up the
 * environment, closing extra file descriptors, setting the user and group
 * ids, and executing the command or shell.
 */
#define ARGV_MAX 10
void
do_child(struct ssh *ssh, Session *s, const char *command)
{
	extern char **environ;
	char **env, *argv[ARGV_MAX], remote_id[512];
	const char *shell, *shell0;
	struct passwd *pw = s->pw;
	int r = 0;

	sshpkt_fmt_connection_id(ssh, remote_id, sizeof(remote_id));

	/* remove hostkey from the child's memory */
	destroy_sensitive_data();
	ssh_packet_clear_keys(ssh);

	/* Force a password change */
	if (s->authctxt->force_pwchange) {
		do_setusercontext(pw);
		child_close_fds(ssh);
		do_pwchange(s);
		exit(1);
	}

	/*
	 * Login(1) does this as well, and it needs uid 0 for the "-h"
	 * switch, so we let login(1) to this for us.
	 */
	do_nologin(pw);
	do_setusercontext(pw);

	/*
	 * Get the shell from the password data.  An empty shell field is
	 * legal, and means /bin/sh.
	 */
	shell = (pw->pw_shell[0] == '\0') ? _PATH_BSHELL : pw->pw_shell;

	/*
	 * Make sure $SHELL points to the shell from the password file,
	 * even if shell is overridden from login.conf
	 */
	env = do_setup_env(ssh, s, shell);

	shell = login_getcapstr(lc, "shell", (char *)shell, (char *)shell);

	/*
	 * Close the connection descriptors; note that this is the child, and
	 * the server will still have the socket open, and it is important
	 * that we do not shutdown it.  Note that the descriptors cannot be
	 * closed before building the environment, as we call
	 * ssh_remote_ipaddr there.
	 */
	child_close_fds(ssh);

	/*
	 * Must take new environment into use so that .ssh/rc,
	 * /etc/ssh/sshrc and xauth are run in the proper environment.
	 */
	environ = env;

#ifdef KRB5
	/*
	 * At this point, we check to see if AFS is active and if we have
	 * a valid Kerberos 5 TGT. If so, it seems like a good idea to see
	 * if we can (and need to) extend the ticket into an AFS token. If
	 * we don't do this, we run into potential problems if the user's
	 * home directory is in AFS and it's not world-readable.
	 */

	if (options.kerberos_get_afs_token && k_hasafs() &&
	    (s->authctxt->krb5_ctx != NULL)) {
		char cell[64];

		debug("Getting AFS token");

		k_setpag();

		if (k_afs_cell_of_file(pw->pw_dir, cell, sizeof(cell)) == 0)
			krb5_afslog(s->authctxt->krb5_ctx,
			    s->authctxt->krb5_fwd_ccache, cell, NULL);

		krb5_afslog_home(s->authctxt->krb5_ctx,
		    s->authctxt->krb5_fwd_ccache, NULL, NULL, pw->pw_dir);
	}
#endif

	/* Change current directory to the user's home directory. */
	if (chdir(pw->pw_dir) < 0) {
		/* Suppress missing homedir warning for chroot case */
		r = login_getcapbool(lc, "requirehome", 0);
		if (r || !in_chroot) {
			fprintf(stderr, "Could not chdir to home "
			    "directory %s: %s\n", pw->pw_dir,
			    strerror(errno));
		}
		if (r)
			exit(1);
	}

	closefrom(STDERR_FILENO + 1);

	do_rc_files(ssh, s, shell);

	/* restore SIGPIPE for child */
	signal(SIGPIPE, SIG_DFL);

	if (s->is_subsystem == SUBSYSTEM_INT_SFTP_ERROR) {
		error("Connection from %s: refusing non-sftp session",
		    remote_id);
		printf("This service allows sftp connections only.\n");
		fflush(NULL);
		exit(1);
	} else if (s->is_subsystem == SUBSYSTEM_INT_SFTP) {
		extern int optind, optreset;
		int i;
		char *p, *args;

		setproctitle("%s@%s", s->pw->pw_name, INTERNAL_SFTP_NAME);
		args = xstrdup(command ? command : "sftp-server");
		for (i = 0, (p = strtok(args, " ")); p; (p = strtok(NULL, " ")))
			if (i < ARGV_MAX - 1)
				argv[i++] = p;
		argv[i] = NULL;
		optind = optreset = 1;
		__progname = argv[0];
		exit(sftp_server_main(i, argv, s->pw));
	}

	fflush(NULL);

	/* Get the last component of the shell name. */
	if ((shell0 = strrchr(shell, '/')) != NULL)
		shell0++;
	else
		shell0 = shell;

	/*
	 * If we have no command, execute the shell.  In this case, the shell
	 * name to be passed in argv[0] is preceded by '-' to indicate that
	 * this is a login shell.
	 */
	if (!command) {
		char argv0[256];

		/* Start the shell.  Set initial character to '-'. */
		argv0[0] = '-';

		if (strlcpy(argv0 + 1, shell0, sizeof(argv0) - 1)
		    >= sizeof(argv0) - 1) {
			errno = EINVAL;
			perror(shell);
			exit(1);
		}

		/* Execute the shell. */
		argv[0] = argv0;
		argv[1] = NULL;
		execve(shell, argv, env);

		/* Executing the shell failed. */
		perror(shell);
		exit(1);
	}
	/*
	 * Execute the command using the user's shell.  This uses the -c
	 * option to execute the command.
	 */
	argv[0] = (char *) shell0;
	argv[1] = "-c";
	argv[2] = (char *) command;
	argv[3] = NULL;
	execve(shell, argv, env);
	perror(shell);
	exit(1);
}

void
session_unused(int id)
{
	debug3("%s: session id %d unused", __func__, id);
	if (id >= options.max_sessions ||
	    id >= sessions_nalloc) {
		fatal("%s: insane session id %d (max %d nalloc %d)",
		    __func__, id, options.max_sessions, sessions_nalloc);
	}
	memset(&sessions[id], 0, sizeof(*sessions));
	sessions[id].self = id;
	sessions[id].used = 0;
	sessions[id].chanid = -1;
	sessions[id].ptyfd = -1;
	sessions[id].ttyfd = -1;
	sessions[id].ptymaster = -1;
	sessions[id].x11_chanids = NULL;
	sessions[id].next_unused = sessions_first_unused;
	sessions_first_unused = id;
}

Session *
session_new(void)
{
	Session *s, *tmp;

	if (sessions_first_unused == -1) {
		if (sessions_nalloc >= options.max_sessions)
			return NULL;
		debug2("%s: allocate (allocated %d max %d)",
		    __func__, sessions_nalloc, options.max_sessions);
		tmp = xrecallocarray(sessions, sessions_nalloc,
		    sessions_nalloc + 1, sizeof(*sessions));
		if (tmp == NULL) {
			error("%s: cannot allocate %d sessions",
			    __func__, sessions_nalloc + 1);
			return NULL;
		}
		sessions = tmp;
		session_unused(sessions_nalloc++);
	}

	if (sessions_first_unused >= sessions_nalloc ||
	    sessions_first_unused < 0) {
		fatal("%s: insane first_unused %d max %d nalloc %d",
		    __func__, sessions_first_unused, options.max_sessions,
		    sessions_nalloc);
	}

	s = &sessions[sessions_first_unused];
	if (s->used) {
		fatal("%s: session %d already used",
		    __func__, sessions_first_unused);
	}
	sessions_first_unused = s->next_unused;
	s->used = 1;
	s->next_unused = -1;
	debug("session_new: session %d", s->self);

	return s;
}

static void
session_dump(void)
{
	int i;
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];

		debug("dump: used %d next_unused %d session %d %p "
		    "channel %d pid %ld",
		    s->used,
		    s->next_unused,
		    s->self,
		    s,
		    s->chanid,
		    (long)s->pid);
	}
}

int
session_open(Authctxt *authctxt, int chanid)
{
	Session *s = session_new();
	debug("session_open: channel %d", chanid);
	if (s == NULL) {
		error("no more sessions");
		return 0;
	}
	s->authctxt = authctxt;
	s->pw = authctxt->pw;
	if (s->pw == NULL || !authctxt->valid)
		fatal("no user for session %d", s->self);
	debug("session_open: session %d: link with channel %d", s->self, chanid);
	s->chanid = chanid;
	return 1;
}

Session *
session_by_tty(char *tty)
{
	int i;
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];
		if (s->used && s->ttyfd != -1 && strcmp(s->tty, tty) == 0) {
			debug("session_by_tty: session %d tty %s", i, tty);
			return s;
		}
	}
	debug("session_by_tty: unknown tty %.100s", tty);
	session_dump();
	return NULL;
}

static Session *
session_by_channel(int id)
{
	int i;
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];
		if (s->used && s->chanid == id) {
			debug("session_by_channel: session %d channel %d",
			    i, id);
			return s;
		}
	}
	debug("session_by_channel: unknown channel %d", id);
	session_dump();
	return NULL;
}

static Session *
session_by_x11_channel(int id)
{
	int i, j;

	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];

		if (s->x11_chanids == NULL || !s->used)
			continue;
		for (j = 0; s->x11_chanids[j] != -1; j++) {
			if (s->x11_chanids[j] == id) {
				debug("session_by_x11_channel: session %d "
				    "channel %d", s->self, id);
				return s;
			}
		}
	}
	debug("session_by_x11_channel: unknown channel %d", id);
	session_dump();
	return NULL;
}

static Session *
session_by_pid(pid_t pid)
{
	int i;
	debug("session_by_pid: pid %ld", (long)pid);
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];
		if (s->used && s->pid == pid)
			return s;
	}
	error("session_by_pid: unknown pid %ld", (long)pid);
	session_dump();
	return NULL;
}

static int
session_window_change_req(struct ssh *ssh, Session *s)
{
	int r;

	if ((r = sshpkt_get_u32(ssh, &s->col)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &s->row)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &s->xpixel)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &s->ypixel)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);
	pty_change_window_size(s->ptyfd, s->row, s->col, s->xpixel, s->ypixel);
	return 1;
}

static int
session_pty_req(struct ssh *ssh, Session *s)
{
	int r;

	if (!auth_opts->permit_pty_flag || !options.permit_tty) {
		debug("Allocating a pty not permitted for this connection.");
		return 0;
	}
	if (s->ttyfd != -1) {
		ssh_packet_disconnect(ssh, "Protocol error: you already have a pty.");
		return 0;
	}

	if ((r = sshpkt_get_cstring(ssh, &s->term, NULL)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &s->col)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &s->row)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &s->xpixel)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &s->ypixel)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);

	if (strcmp(s->term, "") == 0) {
		free(s->term);
		s->term = NULL;
	}

	/* Allocate a pty and open it. */
	debug("Allocating pty.");
	if (!PRIVSEP(pty_allocate(&s->ptyfd, &s->ttyfd, s->tty,
	    sizeof(s->tty)))) {
		free(s->term);
		s->term = NULL;
		s->ptyfd = -1;
		s->ttyfd = -1;
		error("session_pty_req: session %d alloc failed", s->self);
		return 0;
	}
	debug("session_pty_req: session %d alloc %s", s->self, s->tty);

	ssh_tty_parse_modes(ssh, s->ttyfd);

	if ((r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);

	if (!use_privsep)
		pty_setowner(s->pw, s->tty);

	/* Set window size from the packet. */
	pty_change_window_size(s->ptyfd, s->row, s->col, s->xpixel, s->ypixel);

	session_proctitle(s);
	return 1;
}

static int
session_subsystem_req(struct ssh *ssh, Session *s)
{
	struct stat st;
	int r, success = 0;
	char *prog, *cmd;
	u_int i;

	if ((r = sshpkt_get_cstring(ssh, &s->subsys, NULL)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);
	debug2("subsystem request for %.100s by user %s", s->subsys,
	    s->pw->pw_name);

	for (i = 0; i < options.num_subsystems; i++) {
		if (strcmp(s->subsys, options.subsystem_name[i]) == 0) {
			prog = options.subsystem_command[i];
			cmd = options.subsystem_args[i];
			if (strcmp(INTERNAL_SFTP_NAME, prog) == 0) {
				s->is_subsystem = SUBSYSTEM_INT_SFTP;
				debug("subsystem: %s", prog);
			} else {
				if (stat(prog, &st) < 0)
					debug("subsystem: cannot stat %s: %s",
					    prog, strerror(errno));
				s->is_subsystem = SUBSYSTEM_EXT;
				debug("subsystem: exec() %s", cmd);
			}
			success = do_exec(ssh, s, cmd) == 0;
			break;
		}
	}

	if (!success)
		logit("subsystem request for %.100s by user %s failed, "
		    "subsystem not found", s->subsys, s->pw->pw_name);

	return success;
}

static int
session_x11_req(struct ssh *ssh, Session *s)
{
	int r, success;
	u_char single_connection = 0;

	if (s->auth_proto != NULL || s->auth_data != NULL) {
		error("session_x11_req: session %d: "
		    "x11 forwarding already active", s->self);
		return 0;
	}
	if ((r = sshpkt_get_u8(ssh, &single_connection)) != 0 ||
	    (r = sshpkt_get_cstring(ssh, &s->auth_proto, NULL)) != 0 ||
	    (r = sshpkt_get_cstring(ssh, &s->auth_data, NULL)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &s->screen)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);

	s->single_connection = single_connection;

	if (xauth_valid_string(s->auth_proto) &&
	    xauth_valid_string(s->auth_data))
		success = session_setup_x11fwd(ssh, s);
	else {
		success = 0;
		error("Invalid X11 forwarding data");
	}
	if (!success) {
		free(s->auth_proto);
		free(s->auth_data);
		s->auth_proto = NULL;
		s->auth_data = NULL;
	}
	return success;
}

static int
session_shell_req(struct ssh *ssh, Session *s)
{
	int r;

	if ((r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);
	return do_exec(ssh, s, NULL) == 0;
}

static int
session_exec_req(struct ssh *ssh, Session *s)
{
	u_int success;
	int r;
	char *command = NULL;

	if ((r = sshpkt_get_cstring(ssh, &command, NULL)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);

	success = do_exec(ssh, s, command) == 0;
	free(command);
	return success;
}

static int
session_break_req(struct ssh *ssh, Session *s)
{
	int r;

	if ((r = sshpkt_get_u32(ssh, NULL)) != 0 || /* ignore */
	    (r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);

	if (s->ptymaster == -1 || tcsendbreak(s->ptymaster, 0) < 0)
		return 0;
	return 1;
}

static int
session_env_req(struct ssh *ssh, Session *s)
{
	char *name, *val;
	u_int i;
	int r;

	if ((r = sshpkt_get_cstring(ssh, &name, NULL)) != 0 ||
	    (r = sshpkt_get_cstring(ssh, &val, NULL)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);

	/* Don't set too many environment variables */
	if (s->num_env > 128) {
		debug2("Ignoring env request %s: too many env vars", name);
		goto fail;
	}

	for (i = 0; i < options.num_accept_env; i++) {
		if (match_pattern(name, options.accept_env[i])) {
			debug2("Setting env %d: %s=%s", s->num_env, name, val);
			s->env = xrecallocarray(s->env, s->num_env,
			    s->num_env + 1, sizeof(*s->env));
			s->env[s->num_env].name = name;
			s->env[s->num_env].val = val;
			s->num_env++;
			return (1);
		}
	}
	debug2("Ignoring env request %s: disallowed name", name);

 fail:
	free(name);
	free(val);
	return (0);
}

/*
 * Conversion of signals from ssh channel request names.
 * Subset of signals from RFC 4254 section 6.10C, with SIGINFO as
 * local extension.
 */
static int
name2sig(char *name)
{
#define SSH_SIG(x) if (strcmp(name, #x) == 0) return SIG ## x
	SSH_SIG(HUP);
	SSH_SIG(INT);
	SSH_SIG(KILL);
	SSH_SIG(QUIT);
	SSH_SIG(TERM);
	SSH_SIG(USR1);
	SSH_SIG(USR2);
#undef	SSH_SIG
	if (strcmp(name, "INFO@openssh.com") == 0)
		return SIGINFO;
	return -1;
}

static int
session_signal_req(struct ssh *ssh, Session *s)
{
	char *signame = NULL;
	int r, sig, success = 0;

	if ((r = sshpkt_get_cstring(ssh, &signame, NULL)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0) {
		error("%s: parse packet: %s", __func__, ssh_err(r));
		goto out;
	}
	if ((sig = name2sig(signame)) == -1) {
		error("%s: unsupported signal \"%s\"", __func__, signame);
		goto out;
	}
	if (s->pid <= 0) {
		error("%s: no pid for session %d", __func__, s->self);
		goto out;
	}
	if (s->forced || s->is_subsystem) {
		error("%s: refusing to send signal %s to %s session", __func__,
		    signame, s->forced ? "forced-command" : "subsystem");
		goto out;
	}
	if (!use_privsep || mm_is_monitor()) {
		error("%s: session signalling requires privilege separation",
		    __func__);
		goto out;
	}

	debug("%s: signal %s, killpg(%ld, %d)", __func__, signame,
	    (long)s->pid, sig);
	temporarily_use_uid(s->pw);
	r = killpg(s->pid, sig);
	restore_uid();
	if (r != 0) {
		error("%s: killpg(%ld, %d): %s", __func__, (long)s->pid,
		    sig, strerror(errno));
		goto out;
	}

	/* success */
	success = 1;
 out:
	free(signame);
	return success;
}

static int
session_auth_agent_req(struct ssh *ssh, Session *s)
{
	static int called = 0;
	int r;

	if ((r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);
	if (!auth_opts->permit_agent_forwarding_flag ||
	    !options.allow_agent_forwarding) {
		debug("%s: agent forwarding disabled", __func__);
		return 0;
	}
	if (called) {
		return 0;
	} else {
		called = 1;
		return auth_input_request_forwarding(ssh, s->pw);
	}
}

int
session_input_channel_req(struct ssh *ssh, Channel *c, const char *rtype)
{
	int success = 0;
	Session *s;

	if ((s = session_by_channel(c->self)) == NULL) {
		logit("%s: no session %d req %.100s", __func__, c->self, rtype);
		return 0;
	}
	debug("%s: session %d req %s", __func__, s->self, rtype);

	/*
	 * a session is in LARVAL state until a shell, a command
	 * or a subsystem is executed
	 */
	if (c->type == SSH_CHANNEL_LARVAL) {
		if (strcmp(rtype, "shell") == 0) {
			success = session_shell_req(ssh, s);
		} else if (strcmp(rtype, "exec") == 0) {
			success = session_exec_req(ssh, s);
		} else if (strcmp(rtype, "pty-req") == 0) {
			success = session_pty_req(ssh, s);
		} else if (strcmp(rtype, "x11-req") == 0) {
			success = session_x11_req(ssh, s);
		} else if (strcmp(rtype, "auth-agent-req@openssh.com") == 0) {
			success = session_auth_agent_req(ssh, s);
		} else if (strcmp(rtype, "subsystem") == 0) {
			success = session_subsystem_req(ssh, s);
		} else if (strcmp(rtype, "env") == 0) {
			success = session_env_req(ssh, s);
		}
	}
	if (strcmp(rtype, "window-change") == 0) {
		success = session_window_change_req(ssh, s);
	} else if (strcmp(rtype, "break") == 0) {
		success = session_break_req(ssh, s);
	} else if (strcmp(rtype, "signal") == 0) {
		success = session_signal_req(ssh, s);
	}

	return success;
}

void
session_set_fds(struct ssh *ssh, Session *s,
    int fdin, int fdout, int fderr, int ignore_fderr, int is_tty)
{
	/*
	 * now that have a child and a pipe to the child,
	 * we can activate our channel and register the fd's
	 */
	if (s->chanid == -1)
		fatal("no channel for session %d", s->self);
	channel_set_fds(ssh, s->chanid,
	    fdout, fdin, fderr,
	    ignore_fderr ? CHAN_EXTENDED_IGNORE : CHAN_EXTENDED_READ,
	    1, is_tty, CHAN_SES_WINDOW_DEFAULT);
}

/*
 * Function to perform pty cleanup. Also called if we get aborted abnormally
 * (e.g., due to a dropped connection).
 */
void
session_pty_cleanup2(Session *s)
{
	if (s == NULL) {
		error("%s: no session", __func__);
		return;
	}
	if (s->ttyfd == -1)
		return;

	debug("%s: session %d release %s", __func__, s->self, s->tty);

	/* Record that the user has logged out. */
	if (s->pid != 0)
		record_logout(s->pid, s->tty);

	/* Release the pseudo-tty. */
	if (getuid() == 0)
		pty_release(s->tty);

	/*
	 * Close the server side of the socket pairs.  We must do this after
	 * the pty cleanup, so that another process doesn't get this pty
	 * while we're still cleaning up.
	 */
	if (s->ptymaster != -1 && close(s->ptymaster) < 0)
		error("close(s->ptymaster/%d): %s",
		    s->ptymaster, strerror(errno));

	/* unlink pty from session */
	s->ttyfd = -1;
}

void
session_pty_cleanup(Session *s)
{
	PRIVSEP(session_pty_cleanup2(s));
}

static char *
sig2name(int sig)
{
#define SSH_SIG(x) if (sig == SIG ## x) return #x
	SSH_SIG(ABRT);
	SSH_SIG(ALRM);
	SSH_SIG(FPE);
	SSH_SIG(HUP);
	SSH_SIG(ILL);
	SSH_SIG(INT);
	SSH_SIG(KILL);
	SSH_SIG(PIPE);
	SSH_SIG(QUIT);
	SSH_SIG(SEGV);
	SSH_SIG(TERM);
	SSH_SIG(USR1);
	SSH_SIG(USR2);
#undef	SSH_SIG
	return "SIG@openssh.com";
}

static void
session_close_x11(struct ssh *ssh, int id)
{
	Channel *c;

	if ((c = channel_by_id(ssh, id)) == NULL) {
		debug("%s: x11 channel %d missing", __func__, id);
	} else {
		/* Detach X11 listener */
		debug("%s: detach x11 channel %d", __func__, id);
		channel_cancel_cleanup(ssh, id);
		if (c->ostate != CHAN_OUTPUT_CLOSED)
			chan_mark_dead(ssh, c);
	}
}

static void
session_close_single_x11(struct ssh *ssh, int id, void *arg)
{
	Session *s;
	u_int i;

	debug3("%s: channel %d", __func__, id);
	channel_cancel_cleanup(ssh, id);
	if ((s = session_by_x11_channel(id)) == NULL)
		fatal("%s: no x11 channel %d", __func__, id);
	for (i = 0; s->x11_chanids[i] != -1; i++) {
		debug("%s: session %d: closing channel %d",
		    __func__, s->self, s->x11_chanids[i]);
		/*
		 * The channel "id" is already closing, but make sure we
		 * close all of its siblings.
		 */
		if (s->x11_chanids[i] != id)
			session_close_x11(ssh, s->x11_chanids[i]);
	}
	free(s->x11_chanids);
	s->x11_chanids = NULL;
	free(s->display);
	s->display = NULL;
	free(s->auth_proto);
	s->auth_proto = NULL;
	free(s->auth_data);
	s->auth_data = NULL;
	free(s->auth_display);
	s->auth_display = NULL;
}

static void
session_exit_message(struct ssh *ssh, Session *s, int status)
{
	Channel *c;
	int r;

	if ((c = channel_lookup(ssh, s->chanid)) == NULL)
		fatal("%s: session %d: no channel %d",
		    __func__, s->self, s->chanid);
	debug("%s: session %d channel %d pid %ld",
	    __func__, s->self, s->chanid, (long)s->pid);

	if (WIFEXITED(status)) {
		channel_request_start(ssh, s->chanid, "exit-status", 0);
		if ((r = sshpkt_put_u32(ssh, WEXITSTATUS(status))) != 0 ||
		    (r = sshpkt_send(ssh)) != 0)
			sshpkt_fatal(ssh, r, "%s: exit reply", __func__);
	} else if (WIFSIGNALED(status)) {
		channel_request_start(ssh, s->chanid, "exit-signal", 0);
		if ((r = sshpkt_put_cstring(ssh, sig2name(WTERMSIG(status)))) != 0 ||
		    (r = sshpkt_put_u8(ssh, WCOREDUMP(status)? 1 : 0)) != 0 ||
		    (r = sshpkt_put_cstring(ssh, "")) != 0 ||
		    (r = sshpkt_put_cstring(ssh, "")) != 0 ||
		    (r = sshpkt_send(ssh)) != 0)
			sshpkt_fatal(ssh, r, "%s: exit reply", __func__);
	} else {
		/* Some weird exit cause.  Just exit. */
		ssh_packet_disconnect(ssh, "wait returned status %04x.", status);
	}

	/* disconnect channel */
	debug("%s: release channel %d", __func__, s->chanid);

	/*
	 * Adjust cleanup callback attachment to send close messages when
	 * the channel gets EOF. The session will be then be closed
	 * by session_close_by_channel when the childs close their fds.
	 */
	channel_register_cleanup(ssh, c->self, session_close_by_channel, 1);

	/*
	 * emulate a write failure with 'chan_write_failed', nobody will be
	 * interested in data we write.
	 * Note that we must not call 'chan_read_failed', since there could
	 * be some more data waiting in the pipe.
	 */
	if (c->ostate != CHAN_OUTPUT_CLOSED)
		chan_write_failed(ssh, c);
}

void
session_close(struct ssh *ssh, Session *s)
{
	u_int i;

	verbose("Close session: user %s from %.200s port %d id %d",
	    s->pw->pw_name,
	    ssh_remote_ipaddr(ssh),
	    ssh_remote_port(ssh),
	    s->self);

	if (s->ttyfd != -1)
		session_pty_cleanup(s);
	free(s->term);
	free(s->display);
	free(s->x11_chanids);
	free(s->auth_display);
	free(s->auth_data);
	free(s->auth_proto);
	free(s->subsys);
	if (s->env != NULL) {
		for (i = 0; i < s->num_env; i++) {
			free(s->env[i].name);
			free(s->env[i].val);
		}
		free(s->env);
	}
	session_proctitle(s);
	session_unused(s->self);
}

void
session_close_by_pid(struct ssh *ssh, pid_t pid, int status)
{
	Session *s = session_by_pid(pid);
	if (s == NULL) {
		debug("%s: no session for pid %ld", __func__, (long)pid);
		return;
	}
	if (s->chanid != -1)
		session_exit_message(ssh, s, status);
	if (s->ttyfd != -1)
		session_pty_cleanup(s);
	s->pid = 0;
}

/*
 * this is called when a channel dies before
 * the session 'child' itself dies
 */
void
session_close_by_channel(struct ssh *ssh, int id, void *arg)
{
	Session *s = session_by_channel(id);
	u_int i;

	if (s == NULL) {
		debug("%s: no session for id %d", __func__, id);
		return;
	}
	debug("%s: channel %d child %ld", __func__, id, (long)s->pid);
	if (s->pid != 0) {
		debug("%s: channel %d: has child, ttyfd %d",
		    __func__, id, s->ttyfd);
		/*
		 * delay detach of session, but release pty, since
		 * the fd's to the child are already closed
		 */
		if (s->ttyfd != -1)
			session_pty_cleanup(s);
		return;
	}
	/* detach by removing callback */
	channel_cancel_cleanup(ssh, s->chanid);

	/* Close any X11 listeners associated with this session */
	if (s->x11_chanids != NULL) {
		for (i = 0; s->x11_chanids[i] != -1; i++) {
			session_close_x11(ssh, s->x11_chanids[i]);
			s->x11_chanids[i] = -1;
		}
	}

	s->chanid = -1;
	session_close(ssh, s);
}

void
session_destroy_all(struct ssh *ssh, void (*closefunc)(Session *))
{
	int i;
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];
		if (s->used) {
			if (closefunc != NULL)
				closefunc(s);
			else
				session_close(ssh, s);
		}
	}
}

static char *
session_tty_list(void)
{
	static char buf[1024];
	int i;
	buf[0] = '\0';
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];
		if (s->used && s->ttyfd != -1) {
			if (buf[0] != '\0')
				strlcat(buf, ",", sizeof buf);
			strlcat(buf, strrchr(s->tty, '/') + 1, sizeof buf);
		}
	}
	if (buf[0] == '\0')
		strlcpy(buf, "notty", sizeof buf);
	return buf;
}

void
session_proctitle(Session *s)
{
	if (s->pw == NULL)
		error("no user for session %d", s->self);
	else
		setproctitle("%s@%s", s->pw->pw_name, session_tty_list());
}

int
session_setup_x11fwd(struct ssh *ssh, Session *s)
{
	struct stat st;
	char display[512], auth_display[512];
	char hostname[NI_MAXHOST];
	u_int i;

	if (!auth_opts->permit_x11_forwarding_flag) {
		ssh_packet_send_debug(ssh, "X11 forwarding disabled by key options.");
		return 0;
	}
	if (!options.x11_forwarding) {
		debug("X11 forwarding disabled in server configuration file.");
		return 0;
	}
	if (options.xauth_location == NULL ||
	    (stat(options.xauth_location, &st) == -1)) {
		ssh_packet_send_debug(ssh, "No xauth program; cannot forward X11.");
		return 0;
	}
	if (s->display != NULL) {
		debug("X11 display already set.");
		return 0;
	}
	if (x11_create_display_inet(ssh, options.x11_display_offset,
	    options.x11_use_localhost, s->single_connection,
	    &s->display_number, &s->x11_chanids) == -1) {
		debug("x11_create_display_inet failed.");
		return 0;
	}
	for (i = 0; s->x11_chanids[i] != -1; i++) {
		channel_register_cleanup(ssh, s->x11_chanids[i],
		    session_close_single_x11, 0);
	}

	/* Set up a suitable value for the DISPLAY variable. */
	if (gethostname(hostname, sizeof(hostname)) < 0)
		fatal("gethostname: %.100s", strerror(errno));
	/*
	 * auth_display must be used as the displayname when the
	 * authorization entry is added with xauth(1).  This will be
	 * different than the DISPLAY string for localhost displays.
	 */
	if (options.x11_use_localhost) {
		snprintf(display, sizeof display, "localhost:%u.%u",
		    s->display_number, s->screen);
		snprintf(auth_display, sizeof auth_display, "unix:%u.%u",
		    s->display_number, s->screen);
		s->display = xstrdup(display);
		s->auth_display = xstrdup(auth_display);
	} else {
		snprintf(display, sizeof display, "%.400s:%u.%u", hostname,
		    s->display_number, s->screen);
		s->display = xstrdup(display);
		s->auth_display = xstrdup(display);
	}

	return 1;
}

static void
do_authenticated2(struct ssh *ssh, Authctxt *authctxt)
{
	server_loop2(ssh, authctxt);
}

void
do_cleanup(struct ssh *ssh, Authctxt *authctxt)
{
	static int called = 0;

	debug("do_cleanup");

	/* no cleanup if we're in the child for login shell */
	if (is_child)
		return;

	/* avoid double cleanup */
	if (called)
		return;
	called = 1;

	if (authctxt == NULL || !authctxt->authenticated)
		return;
#ifdef KRB5
	if (options.kerberos_ticket_cleanup &&
	    authctxt->krb5_ctx)
		krb5_cleanup_proc(authctxt);
#endif

#ifdef GSSAPI
	if (options.gss_cleanup_creds)
		ssh_gssapi_cleanup_creds();
#endif

	/* remove agent socket */
	auth_sock_cleanup_proc(authctxt->pw);

	/* remove userauth info */
	if (auth_info_file != NULL) {
		temporarily_use_uid(authctxt->pw);
		unlink(auth_info_file);
		restore_uid();
		free(auth_info_file);
		auth_info_file = NULL;
	}

	/*
	 * Cleanup ptys/utmp only if privsep is disabled,
	 * or if running in monitor.
	 */
	if (!use_privsep || mm_is_monitor())
		session_destroy_all(ssh, session_pty_cleanup2);
}

/* Return a name for the remote host that fits inside utmp_size */

const char *
session_get_remote_name_or_ip(struct ssh *ssh, u_int utmp_size, int use_dns)
{
	const char *remote = "";

	if (utmp_size > 0)
		remote = auth_get_canonical_hostname(ssh, use_dns);
	if (utmp_size == 0 || strlen(remote) > utmp_size)
		remote = ssh_remote_ipaddr(ssh);
	return remote;
}

