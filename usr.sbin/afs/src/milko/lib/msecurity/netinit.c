/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

#include <config.h>

#include <sys/types.h>
#include <rx/rx.h>
#include <rx/rx_null.h>

#ifdef KERBEROS
#ifdef HAVE_OPENSSL
#include <openssl/des.h>
#else
#include <des.h>
#endif
#include <krb.h>
#include <rxkad.h>
#endif

#include <ko.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <string.h>
#include <assert.h>
#include <err.h>
#include <netinit.h>

#ifndef HAVE_UNISTD_H
#include <unistd.h>
#endif

RCSID("$arla: netinit.c,v 1.16 2002/06/02 21:12:17 lha Exp $");

/*
 * Network functions
 */

static char *srvtab_filename = NULL;

/*
 * If srvtab == NULL, then default is used.
 */

void
network_kerberos_init (char *srvtab)
{
    if (srvtab) {
	srvtab_filename = strdup(srvtab);
    } else {
	srvtab_filename = MILKO_SYSCONFDIR "/srvtab";
    }
}

#ifdef KERBEROS

static char cell_name[REALM_SZ] = "";
static char realm_name[REALM_SZ] = "";
static des_cblock afskey;

static int
server_get_key(void *appl, int kvno, des_cblock *key)
{
    static int read_key = 0;
    int ret;

    assert (srvtab_filename != NULL);

    if (!read_key) {
	ret = read_service_key("afs", cell_name, realm_name, 
			       0, srvtab_filename,
			       (char *)&afskey);
	if (ret) {
	    /*
	     * Try also afs.realm@REALM
	     */

	    strlcpy(cell_name, realm_name, sizeof(cell_name));
	    strlwr (cell_name);
	    ret = read_service_key("afs", cell_name, realm_name, 
				   0, srvtab_filename,
				   (char *)&afskey);
	    if (ret)
		return ret;
	}
	read_key = 1;
    }

    memcpy (key, &afskey, sizeof (afskey));
    return 0;
}

struct rx_securityClass *
netinit_client_getcred(void)
{
    int k_errno;
    CREDENTIALS c;
    struct rx_securityClass *sec;
    struct timeval tv;

    gettimeofday (&tv, NULL);

    k_errno = krb_get_cred("afs", cell_name, realm_name, &c);
    if (krb_life_to_time (c.issue_date, c.lifetime) < tv.tv_sec - 100) {
	dest_tkt();
	k_errno = 4711;
    }
    if(k_errno != KSUCCESS) {
	k_errno = krb_get_svc_in_tkt("afs", cell_name, realm_name,
				     "afs", cell_name, 0xFF /* XXX */,
				     srvtab_filename);
	if (k_errno == KSUCCESS)
	    k_errno = krb_get_cred("afs", cell_name, realm_name, &c);
    }

    if (k_errno != KSUCCESS)
	return NULL;

    sec = rxkad_NewClientSecurityObject(rxkad_auth,
					&c.session,
					c.kvno,
					c.ticket_st.length,
					c.ticket_st.dat);

    return sec;
}
#endif

#define N_SECURITY_OBJECTS 3

int
network_init (int serviceport, 
	      char *servicename, 
	      int serviceid,
	      int32_t (*request) (struct rx_call *),
	      struct rx_service **service,
	      const char *realm)
{
    struct rx_service *serv;
    int maxsec = 1;
    struct rx_securityClass **secObjs;
    static char krbtkfile[MaxPathLen];

    secObjs = malloc(sizeof(*secObjs) * N_SECURITY_OBJECTS);
    if (secObjs == NULL)
	return errno;

    if (rx_Init(0) != 0) 
	errx(1, "Cant open serverport port") ;
    
    secObjs[0] = rxnull_NewServerSecurityObject();   /* XXX 0 */
    if (secObjs[0] == NULL ) 
	errx(1, "cant create security object") ;

#ifdef KERBEROS
    strlcpy (cell_name, cell_getthiscell(), sizeof (cell_name));

    if (krbtkfile[0] == '\0') { 
	snprintf (krbtkfile, sizeof(krbtkfile),
		  "%sfileserver_%d", TKT_ROOT, (unsigned int)(getpid()*time(0)));
	krb_set_tkt_string (krbtkfile);
    }

    if (realm) {
	strlcpy(realm_name, realm, sizeof(realm_name));
    } else if (*realm_name == '\0') {
	int ret;

	ret = krb_get_lrealm(realm_name, 1);
	if (ret)
	    return ret;
    }

    /* 
     * if there is no diffrence between cell and realm name the afskey
     * is named afs.''@REALM. in the other case afs.CELL@REALM is used 
     */

    if (strcasecmp (realm_name, cell_name) == 0)
	cell_name[0] = '\0';

    maxsec = 3;
    secObjs[1] = NULL;
    secObjs[2] = rxkad_NewServerSecurityObject(rxkad_clear,
					       NULL,
					       server_get_key,
					       NULL);
    if (secObjs[2] == NULL)
	errx(1, "init_network: can't init rxkad server-security-object");
#endif

    serv = rx_NewService (serviceport, serviceid, servicename, 
			  secObjs,  maxsec, 
			  request);
    
    if (serv == NULL) 
	errx(1, "Cant create %s service", servicename);

    if (service)
	*service = serv;
    
    return 0;
}

char *
netinit_getrealm(void) {
    return realm_name;
}
