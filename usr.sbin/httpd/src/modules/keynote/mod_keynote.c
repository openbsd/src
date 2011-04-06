/*
 * Copyright (c) 1998, 1999 Niels Provos.  All rights reserved.
 * Copyright (c) 1999, 2000, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 1999, 2000, 2001 Angelos D. Keromytis.  All rights reserved.
 * Copyright (c) 2001 Todd C. Miller.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <mod_ssl.h>
#include <ap_sha1.h>
#include <keynote.h>

MODULE_VAR_EXPORT module keynote_module;

/*
 * This function gets called to create a per-directory configuration
 * record.  This will be called for the "default" server environment, and for
 * each directory for which the parser finds any of our directives applicable.
 * If a directory doesn't have any of our directives involved (i.e., they
 * aren't in the .htaccess file, or a <Location>, <Directory>, or related
 * block), this routine will *not* be called - the configuration for the
 * closest ancestor is used.
 *
 * The return value is a pointer to the created module-specific
 * structure.
 */
static void *
create_keynote_dir_config(pool *p, char *d)
{
    return(ap_make_array(p, 1, sizeof(char **)));
}

/*
 * This function gets called to merge two per-directory configuration
 * records.  This is typically done to cope with things like .htaccess files
 * or <Location> directives for directories that are beneath one for which a
 * configuration record was already created.  The routine has the
 * responsibility of creating a new record and merging the contents of the
 * other two into it appropriately.  If the module doesn't declare a merge
 * routine, the record for the closest ancestor location (that has one) is
 * used exclusively.
 *
 * The routine MUST NOT modify any of its arguments!
 *
 * The return value is a pointer to the created module-specific structure
 * containing the merged values.
 */
static void *
merge_keynote_dir_config(pool *p, void *basev, void *addv)
{
    array_header *base = (array_header *)basev;
    array_header *add = (array_header *)addv;

    return(ap_append_arrays(p, base, add));
}

/*
 * Add an action attribute to the environment of the specified session
 * and log any errors we get, apache style.
 */
static void
add_action_attribute(int sessid, char *name, char *value, request_rec *r)
{
    if (kn_add_action(sessid, name, value, 0) == 0)
	return;

    /* Got an error */
    switch (keynote_errno) {
    case ERROR_SYNTAX:
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
	    r->connection->server,
	    "Invalid action attribute name \"%s\"", name);
	break;
    case ERROR_MEMORY:
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
	    r->connection->server,
	    "Out of memory adding action attribute [%s = \"%s\"]",
	    name, value);
	break;
    case ERROR_NOTFOUND:
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
	    r->connection->server,
	    "Session %d not found while adding action attribute "
	    "[%s = \"%s\"]", sessid, name, value);
	break;
    default:
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
	    r->connection->server, "Unspecified error %d (shouldn't happen)"
	    " while adding action attribute [%s = \"%s\"]", keynote_errno,
	    name, value);
	break;
    }
}

/*
 * Add action attributes to the environment.
 * Currently adds:
 *  app_domain	-> apache
 *  method	-> GET, HEAD, POST, etc.
 *  uri		-> the URI that got us here
 *  protocol	-> access protocol
 *  GMTTimeOfDay	-> GMT time of day, in YYYYmmddHHMMSS format
 *  LocalTimeOfDay	-> Local time of day, in YYYYmmddHHMMSS format
 *  filename	-> last component of URI, or "" if not found
 *  local address
 *  remote address
 *  remote hostname, if known/resolved
 *  local hostname
 *  remote username (RFC 1413)
 *  local username (if authentication was done)
 *  authentication type -> Basic, Digest, etc.
 *
 *  SSL information is set at check_keynote_assertions()
 *
 * XXX IPsec information (if any)
 */
