/*
 * Copyright (c) 1999-2001, 2003, PADL Software Pty Ltd.
 * Copyright (c) 2004, Andrew Bartlett.
 * Copyright (c) 2003 - 2004, Kungliga Tekniska Högskolan.
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
 * 3. Neither the name of PADL Software  nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "hdb_locl.h"

RCSID("$KTH: hdb-ldap.c,v 1.49 2005/04/18 08:03:54 lha Exp $");

#ifdef OPENLDAP

#include <lber.h>
#include <ldap.h>
#include <sys/un.h>
#include <hex.h>

static krb5_error_code LDAP__connect(krb5_context context, HDB *);
static krb5_error_code LDAP_close(krb5_context context, HDB *);

static krb5_error_code
LDAP_message2entry(krb5_context context, HDB * db, LDAPMessage * msg,
		   hdb_entry * ent);

static const char *default_structural_object = "account";
static char *structural_object;
static krb5_boolean samba_forwardable;

struct hdbldapdb {
    LDAP *h_lp;
    int   h_msgid;
    char *h_base;
    char *h_createbase;
};

#define HDB2LDAP(db) (((struct hdbldapdb *)(db)->hdb_db)->h_lp)
#define HDB2MSGID(db) (((struct hdbldapdb *)(db)->hdb_db)->h_msgid)
#define HDBSETMSGID(db,msgid) \
	do { ((struct hdbldapdb *)(db)->hdb_db)->h_msgid = msgid; } while(0)
#define HDB2BASE(dn) (((struct hdbldapdb *)(db)->hdb_db)->h_base)
#define HDB2CREATE(db) (((struct hdbldapdb *)(db)->hdb_db)->h_createbase)

/*
 *
 */

static char * krb5kdcentry_attrs[] = { 
    "cn",
    "createTimestamp",
    "creatorsName",
    "krb5EncryptionType",
    "krb5KDCFlags",
    "krb5Key",
    "krb5KeyVersionNumber",
    "krb5MaxLife",
    "krb5MaxRenew",
    "krb5PasswordEnd",
    "krb5PrincipalName",
    "krb5PrincipalRealm",
    "krb5ValidEnd",
    "krb5ValidStart",
    "modifiersName",
    "modifyTimestamp",
    "objectClass",
    "sambaAcctFlags",
    "sambaKickoffTime",
    "sambaNTPassword",
    "sambaPwdLastSet",
    "sambaPwdMustChange",
    "uid",
    NULL
};

static char *krb5principal_attrs[] = {
    "cn",
    "createTimestamp",
    "creatorsName",
    "krb5PrincipalName",
    "krb5PrincipalRealm",
    "modifiersName",
    "modifyTimestamp",
    "objectClass",
    "uid",
    NULL
};

static int
LDAP_no_size_limit(krb5_context context, LDAP *lp)
{
    int ret, limit = LDAP_NO_LIMIT;

    ret = ldap_set_option(lp, LDAP_OPT_SIZELIMIT, (const void *)&limit);
    if (ret != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_set_option: %s",
			      ldap_err2string(ret));
	return HDB_ERR_BADVERSION;
    }
    return 0;
}

static int
check_ldap(krb5_context context, HDB *db, int ret)
{
    switch (ret) {
    case LDAP_SUCCESS:
	return 0;
    case LDAP_SERVER_DOWN:
	LDAP_close(context, db);
	return 1;
    default:
	return 1;
    }
}

static krb5_error_code
LDAP__setmod(LDAPMod *** modlist, int modop, const char *attribute,
	     int *pIndex)
{
    int cMods;

    if (*modlist == NULL) {
	*modlist = (LDAPMod **)ber_memcalloc(1, sizeof(LDAPMod *));
	if (*modlist == NULL)
	    return ENOMEM;
    }

    for (cMods = 0; (*modlist)[cMods] != NULL; cMods++) {
	if ((*modlist)[cMods]->mod_op == modop &&
	    strcasecmp((*modlist)[cMods]->mod_type, attribute) == 0) {
	    break;
	}
    }

    *pIndex = cMods;

    if ((*modlist)[cMods] == NULL) {
	LDAPMod *mod;

	*modlist = (LDAPMod **)ber_memrealloc(*modlist,
					      (cMods + 2) * sizeof(LDAPMod *));
	if (*modlist == NULL)
	    return ENOMEM;

	(*modlist)[cMods] = (LDAPMod *)ber_memalloc(sizeof(LDAPMod));
	if ((*modlist)[cMods] == NULL)
	    return ENOMEM;

	mod = (*modlist)[cMods];
	mod->mod_op = modop;
	mod->mod_type = ber_strdup(attribute);
	if (mod->mod_type == NULL) {
	    ber_memfree(mod);
	    (*modlist)[cMods] = NULL;
	    return ENOMEM;
	}

	if (modop & LDAP_MOD_BVALUES) {
	    mod->mod_bvalues = NULL;
	} else {
	    mod->mod_values = NULL;
	}

	(*modlist)[cMods + 1] = NULL;
    }

    return 0;
}

static krb5_error_code
LDAP_addmod_len(LDAPMod *** modlist, int modop, const char *attribute,
		unsigned char *value, size_t len)
{
    krb5_error_code ret;
    int cMods, i = 0;

    ret = LDAP__setmod(modlist, modop | LDAP_MOD_BVALUES, attribute, &cMods);
    if (ret)
	return ret;

    if (value != NULL) {
	struct berval **bv;

	bv = (*modlist)[cMods]->mod_bvalues;
	if (bv != NULL) {
	    for (i = 0; bv[i] != NULL; i++)
		;
	    bv = ber_memrealloc(bv, (i + 2) * sizeof(*bv));
	} else
	    bv = ber_memalloc(2 * sizeof(*bv));
	if (bv == NULL)
	    return ENOMEM;

	(*modlist)[cMods]->mod_bvalues = bv;

	bv[i] = ber_memalloc(sizeof(*bv));;
	if (bv[i] == NULL)
	    return ENOMEM;

	bv[i]->bv_val = value;
	bv[i]->bv_len = len;

	bv[i + 1] = NULL;
    }

    return 0;
}

