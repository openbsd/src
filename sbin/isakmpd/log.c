/*	$OpenBSD: log.c,v 1.19 2001/07/10 07:55:05 markus Exp $	*/
/*	$EOM: log.c,v 1.30 2000/09/29 08:19:23 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 1999, 2000 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/types.h>
#include <sys/time.h>

#ifdef USE_DEBUG
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#ifdef HAVE_PCAP
#include <pcap.h>
#else
#include "sysdep/common/pcap.h"
#endif

#endif /* USE_DEBUG */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "isakmp_num.h"
#include "log.h"

static void _log_print (int, int, const char *, va_list, int, int);

static FILE *log_output;

#ifdef USE_DEBUG
static int log_level[LOG_ENDCLASS];

#define TCPDUMP_MAGIC	0xa1b2c3d4
#define SNAPLEN		(64 * 1024)

struct packhdr {
  struct pcap_pkthdr pcap;		/* pcap file packet header */
  struct {
    u_int32_t null_family;		/* NULL encapsulation */
  } null;
  struct ip ip;				/* IP header (w/o options) */
  struct udphdr udp;			/* UDP header */
};

struct isakmp_hdr {
  u_int8_t icookie[8], rcookie[8];
  u_int8_t next, ver, type, flags;
  u_int32_t msgid, len;
};

static char *pcaplog_file = NULL;
static FILE *packet_log;
static u_int8_t pack[SNAPLEN + sizeof (struct packhdr)];
static struct packhdr *hdr;

static int udp_cksum (const struct ip *, const struct udphdr *, int);
static u_int16_t in_cksum (const struct ip *, int);
#endif /* USE_DEBUG */

void
log_init (void)
{
  log_output = stderr;
}

void
log_to (FILE *f)
{
  if (!log_output && f)
    closelog ();
  log_output = f;
  if (!f)
    openlog ("isakmpd", LOG_PID | LOG_CONS, LOG_DAEMON);
}

FILE *
log_current (void)
{
  return log_output;
}

static char *
_log_get_class (int error_class)
{
  /* XXX For test purposes. To be removed later on?  */
  static char *class_text[] = LOG_CLASSES_TEXT;

  if (error_class < 0)
    return "Dflt";
  else if (error_class >= LOG_ENDCLASS)
    return "Unkn";
  else
    return class_text[error_class];
}

static void
_log_print (int error, int syslog_level, const char *fmt, va_list ap, 
	    int class, int level)
{
  char buffer[LOG_SIZE], nbuf[LOG_SIZE + 32];
  static const char fallback_msg[] = 
    "write to log file failed (errno %d), redirecting output to syslog";
  int len;
  struct tm *tm;
  struct timeval now;
  time_t t;

  len = vsnprintf (buffer, LOG_SIZE, fmt, ap);
  if (len < LOG_SIZE - 1 && error)
    snprintf (buffer + len, LOG_SIZE - len, ": %s", strerror (errno));
  if (log_output)
    {
      gettimeofday (&now, 0);
      t = now.tv_sec;
      tm = localtime (&t);
      if (class >= 0)
	sprintf (nbuf, "%02d%02d%02d.%06ld %s %02d ", tm->tm_hour, 
		 tm->tm_min, tm->tm_sec, now.tv_usec, _log_get_class (class), 
		 level);
      else /* LOG_PRINT (-1) or LOG_REPORT (-2) */
	sprintf (nbuf, "%02d%02d%02d.%06ld %s ", tm->tm_hour, 
		 tm->tm_min, tm->tm_sec, now.tv_usec,
		 class == LOG_PRINT ? "Default" : "Report>");	
      strcat (nbuf, buffer);
      strcat (nbuf, "\n");

      if (fwrite (nbuf, strlen (nbuf), 1, log_output) == 0)
	{
	  /* Report fallback.  */
	  syslog (LOG_ALERT, fallback_msg, errno);
	  fprintf (log_output, fallback_msg, errno);

	  /* 
	   * Close log_output to prevent isakmpd from locking the file.
	   * We may need to explicitly close stdout to do this properly.
	   * XXX - Figure out how to match two FILE *'s and rewrite.
	   */  
	  if (fileno (log_output) != -1
	      && fileno (stdout) == fileno (log_output))
	    fclose (stdout);
	  fclose (log_output);

	  /* Fallback to syslog.  */
	  log_to (0);

	  /* (Re)send current message to syslog().  */
	  syslog (class == LOG_REPORT ? LOG_ALERT
		  : syslog_level, "%s", buffer);
	}
    }
  else
    syslog (class == LOG_REPORT ? LOG_ALERT : syslog_level, "%s", buffer);
}