static void
add_action_attributes(int sessid, request_rec *r)
{
    time_t tt;
    char mytimeofday[15];

    add_action_attribute(sessid, "app_domain", "apache", r);
    add_action_attribute(sessid, "method", (char *)r->method, r);
    add_action_attribute(sessid, "protocol", r->protocol, r);
    add_action_attribute(sessid, "filename", r->filename, r);

    tt = time((time_t *) NULL);
    strftime (mytimeofday, 14, "%Y%m%d%H%M%S", gmtime (&tt));
    add_action_attribute(sessid, "GMTTimeOfDay", mytimeofday, r);

    strftime (mytimeofday, 14, "%Y%m%d%H%M%S", localtime (&tt));
    add_action_attribute(sessid, "LocalTimeOfDay", mytimeofday, r);

    add_action_attribute(sessid, "local_address", r->connection->local_ip, r);
    add_action_attribute(sessid, "remote_address", r->connection->remote_ip, r);

    if (r->connection->local_host != NULL)
	add_action_attribute(sessid, "local_hostname",
	    r->connection->local_host, r);

    if (r->connection->remote_host != NULL)
	add_action_attribute(sessid, "remote_hostname",
	    r->connection->remote_host, r);

    if (r->connection->user != NULL)
	add_action_attribute(sessid, "local_username", r->connection->user, r);

    if (r->connection->remote_logname != NULL)
	add_action_attribute(sessid, "remote_username",
	    r->connection->remote_logname, r);

    /* XXX - make the split URI components available too? */
    add_action_attribute(sessid, "uri", r->unparsed_uri, r);
}

static int
keynote_add_authorizer(request_rec *r, int sessid, X509 *cert)
{
    struct keynote_deckey dc;
    EVP_PKEY *key;
    X509_NAME *subject;
    char *akey, *principals[3], *cp;
    int i;

    key = X509_get_pubkey(cert);
    subject = X509_get_subject_name(cert);
    if (!key || (key->type != EVP_PKEY_RSA && key->type != EVP_PKEY_DSA)) {
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, r->connection->server,
	    "Unable to get public key from client's certificate");
	return(1);
    }

    /* Get ascii-encoded version of the key and add as an authorizer. */
    if (key->type == EVP_PKEY_RSA) {
	dc.dec_algorithm = KEYNOTE_ALGORITHM_RSA;
	dc.dec_key = key->pkey.rsa;
    } else {
	dc.dec_algorithm = KEYNOTE_ALGORITHM_DSA;
	dc.dec_key = key->pkey.dsa;
    }
    akey = kn_encode_key(&dc, INTERNAL_ENC_PKCS1, ENCODING_HEX,
	KEYNOTE_PUBLIC_KEY);
    if (akey == NULL) {
	if (keynote_errno == ERROR_MEMORY) {
	    ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		r->connection->server, "Out of memory storing public key");
	    return(-1);
	} else {
	    ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		r->connection->server, "Error storing public key");
	    return(1);
	}
    } else {
	i = 0;
	principals[i++] = ap_pstrcat(r->pool, "rsa-hex:", akey, NULL);
	free(akey);

	/* Generate a "DN:" principal */
	if (subject && (cp = X509_NAME_oneline(subject, NULL, 0)) != NULL) {
	    principals[i++] = ap_pstrcat(r->pool, "DN:", cp, NULL);
	    free(cp);
	}
	principals[i] = NULL;
    }

    for (i = 0; principals[i]; i++) {
	if (kn_add_authorizer(sessid, principals[i]) == -1) {
	    switch (keynote_errno) {
	    case ERROR_MEMORY:
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Out of memory while adding action authorizer %s",
		    principals[i]);
		break;
	    case ERROR_SYNTAX:
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Malformed action authorizer %s", principals[i]);
		break;
	    case ERROR_NOTFOUND:
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server, 
		    "Session %d not found while adding action "
		    "authorizer %s", sessid, principals[i]);
	    default:
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Unspecified error %d (shouldn't happen) "
		    "while adding action authorizer %s",
		    keynote_errno, principals[i]);
		break;
	    }
	}
    }

    return(0);
}

