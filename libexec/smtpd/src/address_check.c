/*
 * $Id: address_check.c,v 1.3 1998/06/03 08:57:05 beck Exp $ 
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
 * Address checking functionality for Obtuse smtpd. 
 * main routine is smtp_check_addr, which checks a from and to address
 * along with the source of the smtp connection to see if the message
 * should be allowed or denied.
 * 
 *  Reads an address check file of the format:
 *
 * allow|deny|noto:SOURCEPAT [SOURCEPAT ...]:FROM [FROM ...]:TO [TO ..] 
 * 
 *  all matches done in lower case.  All patterns must be Lower case except
 *  for specials. 
 *
 * No code from this file is used unless the daemon is 
 * compiled with CHECK_ADDRESS set to 1.
 *
 * **VERY IMPORTANT** - Unlike the rest of smtpd, these routines by
 * default *DO NOT DENY SERVICE ON FAILURE* in other words, if
 * something happens in the course of checking a mail address so that
 * the address can not be checked, the default is to *ALLOW* on
 * failure rather than the normal firewall-parnoid mode of *DENY* on
 * failure. This is because I do not see these routines as something
 * which normally is used to increase external security, they should
 * be used mainly for nuisance prevention (Making it more difficult
 * for internal users to spam/get spammed, forge mail, etc.)
 * 
 * This behaviour is changable by setting
 * CHECK_ADDRESS_DENY_ON_FAILURE at compile time in which case the
 * routines will deny on failure, including the case of when there is
 * no match for a message in the checking file. 
 */

#ifndef CHECK_ADDRESS
#define CHECK_ADDRESS 0
#endif

#if CHECK_ADDRESS  /* this encases everything */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#ifdef NS_MATCH
#include <resolv.h>
#endif
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <limits.h>
/* #include <sys/utsname.h> */
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#ifdef NEEDS_STRINGS_H
#include <strings.h>
#endif
#ifdef NEEDS_FCNTL_H
#include <fcntl.h>
#endif
#ifdef NEEDS_BSTRING_H
#include <bstring.h>
#endif
#ifdef NEEDS_SELECT_H
#include <sys/select.h>
#endif

#include "smtpd.h"
#if JUNIPER_SUPPORT
#ifdef __linux__
#include <linux/juniper_firewall.h>
#else
#include <netinet/juniper_firewall.h>
#endif
#endif
#if USE_REGEX
#include<regex.h>
#endif

#if CHECK_ADDRESS_DENY_ON_FAILURE
#define CHECK_FAILURE 0
#else
#define CHECK_FAILURE 1
#endif

#ifndef CHECK_IDENT
#define CHECK_IDENT 10
#endif

#ifndef NOTO_DELAY
#define NOT0_DELAY 0
#endif

#ifndef DENY_DELAY
#define DENY_DELAY 0
#endif

#ifndef NS_MATCH
#define NS_MATCH 0
#endif

#define SPANBLANK(p)	while (isspace(*p)) p += 1

/* set by parsing routines in case malloc barfs. */
int Failure = 0;
int line = 0;
extern char *victim;

#if NS_MATCH
#define NSLIMIT 100
#define NSIPLIMIT 500
struct ns_match {
  char *string; /* the string that gave us this */
  int count; /*how many nameservers */
  char **servers;  /* names of servers */
  int ip_count; /* count of server ip's */ 
  char **serv_ip; /* ip of servers in dotted decimal */
  int crop; /* exact match == 0, >0 number of .'s moved right to find match */
};

#endif

#if JUNIPER_SUPPORT
/* Is connection from a trusted interface? */
int connection_trusted(void) {
  int session_kind, session_kind_size;
  /*
   * What kind of session is this?
   */
  
  session_kind_size = sizeof(session_kind);
  if ( getsockopt( 0, IPPROTO_TCP, TCP_JUNIPER_SESSION_KIND,
		   &session_kind, &session_kind_size ) != 0 ) {
    syslog(LOG_CRIT,"CRITICAL - can't get session kind flags (%m) on connected session");
    Failure = 1;
    return(0);
  }
  /* connection kinds that aren't untrusted have come in on a trusted
   * interface - i.e. captured sessions 
   */
  return(session_kind != JUNIPER_UNTRUSTED_SESSION);
}
#endif

/*
 * Handle ident timeouts
 */

static jmp_buf timeout_jmpbuf;
static int do_ident=0; /* should we do an ident? */ 

static void alarm_hdlr(int s) {     
  longjmp(timeout_jmpbuf, s); /* sigh. must be a better way */
}

/*
 * Zap our peer with an ident request and see what happens
 */

