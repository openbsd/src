/*
 * smtpd, Obtuse SMTP daemon, storing agent. does simple collection of
 * mail messages, for later forwarding by smtpfwdd.
 *
 * $Id: smtpd.c,v 1.7 1998/07/10 08:06:15 deraadt Exp $
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
char *obtuse_rcsid = "$Id: smtpd.c,v 1.7 1998/07/10 08:06:15 deraadt Exp $";

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <arpa/nameser.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef IRIX_BROKEN_INCLUDES
/* IRIX 5.3 defines EX_OK (see sysexits.h) as something very strange in unistd.h :-) */
#ifdef EX_OK
#undef EX_OK
#endif
#endif
#include <syslog.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/utsname.h>
#include <string.h>
#include <sysexits.h>
#include <ctype.h>
#ifdef NEEDS_STRINGS_H
#include <strings.h>
#endif
#ifdef NEEDS_FCNTL_H
#include <fcntl.h>
#endif
#ifdef NEEDS_LOCKF_H
#include <sys/lockf.h>
#endif
#ifdef NEEDS_BSTRING_H
#include <bstring.h>
#endif
#ifdef NEEDS_SELECT_H
#include <sys/select.h>
#endif

#include "smtp.h"
#include "smtpd.h"
#ifdef BROKEN_SUN_INCLUDES
/* SunOS 5.5 doesn't seem to want to prototype this anywhere - Sigh */
extern int gethostname(char *name, int len);
#endif

#ifndef READ_TIMEOUT
#define READ_TIMEOUT 600
#endif

#ifndef NO_HOSTCHECKS
#define NO_HOSTCHECKS 0
#endif

#ifndef PARANOID_SMTP
#define PARANOID_SMTP 0
#endif

#ifndef PARANOID_DNS
#define PARANOID_DNS 0
#endif

#ifndef SPOOLSUBDIR
#define SPOOLSUBDIR "."
#endif

#ifndef LOG_FACILITY
#define LOG_FACILITY LOG_MAIL
#endif

#ifndef CHECK_FILE
#define CHECK_FILE "/etc/smtpd_check_rules"
#endif

#if SET_LOCALE
#include <locale.h>
#endif

#define SPANBLANK(p)	while (isspace(*p)) p += 1

struct peer_info peerinfo;
struct sockaddr_in peer_sa;
struct sockaddr_in my_sa;

char *my_name = NULL;
char *current_from_mailpath = NULL;
char *client_claimed_name = "UNKNOWN";
char *spoolfile = NULL;
char *spooldir = SPOOLSUBDIR;		/* this is relative to our chroot. */
int read_timeout = READ_TIMEOUT;
long maxsize = 0;
int outfd, replyfd;
#ifdef SUNOS_GETOPT
extern char *optarg;
extern int optind;
#else
char *optarg;
int optind;
#endif
struct smtp_mbuf *input_buf, *output_buf, *reply_buf;
int NoHostChecks = NO_HOSTCHECKS;
int Paranoid_Smtp = PARANOID_SMTP;
int Paranoid_Dns = PARANOID_DNS;
int exiting = 0;
int VerboseSyslog = 1;

#ifndef SMTPD_PID_DIR
#if defined(OpenBSD) || defined(FreeBSD) || defined(NetBSD)
# define SMTPD_PID_DIR "/var/run"
#else
# define SMTPD_PID_DIR SPOOLDIR
#endif
#endif

#ifndef SMTPD_PID_FILENAME
#define SMTPD_PID_FILENAME "smtpd.pid"
#endif


/*
 * Generate the usual cryptic usage statement
 */


void
show_usage()
{
#if NO_COMMANDLINE_OPTIONS
  fprintf(stderr, "this version of smtpd was compiled without command line option support\n");
#else
  fprintf(stderr, "usage: smtpd [-c chrootdir] [-d spooldir] [-u user] [-g group]\n");
  fprintf(stderr, "             [-m maxsize] [-H] [-P] [-D]\n");
#endif /* NO_LINE_OPTIONS */
}

#if CHECK_ADDRESS
char * make_check_fail_reply(char *user, char *host, char *hostIP, 
			     char *from, char *to, char *msg) 
{
  static char replybuf[512]; /* static buffer that returns reply */
  char *c;
  int i = 0;
  int bogus = 0;

  if (user == NULL) {
    user = "UNKNOWN";
  }
  if (host == NULL) {
    host = "UNKNOWN";
  }
  if (msg == NULL) {
    msg = "550 Mail from %F to %T prohibited from your location (%U@%H ip=%I)";
  }
  c = msg;
  if (!isdigit(*c)) {
    /* do some very rudimentary checking. beyond this hope they
     * know what they're doing
     */
    syslog(LOG_ERR, "Reply message doesn't start with numeric code" );
    bogus = 1;
  }
  
  while (*c != '\0' && !bogus && i<512) {
    if (*c != '%') {
      replybuf[i]=*c;
      c++;
      i++;
    }
    else {
      char *add;
      int len;
      c++;
      switch (*c) {
      case '%':
	add = "%";
	break;
      case 'C':
	add = ":";
	break;
      case 'F':
	add = from;
	break;
      case 'T':
	add = to;
	break;
      case 'H':
	add = host;
	break;
      case 'I':
	add = hostIP;
	break;
      case 'U':
	add = user;
	break;
      default:
	syslog(LOG_ERR, "Unknown code %%%c in reply message", *c);
	add = "";
	bogus = 1;
	break;
      }
      len = strlen(add);
      if (len > 128) {
	syslog(LOG_NOTICE, "Very long (%d bytes) value obtained for %%%c in reply message", len, *c);
      }
      if (len+i >= 512) {
	syslog(LOG_ERR, "reply message too long - truncating at 512 bytes");
      }
      strncpy(replybuf+i, add, 511 - i);
      replybuf[511]='\0';
      i = strlen(replybuf);
      c++;
    }
  }
  if (!bogus) {
    replybuf[(i<512)?i:511] = '\0';
    return(replybuf);
  }
  else {
    msg = "550 Recipient not allowed";
    return(msg);
  }
}
#endif  


/*
 * Signal handler that shuts us down if a read on the socket times out
 */

static
void
read_alarm_timeout(int s)
{
  if (s != SIGALRM) {
    syslog(LOG_CRIT, "Read timeout alarm handler called with signal %d!, (not SIGALRM!) - Aborting!", s);
    abort();
  }
  syslog(LOG_ERR, "Timeout on read (more than %d seconds) - Abandoning session", read_timeout);
  smtp_exit(EX_OSERR);
}

#ifdef NO_MEMMOVE
/*
 * Use bcopy on platforms that don't support the newer memmove function
 */

void
memmove(void *to, void *from, int len)
{
  bcopy(from, to, len);
}

#endif

/*
 * Return to the initial state
 */

void
reset_state(smtp_state state)
{
  if (test_state(OK_HELO, state)) {
    zap_state(state);
    set_state(OK_HELO, state);
  } else {
    zap_state(state);
  }

  /*
   * we must throw away anything in the output buffer 
   */
  output_buf->tail = output_buf->data;
  output_buf->offset = 0;
}

/*
 * returns the index of the start of the first end-of-command token *
 * in buf. returns len if it couldn't find one. 
 */

int
crlf_left(unsigned char *buf, size_t len)
{
  int i;

  i = 0;
  while (i < len) {
#ifdef CRLF_PEDANTIC
    /*
     * This is how the RFC says the world should work.
     * Unfortunately, it doesn't.
     */
    if (buf[i] == CR)
      if (i < (len - 1))
	if (buf[i + 1] == LF)
	  return (i);
#else
    /*
     * The world works like this.
     */
    if (buf[i] == LF) {
      if (i > 0 && buf[i - 1] == CR) {
	return (i - 1);
      } else {
	return (i);
      }
    }
#endif
    i++;
  }
/*
 * couldn't find one 
 */
  return (len);
}

/*
 * find next crlf in buf, starting at offset. On finding one, replaces 
 * the first byte of crlf with \0, sets offset to first byte after end 
 * of crlf, and returns start of string if we don't find one we return 
 * NULL 
 */

char *
smtp_get_line(struct smtp_mbuf *mbuf, size_t * offset)
{
  int i;
  size_t len;
  unsigned char *buf;

  buf = mbuf->data + *offset;
  len = mbuf->offset - *offset;

  if (len == 0) {
    return (NULL);
  }
  i = crlf_left(buf, len);
  if (i < (len)) {
    buf[i] = '\0';
    /*
     * jump over end of line token 
     */
    i++;

    if ((i < len) && (buf[i] == LF)) {
      i++;
    }
    *offset += i;

    return (buf);
  }
  return (NULL);
}

/*
 * flush len bytes of an mbuf to file descriptor fd.
 */

void
flush_smtp_mbuf(struct smtp_mbuf *buf, int fd, int len)
{
  int foo = 0;
  static int deaththroes=0;


  if (deaththroes) {
    return; /* We've already had a write barf. Don't try again */ 
  }
  if (len <= buf->offset) {
    while (foo < len) {
      int i;
      
      i = write(fd, (buf->data) + foo, len);
      if (i < 0) {
	syslog(LOG_INFO, "write failed: (%m)");
	deaththroes=1;
	smtp_exit(EX_OSERR);
      }
      foo += i;

      /*
       * ok. reset the mbuf. 
       */
      if (foo == buf->offset) {
	buf->offset = 0;
	buf->tail = buf->data;
      } else {
	clean_smtp_mbuf(buf, foo);
      }
    }
  } else {
    syslog(LOG_CRIT, "You can't write %d bytes from a buffer with only %d in it!", len, (int) buf->offset);
  }
}

/*
 * Strip some data out of an smtp_mbuf
 */

void
clean_smtp_mbuf(struct smtp_mbuf *buf, int len)
{
  if (len > buf->offset) {
    abort();
  }
  if (len < buf->offset) {
    memmove(buf->data, (buf->data) + len, (buf->offset) - len);
    buf->offset = buf->offset - len;
    buf->tail = (buf->data) + (buf->offset);
  } else {
    buf->offset = 0;
    buf->tail = buf->data;
  }
}

/*
 * Allocate and initialize an smtp_mbuf
 */

struct smtp_mbuf *
alloc_smtp_mbuf(size_t size)
{
  struct smtp_mbuf *newbuf;
  newbuf = (struct smtp_mbuf *) malloc(sizeof(struct smtp_mbuf));

  if (newbuf == NULL) {
    return (NULL);
  }
  newbuf->data = (unsigned char *) malloc(sizeof(unsigned char) * size);

  if (newbuf->data == NULL) {
    free(newbuf);
    return (NULL);
  }
  newbuf->size = size;
  newbuf->offset = 0;
  newbuf->tail = newbuf->data;
  return (newbuf);
}

/*
 * Grow data area by "bloat" preserving everything else 
 */