#ifdef USE_DEBUG
void
#ifdef __STDC__
log_debug (int cls, int level, const char *fmt, ...)
#else
log_debug (cls, level, fmt, va_alist)
     int cls;
     int level;
     const char *fmt;
     va_dcl
#endif
{
  va_list ap;

  /*
   * If we are not debugging this class, or the level is too low, just return.
   */
  if (cls >= 0 && (log_level[cls] == 0 || level > log_level[cls]))
    return;
#ifdef __STDC__
  va_start (ap, fmt);
#else
  va_start (ap);
  fmt = va_arg (ap, const char *);
#endif
  _log_print (0, LOG_DEBUG, fmt, ap, cls, level);
  va_end (ap);
}

void
log_debug_buf (int cls, int level, const char *header, const u_int8_t *buf,
	       size_t sz)
{
  char s[73];
  int i, j;

  /*
   * If we are not debugging this class, or the level is too low, just return.
   */
  if (cls >= 0 && (log_level[cls] == 0 || level > log_level[cls]))
    return;

  log_debug (cls, level, "%s:", header);
  for (i = j = 0; i < sz;)
    {
      sprintf (s + j, "%02x", buf[i++]);
      j += 2;
      if (i % 4 == 0)
	{
	  if (i % 32 == 0)
	    {
	      s[j] = '\0';
	      log_debug (cls, level, "%s", s);
	      j = 0;
	    }
	  else
	    s[j++] = ' ';
	}
    }
  if (j)
    {
      s[j] = '\0';
      log_debug (cls, level, "%s", s);
    }
}

void
log_debug_cmd (int cls, int level)
{
  if (cls < 0 || cls >= LOG_ENDCLASS)
    {
      log_print ("log_debug_cmd: invalid debugging class %d", cls);
      return;
    }

  if (level < 0)
    {
      log_print ("log_debug_cmd: invalid debugging level %d for class %d",
		 level, cls);
      return;
    }

  if (level == log_level[cls])
    log_print ("log_debug_cmd: log level unchanged for class %d", cls);
  else
    {
      log_print ("log_debug_cmd: log level changed from %d to %d for class %d",
		 log_level[cls], level, cls);
      log_level[cls] = level;
    }
}
#endif /* USE_DEBUG */

void
#ifdef __STDC__
log_print (const char *fmt, ...)
#else
log_print (fmt, va_alist)
     const char *fmt;
     va_dcl
#endif
{
  va_list ap;

#ifdef __STDC__
  va_start (ap, fmt);
#else
  va_start (ap);
  fmt = va_arg (ap, const char *);
#endif
  _log_print (0, LOG_NOTICE, fmt, ap, LOG_PRINT, 0);
  va_end (ap);
}

void
#ifdef __STDC__
log_error (const char *fmt, ...)
#else
log_error (fmt, va_alist)
     const char *fmt;
     va_dcl
#endif
{
  va_list ap;

#ifdef __STDC__
  va_start (ap, fmt);
#else
  va_start (ap);
  fmt = va_arg (ap, const char *);
#endif
  _log_print (1, LOG_ERR, fmt, ap, LOG_PRINT, 0);
  va_end (ap);
}

void
#ifdef __STDC__
log_fatal (const char *fmt, ...)
#else
log_fatal (fmt, va_alist)
     const char *fmt;
     va_dcl
