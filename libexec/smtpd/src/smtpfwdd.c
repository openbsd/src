/*
 * smtpfwdd, Obtuse SMTP forward daemon, master process watches spool
 * directory for files spooled by smtpd. On seeing one, spawns a child
 * to pick it up and invokes sendmail (or sendmail-like agent) to
 * deliver it.
 *
 * $Id: smtpfwdd.c,v 1.3 1998/06/03 08:57:14 beck Exp $
 * 
 * Copyright (c) 1996, 1997 Obtuse Systems Corporation. All rights
 * reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Obtuse Systems 
 *      Corporation and its contributors.
 * 4. Neither the name of the Obtuse Systems Corporation nor the names
 *    of its contributors may be used to endorse or promote products 
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY OBTUSE SYSTEMS CORPORATION AND
 * CONTRIBUTORS ``AS IS''AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL OBTUSE SYSTEMS CORPORATION OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
char *obtuse_copyright =
"Copyright 1996 - Obtuse Systems Corporation - All rights reserved.";
char *obtuse_rcsid = "$Id: smtpfwdd.c,v 1.3 1998/06/03 08:57:14 beck Exp $";

#include <stdio.h>
#include <signal.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#ifdef IRIX_BROKEN_INCLUDES
/* IRIX 5.3 defines EX_OK (see sysexits.h) as something very strange in unistd.h :-) */
#ifdef EX_OK
#undef EX_OK
#endif
#endif
#include <sysexits.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <utime.h>
#ifdef NEEDS_LOCKF_H
#include <sys/lockf.h>
#endif
#include "smtp.h"

#ifndef MAIL_AGENT
#define MAIL_AGENT "/usr/sbin/sendmail"
#endif

#ifndef LOG_FACILITY
#define LOG_FACILITY LOG_MAIL
#endif

#ifndef MAXCHILDREN
#define MAXCHILDREN 10
#endif

#ifndef MAXARGS
#define MAXARGS 100
#endif

#if MAXARGS < 6
%%%MAXARGS must be at least 6 %%%
#endif

#ifndef POLL_TIME
#define POLL_TIME 10
#endif

#ifndef SENDMAIL_RETRY
#define SENDMAIL_RETRY 1
#endif 

/*
 * How long to wait before trying to re-process a file
 */

#ifndef RETRY_DELAY_TIME
#define RETRY_DELAY_TIME	600
#endif

/* 
 * How long can a spool file be incomplete before I start 
 * yelling about it?
 */ 
#ifndef COMPLETION_WAIT
#define COMPLETION_WAIT         86400
#endif

char *spooldir = NULL;
char *mailagent = MAIL_AGENT;
int children = 0;
int maxchildren = MAXCHILDREN;
int poll_time = POLL_TIME;
int gc_int = COMPLETION_WAIT;
int VerboseSyslog = 1;


#ifdef SUNOS_GETOPT
extern char *optarg;
extern int optind;
#else
char *optarg;
int optind;
#endif

/*
 * zap spoolfile and leave 
 */
void
fail_abort(FILE * f, char *fname)
{
  if (unlink(fname) != 0) {
    /* we could be here after a sibling removed the file. If this is 
     * the case, no problem. Otherwise something's wrong with our
     * setup. 
     */
    if (errno != ENOENT) {
      syslog(LOG_CRIT, "Couldn't remove spool file %s! (%m)", fname);
      exit(EX_CONFIG);
    }
  }
#ifdef USE_LOCKF
  if (lockf(fileno(f), F_TLOCK, 0) == 0 &&
      lockf(fileno(f), F_ULOCK, 0) != 0) {
    syslog(LOG_ERR, "Couldn't unlock spool file %s using lockf after removal (%m)!", fname);
    exit(EX_CONFIG);
  }
#endif
#ifdef USE_FLOCK
  if (flock(fileno(f), LOCK_EX | LOCK_NB) == 0 &&
      flock(fileno(f), LOCK_UN) != 0) {
    syslog(LOG_ERR, "Couldn't unlock spool file %s using flock after removal (%m)!", fname);
    exit(EX_CONFIG);
  }
#endif
  fclose(f);
  exit(EX_DATAERR);
}

/*
 * leave and unlock spoolfile for retry 
 */