int
grow_smtp_mbuf(struct smtp_mbuf *tiny, size_t bloat)
{
  unsigned char *newdata;

  newdata = (unsigned char *) malloc(tiny->size + bloat);
  if (newdata == NULL)
    return (0);
  memcpy((void *) newdata, (void *) tiny->data, tiny->offset);
  free(tiny->data);
  tiny->data = newdata;
  tiny->size += bloat;
  tiny->tail = tiny->data + tiny->offset;
  return (1);
}

/*
 * write len bytes from data into buffer mbuf. growing if necessary.
 * return 1 if successful, 0 for failure. 
 */

int
write_smtp_mbuf(struct smtp_mbuf *mbuf,
		unsigned char *data,
		size_t len)
{
  if (len > (mbuf->size - mbuf->offset)) {
    /*
     * we need a bigger buffer 
     */
    if (!(grow_smtp_mbuf(mbuf, ((len / 1024) + 1) * 1024))) {
      /*
       * let's hope there is enough to syslog :-) 
       */
      syslog(LOG_CRIT, "malloc said no to a %d byte buffer!",
	    (int)(mbuf->size + len + 1024));
      return (0);
    }
  }

  /*
   * buffer is now big enough 
   */
  memcpy((void *) mbuf->tail, (void *) (data), len);
  mbuf->tail += len;
  mbuf->offset += len;
  return (1);
}

/*
 * read up to len bytes from fd into buffer mbuf. growing if
 * neccessary.
 * return amount read if successful.
 *  set errno and return -1 for failure. 
 */
int
read_smtp_mbuf(struct smtp_mbuf *mbuf,
	       int fd,
	       size_t len)
{
  int howmany;

  if (len > (mbuf->size - mbuf->offset)) {
    /*
     * we need a bigger buffer 
     */
    if (!(grow_smtp_mbuf(mbuf, ((len / 1024) + 1) * 1024))) {
      /*
       * let's hope there is enough to syslog :-) 
       */
      syslog(LOG_ERR, "malloc said no to a %d byte buffer!",
	    (int)(mbuf->size + len + 1024));
      errno = ENOMEM;
      return (-1);
    }
  }
  /*
   * buffer is now big enough 
   */
 
  fflush(NULL); 
  signal(SIGALRM, read_alarm_timeout);
  alarm(read_timeout);
  howmany = read(fd, mbuf->tail, len);
  alarm(0);
  signal(SIGALRM, SIG_DFL);
  if (howmany > 0) {
    mbuf->tail += howmany;
    mbuf->offset += howmany;
  }
  return (howmany);
}

/*
 * Write a possibly multi-segement reply into mbuf "outbuf"
 */
int
writereply(struct smtp_mbuf *outbuf,
	   int code,
	   int more,
	   ...)
{
  int ok;
  char message[5];
  va_list ap;
  char *msg;

  va_start(ap, more);
  sprintf(message, "%3d%s", code, (more) ? "-" : " ");
  ok = write_smtp_mbuf(outbuf, message, strlen(message));
  while (ok && (msg = va_arg(ap, char *)) != NULL) {
    ok = write_smtp_mbuf(outbuf, msg, strlen(msg));
  }
  if (ok) {
    ok = write_smtp_mbuf(outbuf, CRLF, 2);
  }
  va_end(ap);
  return (ok);
}

/*
 * open a new spoolfile with appropriate lock and permissions 
 */
int
smtp_open_spoolfile()
{
  int fd;

  if (spoolfile != NULL) {
    syslog(LOG_CRIT, "Attempt to open new spoolfile with %s already open - aborting",
	   spoolfile);
    abort();
  }
  putenv(strdup("TMPDIR=/"));	/*
				 * Linux's tempnam() requires this kludge
				 * or we have to make a /tmp in our
				 * chrootdir. In it's wisdom it decides
				 * that if /tmp doesn't exist we can't
				 * have a tmpname anywhere.. Grumble.. 
				 */

#if USE_MKSTEMP
  /* If someone does manage to misconfigure us so people have
   * access to the spool dir they probably have worse things to 
   * worry about than the race condition but.. Oh well, keeps gcc 
   * from complaining. 
   */
  spoolfile= (char *) malloc(strlen(spooldir)+13);
  if (spoolfile == NULL) {
    syslog(LOG_CRIT, "Couldn't make a unique filename for spooling!");
    smtp_exit(EX_CONFIG);
  }
  strcpy(spoolfile, spooldir);
  strncat(spoolfile, "/smtpdXXXXXX", 12);
  if ((fd = mkstemp(spoolfile)) < 0) {
    syslog(LOG_CRIT, "Couldn't create spool file %s!", spoolfile);
    free(spoolfile);
    spoolfile=NULL;
    smtp_exit(EX_CONFIG);
  }
#else /* USE_MKSTEMP */
  /* gcc will bitch about this. There's nothing wrong with it where
   * we are using it (not in /tmp). There shouldn't be a race condition
   * since nothing other than smtpd should be using this spool dir, or
   * have access to it if it is permitted correctly.
   */
  {
    char *cp;  
    cp=tempnam(spooldir, "smtpd");
    if (cp == NULL) {
      syslog(LOG_CRIT, "Couldn't make a unique filename for spooling!");
      smtp_exit(EX_CONFIG);
    }
    
    spoolfile=(char *) malloc((strlen(spooldir)+strlen(cp)+1) * sizeof(char));
    if (spoolfile == NULL) {
      syslog(LOG_CRIT, "Malloc failed");
      smtp_exit(EX_TEMPFAIL);
    }
    spoolfile[0]='\0';

    /*
     * some versions of tempnam() with a spooldir give you a "/" in
     * front of the filename, that you can append to the directory for
     * a full path. Others like to give you the full path back. This
     * difference really sucks. Use mkstemp (above) if you can. otherwise,
     * this kludge avoids the problem.
     */

    if (strncmp(cp, spooldir, strlen(spooldir)) != 0) {
      /* looks like we don't have the spool directory on the front */
      strcpy(spoolfile, spooldir);
    }
    strcat(spoolfile, cp);
    free(cp);
  }
  if ((fd = open(spoolfile, O_CREAT | O_WRONLY, 0600)) < 0) {
    syslog(LOG_CRIT, "Couldn't create spool file %s!", spoolfile);
    smtp_exit(EX_CONFIG);
  }
#endif /* USE_MKSTEMP */

#ifdef USE_LOCKF
  if (lockf(fd, F_LOCK, 0) != 0) {
    syslog(LOG_ERR, "Couldn't lock spool file %s using lockf!", spoolfile);
    smtp_exit(EX_TEMPFAIL);
  }
#endif
#ifdef USE_FLOCK
  if (flock(fd, LOCK_EX) != 0) {
    syslog(LOG_ERR, "Couldn't lock spool file %s using flock!", spoolfile);
    smtp_exit(EX_TEMPFAIL);
  }
#endif
  return (fd);
}

/*
 * close spoolfile, unlock, and open permissions 
 */
void
smtp_close_spoolfile(int fd)
{
  if (spoolfile == NULL) {
    syslog(LOG_CRIT, "Attempt to close NULL spoolfile!");
    smtp_exit(EX_CONFIG);
  }
#ifdef USE_LOCKF
  if (lockf(fd, F_TLOCK, 0) == 0)
    if (lockf(fd, F_ULOCK, 0) != 0) {
      syslog(LOG_CRIT, "Couldn't unlock spool file %s using lockf!", spoolfile);
      smtp_exit(EX_OSERR);
    }
#endif
#ifdef USE_FLOCK
  if (flock(fd, LOCK_EX | LOCK_NB) == 0)
    if (flock(fd, LOCK_UN) != 0) {
      syslog(LOG_CRIT, "Couldn't unlock spool file %s using flock!", spoolfile);
      smtp_exit(EX_OSERR);
    }
#endif
  close(fd);
  chmod(spoolfile, 0750);	/*
				 * Mark file as 'complete' 
				 */
#if 0
  syslog(LOG_DEBUG, "Marking file %s as complete", spoolfile);
#endif
  free(spoolfile);
  spoolfile = NULL;
}

/*
 * unlock spoolfile and remove it 
 */
void
smtp_nuke_spoolfile(int fd)
{
  if (spoolfile == NULL) {
    syslog(LOG_CRIT, "Attempt to remove NULL spoolfile!");
    smtp_exit(EX_SOFTWARE);
  }
  if (unlink(spoolfile) != 0) {
    syslog(LOG_CRIT, "Couldn't remove spool file %s! (%m)", spoolfile);
    free(spoolfile);
    spoolfile = NULL;
    smtp_exit(EX_CONFIG);
  }
#ifdef USE_LOCKF
  if (lockf(fd, F_TLOCK, 0) == 0)
    if (lockf(fd, F_ULOCK, 0) != 0) {
      syslog(LOG_CRIT, "Couldn't unlock spool file %s using lockf! (%m)", spoolfile);
      free(spoolfile);
      spoolfile = NULL;
      smtp_exit(EX_OSERR);
    }
#endif
#ifdef USE_FLOCK
  if (flock(fd, LOCK_EX | LOCK_NB) == 0)
    if (flock(fd, LOCK_UN) != 0) {
      syslog(LOG_CRIT, "Couldn't unlock spool file %s using flock! (%m)", spoolfile);
      free(spoolfile);
      spoolfile = NULL;
      smtp_exit(EX_OSERR);
    }
#endif
  close(fd);
  free(spoolfile);
  spoolfile = NULL;
}

/*
 * Try to say something meaningful to our client and then exit.
 */

void
smtp_exit(int val)
{
  if (val != 0) {
    /*
     * we're leaving the client hanging. attempt to tell them we're
     * going away 
     */
    if (exiting++<1) {
       writereply(reply_buf, 421, 0, m421msg, NULL);
    }
     
    /*
     * if we have an open spool file that's unclosed, blast it out of
     * existence 
     */
    if (exiting++<2) {
       if (spoolfile != NULL) {
	      smtp_nuke_spoolfile(outfd);
       }
    }
  } else {
    if (exiting++<1) {
       if (spoolfile != NULL) {
	  smtp_close_spoolfile(outfd);
	  exiting++;
       }
    }
  }
  if (exiting++<3) {
	  flush_smtp_mbuf(reply_buf, replyfd, reply_buf->offset);
  }

  if (!VerboseSyslog) {
    accumlog(LOG_INFO, 0);		/* flush? */
  }
     
  exit(val);
}

/*
 * clean up things (mostly hostnames) that will go into the syslogs 
 */

