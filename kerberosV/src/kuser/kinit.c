/*
 * Copyright (c) 1997-2004 Kungliga Tekniska Högskolan
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

#include "kuser_locl.h"
RCSID("$KTH: kinit.c,v 1.121 2005/06/14 00:14:43 lha Exp $");

#ifndef KRB4
#include "krb5-v4compat.h"
#endif

struct krb5_pk_identity;
struct krb5_pk_cert;
struct ContentInfo;
struct _krb5_krb_auth_data;
#include "krb5-private.h"

int forwardable_flag	= -1;
int proxiable_flag	= -1;
int renewable_flag	= -1;
int renew_flag		= 0;
int pac_flag		= -1;
int validate_flag	= 0;
int version_flag	= 0;
int help_flag		= 0;
int addrs_flag		= 1;
struct getarg_strings extra_addresses;
int anonymous_flag	= 0;
char *lifetime 		= NULL;
char *renew_life	= NULL;
char *server		= NULL;
char *cred_cache	= NULL;
char *start_str		= NULL;
struct getarg_strings etype_str;
int use_keytab		= 0;
char *keytab_str	= NULL;
int do_afslog		= -1;
int get_v4_tgt		= -1;
int convert_524		= 0;
int fcache_version;
char *pk_user_id	= NULL;
char *pk_x509_anchors	= NULL;
int pk_use_dh		= -1;

static char *krb4_cc_name;

static struct getargs args[] = {
    /* 
     * used by MIT
     * a: ~A
     * V: verbose
     * F: ~f
     * P: ~p
     * C: v4 cache name?
     * 5: 
     */
    { "524init", 	'4', arg_flag, &get_v4_tgt,
      "obtain version 4 TGT" },

    { "524convert", 	'9', arg_flag, &convert_524,
      "only convert ticket to version 4" },

    { "afslog", 	0  , arg_flag, &do_afslog,
      "obtain afs tokens"  },

    { "cache", 		'c', arg_string, &cred_cache,
      "credentials cache", "cachename" },

    { "forwardable",	'f', arg_flag, &forwardable_flag,
      "get forwardable tickets"},

    { "keytab",         't', arg_string, &keytab_str,
      "keytab to use", "keytabname" },

    { "lifetime",	'l', arg_string, &lifetime,
      "lifetime of tickets", "time"},

    { "proxiable",	'p', arg_flag, &proxiable_flag,
      "get proxiable tickets" },

    { "renew",          'R', arg_flag, &renew_flag,
      "renew TGT" },

    { "renewable",	0,   arg_flag, &renewable_flag,
      "get renewable tickets" },

    { "renewable-life",	'r', arg_string, &renew_life,
      "renewable lifetime of tickets", "time" },

    { "server", 	'S', arg_string, &server,
      "server to get ticket for", "principal" },

    { "start-time",	's', arg_string, &start_str,
      "when ticket gets valid", "time" },

    { "use-keytab",     'k', arg_flag, &use_keytab,
      "get key from keytab" },

    { "validate",	'v', arg_flag, &validate_flag,
      "validate TGT" },

    { "enctypes",	'e', arg_strings, &etype_str,
      "encryption types to use", "enctypes" },

    { "fcache-version", 0,   arg_integer, &fcache_version,
      "file cache version to create" },

    { "addresses",	'A',   arg_negative_flag,	&addrs_flag,
      "request a ticket with no addresses" },

    { "extra-addresses",'a', arg_strings,	&extra_addresses,
      "include these extra addresses", "addresses" },

    { "anonymous",	0,   arg_flag,	&anonymous_flag,
      "request an anonymous ticket" },

    { "request-pac",	0,   arg_flag,	&pac_flag,
      "request a Windows PAC" },

#ifdef PKINIT
    {  "pk-user",	'C',	arg_string,	&pk_user_id,
       "principal's public/private/certificate identifier",
       "id" },

    {  "x509-anchors",	'D',  arg_string, &pk_x509_anchors,
       "directory with CA certificates", "directory" },

    {  "pkinit-use-dh",       0,  arg_flag, &pk_use_dh,
       "make pkinit use DH" },
#endif
    { "version", 	0,   arg_flag, &version_flag },
    { "help",		0,   arg_flag, &help_flag }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "[principal [command]]");
    exit (ret);
}

#ifdef KRB4
/* for when the KDC tells us it's a v4 one, we try to talk that */