static int
keynote_get_valid_times(request_rec *r, X509 *cert, char *before, size_t beforelen, char **timecomp, char *after, size_t afterlen, char **timecomp2)
{
    ASN1_TIME *tm;
    time_t tt;
    int i;

    if (((tm = X509_get_notBefore(cert)) == NULL) ||
	(tm->type != V_ASN1_UTCTIME && tm->type != V_ASN1_GENERALIZEDTIME)) {
	tt = time((time_t *) NULL);
	strftime(before, 14, "%G%m%d%H%M%S", localtime(&tt));
	*timecomp = "LocalTimeOfDay";
    } else {
	if (tm->data[tm->length - 1] == 'Z') {
	    *timecomp = "GMTTimeOfDay";
	    i = tm->length - 2;
	} else {
	    *timecomp = "LocalTimeOfDay";
	    i = tm->length - 1;
	}

	for (; i >= 0; i--) {
	    if (tm->data[i] < '0' || tm->data[i] > '9') {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Invalid data in certificate's NotValidBefore time field");
	        return(-1);
	    }
	}

	if (tm->type == V_ASN1_UTCTIME) {
	    if (tm->length < 10 || tm->length > 13) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Invalid length of certificate's NotValidBefore time field (%d)",
		    tm->length);
		return(-1);
	    }

	    /* Validity checks.  */
	    if ((tm->data[2] != '0' && tm->data[2] != '1')
		|| (tm->data[2] == '0' && tm->data[3] == '0')
		|| (tm->data[2] == '1' && tm->data[3] > '2')
		|| (tm->data[4] > '3')
		|| (tm->data[4] == '0' && tm->data[5] == '0')
		|| (tm->data[4] == '3' && tm->data[5] > '1')
		|| (tm->data[6] > '2')
		|| (tm->data[6] == '2' && tm->data[7] > '3')
		|| (tm->data[8] > '5')) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Invalid value in certificate's NotValidBefore time field");
		return(-1);
	    }

	    /* Stupid UTC tricks.  */
	    if (tm->data[0] < '5')
		snprintf(before, beforelen, "20%s", tm->data);
	    else
		snprintf(before, beforelen, "19%s", tm->data);
	} else {
	    /* V_ASN1_GENERICTIME */
	    if (tm->length < 12 || tm->length > 15) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Invalid length of certificate's NotValidBefore time field (%d)",
		    tm->length);
	        return(-1);
	    }

	    /* Validity checks.  */
	    if ((tm->data[4] != '0' && tm->data[4] != '1')
		|| (tm->data[4] == '0' && tm->data[5] == '0')
	        || (tm->data[4] == '1' && tm->data[5] > '2')
	        || (tm->data[6] > '3')
	        || (tm->data[6] == '0' && tm->data[7] == '0')
	        || (tm->data[6] == '3' && tm->data[7] > '1')
	        || (tm->data[8] > '2')
	        || (tm->data[8] == '2' && tm->data[9] > '3')
	        || (tm->data[10] > '5')) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Invalid value in certificate's NotValidBefore time field");
	        return(-1);
	    }
	    snprintf(before, beforelen, "%s", tm->data);
	}

	/* Fix missing seconds.  */
	if (tm->length < 12) {
	    before[12] = '0';
	    before[13] = '0';
	}

	/* This will overwrite trailing 'Z'.  */
	before[14] = '\0';
    }

    tm = X509_get_notAfter(cert);
    if (tm == NULL &&
	(tm->type != V_ASN1_UTCTIME && tm->type != V_ASN1_GENERALIZEDTIME)) {
	tt = time(0);
	strftime(after, 14, "%G%m%d%H%M%S", localtime(&tt));
	*timecomp2 = "LocalTimeOfDay";
    } else {
	if (tm->data[tm->length - 1] == 'Z') {
	    *timecomp2 = "GMTTimeOfDay";
	    i = tm->length - 2;
	} else {
	    *timecomp2 = "LocalTimeOfDay";
	    i = tm->length - 1;
	}

	for (; i >= 0; i--) {
	    if (tm->data[i] < '0' || tm->data[i] > '9') {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Invalid data in certificate's NotValidAfter time field");
		return(-1);
	    }
	}

	if (tm->type == V_ASN1_UTCTIME) {
	    if (tm->length < 10 || tm->length > 13) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Invalid length of certificate's NotValidAfter time field (%d)",
		    tm->length);
		return(-1);
	    }

	    /* Validity checks. */
	    if ((tm->data[2] != '0' && tm->data[2] != '1')
	        || (tm->data[2] == '0' && tm->data[3] == '0')
	        || (tm->data[2] == '1' && tm->data[3] > '2')
	        || (tm->data[4] > '3')
	        || (tm->data[4] == '0' && tm->data[5] == '0')
	        || (tm->data[4] == '3' && tm->data[5] > '1')
	        || (tm->data[6] > '2')
	        || (tm->data[6] == '2' && tm->data[7] > '3')
	        || (tm->data[8] > '5')) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Invalid value in certificate's NotValidAfter time field");
	        return(-1);
	    }

	    /* Stupid UTC tricks.  */
	    if (tm->data[0] < '5')
	      snprintf(after, afterlen, "20%s", tm->data);
	    else
	      snprintf(after, afterlen, "19%s", tm->data);
	} else {
	    /* V_ASN1_GENERICTIME */
	    if (tm->length < 12 || tm->length > 15) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Invalid length of certificate's NotValidAfter time field (%d)",
		    tm->length);
		return(-1);
	    }

	    /* Validity checks.  */
	    if ((tm->data[4] != '0' && tm->data[4] != '1')
		|| (tm->data[4] == '0' && tm->data[5] == '0')
		|| (tm->data[4] == '1' && tm->data[5] > '2')
		|| (tm->data[6] > '3')
		|| (tm->data[6] == '0' && tm->data[7] == '0')
		|| (tm->data[6] == '3' && tm->data[7] > '1')
		|| (tm->data[8] > '2')
		|| (tm->data[8] == '2' && tm->data[9] > '3')
		|| (tm->data[10] > '5')) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Invalid value in certificate's NotValidAfter time field");
		return(-1);
	    }
	    snprintf(after, afterlen, "%s", tm->data);
        }

	/* Fix missing seconds.  */
	if (tm->length < 12) {
	    after[12] = '0';
	    after[13] = '0';
	}
	after[14] = '\0'; /* This will overwrite trailing 'Z' */
    }
    return(0);
}

