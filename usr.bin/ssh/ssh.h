/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * Generic header file for ssh.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/* RCSID("$OpenBSD: ssh.h,v 1.60 2001/01/19 15:55:11 markus Exp $"); */

#ifndef SSH_H
#define SSH_H

#include "rsa.h"
#include "cipher.h"

/* Cipher used for encrypting authentication files. */
#define SSH_AUTHFILE_CIPHER	SSH_CIPHER_3DES

/* Default port number. */
#define SSH_DEFAULT_PORT	22

/* Maximum number of TCP/IP ports forwarded per direction. */
#define SSH_MAX_FORWARDS_PER_DIRECTION	100

/*
 * Maximum number of RSA authentication identity files that can be specified
 * in configuration files or on the command line.
 */
#define SSH_MAX_IDENTITY_FILES		100

/*
 * Major protocol version.  Different version indicates major incompatiblity
 * that prevents communication.
 *
 * Minor protocol version.  Different version indicates minor incompatibility
 * that does not prevent interoperation.
 */
#define PROTOCOL_MAJOR_1	1
#define PROTOCOL_MINOR_1	5

/* We support both SSH1 and SSH2 */
#define PROTOCOL_MAJOR_2	2
#define PROTOCOL_MINOR_2	0

/*
 * Name for the service.  The port named by this service overrides the
 * default port if present.
 */
#define SSH_SERVICE_NAME	"ssh"

/*
 * Name of the environment variable containing the pathname of the
 * authentication socket.
 */
#define SSH_AGENTPID_ENV_NAME	"SSH_AGENT_PID"

/*
 * Name of the environment variable containing the pathname of the
 * authentication socket.
 */
#define SSH_AUTHSOCKET_ENV_NAME "SSH_AUTH_SOCK"

/*
 * Environment variable for overwriting the default location of askpass
 */
#define SSH_ASKPASS_ENV		"SSH_ASKPASS"

/*
 * Force host key length and server key length to differ by at least this
 * many bits.  This is to make double encryption with rsaref work.
 */
#define SSH_KEY_BITS_RESERVED		128

/*
 * Length of the session key in bytes.  (Specified as 256 bits in the
 * protocol.)
 */
#define SSH_SESSION_KEY_LENGTH		32

/* Name of Kerberos service for SSH to use. */
#define KRB4_SERVICE_NAME		"rcmd"


/*------------ definitions for login.c -------------*/

/*
 * Returns the time when the user last logged in.  Returns 0 if the
 * information is not available.  This must be called before record_login.
 * The host from which the user logged in is stored in buf.
 */
u_long
get_last_login_time(uid_t uid, const char *logname,
    char *buf, u_int bufsize);

/*
 * Records that the user has logged in.  This does many things normally done
 * by login(1).
 */
void
record_login(pid_t pid, const char *ttyname, const char *user, uid_t uid,
    const char *host, struct sockaddr *addr);

/*
 * Records that the user has logged out.  This does many thigs normally done
 * by login(1) or init.
 */
void    record_logout(pid_t pid, const char *ttyname);

/*------------ definitions for sshconnect.c ----------*/

/*
 * Opens a TCP/IP connection to the remote server on the given host.  If port
 * is 0, the default port will be used.  If anonymous is zero, a privileged
 * port will be allocated to make the connection. This requires super-user
 * privileges if anonymous is false. Connection_attempts specifies the
 * maximum number of tries, one per second.  This returns true on success,
 * and zero on failure.  If the connection is successful, this calls
 * packet_set_connection for the connection.
 */
int
ssh_connect(const char *host, struct sockaddr_storage * hostaddr,
    u_short port, int connection_attempts,
    int anonymous, uid_t original_real_uid,
    const char *proxy_command);

/*
 * Starts a dialog with the server, and authenticates the current user on the
 * server.  This does not need any extra privileges.  The basic connection to
 * the server must already have been established before this is called. If
 * login fails, this function prints an error and never returns. This
 * initializes the random state, and leaves it initialized (it will also have
 * references from the packet module).
 */