static krb5_error_code
LDAP_addmod(LDAPMod *** modlist, int modop, const char *attribute,
	    const char *value)
{
    int cMods, i = 0;
    krb5_error_code ret;

    ret = LDAP__setmod(modlist, modop, attribute, &cMods);
    if (ret)
	return ret;

    if (value != NULL) {
	char **bv;

	bv = (*modlist)[cMods]->mod_values;
	if (bv != NULL) {
	    for (i = 0; bv[i] != NULL; i++)
		;
	    bv = ber_memrealloc(bv, (i + 2) * sizeof(*bv));
	} else
	    bv = ber_memalloc(2 * sizeof(*bv));
	if (bv == NULL)
	    return ENOMEM;

	(*modlist)[cMods]->mod_values = bv;

	bv[i] = ber_strdup(value);
	if (bv[i] == NULL)
	    return ENOMEM;

	bv[i + 1] = NULL;
    }

    return 0;
}

static krb5_error_code
LDAP_addmod_generalized_time(LDAPMod *** mods, int modop,
			     const char *attribute, KerberosTime * time)
{
    char buf[22];
    struct tm *tm;

    /* XXX not threadsafe */
    tm = gmtime(time);
    strftime(buf, sizeof(buf), "%Y%m%d%H%M%SZ", tm);

    return LDAP_addmod(mods, modop, attribute, buf);
}

static krb5_error_code
LDAP_addmod_integer(krb5_context context,
		    LDAPMod *** mods, int modop,
		    const char *attribute, unsigned long l)
{
    krb5_error_code ret;
    char *buf;

    ret = asprintf(&buf, "%ld", l);
    if (ret < 0) {
	krb5_set_error_string(context, "asprintf: out of memory:");
	return ret;
    }
    ret = LDAP_addmod(mods, modop, attribute, buf);
    free (buf);
    return ret;
}

static krb5_error_code
LDAP_get_string_value(HDB * db, LDAPMessage * entry,
		      const char *attribute, char **ptr)
{
    char **vals;
    int ret;

    vals = ldap_get_values(HDB2LDAP(db), entry, (char *) attribute);
    if (vals == NULL) {
	*ptr = NULL;
	return HDB_ERR_NOENTRY;
    }

    *ptr = strdup(vals[0]);
    if (*ptr == NULL)
	ret = ENOMEM;
    else
	ret = 0;

    ldap_value_free(vals);

    return ret;
}

static krb5_error_code
LDAP_get_integer_value(HDB * db, LDAPMessage * entry,
		       const char *attribute, int *ptr)
{
    char **vals;

    vals = ldap_get_values(HDB2LDAP(db), entry, (char *) attribute);
    if (vals == NULL)
	return HDB_ERR_NOENTRY;

    *ptr = atoi(vals[0]);
    ldap_value_free(vals);
    return 0;
}

static krb5_error_code
LDAP_get_generalized_time_value(HDB * db, LDAPMessage * entry,
				const char *attribute, KerberosTime * kt)
{
    char *tmp, *gentime;
    struct tm tm;
    int ret;

    *kt = 0;

    ret = LDAP_get_string_value(db, entry, attribute, &gentime);
    if (ret)
	return ret;

    tmp = strptime(gentime, "%Y%m%d%H%M%SZ", &tm);
    if (tmp == NULL) {
	free(gentime);
	return HDB_ERR_NOENTRY;
    }

    free(gentime);

    *kt = timegm(&tm);

    return 0;
}

static krb5_error_code
LDAP_entry2mods(krb5_context context, HDB * db, hdb_entry * ent,
		LDAPMessage * msg, LDAPMod *** pmods)
{
    krb5_error_code ret;
    krb5_boolean is_new_entry;
    char *tmp = NULL;
    LDAPMod **mods = NULL;
    hdb_entry orig;
    unsigned long oflags, nflags;
    int i;

    krb5_boolean is_samba_account = FALSE;
    krb5_boolean is_account = FALSE;
    krb5_boolean is_heimdal_entry = FALSE;
    krb5_boolean is_heimdal_principal = FALSE;

    char **values;

    *pmods = NULL;

    if (msg != NULL) {

	ret = LDAP_message2entry(context, db, msg, &orig);
	if (ret)
	    goto out;

	is_new_entry = FALSE;
	    
	values = ldap_get_values(HDB2LDAP(db), msg, "objectClass");
	if (values) {
	    int num_objectclasses = ldap_count_values(values);
	    for (i=0; i < num_objectclasses; i++) {
		if (strcasecmp(values[i], "sambaSamAccount") == 0) {
		    is_samba_account = TRUE;
		} else if (strcasecmp(values[i], structural_object) == 0) {
		    is_account = TRUE;
		} else if (strcasecmp(values[i], "krb5Principal") == 0) {
		    is_heimdal_principal = TRUE;
		} else if (strcasecmp(values[i], "krb5KDCEntry") == 0) {
		    is_heimdal_entry = TRUE;
		}
	    }
	    ldap_value_free(values);
	}

	/*
	 * If this is just a "account" entry and no other objectclass
	 * is hanging on this entry, its really a new entry.
	 */
	if (is_samba_account == FALSE && is_heimdal_principal == FALSE && 
	    is_heimdal_entry == FALSE) {
	    if (is_account == TRUE) {
		is_new_entry = TRUE;
	    } else {
		ret = HDB_ERR_NOENTRY;
		goto out;
	    }
	}
    } else
	is_new_entry = TRUE;

    if (is_new_entry) {

	/* to make it perfectly obvious we're depending on
	 * orig being intiialized to zero */
	memset(&orig, 0, sizeof(orig));

	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass", "top");
	if (ret)
	    goto out;
	
	/* account is the structural object class */
	if (is_account == FALSE) {
	    ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass", 
			      structural_object);
	    is_account = TRUE;
	    if (ret)
		goto out;
	}

	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass", "krb5Principal");
	is_heimdal_principal = TRUE;
	if (ret)
	    goto out;

	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass", "krb5KDCEntry");
	is_heimdal_entry = TRUE;
	if (ret)
	    goto out;
    }

    if (is_new_entry || 
	krb5_principal_compare(context, ent->principal, orig.principal)
	== FALSE)
    {
	if (is_heimdal_principal || is_heimdal_entry) {

	    ret = krb5_unparse_name(context, ent->principal, &tmp);
	    if (ret)
		goto out;

	    ret = LDAP_addmod(&mods, LDAP_MOD_REPLACE,
			      "krb5PrincipalName", tmp);
	    if (ret) {
		free(tmp);
		goto out;
	    }
	    free(tmp);
	}

	if (is_account || is_samba_account) {
	    ret = krb5_unparse_name_short(context, ent->principal, &tmp);
	    if (ret)
		goto out;
	    ret = LDAP_addmod(&mods, LDAP_MOD_REPLACE, "uid", tmp);
	    if (ret) {
		free(tmp);
		goto out;
	    }
	    free(tmp);
	}
    }

    if (is_heimdal_entry && (ent->kvno != orig.kvno || is_new_entry)) {
	ret = LDAP_addmod_integer(context, &mods, LDAP_MOD_REPLACE,
			    "krb5KeyVersionNumber", 
			    ent->kvno);
	if (ret)
	    goto out;
    }

    if (is_heimdal_entry && ent->valid_start) {
	if (orig.valid_end == NULL
	    || (*(ent->valid_start) != *(orig.valid_start))) {
	    ret = LDAP_addmod_generalized_time(&mods, LDAP_MOD_REPLACE,
					       "krb5ValidStart",
					       ent->valid_start);
	    if (ret)
		goto out;
	}
    }

    if (ent->valid_end) {
 	if (orig.valid_end == NULL || (*(ent->valid_end) != *(orig.valid_end))) {
	    if (is_heimdal_entry) { 
		ret = LDAP_addmod_generalized_time(&mods, LDAP_MOD_REPLACE,
						   "krb5ValidEnd",
						   ent->valid_end);
		if (ret)
		    goto out;
            }
	    if (is_samba_account) {
		ret = LDAP_addmod_integer(context, &mods,  LDAP_MOD_REPLACE,
					  "sambaKickoffTime", 
					  *(ent->valid_end));
		if (ret)
		    goto out;
	    }
   	}
    }

    if (ent->pw_end) {
	if (orig.pw_end == NULL || (*(ent->pw_end) != *(orig.pw_end))) {
	    if (is_heimdal_entry) {
		ret = LDAP_addmod_generalized_time(&mods, LDAP_MOD_REPLACE,
						   "krb5PasswordEnd",
						   ent->pw_end);
		if (ret)
		    goto out;
	    }

	    if (is_samba_account) {
		ret = LDAP_addmod_integer(context, &mods, LDAP_MOD_REPLACE,
					  "sambaPwdMustChange", 
					  *(ent->pw_end));
		if (ret)
		    goto out;
	    }
	}
    }