#endif
{
  va_list ap;

#ifdef __STDC__
  va_start (ap, fmt);
#else
  va_start (ap);
  fmt = va_arg (ap, const char *);
#endif
  _log_print (1, LOG_CRIT, fmt, ap, LOG_PRINT, 0);
  va_end (ap);
  exit (1);
}

#ifdef USE_DEBUG
void
log_packet_init (char *newname)
{
  struct pcap_file_header sf_hdr;
  mode_t old_umask;

  if (pcaplog_file && strcmp (pcaplog_file, PCAP_FILE_DEFAULT) != 0)
    free (pcaplog_file);

  pcaplog_file = strdup (newname);
  if (!pcaplog_file)
    {
      log_error ("log_packet_init: strdup (\"%s\") failed", newname);
      return;
    }

  old_umask = umask (S_IRWXG | S_IRWXO);
  packet_log = fopen (pcaplog_file, "w");
  umask (old_umask);

  if (!packet_log)
    {
      log_error ("log_packet_init: fopen (\"%s\", \"w\") failed", 
		 pcaplog_file);
      return;
    }

  log_print ("log_packet_init: starting IKE packet capture to file \"%s\"", 
	     pcaplog_file);

  sf_hdr.magic = TCPDUMP_MAGIC;
  sf_hdr.version_major = PCAP_VERSION_MAJOR;
  sf_hdr.version_minor = PCAP_VERSION_MINOR;
  sf_hdr.thiszone = 0;
  sf_hdr.snaplen = SNAPLEN;
  sf_hdr.sigfigs = 0;
  sf_hdr.linktype = DLT_NULL;

  fwrite ((char *)&sf_hdr, sizeof sf_hdr, 1, packet_log);
  fflush (packet_log);
  
  /* prep dummy header prepended to each packet */
  hdr = (struct packhdr *)pack;
  hdr->null.null_family = htonl(AF_INET);
  hdr->ip.ip_v = 0x4;
  hdr->ip.ip_hl = 0x5;
  hdr->ip.ip_p = IPPROTO_UDP;
  hdr->udp.uh_sport = htons (500);
  hdr->udp.uh_dport = htons (500);
}

void
log_packet_restart (char *newname)
{
  struct stat st;

  if (packet_log)
    {
      log_print ("log_packet_restart: capture already active on file \"%s\"",
		 pcaplog_file);
      return;
    }

  if (newname)
    {
      if (stat (newname, &st) == 0)
	log_print ("log_packet_restart: won't overwrite existing \"%s\"", 
		   newname);
      else
	log_packet_init (newname);
    }
  else if (!pcaplog_file)
    log_packet_init (PCAP_FILE_DEFAULT);
  else if (stat (pcaplog_file, &st) != 0)
    log_packet_init (pcaplog_file);
  else
    {
      /* Re-activate capture on current file.  */
      packet_log = fopen (pcaplog_file, "a");
      if (!packet_log)
	log_error ("log_packet_restart: fopen (\"%s\", \"a\") failed", 
		   pcaplog_file);
      else
	log_print ("log_packet_restart: capture restarted on file \"%s\"",
		   pcaplog_file);
    }
}

void
log_packet_stop (void)
{
  /* Stop capture.  */
  if (packet_log)
    {
      fclose (packet_log);
      log_print ("log_packet_stop: stopped capture");
    }
  packet_log = 0;
}