int
rfc931_ident(struct peer_info *pi, int ident)
{
  struct sockaddr_in my_query_sa, peer_query_sa;
  int fd, i;
  char tbuf[1024];
  char tbuf2[1024];
  char *cp;
  unsigned int peer_port, my_port; 
  
  /* Vanna, Vanna, pick me a socket.. */

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return(0);
  }
  my_query_sa = *(pi->my_sa);
  peer_query_sa = *(pi->peer_sa);
  my_query_sa.sin_port = htons(0);
  peer_query_sa.sin_port = htons(113);
  
  if (setjmp(timeout_jmpbuf) == 0) {
    signal(SIGALRM, alarm_hdlr);
    alarm(ident);
    
    if (bind(fd, (struct sockaddr *) &my_query_sa,
	     sizeof(my_query_sa)) < 0) {
      alarm(0);
      signal(SIGALRM, SIG_DFL);
      close(fd);
      return(0);
    }

    if (connect(fd, (struct sockaddr *) &peer_query_sa,
		sizeof(peer_query_sa)) < 0) {
      alarm(0);
      signal(SIGALRM, SIG_DFL);
      close(fd);
      return(0);
    }

    sprintf(tbuf, "%u,%u\r\n", ntohs(pi->peer_sa->sin_port),
	    ntohs(pi->my_sa->sin_port));
    i=0;
    while (i < strlen(tbuf)) { 
      int j;
      j=write(fd, tbuf+i, (strlen(tbuf+i)));
      if (j < 0) {
	syslog(LOG_DEBUG, "write error sending ident request (%m)");
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	close(fd);
	return(0);
      }
      else if (j > 0){
	i+=j;
      }
    } 

    /* read the answer back */

    i = 0;
    bzero(tbuf, sizeof(tbuf));
    while((cp = strchr(tbuf, '\n')) == NULL && i < sizeof(tbuf) - 1) {
      int j;
      j = read(fd, tbuf+i, (sizeof(tbuf) - 1) - i);
      if (j < 0) {
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	close(fd);
	return(0);
      }
      i+=j; 
    }
    tbuf[i]='\0';	/* Guaranteed to be room for the '\0' */
    
    /* RFC or no RFC, there is absolutely no excuse
     * for a >80 char ident. 
     */
    
    peer_port = my_port = 0;
    
    if (((sscanf(tbuf,"%u , %u : USERID :%*[^:]:%80s",
		&peer_port, &my_port, tbuf2)) != 3) ||
	(ntohs(pi->peer_sa->sin_port) != peer_port) ||
	(ntohs(pi->my_sa->sin_port) != my_port)) {
      pi->peer_dirty_ident = NULL;
      alarm(0);
      signal(SIGALRM, SIG_DFL);
      close(fd);
      return(0);
    }
    if ((cp = strchr(tbuf2, '\r')) != NULL) {
      *cp = '\0';
    }
    
    pi->peer_dirty_ident = strdup(tbuf2);
    if ( pi->peer_dirty_ident == NULL ) {
      Failure = 1;
      alarm(0);
      signal(SIGALRM, SIG_DFL);
      close(fd);
      return(0);
    }

    /* sanitize what we got from the peer, caller can check
     * differences from original if they care.
     */
    
    pi->peer_clean_ident = strdup( cleanitup(pi->peer_dirty_ident) );
    if (pi->peer_clean_ident == NULL) {
      Failure = 1;
      free(pi->peer_dirty_ident);
      pi->peer_dirty_ident = NULL;
      alarm(0);
      signal(SIGALRM, SIG_DFL);
      close(fd);
      return(0);
    }

    /* Normal return */

    alarm(0);
    signal(SIGALRM, SIG_DFL);
    close(fd);
    return(1);

  } else {

    /* Timeout */
    alarm(0);
    signal(SIGALRM, SIG_DFL);
    close(fd);
    return(0);

  }

}

/* case insensitive pattern match, "pat" is assumed to 
 * be lower case (to avoid matching uppercase specials) 
 */
int match_case_pattern(char *pat, char *string) {
  char c;

  while (1) {
    c = *pat;
    pat++; 
    switch(c) {
    case '\0' :
      return(*string == '\0');
    case '*' :
      c=*pat;
      while (c == '*') {
	pat++;
	c = *pat;
      }
      if (c == '\0')  {
	return(1);
      }
      while (*string != '\0') {
	if (match_case_pattern(pat, string)) {
	  return(1);
	}
	string++;
      }
      return(0);
    default:
      if (tolower(c) != tolower(*string)) {
	return(0);
      }
      string++;
      break;
    }
  }
}


#if USE_REGEX
void regex_error(int code, const regex_t *preg) {
  
  char msgbuf[161];
  int i;
  
  msgbuf[0]='\0';
  i=regerror(code, preg, msgbuf, 160);
  msgbuf[160]='\0';

  syslog(LOG_ERR, "regex: %s%s (line %d of check_rules)",
	 (i>160)?"":"(truncated) ", msgbuf, line);
  if (i > 160) {
    syslog(LOG_ERR, "regex: previous error message truncated by %d bytes",
	   i - 160);
  }
}
    

static int match_regex(const char *rstring, char *string) {
  /* match a regular expression in rstring against string. */
  int r;
  regex_t reg, *preg;
  
  preg = &reg;
  
  r=regcomp(preg, rstring, REG_EXTENDED|REG_ICASE|REG_NOSUB);
  if (r != 0) {
    regex_error(r, preg);
    regfree(preg);
    Failure = 1; 
    return(0);
  }
  r = regexec(preg, string, 0, NULL, 0);
  switch (r) {
  case 0:
    regfree(preg);
    return(1);
    break;
  case REG_NOMATCH:
    regfree(preg);
    return(0);
    break;
  default:
    regex_error(r, preg);
    regfree(preg);
    Failure = 1;
    return(0);
    break;
  }
}
#endif  

/* Cribbed and modified from logdaemon5.6 by Wieste Venema*/ 
static int string_match(char *tok, char *string)
{
    if (tok == NULL) {
      return(0);
    }
    /*
     * If the token has the magic value "ALL" the match always succeeds.
     * Otherwise, return 1 if the token fully matches the string.
     */
    if (strcmp(tok, "ALL") == 0) {		/* all: always matches */
	return (1);
    } 
    if (string == NULL ) {
      /* normal case, had no response on gethostbyname(), ident, etc. */
      return(0);
    }
    else {
#if USE_REGEX
      if ( (tok[0] == '/') && (tok[strlen(tok) - 1] == '/') ) {
	/* match as a regex */
	char * rstring;
	rstring = strdup(tok);
	if (rstring == NULL) {
	  syslog(LOG_ERR, "malloc failed");
	  Failure = 1;
	  return(0);
	}
	else {
	  rstring[strlen(rstring) - 1] = '\0';
	  if (match_regex(rstring+1, string)) {
	    free(rstring);
	    return(1);
	  }
	  else {
	    free(rstring);
	    return(0);
	  }
	}
      }
#endif
      return(match_case_pattern(tok, string));
    }
    return(0);
}