static int
keynote_fake_assertion(request_rec *r, int sessid, X509 *cert, EVP_PKEY *pkey, X509_NAME *name)
{
    struct keynote_deckey dc;
    EVP_PKEY *key;
    X509_NAME *issuer, *subject;
    char *akey, *ikey, *buf, *stext, *itext;
    char before[15], after[15];
    char *timecomp, *timecomp2;
    char *fmt = "Authorizer: \"%s%s\"\nLicensees: \"%s%s\"\n"
	"Conditions: %s >= \"%s\" && %s <= \"%s\";\n";

    if (pkey && pkey->type != EVP_PKEY_RSA && pkey->type != EVP_PKEY_DSA) {
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, r->connection->server,
	    "Issuer's public key is invalid");
	return(1);
    }

    issuer = X509_get_issuer_name(cert);
    subject = X509_get_subject_name(cert);
    if (X509_NAME_cmp(issuer, name) != 0) {
	itext = X509_NAME_oneline(issuer, NULL, 0);
	stext = X509_NAME_oneline(name, NULL, 0);
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, r->connection->server,
	    "Subject doesn't match issuer's certificate: %s != %s", itext, stext);
	free(itext);
	free(stext);
	return(1);
    }

    key = X509_get_pubkey(cert);
    if (!key || (key->type != EVP_PKEY_RSA && key->type != EVP_PKEY_DSA)) {
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, r->connection->server,
	    "Unable to get public key from client's certificate");
	return(1);
    }

    /* Get ascii-encoded version of the public key */
    if (key->type == EVP_PKEY_RSA) {
	dc.dec_algorithm = KEYNOTE_ALGORITHM_RSA;
	dc.dec_key = key->pkey.rsa;
    } else {
	dc.dec_algorithm = KEYNOTE_ALGORITHM_DSA;
	dc.dec_key = key->pkey.dsa;
    }
    akey = kn_encode_key(&dc, INTERNAL_ENC_PKCS1, ENCODING_HEX,
	KEYNOTE_PUBLIC_KEY);
    if (akey == NULL) {
	if (keynote_errno == ERROR_MEMORY) {
	    ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		r->connection->server, "Out of memory storing public key");
	    return(-1);
	} else {
	    ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		r->connection->server, "Error storing public key");
	    return(1);
	}
    }

    /* Get ascii-encoded version of the issuer's public key */
    if (pkey) {
	if (pkey->type == EVP_PKEY_RSA) {
	    dc.dec_algorithm = KEYNOTE_ALGORITHM_RSA;
	    dc.dec_key = pkey->pkey.rsa;
	} else {
	    dc.dec_algorithm = KEYNOTE_ALGORITHM_DSA;
	    dc.dec_key = pkey->pkey.dsa;
	}
	ikey = kn_encode_key(&dc, INTERNAL_ENC_PKCS1, ENCODING_HEX,
	    KEYNOTE_PUBLIC_KEY);
	if (ikey == NULL) {
	    free(akey);
	    if (keynote_errno == ERROR_MEMORY) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server, "Out of memory storing public key");
		return(-1);
	    } else {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server, "Error storing public key");
		return(1);
	    }
	}
    } else
	ikey = NULL;

    if (keynote_get_valid_times(r, cert, before, sizeof(before), &timecomp, after, sizeof(after), &timecomp2) == -1) {
	free(akey);
	if (ikey)
	    free(ikey);
	return(-1);
    }

    itext = X509_NAME_oneline(issuer, NULL, 0);
    stext = X509_NAME_oneline(subject, NULL, 0);

    if (ikey)
	buf = ap_psprintf(r->pool, fmt, "rsa-hex:", ikey, "rsa-hex:", akey,
	    timecomp, before, timecomp2, after);
    else
	buf = ap_psprintf(r->pool, fmt, "DN:", itext, "rsa-hex:", akey,
	    timecomp, before, timecomp2, after);
    if (kn_add_assertion(sessid, buf, strlen(buf), ASSERT_FLAG_LOCAL) == -1) {
	free(stext);
	free(itext);
	free(akey);
	if (ikey)
	    free(ikey);
	goto assert_failed;
    }

    buf = ap_psprintf(r->pool, fmt, "DN:", itext, "DN:", stext,
	timecomp, before, timecomp2, after);
    free(stext);
    free(itext);
    free(akey);
    if (ikey)
	free(ikey);
    if (kn_add_assertion(sessid, buf, strlen(buf), ASSERT_FLAG_LOCAL) != -1)
	return(0);

