/* movemail foo bar -- move file foo to file bar,
   locking file foo the way /bin/mail respects.
   Copyright (C) 1986, 1992, 1993, 1994 Free Software Foundation, Inc.

This file is part of GNU Emacs.

GNU Emacs is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Important notice: defining MAIL_USE_FLOCK or MAIL_USE_LOCKF *will
   cause loss of mail* if you do it on a system that does not normally
   use flock as its way of interlocking access to inbox files.  The
   setting of MAIL_USE_FLOCK and MAIL_USE_LOCKF *must agree* with the
   system's own conventions.  It is not a choice that is up to you.

   So, if your system uses lock files rather than flock, then the only way
   you can get proper operation is to enable movemail to write lockfiles there.
   This means you must either give that directory access modes
   that permit everyone to write lockfiles in it, or you must make movemail
   a setuid or setgid program.  */

/*
 * Modified January, 1986 by Michael R. Gretzinger (Project Athena)
 *
 * Added POP (Post Office Protocol) service.  When compiled -DPOP
 * movemail will accept input filename arguments of the form
 * "po:username".  This will cause movemail to open a connection to
 * a pop server running on $MAILHOST (environment variable).  Movemail
 * must be setuid to root in order to work with POP.
 * 
 * New module: popmail.c
 * Modified routines:
 *	main - added code within #ifdef MAIL_USE_POP; added setuid (getuid ())
 *		after POP code. 
 * New routines in movemail.c:
 *	get_errmsg - return pointer to system error message
 *
 * Modified August, 1993 by Jonathan Kamens (OpenVision Technologies)
 *
 * Move all of the POP code into a separate file, "pop.c".
 * Use strerror instead of get_errmsg.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#include <protos.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif /* HAVE_SYS_RESOURCE_H */
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <errno.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#if 0
#include "syswait.h"
#endif
#ifdef SOCKS
#include <socks.h>
/* This doesn't belong here. */
struct tm *localtime(const time_t *);
struct hostent  *gethostbyname(const char *);
#endif
#include <roken.h>

#include "pop.h"

#ifdef MSDOS
#undef access
#endif /* MSDOS */

#ifndef F_OK
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#endif

#ifdef HAVE_SYS_LOCKING_H
#include <sys/locking.h>
#endif

#ifdef MAIL_USE_LOCKF
#define MAIL_USE_SYSTEM_LOCK
#endif

#ifdef MAIL_USE_FLOCK
#define MAIL_USE_SYSTEM_LOCK
#endif

#ifdef MAIL_USE_MMDF
extern int lk_open (), lk_close ();
#endif

void fatal (char *s1, char *s2);
void error (char *s1, ...);
void pfatal_with_name (char *name);
void pfatal_and_delete (char *name);
char *concat (char *s1, char *s2, char *s3);
long *xmalloc (unsigned size);
int popmail (char *user, char *outfile);
int pop_retr (popserver server, int msgno, int (*action)(char *, void *),
	      void *arg);
int mbx_write (char *line, FILE *mbf);
int mbx_delimit_begin (FILE *mbf);
int mbx_delimit_end (FILE *mbf);

typedef int WAITTYPE;
#define WRETCODE(x) WEXITSTATUS(x)

/* Nonzero means this is name of a lock file to delete on fatal error.  */
char *delete_lockname;