void
fail_retry(FILE * f, char *fname)
{
  struct utimbuf utb;

  /*
   * first make sure the others x bit is on so we know this file has
   * been tried. 
   */

  if (chmod(fname, 0755) != 0) {
    syslog(LOG_ERR, "Couldn't change mode of %s for retry (%m)! abandoning message!", fname);
    fail_abort(f, fname);
  }
  /*
   * touch the file, so we base the time of the next retry on the
   * current time. 
   */
  utb.actime = utb.modtime = time(NULL);
  if (utime(fname, &utb) != 0) {
    syslog(LOG_ERR, "Couldn't set modification time of %s for retry (%m)! abandoning message!", fname);
    fail_abort(f, fname);
  }
#ifdef USE_LOCKF
  if (lockf(fileno(f), F_TLOCK, 0) == 0)
    if (lockf(fileno(f), F_ULOCK, 0) != 0) {
      syslog(LOG_ERR, "Couldn't unlock spool file %s with lockf for retry (%m)!", fname);
      exit(EX_CONFIG);
    }
#endif
#ifdef USE_FLOCK
  if (flock(fileno(f), LOCK_EX | LOCK_NB) == 0)
    if (flock(fileno(f), LOCK_UN) != 0) {
      syslog(LOG_ERR, "Couldn't unlock spool file %s with flock for retry (%m)!", fname);
      exit(EX_CONFIG);
    }
#endif
  fclose(f);
  exit(EX_TEMPFAIL);
}

/*
 * is spool file fname complete? it's complete if it's mode 750.
 * This doesn't mean we can lock it, but means it's ok to try. 
 */
int
smtp_spoolfile_complete(const char *fname)
{
  struct stat buf;

  if (stat(fname, &buf) != 0) {
    /*
     * If the file doesn't exist then some other child just finished
     * processing it - not a problem!
     * Anything else is a serious problem (OS is probably insane).
     */
    if (errno != ENOENT) {
      syslog(LOG_CRIT, "Can't stat %s (%m) - bye!", fname);
      exit(EX_CONFIG);
    }
    return (0);
  }
  if (!(S_ISREG(buf.st_mode))) {
    syslog(LOG_CRIT, "Spool file %s isn't a regular file!", fname);
    exit(EX_CONFIG);
  }
  if ((buf.st_mode & 0110) != 0110) {
#ifdef VERBOSE     
     syslog(LOG_DEBUG, "%s not complete now.", fname);
#endif     
     if (gc_int && (buf.st_mtime+gc_int <= time(NULL))) {
	/* 
	 * This file has been hanging around incomplete for more than
	 * gc_int, This could be due simply to a (really) slow connection/big
	 * message tying up an smtpd process for a long time, or it could
	 * be due to something like the machine being rebooted killing off
	 * an smtpd process that had started to receive a message before
	 * it was able to finish and mark this file as complete.
	 * 
	 * Therefore we better let the appropriate authority know about this
	 * file.
	 */
	struct utimbuf utb;
	utb.actime = utb.modtime = time(NULL);
	if (utime(fname, &utb) != 0) {
	   syslog(LOG_ALERT, "utime() failed on spool file %s (%m)", fname);
	}
	syslog(LOG_ALERT, "Spool file %s has been incomplete since %s. Please investigate.", fname, ctime(&(buf.st_ctime)));
     }
    return (0);
  }
  if ((buf.st_mode & 0111) == 0111) {
    /*
     * if the others execute bit is ticked, then this file had been
     * previously tried, and we got a temp. sendmail failure. We
     * don't want to retry too often, so make sure the mtime is more 
     * than RETRY_DELAY_TIME seconds ago. 
     */
    if ((time(NULL) - buf.st_mtime) < RETRY_DELAY_TIME) {
#ifdef VERBOSE       
      syslog(LOG_DEBUG, "Skipping file %s, delivery last attempted at %s.", fname, ctime(&(buf.st_mtime)));
#endif
      return (0);
    }
    else { 
      syslog(LOG_DEBUG, "Retrying delivery of file %s, last attempted at %s.", fname, ctime(&(buf.st_mtime)));
    }
  }
  return (1);
}

/*
 * Generate obituaries for our dead children and keep track of how many
 * of our kids are still alive.
 */

void
reap_children(void)
{
  while (1) {
    pid_t pid;
    int status;

    pid = waitpid(-1, &status, WNOHANG);

    if (pid == 0) {
      return;
    } else if (pid == -1) {
      if (errno != ECHILD) {
	syslog(LOG_CRIT, "CRITICAL - waitpid failed (%m) - aborting");
	abort();
      }
      return;
    }
    children--;
    if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
      switch (WEXITSTATUS(status)) {
      case EX_TEMPFAIL:
	
	/*
	 * normal retry case 
	 */
	syslog(LOG_DEBUG, "Child process (%d) exited indicating retry", pid);
	break;
      case EX_CONFIG:
	/*
	 * we only exit with this code if we know we've got
	 * configuration problems. If a child exits like this,
	 * we also should exit 
	 */
	syslog(LOG_CRIT, "Child process (%d) failed due to configuration problems. Exiting", pid);
	exit(EX_CONFIG);
	break;
      default:
	/*
	 * permanent failure or something unusual 
	 */
	syslog(LOG_DEBUG, "Child process (%d) failed - no retry", pid);
	break;
      }
    }
  }
}