int masked_ip_match(char *tok, char *string)
{
  /* see if token looks like an masked ip address (a.b.c.d/bits),
   * if so, match it against the ip address in dotted
   * decimal form in string. If it doesn't look like a masked ip,
   * of form a.b.c.d/bits, match it against the string as a 
   * regular pattern. This allows for things patterns like:
   * 192.168.20.0/24  == class C 192.168.20.0
   * 192.168.20.*  == same thing.
   */  

  char *p, *tbuf;
  int period_cnt, non_digit;
  in_addr_t adt, mat, madt;
  in_addr_t *addr, *mask;

  mat=INADDR_BROADCAST;
  addr=&adt;
  mask=&mat;

  if (tok==NULL) {
    return(0);
  }

  period_cnt = 0;
  non_digit = 0;
  for ( p = tok; *p != '\0' && *p != '/'; p += 1 ) {
    if ( *p == '.' ) {
      if ( p > tok && *(p-1) == '.' ) {
	return(match_case_pattern(tok, string));
      }
      period_cnt += 1;
    } else if ( !isdigit(*p) ) {
      return(match_case_pattern(tok, string));
    }
  }

  tbuf = malloc(p - tok + 1);
  if (tbuf == NULL) {
    syslog(LOG_ERR, "masked_ip_match: malloc failed");
    Failure = 1;
    return(0);
  }
  strncpy(tbuf,tok,p-tok);
  tbuf[p-tok] = '\0';

  if ( period_cnt == 3 ) {
    int a1, a2, a3, a4;
    
    sscanf(tbuf,"%u.%u.%u.%u",&a1,&a2,&a3,&a4);
    if ( a1 > 255 || a2 > 255 || a3 > 255 || a4 > 255 ) {
      return(0);
    }
    
    ((char *)addr)[0] = a1;
    ((char *)addr)[1] = a2;
    ((char *)addr)[2] = a3;
    ((char *)addr)[3] = a4;
    
  } else if ( strcmp(tbuf,"0") == 0 ) {
    
    ((char *)addr)[0] = 0;
    ((char *)addr)[1] = 0;
    ((char *)addr)[2] = 0;
    ((char *)addr)[3] = 0;
    
  } else {
    /* not a masked address  */
    return(match_case_pattern(tok, string));
  }
  
  free(tbuf);
  if (*p == '/'){
    long bits;
    char *end;
    
    p += 1;
    if ( *p == '\0' ) {
      return(0);
    } else if ( !isdigit(*p) ) {
      /* no number for mask */
      return(0);
    }

    bits = strtol(p,&end,10);
    if ( *end != '\0' ) {
      /* junk at end */
      return(0);
   }
    
    if ( bits < 0 || bits > 32 ) {
      /* out of range */
      return(0);
    }
    
    if ( bits == 0 ) {	/* left shifts of 32 aren't defined */
      mat = 0;
    } else {
      ((char *)mask)[0] = (-1 << (32 - bits)) >> 24;
      ((char *)mask)[1] = (-1 << (32 - bits)) >> 16;
      ((char *)mask)[2] = (-1 << (32 - bits)) >>  8;
      ((char *)mask)[3] = (-1 << (32 - bits)) >>  0;
    }
  }

  /* mask off values */
  adt &= mat;
    
  /* convert string to ipaddr */
  madt=inet_addr(string);
  if (madt == -1) {
    return(0);
  }

  /* mask off connecting address */
  madt  &= mat;

  /* for all the marbles */
  return(madt == adt);
}

/* do a Vixie style rbl lookup for dotquad addr in rbl domain
 * rbl_domain.
 */
int vixie_rbl_lookup(char * rbl_domain, char * addr) {
  char *t, *d, *a;
  t = strdup(addr);
  if (t==NULL) {
    syslog(LOG_ERR, "Malloc failed!"); 
    Failure = 1;
    return(0);
  }
  d = (char *) malloc(strlen(t)+strlen(rbl_domain)+1);
  if (d==NULL) {
    syslog(LOG_ERR, "Malloc failed!"); 
    free(t);
    Failure = 1;
    return(0);
  }
  *d='\0';
  while((a = strrchr(t, '.'))) {
    strcat(d, a+1);
    strcat(d, ".");
    *a='\0';
  }
  strcat(d, t);
  strcat(d, rbl_domain);
  if (gethostbyname(d) != NULL) {
    free(t); free(d);
    return(1);
  }
  free(t); free(d);
  return(0);
}

static int ip_match(char *tok, char *string)
{
    /*
     * If the token has the magic value "ALL" the match always succeeds.
     * Otherwise, return 1 if the token matches the dotted decimal ip
     * address in string.
     */
    if (strcmp(tok, "ALL") == 0) {		/* all: always matches */
	return (1);
    } 
    else if ((string == NULL)) {
      return(0);
    } 
    else if (strncmp(tok, "RBL.", 4) == 0) {
      /* do an rbl style lookup on the IP address in string usind
       * rbl domain of whatever followed RBL in tok
       */
      return(vixie_rbl_lookup(tok+3, string));
    }
    else {
      return(masked_ip_match(tok, string));
    }
    return(0);
}


#if NS_MATCH  
/* Routines for looking up and matching nameservers. 
 * These routines are based on the soa lookup program from 
 * the O'reilly "DNS and BIND" nutshell handbook by Paul Ablitz
 * and Cricket Liu (page 300). 
 */

int
skipName(startOfMsg, cp, endOfMsg)
u_char *startOfMsg;
u_char *cp;
u_char *endOfMsg;
{
    char buf[MAXDNAME];  /* buffer to expand name into */
    int n;               /* number of bytes in compressed name */

    if((n = dn_expand(startOfMsg, endOfMsg, cp,
                                            buf, MAXDNAME)) < 0){
        syslog (LOG_ERR, "dn_expand failed in skipName");
        Failure = 1;
	return(0);
    }
    return(n);
}

/****************************************************************
 * skipToData -- This routine advances the cp pointer to the    *
 *     start of the resource record data portion.  On the way,  *
 *     it fills in the type, class, ttl, and data length        *
 ****************************************************************/
int
skipToData(startOfMsg, cp, type, class, ttl, dlen, endOfMsg)
u_char     *startOfMsg;
u_char     *cp;
u_short    *type;
u_short    *class;
u_int  *ttl;
u_short    *dlen;
u_char     *endOfMsg;
{
    u_char *tmp_cp = cp;  /* temporary version of cp */

    /* Skip the domain name; it matches the name we looked up */
    tmp_cp += skipName(startOfMsg, tmp_cp, endOfMsg);

    /*
     * Grab the type, class, and ttl.  GETSHORT and GETLONG
     * are macros defined in arpa/nameser.h.
     */
    GETSHORT(*type, tmp_cp);
    GETSHORT(*class, tmp_cp);
    GETLONG(*ttl, tmp_cp);
    GETSHORT(*dlen, tmp_cp);

    return(tmp_cp - cp);
}


/****************************************************************
 * findNameServers -- find all of the name servers and MX records for
 * the given string and store their names and ip addresses.  chop off
 * lhs parts of the string untill we find a match. store results in
 * the nameservers structure passed in. don't redo lookups if the
 * nameservers structure already contains what we want from a previous
 * call.
 ****************************************************************/  