void
log_packet_iov (struct sockaddr *src, struct sockaddr *dst, struct iovec *iov,
		int iovcnt)
{
  struct isakmp_hdr *isakmphdr;
  int off, len, i;
  
  len = 0;
  for (i = 0; i < iovcnt; i++)
    len += iov[i].iov_len;
  
  if (!packet_log || len > SNAPLEN)
    return;
  
  /* copy packet into buffer */
  off = sizeof *hdr;
  for (i = 0; i < iovcnt; i++) 
    {
      memcpy (pack + off, iov[i].iov_base, iov[i].iov_len);
      off += iov[i].iov_len;
    }
  
  /* isakmp - turn off the encryption bit in the isakmp hdr */
  isakmphdr = (struct isakmp_hdr *)(pack + sizeof *hdr);
  isakmphdr->flags &= ~(ISAKMP_FLAGS_ENC);
  
  /* udp */
  len += sizeof hdr->udp;
  hdr->udp.uh_ulen = htons (len);
  
  /* ip */
  len += sizeof hdr->ip;
  hdr->ip.ip_len = htons (len);

  switch (src->sa_family)
    {
    case AF_INET:
      hdr->ip.ip_src.s_addr = ((struct sockaddr_in *)src)->sin_addr.s_addr;
      hdr->ip.ip_dst.s_addr = ((struct sockaddr_in *)dst)->sin_addr.s_addr;
      break;
    case AF_INET6:
      /* XXX TBD */
    default:
      hdr->ip.ip_src.s_addr = 0x02020202;
      hdr->ip.ip_dst.s_addr = 0x01010101;
    }

  /* Let's use the IP ID as a "packet counter".  */
  i = ntohs (hdr->ip.ip_id) + 1;
  hdr->ip.ip_id = htons (i);

  /* Calculate UDP checksum.  */
  hdr->udp.uh_sum = 0; 
  hdr->udp.uh_sum = udp_cksum (&hdr->ip, &hdr->udp, len);

  /* Calculate IP header checksum. */
  hdr->ip.ip_sum = 0;
  hdr->ip.ip_sum = in_cksum (&hdr->ip, hdr->ip.ip_hl << 2);

  /* null header */
  len += sizeof hdr->null;
  
  /* pcap file packet header */
  gettimeofday (&hdr->pcap.ts, 0);
  hdr->pcap.caplen = len;
  hdr->pcap.len = len;
  len += sizeof hdr->pcap;
  
  fwrite (pack, len, 1, packet_log);
  fflush (packet_log);
  return;
}

/* Copied from tcpdump/print-udp.c  */
static int
udp_cksum (const struct ip *ip, const struct udphdr *up, int len)
{
  int i, tlen;
  union phu {
    struct phdr {
      u_int32_t src;
      u_int32_t dst;
      u_char mbz;
      u_char proto;
      u_int16_t len;
    } ph;
    u_int16_t pa[6];
  } phu;
  const u_int16_t *sp;
  u_int32_t sum;
  tlen = ntohs (ip->ip_len) - ((const char *)up-(const char*)ip);
  
  /* pseudo-header.. */
  phu.ph.len = htons (tlen);
  phu.ph.mbz = 0;
  phu.ph.proto = ip->ip_p;
  memcpy (&phu.ph.src, &ip->ip_src.s_addr, sizeof (u_int32_t));
  memcpy (&phu.ph.dst, &ip->ip_dst.s_addr, sizeof (u_int32_t));
  
  sp = &phu.pa[0];
  sum = sp[0] + sp[1] + sp[2] + sp[3] + sp[4] + sp[5];
  
  sp = (const u_int16_t *)up;
  
  for (i = 0; i < (tlen&~1); i += 2)
    sum += *sp++;
  
  if (tlen & 1) {
    sum += htons ((*(const char *)sp) << 8);
  }
  
  while (sum > 0xffff)
    sum = (sum & 0xffff) + (sum >> 16);
  sum = ~sum & 0xffff;
  
  return sum;
}

/* Copied from tcpdump/print-ip.c, modified.  */
static u_int16_t
in_cksum (const struct ip *ip, int len)
{
  int nleft = len;
  const u_short *w = (const u_short *)ip;
  u_short answer;
  int sum = 0;
  
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }
  if (nleft == 1)
    sum += htons (*(u_char *)w << 8);
  
  sum = (sum >> 16) + (sum & 0xffff);     /* add hi 16 to low 16 */
  sum += (sum >> 16);                     /* add carry */
  answer = ~sum;                          /* truncate to 16 bits */
  return answer;
}


#endif /* USE_DEBUG */