static int
key_to_key(const char *user,
	   char *instance,
	   const char *realm,
	   const void *arg,
	   des_cblock *key)
{
    memcpy(key, arg, sizeof(des_cblock));
    return 0;
}

static int
do_v4_fallback (krb5_context context,
		const krb5_principal principal,
		int lifetime,
		int use_srvtab, const char *srvtab_str,
		const char *passwd)
{
    int ret;
    krb_principal princ;
    des_cblock key;
    krb5_error_code kret;

    if (lifetime == 0)
	lifetime = DEFAULT_TKT_LIFE;
    else
	lifetime = krb_time_to_life (0, lifetime);

    kret = krb5_524_conv_principal (context, principal,
				    princ.name,
				    princ.instance,
				    princ.realm);
    if (kret) {
	krb5_warn (context, kret, "krb5_524_conv_principal");
	return 1;
    }

    if (use_srvtab || srvtab_str) {
	if (srvtab_str == NULL)
	    srvtab_str = KEYFILE;

	ret = read_service_key (princ.name, princ.instance, princ.realm,
				0, srvtab_str, (char *)&key);
	if (ret) {
	    warnx ("read_service_key %s: %s", srvtab_str,
		   krb_get_err_text (ret));
	    return 1;
	}
	ret = krb_get_in_tkt (princ.name, princ.instance, princ.realm,
			      KRB_TICKET_GRANTING_TICKET, princ.realm,
			      lifetime, key_to_key, NULL, key);
    } else {
	ret = krb_get_pw_in_tkt(princ.name, princ.instance, princ.realm, 
				KRB_TICKET_GRANTING_TICKET, princ.realm, 
				lifetime, passwd);
    }
    memset (key, 0, sizeof(key));
    if (ret) {
	warnx ("%s", krb_get_err_text(ret));
	return 1;
    }
    if (do_afslog && k_hasafs()) {
	if ((ret = krb_afslog(NULL, NULL)) != 0 && ret != KDC_PR_UNKNOWN) {
	    if(ret > 0)
		warnx ("%s", krb_get_err_text(ret));
	    else
		warnx ("failed to store AFS token");
	}
    }
    return 0;
}


/*
 * the special version of get_default_principal that takes v4 into account
 */

static krb5_error_code
kinit_get_default_principal (krb5_context context,
			     krb5_principal *princ)
{
    krb5_error_code ret;
    krb5_ccache id;
    krb_principal v4_princ;
    int kret;

    ret = krb5_cc_default (context, &id);
    if (ret == 0) {
	ret = krb5_cc_get_principal (context, id, princ);
	krb5_cc_close (context, id);
	if (ret == 0)
	    return 0;
    }

    kret = krb_get_tf_fullname (tkt_string(),
				v4_princ.name,
				v4_princ.instance,
				v4_princ.realm);
    if (kret == KSUCCESS) {
	ret = krb5_425_conv_principal (context,
				       v4_princ.name,
				       v4_princ.instance,
				       v4_princ.realm,
				       princ);
	if (ret == 0)
	    return 0;
    }
    return krb5_get_default_principal (context, princ);
}

#else /* !KRB4 */

static krb5_error_code
kinit_get_default_principal (krb5_context context,
			     krb5_principal *princ)
{
    return krb5_get_default_principal (context, princ);
}

#endif /* !KRB4 */

static krb5_error_code
get_server(krb5_context context,
	   krb5_principal client,
	   const char *server,
	   krb5_principal *princ)
{
    krb5_realm *client_realm;
    if(server)
	return krb5_parse_name(context, server, princ);

    client_realm = krb5_princ_realm (context, client);
    return krb5_make_principal(context, princ, *client_realm,
			       KRB5_TGS_NAME, *client_realm, NULL);
}