void findNameServers(char * string, struct ns_match *nameservers) {
    union {
        HEADER hdr;           /* defined in resolv.h */
        u_char buf[PACKETSZ]; /* defined in arpa/nameser.h */
    } response;               /* response buffers */
    int responseLen;          /* buffer length */

    u_char     *cp;       /* character pointer to parse DNS packet */
    u_char     *endOfMsg; /* need to know the end of the message */
    u_short    class;     /* classes defined in arpa/nameser.h */
    u_short    type;      /* types defined in arpa/nameser.h */
    u_int  ttl;       /* resource record time to live */
    u_short    dlen;      /* size of resource record data */

    int i, count, dup; /* misc variables */

    char *next = NULL;

    /* 
     * Look up the NS records for the given string. We expect the string
     * to be a hostname, the rhs of an e-mail address, or 
     * xx.xx.xx.xx.in-addr.arpa.
     */


    if (nameservers->string != NULL) {
      if (strcmp(nameservers->string, string) == 0) {
	/* This structure already contains what we want, just return */
	return;
      }
      else {
	/* This structure contains old data. free it */
	int i;
	free(nameservers->string);
	for (i=0; i<nameservers->count; i++) {
	  free(nameservers->servers[i]);
	}
	nameservers->count = 0;
	for (i=0; i<nameservers->ip_count; i++) {
	  free(nameservers->serv_ip[i]);
	}
	nameservers->ip_count = 0;
	/* put our new string in the top. */ 
	nameservers->string = strdup(string);
	if (nameservers->string == NULL) {
	  syslog(LOG_ERR, "malloc failed");
	  Failure = 1;
	return;
	}
	nameservers->crop = 0;
      }
    }
    else {
      /* allocate space in the structure */
      nameservers->string = strdup(string);
      if (nameservers->string == NULL) {
	syslog(LOG_ERR, "malloc failed");
	Failure = 1;
	return;
      }
      nameservers->servers = (char **) malloc(NSLIMIT * sizeof(char *));
      if (nameservers->servers == NULL) {
	syslog(LOG_ERR, "malloc failed");
	Failure = 1;
	return;
      }
      nameservers->serv_ip = (char **) malloc(NSIPLIMIT * sizeof(char *));
      if (nameservers->serv_ip == NULL) {
	syslog(LOG_ERR, "malloc failed");
	Failure = 1;
	return;
      }
    }
    cp = nameservers->string;
    while ((responseLen = 
           res_query(cp,      /* the domain we care about   */
                     C_IN,        /* Internet class records     */
                     T_ANY,        /* pah, give me anything, I'll find NS. */
                     (u_char *)&response,      /*response buffer*/
                     sizeof(response)))        /*buffer size    */
                                        < 0){  /*If negative    */

      /*
       * move ahead to the next thing after a "." in our string. 
       * see if we can find something for that. Don't look up stuff when
       * no "." is left, so we don't look up top-level domains.
       */
      cp = (next == NULL)?strchr(cp, '.'):next;
      if (cp == NULL) {
	return;
      }
      cp++;
      next = strchr(cp, '.');
      if (next == NULL) {
	return;
      }
      nameservers->crop++; /* keep track of how many pieces we lopped off */
    }
    
    /*
     * Keep track of the end of the message so we don't 
     * pass it while parsing the response.  responseLen is 
     * the value returned by res_query.
     */
    endOfMsg = response.buf + responseLen;

    /*
     * Set a pointer to the start of the question section, 
     * which begins immediately AFTER the header.
     */
    cp = response.buf + sizeof(HEADER);

    /*
     * Skip over the whole question section.  The question 
     * section is comprised of a name, a type, and a class.  
     * QFIXEDSZ (defined in arpa/nameser.h) is the size of 
     * the type and class portions, which is fixed.  Therefore, 
     * we can skip the question section by skipping the 
     * name (at the beginning) and then advancing QFIXEDSZ.
     * After this calculation, cp points to the start of the 
     * answer section, which is a list of NS records.
     */
    cp += skipName(response.buf, cp, endOfMsg) + QFIXEDSZ;

    /*
     * Create a list of name servers from the response.
     * NS records may be in the answer section and/or in the
     * authority section depending on the DNS implementation.  
     * Walk through both.  The name server addresses may be in
     * the additional records section, but we will ignore them
     * since it is much easier to call gethostbyname() later
     * than to parse and store the addresses here.
     */
    count = ntohs(response.hdr.ancount) + 
            ntohs(response.hdr.nscount);
    while (    (--count >= 0)        /* still more records     */
            && (cp < endOfMsg)       /* still inside the packet*/
            && (nameservers->count < NSLIMIT)) { /* still under our limit  */

        if (nameservers->count == (NSLIMIT / 4)) {
	  syslog(LOG_INFO, "%d distinct answers and counting for nameserver info from %s. Possibly very bogus.", nameservers->count, string);
	}

        /* Skip to the data portion of the resource record */
        cp += skipToData(response.buf, cp, &type, &class, &ttl, 
                         &dlen, endOfMsg);

        if (type == T_NS || type == T_MX) { /* look for Nameserver OR MX */

	    u_char tmp_buf[MAXDNAME];
 
	    /*  Don't forget to skip over the MX priority! 
             *  Thanks hps@tanstaafl.de.
             */
	    if(type == T_MX)
	      {
		u_short mx;
		
		GETSHORT(mx, cp);
		dlen -= 2;
	      }
	    
            /* Expand the name server's name */
            if (dn_expand(response.buf, /* Start of the packet   */
                          endOfMsg,     /* End of the packet     */
                          cp,           /* Position in the packet*/
                          tmp_buf, /* Result    */
                          MAXDNAME)     /* size of tmp_buf buffer */
                                    < 0) { /* Negative: error    */

	      /* unfortunately people use lame records that 
	       * dn_expand fails on! sigh, A dns server is only as
	       * good as the weakest link of the code running on it and
	       * the maintainer of it. So (barring the DN_EXPAND_NAME_FAIL
	       * being defined) we'll just ignore these failures, and
	       * treat them like the record didn't exist. This 
	       * mimics sendmail's behaviour in the same instances
	       * (see sendmail domain.c) -BB
	       */

#ifdef DN_EXPAND_NAME_FAIL
                syslog (LOG_ERR, "dn_expand failed to expand %s record in findNameServers",(type == T_NS)?"NS":"MX" );
		Failure = 1;
		return;
#else
                syslog (LOG_DEBUG, "dn_expand failed to expand %s record in findNameServers - ignored record", (type == T_NS)?"NS":"MX" );
#endif
            }
	    else { /* dn_expand ok */

	      /* clean up the answer, in case someone's got something  
	       * hostile or lame in their DNS.
	       */
	      
	      if ((nameservers->servers[nameservers->count]=
		   strdup(cleanitup(tmp_buf))) == NULL) {
		Failure = 1;
		syslog(LOG_ERR, "malloc failed");
		return;
	      }
	      
	      /*
	       * Check the name we've just unpacked and add it to 
	       * the list of servers if it is not a duplicate.
	       * If it is a duplicate, just ignore it.
	       */
	      for(i = 0, dup=0; (i < nameservers->count) && !dup; i++)
                dup = !strcasecmp(nameservers->servers[i], nameservers->servers[nameservers->count]);
	      if(dup) 
                free(nameservers->servers[nameservers->count]);
	      else {
		(nameservers->count)++;
	      }
	    }
	}

        /* Advance the pointer over the resource record data */
        cp += dlen;

    } /* end of while */

    /* We should now have the nameserver names in the severs list. 
     * we now need to get all their IP's. We want to be able to 
     * compare IP's to allow for matching anything NS'ed by anything
     * in a rogue provider's block. 
     */
    for (i=0; i<nameservers->count; i++) {
      struct hostent *host;
      char **pp;
      host = gethostbyname(nameservers->servers[i]);
      if (host != NULL) {
	for (pp=host->h_addr_list; *pp != NULL; pp += 1) {
	  nameservers->serv_ip[nameservers->ip_count]=
	    strdup(inet_ntoa(*((struct in_addr *) *pp)));
	  if (nameservers->serv_ip[nameservers->ip_count] == NULL){
	    syslog(LOG_ERR, "Malloc failed");
	    (nameservers->ip_count)--; 
	    Failure = 1;
	    return;
	  }
	  (nameservers->ip_count)++;
	}
      }
      if (nameservers->ip_count == (NSIPLIMIT / 4)) {
	syslog(LOG_INFO, "%d ip addresses and counting for nameserver info from %s. Possibly very bogus.", nameservers->ip_count, string);
      }
      if (nameservers->ip_count == NSIPLIMIT) {
	syslog(LOG_ERR, "Got %d ip addresses for nameserver infor from %s. I've stopped looking for more.", NSIPLIMIT, string);
 	break;
      }
    }
    if (nameservers->count == NSLIMIT) {
      syslog(LOG_ERR, "Got %d distinct answers for nameserver info from %s. I've stopped looking for more.", nameservers->count, string);
    }
    
}