/*
 * Say something vaguely useful
 */

void
show_usage()
{
  fprintf(stderr, "usage: smtpdfwdd [-u user] [-g group] [-g spooldir] [-s sendmailprog]\n");
}

/*
 * forward a mail message recieved by smtpd contained in file fname.
 * file is expected to be as follows:
 * -------------------
 * FROM addr
 * RCPT addr          (or SENT addr)
 * ...
 * BODY
 * message body
 * ...
 * -------------------
 *
 * The FROM line indicates who sent this message.
 * The RCPT lines each indicate an intended recipient.
 * Any SENT lines indicate recipients that this message has already been
 * delivered to (these only happen if a message is partially processed
 * before sendmail signals a temporary failure).
 *
 * Everything before "BODY" will have been sanitized by smtpd. It's up
 * to us to do anything we want to the message body, as smtpd takes that
 * in verbatim.
 *
 * A message is processed MAXARGS recipients at a time.  As each batch
 * is processed, the RCPT verbs for the batch are turned into SENT verbs.
 * This prevents the message from being sent to the same people more than
 * once if a subsequent batch fails with a retry-able error.  It also
 * limits the number of people who will get the message twice if the system
 * crashes at a bad moment.
 *
 * We call MAIL_AGENT -f fromaddr toaddr toaddr toaddr ...
 * to forward mail. I.E. MAIL_AGENT should be sendmail or something
 * else that delivers mail and will take those arguments a-la sendmail.
 * For filtering message bodies for unwanted things, one can call a filter
 * program which checks the message body as or before passing it through
 * to a delivery program. MAIL_AGENT needs to exit with sendmail-like
 * exit codes.
 *
 * We exit with
 *  EX_TEMPFAIL - Retry later for whatever reason
 *  EX_CONFIG - Something's horribly wrong, and our parent should exit
 *  EX_OK - We have removed the spoolfile after success
 *  anything else - We have removed the spoolfile after failure (no retry)
 */

