/* Obtuse smtpd SMTP store daemon header file
 *
 * $Id: smtpd.h,v 1.2 1998/06/03 08:57:12 beck Exp $ 
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

#include<arpa/nameser.h>
#include<sys/time.h>
#include<sys/types.h>
#include<unistd.h>

#define CR 13
#define LF 10

#define WHITE " \t\n"
#define CRLF "\r\n"


/* codes for commands and things that affect states */
#define UNKNOWN 0
#define HELO 1
#define MAIL 2
#define RCPT 3
#define NOOP 4
#define DATA 5
#define RSET 6
#define QUIT 7
#define VRFY 9
#define EXPN 10
#define EHLO 11

#define SUCCESS 1  /* Success condition. It worked */
#define ERROR 0    /* Error condition. It didn't work, but I'll forgive you */
#define FAILURE -1  /* Failure condition. Hasta la vista, Baby.. */

typedef fd_set smtp_state_set;
typedef fd_set * smtp_state;

/* States we can be in */

#define DISCONNECTED 0    /* We ain't connected */
#define OK_HELO 1   /* I've gotten a tcp connection */ 
#define OK_EHLO 2  /* We've said hello and are talking about the weather */
#define OK_MAIL 3  /* I've gotten a valid from */
#define OK_RCPT 4  /* I've gotten a valid to */
#define SNARF_DATA 5  /* I got a data command ok, and am reading data */

#define test_state(val, state) FD_ISSET(val, state)
#define clear_state(val, state) FD_CLR(val, state)
#define set_state(val, state) FD_SET(val, state)
#define state_changed(val, oldstate, newstate) (!(FD_ISSET(val, oldstate)) && (FD_ISSET(val, newstate)))
#define zap_state(state) FD_ZERO(state);

struct smtp_mbuf {
  unsigned char *data; /* start of data */
  size_t size;         /* length of buffer */ 
  size_t offset;       /* offset of first free byte */
  unsigned char *tail; /* pointer to first free byte */
};  

struct peer_info {
  struct sockaddr_in *my_sa;		/* me */
  char *my_dirty_reverse_name;		/* my hostname */
  char *my_clean_reverse_name;		/* sanitized my hostname */
  struct sockaddr_in *peer_sa;		/* other end */
  char *peer_dirty_reverse_name;	/* hostname of other end (via gethostbyaddr) */
  char *peer_clean_reverse_name;	/* sanitized hostname of other end */
  char *peer_dirty_forward_name;	/* official hostname of other end (via gethostbyname) */
  char *peer_clean_forward_name;	/* sanitized official hostname of other end */
  char *peer_ok_addr;			/* dotted IP addr, if matched both ways */
  char *peer_dirty_ident;		/* ident reply from peer */
  char *peer_clean_ident;		/* sanitized ident reply from peer */
};


extern void reset_state(smtp_state);
extern int sane_state(smtp_state);  
extern struct smtp_mbuf * alloc_smtp_mbuf(size_t size);
extern char * smtp_get_line(struct smtp_mbuf * mbuf, size_t * offset);
extern void flush_smtp_mbuf(struct smtp_mbuf * mbuf, int fd, int len);
extern int grow_smtp_mbuf(struct smtp_mbuf *tiny, size_t bloat);
extern void smtp_exit(int val);
extern unsigned char * cleanitup(const unsigned char *s);
extern unsigned char * smtp_cleanitup(const unsigned char *s);
extern void clean_smtp_mbuf(struct smtp_mbuf *buf, int len);
#if CHECK_ADDRESS
extern int smtpd_addr_check(const char *, struct peer_info *, const char *, const char *, char **);
#endif 

/* 
 * Informational status messages.  SMTP put these in for "human users". 
 * These days, many of these are kind of pointless, The only things that
 * normally should talk smtp don't make syntax errors, and don't pay
 * attention at all to anything beyond the error code. The only people 
 * seeing most of this are gnobs who telnet to port 25 to forge mail :-)
 */

#ifndef VANILLA_MESSAGES
#define VANILLA_MESSAGES 0
#endif

#if !VANILLA_MESSAGES
#define m220msg "SMTP ready, Who are you gonna pretend to be today?" 
#define m221msg "It's been real. Take off Eh!" 
#define m250helook "Is thrilled beyond bladder control to meet"
#define m250ehlook "ESMTP" 
#define m250fromok "(yeah sure, it's probably forged)"
#define m250rcptok "I know them! they'll just *LOVE* to hear from you!"
#define m250gotit "Whew! Done! Was it as good for you as it was for me?"
#define m250msg "So far, So good, (So what!)"
#define m252msg "Sorry, No joy for the VRFY police. (Been reading RFC's have we?)"
#define m354msg "OK, fire away. End with <CRLF>.<CRLF>"

#define m421msg "Sorry, gotta run, I've got my head stuck in the cupboard"
/* this one can get seen in bounces */
#define m452msg "Sorry, I couldn't take anything that big now!, maybe later."
#define m451msg "Sorry, my brain hurts"

#define m500msg "Bloody Amateur! Proper forging of mail requires recognizable SMTP commands!" 
#define m500dummy "Do I really look that stupid?" 
#define m501msg "If you're gonna forge mail get the command parameters right! Kids these days!"
#define m502msg "Sorry, I'm too dumb to understand that one.."
#define m503msg "You can't do that here!"
#define m504msg "Sorry, that option is only available on later models"
#define m550msg "Your mother was a HAMSTER and your father smelt of ELDERBERRIES! "
#define m550frombad "Sorry, I know that one by reputation"
/* this one can get seen in normal bounces */
#define m550tounkn "Doesn't sound like anyone I know."

#define m550tobad "Sorry, I ran that one off a while back."
#define m552msg "Sorry, I could never handle anything that big!"
#define m554msg "This daemon finds you amusing.. Go read the forging FAQ.."
#define m554norcpt "I gotta know who gets this masterpiece of forgery!"
#define m554nofrom "Yeesh! Ya gotta give a FROM when forging mail, that's the whole point!"
#define m521msg "says \"Go away or I shall taunt you a second time you second-hand electric donkey-bottom biters..\""
#else  /* Boring sendmail/RFC821-ish messages */
#define m220msg "Sendmail 4.1/SMI-4.1 ready."
#define m221msg "Closing connection" 
#define m250helook "pleased to meet you,"
#define m250fromok "sender OK"
#define m250rcptok "recipient OK"
#define m250gotit "Message accepted for delivery"
#define m250msg "OK"
#define m252msg "Can not VRFY user"

#define m354msg "OK End with <CRLF>.<CRLF>"

#define m421msg "Service not available, closing transmission channel"
/* this one can get seen in bounces */
#define m452msg "Requested action not taken: insufficient system storage"
#define m451msg "Requested action not taken: local error in processing"

#define m500msg "Syntax Error, command unrecognized" 
#define m500dummy "Syntax Error, command unrecognized" 
#define m501msg "Syntax Error in parameters or arguments"
#define m502msg "Command not implemented"
#define m503msg "Bad sequence of commands"
#define m504msg "Command parameter not implemented"
#define m550msg "Requested action not taken: mailbox unavailable"
#define m550frombad "Requested action not taken: mailbox unavailable"
/* this one can get seen in normal bounces */
#define m550tounkn "Requested action not taken: mailbox unavailable"

#define m550tobad "Requested action not taken: mailbox unavailable"
#define m552msg "Requested mail action aborted: exceeded storage allocation"
#define m554msg "Transaction failed"
#define m554norcpt "Transaction failed"
#define m554nofrom "Transaction failed"
#define m521msg "Doesn't talk SMTP, Sorry"
#endif