assert_failed:
    switch (keynote_errno) {
    case ERROR_MEMORY:
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
	    r->connection->server,
	    "Out of memory, trying to add policy assertion %s", buf);
	break;
    case ERROR_SYNTAX:
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
	    r->connection->server, "Syntax error parsing policy assertion %s",
	    buf);
	break;
    default:
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
	    r->connection->server,
	    "Unspecified error %d (shouldn't happen) "
	    "while adding policy assertion %s", keynote_errno, buf);
	break;
    }
    return(-1);
}

static int
check_keynote_assertions(request_rec *r)
{
    array_header *policy_asserts = (array_header *)ap_get_module_config(r->per_dir_config, &keynote_module);
    int sessid, res, i, noclientcert = 0;
    int rval = OK;
    size_t authLen;
    char **assertions;
    SSL_CTX *ctx;
    SSL *ssl;
    X509 *cert, *icert;
    STACK_OF(X509) *certstack;
    STACK_OF(X509_NAME) *CA_list;
    X509_NAME *issuer, *subject;
    static char *return_values[] = { "false", "true" };
    AP_SHA1_CTX context;
    unsigned char digest[SHA_DIGESTSIZE];
    char *pwauth;
    const char *sent_pw;

    /* If there are no KeyNote assertions we have nothing to do. */
    if (policy_asserts->nelts == 0)
	return(DECLINED);

    /* Initialize keynote session.  */
    sessid = kn_init();
    if (sessid == -1) {
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
	    r->connection->server,
	    "keynote init failed: keynote_errno=%d",
	    keynote_errno);
	return(FORBIDDEN);
    }

    /* If this is an SSL session, see if client certs were used. */
    if ((ssl = ap_ctx_get(r->connection->client->ctx, "ssl")) != NULL) {
	ctx = SSL_get_SSL_CTX(ssl);

	/* XXX Initialize SSL-related action attributes */

	/* Get client's certificate or deny them */
	certstack = SSL_get_peer_cert_chain(ssl);
	if ((cert = SSL_get_peer_certificate(ssl)) != NULL) {
	    /* Missing or self-signed, deny them */
	    issuer = X509_get_issuer_name(cert);
	    subject = X509_get_subject_name(cert);
	    if (!issuer || !subject || X509_name_cmp(issuer, subject) == 0) {
		rval = FORBIDDEN;
		goto done;
	    }

	    /* Build a set of fake assertions corresponding to the certificate chain. */
	    for (i = 0; i < sk_X509_num(certstack) && (icert = sk_X509_value(certstack, i)); i++) {
		if (keynote_fake_assertion(r, sessid, cert, X509_get_pubkey(icert), X509_get_subject_name(icert)) == -1) {
		    rval = FORBIDDEN;
		    goto done;
		}
		cert = icert;
	    }

	    /* The issuer of the last cert in the chain should be in the CA list. */
	    issuer = X509_get_issuer_name(cert);
	    CA_list = SSL_CTX_get_client_CA_list(ctx);
	    for (i = 0; i < sk_X509_num(CA_list); i++) {
		subject = sk_X509_NAME_value(CA_list, i);
		if (subject && X509_NAME_cmp(issuer, subject) == 0) {
		    /* An X509_NAME does not contain the public key. */
		    if (keynote_fake_assertion(r, sessid, cert, NULL, subject) == -1) {
			rval = FORBIDDEN;
			goto done;
		    }
		    break;
		}
	    }

	    if (i >= sk_X509_num(CA_list))
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, r->connection->server,
		    "didn't find CA for issuer of last cert in chain");

	    /* Add the user's public key as an authorizer. */
	    if (keynote_add_authorizer(r, sessid, cert) == -1) {
		rval = FORBIDDEN;
		goto done;
	    }
	} else
	    noclientcert = 1; /* No client certificates used. */
    } else
	noclientcert = 1; /* SSL was not used. */

    /* See if we have a passphrase.  */
    if (noclientcert == 1) {
	if ((res = ap_get_basic_auth_pw(r, &sent_pw)) == 0) {
	    /* Add passphrase as the authorizer. */
	    ap_SHA1Init(&context);
	    ap_SHA1Update(&context, sent_pw, strlen(sent_pw));
	    ap_SHA1Final(digest, &context);

	    pwauth = calloc(120, sizeof(char));
	    if (pwauth == NULL) {
		rval = FORBIDDEN;
		goto done;
	    }
	    res = strlen("passphrase-sha1-base64:");
	    strlcpy(pwauth, "passphrase-sha1-base64:", res + 1);
	    ap_base64encode_binary(pwauth + strlen(pwauth), digest,
		sizeof(digest));

	    /* Add passphrase authorizer directly to the session. */
	    kn_add_authorizer(sessid, pwauth);
	    free(pwauth);

	    /* Add username as a principal too. */
	    if (r->connection->user != NULL) {
		int n;

		authLen = strlen(r->connection->user) + 1 + strlen("username:");
		pwauth = calloc(authLen, sizeof(char));
		if (pwauth == NULL) {
		    rval = FORBIDDEN;
		    goto done;
		}

		n = snprintf(pwauth, authLen, "username:%s",
		    r->connection->user);
		if (n == -1 || n >= authLen) {
		    rval = FORBIDDEN;
		    free(pwauth);
		    goto done;
		}

		kn_add_authorizer(sessid, pwauth);
		free(pwauth);
	    }
	} else {
	    kn_add_authorizer(sessid, "");
	}
    }

    /* Setup our environment.  */
    add_action_attributes(sessid, r);

    /* Add our policy assertions (as specified in the config file). */
    assertions = (char **)policy_asserts->elts;
    for (i = 0; i < policy_asserts->nelts; i++) {
	if (kn_add_assertion(sessid, assertions[i],
	    strlen(assertions[i]), ASSERT_FLAG_LOCAL) == -1) {
	    switch (keynote_errno) {
	    case ERROR_MEMORY:
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Out of memory, trying to add policy assertion %s",
		    assertions[i]);
		break;
	    case ERROR_SYNTAX:
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server, "Syntax error "
		    "parsing policy assertion %s", assertions[i]);
		break;
	    default:
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server,
		    "Unspecified error %d (shouldn't happen) "
		    "while adding policy assertion %s",
		    keynote_errno, assertions[i]);
		break;
	    }
	    rval = FORBIDDEN;
	    goto done;
	}
    }

    /* Now do the actual query. */
    switch ((res = kn_do_query(sessid, return_values, 2))) {
    case 0:
	rval = FORBIDDEN;

	/* Log failed assertions */
	for (i = 0; i < policy_asserts->nelts; i++) {
	    if (kn_get_failed(sessid, KEYNOTE_ERROR_SYNTAX, i) != -1) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server, "Assertion failed "
		    "due to a syntax error: %s", assertions[i]);
	    } else if (kn_get_failed(sessid, KEYNOTE_ERROR_SIGNATURE, i) != -1) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server, "Failed to verify "
		    "signature on assertion: %s", assertions[i]);
	    } else if (kn_get_failed(sessid, KEYNOTE_ERROR_ANY, i) != -1) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		    r->connection->server, "Unspecified error "
		    "when processing assertion: %s", assertions[i]);
	    }
	}
	break;
    case 1:
	rval = OK;
	break;
    case -1:
	rval = FORBIDDEN;
	switch (keynote_errno) {
	case ERROR_MEMORY:
	    ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		r->connection->server,
		"Out of memory while performing authorization "
		"query.");
	    break;
	case ERROR_NOTFOUND:
	    ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		r->connection->server,
		"Session %d not found while performing "
		"authorization query.", sessid);
	    break;
	default:
	    ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
		r->connection->server,
		"Unspecified error %d (shouldn't happen) while "
		"performing authorization query.", keynote_errno);
	    break;
	}