static krb5_error_code
do_524init(krb5_context context, krb5_ccache ccache, 
	   krb5_creds *creds, const char *server)
{
    krb5_error_code ret;

    struct credentials c;
    krb5_creds in_creds, *real_creds;

    if(creds != NULL)
	real_creds = creds;
    else {
	krb5_principal client;
	krb5_cc_get_principal(context, ccache, &client);
	memset(&in_creds, 0, sizeof(in_creds));
	ret = get_server(context, client, server, &in_creds.server);
	if(ret) {
	    krb5_free_principal(context, client);
	    return ret;
	}
	in_creds.client = client;
	ret = krb5_get_credentials(context, 0, ccache, &in_creds, &real_creds);
	krb5_free_principal(context, client);
	krb5_free_principal(context, in_creds.server);
	if(ret)
	    return ret;
    }
    ret = krb524_convert_creds_kdc_ccache(context, ccache, real_creds, &c);
    if(ret)
	krb5_warn(context, ret, "converting creds");
    else {
	krb5_error_code tret = _krb5_krb_tf_setup(context, &c, NULL, 0);
	if(tret)
	    krb5_warn(context, tret, "saving v4 creds");
    }

    if(creds == NULL)
	krb5_free_creds(context, real_creds);
    memset(&c, 0, sizeof(c));

    return ret;
}

static int
renew_validate(krb5_context context, 
	       int renew,
	       int validate,
	       krb5_ccache cache, 
	       const char *server,
	       krb5_deltat life)
{
    krb5_error_code ret;
    krb5_creds in, *out;
    krb5_kdc_flags flags;

    memset(&in, 0, sizeof(in));

    ret = krb5_cc_get_principal(context, cache, &in.client);
    if(ret) {
	krb5_warn(context, ret, "krb5_cc_get_principal");
	return ret;
    }
    ret = get_server(context, in.client, server, &in.server);
    if(ret) {
	krb5_warn(context, ret, "get_server");
	goto out;
    }
    flags.i = 0;
    flags.b.renewable         = flags.b.renew = renew;
    flags.b.validate          = validate;
    if (forwardable_flag != -1)
	flags.b.forwardable       = forwardable_flag;
    if (proxiable_flag != -1)
	flags.b.proxiable         = proxiable_flag;
    if (anonymous_flag != -1)
	flags.b.request_anonymous = anonymous_flag;
    if(life)
	in.times.endtime = time(NULL) + life;

    ret = krb5_get_kdc_cred(context,
			    cache,
			    flags,
			    NULL,
			    NULL,
			    &in,
			    &out);
    if(ret) {
	krb5_warn(context, ret, "krb5_get_kdc_cred");
	goto out;
    }
    ret = krb5_cc_initialize(context, cache, in.client);
    if(ret) {
	krb5_free_creds (context, out);
	krb5_warn(context, ret, "krb5_cc_initialize");
	goto out;
    }
    ret = krb5_cc_store_cred(context, cache, out);

    if(ret == 0 && server == NULL) {
	/* only do this if it's a general renew-my-tgt request */
	if(get_v4_tgt)
	    do_524init(context, cache, out, NULL);
	if(do_afslog && k_hasafs())
	    krb5_afslog(context, cache, NULL, NULL);
    }

    krb5_free_creds (context, out);
    if(ret) {
	krb5_warn(context, ret, "krb5_cc_store_cred");
	goto out;
    }
out:
    krb5_free_cred_contents(context, &in);
    return ret;
}

