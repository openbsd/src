/*
 * Copyright (c) 1995, 1996, 1997, 2003 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "rxkad_locl.h"

RCSID("$arla: compat.c,v 1.6 2003/01/11 00:50:30 lha Exp $");

void initialize_rxk_error_table(void);

void
initialize_rxk_error_table(void)
{
  /* A no op, our com_err is not compatible anyways. */
}

uint32_t
life_to_time(uint32_t start, int life_)
{
  return krb_life_to_time(start, life_);
}

int
time_to_life(uint32_t start, uint32_t end)
{
  return krb_time_to_life(start, end);
}

/* function returns:
 *
 * -2 if zero or negative lifetime, or start time is more than now plus time
 * uncertainty plus max ticket lifetime, or if there is an end time, it's
 * before now minus uncertainty, the start time is non-zero, and now minus
 * the start time is greater than the max ticket lifetime plus 24 hours
 *
 * -1 if there is an end time, it's before now minus uncertainty, and the
 * start time is not non-zero or now minus the start time is not greater
 * than the max ticket lifetime plus 24 hours
 *
 * 0 if the times are consistent (not covered by above) but start time is 
 * less than now plus uncertainty
 *
 * 1 if the start time is in the past and the end time is infinity.
 *
 * 2 if the start time is past and the end time is in the future
 * and the lifetime is within the legal limit.
 */
int
tkt_CheckTimes(int32_t begin, int32_t end, int32_t now)
{
    if (end <= begin
	|| begin > now + KTC_TIME_UNCERTAINTY + MAXKTCTICKETLIFETIME
	|| (end
	    && end < now - KTC_TIME_UNCERTAINTY
	    && now - begin > MAXKTCTICKETLIFETIME + MAXKTCTICKETLIFETIME))
	return -2;
    if (end
	&& end < now - KTC_TIME_UNCERTAINTY
	&& (begin == 0 || now - begin <= 2 * MAXKTCTICKETLIFETIME))
	return -1;
    if (begin < now + KTC_TIME_UNCERTAINTY)
	return 0;
    if (begin < now && end == 0)
	return 1;
    if (begin < now
	&& end > now
	&& (end - begin) < MAXKTCTICKETLIFETIME)
	return 2;
    return 2;
}


int
tkt_MakeTicket(char *ticket,
	       int *ticketLen,
	       struct ktc_encryptionKey *key,
	       char *name, char *inst, char *cell,
	       uint32_t start, uint32_t end,
	       struct ktc_encryptionKey *sessionKey,
	       uint32_t host,
	       char *sname, char *sinst)
{
  int code;
  KTEXT_ST tkt;

  /* This routine will probably never be called, only kaserver needs it */

  code = krb_create_ticket(&tkt,
			   0, /*flags*/
			   name, inst, cell,
			   host,
			   sessionKey,
			   krb_time_to_life(start, end), start,
			   sname, sinst,
			   (des_cblock *)key);
  if (code != KSUCCESS)
    return code;

  *ticketLen = tkt.length;
  memcpy(ticket, tkt.dat, tkt.length);
  return code;
}

int
tkt_DecodeTicket (char *asecret,
		  int32_t ticketLen,
		  struct ktc_encryptionKey *key_,
		  char *name,
		  char *inst,
		  char *cell,
		  char *sessionKey,
		  int32_t *host_,
		  int32_t *start_,
		  int32_t *end)
{
    des_cblock *key = (des_cblock *)key_;
    des_key_schedule sched;
    KTEXT_ST txt;
    int ret;
    unsigned char flags;
    int life;
    char sname[ANAME_SZ];
    char sinst[INST_SZ];
    uint32_t *start = (uint32_t *)start_;
    uint32_t *host = (uint32_t *)host_;

    des_key_sched(key, sched);
    txt.length = ticketLen;
    memcpy (txt.dat, asecret, ticketLen);
    ret = decomp_ticket (&txt,
			 &flags,
			 name,
			 inst,
			 cell,
			 host,
			 (unsigned char *)sessionKey,
			 &life,
			 start,
			 sname,
			 sinst,
			 key,
			 sched);
    if (ret == KSUCCESS)
	*end = krb_life_to_time(*start, life);
    return ret;
}
