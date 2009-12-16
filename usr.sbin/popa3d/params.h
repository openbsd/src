/* $OpenBSD: params.h,v 1.9 2009/12/16 20:42:26 sobrado Exp $ */

/*
 * Global POP daemon parameters.
 */

#ifndef _POP_PARAMS_H
#define _POP_PARAMS_H

/*
 * Our name to use when talking to various interfaces.
 */
#define POP_SERVER			"popa3d"

/*
 * Are we going to be a standalone server or start via an inetd clone?
 */
#define POP_STANDALONE			1

#if POP_STANDALONE

/*
 * Should the command line options be supported?
 * If enabled, popa3d will default to inetd mode and will require a -D
 * to actually enable the standalone mode.
 */
#define POP_OPTIONS			1

/*
 * The address and port to listen on.
 */
#define DAEMON_ADDR			"0.0.0.0"	/* INADDR_ANY */
#define DAEMON_PORT			110

/*
 * Should libwrap be used?
 *
 * This may make things slower and also adds to code running as root,
 * so it is recommended that you use a packet filter instead. This
 * option is provided primarily as a way to meet conventions of certain
 * systems where all services obey libwrap access controls.
 */
#ifdef LIBWRAP
#define DAEMON_LIBWRAP			1
#else
#define DAEMON_LIBWRAP			0
#endif

#if DAEMON_LIBWRAP
/*
 * How do we talk to libwrap?
 */
#define DAEMON_LIBWRAP_IDENT		POP_SERVER
#endif

/*
 * Limit the number of POP sessions we can handle at a time to reduce
 * the impact of connection flood DoS attacks.
 */
#define MAX_SESSIONS			100
#define MAX_SESSIONS_PER_SOURCE		10
#define MAX_BACKLOG			5
#define MIN_DELAY			10

#endif

/*
 * Do we want to support virtual domains?
 */
#define POP_VIRTUAL			0

#if POP_VIRTUAL

/*
 * VIRTUAL_HOME_PATH is where the virtual domain root directories live.
 */
#define VIRTUAL_HOME_PATH		"/vhome"

/*
 * Subdirectories within each virtual domain root for the authentication
 * information and mailboxes, respectively. These defaults correspond to
 * full pathnames of the form "/vhome/IP/{auth,mail}/username".
 */
#define VIRTUAL_AUTH_PATH		"auth"
#define VIRTUAL_SPOOL_PATH		"mail"

/*
 * Do we want to support virtual domains only? Normally, if the connected
 * IP address doesn't correspond to a directory in VIRTUAL_HOME_PATH, the
 * authentication will be done globally.
 */
#define VIRTUAL_ONLY			0

#else

/*
 * We don't support virtual domains (!POP_VIRTUAL), so we're definitely
 * not virtual-only. Don't edit this.
 */
#define VIRTUAL_ONLY			0

#endif

/*
 * A pseudo-user to run as before authentication. The user and its UID
 * must not be used for any other purpose.
 */
#define POP_USER			POP_SERVER

/*
 * An empty directory to chroot to before authentication. The directory
 * and its parent directories must not be writable by anyone but root.
 */
#define POP_CHROOT			"/var/empty"

/*
 * Sessions will be closed if idle for longer than POP_TIMEOUT seconds.
 * RFC 1939 says that "such a timer MUST be of at least 10 minutes'
 * duration", so I've made 10 minutes the default. In practice, you
 * may want to reduce this to, say, 2 minutes.
 */
#define POP_TIMEOUT			(10 * 60)

/*
 * Do we want to support the obsolete LAST command, as defined in RFC
 * 1460? It has been removed from the protocol in 1994 by RFC 1725,
 * and isn't even mentioned in RFC 1939. Still, some software doesn't
 * work without it.
 */
#define POP_SUPPORT_LAST		1

/*
 * Introduce some sane limits on the mailbox size in order to prevent
 * a single huge mailbox from stopping the entire POP service.
 */
#define MAX_MAILBOX_MESSAGES		200000
#define MAX_MAILBOX_OPEN_BYTES		2000000000
#define MAX_MAILBOX_WORK_BYTES		2500000000

#if !VIRTUAL_ONLY

/*
 * Choose the password authentication method your system uses:
 *
 * AUTH_PASSWD		Use getpwnam(3) only, for *BSD or readable passwd;
 *
 * Note that there's no built-in password aging support.
 */
#define AUTH_PASSWD			1

#endif

#if POP_VIRTUAL || AUTH_PASSWD

/*
 * A salt used to waste some CPU time on dummy crypt(3) calls and make
 * it harder (but still far from impossible, on most systems) to check
 * for valid usernames. Adjust it for your crypt(3).
 */
/*  echo -n "dummyblowfishsalt" | encrypt -b 6 */
#define AUTH_DUMMY_SALT		"$2a$06$bycSsJMBAEDy1E6zzaL5u.vd4GlIrmCWyDgB33OD36h6mrRympUwS"

#endif

/*
 * Message to return to the client when authentication fails. You can
 * #undef this for no message.
 */
#define AUTH_FAILED_MESSAGE		"Authentication failed (bad password?)"

#if !VIRTUAL_ONLY

/*
 * Your mail spool directory. Note: only local (non-NFS) mode 775 mail
 * spools are currently supported.
 *
 * #undef this for qmail-style $HOME/Mailbox mailboxes.
 */
#define MAIL_SPOOL_PATH			"/var/mail"

#ifndef MAIL_SPOOL_PATH
/*
 * The mailbox file name relative to the user's home directory.
 */
#define HOME_MAILBOX_NAME		"Mailbox"
#endif

#endif

/*
 * Locking method your system uses for user mailboxes. It is important
 * that you set this correctly.
 *
 * *BSDs use flock(2), others typically use fcntl(2).
 */
#define LOCK_FCNTL			0
#define LOCK_FLOCK			1

/*
 * How do we talk to syslogd? These should be fine for most systems.
 */
#define SYSLOG_IDENT			POP_SERVER
#define SYSLOG_OPTIONS			LOG_PID
#define SYSLOG_FACILITY			LOG_DAEMON
#define SYSLOG_PRI_LO			LOG_INFO
#define SYSLOG_PRI_HI			LOG_NOTICE
#define SYSLOG_PRI_ERROR		LOG_CRIT

/*
 * There's probably no reason to touch anything below this comment.
 */

/*
 * According to RFC 1939: "Keywords and arguments are each separated by
 * a single SPACE character. Keywords are three or four characters long.
 * Each argument may be up to 40 characters long." We're only processing
 * up to two arguments, so it is safe to truncate after this length.
 */
#define POP_BUFFER_SIZE			0x80

/*
 * There's no reason to change this one either. Making this larger would
 * waste memory, and smaller values could make the authentication fail.
 */
#define AUTH_BUFFER_SIZE		(2 * POP_BUFFER_SIZE)

#if POP_VIRTUAL

/*
 * Buffer size for reading entire per-user authentication files.
 */
#define VIRTUAL_AUTH_SIZE		0x100

#endif

/*
 * File buffer sizes to use while parsing the mailbox and retrieving a
 * message, respectively. Can be changed.
 */
#define FILE_BUFFER_SIZE		0x10000
#define RETR_BUFFER_SIZE		0x8000

/*
 * The mailbox parsing code isn't allowed to truncate lines earlier than
 * this length. Keep this at least as large as the longest header field
 * name we need to check for, but not too large for performance reasons.
 */
#define LINE_BUFFER_SIZE		0x20

#endif