#if 0 /* we we have last_pw_change */
    if (is_samba_account && ent->last_pw_change) {
	if (orig.last_pw_change == NULL || (*(ent->last_pw_change) != *(orig.last_pw_change))) {
	    ret = LDAP_addmod_integer(context, &mods, LDAP_MOD_REPLACE,
				      "sambaPwdLastSet", 
				      *(ent->last_pw_change));
	    if (ret)
		goto out;
	}
    }
#endif

    if (is_heimdal_entry && ent->max_life) {
	if (orig.max_life == NULL
	    || (*(ent->max_life) != *(orig.max_life))) {

	    ret = LDAP_addmod_integer(context, &mods, LDAP_MOD_REPLACE,
				      "krb5MaxLife", 
				      *(ent->max_life));
	    if (ret)
		goto out;
	}
    }

    if (is_heimdal_entry && ent->max_renew) {
	if (orig.max_renew == NULL
	    || (*(ent->max_renew) != *(orig.max_renew))) {

	    ret = LDAP_addmod_integer(context, &mods, LDAP_MOD_REPLACE,
				      "krb5MaxRenew",
				      *(ent->max_renew));
	    if (ret)
		goto out;
	}
    }

    oflags = HDBFlags2int(orig.flags);
    nflags = HDBFlags2int(ent->flags);

    if (is_heimdal_entry && oflags != nflags) {

	ret = LDAP_addmod_integer(context, &mods, LDAP_MOD_REPLACE,
				  "krb5KDCFlags",
				  nflags);
	if (ret)
	    goto out;
    }

    /* Remove keys if they exists, and then replace keys. */
    if (!is_new_entry && orig.keys.len > 0) {
	values = ldap_get_values(HDB2LDAP(db), msg, "krb5Key");
	if (values) {
	    ldap_value_free(values);

	    ret = LDAP_addmod(&mods, LDAP_MOD_DELETE, "krb5Key", NULL);
	    if (ret)
		goto out;
	}
    }

    for (i = 0; i < ent->keys.len; i++) {

	if (is_samba_account
	    && ent->keys.val[i].key.keytype == ETYPE_ARCFOUR_HMAC_MD5) {
	    char *ntHexPassword;
	    char *nt;
		    
	    /* the key might have been 'sealed', but samba passwords
	       are clear in the directory */
	    ret = hdb_unseal_key(context, db, &ent->keys.val[i]);
	    if (ret)
		goto out;
		    
	    nt = ent->keys.val[i].key.keyvalue.data;
	    /* store in ntPassword, not krb5key */
	    ret = hex_encode(nt, 16, &ntHexPassword);
	    if (ret < 0) {
		krb5_set_error_string(context, "hdb-ldap: failed to "
				      "hex encode key");
		ret = ENOMEM;
		goto out;
	    }
	    ret = LDAP_addmod(&mods, LDAP_MOD_REPLACE, "sambaNTPassword", 
			      ntHexPassword);
	    free(ntHexPassword);
	    if (ret)
		goto out;
		    
	    /* have to kill the LM passwod if it exists */
	    values = ldap_get_values(HDB2LDAP(db), msg, "sambaLMPassword");
	    if (values) {
		ldap_value_free(values);
		ret = LDAP_addmod(&mods, LDAP_MOD_DELETE,
				  "sambaLMPassword", NULL);
		if (ret)
		    goto out;
	    }
		    
	} else if (is_heimdal_entry) {
	    unsigned char *buf;
	    size_t len, buf_size;

	    ASN1_MALLOC_ENCODE(Key, buf, buf_size, &ent->keys.val[i], &len, ret);
	    if (ret)
		goto out;
	    if(buf_size != len)
		krb5_abortx(context, "internal error in ASN.1 encoder");

	    /* addmod_len _owns_ the key, doesn't need to copy it */
	    ret = LDAP_addmod_len(&mods, LDAP_MOD_ADD, "krb5Key", buf, len);
	    if (ret)
		goto out;
	}
    }

    if (ent->etypes) {
	/* clobber and replace encryption types. */
	if (!is_new_entry) {
	    values = ldap_get_values(HDB2LDAP(db), msg, "krb5EncryptionType");
	    if (values) {
		ldap_value_free(values);
		ret = LDAP_addmod(&mods, LDAP_MOD_DELETE, "krb5EncryptionType",
				  NULL);
		if (ret)
		    goto out;
	    }
	}
	for (i = 0; i < ent->etypes->len; i++) {
	    if (is_samba_account && 
		ent->keys.val[i].key.keytype == ETYPE_ARCFOUR_HMAC_MD5)
	    {
		;
	    } else if (is_heimdal_entry) {
		ret = LDAP_addmod_integer(context, &mods, LDAP_MOD_ADD,
					  "krb5EncryptionType",
					  ent->etypes->val[i]);
		if (ret)
		    goto out;
	    }
	}
    }

    /* for clarity */
    ret = 0;

 out:

    if (ret == 0)
	*pmods = mods;
    else if (mods != NULL) {
	ldap_mods_free(mods, 1);
	*pmods = NULL;
    }

    if (msg)
	hdb_free_entry(context, &orig);

    return ret;
}