int
main (int argc, char **argv)
{
  char *inname, *outname;
  int indesc, outdesc;
  int nread;
  WAITTYPE status;

#ifndef MAIL_USE_SYSTEM_LOCK
  struct stat st;
  long now;
  int tem;
  char *lockname, *p;
  char *tempname;
  int desc;
#endif /* not MAIL_USE_SYSTEM_LOCK */

  delete_lockname = 0;

  if (argc < 3)
    {
      fprintf (stderr, "Usage: movemail inbox destfile\n");
      exit(1);
    }

  inname = argv[1];
  outname = argv[2];

#ifdef MAIL_USE_MMDF
  mmdf_init (argv[0]);
#endif

  if (*outname == 0)
    fatal ("Destination file name is empty", 0);

  /* Check access to output file.  */
  if (access (outname, F_OK) == 0 && access (outname, W_OK) != 0)
    pfatal_with_name (outname);

  /* Also check that outname's directory is writeable to the real uid.  */
  {
    char *buf = (char *) xmalloc (strlen (outname) + 1);
    char *p;
    strcpy (buf, outname);
    p = buf + strlen (buf);
    while (p > buf && p[-1] != '/')
      *--p = 0;
    if (p == buf)
      *p++ = '.';
    if (access (buf, W_OK) != 0)
      pfatal_with_name (buf);
    free (buf);
  }

  if (!strncmp (inname, "po:", 3))
    {
      int status;

      status = popmail (inname + 3, outname);
      exit (status);
    }

  setuid (getuid ());

  /* Check access to input file.  */
  if (access (inname, R_OK | W_OK) != 0)
    pfatal_with_name (inname);

#ifndef MAIL_USE_MMDF
#ifndef MAIL_USE_SYSTEM_LOCK
  /* Use a lock file named after our first argument with .lock appended:
     If it exists, the mail file is locked.  */
  /* Note: this locking mechanism is *required* by the mailer
     (on systems which use it) to prevent loss of mail.

     On systems that use a lock file, extracting the mail without locking
     WILL occasionally cause loss of mail due to timing errors!

     So, if creation of the lock file fails
     due to access permission on the mail spool directory,
     you simply MUST change the permission
     and/or make movemail a setgid program
     so it can create lock files properly.

     You might also wish to verify that your system is one
     which uses lock files for this purpose.  Some systems use other methods.

     If your system uses the `flock' system call for mail locking,
     define MAIL_USE_SYSTEM_LOCK in config.h or the s-*.h file
     and recompile movemail.  If the s- file for your system
     should define MAIL_USE_SYSTEM_LOCK but does not, send a bug report
     to bug-gnu-emacs@prep.ai.mit.edu so we can fix it.  */

  lockname = concat (inname, ".lock", "");
  tempname = (char *) xmalloc (strlen (inname) + strlen ("EXXXXXX") + 1);
  strcpy (tempname, inname);
  p = tempname + strlen (tempname);
  while (p != tempname && p[-1] != '/')
    p--;
  *p = 0;
  strcpy (p, "EXXXXXX");
  mktemp (tempname);
  unlink (tempname);

  while (1)
    {
      /* Create the lock file, but not under the lock file name.  */
      /* Give up if cannot do that.  */
      desc = open (tempname, O_WRONLY | O_CREAT | O_EXCL, 0666);
      if (desc < 0)
	{
	  char *message = (char *) xmalloc (strlen (tempname) + 50);
	  sprintf (message, "%s--see source file lib-src/movemail.c",
		   tempname);
	  pfatal_with_name (message);
	}
      close (desc);

      tem = link (tempname, lockname);
      unlink (tempname);
      if (tem >= 0)
	break;
      sleep (1);

      /* If lock file is five minutes old, unlock it.
	 Five minutes should be good enough to cope with crashes
	 and wedgitude, and long enough to avoid being fooled
	 by time differences between machines.  */
      if (stat (lockname, &st) >= 0)
	{
	  now = time (0);
	  if (st.st_ctime < now - 300)
	    unlink (lockname);
	}
    }

  delete_lockname = lockname;
#endif /* not MAIL_USE_SYSTEM_LOCK */
#endif /* not MAIL_USE_MMDF */

  if (fork () == 0)
    {
      setuid (getuid ());

#ifndef MAIL_USE_MMDF
#ifdef MAIL_USE_SYSTEM_LOCK
      indesc = open (inname, O_RDWR);
#else  /* if not MAIL_USE_SYSTEM_LOCK */
      indesc = open (inname, O_RDONLY);
#endif /* not MAIL_USE_SYSTEM_LOCK */
#else  /* MAIL_USE_MMDF */
      indesc = lk_open (inname, O_RDONLY, 0, 0, 10);
#endif /* MAIL_USE_MMDF */

      if (indesc < 0)
	pfatal_with_name (inname);

#if defined (BSD) || defined (XENIX)
      /* In case movemail is setuid to root, make sure the user can
	 read the output file.  */
      /* This is desirable for all systems
	 but I don't want to assume all have the umask system call */
      umask (umask (0) & 0333);
#endif /* BSD or Xenix */
      outdesc = open (outname, O_WRONLY | O_CREAT | O_EXCL, 0666);
      if (outdesc < 0)
	pfatal_with_name (outname);
#ifdef MAIL_USE_SYSTEM_LOCK
#ifdef MAIL_USE_LOCKF
      if (lockf (indesc, F_LOCK, 0) < 0) pfatal_with_name (inname);
#else /* not MAIL_USE_LOCKF */
#ifdef XENIX
      if (locking (indesc, LK_RLCK, 0L) < 0) pfatal_with_name (inname);
#else
      if (flock (indesc, LOCK_EX) < 0) pfatal_with_name (inname);
#endif
#endif /* not MAIL_USE_LOCKF */
#endif /* MAIL_USE_SYSTEM_LOCK */

      {
	char buf[1024];

	while (1)
	  {
	    nread = read (indesc, buf, sizeof buf);
	    if (nread != write (outdesc, buf, nread))
	      {
		int saved_errno = errno;
		unlink (outname);
		errno = saved_errno;
		pfatal_with_name (outname);
	      }
	    if (nread < sizeof buf)
	      break;
	  }
      }

#ifdef BSD
      if (fsync (outdesc) < 0)
	pfatal_and_delete (outname);
#endif

      /* Check to make sure no errors before we zap the inbox.  */
      if (close (outdesc) != 0)
	pfatal_and_delete (outname);

#ifdef MAIL_USE_SYSTEM_LOCK
#if defined (STRIDE) || defined (XENIX)
      /* Stride, xenix have file locking, but no ftruncate.  This mess will do. */
      close (open (inname, O_CREAT | O_TRUNC | O_RDWR, 0666));
#else
      ftruncate (indesc, 0L);
#endif /* STRIDE or XENIX */
#endif /* MAIL_USE_SYSTEM_LOCK */

#ifdef MAIL_USE_MMDF
      lk_close (indesc, 0, 0, 0);
#else
      close (indesc);
#endif

#ifndef MAIL_USE_SYSTEM_LOCK
      /* Delete the input file; if we can't, at least get rid of its
	 contents.  */
#ifdef MAIL_UNLINK_SPOOL
      /* This is generally bad to do, because it destroys the permissions
	 that were set on the file.  Better to just empty the file.  */
      if (unlink (inname) < 0 && errno != ENOENT)
#endif /* MAIL_UNLINK_SPOOL */
	creat (inname, 0600);
#endif /* not MAIL_USE_SYSTEM_LOCK */

      exit (0);
    }

  wait (&status);
  if (!WIFEXITED (status))
    exit (1);
  else if (WRETCODE (status) != 0)
    exit (WRETCODE (status));

#if !defined (MAIL_USE_MMDF) && !defined (MAIL_USE_SYSTEM_LOCK)
  unlink (lockname);
#endif /* not MAIL_USE_MMDF and not MAIL_USE_SYSTEM_LOCK */
  return 0;
}