void
forward(char *fname)
{
  FILE *f = NULL;
  char line[SMTP_MAX_CMD_LINE];
  char *c, *from;
  int sentout;
  off_t body;
  struct smtp_victim *victim, *victims;

  victim = (struct smtp_victim *) malloc(sizeof(struct smtp_victim));

  victim->name = NULL;
  victim->next = NULL;
  victims = victim;

  if (victims == NULL) {
    syslog(LOG_ERR, "Malloc failed, aborting delivery of %s", fname);
    fail_abort(f, fname);
  }
  /*
   * Step 1 - open the file for updating. exit silently if it fails, 
   * since that is most likely due to one of our siblings having dealt
   * with it and removed it.
   */

  f = fopen(fname, "r+");
  if (f == NULL) {
    syslog(LOG_CRIT, "Couldn't open spool file %s! (%m)", fname);
    exit(EX_TEMPFAIL);
  }
  /*
   * Step 2 - try to get a non-blocking exclusive lock on the file.
   * Just exit (relatively) silently if it fails.  This happens for a number
   * of reasons:
   *
   *    - one of our siblings has already got the file
   *    - smtpd isn't done with it yet
   */

#ifdef USE_LOCKF
  if (lockf(fileno(f), F_TLOCK, 0) != 0) {
    syslog(LOG_DEBUG, "Couldn't lock spool file %s using lockf (%m)", fname);
    exit(EX_TEMPFAIL);
  }
#endif
#ifdef USE_FLOCK
  if (flock(fileno(f), LOCK_EX | LOCK_NB) != 0) {
    syslog(LOG_DEBUG, "Couldn't lock spool file %s using flock (%m)", fname);
    exit(EX_TEMPFAIL);
  }
#endif

  /*
   * Step 3 - do a basic sanity test on the file
   *
   * We do the test using the file's name instead of the just opened
   * file descriptor to avoid the following race condition:
   *
   *    - we and one of our siblings both open the file successfully above
   *    - we're suspended while our sibling completely processes the file
   *      (including unlinking the file).
   *    - we finally get around to locking the file.  Since our sibling is
   *      done, the lock attempt works.
   *    - we do the sanity test using the file descriptor (which is associated
   *      with a file that no longer has a name).
   *    - we process the file again.
   *
   * By doing the following sanity check using the file's name instead
   * of the file descriptor, we avoid the race because, if the above sequence
   * of events occurs, the file won't exist when we do the sanity test
   * (which will cause the sanity test to fail).
   *
   */

  if (!smtp_spoolfile_complete(fname)) {
    /*
     * smtpd hasn't finished with this one yet or the file is gone.
     * Bail out.  If the file still exists, it will get tried again later.
     */

    /* If we locked the file (above) and have discovered it isn't complete,
     * be sure to unlock it. Sadly, some OS's seem to think that locks
     * can stay after a process goes away. Sigh. -BB
     */

#ifdef USE_LOCKF
    if (lockf(fileno(f), F_TLOCK, 0) == 0)
      if (lockf(fileno(f), F_ULOCK, 0) != 0) {
	syslog(LOG_ERR, "Couldn't unlock incomplete spool file %s (%m)!", fname);
	exit(EX_CONFIG);
      }
#endif
#ifdef USE_FLOCK
    if (flock(fileno(f), LOCK_EX | LOCK_NB) == 0)
      if (flock(fileno(f), LOCK_UN) != 0) {
	syslog(LOG_ERR, "Couldn't unlock incomplete spool file %s (%m)!", fname);
	exit(EX_CONFIG);
      }
#endif

    exit(EX_TEMPFAIL);
  }
  /*
   * parse file 
   */

  if (fgets(line, sizeof(line), f) == NULL) {
    syslog(LOG_ERR, "read failed on spool file %s (%m) - message not forwarded", fname);
    fail_abort(f, fname);
  }
  line[SMTP_MAX_CMD_LINE - 1] = '\0';

  if (strncmp(line, "FROM ", 5) != 0) {
    syslog(LOG_ERR, "File %s corrupt (no FROM line) - message not forwarded", fname);
    fail_abort(f, fname);
  }
  c = strchr(line, '\n');
  if (c == NULL) {
    syslog(LOG_ERR, "FROM line too long in %s - message not forwarded", fname);
    fail_abort(f, fname);
  }
  *c = '\0';
  from = strdup(line + 5);
  if (from == NULL) {
    syslog(LOG_INFO, "Malloc failed - retrying later");
    fail_retry(f, fname);
  }

#if STRIP_QUOTES
  /* remove <> quotes from sender, as some MTA's (like qmail) don't deal
   * with it well. 
   */
  if ((from[0]=='<') && (from[strlen(from)-1]=='>')) {
     from[strlen(from)-1]='\0';
     from++;
  }
#endif
	
  for (;;) {
    long vloc;

    vloc = ftell(f);
    if (fgets(line, sizeof(line), f) == NULL) {
      syslog(LOG_ERR, "read failed on spool file %s (%m) - message not forwarded", fname);
      fail_abort(f, fname);
    }
    line[SMTP_MAX_CMD_LINE - 1] = '\0';
    if (strncmp(line, "SENT ", 5) == 0) {
      /*
       * we already sent it to this victim on a previous attempt. 
       */
      continue;
    }
    if (strncmp(line, "RCPT ", 5) != 0) {
      break;
    }
    /*
     * we have a RCPT 
     */
    if (victim->name != NULL) {
      victim->next = (struct smtp_victim *) malloc(sizeof(struct smtp_victim));

      victim = victim->next;
      victim->name = NULL;
      victim->next = NULL;
    }
    c = strchr(line, '\n');
    if (c == NULL) {
      syslog(LOG_ERR, "RCPT line too long in %s - message not forwarded", fname);
      fail_abort(f, fname);
    }
    *c = '\0';
    if ((victim->name = strdup(line + 5)) == NULL) {
      syslog(LOG_INFO, "Malloc failed - retrying later");
      fail_retry(f, fname);
    }
#if STRIP_QUOTES
    /* again, strip <> if present in case MTA can't handle it */
    if ((victim->name[0]=='<') && (victim->name[strlen(victim->name)-1]=='>')) {
       victim->name[strlen(victim->name)-1]='\0';
       (victim->name)++;
    }
#endif
    victim->location = vloc;
  }

  c = strchr(line, '\n');
  if (c == NULL) {
    syslog(LOG_ERR, "BODY line too long in %s - message not forwarded", fname);
    fail_abort(f, fname);
  }
  *c = '\0';
  if (strcmp(line, "BODY") != 0) {
    syslog(LOG_ERR, "File %s corrupt (no BODY after RCPT) - message not forwarded", fname);
    fail_abort(f, fname);
  }
  /*
   * We're now at the start of our message body with the list of
   * recipients in "victims" and the sender in "from". fire off our
   * mail program to send it out 
   */
  body = ftell(f);
  victim = victims;
  sentout = 0;
  if (!VerboseSyslog) {
    accumlog(LOG_INFO, "Forwading %s", fname);
  }
  while (victim != NULL) {
    int status, pid, pidw, i, rstart;
    struct smtp_victim *sv = victim;
    char *av[MAXARGS];

    i=0;
    av[i++] = mailagent;
#if SENDMAIL_OITRUE
    if (strstr(mailagent, "sendmail") != 0) {
      /*
       * Sendmail has a feature/bug that it will by default 
       * stop on a line with just a '.'. We need to 
       * tell sendmail to ignore a line that contains just a '.' 
       * otherwise it decides that it's the end of the message. 
       * We may not need this if "sendmail" isn't really sendmail.
       * (for example, qmail's phony "sendmail" that calls qmail-inject
       * doesn't need this).
       */
      av[i++] = "-oiTrue";
    }
#endif
    av[i++] = "-f";
    av[i++] = from;
    rstart = i;
    while (i < MAXARGS - 2) {
      if (VerboseSyslog) {      
	syslog(LOG_INFO, "forwarding to recipient %s", victim->name);
      } else {
	accumlog(LOG_INFO, " to=%s", victim->name);
      }
      av[i++] = victim->name;
      victim = victim->next;
      if (victim == NULL) {
	break;
      }
    }
    av[i] = NULL;

    if ((pid = fork()) == 0) {
      int xerrno;

      close(0);
      close(1);
      close(2);
      if (dup(fileno(f)) != 0) {
	syslog(LOG_ERR, "Couldn't dup open %s to stdin (%m)", fname);
	exit(EX_OSERR);
      }

      fclose(f);
      closelog();
      if (lseek(0, body, SEEK_SET) < 0) {
	syslog(LOG_ERR, "Can't lseek spool file %s! (%m)", fname);
	exit(EX_OSERR);
      }
      execv(av[0], av);
      xerrno = errno;
      openlog("smtpfwdd", LOG_PID | LOG_NDELAY, LOG_FACILITY);
      errno = xerrno;
      if (errno == ENOMEM) {
	syslog(LOG_INFO, "exec of %s failed (%m) - retrying it later", av[0]);
	fail_retry(fdopen(0, "r+"), fname);
      } else {
	syslog(LOG_CRIT, "exec of %s failed! (%m)", av[0]);
	exit(EX_CONFIG);
      }
    } else if (pid < 0) {
      syslog(LOG_INFO, "fork failed - retrying message later");
      fail_retry(f, fname);
    }
    do {
      pidw = wait(&status);
    }
    while ((pidw != pid) && (pidw != -1));

    if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
      /*
       * Sendmail go boom.  boo hoo. 
       */
      switch (WEXITSTATUS(status)) {
#if SENDMAIL_RETRY
      case EX_OSERR:
      case EX_OSFILE:
      case EX_IOERR:	
      case EX_UNAVAILABLE:
      case EX_TEMPFAIL:
	syslog(LOG_INFO, "Temporary sendmail failure (status %d), will retry later", status);
	fail_retry(f, fname);
	break;
#endif
#ifdef EX_NOUSER
      case EX_NOUSER:
	syslog(LOG_INFO, "Sendmail exited indicating one or more local recipients did not exist (no retry)");
	fail_abort(f, fname);  
#endif
      case EX_CONFIG:
	syslog(LOG_CRIT, "Sendmail configuration error!");
	exit(EX_CONFIG);
      default:
	syslog(LOG_INFO, "Sendmail exited abnormally (status %d) - message not forwarded.", status);
	fail_abort(f, fname);
      }
    }
    /*
     * yippee. so far so good 
     */
    sentout += (i - rstart);

    if (victim != NULL) {

      /*
       * We got more, and have to do it all again. Before we do,
       * tag the existing recipients who got sent out by changing
       * RCPT to SENT in the spoolfile. In this way we avoid 
       * delivering this again if we have a temporary sendmail
       * failure and retry after having sent it out to part of the 
       * recipients successfully. 
       */


      for (i = rstart; i < (MAXARGS - 2); i++) {
	if (fseek(f, sv->location, SEEK_SET) != 0) {
	  syslog(LOG_ERR, "Couldn't fseek %s (%m)\n - message abandoned after delivery to first %d recipients", fname, sentout);
	  fail_abort(f, fname);
	}
	fprintf(f, "SENT");
	fflush(f);
	sv = sv->next;
	if (sv == NULL) {
	  break;
	}
      }


      if (fseek(f, body, SEEK_SET) != 0) {
	syslog(LOG_ERR, "Couldn't fseek %s (%m)\n - message abandoned after delivery to first %d recipients", fname, sentout);
	fail_abort(f, fname);
      }
    }
  }

  /*
   * All seems to have worked 
   */
  if (VerboseSyslog) {
    syslog(LOG_INFO, "%s forwarded to %d recipients", fname, sentout);
  } else {
    accumlog(LOG_INFO, ", forwarded to %d recipients", sentout);
    accumlog(LOG_INFO, 0);		/* flush */
  }    
  if (unlink(fname) != 0) {
    syslog(LOG_CRIT, "Couldn't remove spool file %s! (%m)", fname);
    exit(EX_CONFIG);
  }