static int nameserver_match(char *pat, char *match_string, 
			    struct ns_match *nameservers) 

{ /* match a pattern against the namserver for match_string */

  struct hostent *host;
  findNameServers(match_string, nameservers);

  if (strcmp(pat, "UNKNOWN") == 0) {
    /* return a match if the string doesn't resolve as a hostname, and
     * we didn't find a nameserver, or we had to lop off bogus parts
     * of the string to find one.  
     */
    
    host = gethostbyname(match_string);
    if ((host == NULL)
	&& (nameservers->count == 0 || nameservers->crop > 0)) {
	syslog (LOG_DEBUG, "Matched %s UNKNOWN to name service",
		match_string);
      return(1);
    }
    else {
      return(0);
    }
  }

  else if (strcmp(pat, "KNOWN") == 0) {
    /* return a match if the string does resolve as a hostname, or
     * we did find a nameserver without lopping off bogus parts
     * of the string to find one.  
     */
    
    host = gethostbyname(match_string);
    if ((host != NULL) 
	|| (nameservers->count > 0 && nameservers->crop == 0)) {
      syslog (LOG_DEBUG, "Matched %s KNOWN to name service (%s)",
	      match_string, 
	      (host != NULL)?"resolves as hostname":"has NS or MX record" 
	      );
      return(1);
    }
    else {
      return(0);
    }
  }

  else {
    int i;
    /* check against each nameserver IP */
    for (i=0; i<nameservers->ip_count; i++) {
      if (ip_match(pat, nameservers->serv_ip[i])) {
	syslog (LOG_DEBUG, "Matched nameserver ip of %s", 
		nameservers->serv_ip[i]);
	return(1);
      }
    }
    /* check against each nameserver name */
    for (i=0; i<nameservers->count; i++) {
      if (string_match(pat, nameservers->servers[i])) {
	syslog (LOG_DEBUG, "Matched nameserver hostname of %s", 
		nameservers->servers[i]);
	return(1);
      }
    }
  }
  return(0);
}
#endif  