/* Print error message and exit.  */

void
fatal (char *s1, char *s2)
{
  if (delete_lockname)
    unlink (delete_lockname);
  error (s1, s2);
  exit (1);
}

/* Print error message.  `s1' is printf control string, `s2' is arg for it. */

void
error (char *s1, ...)
{
  va_list args;

  va_start(args, s1);
  fprintf (stderr, "movemail: ");
  vfprintf (stderr, s1, args);
  fprintf (stderr, "\n");
  va_end(args);
}

void
pfatal_with_name (char *name)
{
  char *s = concat ("", strerror (errno), " for %s");
  fatal (s, name);
}

void
pfatal_and_delete (char *name)
{
  char *s = concat ("", strerror (errno), " for %s");
  unlink (name);
  fatal (s, name);
}

/* Return a newly-allocated string whose contents concatenate those of s1, s2, s3.  */

char *
concat (char *s1, char *s2, char *s3)
{
  int len1 = strlen (s1), len2 = strlen (s2), len3 = strlen (s3);
  char *result = (char *) xmalloc (len1 + len2 + len3 + 1);

  strcpy (result, s1);
  strcpy (result + len1, s2);
  strcpy (result + len1 + len2, s3);
  *(result + len1 + len2 + len3) = 0;

  return result;
}

/* Like malloc but get fatal error if memory is exhausted.  */

long *
xmalloc (unsigned size)
{
  long *result = (long *) malloc (size);
  if (!result)
    fatal ("virtual memory exhausted", 0);
  return result;
}

/* This is the guts of the interface to the Post Office Protocol.  */

#define NOTOK (-1)
#define OK 0
#define DONE 1

char *progname;
FILE *sfi;
FILE *sfo;
char ibuffer[BUFSIZ];
char obuffer[BUFSIZ];
char Errmsg[80];