#ifdef USE_LOCKF
  if (lockf(fileno(f), F_TLOCK, 0) == 0)
    if (lockf(fileno(f), F_ULOCK, 0) != 0) {
      syslog(LOG_ERR, "Couldn't unlock spool file %s using lockf after removal (%m)!", fname);
      exit(EX_CONFIG);
    }
#endif
#ifdef USE_FLOCK
  if (flock(fileno(f), LOCK_EX | LOCK_NB) == 0)
    if (flock(fileno(f), LOCK_UN) != 0) {
      syslog(LOG_ERR, "Couldn't unlock spool file %s using flock after removal (%m)!", fname);
      exit(EX_CONFIG);
    }
#endif

  fclose(f);
  exit(EX_OK);
}

/*
 * The brains of this operation
 */

int
main(int argc, char **argv)
{
  int opt;
  char *optstring = "qu:g:d:s:M:P:";
  int pid;

  char *username = SMTP_USER;
  char *groupname = SMTP_GROUP;
  struct passwd *user = NULL;
  struct group *group = NULL;

  openlog("smtpfwdd", LOG_PID | LOG_NDELAY, LOG_FACILITY);

  /*
   * grab arguments 
   */
#ifdef GETOPT_EOF
  while ((opt = getopt(argc, argv, optstring)) != EOF) {
#else
  while ((opt = getopt(argc, argv, optstring)) > 0) {
#endif
    switch (opt) {
    case 'q':
      VerboseSyslog = 0;
      break;    
    case 'd':
      if (optarg[0] != '/') {
	fprintf(stderr, "The \"-d\" option requires an absolute pathname argument, \"%s\" is bogus\n", optarg);
	show_usage();
	exit(EX_CONFIG);
      }
      spooldir = optarg;
      break;
    case 's':
      if (optarg[0] != '/') {
	fprintf(stderr, "The \"-s\" option requires an absolute pathname argument, \"%s\" is bogus\n", optarg);
	show_usage();
	exit(EX_CONFIG);
      }
      mailagent = optarg;
      break;
    case 'M':
      {
	long newmax;
	char *foo;

	newmax = strtol(optarg, &foo, 10);
	if (*foo == '\0') {
	  if (newmax > 1000 || newmax < 1) {
	    fprintf(stderr, "Unreasonable (%ld) max children value\n", newmax);
	    show_usage();
	    exit(EX_CONFIG);
	  }
	  maxchildren = newmax;
	} else {
	  fprintf(stderr, "The \"-M\" option requires a positive integer argument, \"%s\" is bogus\n", optarg);
	  show_usage();
	  exit(EX_CONFIG);
	}
      }
      break;
    case 'P':
      {
	long newpoll;
	char *foo;

	newpoll = strtol(optarg, &foo, 10);
	if (*foo == '\0') {
	  if (newpoll > 1000 || newpoll < 1) {
	    fprintf(stderr, "Unreasonable (%ld) max poll value\n", newpoll);
	    show_usage();
	    exit(EX_CONFIG);
	  }
	  poll_time = newpoll;
	} else {
	  fprintf(stderr, "The \"-P\" option requires a positive integer argument, \"%s\" is bogus\n", optarg);
	  show_usage();
	  exit(EX_CONFIG);
	}
      }
      break;
    case 'u':
      {
	long userid;
	char *foo;

	userid = strtol(optarg, &foo, 10);
	if (*foo == '\0') {
	  /*
	   * looks like we got something that looks like a
	   * number, try to find user by uid 
	   */
	  user = getpwuid((uid_t) userid);
	  if (user == NULL) {
	    fprintf(stderr, "Invalid uid argument for the \"-u\" option, no user found for uid %s\n", optarg);
	    show_usage();
	    exit(EX_CONFIG);
	  }
	  username = user->pw_name;
	} else {
	  /*
	   * optarg didn't look like a number, so try looking it 
	   * up as a username.
	   */
	  user = getpwnam(optarg);
	  if (user == NULL) {
	    fprintf(stderr, "Invalid username argument for the \"-u\" option, no user found for name %s\n", optarg);
	    show_usage();
	    exit(EX_CONFIG);
	  }
	  username = user->pw_name;
	}
      }
      break;
    case 'g':
      {
	long grpid;
	char *foo;

	grpid = strtol(optarg, &foo, 10);
	if (*foo == '\0') {
	  /*
	   * looks like we got something that looks like a
	   * number try to find user by uid 
	   */
	  group = getgrgid((gid_t) grpid);
	  if (group == NULL) {
	    fprintf(stderr, "Invalid gid argument for the \"-g\" option, no group found for gid %s\n", optarg);
	    show_usage();
	    exit(EX_CONFIG);
	  }
	  groupname = group->gr_name;
	} else {
	  /*
	   * optarg didn't look like a number, so try looking it 
	   * up as a * groupname. 
	   */
	  group = getgrnam(optarg);
	  if (group == NULL) {
	    fprintf(stderr, "Invalid groupname argument for the \"-g\" option, no group found for name %s\n", optarg);
	    show_usage();
	    exit(EX_CONFIG);
	  }
	  groupname = group->gr_name;
	}
      }
      break;
    default:
      fprintf(stderr, "Unknown option \"-%c\"\n", opt);
      show_usage();
      exit(EX_CONFIG);
      break;
    }
  }

  /*
   * OK, got my options, now change uid/gid 
   */
  if (user == NULL) {
    /*
     * none provided, use the default 
     */
    long userid;
    char *foo;

    userid = strtol(username, &foo, 10);
    if (*foo == '\0') {
      /*
       * looks like we got something that looks like a number * try
       * to find user by uid 
       */
      user = getpwuid((uid_t) userid);
      if (user == NULL) {
	fprintf(stderr, "Eeek! I was compiled to run as uid %s, but no user found for uid %s\n", username, username);
	fprintf(stderr, "Please recompile me to use a valid user, or specify one with the \"-u\" option.\n");
	exit(EX_CONFIG);
      }
      username = user->pw_name;
    } else {
      /*
       * username didn't look like a number, so try looking it up as 
       * a username. 
       */
      user = getpwnam(username);
      if (user == NULL) {
	fprintf(stderr, "Eeek! I was compiled to run as user \"%s\", but no user found for username \"%s\"\n", username, username);
	fprintf(stderr, "Please recompile me to use a valid user, or specify one with the \"-u\" option.\n");
	exit(EX_CONFIG);
      }
      username = user->pw_name;
    }
  }
  if (group == NULL) {
    /*
     * didn't get a group, use the default 
     */
    long grpid;
    char *foo;

    grpid = strtol(groupname, &foo, 10);
    if (*foo == '\0') {
      /*
       * looks like we got something that looks like a number, try
       * to find group by gid 
       */
      group = getgrgid((gid_t) grpid);
      if (group == NULL) {
	fprintf(stderr, "Eeek! I was compiled to run as gid %s, but no group found for gid %s\n", groupname, groupname);
	fprintf(stderr, "Please recompile me to use a valid group, or specify one with the \"-g\" option.\n");
	exit(EX_CONFIG);
      }
      groupname = group->gr_name;
    } else {
      /*
       * groupname didn't look like a number, so try looking it up
       * as a groupname. 
       */
      group = getgrnam(groupname);
      if (group == NULL) {
	fprintf(stderr, "Eeek! I was compiled to run as group \"%s\", but no group found for groupname \"%s\"\n", groupname, groupname);
	fprintf(stderr, "Please recompile me to use a valid group, or specify one with the \"-g\" option.\n");
	exit(EX_CONFIG);
      }
      groupname = group->gr_name;
    }
  }
  /*
   * If we're here, we have a valid user and group to run as 
   */
  if (group == NULL || user == NULL) {
    fprintf(stderr, "Didn't find a user or group, (Shouldn't happen)\n");
    abort();
  }
  if (user->pw_uid == 0) {
    fprintf(stderr, "Sorry, I don't want to run as root! It's a bad idea!\n");
    fprintf(stderr, "Please recompile me to use a valid user, or specify one with the \"-u\" option.\n");
    exit(EX_CONFIG);
  }
  if (group->gr_gid == 0) {
    fprintf(stderr, "Sorry, I don't want to run as group 0. It's a bad idea!\n");
    fprintf(stderr, "Please recompile me to use a valid group, or specify one with the \"-g\" option.\n");
    exit(EX_CONFIG);
  }
  if (setgid(group->gr_gid) != 0) {
    perror("Setgid failed!");
    exit(EX_CONFIG);
  }
  if (setuid(user->pw_uid) != 0) {
    perror("Setuid failed!");
    exit(EX_CONFIG);
  }

  /* If we didn't get a spooldir, use the default SPOOLDIR.SPOOLSUBDIR */
  if (spooldir == NULL) {
    spooldir = (char *) malloc((strlen(SPOOLDIR) + strlen(SPOOLSUBDIR) + 2)
			       * sizeof(char));
    if (spooldir == NULL) {
	fprintf(stderr, "Malloc failed allocating room for spooldir filename! Can't continue, Sorry!\n");
	exit(EX_OSERR);
    }
    sprintf(spooldir, "%s/%s", SPOOLDIR, SPOOLSUBDIR);
  }
  
  /*
   * OK, we're now running as a non-root user and group, hopefully one
   * that can run sendmail -f and have it work. 
   */

  if (chdir(spooldir) != 0) {
    perror("Chdir failed!");
    fprintf(stderr, "Can't change directory to spooldir %s\n", spooldir);
    exit(EX_CONFIG);
  }
  if ((pid = fork()) != 0) {
    if (pid < 0) {
      syslog(LOG_CRIT, "fork failed (%m) while trying to become a daemon");
    }
    exit(EX_OSERR);
  } else {
    DIR *dir;

    /*
     * Try to get a semaphore file. Prevents multiple instances of
     * smtpfwdd from running at once on the same spool directory. 
     */

    {
      int lfd;
      char tbuf[100];

      lfd = open(".smtpfwdd.lock", O_WRONLY | O_CREAT, 0644);
      if (lfd < 0) {
	syslog(LOG_CRIT, "can't open semaphore file in \"%s\" (%m) - bye!", spooldir);
	exit(EX_CONFIG);
      }
#ifdef USE_LOCKF
      if (lockf(lfd, F_TLOCK, 0) != 0) {
	syslog(LOG_ERR, "I'm already running in %s", spooldir);
	exit(EX_CONFIG);
      }
#endif
#ifdef USE_FLOCK
      if (flock(lfd, LOCK_EX | LOCK_NB) != 0) {
	syslog(LOG_ERR, "I'm already running in %s", spooldir);
	exit(EX_CONFIG);
      }
#endif

      /*
       * Done - put our pid in the semaphore file.
       * Note that we keep the semaphore file open but forget the file's fd.
       */

      sprintf(tbuf, "%7d\n", (int) getpid());
      write(lfd, tbuf, strlen(tbuf));
    }

    setsid();

    signal(SIGCHLD, SIG_DFL);

    dir = opendir(".");
    if (dir == NULL) {
      syslog(LOG_CRIT, "Can't open directory %s (%m) - exiting",
	     spooldir);
      exit(EX_CONFIG);
    }
    for (;;) {
      struct dirent *direct;
      int cpid;

      while ((direct = readdir(dir)) != NULL) {
	int groks = 0;

	reap_children();
	while (children >= maxchildren) {
	  groks++;
	  if (groks == 60) {
	    syslog(LOG_ERR, "Too many children for last minute! Please investigate!");
	    groks = 0;
	  }
	  sleep(1);
	  reap_children();
	}
 	if (!VerboseSyslog) {
	  /* should be empty - but just in case */
	  accumlog(LOG_INFO, 0);
	}
	/*
	 * If we have a file with an appropriate name and it is
	 * complete then create a child which will try to forward the
	 * message.
	 */
	if (strncmp(direct->d_name, "smtpd", 5) == 0
	    && smtp_spoolfile_complete(direct->d_name)) {
	  children++;
	  if ((cpid = fork()) == 0) {
	    forward(direct->d_name);
	    /*
	     * NOTREACHED 
	     */
	    syslog(LOG_CRIT,
		   "Returned from forward()! SHOULD NOT HAPPEN!");
	    exit(EX_CONFIG);
	  }
	  if (cpid < 0) {
	    syslog(LOG_ERR, "Fork failed! (%m)");
	    children--;
	  }
	}
      }
      rewinddir(dir);
      sleep(poll_time);
    }
  }
}