static int address_match(char *pat,
			 const char *match_string,
			 char * user
#if NS_MATCH
			 , struct ns_match * ns
#endif
			 ) {
  /* match an address against a pattern for one. To us an
   * address is a right and left part, deliniated by the  
   * rightmost "@" in the string. 
   */
  char *at, *string, *ostring;
  char *rightp, *leftp;
  char *lefts, *rights;
  int rval = 0;

  string=strdup(match_string);
  if (string == NULL) {
    syslog (LOG_ERR, "Malloc failed");
    Failure = 1;
    return(0);
  }
  ostring = string;
  
  /* an address may be (probably is) enclosed in <>. If it is, 
   * strip them out before the match
   */
  if ((string[0] == '<') && (string[strlen(string) - 1] == '>')) {
    string[strlen(string) - 1] = '\0';
    string++;
    SPANBLANK(string);
  }

  /* if an address is all regex, don't split it, just use regex 
   * on the whole thing 
   * N.B.  this means that you can't use two regex in a pattern i.e.
   * /regex/@bar will work,
   * foo@/regex/ will work, but
   * /regex/@/regex/ won't work
   */
#if USE_REGEX
      if ( (pat[0] == '/') && (pat[strlen(pat) - 1] == '/') ) {
	if (string_match(pat, string)) {
	  free(ostring);
	  return(1);
	}
	free(ostring);
	return(0);
      }
#endif

  /* split the pattern */
  leftp = pat;
  at = strrchr(pat, '@');
  if (at != NULL) {
    *at = '\0';
    rightp = at+1;
  }
  else {
#if NS_MATCH
    /* pattern has no @ could be an NS= */
    if (strncmp (pat, "NS=", 3) == 0) {
      pat+=3;
      /* ok, we want Nameserver, which means the nameserver for
       * whatever's on the right of the @ in the string.
       */
      at = strrchr(string, '@');
      at = (at == NULL)?string:at+1; 
      rval = nameserver_match(pat, at, ns); 
      free(ostring);
      return(rval);
    }
#endif
    rval = string_match(pat, string);
    free(ostring);
    return(rval);
  }


  lefts = string;
  at = strrchr(string, '@');
  if (at != NULL) {
    *at = '\0';
    rights = at+1;
  }
  else {
    rights=NULL;
  }

  if (strcmp(leftp, "USER") == 0) {
    /* for special USER, we replace left side with the username 
     * from the source, for purposes of the match. Username 
     * should have already been forced to lowercase by our caller.
     * 
     * This is used mostly to check the ident of the smtp connection, 
     * and in cases where the ident reply can be trusted, force the 
     * lhs of the email address to match the username returned by ident.
     */
    if ((user != NULL) && (strrchr(user, '*') == NULL)) {
      leftp=user;
    }
    else {
      leftp = NULL;
    }
  }

  if ( ((rightp == NULL) && (rights != NULL)) ||
       ((rights == NULL) && (rightp != NULL)) ) {
    /* if pattern has rhs and string doesn't or vice versa, no go */
    free(ostring);
    return (0);
  }

  if ( string_match(leftp, lefts) ) {
    if ( rights != NULL ) {
#if NS_MATCH
      /* RHS an NS= */
      if (strncmp (rightp, "NS=", 3) == 0) {
	rightp+=3;
	rval = nameserver_match(rightp, rights, ns); 
	free(ostring);
	return(rval);
      }
#endif
      rval = (string_match(rightp, rights));
      free(ostring);
      return(rval);
    }
    free(ostring);
    return(1);
  }
  free(ostring);
  return (0);
}

static int match_host(char * pat, char *host, char * ip) {
  /* match a pattern against a hostname or ip address */
#if NS_MATCH
  static struct ns_match nserv = {NULL, 0, NULL, 0, NULL, 0};
#endif
  
  /* avoid bozos registering a hostname to look like an address */
  if ((host != NULL) && (inet_addr(host) != -1)) { 
    syslog(LOG_ALERT, "ALERT - hostname \"%s\" looks like an IP address. possible subversion attempt!", host); 
    return(ip_match(pat, ip)); 
  }
  else if (strcmp(pat,"KNOWN") == 0) { 
    /* KNOWN == Fully registered.. */
    return((host != NULL) && (strcmp(host, "UNKNOWN") != 0));
  }
  else if (strcmp(pat,"UNKNOWN") == 0) {
    /* UNKNOWN == Not Fully registered.. */
    return((host == NULL) || (strcmp(host, "UNKNOWN") == 0));
  }
#if JUNIPER_SUPPORT
  else if (strcmp(pat,"TRUSTED") == 0) { 
    /* connection on trusted interface */
    return(connection_trusted());
  }
  else if (strcmp(pat,"UNTRUSTED") == 0) {
    /* connection on untrusted interface */
    return(!connection_trusted());
  }
#endif
#if NS_MATCH
  else if (strncmp(pat,"NS=", 3) == 0) { 
    /* we want to match the nameserver for the host, not the host
     * itself.
     */
    pat += 3;
    /* we'll try looking up to find NS using the hostname if we have it, 
     * otherwise, we use the reverse lookup on the IP.
     */
    if (host != NULL && (strcmp(host, "UNKNOWN") != 0)) {
      return(nameserver_match(pat, host, &nserv));
    }
    else { 
      /*
       * phrob ip string into xx.xx.xx.xx.in-addr.arpa.
       */

      char *t, *d, *a;
      t = strdup(ip);
      if (t==NULL) {
	syslog(LOG_ERR, "Malloc failed!"); 
	Failure = 1;
	return(0);
      }
      d = (char *) malloc(strlen(t)+16);
      if (d==NULL) {
	syslog(LOG_ERR, "Malloc failed!"); 
	free(t);
	Failure = 1;
	return(0);
      }
      *d='\0';
      while((a = strrchr(t, '.'))) {
	strcat(d, a+1);
	strcat(d, ".");
	*a='\0';
      }
      strcat(d, t);
      strcat(d, ".in-addr.arpa.");
      if (nameserver_match(pat, d, &nserv)) {
	free(t); free(d);
	return(1);
      }
      free(t); free(d);
      return(0);
    }
  }
#endif
  else {
    return(string_match(pat, host) || ip_match(pat, ip));
  }
}

static int source_match(char *pat, struct peer_info *pi) {

  /* 
   * match a source against a pattern for one.
   */

  char *pa;

  /* if pattern doesn't have a user part (i.e. is just a host or ip)
   * we don't match the user, it's assumed we don't care.
   */

  if ( ((pa = strrchr(pat, '@')) == NULL)
#if USE_REGEX
        || ((pat[0]=='/') && (pat[strlen(pat)-1] == '/'))
#endif
      ) {
    return (match_host(pat, pi->peer_clean_reverse_name, pi->peer_ok_addr));
  }
  else {
    *pa = '\0';
    /* if the pattern has a user part, we need user information. If it
     * hasn't been provided to us, try to get it by zapping our peer
     * with an ident request 
     */
    if (do_ident) {
      if (CHECK_IDENT > 0) {
	(void) rfc931_ident(pi, CHECK_IDENT);
	if (pi->peer_clean_ident != NULL) {
	  int i;
	  /* force to lower case */
	  for (i=0; i < strlen(pi->peer_clean_ident); i++) {
	    pi->peer_clean_ident[i]=tolower(pi->peer_clean_ident[i]);
	  }
	}
      }
      do_ident = 0;
    }
    if (((strcmp(pat, "KNOWN") == 0) && (pi->peer_clean_ident != NULL)) ||
	(string_match(pat, pi->peer_clean_ident))
	) {
      if (match_host(pa+1, pi->peer_clean_reverse_name, pi->peer_ok_addr)) {
	return(1);
      }
    }
  }
  return(0);
}