void
ssh_login(int host_key_valid, RSA * host_key, const char *host,
    struct sockaddr * hostaddr, uid_t original_real_uid);

/*------------ Definitions for various authentication methods. -------*/

/*
 * Tries to authenticate the user using the .rhosts file.  Returns true if
 * authentication succeeds.  If ignore_rhosts is non-zero, this will not
 * consider .rhosts and .shosts (/etc/hosts.equiv will still be used).
 */
int     auth_rhosts(struct passwd * pw, const char *client_user);

/*
 * Tries to authenticate the user using the .rhosts file and the host using
 * its host key.  Returns true if authentication succeeds.
 */
int
auth_rhosts_rsa(struct passwd * pw, const char *client_user, RSA* client_host_key);

/*
 * Tries to authenticate the user using password.  Returns true if
 * authentication succeeds.
 */
int     auth_password(struct passwd * pw, const char *password);

/*
 * Performs the RSA authentication dialog with the client.  This returns 0 if
 * the client could not be authenticated, and 1 if authentication was
 * successful.  This may exit if there is a serious protocol violation.
 */
int     auth_rsa(struct passwd * pw, BIGNUM * client_n);

/*
 * Parses an RSA key (number of bits, e, n) from a string.  Moves the pointer
 * over the key.  Skips any whitespace at the beginning and at end.
 */
int     auth_rsa_read_key(char **cpp, u_int *bitsp, BIGNUM * e, BIGNUM * n);

/*
 * Returns the name of the machine at the other end of the socket.  The
 * returned string should be freed by the caller.
 */
char   *get_remote_hostname(int socket);

/*
 * Return the canonical name of the host in the other side of the current
 * connection (as returned by packet_get_connection).  The host name is
 * cached, so it is efficient to call this several times.
 */
const char *get_canonical_hostname(void);

/*
 * Returns the remote IP address as an ascii string.  The value need not be
 * freed by the caller.
 */
const char *get_remote_ipaddr(void);

/* Returns the port number of the peer of the socket. */
int     get_peer_port(int sock);

/* Returns the port number of the remote/local host. */
int     get_remote_port(void);
int	get_local_port(void);


/*
 * Performs the RSA authentication challenge-response dialog with the client,
 * and returns true (non-zero) if the client gave the correct answer to our
 * challenge; returns zero if the client gives a wrong answer.
 */
int     auth_rsa_challenge_dialog(RSA *pk);

/*
 * Reads a passphrase from /dev/tty with echo turned off.  Returns the
 * passphrase (allocated with xmalloc).  Exits if EOF is encountered. If
 * from_stdin is true, the passphrase will be read from stdin instead.
 */
char   *read_passphrase(char *prompt, int from_stdin);


/*------------ Definitions for logging. -----------------------*/

/* Supported syslog facilities and levels. */
typedef enum {
	SYSLOG_FACILITY_DAEMON,
	SYSLOG_FACILITY_USER,
	SYSLOG_FACILITY_AUTH,
	SYSLOG_FACILITY_LOCAL0,
	SYSLOG_FACILITY_LOCAL1,
	SYSLOG_FACILITY_LOCAL2,
	SYSLOG_FACILITY_LOCAL3,
	SYSLOG_FACILITY_LOCAL4,
	SYSLOG_FACILITY_LOCAL5,
	SYSLOG_FACILITY_LOCAL6,
	SYSLOG_FACILITY_LOCAL7
}       SyslogFacility;

typedef enum {
	SYSLOG_LEVEL_QUIET,
	SYSLOG_LEVEL_FATAL,
	SYSLOG_LEVEL_ERROR,
	SYSLOG_LEVEL_INFO,
	SYSLOG_LEVEL_VERBOSE,
	SYSLOG_LEVEL_DEBUG1,
	SYSLOG_LEVEL_DEBUG2,
	SYSLOG_LEVEL_DEBUG3
}       LogLevel;
/* Initializes logging. */
void    log_init(char *av0, LogLevel level, SyslogFacility facility, int on_stderr);

