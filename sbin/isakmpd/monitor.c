/*	$OpenBSD: monitor.c,v 1.12 2004/03/15 16:34:28 hshoexer Exp $	*/

/*
 * Copyright (c) 2003 Håkan Olsson.  All rights reserved.
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined (USE_POLICY)
#include <regex.h>
#include <keynote.h>
#endif

#include "conf.h"
#include "log.h"
#include "monitor.h"
#include "policy.h"
#include "util.h"

struct monitor_state
{
  pid_t	pid;
  int	s;
  char	root[MAXPATHLEN];
} m_state;

volatile sig_atomic_t sigchlded = 0;
extern volatile sig_atomic_t sigtermed;

/* Private functions.  */
int m_write_int32 (int, int32_t);
int m_write_raw (int, char *, size_t);
int m_read_int32 (int, int32_t *);
int m_read_raw (int, char *, size_t);
void m_flush (int);

void m_priv_getfd (int);
void m_priv_getsocket (int);
void m_priv_setsockopt (int);
void m_priv_bind (int);
void m_priv_mkfifo (int);
void m_priv_local_sanitize_path (char *, size_t, int);

/*
 * Public functions, unprivileged.
 */

/* Setup monitor context, fork, drop child privs.  */
pid_t
monitor_init (void)
{
  struct passwd *pw;
  int p[2];
  memset (&m_state, 0, sizeof m_state);

  if (socketpair (AF_UNIX, SOCK_STREAM, PF_UNSPEC, p) != 0)
    log_fatal ("monitor_init: socketpair() failed");

  pw = getpwnam (ISAKMPD_PRIVSEP_USER);
  if (pw == NULL)
    log_fatal ("monitor_init: getpwnam(\"%s\") failed",
	       ISAKMPD_PRIVSEP_USER);

  m_state.pid = fork ();
  m_state.s = p[m_state.pid ? 1 : 0];
  strlcpy (m_state.root, pw->pw_dir, sizeof m_state.root);

  LOG_DBG ((LOG_SYSDEP, 30, "monitor_init: pid %d my fd %d", m_state.pid,
	    m_state.s));

  /* The child process should drop privileges now.  */
  if (!m_state.pid)
    {
      if (chroot (pw->pw_dir) != 0 || chdir("/") != 0)
	log_fatal ("monitor_init: chroot failed");

      if (setgid (pw->pw_gid) != 0)
	log_fatal ("monitor_init: setgid(%d) failed", pw->pw_gid);

      if (setuid (pw->pw_uid) != 0)
	log_fatal ("monitor_init: setuid(%d) failed", pw->pw_uid);

      LOG_DBG ((LOG_MISC, 10,
		"monitor_init: privileges dropped for child process"));
    }
  else
    {
      setproctitle ("monitor [priv]");
    }

  return m_state.pid;
}

int
monitor_open (const char *path, int flags, mode_t mode)
{
  int fd, mode32 = (int32_t) mode;
  char realpath[MAXPATHLEN];

  if (m_state.pid)
    {
      /* Called from the parent, i.e already privileged.  */
      return open (path, flags, mode);
    }

  if (path[0] == '/')
    strlcpy (realpath, path, sizeof realpath);
  else
    snprintf (realpath, sizeof realpath, "%s/%s", m_state.root, path);

  /* Write data to priv process.  */
  if (m_write_int32 (m_state.s, MONITOR_GET_FD))
    goto errout;

  if (m_write_raw (m_state.s, realpath, strlen (realpath) + 1))
    goto errout;

  if (m_write_int32 (m_state.s, flags))
    goto errout;

  if (m_write_int32 (m_state.s, mode32))
    goto errout;


  /* Wait for response.  */
  fd = mm_receive_fd (m_state.s);
  if (fd < 0)
    {
      log_error ("monitor_open: open(\"%s\") failed", path);
      return -1;
    }

  return fd;

 errout:
  log_error ("monitor_open: problem talking to privileged process");
  return -1;
}