static krb5_error_code
get_new_tickets(krb5_context context, 
		krb5_principal principal,
		krb5_ccache ccache,
		krb5_deltat ticket_life,
		int interactive)
{
    krb5_error_code ret;
    krb5_get_init_creds_opt *opt;
    krb5_addresses no_addrs;
    krb5_creds cred;
    char passwd[256];
    krb5_deltat start_time = 0;
    krb5_deltat renew = 0;
    char *renewstr = NULL;

    memset(&cred, 0, sizeof(cred));

    ret = krb5_get_init_creds_opt_alloc (context, &opt);
    if (ret)
	krb5_err(context, 1, ret, "krb5_get_init_creds_opt_alloc");
    
    krb5_get_init_creds_opt_set_default_flags(context, "kinit", 
					      /* XXX */principal->realm, opt);

    if(forwardable_flag != -1)
	krb5_get_init_creds_opt_set_forwardable (opt, forwardable_flag);
    if(proxiable_flag != -1)
	krb5_get_init_creds_opt_set_proxiable (opt, proxiable_flag);
    if(anonymous_flag != -1)
	krb5_get_init_creds_opt_set_anonymous (opt, anonymous_flag);
    if (pac_flag != -1)
	krb5_get_init_creds_opt_set_pac_request(context, opt, 
						pac_flag ? TRUE : FALSE);
    if (pk_user_id) {
	int flags = 0;
	if (pk_use_dh == 1)
	    flags |= 1;
	ret = krb5_get_init_creds_opt_set_pkinit(context, opt,
						 principal,
						 pk_user_id,
						 pk_x509_anchors,
						 flags,
						 NULL,
						 NULL,
						 NULL);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_get_init_creds_opt_set_pkinit");
    }

    if (!addrs_flag) {
	no_addrs.len = 0;
	no_addrs.val = NULL;

	krb5_get_init_creds_opt_set_address_list (opt, &no_addrs);
    }

    if (renew_life == NULL && renewable_flag)
	renewstr = "1 month";
    if (renew_life)
	renewstr = renew_life;
    if (renewstr) {
	renew = parse_time (renewstr, "s");
	if (renew < 0)
	    errx (1, "unparsable time: %s", renewstr);
	
	krb5_get_init_creds_opt_set_renew_life (opt, renew);
    }

    if(ticket_life != 0)
	krb5_get_init_creds_opt_set_tkt_life (opt, ticket_life);

    if(start_str) {
	int tmp = parse_time (start_str, "s");
	if (tmp < 0)
	    errx (1, "unparsable time: %s", start_str);

	start_time = tmp;
    }

    if(etype_str.num_strings) {
	krb5_enctype *enctype = NULL;
	int i;
	enctype = malloc(etype_str.num_strings * sizeof(*enctype));
	if(enctype == NULL)
	    errx(1, "out of memory");
	for(i = 0; i < etype_str.num_strings; i++) {
	    ret = krb5_string_to_enctype(context, 
					 etype_str.strings[i], 
					 &enctype[i]);
	    if(ret)
		errx(1, "unrecognized enctype: %s", etype_str.strings[i]);
	}
	krb5_get_init_creds_opt_set_etype_list(opt, enctype, 
					       etype_str.num_strings);
    }

    if(use_keytab || keytab_str) {
	krb5_keytab kt;
	if(keytab_str)
	    ret = krb5_kt_resolve(context, keytab_str, &kt);
	else
	    ret = krb5_kt_default(context, &kt);
	if (ret)
	    krb5_err (context, 1, ret, "resolving keytab");
	ret = krb5_get_init_creds_keytab (context,
					  &cred,
					  principal,
					  kt,
					  start_time,
					  server,
					  opt);
	krb5_kt_close(context, kt);
    } else if (pk_user_id) {
	ret = krb5_get_init_creds_password (context,
					    &cred,
					    principal,
					    NULL,
					    krb5_prompter_posix,
					    NULL,
					    start_time,
					    server,
					    opt);
    } else if (!interactive) {
	krb5_warnx(context, "Not interactive, failed to get initial ticket");
	krb5_get_init_creds_opt_free(opt);
	return 0;
    } else {
	char *p, *prompt;

	krb5_unparse_name (context, principal, &p);
	asprintf (&prompt, "%s's Password: ", p);
	free (p);

	if (UI_UTIL_read_pw_string(passwd, sizeof(passwd)-1, prompt, 0)){
	    memset(passwd, 0, sizeof(passwd));
	    exit(1);
	}

	free (prompt);
	
	ret = krb5_get_init_creds_password (context,
					    &cred,
					    principal,
					    passwd,
					    krb5_prompter_posix,
					    NULL,
					    start_time,
					    server,
					    opt);
    }
    krb5_get_init_creds_opt_free(opt);
#ifdef KRB4
    if (ret == KRB5KRB_AP_ERR_V4_REPLY || ret == KRB5_KDC_UNREACH) {
	int exit_val;

	exit_val = do_v4_fallback (context, principal, ticket_life,
				   use_keytab, keytab_str, passwd);
	get_v4_tgt = 0;
	do_afslog  = 0;
	memset(passwd, 0, sizeof(passwd));
	if (exit_val == 0 || ret == KRB5KRB_AP_ERR_V4_REPLY)
	    return exit_val;
    }
#endif
    memset(passwd, 0, sizeof(passwd));

    switch(ret){
    case 0:
	break;
    case KRB5_LIBOS_PWDINTR: /* don't print anything if it was just C-c:ed */
	exit(1);
    case KRB5KRB_AP_ERR_BAD_INTEGRITY:
    case KRB5KRB_AP_ERR_MODIFIED:
	krb5_errx(context, 1, "Password incorrect");
	break;
    default:
	krb5_err(context, 1, ret, "krb5_get_init_creds");
    }

    if(ticket_life != 0) {
	if(abs(cred.times.endtime - cred.times.starttime - ticket_life) > 30) {
	    char life[64];
	    unparse_time_approx(cred.times.endtime - cred.times.starttime, 
				life, sizeof(life));
	    krb5_warnx(context, "NOTICE: ticket lifetime is %s", life);
	}
    }
    if(renew_life) {
	if(abs(cred.times.renew_till - cred.times.starttime - renew) > 30) {
	    char life[64];
	    unparse_time_approx(cred.times.renew_till - cred.times.starttime, 
				life, sizeof(life));
	    krb5_warnx(context, "NOTICE: ticket renewable lifetime is %s", 
		       life);
	}
    }

    ret = krb5_cc_initialize (context, ccache, cred.client);
    if (ret)
	krb5_err (context, 1, ret, "krb5_cc_initialize");
    
    ret = krb5_cc_store_cred (context, ccache, &cred);
    if (ret)
	krb5_err (context, 1, ret, "krb5_cc_store_cred");

    krb5_free_cred_contents (context, &cred);

    return 0;
}