/* Logging implementation, depending on server or client */
void    do_log(LogLevel level, const char *fmt, va_list args);

/* name to facility/level */
SyslogFacility log_facility_number(char *name);
LogLevel log_level_number(char *name);

/* Output a message to syslog or stderr */
void    fatal(const char *fmt,...) __attribute__((format(printf, 1, 2)));
void    error(const char *fmt,...) __attribute__((format(printf, 1, 2)));
void    log(const char *fmt,...) __attribute__((format(printf, 1, 2)));
void    verbose(const char *fmt,...) __attribute__((format(printf, 1, 2)));
void    debug(const char *fmt,...) __attribute__((format(printf, 1, 2)));
void    debug2(const char *fmt,...) __attribute__((format(printf, 1, 2)));
void    debug3(const char *fmt,...) __attribute__((format(printf, 1, 2)));

/* same as fatal() but w/o logging */
void    fatal_cleanup(void);

/*
 * Registers a cleanup function to be called by fatal()/fatal_cleanup()
 * before exiting. It is permissible to call fatal_remove_cleanup for the
 * function itself from the function.
 */
void    fatal_add_cleanup(void (*proc) (void *context), void *context);

/* Removes a cleanup function to be called at fatal(). */
void    fatal_remove_cleanup(void (*proc) (void *context), void *context);

/* ---- misc */

/*
 * Expands tildes in the file name.  Returns data allocated by xmalloc.
 * Warning: this calls getpw*.
 */
char   *tilde_expand_filename(const char *filename, uid_t my_uid);

/* remove newline at end of string */
char	*chop(char *s);

/* return next token in configuration line */
char	*strdelim(char **s);

/* set filedescriptor to non-blocking */
void	set_nonblock(int fd);

/*
 * Performs the interactive session.  This handles data transmission between
 * the client and the program.  Note that the notion of stdin, stdout, and
 * stderr in this function is sort of reversed: this function writes to stdin
 * (of the child program), and reads from stdout and stderr (of the child
 * program).
 */
void    server_loop(pid_t pid, int fdin, int fdout, int fderr);
void    server_loop2(void);

/* Client side main loop for the interactive session. */
int     client_loop(int have_pty, int escape_char, int id);

/* Linked list of custom environment strings (see auth-rsa.c). */
struct envstring {
	struct envstring *next;
	char   *s;
};

/*
 * Ensure all of data on socket comes through. f==read || f==write
 */
ssize_t	atomicio(ssize_t (*f)(), int fd, void *s, size_t n);

#ifdef KRB4
#include <krb.h>
/*
 * Performs Kerberos v4 mutual authentication with the client. This returns 0
 * if the client could not be authenticated, and 1 if authentication was
 * successful.  This may exit if there is a serious protocol violation.
 */
int     auth_krb4(const char *server_user, KTEXT auth, char **client);
int     krb4_init(uid_t uid);
void    krb4_cleanup_proc(void *ignore);
int	auth_krb4_password(struct passwd * pw, const char *password);

#ifdef AFS
#include <kafs.h>

/* Accept passed Kerberos v4 ticket-granting ticket and AFS tokens. */
int     auth_kerberos_tgt(struct passwd * pw, const char *string);
int     auth_afs_token(struct passwd * pw, const char *token_string);

int     creds_to_radix(CREDENTIALS * creds, u_char *buf, size_t buflen);
int     radix_to_creds(const char *buf, CREDENTIALS * creds);
#endif				/* AFS */

#endif				/* KRB4 */

/* AF_UNSPEC or AF_INET or AF_INET6 */
extern int IPv4or6;

#endif				/* SSH_H */
