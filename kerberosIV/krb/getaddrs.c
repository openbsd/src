/* $KTH: getaddrs.c,v 1.20 1997/11/09 06:13:32 assar Exp $ */

/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

#include "krb_locl.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/sockio.h>

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif /* MAX */

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif /* MIN */

/*
 * Return number and list of all local adresses.
 */

int
k_get_all_addrs (struct in_addr **l)
{
     int fd;
     char buf[BUFSIZ];
     struct ifreq ifreq;
     struct ifconf ifconf;
     int num, j;
     char *p;

     fd = socket(AF_INET, SOCK_DGRAM, 0);
     if (fd < 0)
	  return -1;

     ifconf.ifc_len = sizeof(buf);
     ifconf.ifc_buf = buf;
     if(ioctl(fd, SIOCGIFCONF, &ifconf) < 0)
	  return -1;
     num = ifconf.ifc_len / sizeof(struct ifreq);
     *l = malloc(num * sizeof(struct in_addr));
     if(*l == NULL) {
	  close (fd);
	  return -1;
     }

     j = 0;
     ifreq.ifr_name[0] = '\0';
     for (p = ifconf.ifc_buf; p < ifconf.ifc_buf + ifconf.ifc_len;) {
          struct ifreq *ifr = (struct ifreq *)p;
	  size_t sz = sizeof(*ifr);
	  sz = MAX(sz, sizeof(ifr->ifr_name) + ifr->ifr_addr.sa_len);

	  if(strncmp(ifreq.ifr_name, ifr->ifr_name, sizeof(ifr->ifr_name))) {
	       if(ioctl(fd, SIOCGIFFLAGS, ifr) < 0) {
		    close (fd);
		    free (*l);
		    return -1;
	       }
	       if (ifr->ifr_flags & IFF_UP) {
		    if(ioctl(fd, SIOCGIFADDR, ifr) < 0) {
			 close (fd);
			 free (*l);
			 return -1;
		    }
		    (*l)[j++] = ((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr;
	       }
	       ifreq = *ifr;
	  }
	  p = p + sz;
     }
     if (j != num)
	  *l = realloc (*l, j * sizeof(struct in_addr));
     close (fd);
     return j;
}