static time_t
ticket_lifetime(krb5_context context, krb5_ccache cache, 
		krb5_principal client, const char *server)
{
    krb5_creds in_cred, *cred;
    krb5_error_code ret;
    time_t timeout;

    memset(&in_cred, 0, sizeof(in_cred));

    ret = krb5_cc_get_principal(context, cache, &in_cred.client);
    if(ret) {
	krb5_warn(context, ret, "krb5_cc_get_principal");
	return 0;
    }
    ret = get_server(context, in_cred.client, server, &in_cred.server);
    if(ret) {
	krb5_free_principal(context, in_cred.client);
	krb5_warn(context, ret, "get_server");
	return 0;
    }

    ret = krb5_get_credentials(context, KRB5_GC_CACHED,
			       cache, &in_cred, &cred);
    krb5_free_principal(context, in_cred.client);
    krb5_free_principal(context, in_cred.server);
    if(ret) {
	krb5_warn(context, ret, "krb5_get_credentials");
	return 0;
    }
    timeout = cred->times.endtime - cred->times.starttime;
    if (timeout < 0)
	timeout = 0;
    krb5_free_creds(context, cred);
    return timeout;
}

struct renew_ctx {
    krb5_context context;
    krb5_ccache  ccache;
    krb5_principal principal;
    krb5_deltat ticket_life;
};

static time_t
renew_func(void *ptr)
{
    struct renew_ctx *ctx = ptr;
    krb5_error_code ret;
    time_t expire;


    ret = renew_validate(ctx->context, renewable_flag, validate_flag,
			     ctx->ccache, server, ctx->ticket_life);
    if (ret)
	get_new_tickets(ctx->context, ctx->principal, 
			ctx->ccache, ctx->ticket_life, 0);

    if(get_v4_tgt || convert_524)
	do_524init(ctx->context, ctx->ccache, NULL, server);
    if(do_afslog && k_hasafs())
	krb5_afslog(ctx->context, ctx->ccache, NULL, NULL);

    expire = ticket_lifetime(ctx->context, ctx->ccache, ctx->principal,
			     server) / 2;
    return expire + 1;
}