default:
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO,
	    r->connection->server, "Weird KeyNote result=%d", res);
	break;
    }

done:
    kn_close(sessid);

    return(rval);
}

/*
 * Take an assertion stored in a file and push it (verbatim) into 
 * the policy_asserts array.
 */
static const char *
store_assertion(cmd_parms *cmd, void *policy_assertsv, char *filename)
{
    int fd, serrno, nelts = 0;
    ssize_t nread;
    struct stat sb;
    char *assert, **asrts;
    array_header *policy_asserts = (array_header *)policy_assertsv;

    filename = ap_server_root_relative(cmd->pool, filename);
    if ((fd = open(filename, O_RDONLY)) == -1)
	return(ap_pstrcat(cmd->pool, "Can't open ", filename, ": ",
	    strerror(errno), NULL));

    if (fstat(fd, &sb) == -1)
	return(ap_pstrcat(cmd->pool, "Can't fstat ", filename, ": ",
	    strerror(errno), NULL));
    
    assert = calloc(sb.st_size + 1, sizeof(char));
    nread = read(fd, assert, sb.st_size);
    serrno = errno;
    close(fd);
    if (nread != sb.st_size) {
	if (nread == -1)
	    return(ap_pstrcat(cmd->pool, "Can't read ", filename, ": ",
		strerror(serrno), NULL));
	else
	    return(ap_pstrcat(cmd->pool, "Short read from", filename, NULL));
    }

    /* Break up into constituent assertions */
    asrts = kn_read_asserts(assert, sb.st_size, &nelts);
    free(assert);

    while (--nelts >= 0) {
	/* Now store the individual assertions in the array */
	 *(char **)ap_push_array(policy_asserts) = ap_pstrdup(cmd->pool, asrts[nelts]);
	 free(asrts[nelts]);
    }

    /* We don't need this anymore */
    if (asrts)
	free(asrts);

    return(NULL);
}

static command_rec keynote_cmds[] = {
    {
	"KeyNotePolicy",		/* directive name */
	store_assertion,		/* config action routine */
	NULL,				/* arg to include in call */
	OR_FILEINFO,			/* where available (FileInfo) */
	ITERATE,			/* call once for each arg */
	"Add a KeyNote policy file"	/* directive description */
    },
    { NULL }
};

module MODULE_VAR_EXPORT keynote_module =
{
    STANDARD_MODULE_STUFF,
    NULL,			/* module initializer */
    create_keynote_dir_config,	/* per-directory config creator */
    merge_keynote_dir_config,	/* dir config merger */
    NULL,			/* server config creator */
    NULL,			/* server config merger */
    keynote_cmds,		/* command table */
    NULL,			/* list of handlers */
    NULL,			/* filename-to-URI translation */
    NULL,			/* check/validate user_id */
    NULL,			/* check user_id is valid *here* */
    check_keynote_assertions,	/* check access by host address */
    NULL,			/* MIME type checker/setter */
    NULL,			/* fixups */
    NULL,			/* logger */
};