/* Cribbed and modified from logdaemon5.6 by Wieste Venema */ 
static int address_list_match(char *list,
			      const char *item,
			      char * user
#if NS_MATCH
			      , struct ns_match *ns
#endif
			      )
{
    char   *tok;
    int     match = 0;
    char *sep = ", \t";

    /*
     * Process tokens one at a time. We have exhausted all possible matches
     * when we reach an "EXCEPT" token or the end of the list. If we do find
     * a match, look for an "EXCEPT" list and recurse to determine whether
     * the match is affected by any exceptions.
     */

    for (tok = strtok(list, sep); tok != 0; tok = strtok((char *) 0, sep)) {
	if (strcasecmp(tok, "EXCEPT") == 0)	/* EXCEPT: give up */
	    break;
#if NS_MATCH
	if ((match = address_match(tok, item, user, ns))) 	/* YES */
#else
	if ((match = address_match(tok, item, user))) 	/* YES */
#endif
	    break;
    }
    /* Process exceptions to matches. */

    if (match != 0) {
	while ((tok = strtok((char *) 0, sep)) && strcasecmp(tok, "EXCEPT"))
	     /* VOID */ ;
#if NS_MATCH
	if (tok == 0 || address_list_match((char *) 0, item, user, ns) == 0)
#else
	if (tok == 0 || address_list_match((char *) 0, item, user) == 0)
#endif
	    return (match);
    }
    return (0);
}

/* check a source against a list of sources */
static int source_list_match(char *list, struct peer_info *pi)
{
    char   *tok;
    int     match = 0;
    char *sep = ", \t";

    /*
     * Process tokens one at a time. We have exhausted all possible matches
     * when we reach an "EXCEPT" token or the end of the list. If we do find
     * a match, look for an "EXCEPT" list and recurse to determine whether
     * the match is affected by any exceptions.
     */

    for (tok = strtok(list, sep); tok != 0; tok = strtok((char *) 0, sep)) {
	if (strcasecmp(tok, "EXCEPT") == 0)	/* EXCEPT: give up */
	    break;
	if ((match = (source_match(tok, pi))))	/* YES */
	    break;
    }
    /* Process exceptions to matches. */

    if (match != 0) {
	while ((tok = strtok((char *) 0, sep)) && strcasecmp(tok, "EXCEPT"))
	     /* VOID */ ;
	if (tok == 0 || source_list_match((char *) 0, pi) == 0)
	    return (match);
    }
    return (0);
}

/* Parse an address check file to see if a particular message from
 * "fromaddr" to "toaddr" is allowed from a particular peer. Minimally,
 * pi must have filled in pi->my_sa, pi->peer_sa, and pi->peer_ok_addr.
 * When used, this gets from smtpd for each recipient of a message.
 */