int
main (int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_ccache  ccache;
    krb5_principal principal;
    int optind = 0;
    krb5_deltat ticket_life = 0;

    ret = krb5_init_context (&context);
    if (ret == KRB5_CONFIG_BADFORMAT)
	errx (1, "krb5_init_context failed to parse configuration file");
    else if (ret)
	errx(1, "krb5_init_context failed: %d", ret);
  
    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optind))
	usage(1);
    
    if (help_flag)
	usage (0);

    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    argc -= optind;
    argv += optind;

    if (argv[0]) {
	ret = krb5_parse_name (context, argv[0], &principal);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_parse_name");
    } else {
	ret = kinit_get_default_principal (context, &principal);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_get_default_principal");
    }

    if(fcache_version)
	krb5_set_fcache_version(context, fcache_version);

    if(cred_cache) 
	ret = krb5_cc_resolve(context, cred_cache, &ccache);
    else {
	if(argc > 1) {
	    char s[1024];
	    ret = krb5_cc_gen_new(context, &krb5_fcc_ops, &ccache);
	    if(ret)
		krb5_err(context, 1, ret, "creating cred cache");
	    snprintf(s, sizeof(s), "%s:%s",
		     krb5_cc_get_type(context, ccache),
		     krb5_cc_get_name(context, ccache));
	    setenv("KRB5CCNAME", s, 1);
	    if (get_v4_tgt) {
		int fd;
		if (asprintf(&krb4_cc_name, "%s_XXXXXXXXXXX", TKT_ROOT) < 0)
		    krb5_errx(context, 1, "out of memory");
		if((fd = mkstemp(krb4_cc_name)) >= 0) {
		    close(fd);
		    setenv("KRBTKFILE", krb4_cc_name, 1);
		} else {
		    free(krb4_cc_name);
		    krb4_cc_name = NULL;
		}
	    }
	} else
	    ret = krb5_cc_default (context, &ccache);
    }
    if (ret)
	krb5_err (context, 1, ret, "resolving credentials cache");

    if(argc > 1 && k_hasafs ())
	k_setpag();

    if (lifetime) {
	int tmp = parse_time (lifetime, "s");
	if (tmp < 0)
	    errx (1, "unparsable time: %s", lifetime);

	ticket_life = tmp;
    }
    if(renewable_flag == -1)
	/* this seems somewhat pointless, but whatever */
	krb5_appdefault_boolean(context, "kinit",
				krb5_principal_get_realm(context, principal),
				"renewable", FALSE, &renewable_flag);
    if(get_v4_tgt == -1)
	krb5_appdefault_boolean(context, "kinit", 
				krb5_principal_get_realm(context, principal), 
				"krb4_get_tickets", FALSE, &get_v4_tgt);
    if(do_afslog == -1)
	krb5_appdefault_boolean(context, "kinit", 
				krb5_principal_get_realm(context, principal), 
				"afslog", TRUE, &do_afslog);

    if (pk_x509_anchors == NULL)
	krb5_appdefault_string(context, "kinit",
			       krb5_principal_get_realm(context, principal), 
			       "pkinit-anchors", NULL, &pk_x509_anchors);

#ifdef PKINIT
    if(pk_use_dh == -1)
	krb5_appdefault_boolean(context, "kinit", 
				krb5_principal_get_realm(context, principal), 
				"pkinit-use-dh", FALSE, &pk_use_dh);
#endif

    if(!addrs_flag && extra_addresses.num_strings > 0)
	krb5_errx(context, 1, "specifying both extra addresses and "
		  "no addresses makes no sense");
    {
	int i;
	krb5_addresses addresses;
	memset(&addresses, 0, sizeof(addresses));
	for(i = 0; i < extra_addresses.num_strings; i++) {
	    ret = krb5_parse_address(context, extra_addresses.strings[i], 
				     &addresses);
	    if (ret == 0) {
		krb5_add_extra_addresses(context, &addresses);
		krb5_free_addresses(context, &addresses);
	    }
	}
	free_getarg_strings(&extra_addresses);
    }

    if(renew_flag || validate_flag) {
	ret = renew_validate(context, renew_flag, validate_flag, 
			     ccache, server, ticket_life);
	exit(ret != 0);
    }

    if(!convert_524)
	get_new_tickets(context, principal, ccache, ticket_life, 1);

    if(get_v4_tgt || convert_524)
	do_524init(context, ccache, NULL, server);
    if(do_afslog && k_hasafs())
	krb5_afslog(context, ccache, NULL, NULL);
    if(argc > 1) {
	struct renew_ctx ctx;
	time_t timeout;

	timeout = ticket_lifetime(context, ccache, principal, server) / 2;

	ctx.context = context;
	ctx.ccache = ccache;
	ctx.principal = principal;
	ctx.ticket_life = ticket_life;

	ret = simple_execvp_timed(argv[1], argv+1, 
				  renew_func, &ctx, timeout);
#define EX_NOEXEC	126
#define EX_NOTFOUND	127
	if(ret == EX_NOEXEC)
	    krb5_warnx(context, "permission denied: %s", argv[1]);
	else if(ret == EX_NOTFOUND)
	    krb5_warnx(context, "command not found: %s", argv[1]);
	
	krb5_cc_destroy(context, ccache);
	_krb5_krb_dest_tkt(context, krb4_cc_name);
	if(k_hasafs())
	    k_unlog();
    } else {
	krb5_cc_close (context, ccache);
	ret = 0;
    }
    krb5_free_principal(context, principal);
    krb5_free_context (context);
    return ret;
}