unsigned char *
cleanitup(const unsigned char *s)
{
  static unsigned char *bufs[20];
  static int first = 1, next_buffer;
  unsigned char *dst, *buffer_addr;
  const unsigned char *src;
  int saw_weird_char, saw_bad_char, saw_high_bit = 0;

  if (first) {
    int i;

    for (i = 0; i < 20; i += 1) {
      bufs[i] = malloc(1024);
      if (bufs[i] == NULL) {
	syslog(LOG_CRIT, "CRITICAL - malloc (1024 bytes) failed in cleanitup");
	abort();
      }
    }
    first = 0;
  }
  src = s;
  dst = bufs[(next_buffer++) % 20];
  buffer_addr = dst;
  saw_weird_char = 0;
  saw_bad_char = 0;
  saw_high_bit = 0;

  while (*src != '\0') {
    unsigned char xch, ch;

    xch = *src++;
    ch = xch & 0x7f;

    if (ch != xch) {
      saw_high_bit = 1;
      *dst++ = '^';
      *dst++ = '=';
    }
    if (isalnum(ch) || strchr(" -/:=.@_[]", ch) != NULL) {

      *dst++ = ch;

    } else if (strchr("\\`$|;()*", ch) != NULL) {

      saw_bad_char = 1;

      *dst++ = '^';
      *dst++ = 'x';
      *dst++ = "0123456789abcdef"[(xch >> 4) & 0xf];
      *dst++ = "0123456789abcdef"[(xch) & 0xf];

    } else {

      saw_weird_char = 1;

      switch (ch) {
      case '\n':
	*dst++ = '^';
	*dst++ = 'n';
	break;
      case '\r':
	*dst++ = '^';
	*dst++ = 'r';
	break;
      case '\t':
	*dst++ = '^';
	*dst++ = 't';
	break;
      case '\b':
	*dst++ = '^';
	*dst++ = 'b';
	break;
      case '^':
	*dst++ = '^';
	*dst++ = '^';
	break;
      default:
	*dst++ = '^';
	*dst++ = 'x';
	*dst++ = "0123456789abcdef"[(xch >> 4) & 0xf];
	*dst++ = "0123456789abcdef"[(xch) & 0xf];
      }

    }

    if (dst - buffer_addr > 1024 - 10) {
      syslog(LOG_INFO, "INFO(cleanitup) - buffer overflow - chopping");
      break;
    }
  }

  *dst = '\0';


  if (saw_bad_char) {
    syslog(LOG_ALERT, "ALERT(cleanitup) - saw VERY unusual character (cleaned string is \"%s\")", buffer_addr);
  }
  if (saw_weird_char) {
    syslog(LOG_INFO, "INFO(cleanitup) - saw unusual character (cleaned string is \"%s\")", buffer_addr);
  }
  if (saw_high_bit) {
    syslog(LOG_INFO, "INFO(cleanitup) - saw character with high bit set (cleaned string is \"%s\")", buffer_addr);
  }
  return (buffer_addr);
}

/*
 * less paranoid version of cleanitup that tries to handle mail addresses
 * without mangling them.
 */
unsigned char *
smtp_cleanitup(const unsigned char *s)
{
  static unsigned char *bufs[20];
  static int first = 1, next_buffer;
  unsigned char *dst, *buffer_addr;
  const unsigned char *src;
  int firstone, arg_attempt, saw_weird_char, saw_bad_char, saw_high_bit = 0;

  if (first) {
    int i;

    for (i = 0; i < 20; i += 1) {
      bufs[i] = malloc(1024);
      if (bufs[i] == NULL) {
	syslog(LOG_CRIT, "CRITICAL - malloc (1024 bytes) failed in smtp_cleanitup");
	abort();
      }
    }
    first = 0;
  }
  src = s;
  dst = bufs[(next_buffer++) % 20];
  buffer_addr = dst;
  saw_weird_char = 0;
  saw_bad_char = 0;
  saw_high_bit = 0;
  arg_attempt = 0;

  firstone = 1;
  while (*src != '\0') {
    unsigned char xch, ch;

    xch = *src++;
    ch = xch & 0x7f;

    if (ch != xch) {
      saw_high_bit = 1;
      *dst++ = '^';
      *dst++ = '=';
    }
    /*
     * <sjg> RFC822 allows both ' and " in local-part.
     * " is infact _required_ if local-part contains spaces as is
     * common in x400 (yuk).
     */
    if (isalnum(ch) || strchr(" -,:=.@_!<>()[]/+%'\"", ch) != NULL) {
      if (firstone && (ch == '-')) {
	arg_attempt = 1;
	*dst++ = '^';
	*dst++ = '-';
      } else {
	*dst++ = ch;
      }

    } else if (strchr("\\`$|*;", ch) != NULL) {

      saw_bad_char = 1;

      *dst++ = '^';
      *dst++ = 'x';
      *dst++ = "0123456789abcdef"[(xch >> 4) & 0xf];
      *dst++ = "0123456789abcdef"[(xch) & 0xf];

    } else {

      saw_weird_char = 1;

      switch (ch) {
      case '\n':
	*dst++ = '^';
	*dst++ = 'n';
	break;
      case '\r':
	*dst++ = '^';
	*dst++ = 'r';
	break;
      case '\t':
	*dst++ = '^';
	*dst++ = 't';
	break;
      case '\b':
	*dst++ = '^';
	*dst++ = 'b';
	break;
      case '^':
	*dst++ = '^';
	*dst++ = '^';
	break;
      default:
	*dst++ = '^';
	*dst++ = 'x';
	*dst++ = "0123456789abcdef"[(xch >> 4) & 0xf];
	*dst++ = "0123456789abcdef"[(xch) & 0xf];
      }

    }

    if (dst - buffer_addr > 1024 - 10) {
      syslog(LOG_INFO, "INFO(smtp_cleanitup) - buffer overflow - chopping");
      break;
    }
    firstone = 0;
  }


  *dst = '\0';

  if (arg_attempt) {
    syslog(LOG_ALERT, "ALERT(smtp_cleanitup) - '-' as first character in address (cleaned string is \"%s\")", buffer_addr);
  }
  if (saw_bad_char) {
    syslog(LOG_ALERT, "ALERT(smtp_cleanitup) - saw VERY unusual character (cleaned string is \"%s\")", buffer_addr);
  }
  if (saw_weird_char) {
    syslog(LOG_DEBUG, "INFO(smtp_cleanitup) - saw unusual character (cleaned string is \"%s\")", buffer_addr);
  }
  if (saw_high_bit) {
    syslog(LOG_DEBUG, "INFO(smtp_cleanitup) - saw character with high bit set cleaned string is \"%s\")", buffer_addr);
  }
  return (buffer_addr);
}


/*
 * is smtp command "cmd" legal in state "state" 
 */
int
cmd_ok(int cmd, smtp_state state)
{
  if (sane_state(state)) {
    switch (cmd) {
    case HELO:
      return (!(test_state(OK_HELO, state)));
#if EHLO_KLUDGE
    case EHLO:
      return(test_state(OK_EHLO, state));
#endif
    case MAIL:
      if (test_state(OK_HELO, state) && (!test_state(OK_MAIL, state)))
	return (1);
      else
	return (0);
    case RCPT:
      if (test_state(OK_MAIL, state) && (!test_state(SNARF_DATA, state)))
	return (1);
      else
	return (0);
    case DATA:
      if (test_state(OK_RCPT, state))
	return (1);
      else
	return (0);
    default:
      return (1);
    }
  }
  return (0);
}

/*
 * is this state legal? returns 1 if so, 0 if not 
 */
int
sane_state(smtp_state state)
{
  if (test_state(OK_MAIL, state) && !test_state(OK_HELO, state)) {
    syslog(LOG_DEBUG, "Bad state. can't be OK_MAIL and not OK_HELO");
    return (0);
  }
  if (test_state(OK_RCPT, state) && !test_state(OK_MAIL, state)) {
    syslog(LOG_DEBUG, "Bad state. can't be OK_RCPT and not OK_MAIL");
    return (0);
  }
  if (test_state(SNARF_DATA, state) && !test_state(OK_RCPT, state)) {
    syslog(LOG_DEBUG, "Bad state. can't be SNARF_DATA and not OK_RCPT");
    return (0);
  }
  return (1);
}

/*
 * state change engine. given "state", change state after processing * 
 * command "cmd" with status "status", 
 */
void
state_change(smtp_state state, int cmd, int status)
{
  /*
   * basic state sanity checks 
   */

  if (!sane_state(state)) {
    reset_state(state);
    return;
  }
  switch (cmd) {
  case HELO:
    switch (status) {
    case SUCCESS:
      set_state(OK_HELO, state);	/*
					 * we got a helo 
					 */
      return;
    case ERROR:
      clear_state(OK_HELO, state);
      return;
    case FAILURE:
      reset_state(state);
      return;

    default:
      syslog(LOG_CRIT, "Hey, I shouldn't be here (Bad HELO status in change_state)!\n");
      abort();
    }
#if EHLO_KLUDGE
  case EHLO:
    switch (status) {
    case SUCCESS:
      set_state(OK_EHLO, state);	/*
					 * we got a ehlo 
					 */
      return;
    case ERROR:
      clear_state(OK_EHLO, state);
      return;
    case FAILURE:
      reset_state(state);
      return;

    default:
      syslog(LOG_CRIT, "Hey, I shouldn't be here (Bad EHLO status in change_state)!\n");
      abort();
    }
#endif
  case MAIL:
    switch (status) {
    case SUCCESS:
      set_state(OK_MAIL, state);
      return;
    case ERROR:
      /*
       * no state change 
       */
      return;
    case FAILURE:
      reset_state(state);
      return;
    default:
      syslog(LOG_CRIT, "Hey, I shouldn't be here (Bad MAIL status in change_state)!\n");
      abort();
    }
  case RCPT:
    switch (status) {
    case SUCCESS:
      set_state(OK_RCPT, state);
      return;
    case ERROR:
      /*
       * no state change 
       */
      return;
    case FAILURE:
      reset_state(state);
      return;
    default:
      syslog(LOG_CRIT, "Hey, I shouldn't be here (Bad RCPT status in change_state)!\n");
      abort();
    }
  case NOOP:
    switch (status) {
    case SUCCESS:
      return;
    default:
      syslog(LOG_CRIT, "Hey, I shouldn't be here (Bad NOOP status in change_state)!\n");
      abort();
    }
  case DATA:
    switch (status) {
    case SUCCESS:
      set_state(SNARF_DATA, state);
      return;
    case ERROR:
      /*
       * hmm. hard to do this 
       */
      return;
    case FAILURE:
      reset_state(state);
      return;
    default:
      syslog(LOG_CRIT, "Hey, I shouldn't be here (Bad DATA status in change_state)!\n");
      abort();
    }
  case UNKNOWN:
    switch (status) {
    case SUCCESS:
      return;
    case ERROR:
      return;
    case FAILURE:
      reset_state(state);
      return;
    default:
      syslog(LOG_CRIT, "Hey, I shouldn't be here (Bad UNKNOWN status in change_state)!\n");
      abort();
    }
  case QUIT:
    /*
     * one can always quit 
     */
    smtp_exit(EX_OK);
    break;
  case RSET:
    /*
     * one can always reset 
     */
    reset_state(state);
    break;
  default:
    /*
     * shouldn't get here on valid input. 
     */
    syslog(LOG_CRIT, "Hey, I shouldn't be here (end of change_state)!\n");
    abort();
  }
}