int
popmail (char *user, char *outfile)
{
  int nmsgs, nbytes;
  int i;
  int mbfi;
  FILE *mbf;
  popserver server;

  server = pop_open (0, user, 0, POP_NO_GETPASS);
  if (! server)
    {
      error (pop_error);
      return (1);
    }

  if (pop_stat (server, &nmsgs, &nbytes))
    {
      error (pop_error);
      return (1);
    }

  if (!nmsgs)
    {
      pop_close (server);
      return (0);
    }

  mbfi = open (outfile, O_WRONLY | O_CREAT | O_EXCL, 0666);
  if (mbfi < 0)
    {
      pop_close (server);
      error ("Error in open: %s, %s", strerror (errno), outfile);
      return (1);
    }
  fchown (mbfi, getuid (), -1);

  if ((mbf = fdopen (mbfi, "w")) == NULL)
    {
      pop_close (server);
      error ("Error in fdopen: %s", strerror (errno));
      close (mbfi);
      unlink (outfile);
      return (1);
    }

  for (i = 1; i <= nmsgs; i++)
    {
      mbx_delimit_begin (mbf);
      if (pop_retr (server, i, (int (*)(char *, void *))mbx_write, mbf) != OK)
	{
	  error (Errmsg);
	  close (mbfi);
	  return (1);
	}
      mbx_delimit_end (mbf);
      fflush (mbf);
      if (ferror (mbf))
	{
	  error ("Error in fflush: %s", strerror (errno));
	  pop_close (server);
	  close (mbfi);
	  return (1);
	}
    }

  /* On AFS, a call to write only modifies the file in the local
   *     workstation's AFS cache.  The changes are not written to the server
   *      until a call to fsync or close is made.  Users with AFS home
   *      directories have lost mail when over quota because these checks were
   *      not made in previous versions of movemail. */

#ifdef BSD
  if (fsync (mbfi) < 0)
    {
      error ("Error in fsync: %s", strerror (errno));
      return (1);
    }
#endif

  if (close (mbfi) == -1)
    {
      error ("Error in close: %s", strerror (errno));
      return (1);
    }

  for (i = 1; i <= nmsgs; i++)
    {
      if (pop_delete (server, i))
	{
	  error (pop_error);
	  pop_close (server);
	  return (1);
	}
    }

  if (pop_quit (server))
    {
      error (pop_error);
      return (1);
    }
    
  return (0);
}

int
pop_retr (popserver server, int msgno, int (*action)(char *, void *),
	  void *arg)
{
  char *line;
  int ret;

  if (pop_retrieve_first (server, msgno, &line))
    {
      strncpy (Errmsg, pop_error, sizeof (Errmsg));
      Errmsg[sizeof (Errmsg)-1] = '\0';
      return (NOTOK);
    }

  while (! (ret = pop_retrieve_next (server, &line)))
    {
      if (! line)
	break;

      if ((*action)(line, arg) != OK)
	{
	  strcpy (Errmsg, strerror (errno));
	  pop_close (server);
	  return (NOTOK);
	}
    }

  if (ret)
    {
      strncpy (Errmsg, pop_error, sizeof (Errmsg));
      Errmsg[sizeof (Errmsg)-1] = '\0';
      return (NOTOK);
    }

  return (OK);
}

/* Do this as a macro instead of using strcmp to save on execution time. */
#define IS_FROM_LINE(a) ((a[0] == 'F') \
			 && (a[1] == 'r') \
			 && (a[2] == 'o') \
			 && (a[3] == 'm') \
			 && (a[4] == ' '))

int
mbx_write (char *line, FILE *mbf)
{
  if (IS_FROM_LINE (line))
    {
      if (fputc ('>', mbf) == EOF)
	return (NOTOK);
    }
  if (fputs (line, mbf) == EOF) 
    return (NOTOK);
  if (fputc (0x0a, mbf) == EOF)
    return (NOTOK);
  return (OK);
}

int
mbx_delimit_begin (FILE *mbf)
{
#ifdef BABYL
  if (fputs ("\f\n0, unseen,,\n", mbf) == EOF)
    return (NOTOK);
#else
  char *truename = "received_at";
  time_t date;
  time(&date);
  if (fprintf(mbf, "From %s %s", truename, ctime(&date)) == EOF)
    return (NOTOK);
#endif
  
  return (OK);
}

int mbx_delimit_end (FILE *mbf)
{
#ifdef BABYL
  if (putc ('\037', mbf) == EOF)
    return (NOTOK);
#endif
  return (OK);
}


#ifndef HAVE_STRERROR
char *
strerror (errnum)
     int errnum;
{
  extern char *sys_errlist[];
  extern int sys_nerr;

  if (errnum >= 0 && errnum < sys_nerr)
    return sys_errlist[errnum];
  return (char *) "Unknown error";
}

#endif /* ! HAVE_STRERROR */
