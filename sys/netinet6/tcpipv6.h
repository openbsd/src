/*	$OpenBSD: tcpipv6.h,v 1.8 2003/05/26 05:31:22 itojun Exp $	*/

/*
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 * 
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 	This product includes software developed at the Information
 * 	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

#ifndef _NETINET6_TCPIPV6_H
#define _NETINET6_TCPIPV6_H 1

#include <netinet/ip6.h>
#include <netinet/tcp.h>

struct tcpipv6hdr {
	struct ip6_hdr ti6_i;
	struct tcphdr ti6_t;
};

#define ti6_src		ti6_i.ip6_src
#define ti6_dst		ti6_i.ip6_dst
#define ti6_plen	ti6_i.ip6_plen
#define	ti6_sport	ti6_t.th_sport
#define	ti6_dport	ti6_t.th_dport
#define	ti6_seq		ti6_t.th_seq
#define	ti6_ack		ti6_t.th_ack
#define	ti6_x2		ti6_t.th_x2
#define	ti6_off		ti6_t.th_off
#define	ti6_flags	ti6_t.th_flags
#define	ti6_win		ti6_t.th_win
#define	ti6_sum		ti6_t.th_sum
#define	ti6_urp		ti6_t.th_urp

#endif /* _NETINET6_TCPIPV6_H */