/*
 * parse a single smtp command in inbuf.
 *
 * PRE: "inbuf" contains one read line, \0 terminated without CRLF
 * at the end, and a non-whitespace character at the start. initial
 * state pointer passed in as "state". "outbuf" is our buffer for output
 * we're keeping, "replybuf" is our buffer for replies to the client.
 *
 * POST: any output from the command is output to the end of outbuf,
 * any replys to the client are output to the end of replybuf.
 * state is changed accordingly.
 *
 */

void
smtp_parse_cmd(unsigned char *inbuf,
	       struct smtp_mbuf *outbuf,
	       struct smtp_mbuf *replybuf,
	       smtp_state state)
{
  unsigned char *buf, *cp;
  size_t ilen;
  unsigned char verb[5];

  ilen = strlen(inbuf);
  if (ilen < 4) {
    if (ilen == 3) {
      memcpy(verb, inbuf, 3);
      verb[3] = '\0';
      if (strcasecmp(verb, "WIZ") == 0) {
	syslog(LOG_ALERT,
	       "Wizard command attempted from address %s(%s), name %s",
	       peerinfo.peer_ok_addr, peerinfo.peer_clean_reverse_name, client_claimed_name);
	writereply(replybuf, 250, 0, m250msg, NULL);
	state_change(state, NOOP, SUCCESS);
	return;
      }
    } else {
      /*
       * we need at least one complete verb. 
       */
      writereply(replybuf, 500, 0, m500msg, NULL);
      state_change(state, UNKNOWN, ERROR);
      return;
    }
  }
  memcpy(verb, inbuf, 4);
  verb[4] = '\0';
  buf = inbuf + 4;

  /* The basic vanilla SMTP commands, minimum as specified in RFC 821;
   * HELO, MAIL, RCPT, DATA, RSET, NOOP, QUIT. Added minimal VRFY
   * after rumors (never substantiated) that some mail agents might
   * try it thanks to RFC 1123.  We don't bother checking address
   * <domain> parameter syntax rigidly like RFC 1123 says we should,
   * leaving that up to the MTA invoked at the end. I don't believe
   * the added code complexity is worth any practical benefit here
   * when we are invoking the MTA after. Feel free to convince me
   * otherwise. 
   */

  /*
   * HELO 
   */

#if EHLO_KLUDGE
  if ((strcasecmp(verb, "HELO") == 0) ||
      (cmd_ok(EHLO, state) && (strcasecmp(verb, "EHLO") == 0))) {
#else
  if (strcasecmp(verb, "HELO") == 0) {
#endif

    /*
     * Hello hello.. a-la RFC 821 
     */
    if (!cmd_ok(HELO, state)) {
      writereply(replybuf, 503, 0, m503msg, NULL);
      state_change(state, HELO, FAILURE);
      return;
    }
    /*
     * at this point I shouldn't have anything bigger than a hostname
     * left. 
     */
    SPANBLANK(buf);
    if (strlen(buf) > SMTP_MAXFQNAME) {
      /*
       * someone gave us a *big* name for themselves. draw them to
       * our attention, and fail. 
       */
      syslog(LOG_ALERT,
	     "More than %d bytes on HELO from %s(%s).",
	     SMTP_MAXFQNAME, peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr);
      state_change(state, HELO, FAILURE);
      return;
    }
    client_claimed_name = strdup(cleanitup(buf));
    if (client_claimed_name == NULL) {
      syslog(LOG_ERR, "Malloc failed, abandoning session.");
      smtp_exit(EX_OSERR);
    }
    if (strcmp(buf, client_claimed_name) != 0) {
      syslog(LOG_ALERT, "Suspicious characters in HELO: hostname from host %s(%s), cleaned to %s",
	     peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr, client_claimed_name);
      if (Paranoid_Smtp) {
	syslog(LOG_CRIT, "Abandoning session from %s(%s) due to suspicious HELO: hostname",
	       peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr);
	smtp_exit(EX_PROTOCOL);
      }
    }
    writereply(replybuf, 250, 0,
	       peerinfo.my_clean_reverse_name,
	       " ",
	       m250helook,
	       " ",
	       client_claimed_name,
	       NULL);
    /*
     * log the connection 
     */
    if (VerboseSyslog) {
      syslog(LOG_INFO, "SMTP HELO from %s(%s) as \"%s\"",
	     peerinfo.peer_clean_reverse_name,
	     peerinfo.peer_ok_addr, client_claimed_name);
    }
    else {
      accumlog(LOG_INFO, 0);		/* flush anything left */
      accumlog(LOG_INFO, "relay=%s/%s",
	       peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr);
      if (strcasecmp(peerinfo.peer_clean_reverse_name, client_claimed_name))
	accumlog(LOG_INFO, " as \"%s\"", client_claimed_name);
    }     
    state_change(state, HELO, SUCCESS);
  } else if (strcasecmp(verb, "MAIL") == 0) {
    if (!cmd_ok(MAIL, state)) {
      writereply(replybuf, 554, 0, m554msg, NULL);
      state_change(state, MAIL, ERROR);
      return;
    }
    /*
     * at this point I shouldn't have anything bigger than a return *
     * address and a FROM: left. 
     */
    if (strlen(buf) > SMTP_MAX_MAILPATH + 7) {
      /*
       * someone gave us a *big* name * for themselves. draw them to 
       * our attention, and fail. 
       */
      syslog(LOG_ALERT,
	     "More than %d bytes on MAIL from address %s(%s).",
	     SMTP_MAX_MAILPATH, peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr);
      state_change(state, MAIL, FAILURE);
      return;
    }
    SPANBLANK(buf);
    if (strncasecmp(buf, "FROM:", 5) != 0) {
      writereply(replybuf, 501, 0, m501msg, NULL);
      state_change(state, MAIL, ERROR);
      return;
    }
    buf += 5;
    SPANBLANK(buf);
    /*
     * <sjg> if local-part contains ", then spaces are allowed
     */
    cp = NULL;
    if (buf[0] == '"' || buf[1] == '"')
      cp = strrchr(buf, '"');             /* REVISIT: find last " */
    if (cp == NULL)
      cp = buf+1;
    cp = strchr(cp+1, ' ');
    if (cp != NULL) {
      /* stuff on the end */
      *cp = '\0';
      cp++;
      SPANBLANK(cp);
      if (*cp != '\0') {
	/* We could deal with ESMTP SIZE here. If so it's either
	 * OK or bogus, in which we have to return 555. 
	 *  
	 * Without ESMTP. this is crud on the end, and we give 501
	 */
	writereply(replybuf, 501, 0, m501msg, NULL);
	state_change(state, MAIL, ERROR);
	return;
      }
    }
    current_from_mailpath = strdup(smtp_cleanitup(buf));
    if (current_from_mailpath == NULL) {
      /*
       * doh! malloc has failed us. 
       */
      syslog(LOG_ERR, "Malloc failed, abandoning session.");
      smtp_exit(EX_OSERR);
    }
       
    if (strcmp(buf, current_from_mailpath) != 0) {
      syslog(LOG_ALERT, "Suspicious characters in FROM: address from host %s(%s), cleaned to %s",
	     peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr, current_from_mailpath);
      if (Paranoid_Smtp) {
	syslog(LOG_CRIT, "Abandoning session from %s(%s) due to suspicious FROM: address",
	       peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr);
	smtp_exit(EX_PROTOCOL);
      }
    }
    writereply(replybuf, 250, 0,
	       "sender is ",
	       current_from_mailpath,
	       ", ",
	       m250fromok,
	       NULL);

    /*
     * log the connection 
     */
    if (VerboseSyslog) {
      syslog(LOG_INFO, "mail from %s", current_from_mailpath);
    } else {
      accumlog(LOG_INFO, " from=%s", current_from_mailpath);
    }

    /*
     * put our output in the outbuf 
     */
    if (write_smtp_mbuf(outbuf, "FROM ", strlen("FROM ")) &&
	write_smtp_mbuf(outbuf, current_from_mailpath,
			strlen(current_from_mailpath)) &&
	write_smtp_mbuf(outbuf, "\n", 1)) {
      state_change(state, MAIL, SUCCESS);
    } else {
      state_change(state, MAIL, FAILURE);
    }
  } else if (strcasecmp(verb, "RCPT") == 0) {
    char *victim;
    int badrcpt=0;

    if (!cmd_ok(RCPT, state)) {
      writereply(replybuf, 554, 0, m554nofrom, NULL);
      state_change(state, RCPT, ERROR);
      return;
    }
    /*
     * at this point I shouldn't have anything bigger than a return *
     * address and a RCPT: left. 
     */
    if (strlen(buf) > SMTP_MAX_MAILPATH + 1) {
      /*
       * someone gave us a *big* name for themselves. * draw them to 
       * our attention, and fail. 
       */
      syslog(LOG_ALERT,
	     "More than %d bytes on RCPT from address %s(%s).",
	     SMTP_MAX_MAILPATH, peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr);
      state_change(state, RCPT, FAILURE);
      return;
    }
    SPANBLANK(buf);
    if ((strlen(buf) < 3) || strncasecmp(buf, "TO:", 3) != 0) {
      writereply(replybuf, 501, 0, m501msg, NULL);
      state_change(state, RCPT, ERROR);
      return;
    }
    buf += 3;
    SPANBLANK(buf);
    /*
     * <sjg> if local-part contains ", then spaces are allowed
     */
    cp = NULL;
    if (buf[0] == '"' || buf[1] == '"')
      cp = strrchr(buf, '"');             /* REVISIT: find last " */
    if (cp == NULL)
      cp = buf;
    cp = strchr(cp+1, ' ');
    if (cp != NULL) {
      /* stuff on the end */
      *cp = '\0';
      cp++;
      SPANBLANK(cp);
      if (*cp != '\0') {
	/*  
	 * Without ESMTP. this is crud on the end, and we give 501
	 */
	writereply(replybuf, 501, 0, m501msg, NULL);
	state_change(state, RCPT, ERROR);
	return;
      }
    }
    victim = strdup(smtp_cleanitup(buf));
    if (victim == NULL) {
      syslog(LOG_ERR, "Malloc failed, abandoning connection.");
      smtp_exit(EX_OSERR);
    }
    if (strcmp(buf, victim) != 0) {
      syslog(LOG_ALERT, "Suspicious characters in RCPT: address from host %s(%s), cleaned to %s",
	     peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr, victim);
      if (Paranoid_Smtp) {
	syslog(LOG_CRIT, "Abandoning session from %s(%s) due to suspicious RCPT: address",
	       peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr);
	smtp_exit(EX_PROTOCOL);
      }
    }
#if CHECK_ADDRESS
    /* 
     * check and see if we are allowed to send mail from our FROM to
     * our RCPT on a connection from the client we're talking to.
     */

    {
      char *deathmsg = NULL;
      if (NoHostChecks) {
	/* It's just possible that someone will be trying to do this
	 * without having the DNS lookup checks on. Sigh.. 
	 * As a minimum for the lookups we need to know who we are
	 * and who is at the other end. meaning we must have
	 * peerinfo.my_sa, peerinfo.peer_sa and peerinfo.peer_ok_addr
	 * filled in. otherwise we have nothing to check against.
	 */
	int slen;
	slen = sizeof(my_sa);
	if (getsockname(0, (struct sockaddr *) &my_sa, &slen)
	    != 0) {
	  syslog(LOG_ERR, "ERROR - getsockname failed (%m) Who am i?");
	  exit(EX_OSERR);
	}
	peerinfo.my_sa = &my_sa;
	slen = sizeof(peer_sa);
	if (getpeername(0, (struct sockaddr *) &peer_sa, &slen)
	    != 0) {
	  syslog(LOG_ERR, "ERROR - getpeername failed (%m)");
	  exit(EX_OSERR);
	}
	peerinfo.peer_ok_addr = strdup(inet_ntoa(peer_sa.sin_addr));
	peerinfo.peer_sa = &peer_sa;
	if (peerinfo.peer_ok_addr == NULL) {
	  syslog(LOG_ERR, "Malloc failed during initialization - bye!");
	  exit(EX_CONFIG);
	}
      }

      /* We may have a username passed down in the environment from
       * our caller if they did an ident. Juniperd in particular may
       * pass a JUNIPER_IDENT, which will have been cleaned by
       * the cleanitup() routine, and will be "UNKNOWN" if no value
       * was obtained.
       */


      peerinfo.peer_clean_ident	= getenv("JUNIPER_IDENT");
      
      if (peerinfo.peer_clean_ident != NULL) {
	if (strcmp(peerinfo.peer_clean_ident, "UNKNOWN") == 0) {
	  peerinfo.peer_clean_ident = NULL;
	}
	peerinfo.peer_dirty_ident = peerinfo.peer_clean_ident;
      }

      /* otherwise, allow our invoker to pass us in an ident value in the
       * environment as SMTPD_IDENT. (for people who do this with the tcp 
       * wrapper.)We must however, clean the string.
       */ 
      
      if (peerinfo.peer_clean_ident == NULL) {
	peerinfo.peer_dirty_ident = getenv("SMTPD_IDENT");
	if (peerinfo.peer_dirty_ident != NULL) {
	  if (strcmp(peerinfo.peer_dirty_ident, "UNKNOWN") == 0) {
	    peerinfo.peer_dirty_ident = NULL;
	  }
	  else {
	    peerinfo.peer_clean_ident = 
	      strdup(cleanitup(peerinfo.peer_dirty_ident));
	    if (peerinfo.peer_clean_ident == NULL) {
	      syslog(LOG_ERR, "ERROR - Malloc failed");
	      exit(EX_OSERR);
	    }
	  }
	}
      }
      switch (smtpd_addr_check( CHECK_FILE, 
				&peerinfo,
				current_from_mailpath,
				victim,
				&deathmsg) ) {
      case 1:
	/* we matched an "allow" rule - continue */
	break; 
      case 0:
	/* we matched a "deny" rule.  syslog and send back failure message */
	if (VerboseSyslog) {
	  syslog(LOG_INFO, "Forbidden FROM or RCPT for host %s(%s) - Abandoning session",
		 peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr);
	}
	else {
	  accumlog(LOG_INFO, " forbidden FROM or RCPT");
	  accumlog(LOG_INFO, 0);	/* flush it */
	}
	{
	  char *c;
	  c = make_check_fail_reply(peerinfo.peer_clean_ident,
					 peerinfo.peer_clean_reverse_name,
					 peerinfo.peer_ok_addr,
					 current_from_mailpath,
					 victim,
					 deathmsg);
	  if (write_smtp_mbuf(replybuf, c, strlen(c))) {
	    write_smtp_mbuf(replybuf, CRLF, 2);
	  }	
	}
	smtp_exit(EX_PROTOCOL);
	break;; /* notreached */

      case -1:
	/* we matched a "noto" rule. send message, and set state */ 
	{
	  char *c;
	  c = make_check_fail_reply(peerinfo.peer_clean_ident,
					 peerinfo.peer_clean_reverse_name,
					 peerinfo.peer_ok_addr,
					 current_from_mailpath,
					 victim,
					 deathmsg);
	  if (write_smtp_mbuf(replybuf, c, strlen(c))) {
	    write_smtp_mbuf(replybuf, CRLF, 2);
	  }
	}
	badrcpt = 1;

 	if (VerboseSyslog) {
	  syslog(LOG_INFO, "Discarded bad recipient %s", victim);
 	} else {
	  accumlog(LOG_INFO, " discarded bad recipient=%s", victim);
 	}
	state_change(state, RCPT, ERROR);
	break;

      default: 
	syslog(LOG_CRIT, "Aieee! smtpd_check_address returned bogus value! *SHOULD NOT HAPPEN*");
	abort();
      }
    }
#endif /* CHECK_ADDRESS */
    if (!badrcpt) {
     writereply(replybuf, 250, 0,
		 "recipient ",
		 victim,
		 ", ",
		 m250rcptok,
		 NULL);
      
      /*
       * log the recipient.
       */
      if (VerboseSyslog) {
	syslog(LOG_INFO, "Recipient %s", victim);
      } else {
	accumlog(LOG_INFO, " to=%s",  victim);
      }	
      if (write_smtp_mbuf(outbuf, "RCPT ", strlen("RCPT ")) &&
	  write_smtp_mbuf(outbuf, victim, strlen(victim)) &&
	  write_smtp_mbuf(outbuf, "\n", 1)) {
	state_change(state, RCPT, SUCCESS);
      } else {
	state_change(state, RCPT, FAILURE);
      }
    }
    free(victim);
  } else if (strcasecmp(verb, "NOOP") == 0) {
    writereply(replybuf, 250, 0, m250msg, NULL);
    state_change(state, NOOP, SUCCESS);
  } else if (strcasecmp(verb, "VRFY") == 0) {
    writereply(replybuf, 252, 0, m252msg, NULL);
    state_change(state, NOOP, SUCCESS);
  } else if (strcasecmp(verb, "DEBU") == 0) {
    syslog(LOG_ALERT,
	   "Debug command attempted from %s(%s), name %s",
	   peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr, client_claimed_name);
    writereply(replybuf, 250, 0, m250msg, NULL);
    state_change(state, NOOP, SUCCESS);
  } else if (strcasecmp(verb, "RSET") == 0) {
    writereply(replybuf, 250, 0, m250msg, NULL);
    state_change(state, RSET, SUCCESS);
  } else if (strcasecmp(verb, "QUIT") == 0) {
    writereply(replybuf, 221, 0, m221msg, NULL);
    state_change(state, QUIT, SUCCESS);
  } else if (strcasecmp(verb, "DATA") == 0) {
    if (cmd_ok(DATA, state)) {
      writereply(replybuf, 354, 0, m354msg, NULL);
      state_change(state, SNARF_DATA, SUCCESS);
    } else {
      writereply(replybuf, 554, 0, m554norcpt, NULL);
      state_change(state, SNARF_DATA, ERROR);
    }
#if EHLO_KLUDGE
  } else if (strcasecmp(verb, "EHLO") == 0) {
    writereply(replybuf, 500, 0, m500msg, NULL);
    state_change(state, EHLO, SUCCESS);
#endif
  } else {
    /*
     * if we get here our verb don't look like a verb. this means that
     * we should fire off a syntax error to the client
     */
    writereply(replybuf, 500, 0, m500msg, NULL);
    state_change(state, UNKNOWN, ERROR);
  }
  return;
}

/*
 * Read a message body.
 * return values: * 1 - everything OK * 2 - read failed or connection
 * died before we got it all * 3 - size exceeded * 4 - no space on
 * write device * 5 - not enough memory 
 */

int
snarfdata(int in, int out, long *size, int bin)
{
  struct smtp_mbuf *buf;
  struct smtp_mbuf *outbuf;
  int snarfed;
  int dot = 0;
  int i;
  long max, outbytes;
  int body = 0;

  /*
   * initial message size 
   */
  max = (*size ? *size : LONG_MAX);


  /*
   * Initialize the smtp_mbuf's.
   * We start of with absurdly small sizes in order to ensure that the
   * code which grows an mbuf gets exercised (i.e. if it is broken then
   * the program will probably die and the bug will (hopefully) get fixed).
   */

  buf = alloc_smtp_mbuf(1024);
  if (buf == NULL) {
    syslog(LOG_DEBUG, "Couldn't allocate input buffer for data command");
    return (5);
  }
  outbuf = alloc_smtp_mbuf(1024);
  if (outbuf == NULL) {
    syslog(LOG_DEBUG, "Couldn't allocate output buffer for data command");
    return (5);
  }
  outbytes = 0;
  while (1) {
    int linestart;
    int lineend;

    linestart = lineend = 0;

    snarfed = read_smtp_mbuf(buf, in, 1024);
    if (snarfed < 0) {
      if (VerboseSyslog) {	
	syslog(LOG_INFO, "read error receiving message body: %m");
      } else {
	accumlog(LOG_INFO, " read error receiving message body: %s",
		 strerror(errno));
      }
      return (2);
    }
    if (snarfed == 0) {
      if (VerboseSyslog) {      
	syslog(LOG_INFO, "EOF while receiving message body");
      } else {
	accumlog(LOG_INFO, " EOF while receiving message body");
      }
      return (2);
    }
    if (outbuf->size < buf->size) {
      if (grow_smtp_mbuf(outbuf, (buf->size - outbuf->size)) == 0) {
	syslog(LOG_INFO, "Couldn't grow #1");
	return (5);
      }
    }
    for (i = 0; i < buf->offset; i++) {
      switch (buf->data[i]) {
      case LF:
	/*
	 * we got an LF 
	 */
	if (dot == 1) {
	  /*
	   * Lonesome Dot sings: "We're done!" 
	   */
	  *size = outbytes;
	  return (1);
	  break;
	} else {
	  /*
	   * write out from linestart to lineend (inclusive) 
	   */
	  buf->data[lineend] = LF;	/*
					 * I must at least write
					 * out this LF 
					 */
	  /*
	   * check for unusual headers.
	   * these form the basis of a number of more interesting attacks
	   * 	- Julian Assange <proff@suburbia.net>
	   */
	  if (!body) {
	    	if (buf->data[linestart] == LF ||
	    	    (buf->data[linestart] == '\r' && buf->data[linestart+1] == LF))
                {
			body = 1;
                } else
		{
			char *p;
		        int off=0;
			int unprintable = 0;
			for (p=buf->data+linestart; *p != LF; p++)
			{
			        /* add isalpha(), to allow for non
                                 * conventional locales. where 
                                 * isalpha != isprint.
                                 */   
				if (!isalpha(*p) && !isprint(*p) &&
				    !isspace(*p))
				{
				        syslog(LOG_DEBUG, "Unprintable character value=%d in message header at offset %d", (int)*p, off);
					*p='?';
					unprintable++;
				}
		                off++;
			}
			if (unprintable)
			{
			    buf->data[lineend]='\0';
			    syslog(LOG_ALERT, "%d unprintable characters in \"%.255s\"", unprintable, buf->data+linestart);
			    buf->data[lineend]=LF;
			    if (Paranoid_Smtp) {
			       syslog(LOG_CRIT, "Abandoning session from %s(%s) due to unprintable message header",
				      peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr);
			       smtp_exit(EX_PROTOCOL);
			    }
	                }
			if (lineend - linestart > 255) {
			    syslog(LOG_ALERT, "unusually long header (trucated) [%d bytes] = \"%.255s\"...", lineend-linestart, buf->data+linestart);
			    buf->data[linestart+255]=LF;
			    lineend = linestart + 255;
			}
		}
	  }
	  while (linestart <= lineend) {
	    int j;
	    j = write(out, (buf->data) + linestart,
		      ((lineend - linestart) + 1));
	    if (j < 0) {
	      /*
	       * we can't write to the out fd. return
	       * indicating that.
	       */
	      syslog(LOG_ERR, "Write failed to spoolfile! (%m)");
	      return (4);
	    } else if (j == 0) {
	      syslog(LOG_CRIT, "zero length write to file - bye!");
	      exit(EX_CONFIG);
	    }
	    outbytes += j;
	    linestart += j;
	    if (outbytes >= max) {
	      /*
	       * we've blown over our maxsize limit. 
	       */
	      syslog(LOG_ERR,
		     "Message body exceeds maximum size of %ld", max);
	      return (3);
	    }
	  }
	}
	dot = 0;
	linestart = i + 1;
	lineend = i + 1;
	break;
      case CR:
	/*
	 * we got a CR. if it's at the end of a line, (right
	 * before an LF), we ignore it, and it goes away. Any
	 * other character will advance lineend past it, meaning
	 * it gets picked up as data. Dot is also unchanged
	 * since this could be the start of a crlf after we saw
	 * a first character dot. The next character will bring
	 * enlightenment. 
	 */
	break;
      case '.':
	if (i == (linestart)) {
	  if (dot == 0) {
	    /*
	     * this is a dot at start of line. It could mean
	     * we're finished. We're either finished, or we
	     * do not replicate this first dot in the output.
	     * (RFC 821, 4.5.2) 
	     */
	    dot = 1;
	    /*
	     * if we aren't finished, then this dot can't
	     * appear, so increment linestart by one 
	     */
	    linestart++;
	  } else {
	    /*
	     * this is a second dot, after we saw one last
	     * time and moved linestart. This one stays, and 
	     * this ain't Lonsesome Dot 
	     */
	    dot = 0;
	  }
	} else {
	  /*
	   * this is a plain ordinary dot with no pretensions,
	   * it's like any other character. Clear dot in order 
	   * to properly handle ".\r." case at the start of a
	   * line (i.e ".\r." is NOT the same as ".."  even if
	   * it appears at the start of a line). 
	   */
	  dot = 0;
	}
	lineend = i + 1;
	break;
      default:
	dot = 0;
	lineend = i + 1;
	break;
      }
    }
    /*
     * we had part of a line left in the buffer. Keep it and throw
     * away the rest. 
     */
    clean_smtp_mbuf(buf, linestart);
  }
}

/*
 * The brains of this operation
 */

int
main(int argc, char **argv)
{
  int opt;
  long smtp_port = 25;
  char *optstring = "l:p:q:d:u:s:g:m:i:cHPDL";
  int i, k;
  smtp_state_set last_state_s, current_state_s;	 /* The real state vector. */
  smtp_state last_state, current_state; /* Pointers to the state vector. */

  char *chrootdir = SPOOLDIR;
  char *username = SMTP_USER;
  char *groupname = SMTP_GROUP;
  struct passwd *user = NULL;
  struct group *group = NULL;
  struct sigaction new_sa;
  int daemon_mode = 0;
  int listen_fd = -1; /* make gcc be quiet */
  int pid_fd = -1;
  char *pid_fname = NULL;
  struct in_addr listen_addr;
  int child_no_openlog = 0; /* don't openlog() in children - use inherited 
			     * parent fd
			     */

  bzero(&peerinfo, sizeof(struct peer_info));
  peerinfo.peer_clean_forward_name = "UNKNOWN";
  peerinfo.peer_clean_reverse_name = "UNKNOWN";
  peerinfo.peer_ok_addr = "";

  umask (077);

  openlog("smtpd", LOG_PID | LOG_NDELAY, LOG_FACILITY);
  listen_addr.s_addr = INADDR_ANY;

#if SET_LOCALE
  /* try to set our localization to the one specified */
  (void) setlocale(LC_CTYPE, LOCALIZATION);
#endif

#if NO_COMMANDLINE_OPTIONS
  if (argc > 1) {
    syslog(LOG_ALERT, "Attempt to start smtpd with command line options");
    show_usage();
    exit(EX_USAGE);
  }
#else
#ifdef GETOPT_EOF
  while ((opt = getopt(argc, argv, optstring)) != EOF) {
#else
  while ((opt = getopt(argc, argv, optstring)) > 0) {
#endif
    switch (opt) {
    case 'p':
      {
	char *foo; 
	smtp_port = strtol(optarg, &foo, 10);
	if (*foo != '\0') {
	  /* this doesn't smell like a number. Bail */
	  syslog(LOG_ERR, "Invalid port argument for the \"-p\" option");
	  show_usage();
	  exit(EX_USAGE);
	}
      }
      break;
    case 'l':
      listen_addr.s_addr = inet_addr(optarg);
      if (listen_addr.s_addr == htonl(INADDR_NONE)) {
	  syslog(LOG_ERR, "Invalid ip address given for the \"-l\" option");
	  show_usage();
	  exit(EX_USAGE);
      }
      break;
    case 'i':
      if (optarg[0] != '/') {
	syslog(LOG_ERR, "The \"-i\" option requires an absolute pathname argument\n");
	show_usage();
	exit(EX_USAGE);
      }
      pid_fname = optarg;
      break;
    case 'q':
      VerboseSyslog = 0;
      break;
    case 'c':
      if (optarg[0] != '/') {
	syslog(LOG_ERR, "The \"-c\" option requires an absolute pathname argument\n");
	show_usage();
	exit(EX_USAGE);
      }
      chrootdir = optarg;
      break;
    case 'D':
      daemon_mode = 1;
      break;
    case 'L':
      child_no_openlog = 1;
      break;
    case 'm':
      peerinfo.my_clean_reverse_name = optarg;
      break;
    case 'H':
      NoHostChecks = 1;
      syslog(LOG_INFO, "smtpd Host/Address checking disabled by \"-H\" option");
      break;
    case 'P':
      Paranoid_Smtp = 1;
      Paranoid_Dns = 1;
      syslog(LOG_INFO, "smtpd running in Paranoid mode");
      break;
    case 'd':
      if (optarg[0] != '/') {
	syslog(LOG_ERR, "%s, The \"-d\" option requires an absolute pathname argument\n", optarg);
	show_usage();
	exit(EX_USAGE);
      }
      spooldir = optarg;
      break;
    case 'u':
      {
	long userid;
	char *foo;

	userid = strtol(optarg, &foo, 10);
	if (*foo == '\0') {
	  /*
	   * looks like we got something that looks like a
	   * number try to find user by uid
	   */
	  user = getpwuid((uid_t) userid);
	  if (user == NULL) {
	    syslog(LOG_ERR, "Invalid uid argument for the \"-u\" option, no user found for uid %s\n", optarg);
	    show_usage();
	    exit(EX_USAGE);
	  }
	  username = user->pw_name;
	} else {
	  /*
	   * optarg didn't look like a number, so try looking it 
	   * up as a * username. 
	   */
	  user = getpwnam(optarg);
	  if (user == NULL) {
	    syslog(LOG_ERR, "Invalid username argument for the \"-u\" option, no user found for name %s\n", optarg);
	    show_usage();
	    exit(EX_USAGE);
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
	   * number, try to find user by uid 
	   */
	  group = getgrgid((gid_t) grpid);
	  if (group == NULL) {
	    syslog(LOG_ERR, "Invalid gid argument for the \"-g\" option, no group found for gid %s\n", optarg);
	    show_usage();
	    exit(EX_USAGE);
	  }
	  groupname = group->gr_name;
	} else {
	  /*
	   * optarg didn't look like a number, so try looking it 
	   * up as a groupname. 
	   */
	  group = getgrnam(optarg);
	  if (group == NULL) {
	    syslog(LOG_ERR, "Invalid groupname argument for the \"-g\" option, no group found for name %s\n", optarg);
	    show_usage();
	    exit(EX_USAGE);
	  }
	  groupname = group->gr_name;
	}
      }
      break;
    case 's':
      {
	char *foo;

	maxsize = strtol(optarg, &foo, 10);
	if (*foo != '\0') {
	  syslog(LOG_ERR, "The \"-s\" option requires a size argument\n");
	  show_usage();
	  exit(EX_USAGE);
	}
	if (maxsize <= 0) {
	  syslog(LOG_ERR, "\"-s\" argument must be positive!\n");
	  show_usage();
	  exit(EX_USAGE);
	}
      }
      break;
    default:
      syslog(LOG_ERR, "Unknown option \"-%c\"\n", opt);
      show_usage();
      exit(EX_USAGE);
      break;
    }
  }
#endif /* NO_COMMANDLINE_OPTIONS */

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
       * looks like we got something that looks like a number, try
       * to find user by uid
       */
      user = getpwuid((uid_t) userid);
      if (user == NULL) {
	syslog(LOG_ERR, "Eeek! I was compiled to run as uid %s, but no user found for uid %s\n", username, username);
	syslog(LOG_ERR, "Please recompile me to use a valid user, or specify one with the \"-u\" option.\n");
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
	syslog(LOG_ERR, "Eeek! I was compiled to run as user \"%s\", but no user found for username \"%s\"\n", username, username);
	syslog(LOG_ERR, "Please recompile me to use a valid user, or specify one with the \"-u\" option.\n");
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
	syslog(LOG_ERR, "Eeek! I was compiled to run as gid %s, but no group found for gid %s\n", groupname, groupname);
	syslog(LOG_ERR, "Please recompile me to use a valid group, or specify one with the \"-g\" option.\n");
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
	syslog(LOG_ERR, "Eeek! I was compiled to run as group \"%s\", but no group found for groupname \"%s\"\n", groupname, groupname);
	syslog(LOG_ERR, "Please recompile me to use a valid group, or specify one with the \"-g\" option.\n");
	exit(EX_CONFIG);
      }
      groupname = group->gr_name;
    }
  }
  /*
   * If we're here, we have a valid user and group to run as 
   */
  if (group == NULL || user == NULL) {
    syslog(LOG_CRIT, "Didn't find a user or group, (Shouldn't happen)\n");
    abort();
  }
  if (user->pw_uid == 0) {
    syslog(LOG_CRIT, "Sorry, I don't want to run as root! It's a bad idea!");
    syslog(LOG_CRIT, "Please recompile me to use a valid user, or specify one with the \"-u\" option.\n");
    exit(EX_CONFIG);
  }
  if (group->gr_gid == 0) {
    syslog(LOG_CRIT, "Sorry, I don't want to run as group 0. It's a bad idea!");
    syslog(LOG_CRIT, "Please recompile me to use a valid group, or specify one with the \"-g\" option.\n");
    exit(EX_CONFIG);
  }
  if ( daemon_mode ) {
    struct sockaddr_in sa;

    listen_fd = socket(AF_INET,SOCK_STREAM,0);
    if ( listen_fd < 0 ) {
      syslog(LOG_ERR, "Can't get a listen socket for daemon mode (%m)");
      exit(EX_OSERR);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(smtp_port);
    sa.sin_addr.s_addr = listen_addr.s_addr;

    /* Need to do this while we're still root */

    if ( bind(listen_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0 ) {
      syslog(LOG_ERR, "Can't bind listen socket to port %ld in daemon mode (%m)"
	     , smtp_port);
      exit(EX_OSERR);
    }

  }
  /*  we may have requested that children do not re-open syslogs in case
   * we haven't set up the chroot for dealing with them. in this case, 
   * reopen the logs as the master to not show process id, (as to not
   * be misleading when child processes use the open fd for syslog).
   */
  if (child_no_openlog) {
    syslog(LOG_INFO, "Child process openlog() suppressed with -L option");
    syslog(LOG_INFO, "Re-opening syslog without PID information");
    closelog();
    openlog("smtpd", LOG_NDELAY, LOG_FACILITY);
    syslog(LOG_INFO, "Log reopened.");
  }

  if (daemon_mode) {
    /* open pid file fd while we're still root. */

    if ( (pid_fname == NULL) && 
	 (((pid_fname = malloc(sizeof(SMTPD_PID_DIR)
			       + sizeof(SMTPD_PID_FILENAME) + 2)))
	  != NULL ) ) {
      (void) sprintf(pid_fname, "%s/%s", SMTPD_PID_DIR, SMTPD_PID_FILENAME);
    }
    if (pid_fname != NULL) {
      if ((pid_fd = open(pid_fname, O_CREAT | O_WRONLY, 0644)) < 0) {
	syslog(LOG_ERR, "Couldn't create pid file %s: %m", pid_fname);
	exit(EX_CONFIG);
      }
    }
  }
      
  if (chrootdir != NULL) {
    if (chdir(chrootdir) != 0) {
      syslog(LOG_CRIT, "Couldn't chdir to directory %s! (%m)",
	     chrootdir);
      exit(EX_CONFIG);
    }
    if (chroot(chrootdir) != 0) {
      syslog(LOG_CRIT, "Couldn't chroot to directory %s! (%m)",
	     chrootdir);
      exit(EX_CONFIG);
    }
  } else {
    syslog(LOG_CRIT, "No chroot directory specified! Aborting.");
    abort();
  }

  if (spooldir == NULL) {
    syslog(LOG_CRIT, "NULL spool directory! Aborting.");
    abort();
  }

  if (setgid(group->gr_gid) != 0) {
    syslog(LOG_ERR, "I can't change groups! Setgid failed! (%m)");
    syslog(LOG_ERR, "Exiting due to setgid failure");
    exit(EX_OSERR);
  }
  if (setuid(user->pw_uid) != 0) {
    syslog(LOG_ERR, "I can't change groups! Setgid failed! (%m)");
    syslog(LOG_ERR, "Exiting due to setuid failure");
    exit(EX_OSERR);
  }
  /*
   * Ok, the world seems good. Should we run as a daemon?
   */

  if ( daemon_mode ) {
    int failures;
    int rval;

    rval = fork();
    if ( rval > 0 ) {
	/* Parent - just exit */
	exit(EX_OK);
    } else if ( rval < 0 ) {
      syslog(LOG_ERR, "Can't do first fork in daemon mode (%m)");
      exit(EX_OSERR);
    }
    setsid();

    /* write our pid into the (inherited) pid_fd */

   if (pid_fd >= 0) {
      char buf[80];
#ifdef USE_FLOCK
      if (lockf(pid_fd, F_TLOCK, 0) != 0) 
#else
      if (flock(pid_fd, LOCK_EX|LOCK_NB) != 0)
#endif
	{
	  syslog(LOG_ERR,
		 "Couldn't get lock on pid file %s! Am I already running?", pid_fname);
	  exit(1);
	}
      sprintf(buf, "%d\n", getpid());
      write(pid_fd, buf, strlen(buf));
      /* do not close - leave this fd open to keep lock */
    }


    if ( listen(listen_fd,10) < 0 ) {
      syslog(LOG_ERR, "Can't listen on socket in daemon mode (%m)");
      exit(EX_OSERR);
    }

    failures = 0;
    syslog(LOG_INFO,"smtpd running in daemon mode - ready to accept connections");


    while (1) {
      int fd;
      int slen;
      struct sockaddr_in peer;
      int status;

      while ( waitpid(0,&status,WNOHANG) > 0 )
	;

      slen = sizeof(peer);
      fd = accept(listen_fd, (struct sockaddr *)&peer, &slen);
      if ( fd < 0 ) {
	if ( failures++ < 10 ) {
	  syslog(LOG_INFO,"accept call failed in daemon mode (%m) - continuing");
	} else {
	  syslog(LOG_ERR,"too many consecutive accept call failures in daemon mode (%m)");
	  exit(EX_OSERR);
	}
      } else {
	int rval;

	failures = 0;

	rval = fork();
	if ( rval > 0 ) {

	  /*
	   * Parent - close the accepted fd and continue the loop
	   */

	  close(fd);

	} else if ( rval == 0 ) {

	  /*
	   * Child - make ourselves look like an inetd child
	   * and break out of the loop to allow the regular inetd-style
	   * processing to occur.
	   */
	  close(pid_fd); /* we don't need this anymore */

	  dup2(fd,0);
	  dup2(fd,1);
	  close(fd);
	  if (!child_no_openlog) {
	    closelog();
	    openlog("smtpd", LOG_PID | LOG_NDELAY, LOG_FACILITY);
	  }
	  break;

	} else {

	  close(fd);
	  syslog(LOG_INFO, "Can't fork child in daemon mode (%m)");
	  exit(EX_OSERR);

	}

      }

    }

  }

  /* We need to ignore SIGPIPE */
#ifdef BSD_SIGNAL
  signal(SIGPIPE, SIG_IGN);
#else
  memset(&new_sa, 0, sizeof(new_sa));
  new_sa.sa_handler = SIG_IGN;
  (void)sigemptyset(&new_sa.sa_mask);
  new_sa.sa_flags = SA_RESTART;
  if ( sigaction( SIGPIPE, &new_sa, NULL ) != 0 ) {
    syslog(LOG_CRIT,"CRITICAL - sigaction failed (%m)");
    exit(EX_OSERR);
  }
#endif

  /*
   * Who's on the other end of this line?
   */

  if (!NoHostChecks) {
    int slen;
    struct hostent *tmp_he;
    int ok;
    char **pp;

    /*
     * set who we are in case our caller didn't tell us to be someone
     * else 
     */

    slen = sizeof(my_sa);
    if (getsockname(0, (struct sockaddr *) &my_sa, &slen)
	!= 0) {
      syslog(LOG_ERR, "ERROR - getsockname failed (%m) Who am i?");
      exit(EX_OSERR);
    }
    peerinfo.my_sa = &my_sa;
    if (peerinfo.my_clean_reverse_name == NULL) {
      tmp_he = gethostbyaddr((char *) &(my_sa.sin_addr.s_addr),
			     sizeof(my_sa.sin_addr.s_addr),
			     AF_INET);
      if (tmp_he != NULL) {
	peerinfo.my_clean_reverse_name = strdup(cleanitup(tmp_he->h_name));
	if (peerinfo.my_clean_reverse_name == NULL) {
	  syslog(LOG_ERR, "Malloc failed during initialization - bye!");
	  exit(EX_CONFIG);
	}
	if (strcmp(tmp_he->h_name, peerinfo.my_clean_reverse_name) != 0) {
	  syslog(LOG_CRIT, "CRITICAL - Suspicious characters in MY hostname! (for ip=%s) cleaned to %s.\n", peerinfo.peer_ok_addr, peerinfo.my_clean_reverse_name);
	  syslog(LOG_CRIT, "CRITICAL - YOUR DNS IS EITHER COMPROMISED OR MISCONFIGURED! INVESTIGATE!");
	  smtp_exit(EX_CONFIG);
	}
      }
    }
    slen = sizeof(peer_sa);
    if (getpeername(0, (struct sockaddr *) &peer_sa, &slen)
	!= 0) {
      syslog(LOG_ERR, "ERROR - getpeername failed (%m)");
      exit(EX_OSERR);
    }
    peerinfo.peer_ok_addr = strdup(inet_ntoa(peer_sa.sin_addr));
    peerinfo.peer_sa = &peer_sa;
    if (peerinfo.peer_ok_addr == NULL) {
      syslog(LOG_ERR, "Malloc failed during initialization - bye!");
      exit(EX_CONFIG);
    }
    /*
     * get reverse name 
     */

    tmp_he = gethostbyaddr((char *) &(peer_sa.sin_addr.s_addr),
			   sizeof(peer_sa.sin_addr.s_addr),
			   AF_INET);
    if (tmp_he != NULL) {
      peerinfo.peer_dirty_reverse_name = strdup((tmp_he->h_name));
      if (peerinfo.peer_dirty_reverse_name == NULL) {
	syslog(LOG_ERR, "Malloc failed during initialization - bye!");
	exit(EX_CONFIG);
      }
    } else {
      syslog(LOG_INFO, "No reverse mapping for address %s (%d)",
	     peerinfo.peer_ok_addr, h_errno);
      peerinfo.peer_dirty_reverse_name = "UNKNOWN";
    }

    peerinfo.peer_clean_reverse_name = strdup(cleanitup(peerinfo.peer_dirty_reverse_name));
    if (peerinfo.peer_clean_reverse_name == NULL) {
      syslog(LOG_ERR, "Malloc failed during initialization - bye!");
      exit(EX_CONFIG);
    }
    if (strcmp(peerinfo.peer_clean_reverse_name, peerinfo.peer_dirty_reverse_name) != 0) {
      syslog(LOG_ALERT, "Suspicious characters in hostname for address %s, cleaned to %s",
	     peerinfo.peer_ok_addr, peerinfo.peer_clean_reverse_name);
      if (Paranoid_Dns) {
	syslog(LOG_CRIT, "Abandoning session from %s due to suspicious hostname",
	       peerinfo.peer_ok_addr);
	smtp_exit(EX_PROTOCOL);
      }
    }
    /*
     * get forward name 
     */

    ok = 0;
    tmp_he = gethostbyname(peerinfo.peer_dirty_reverse_name);
    if (tmp_he != NULL) {
      peerinfo.peer_dirty_forward_name = strdup(tmp_he->h_name);
      if (peerinfo.peer_dirty_forward_name != NULL) {
	for (pp = tmp_he->h_addr_list; *pp != NULL; pp += 1) {
	  if (bcmp(((struct in_addr *) *pp),
		   (struct in_addr *) &(peer_sa.sin_addr.s_addr),
		   sizeof(struct in_addr)) == 0) {
	    ok = 1;
	    break;
	  }
	}
      } else {
	peerinfo.peer_dirty_forward_name = "UNKNOWN";
      }
    } else {
      peerinfo.peer_dirty_forward_name = "UNKNOWN";
    }

    peerinfo.peer_clean_forward_name = strdup(cleanitup(peerinfo.peer_dirty_forward_name));
    if (peerinfo.peer_clean_forward_name == NULL) {
      syslog(LOG_ERR, "Malloc failed during initialization - bye!");
      exit(EX_CONFIG);
    }
    if (strcmp(peerinfo.peer_clean_forward_name, peerinfo.peer_dirty_forward_name) != 0) {
      syslog(LOG_ALERT, "Suspicious characters in hostname for address %s, cleaned to %s",
	     peerinfo.peer_ok_addr, peerinfo.peer_clean_forward_name);
      if (Paranoid_Dns) {
	syslog(LOG_CRIT, "Abandoning session from %s due to suspicious hostname",
	       peerinfo.peer_ok_addr);
	smtp_exit(EX_PROTOCOL);
      }
    }
    /*
     * If we got a forward name and it doesn't match the reverse name
     * then grumble (and exit if paranoid mode is set).
     */

    /* Andreas Borchert <borchert@mathematik.uni-ulm.de> noticed
     * That I was using strcmp here instead of strcasecmp. The match
     * should be made case-insensitevly according to rfc 1033
     */

    if (ok && (strcasecmp(peerinfo.peer_clean_forward_name, peerinfo.peer_clean_reverse_name) != 0)
	&& *peerinfo.peer_clean_forward_name != '\0') {
      syslog(LOG_ALERT, "Probable DNS spoof/misconfiguration from ip=%s, claiming to be host %s", peerinfo.peer_ok_addr, peerinfo.peer_clean_reverse_name);
      if (Paranoid_Dns) {
	syslog(LOG_CRIT, "Abandoning session from ip=%s due to DNS inconsistency", peerinfo.peer_ok_addr);
	exit(EX_PROTOCOL);
      }
    }
  }
  if (peerinfo.my_clean_reverse_name == NULL) {
    /*
     * Our caller didn't say who we're gonna claim to be, and we
     * didn't get one from a getsockname. get our hostname and use
     * that. 
     */
    char hname[MAXHOSTNAMELEN];
    struct hostent *hp;

    if (gethostname(hname, sizeof hname) != 0) {
      syslog(LOG_ERR, "gethostname() call failed! (%m) Who am I?");
      exit(EX_OSERR);
    }
    if ((hp = gethostbyname(hname)) != NULL) {
      peerinfo.my_clean_reverse_name = strdup(hp->h_name);
    } else {
      peerinfo.my_clean_reverse_name = strdup(hname);
    }
    if (peerinfo.my_clean_reverse_name == NULL) {
      syslog(LOG_ERR, "Malloc failed, abandoning session.");
      exit(EX_OSERR);
    }
  }
  /*
   * Allocate the mbuf's - start off small to ensure that 'grow mbuf'
   * code gets exercised (detects bugs faster). 
   */
  input_buf = alloc_smtp_mbuf(64);
  if (input_buf == NULL) {
    syslog(LOG_ERR, "Malloc failed, abandoning session.");
    exit(EX_OSERR);
  }
  output_buf = alloc_smtp_mbuf(64);
  if (output_buf == NULL) {
    syslog(LOG_ERR, "Malloc failed, abandoning session.");
    exit(EX_OSERR);
  }
  reply_buf = alloc_smtp_mbuf(64);
  if (reply_buf == NULL) {
    syslog(LOG_ERR, "Malloc failed, abandoning session.");
    exit(EX_OSERR);
  }
  last_state = &last_state_s;
  current_state = &current_state_s;

  zap_state(current_state);
  zap_state(last_state);

  if (!daemon_mode && listen_addr.s_addr != INADDR_ANY) {
    /* Are we allowed to talk on the address we accepted this connection
     * on? - check to see that we are the defined listening address, or
     * the loopback.
     */
    if ((listen_addr.s_addr != peerinfo.my_sa->sin_addr.s_addr)
	&& (listen_addr.s_addr != htonl(INADDR_LOOPBACK)) 
	)
      {
      /* tell the client to go away - we're not allowed to talk. */
      writereply(reply_buf, 521, 0,
		 peerinfo.my_clean_reverse_name,
		 " ",
		 m521msg,
		 NULL);
      flush_smtp_mbuf(reply_buf, replyfd, reply_buf->offset);
      syslog(LOG_INFO, "Refused connection attempt from %s(%s) to %s(%s)",
	     peerinfo.peer_clean_reverse_name, peerinfo.peer_ok_addr,
	     peerinfo.my_clean_reverse_name, inet_ntoa(listen_addr));
      smtp_exit(EX_OK);
    }
  }
    

  writereply(reply_buf, 220, 0,
	     peerinfo.my_clean_reverse_name,
	     " ",
	     m220msg,
	     NULL);

  replyfd = 1;
  for (;;) {
    char *line;
    size_t offset;
    time_t tt;

    flush_smtp_mbuf(reply_buf, replyfd, reply_buf->offset);
    k = read_smtp_mbuf(input_buf, 0, 1024);
    if (k < 0) {
      syslog(LOG_ERR, "Read failed from client fd (%m) - Abandoning session");
      smtp_exit(EX_OSERR);
    } else if (k == 0) {
      /*
       * eof 
       */
      if (VerboseSyslog) {
	syslog(LOG_INFO, "EOF on client fd.  At least they could say goodbye!");
      } else {
	accumlog(LOG_INFO, "EOF on client fd.");
      }
      smtp_exit(EX_OSERR);
    }
    offset = 0;
    line = smtp_get_line(input_buf, &offset);
    while (line != NULL) {
      clean_smtp_mbuf(input_buf, offset);
      offset = 0;
      memcpy(last_state, current_state, sizeof(smtp_state_set));
      smtp_parse_cmd(line, output_buf, reply_buf, current_state);
      flush_smtp_mbuf(reply_buf, replyfd, reply_buf->offset);
      if (sane_state(current_state)) {
	if ((!test_state(SNARF_DATA, last_state)) &&
	    (test_state(SNARF_DATA, current_state))) {
	  char head[512];
	  char *foo;
	  long msize;

	  memset(head, 0, sizeof(head));

	  outfd = smtp_open_spoolfile();
	  time(&tt);
	  strcpy(head, "BODY\nReceived: from ");
	  strncat(head, peerinfo.peer_clean_reverse_name, 65);
	  strcat(head, "(");
	  strncat(head, peerinfo.peer_ok_addr, 65);
	  if (strcasecmp(peerinfo.peer_clean_reverse_name, client_claimed_name) == 0) {
	    strcat(head, ")");
	  } else {
	    strcat(head, "), claiming to be \"");
	    strncat(head, client_claimed_name, 65);
	    strcat(head, "\"");
	  }
	  strcat(head, "\n via SMTP by ");
	  strncat(head, peerinfo.my_clean_reverse_name, 65);
	  strcat(head, ", id ");
	  if ((foo = strrchr(spoolfile, '/')) != NULL) {
	    strncat(head, foo + 1, 65);
	  } else {
	    strncat(head, spoolfile, 65);
	  }
	  strcat(head, "; ");
	  strncat(head, ctime(&tt), 65);
	  if (!write_smtp_mbuf(output_buf, head, strlen(head))) {
	    syslog(LOG_ERR, "Couldn't write to output buffer, abandoning session");
	    smtp_exit(EX_OSERR);
	  }
	  flush_smtp_mbuf(output_buf, outfd, output_buf->offset);
	  msize = maxsize;
	  i = snarfdata(0, outfd, &msize, 0);
	  switch (i) {
	  case 1:
	    /*
	     * success 
	     */
	    smtp_close_spoolfile(outfd);
	    writereply(reply_buf, 250, 0, m250gotit, NULL);
	    flush_smtp_mbuf(reply_buf, replyfd, reply_buf->offset);
	    if (VerboseSyslog) {
	      syslog(LOG_INFO, "Received %ld bytes of message body from %s(%s)",
		     msize, peerinfo.peer_clean_reverse_name,
		     peerinfo.peer_ok_addr);
	    } else {
	      accumlog(LOG_INFO, " bytes=%ld", msize);
	      accumlog(LOG_INFO, 0);	/* flush */
 	    }	    
	    clear_state(SNARF_DATA, current_state);
	    clear_state(OK_RCPT, current_state);
	    clear_state(OK_MAIL, current_state);
	    break;
	  case 2:
	    /*
	     * read failure on input, or something horrific 
	     */
	    writereply(reply_buf, 554, 0, m554msg, NULL);
	    flush_smtp_mbuf(reply_buf, replyfd, reply_buf->offset);
	    smtp_nuke_spoolfile(outfd);
	    clear_state(SNARF_DATA, current_state);
	    clear_state(OK_RCPT, current_state);
	    clear_state(OK_MAIL, current_state);
	    break;
	  case 3:
	    /*
	     * maxsize exceeded 
	     */
	    writereply(reply_buf, 552, 0, m552msg, NULL);
	    flush_smtp_mbuf(reply_buf, replyfd, reply_buf->offset);
	    smtp_nuke_spoolfile(outfd);
	    clear_state(SNARF_DATA, current_state);
	    clear_state(OK_RCPT, current_state);
	    clear_state(OK_MAIL, current_state);
	    break;
	  case 4:
	    /*
	     * No room on spool device 
	     */
	    writereply(reply_buf, 452, 0, m452msg, NULL);
	    flush_smtp_mbuf(reply_buf, replyfd, reply_buf->offset);
	    smtp_nuke_spoolfile(outfd);
	    clear_state(SNARF_DATA, current_state);
	    clear_state(OK_RCPT, current_state);
	    clear_state(OK_MAIL, current_state);
	    break;
	  case 5:
	    /*
	     * malloc barfed 
	     */
	    writereply(reply_buf, 452, 0, m452msg, NULL);
	    flush_smtp_mbuf(reply_buf, replyfd, reply_buf->offset);
	    smtp_nuke_spoolfile(outfd);
	    clear_state(SNARF_DATA, current_state);
	    clear_state(OK_RCPT, current_state);
	    clear_state(OK_MAIL, current_state);
	    break;
	  default:
	    /*
	     * muy trabajo 
	     */
	    writereply(reply_buf, 451, 0, m451msg, NULL);
	    flush_smtp_mbuf(reply_buf, replyfd, reply_buf->offset);
	    smtp_nuke_spoolfile(outfd);
	    smtp_exit(EX_SOFTWARE);
	  }
	}
      } else {
	/*
	 * evil state. 
	 */
	syslog(LOG_CRIT, "CRITICAL - bad state, aborting");
	abort();
      }
      line = smtp_get_line(input_buf, &offset);
    }
  }
}