FILE *
monitor_fopen (const char *path, const char *mode)
{
  FILE *fp;
  int fd, flags = 0, umask = 0;

  /* Only the child process is supposed to run this.  */
  if (m_state.pid)
    log_fatal ("[priv] bad call to monitor_fopen");

  switch (mode[0])
    {
    case 'r':
      flags = (mode[1] == '+' ? O_RDWR : O_RDONLY);
      break;
    case 'w':
      flags = (mode[1] == '+' ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
      break;
    case 'a':
      flags = (mode[1] == '+' ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
      break;
    default:
      log_fatal ("monitor_fopen: bad call");
    }

  fd = monitor_open (path, flags, umask);
  if (fd < 0)
    return NULL;

  /* Got the fd, attach a FILE * to it.  */
  fp = fdopen (fd, mode);
  if (!fp)
    {
      log_error ("monitor_fopen: fdopen() failed");
      close (fd);
      return NULL;
    }

  return fp;
}

int
monitor_stat (const char *path, struct stat *sb)
{
  int fd, r, saved_errno;

  fd = monitor_open (path, O_RDONLY, 0);
  if (fd < 0)
    {
      errno = EACCES; /* A good guess? */
      return -1;
    }

  r = fstat (fd, sb);
  saved_errno = errno;
  close (fd);
  errno = saved_errno;
  return r;
}

int
monitor_socket (int domain, int type, int protocol)
{
  int s;

  if (m_write_int32 (m_state.s, MONITOR_GET_SOCKET))
    goto errout;

  if (m_write_int32 (m_state.s, (int32_t)domain))
    goto errout;

  if (m_write_int32 (m_state.s, (int32_t)type))
    goto errout;

  if (m_write_int32 (m_state.s, (int32_t)protocol))
    goto errout;


  /* Read result.  */
  s = mm_receive_fd (m_state.s);

  return s;

 errout:
  log_error ("monitor_socket: problem talking to privileged process");
  return -1;
}

int
monitor_setsockopt (int s, int level, int optname, const void *optval,
		    socklen_t optlen)
{
  int ret;

  if (m_write_int32 (m_state.s, MONITOR_SETSOCKOPT))
    goto errout;
  mm_send_fd (m_state.s, s); /* XXX? */

  if (m_write_int32 (m_state.s, (int32_t)level))
    goto errout;
  if (m_write_int32 (m_state.s, (int32_t)optname))
    goto errout;
  if (m_write_int32 (m_state.s, (int32_t)optlen))
    goto errout;
  if (m_write_raw (m_state.s, (char *)optval, (size_t)optlen))
    goto errout;

  if (m_read_int32 (m_state.s, &ret))
    goto errout;

  return ret;

 errout:
  log_print ("monitor_setsockopt: read/write error");
  return -1;
}

int
monitor_bind (int s, const struct sockaddr *name, socklen_t namelen)
{
  int ret;

  if (m_write_int32 (m_state.s, MONITOR_BIND))
    goto errout;
  mm_send_fd (m_state.s, s);

  if (m_write_int32 (m_state.s, (int32_t)namelen))
    goto errout;
  if (m_write_raw (m_state.s, (char *)name, (size_t)namelen))
    goto errout;

  if (m_read_int32 (m_state.s, &ret))
    goto errout;

  return ret;

 errout:
  log_print ("monitor_bind: read/write error");
  return -1;
}

int
monitor_mkfifo (const char *path, mode_t mode)
{
  int32_t ret;
  char realpath[MAXPATHLEN];

  /* Only the child process is supposed to run this.  */
  if (m_state.pid)
    log_fatal ("[priv] bad call to monitor_mkfifo");

  if (path[0] == '/')
    strlcpy (realpath, path, sizeof realpath);
  else
    snprintf (realpath, sizeof realpath, "%s/%s", m_state.root, path);

  if (m_write_int32 (m_state.s, MONITOR_MKFIFO))
    goto errout;

  if (m_write_raw (m_state.s, realpath, strlen (realpath) + 1))
    goto errout;

  ret = (int32_t)mode;
  if (m_write_int32 (m_state.s, ret))
    goto errout;

  if (m_read_int32 (m_state.s, &ret))
    goto errout;

  return (int)ret;

 errout:
  log_print ("monitor_mkfifo: read/write error");
  return -1;
}

/*
 * Start of code running with privileges (the monitor process).
 */

/* Help function for monitor_loop().  */
static void
monitor_got_sigchld (int sig)
{
  sigchlded = 1;
}

/* This function is where the privileged process waits(loops) indefinitely.  */
void
monitor_loop (int debugging)
{
  fd_set *fds;
  int n, maxfd, shutdown = 0;

  if (!debugging)
    log_to (0);

  maxfd = m_state.s + 1;

  fds = (fd_set *)calloc (howmany (maxfd, NFDBITS), sizeof (fd_mask));
  if (!fds)
    {
      kill (m_state.pid, SIGTERM);
      log_fatal ("monitor_loop: calloc() failed");
      return;
    }

  /* If the child dies, we should shutdown also.  */
  signal (SIGCHLD, monitor_got_sigchld);

  while (!shutdown)
    {
      /*
       * Currently, there is no need for us to hang around if the child
       * is in the process of shutting down.
       */
      if (sigtermed || sigchlded)
	{
	  if (sigchlded)
	    wait (&n);
	  shutdown++;
	  break;
	}

      FD_ZERO (fds);
      FD_SET (m_state.s, fds);

      n = select (maxfd, fds, NULL, NULL, NULL);
      if (n == -1)
	{
	  if (errno != EINTR)
	    {
	      log_error ("select");
	      sleep (1);
	    }
	}
      else if (n)
	if (FD_ISSET (m_state.s, fds))
	  {
	    int32_t msgcode;
	    if (m_read_int32 (m_state.s, &msgcode))
	      m_flush (m_state.s);
	    else
	      switch (msgcode)
		{
		case MONITOR_GET_FD:
		  m_priv_getfd (m_state.s);
		  break;

		case MONITOR_GET_SOCKET:
		  m_priv_getsocket (m_state.s);
		  break;

		case MONITOR_SETSOCKOPT:
		  m_priv_setsockopt (m_state.s);
		  break;

		case MONITOR_BIND:
		  m_priv_bind (m_state.s);
		  break;

		case MONITOR_MKFIFO:
		  m_priv_mkfifo (m_state.s);
		  break;

		case MONITOR_SHUTDOWN:
		  shutdown++;
		  break;

		default:
		  log_print ("monitor_loop: got unknown code %d", msgcode);
		}
	  }
    }

  free (fds);
  exit (0);
}

/* Privileged: called by monitor_loop.  */
void
m_priv_getfd (int s)
{
  char path[MAXPATHLEN];
  int32_t v;
  int flags;
  mode_t mode;

  /*
   * We expect the following data on the socket:
   *  u_int32_t  pathlen
   *  <variable> path
   *  u_int32_t  flags
   *  u_int32_t  mode
   */

  if (m_read_raw (s, path, MAXPATHLEN))
    goto errout;

  if (m_read_int32 (s, &v))
    goto errout;
  flags = (int)v;

  if (m_read_int32 (s, &v))
    goto errout;
  mode = (mode_t)v;

  m_priv_local_sanitize_path (path, sizeof path, flags);

  v = (int32_t)open (path, flags, mode);
  if (mm_send_fd (s, v))
    {
      close (v);
      goto errout;
    }
  close (v);
  return;

 errout:
  log_error ("m_priv_getfd: read/write operation failed");
  return;
}

/* Privileged: called by monitor_loop.  */
void
m_priv_getsocket (int s)
{
  int domain, type, protocol;
  int32_t v;

  if (m_read_int32 (s, &v))
    goto errout;
  domain = (int)v;

  if (m_read_int32 (s, &v))
    goto errout;
  type = (int)v;

  if (m_read_int32 (s, &v))
    goto errout;
  protocol = (int)v;

  v = (int32_t)socket (domain, type, protocol);
  if (mm_send_fd (s, v))
    {
      close (v);
      goto errout;
    }
  close (v);
  return;

 errout:
  log_error ("m_priv_getsocket: read/write operation failed");
  return;
}

/* Privileged: called by monitor_loop.  */
void
m_priv_setsockopt (int s)
{
  int sock, level, optname;
  char *optval = 0;
  socklen_t optlen;
  int32_t v;

  sock = mm_receive_fd (s);
  if (sock < 0)
    goto errout;

  if (m_read_int32 (s, &level))
    goto errout;

  if (m_read_int32 (s, &optname))
    goto errout;

  if (m_read_int32 (s, &optlen))
    goto errout;

  optval = (char *)malloc (optlen);
  if (!optval)
    goto errout;

  if (m_read_raw (s, optval, optlen))
    goto errout;

  v = (int32_t) setsockopt (sock, level, optname, optval, optlen);
  close (sock);
  sock = -1;
  if (m_write_int32 (s, v))
    goto errout;

  free (optval);
  return;

 errout:
  log_print ("m_priv_setsockopt: read/write error");
  if (optval)
    free (optval);
  if (sock >= 0)
    close (sock);
  return;
}

/* Privileged: called by monitor_loop.  */
void
m_priv_bind (int s)
{
  int sock;
  struct sockaddr *name = 0;
  socklen_t namelen;
  int32_t v;

  sock = mm_receive_fd (s);
  if (sock < 0)
    goto errout;

  if (m_read_int32 (s, &v))
    goto errout;
  namelen = (socklen_t)v;

  name = (struct sockaddr *)malloc (namelen);
  if (!name)
    goto errout;

  if (m_read_raw (s, (char *)name, (size_t)namelen))
    goto errout;

  v = (int32_t)bind (sock, name, namelen);
  if (v < 0)
    log_error ("m_priv_bind: bind(%d,%p,%d) returned %d",
	       sock, name, namelen, v);

  close (sock);
  sock = -1;
  if (m_write_int32 (s, v))
    goto errout;

  free (name);
  return;

 errout:
  log_print ("m_priv_bind: read/write error");
  if (name)
    free (name);
  if (sock >= 0)
    close (sock);
  return;
}

/* Privileged: called by monitor_loop.  */
void
m_priv_mkfifo (int s)
{
  char name[MAXPATHLEN];
  mode_t mode;
  int32_t v;

  if (m_read_raw (s, name, MAXPATHLEN))
    goto errout;

  if (m_read_int32 (s, &v))
    goto errout;
  mode = (mode_t)v;

  /* XXX Sanity checks for 'name'.  */

  unlink (name); /* XXX See ui.c:ui_init() */
  v = (int32_t)mkfifo (name, mode);
  if (v)
    log_error ("m_priv_mkfifo: mkfifo(\"%s\", %d) failed", name, mode);

  if (m_write_int32 (s, v))
    goto errout;

  return;

 errout:
  log_print ("m_priv_mkfifo: read/write error");
  return;
}

/*
 * Help functions, used by both privileged and unprivileged code
 */

/* Write a 32-bit value to a socket.  */
int
m_write_int32 (int s, int32_t value)
{
  u_int32_t v;
  memcpy (&v, &value, sizeof v);
  return (write (s, &v, sizeof v) == -1);
}

/* Write a number of bytes of data to a socket.  */
int
m_write_raw (int s, char *data, size_t dlen)
{
  if (m_write_int32 (s, (int32_t)dlen))
    return 1;
  return (write (s, data, dlen) == -1);
}

int
m_read_int32 (int s, int32_t *value)
{
  u_int32_t v;
  if (read (s, &v, sizeof v) != sizeof v)
    return 1;
  memcpy (value, &v, sizeof v);
  return 0;
}

int
m_read_raw (int s, char *data, size_t maxlen)
{
  u_int32_t v;
  int r;
  if (m_read_int32 (s, &v))
    return 1;
  if (v > maxlen)
    return 1;
  r = read (s, data, v);
  data[v - 1] = 0;
  return (r == -1);
}

/* Drain all available input on a socket.  */
void
m_flush (int s)
{
  u_int8_t tmp;
  int one = 1;

  ioctl (s, FIONBIO, &one);		/* Non-blocking */
  while (read (s, &tmp, 1) > 0) ;
  ioctl (s, FIONBIO, 0);		/* Blocking */
}

/* Check that path/mode is permitted.  */
void
m_priv_local_sanitize_path (char *path, size_t pmax, int flags)
{
  char *p;

  /*
   * Basically, we only permit paths starting with
   *  /etc/isakmpd/	(read only)
   *  /var/run/
   *  /var/tmp
   *  /tmp
   *
   * XXX This is an interim measure only.
   */

  if (strlen (path) < sizeof "/tmp")
    goto bad_path;

  /* Any path containing '..' is invalid.  */
  for (p = path; *p && (p - path) < pmax; p++)
    if (*p == '.' && *(p + 1) == '.')
      goto bad_path;

  /* For any write-mode, only a few paths are permitted.  */
  if ((flags & O_ACCMODE) != O_RDONLY)
    {
      if (strncmp ("/var/run/", path, sizeof "/var/run") == 0 ||
	  strncmp ("/var/tmp/", path, sizeof "/var/tmp") == 0 ||
	  strncmp ("/tmp/", path, sizeof "/tmp") == 0)
	return;
      goto bad_path;
    }

  /* Any other paths are read-only.  */
  if (strncmp (ISAKMPD_ROOT, path, strlen (ISAKMPD_ROOT)) == 0)
    return;

 bad_path:
  log_print ("m_priv_local_sanitize_path: illegal path \"%.1023s\", "
	     "replaced with \"/dev/null\"", path);
  strlcpy (path, "/dev/null", pmax);
  return;
}

