/*
 * Copyright (c) 2002 - 2003, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "at_locl.h"

RCSID("$arla: afstool.c,v 1.7 2003/03/05 14:55:47 lha Exp $");

#if 0 

#include <krb5/krb5.h>
#include <krb5/krb5_asn1.h>
#include <kerberosIV/krb.h>
#include <kerberosIV/kafs.h>

static int
set5tok(void)
{
    CREDENTIALS c;
    char *key;
    int auth_len;
    char *auth;
    krb5_error_code ret;
    krb5_creds in_creds, *out_creds;
    krb5_context ctx;
    krb5_ccache id;
    int kvno;
    char *realm = "NXS.SE";
    int realm_len = strlen(realm);

    krb5_init_context(&ctx);

    if (!k_hasafs())
	return 0;

    ret = krb5_cc_default (ctx, &id);
    if (ret)
	return ret;

    memset(&in_creds, 0, sizeof(in_creds));
    ret = krb5_build_principal(ctx, &in_creds.server,
			       realm_len, realm, "afs", NULL);
    if(ret)
	return ret;
    ret = krb5_build_principal(ctx, &in_creds.client,
			       realm_len, realm, "lha", NULL);
    if(ret){
	krb5_free_principal(ctx, in_creds.server);
	return ret;
    }
    in_creds.session.keytype = KEYTYPE_DES;
    ret = krb5_get_credentials(ctx, 0, id, &in_creds, &out_creds);
    krb5_free_principal(ctx, in_creds.server);
    krb5_free_principal(ctx, in_creds.client);
    if(ret)
	return ret;

    memset(&c, 0, sizeof(c));

    if (0) {
	kvno = 256;
	auth_len = out_creds->ticket.length;
	auth = out_creds->ticket.data;

    } else {
	Ticket t5;
	int siz;
	char buf[2000];

	kvno = 213;

	ret = decode_Ticket(out_creds->ticket.data, out_creds->ticket.length, 
			    &t5, &siz);
	if (ret)
	    return ret;
	if (t5.tkt_vno != 5)
	    return -1;

	ret = encode_EncryptedData(buf + sizeof(buf) - 1, sizeof(buf), 
				   &t5.enc_part, &siz);
	if (ret)
	    return ret;

	memmove(buf, buf + sizeof(buf) - siz, siz);

	auth_len = siz;
	auth = buf;
	
	free_Ticket(&t5);
    }

    if (out_creds->session.keyvalue.length != 8) {
	printf("a des key that isn't 8 bytes ?\n");
	return 1;
    }

    key = out_creds->session.keyvalue.data;

    printf("auth_len %d\n", auth_len);

    c.kvno = kvno; /* v5 */
    memcpy(c.session, key, sizeof(c.session));
    c.issue_date = out_creds->times.starttime;
    c.lifetime = krb_time_to_life(out_creds->times.starttime,
				  out_creds->times.endtime);

    if (auth_len > MAX_KTXT_LEN) {
	printf("auth len too long %d\n", auth_len);
	return 1;
    }

    c.ticket_st.length = auth_len;
    memcpy(c.ticket_st.dat, auth, auth_len);

    ret = kafs_settoken("nxs.se", getuid(), &c);
    if (ret)
	printf("kafs_settoken failed with %d\n", ret);

    return ret;
}
#endif

/*
 *
 */

struct rx_connection *
cbgetconn(const char *cell, const char *host, const char *portstr, 
	  uint32_t service, int auth)
{
    struct rx_connection *conn;
    int portnum;

    if (portstr) {
	portnum = atoi(portstr);
	if (portnum == 0)
	    errx(1, "invalid port number");
    } else
	portnum = afscallbackport;

    conn = arlalib_getconnbyname(cell, host, portnum, service,
				 auth ? AUTHFLAGS_ANY : AUTHFLAGS_NOAUTH);
    if (conn == NULL)
	errx(1, "getconnbyname");

    return conn;
}

/*
 *
 */

AT_STANDARD_SL_CMDS(afstool,cmds,"");

/*
 * command table
 */

static SL_cmd cmds[] = {
    { "apropos",	afstool_apropos_cmd,	"apropos"},
    { "cachemanager",	cm_cmd,	"query and modify cache manager"},
    { "cm" },
    { "fileserver",	fs_cmd,	"query and modify fileserver"},
    { "fs" },
    { "help",		afstool_help_cmd,	"print help"},
    { "?"},
    { "version",	arlalib_version_cmd,	"print version"},
    { "ubik",		u_cmd,	"query the ubiq data"},
    { NULL }
};

/*
 *
 */

int
main(int argc, char **argv)
{
    Log_method *method;
    
    setprogname(argv[0]);
    tzset();

    setprogname(argv[0]);

    method = log_open (getprogname(), "/dev/stderr:notime");
    if (method == NULL)
	errx (1, "log_open failed");
    cell_init(0, method);
    ports_init();
    rx_Init(0);

#if 0
    ret = set5tok();
    if (ret)
	printf("set5tok failed with %d\n", ret);
#endif

    return afstool_cmd(argc, argv);
}