int smtpd_addr_check(const char * checkfname,
		     struct peer_info *pi,
		     const char * from,
		     const char * to,
		     char ** return_message) {
  FILE *fp;
  char buf[1024]; 
  char *action, *sourcepat, *fromaddrpat, *toaddrpat, *junk;
  char *sep = ":";
#if NS_MATCH
  static struct ns_match NS_from = { NULL, 0, NULL, 0, NULL, 0 };
  static struct ns_match NS_rcpt = { NULL, 0, NULL, 0, NULL, 0 };
#endif

  line=0;

  /* force user ident (if supplied) to lower case. */
  if (pi->peer_clean_ident != NULL) {
    int i;
    /* force to lower case */
    for (i=0; i < strlen(pi->peer_clean_ident); i++) {
      pi->peer_clean_ident[i]=tolower(pi->peer_clean_ident[i]);
    }
    do_ident = 0; 
  }
  else {
    do_ident = 1;
  }
  
  if ((fp = fopen(checkfname, "r")) != 0) {
    while(fgets(buf, sizeof(buf) - 1, fp) != NULL) {
      buf[sizeof(buf) - 1]='\0';
      line++;
      if (buf[0]=='\0') {
	/* some nob put a null byte in the file */
	syslog(LOG_ALERT, "Null byte in file %s!", checkfname);
      }
      if (buf[strlen(buf) - 1] != '\n') {
	syslog (LOG_ALERT,
		"Line %d too long in file %s! Can not check address!",
		line, checkfname);
	fclose(fp);
	return(CHECK_FAILURE);
      }
	
      if ((buf[0] == '#') || (buf[0] == '\n')) {
	continue;  /* ignore comments and blank lines */
      }
      
      buf[strlen(buf) - 1] = '\0';
      
      /* parse out fields in line */
      if (!(action = strtok(buf, sep))
	  ||!(sourcepat = strtok(NULL, sep))
	  ||!(fromaddrpat = strtok(NULL, sep))
	  ||!(toaddrpat = strtok(NULL, sep))) {
	syslog (LOG_ERR, "%s: line %d, bad field count", checkfname, line);
	fclose(fp);
	return(CHECK_FAILURE);
      }
      if ((junk=strtok(NULL, sep))) {
	SPANBLANK(junk);
	if (junk[0] != '#') {
	  /* must be a message */
	  if (*return_message != NULL) {
	    free(*return_message);
	  }
	  *return_message = strdup(junk);
	  if (*return_message == NULL) {
	    syslog (LOG_ERR, "Malloc failed!");
	    fclose(fp);
	    return(CHECK_FAILURE);
	  }
	  if ((junk=strtok(NULL, sep))) {
	    SPANBLANK(junk);
	    if (junk[0] != '#') {
	      syslog (LOG_ERR, "%s: line %d, junk at end of line \"%s\"",
		      checkfname, line, junk);
	      fclose(fp);
	      return(CHECK_FAILURE);
	    }
	  }
	}
	else {
	  if (*return_message != NULL) {
	    free(*return_message);
	    *return_message=NULL; 
	  }
	}
      }
      else {
	if (*return_message != NULL) {
	  free(*return_message);
	  *return_message=NULL; 
	}
      }
      
      /* is this line applicable to our source */
      if ( source_list_match(sourcepat, pi) 

	   /* yes it is. does the from address match? */
	   && ( address_list_match(fromaddrpat, 
				   from, 
				   pi->peer_clean_ident
#if NS_MATCH
				   , &NS_from
#endif
				   ) )  
	   
	   /* yep. How about the to address ? */
	   && ( address_list_match(toaddrpat, 
				   to,
				   pi->peer_clean_ident
#if NS_MATCH
				   , &NS_rcpt
#endif				   
				   ) )

	   ) {

	if (Failure) {
	  /* Something died while parsing */
	  syslog (LOG_ERR, "Returning default of %d due to previous failure", CHECK_FAILURE);
          fclose(fp);
	  return(CHECK_FAILURE);
	}
	/* we've matched this rule. is it an allow or deny? */
	if (strcmp(action, "allow") == 0) {
	  /* allows succeed silently */
	  syslog(LOG_DEBUG, "smtp connection from %s@%s(%s) MAIL FROM: %s RCPT TO: %s, allowed by line %d of %s",
		 (pi->peer_clean_ident != NULL)?
		 pi->peer_clean_ident:"UNKNOWN",
		 (pi->peer_clean_reverse_name != NULL)?
		 pi->peer_clean_reverse_name:"UNKNOWN",
		 pi->peer_ok_addr, from, to, line, checkfname);
          fclose(fp);
	  return (1);
	}
	else if (strcmp(action, "noto") == 0) {
	  /* notos generate a log message */
	  syslog(LOG_INFO, "smtp connection from %s@%s(%s) attempted MAIL FROM: %s RCPT TO: %s, noto by line %d of %s",
		 (pi->peer_clean_ident != NULL)?
		 pi->peer_clean_ident:"UNKNOWN",
		 (pi->peer_clean_reverse_name != NULL)?
		 pi->peer_clean_reverse_name:"UNKNOWN",
		 pi->peer_ok_addr, from, to, line, checkfname);
          fclose(fp);
	  return (-1);
	}
	else if (strcmp(action, "deny") == 0) {
	  /* denys generate a log message */
	  syslog(LOG_INFO, "smtp connection from %s@%s(%s) attempted MAIL FROM: %s RCPT TO: %s, denied by line %d of %s",
		 (pi->peer_clean_ident != NULL)?
		 pi->peer_clean_ident:"UNKNOWN",
		 (pi->peer_clean_reverse_name != NULL)?
		 pi->peer_clean_reverse_name:"UNKNOWN",
		 pi->peer_ok_addr, from, to, line, checkfname);
          fclose(fp);
	  return (0);
	}

	/* noto_delay and deny_delay. Ok, I admit they aren't very
	 * nice, but I like to be able to do the same thing to
	 * spammers that I do to phone telemarketers (Feign interest,
	 * saying you'll be right back with some convincing excuse,
	 * then put the phone down until they get bored and hang up).
	 * I figure it's my duty protecting the next victim on the 
	 * list for however long it takes them, and costing the caller 
	 * however many minutes in unproductive time.
	 * 
	 * I used to do this by having my packet filter drop TCP SYN's
	 * to port 25 from their sites to make them TCP timout ( which
	 * is also effective :), but it can make for a long packet 
	 * filter list in the kernel. 
	 *
	 * - Bob's Evil Twin. 
	 */

	else if (strcmp(action, "deny_delay") == 0) {
	  /* denys generate a log message */
	  syslog(LOG_INFO, "smtp connection from %s@%s(%s) attempted MAIL FROM: %s RCPT TO: %s, denied with delay by line %d of %s",
		 (pi->peer_clean_ident != NULL)?
		 pi->peer_clean_ident:"UNKNOWN",
		 (pi->peer_clean_reverse_name != NULL)?
		 pi->peer_clean_reverse_name:"UNKNOWN",
		 pi->peer_ok_addr, from, to, line, checkfname);
	  syslog(LOG_INFO, "Sleeping for a deny_delay of %d seconds", DENY_DELAY);
	  sleep(DENY_DELAY);
          fclose(fp);
	  return (0);
	}
	else if (strcmp(action, "noto_delay") == 0) {
	  /* notos generate a log message */
	  syslog(LOG_INFO, "smtp connection from %s@%s(%s) attempted MAIL FROM: %s RCPT TO: %s, noto with delay by line %d of %s",
		 (pi->peer_clean_ident != NULL)?
		 pi->peer_clean_ident:"UNKNOWN",
		 (pi->peer_clean_reverse_name != NULL)?
		 pi->peer_clean_reverse_name:"UNKNOWN",
		 pi->peer_ok_addr, from, to, line, checkfname);
	  syslog(LOG_INFO, "Sleeping for a noto_delay of %d seconds", NOTO_DELAY);
	  sleep(NOTO_DELAY);
          fclose(fp);
	  return (-1);
	}
	else if (strcmp(action, "debug") == 0) {
	  syslog (LOG_INFO, "DEBUG: Matched line %d, connection from %s@%s(%s) MAIL FROM: %s RCPT TO: %s, continuing.",
		line,
		 (pi->peer_clean_ident != NULL)?
		 pi->peer_clean_ident:"UNKNOWN",
		 (pi->peer_clean_reverse_name != NULL)?
		 pi->peer_clean_reverse_name:"UNKNOWN",
		 pi->peer_ok_addr, from, to);
	}
	else {
	  /* bogus action - fail */
	  syslog (LOG_ERR, "Unknown action \"%s\" in rule at line %d of file %s", action, line, checkfname);
          fclose(fp);
	  return(CHECK_FAILURE);
	}
      }

      /* This is currently unneccessary, as it gets checked above and we don't
       * call anything in here that will set Failure. It is left here in case
       * of future modifications 
       */
      if (Failure) {
	  /* Something died while parsing */
	  syslog (LOG_ERR, "Returning default of %d due to previous failure", CHECK_FAILURE);
          fclose(fp);
	  return(CHECK_FAILURE);
      }
    }
    /* we've parsed the whole file, and no match. as such we return 
     * CHECK_FAILURE, our choice of fail-on behaviour at compile time 
     * determining if this is an allowed or denied message.
     */
    syslog(LOG_DEBUG, "smtp connection from %s@%s(%s) MAIL FROM: %s RCPT TO: %s, default action, reached end of %s",
	   (pi->peer_clean_ident != NULL)?
	   pi->peer_clean_ident:"UNKNOWN",
	   (pi->peer_clean_reverse_name != NULL)?
	   pi->peer_clean_reverse_name:"UNKNOWN",
	   pi->peer_ok_addr, from, to, checkfname);
    fclose(fp);
    return (CHECK_FAILURE);
  }
  else { 
    /* fopen() coughed up a lung */
    syslog(LOG_ERR, "Can not open from address check file %s (%m)!", checkfname);
    return(CHECK_FAILURE); 
  }
}

#endif /* CHECK_ADDRESS */