static krb5_error_code
LDAP_dn2principal(krb5_context context, HDB * db, const char *dn,
		  krb5_principal * principal)
{
    krb5_error_code ret;
    int rc;
    char **values;
    LDAPMessage *res = NULL, *e;

    ret = LDAP_no_size_limit(context, HDB2LDAP(db));
    if (ret)
	goto out;

    rc = ldap_search_s(HDB2LDAP(db), dn, LDAP_SCOPE_SUBTREE,
		       "(objectClass=krb5Principal)", krb5principal_attrs,
		       0, &res);
    if (check_ldap(context, db, rc)) {
	krb5_set_error_string(context, "ldap_search_s: %s",
			      ldap_err2string(rc));
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    e = ldap_first_entry(HDB2LDAP(db), res);
    if (e == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    values = ldap_get_values(HDB2LDAP(db), e, "krb5PrincipalName");
    if (values == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    ret = krb5_parse_name(context, values[0], principal);
    ldap_value_free(values);

  out:
    if (res)
	ldap_msgfree(res);

    return ret;
}

static krb5_error_code
LDAP__lookup_princ(krb5_context context,
		   HDB *db,
		   const char *princname,
		   const char *userid,
		   LDAPMessage **msg)
{
    krb5_error_code ret;
    int rc;
    char *filter = NULL;

    ret = LDAP__connect(context, db);
    if (ret)
	return ret;

    rc = asprintf(&filter,
		  "(&(objectClass=krb5Principal)(krb5PrincipalName=%s))",
		  princname);
    if (rc < 0) {
	krb5_set_error_string(context, "asprintf: out of memory");
	ret = ENOMEM;
	goto out;
    }

    ret = LDAP_no_size_limit(context, HDB2LDAP(db));
    if (ret)
	goto out;

    rc = ldap_search_s(HDB2LDAP(db), HDB2BASE(db), LDAP_SCOPE_SUBTREE, filter, 
		       krb5kdcentry_attrs, 0, msg);
    if (check_ldap(context, db, rc)) {
	krb5_set_error_string(context, "ldap_search_s: %s",
			      ldap_err2string(rc));
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    if (userid && ldap_count_entries(HDB2LDAP(db), *msg) == 0) {
	free(filter);
	filter = NULL;
	ldap_msgfree(*msg);
	*msg = NULL;
	
	rc = asprintf(&filter,
	    "(&(|(objectClass=sambaSamAccount)(objectClass=%s))(uid=%s))",
		      structural_object, userid);
	if (rc < 0) {
	    krb5_set_error_string(context, "asprintf: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	    
	ret = LDAP_no_size_limit(context, HDB2LDAP(db));
	if (ret)
	    goto out;

	rc = ldap_search_s(HDB2LDAP(db), HDB2BASE(db), LDAP_SCOPE_SUBTREE, 
			   filter, krb5kdcentry_attrs, 0, msg);
	if (check_ldap(context, db, rc)) {
	    krb5_set_error_string(context, "ldap_search_s: %s",
				  ldap_err2string(rc));
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}
    }

    ret = 0;

  out:
    if (filter)
	free(filter);

    return ret;
}

static krb5_error_code
LDAP_principal2message(krb5_context context, HDB * db,
		       krb5_principal princ, LDAPMessage ** msg)
{
    char *name, *name_short = NULL;
    krb5_error_code ret;
    krb5_realm *r, *r0;

    *msg = NULL;

    ret = krb5_unparse_name(context, princ, &name);
    if (ret)
	return ret;

    ret = krb5_get_default_realms(context, &r0);
    if(ret) {
	free(name);
	return ret;
    }
    for (r = r0; *r != NULL; r++) {
	if(strcmp(krb5_principal_get_realm(context, princ), *r) == 0) {
	    ret = krb5_unparse_name_short(context, princ, &name_short);
	    if (ret) {
		krb5_free_host_realm(context, r0);
		free(name);
		return ret;
	    }
	    break;
	}
    }
    krb5_free_host_realm(context, r0);

    ret = LDAP__lookup_princ(context, db, name, name_short, msg);
    free(name);
    free(name_short);

    return ret;
}

/*
 * Construct an hdb_entry from a directory entry.
 */
static krb5_error_code
LDAP_message2entry(krb5_context context, HDB * db, LDAPMessage * msg,
		   hdb_entry * ent)
{
    char *unparsed_name = NULL, *dn = NULL, *ntPasswordIN = NULL;
    char *samba_acct_flags = NULL;
    int ret;
    unsigned long tmp;
    struct berval **keys;
    char **values;
    int tmp_time;

    memset(ent, 0, sizeof(*ent));
    ent->flags = int2HDBFlags(0);

    ret = LDAP_get_string_value(db, msg, "krb5PrincipalName", &unparsed_name);
    if (ret == 0) {
	ret = krb5_parse_name(context, unparsed_name, &ent->principal);
	if (ret)
	    goto out;
    } else {
	ret = LDAP_get_string_value(db, msg, "uid",
				    &unparsed_name);
	if (ret == 0) {
	    ret = krb5_parse_name(context, unparsed_name, &ent->principal);
	    if (ret)
		goto out;
	} else {
	    krb5_set_error_string(context, "hdb-ldap: ldap entry missing"
				  "principal name");
	    return HDB_ERR_NOENTRY;
	}
    }

    ret = LDAP_get_integer_value(db, msg, "krb5KeyVersionNumber",
				 &ent->kvno);
    if (ret)
	ent->kvno = 0;

    keys = ldap_get_values_len(HDB2LDAP(db), msg, "krb5Key");
    if (keys != NULL) {
	int i;
	size_t l;

	ent->keys.len = ldap_count_values_len(keys);
	ent->keys.val = (Key *) calloc(ent->keys.len, sizeof(Key));
	if (ent->keys.val == NULL) {
	    krb5_set_error_string(context, "calloc: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	for (i = 0; i < ent->keys.len; i++) {
	    decode_Key((unsigned char *) keys[i]->bv_val,
		       (size_t) keys[i]->bv_len, &ent->keys.val[i], &l);
	}
	ber_bvecfree(keys);
    } else {
#if 1
	/*
	 * This violates the ASN1 but it allows a principal to
	 * be related to a general directory entry without creating
	 * the keys. Hopefully it's OK.
	 */
	ent->keys.len = 0;
	ent->keys.val = NULL;
#else
	ret = HDB_ERR_NOENTRY;
	goto out;
#endif
    }

    values = ldap_get_values(HDB2LDAP(db), msg, "krb5EncryptionType");
    if (values != NULL) {
	int i;

	ent->etypes = malloc(sizeof(*(ent->etypes)));
	if (ent->etypes == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	ent->etypes->len = ldap_count_values(values);
	ent->etypes->val = calloc(ent->etypes->len, sizeof(int));
	if (ent->etypes->val == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	for (i = 0; i < ent->etypes->len; i++) {
	    ent->etypes->val[i] = atoi(values[i]);
	}
	ldap_value_free(values);
    }

    /* manually construct the NT (type 23) key */
    ret = LDAP_get_string_value(db, msg, "sambaNTPassword", &ntPasswordIN);
    if (ret == 0) {
	int *etypes;
	Key *keys;

	keys = realloc(ent->keys.val,
		       (ent->keys.len + 1) * sizeof(ent->keys.val[0]));
	if (keys == NULL) {
	    free(ntPasswordIN);
	    krb5_set_error_string(context, "malloc: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	ent->keys.val = keys;
	memset(&ent->keys.val[ent->keys.len], 0, sizeof(Key));
	ent->keys.val[ent->keys.len].key.keytype = ETYPE_ARCFOUR_HMAC_MD5;
	ret = krb5_data_alloc (&ent->keys.val[ent->keys.len].key.keyvalue, 16);
	if (ret) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    free(ntPasswordIN);
	    ret = ENOMEM;
	    goto out;
	}
	ret = hex_decode(ntPasswordIN,
			 ent->keys.val[ent->keys.len].key.keyvalue.data, 16);
	ent->keys.len++;

	if (ent->etypes == NULL) {
	    ent->etypes = malloc(sizeof(*(ent->etypes)));
	    if (ent->etypes == NULL) {
		krb5_set_error_string(context, "malloc: out of memory");
		ret = ENOMEM;
		goto out;
	    }
	    ent->etypes->val = NULL;
	    ent->etypes->len = 0;
	}

	etypes = realloc(ent->etypes->val, 
			 (ent->etypes->len + 1) * sizeof(ent->etypes->val[0]));
	if (etypes == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    ret = ENOMEM;
	    goto out;			    
	}
	ent->etypes->val = etypes;
	ent->etypes->val[ent->etypes->len] = ETYPE_ARCFOUR_HMAC_MD5;
	ent->etypes->len++;
    }

    ret = LDAP_get_generalized_time_value(db, msg, "createTimestamp",
					  &ent->created_by.time);
    if (ret)
	ent->created_by.time = time(NULL);

    ent->created_by.principal = NULL;

    ret = LDAP_get_string_value(db, msg, "creatorsName", &dn);
    if (ret == 0) {
	if (LDAP_dn2principal(context, db, dn, &ent->created_by.principal)
	    != 0) {
	    ent->created_by.principal = NULL;
	}
	free(dn);
    }

    ent->modified_by = (Event *) malloc(sizeof(Event));
    if (ent->modified_by == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret = LDAP_get_generalized_time_value(db, msg, "modifyTimestamp",
					  &ent->modified_by->time);
    if (ret == 0) {
	ret = LDAP_get_string_value(db, msg, "modifiersName", &dn);
	if (LDAP_dn2principal(context, db, dn, &ent->modified_by->principal))
	    ent->modified_by->principal = NULL;
	free(dn);
    } else {
	free(ent->modified_by);
	ent->modified_by = NULL;
    }

    ent->valid_start = malloc(sizeof(*ent->valid_start));
    if (ent->valid_start == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret = LDAP_get_generalized_time_value(db, msg, "krb5ValidStart",
					  ent->valid_start);
    if (ret) {
	/* OPTIONAL */
	free(ent->valid_start);
	ent->valid_start = NULL;
    }
    
    ent->valid_end = malloc(sizeof(*ent->valid_end));
    if (ent->valid_end == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret = LDAP_get_generalized_time_value(db, msg, "krb5ValidEnd",
					  ent->valid_end);
    if (ret) {
	/* OPTIONAL */
	free(ent->valid_end);
	ent->valid_end = NULL;
    }

    ret = LDAP_get_integer_value(db, msg, "sambaKickoffTime", &tmp_time);
    if (ret == 0) {
 	if (ent->valid_end == NULL) {
 	    ent->valid_end = malloc(sizeof(*ent->valid_end));
 	    if (ent->valid_end == NULL) {
 		krb5_set_error_string(context, "malloc: out of memory");
 		ret = ENOMEM;
 		goto out;
 	    }
 	}
 	*ent->valid_end = tmp_time;
    }

    ent->pw_end = malloc(sizeof(*ent->pw_end));
    if (ent->pw_end == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret = LDAP_get_generalized_time_value(db, msg, "krb5PasswordEnd",
					  ent->pw_end);
    if (ret) {
	/* OPTIONAL */
	free(ent->pw_end);
	ent->pw_end = NULL;
    }

    ret = LDAP_get_integer_value(db, msg, "sambaPwdMustChange", &tmp_time);
    if (ret == 0) {
	if (ent->pw_end == NULL) {
	    ent->pw_end = malloc(sizeof(*ent->pw_end));
	    if (ent->pw_end == NULL) {
		krb5_set_error_string(context, "malloc: out of memory");
		ret = ENOMEM;
		goto out;
	    }
	}
	*ent->pw_end = tmp_time;
    }

#if 0 /* we we have last_pw_change */
    ent->last_pw_change = malloc(sizeof(*ent->last_pw_change));
    if (ent->last_pw_change == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret = LDAP_get_integer_value(db, msg, "sambaPwdLastSet",
				 &tmp_time);
    if (ret) {
	/* OPTIONAL */
	free(ent->last_pw_change);
	ent->last_pw_change = NULL;
    } else
	*ent->last_pw_change = tmp_time;
#endif

    ent->max_life = malloc(sizeof(*ent->max_life));
    if (ent->max_life == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret = LDAP_get_integer_value(db, msg, "krb5MaxLife", ent->max_life);
    if (ret) {
	free(ent->max_life);
	ent->max_life = NULL;
    }

    ent->max_renew = malloc(sizeof(*ent->max_renew));
    if (ent->max_renew == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret = LDAP_get_integer_value(db, msg, "krb5MaxRenew", ent->max_renew);
    if (ret) {
	free(ent->max_renew);
	ent->max_renew = NULL;
    }

    values = ldap_get_values(HDB2LDAP(db), msg, "krb5KDCFlags");
    if (values != NULL) {
	tmp = strtoul(values[0], (char **) NULL, 10);
	if (tmp == ULONG_MAX && errno == ERANGE) {
	    krb5_set_error_string(context, "strtoul: could not convert flag");
	    ret = ERANGE;
	    goto out;
	}
    } else {
	tmp = 0;
    }

    ent->flags = int2HDBFlags(tmp);

    /* Try and find Samba flags to put into the mix */
    ret = LDAP_get_string_value(db, msg, "sambaAcctFlags", &samba_acct_flags);
    if (ret == 0) {
	/* parse the [UXW...] string:
	       
	'N'    No password	 
	'D'    Disabled	 
	'H'    Homedir required	 
	'T'    Temp account.	 
	'U'    User account (normal) 	 
	'M'    MNS logon user account - what is this ? 	 
	'W'    Workstation account	 
	'S'    Server account 	 
	'L'    Locked account	 
	'X'    No Xpiry on password 	 
	'I'    Interdomain trust account	 
	    
	*/	 
	    
	int i;
	int flags_len = strlen(samba_acct_flags);

	if (flags_len < 2)
	    goto out2;

	if (samba_acct_flags[0] != '[' 
	    || samba_acct_flags[flags_len - 1] != ']') 
	    goto out2;

	/* Allow forwarding */
	if (samba_forwardable)
	    ent->flags.forwardable = TRUE;

	for (i=0; i < flags_len; i++) {
	    switch (samba_acct_flags[i]) {
	    case ' ':
	    case '[':
	    case ']':
		break;
	    case 'N':
		/* how to handle no password in kerberos? */
		break;
	    case 'D':
		ent->flags.invalid = TRUE;
		break;
	    case 'H':
		break;
	    case 'T':
		/* temp duplicate */
		ent->flags.invalid = TRUE;
		break;
	    case 'U':
		ent->flags.client = TRUE;
		break;
	    case 'M':
		break;
	    case 'W':
	    case 'S':
		ent->flags.server = TRUE;
		ent->flags.client = TRUE;
		break;
	    case 'L':
		ent->flags.invalid = TRUE;
		break;
	    case 'X':
		if (ent->pw_end) {
		    free(ent->pw_end);
		    ent->pw_end = NULL;
		}
		break;
	    case 'I':
		ent->flags.server = TRUE;
		ent->flags.client = TRUE;
		break;
	    }
	}
    out2:
	free(samba_acct_flags);
    }

    ret = 0;

 out:
    if (unparsed_name)
	free(unparsed_name);

    if (ret)
	hdb_free_entry(context, ent);

    return ret;
}

static krb5_error_code
LDAP_close(krb5_context context, HDB * db)
{
    if (HDB2LDAP(db)) {
	ldap_unbind_ext(HDB2LDAP(db), NULL, NULL);
	((struct hdbldapdb *)db->hdb_db)->h_lp = NULL;
    }
    
    return 0;
}

static krb5_error_code
LDAP_lock(krb5_context context, HDB * db, int operation)
{
    return 0;
}

static krb5_error_code
LDAP_unlock(krb5_context context, HDB * db)
{
    return 0;
}

static krb5_error_code
LDAP_seq(krb5_context context, HDB * db, unsigned flags, hdb_entry * entry)
{
    int msgid, rc, parserc;
    krb5_error_code ret;
    LDAPMessage *e;

    msgid = HDB2MSGID(db);
    if (msgid < 0)
	return HDB_ERR_NOENTRY;

    do {
	rc = ldap_result(HDB2LDAP(db), msgid, LDAP_MSG_ONE, NULL, &e);
	switch (rc) {
	case LDAP_RES_SEARCH_REFERENCE:
	    ldap_msgfree(e);
	    ret = 0;
	    break;
	case LDAP_RES_SEARCH_ENTRY:
	    /* We have an entry. Parse it. */
	    ret = LDAP_message2entry(context, db, e, entry);
	    ldap_msgfree(e);
	    break;
	case LDAP_RES_SEARCH_RESULT:
	    /* We're probably at the end of the results. If not, abandon. */
	    parserc =
		ldap_parse_result(HDB2LDAP(db), e, NULL, NULL, NULL,
				  NULL, NULL, 1);
	    if (parserc != LDAP_SUCCESS
		&& parserc != LDAP_MORE_RESULTS_TO_RETURN) {
	        krb5_set_error_string(context, "ldap_parse_result: %s",
				      ldap_err2string(parserc));
		ldap_abandon(HDB2LDAP(db), msgid);
	    }
	    ret = HDB_ERR_NOENTRY;
	    HDBSETMSGID(db, -1);
	    break;
	case LDAP_SERVER_DOWN:
	    ldap_msgfree(e);
	    LDAP_close(context, db);
	    HDBSETMSGID(db, -1);
	    ret = ENETDOWN;
	    break;
	default:
	    /* Some unspecified error (timeout?). Abandon. */
	    ldap_msgfree(e);
	    ldap_abandon(HDB2LDAP(db), msgid);
	    ret = HDB_ERR_NOENTRY;
	    HDBSETMSGID(db, -1);
	    break;
	}
    } while (rc == LDAP_RES_SEARCH_REFERENCE);

    if (ret == 0) {
	if (db->hdb_master_key_set && (flags & HDB_F_DECRYPT)) {
	    ret = hdb_unseal_keys(context, db, entry);
	    if (ret)
		hdb_free_entry(context,entry);
	}
    }

    return ret;
}

static krb5_error_code
LDAP_firstkey(krb5_context context, HDB *db, unsigned flags,
	      hdb_entry *entry)
{
    krb5_error_code ret;
    int msgid;

    ret = LDAP__connect(context, db);
    if (ret)
	return ret;

    ret = LDAP_no_size_limit(context, HDB2LDAP(db));
    if (ret)
	return ret;

    msgid = ldap_search(HDB2LDAP(db), HDB2BASE(db),
			LDAP_SCOPE_SUBTREE,
			"(|(objectClass=krb5Principal)(objectClass=sambaSamAccount))",
			krb5kdcentry_attrs, 0);
    if (msgid < 0)
	return HDB_ERR_NOENTRY;

    HDBSETMSGID(db, msgid);

    return LDAP_seq(context, db, flags, entry);
}

static krb5_error_code
LDAP_nextkey(krb5_context context, HDB * db, unsigned flags,
	     hdb_entry * entry)
{
    return LDAP_seq(context, db, flags, entry);
}

static krb5_error_code
LDAP_rename(krb5_context context, HDB * db, const char *new_name)
{
    return HDB_ERR_DB_INUSE;
}

static krb5_error_code
LDAP__connect(krb5_context context, HDB * db)
{
    int rc, version = LDAP_VERSION3;
    /*
     * Empty credentials to do a SASL bind with LDAP. Note that empty
     * different from NULL credentials. If you provide NULL
     * credentials instead of empty credentials you will get a SASL
     * bind in progress message.
     */
    struct berval bv = { 0, "" };

    if (HDB2LDAP(db)) {
	/* connection has been opened. ping server. */
	struct sockaddr_un addr;
	socklen_t len = sizeof(addr);
	int sd;

	if (ldap_get_option(HDB2LDAP(db), LDAP_OPT_DESC, &sd) == 0 &&
	    getpeername(sd, (struct sockaddr *) &addr, &len) < 0) {
	    /* the other end has died. reopen. */
	    LDAP_close(context, db);
	}
    }

    if (HDB2LDAP(db) != NULL) /* server is UP */
	return 0;

    rc = ldap_initialize(&((struct hdbldapdb *)db->hdb_db)->h_lp, "ldapi:///");
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_initialize: %s", 
			      ldap_err2string(rc));
	return HDB_ERR_NOENTRY;
    }

    rc = ldap_set_option(HDB2LDAP(db), LDAP_OPT_PROTOCOL_VERSION,
			 (const void *)&version);
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_set_option: %s",
			      ldap_err2string(rc));
	LDAP_close(context, db);
	return HDB_ERR_BADVERSION;
    }

    rc = ldap_sasl_bind_s(HDB2LDAP(db), NULL, "EXTERNAL", &bv,
			  NULL, NULL, NULL);
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_sasl_bind_s: %s",
			      ldap_err2string(rc));
	LDAP_close(context, db);
	return HDB_ERR_BADVERSION;
    }

    return 0;
}

static krb5_error_code
LDAP_open(krb5_context context, HDB * db, int flags, mode_t mode)
{
    /* Not the right place for this. */
#ifdef HAVE_SIGACTION
    struct sigaction sa;

    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGPIPE, &sa, NULL);
#else
    signal(SIGPIPE, SIG_IGN);
#endif /* HAVE_SIGACTION */

    return LDAP__connect(context, db);
}

static krb5_error_code
LDAP_fetch(krb5_context context, HDB * db, unsigned flags,
	   hdb_entry * entry)
{
    LDAPMessage *msg, *e;
    krb5_error_code ret;

    ret = LDAP_principal2message(context, db, entry->principal, &msg);
    if (ret)
	return ret;

    e = ldap_first_entry(HDB2LDAP(db), msg);
    if (e == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    ret = LDAP_message2entry(context, db, e, entry);
    if (ret == 0) {
	if (db->hdb_master_key_set && (flags & HDB_F_DECRYPT)) {
	    ret = hdb_unseal_keys(context, db, entry);
	    if (ret)
		hdb_free_entry(context,entry);
	}
    }

  out:
    ldap_msgfree(msg);

    return ret;
}

static krb5_error_code
LDAP_store(krb5_context context, HDB * db, unsigned flags,
	   hdb_entry * entry)
{
    LDAPMod **mods = NULL;
    krb5_error_code ret;
    const char *errfn;
    int rc;
    LDAPMessage *msg = NULL, *e = NULL;
    char *dn = NULL, *name = NULL;

    ret = LDAP_principal2message(context, db, entry->principal, &msg);
    if (ret == 0)
	e = ldap_first_entry(HDB2LDAP(db), msg);

    ret = krb5_unparse_name(context, entry->principal, &name);
    if (ret) {
	free(name);
	return ret;
    }

    ret = hdb_seal_keys(context, db, entry);
    if (ret)
	goto out;

    /* turn new entry into LDAPMod array */
    ret = LDAP_entry2mods(context, db, entry, e, &mods);
    if (ret)
	goto out;

    if (e == NULL) {
	ret = asprintf(&dn, "krb5PrincipalName=%s,%s", name, HDB2CREATE(db));
	if (ret < 0) {
	    krb5_set_error_string(context, "asprintf: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
    } else if (flags & HDB_F_REPLACE) {
	/* Entry exists, and we're allowed to replace it. */
	dn = ldap_get_dn(HDB2LDAP(db), e);
    } else {
	/* Entry exists, but we're not allowed to replace it. Bail. */
	ret = HDB_ERR_EXISTS;
	goto out;
    }

    /* write entry into directory */
    if (e == NULL) {
	/* didn't exist before */
	rc = ldap_add_s(HDB2LDAP(db), dn, mods);
	errfn = "ldap_add_s";
    } else {
	/* already existed, send deltas only */
	rc = ldap_modify_s(HDB2LDAP(db), dn, mods);
	errfn = "ldap_modify_s";
    }

    if (check_ldap(context, db, rc)) {
	char *ld_error = NULL;
	ldap_get_option(HDB2LDAP(db), LDAP_OPT_ERROR_STRING,
			&ld_error);
	krb5_set_error_string(context, "%s: %s (dn=%s) %s: %s", 
			      errfn, name, dn, ldap_err2string(rc), ld_error);
	ret = HDB_ERR_CANT_LOCK_DB;
    } else
	ret = 0;

  out:
    /* free stuff */
    if (dn)
	free(dn);
    if (msg)
	ldap_msgfree(msg);
    if (mods)
	ldap_mods_free(mods, 1);
    if (name)
	free(name);

    return ret;
}

static krb5_error_code
LDAP_remove(krb5_context context, HDB * db, hdb_entry * entry)
{
    krb5_error_code ret;
    LDAPMessage *msg, *e;
    char *dn = NULL;
    int rc, limit = LDAP_NO_LIMIT;

    ret = LDAP_principal2message(context, db, entry->principal, &msg);
    if (ret)
	goto out;

    e = ldap_first_entry(HDB2LDAP(db), msg);
    if (e == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    dn = ldap_get_dn(HDB2LDAP(db), e);
    if (dn == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    rc = ldap_set_option(HDB2LDAP(db), LDAP_OPT_SIZELIMIT, (const void *)&limit);
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_set_option: %s",
			      ldap_err2string(rc));
	ret = HDB_ERR_BADVERSION;
	goto out;
    }

    rc = ldap_delete_s(HDB2LDAP(db), dn);
    if (check_ldap(context, db, rc)) {
	krb5_set_error_string(context, "ldap_delete_s: %s", 
			      ldap_err2string(rc));
	ret = HDB_ERR_CANT_LOCK_DB;
    } else
	ret = 0;

  out:
    if (dn != NULL)
	free(dn);
    if (msg != NULL)
	ldap_msgfree(msg);

    return ret;
}

static krb5_error_code
LDAP_destroy(krb5_context context, HDB * db)
{
    krb5_error_code ret;

    LDAP_close(context, db);

    ret = hdb_clear_master_key(context, db);
    if (HDB2BASE(db))
	free(HDB2BASE(db));
    if (HDB2CREATE(db))
	free(HDB2CREATE(db));
    if (db->hdb_name)
	free(db->hdb_name);
    free(db->hdb_db);
    free(db);

    return ret;
}

krb5_error_code
hdb_ldap_create(krb5_context context, HDB ** db, const char *arg)
{
    struct hdbldapdb *h;
    const char *create_base = NULL;

    if (arg == NULL && arg[0] == '\0') {
	krb5_set_error_string(context, "ldap search base not configured");
	return ENOMEM; /* XXX */
    }

    if (structural_object == NULL) {
	const char *p;

	p = krb5_config_get_string(context, NULL, "kdc", 
				   "hdb-ldap-structural-object", NULL);
	if (p == NULL)
	    p = default_structural_object;
	structural_object = strdup(p);
	if (structural_object == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
    }

    samba_forwardable = 
	krb5_config_get_bool_default(context, NULL, TRUE,
				     "kdc", "hdb-samba-forwardable", NULL);

    *db = malloc(sizeof(**db));
    if (*db == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    memset(*db, 0, sizeof(**db));

    h = malloc(sizeof(*h));
    if (h == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	free(*db);
	*db = NULL;
	return ENOMEM;
    }
    memset(h, 0, sizeof(*h));

    asprintf(&(*db)->hdb_name, "ldap:%s", arg);

    (*db)->hdb_db = h;
    h->h_base = strdup(arg);
    if (h->h_base == NULL) {
	LDAP_destroy(context, *db);
	krb5_set_error_string(context, "strdup: out of memory");
	*db = NULL;
	return ENOMEM;
    }

    create_base = krb5_config_get_string(context, NULL, "kdc", 
					 "hdb-ldap-create-base", NULL);
    if (create_base == NULL)
	create_base = h->h_base;

    h->h_createbase = strdup(create_base);
    if (h->h_createbase == NULL) {
	LDAP_destroy(context, *db);
	krb5_set_error_string(context, "strdup: out of memory");
	*db = NULL;
	return ENOMEM;
    }

    (*db)->hdb_master_key_set = 0;
    (*db)->hdb_openp = 0;
    (*db)->hdb_open = LDAP_open;
    (*db)->hdb_close = LDAP_close;
    (*db)->hdb_fetch = LDAP_fetch;
    (*db)->hdb_store = LDAP_store;
    (*db)->hdb_remove = LDAP_remove;
    (*db)->hdb_firstkey = LDAP_firstkey;
    (*db)->hdb_nextkey = LDAP_nextkey;
    (*db)->hdb_lock = LDAP_lock;
    (*db)->hdb_unlock = LDAP_unlock;
    (*db)->hdb_rename = LDAP_rename;
    (*db)->hdb__get = NULL;
    (*db)->hdb__put = NULL;
    (*db)->hdb__del = NULL;
    (*db)->hdb_destroy = LDAP_destroy;

    return 0;
}

#ifdef OPENLDAP_MODULE

struct hdb_so_method hdb_ldap_interface = {
    HDB_INTERFACE_VERSION,
    "ldap",
    hdb_ldap_create
};

#endif

#endif				/* OPENLDAP */
