/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"

RCSID("$KTH: cache.c,v 1.69.2.1 2005/06/15 11:24:47 lha Exp $");

/*
 * Add a new ccache type with operations `ops', overwriting any
 * existing one if `override'.
 * Return an error code or 0.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_register(krb5_context context, 
		 const krb5_cc_ops *ops, 
		 krb5_boolean override)
{
    int i;

    for(i = 0; i < context->num_cc_ops && context->cc_ops[i].prefix; i++) {
	if(strcmp(context->cc_ops[i].prefix, ops->prefix) == 0) {
	    if(!override) {
		krb5_set_error_string(context,
				      "ccache type %s already exists",
				      ops->prefix);
		return KRB5_CC_TYPE_EXISTS;
	    }
	    break;
	}
    }
    if(i == context->num_cc_ops) {
	krb5_cc_ops *o = realloc(context->cc_ops,
				 (context->num_cc_ops + 1) *
				 sizeof(*context->cc_ops));
	if(o == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return KRB5_CC_NOMEM;
	}
	context->num_cc_ops++;
	context->cc_ops = o;
	memset(context->cc_ops + i, 0, 
	       (context->num_cc_ops - i) * sizeof(*context->cc_ops));
    }
    memcpy(&context->cc_ops[i], ops, sizeof(context->cc_ops[i]));
    return 0;
}

/*
 * Allocate memory for a new ccache in `id' with operations `ops'
 * and name `residual'.
 * Return 0 or an error code.
 */

static krb5_error_code
allocate_ccache (krb5_context context,
		 const krb5_cc_ops *ops,
		 const char *residual,
		 krb5_ccache *id)
{
    krb5_error_code ret;
    krb5_ccache p;

    p = malloc(sizeof(*p));
    if(p == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return KRB5_CC_NOMEM;
    }
    p->ops = ops;
    *id = p;
    ret = p->ops->resolve(context, id, residual);
    if(ret)
	free(p);
    return ret;
}

/*
 * Find and allocate a ccache in `id' from the specification in `residual'.
 * If the ccache name doesn't contain any colon, interpret it as a file name.
 * Return 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_resolve(krb5_context context,
		const char *name,
		krb5_ccache *id)
{
    int i;

    for(i = 0; i < context->num_cc_ops && context->cc_ops[i].prefix; i++) {
	size_t prefix_len = strlen(context->cc_ops[i].prefix);

	if(strncmp(context->cc_ops[i].prefix, name, prefix_len) == 0
	   && name[prefix_len] == ':') {
	    return allocate_ccache (context, &context->cc_ops[i],
				    name + prefix_len + 1,
				    id);
	}
    }
    if (strchr (name, ':') == NULL)
	return allocate_ccache (context, &krb5_fcc_ops, name, id);
    else {
	krb5_set_error_string(context, "unknown ccache type %s", name);
	return KRB5_CC_UNKNOWN_TYPE;
    }
}

/*
 * Generate a new ccache of type `ops' in `id'.
 * Return 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_gen_new(krb5_context context,
		const krb5_cc_ops *ops,
		krb5_ccache *id)
{
    krb5_ccache p;

    p = malloc (sizeof(*p));
    if (p == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return KRB5_CC_NOMEM;
    }
    p->ops = ops;
    *id = p;
    return p->ops->gen_new(context, id);
}

/*
 * Generates a new unique ccache of `type` in `id'. If `type' is NULL,
 * the library chooses the default credential cache type. The supplied
 * `hint' (that can be NULL) is a string that the credential cache
 * type can use to base the name of the credential on, this is to make
 * its easier for the user to differentiate the credentials.
 *
 *  Returns 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_new_unique(krb5_context context, const char *type, 
		   const char *hint, krb5_ccache *id)
{
    const krb5_cc_ops *ops;

    if (type == NULL)
	type = "FILE";

    ops = krb5_cc_get_prefix_ops(context, type);
    if (ops == NULL) {
	krb5_set_error_string(context, "Credential cache type %s is unknown",
			      type);
	return KRB5_CC_UNKNOWN_TYPE;
    }

    return krb5_cc_gen_new(context, ops, id);
}

/*
 * Return the name of the ccache `id'
 */

const char* KRB5_LIB_FUNCTION
krb5_cc_get_name(krb5_context context,
		 krb5_ccache id)
{
    return id->ops->get_name(context, id);
}

/*
 * Return the type of the ccache `id'.
 */

const char* KRB5_LIB_FUNCTION
krb5_cc_get_type(krb5_context context,
		 krb5_ccache id)
{
    return id->ops->prefix;
}

/*
 * Return krb5_cc_ops of a the ccache `id'.
 */

const krb5_cc_ops *
krb5_cc_get_ops(krb5_context context, krb5_ccache id)
{
    return id->ops;
}

/*
 * Expand variables in `str' into `res'
 */

krb5_error_code
_krb5_expand_default_cc_name(krb5_context context, const char *str, char **res)
{
    size_t tlen, len = 0;
    char *tmp, *tmp2, *append;

    *res = NULL;

    while (str && *str) {
	tmp = strstr(str, "%{");
	if (tmp && tmp != str) {
	    append = malloc((tmp - str) + 1);
	    if (append) {
		memcpy(append, str, tmp - str);
		append[tmp - str] = '\0';
	    }
	    str = tmp;
	} else if (tmp) {
	    tmp2 = strchr(tmp, '}');
	    if (tmp2 == NULL) {
		free(*res);
		*res = NULL;
		krb5_set_error_string(context, "variable missing }");
		return KRB5_CONFIG_BADFORMAT;
	    }
	    if (strncasecmp(tmp, "%{uid}", 6) == 0)
		asprintf(&append, "%u", (unsigned)getuid());
	    else if (strncasecmp(tmp, "%{null}", 7) == 0)
		append = strdup("");
	    else {
		free(*res);
		*res = NULL;
		krb5_set_error_string(context, 
				      "expand default cache unknown "
				      "variable \"%.*s\"",
				      (int)(tmp2 - tmp) - 2, tmp + 2);
		return KRB5_CONFIG_BADFORMAT;
	    }
	    if (append == NULL) {
		free(*res);
		res = NULL;
		krb5_set_error_string(context, "malloc - out of memory");
		return ENOMEM;
	    }
	    str = tmp2 + 1;
	} else {
	    append = (char *)str;
	    str = NULL;
	}

	tlen = strlen(append);
	tmp = realloc(*res, len + tlen + 1);
	if (tmp == NULL) {
	    free(*res);
	    *res = NULL;
	    krb5_set_error_string(context, "malloc - out of memory");
	    return ENOMEM;
	}
	*res = tmp;
	memcpy(*res + len, append, tlen + 1);
	len = len + tlen;
	if (str)
	    free(append);
    }    
    return 0;
}

/*
 * Set the default cc name for `context' to `name'.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_set_default_name(krb5_context context, const char *name)
{
    krb5_error_code ret = 0;
    char *p;

    if (name == NULL) {
	const char *e = NULL;

	if(!issuid()) {
	    e = getenv("KRB5CCNAME");
	    if (e)
		p = strdup(e);
	}
	if (e == NULL) {
	    e = krb5_config_get_string(context, NULL, "libdefaults",
				       "default_cc_name", NULL);
	    if (e) {
		ret = _krb5_expand_default_cc_name(context, e, &p);
		if (ret)
		    return ret;
	    }
	}
	if (e == NULL)
	    asprintf(&p,"FILE:/tmp/krb5cc_%u", (unsigned)getuid());
    } else
	p = strdup(name);

    if (p == NULL) {
	krb5_set_error_string(context, "malloc - out of memory");
	return ENOMEM;
    }

    if (context->default_cc_name)
	free(context->default_cc_name);

    context->default_cc_name = p;

    return ret;
}

/*
 * Return a pointer to a context static string containing the default
 * ccache name.
 */

const char* KRB5_LIB_FUNCTION
krb5_cc_default_name(krb5_context context)
{
    if (context->default_cc_name == NULL)
	krb5_cc_set_default_name(context, NULL);

    return context->default_cc_name;
}

/*
 * Open the default ccache in `id'.
 * Return 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_default(krb5_context context,
		krb5_ccache *id)
{
    const char *p = krb5_cc_default_name(context);

    if (p == NULL) {
	krb5_set_error_string(context, "malloc - out of memory");
	return ENOMEM;
    }
    return krb5_cc_resolve(context, p, id);
}

/*
 * Create a new ccache in `id' for `primary_principal'.
 * Return 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_initialize(krb5_context context,
		   krb5_ccache id,
		   krb5_principal primary_principal)
{
    return id->ops->init(context, id, primary_principal);
}


/*
 * Remove the ccache `id'.
 * Return 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_destroy(krb5_context context,
		krb5_ccache id)
{
    krb5_error_code ret;

    ret = id->ops->destroy(context, id);
    krb5_cc_close (context, id);
    return ret;
}

/*
 * Stop using the ccache `id' and free the related resources.
 * Return 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_close(krb5_context context,
	      krb5_ccache id)
{
    krb5_error_code ret;
    ret = id->ops->close(context, id);
    free(id);
    return ret;
}

/*
 * Store `creds' in the ccache `id'.
 * Return 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_store_cred(krb5_context context,
		   krb5_ccache id,
		   krb5_creds *creds)
{
    return id->ops->store(context, id, creds);
}

/*
 * Retrieve the credential identified by `mcreds' (and `whichfields')
 * from `id' in `creds'.
 * Return 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_retrieve_cred(krb5_context context,
		      krb5_ccache id,
		      krb5_flags whichfields,
		      const krb5_creds *mcreds,
		      krb5_creds *creds)
{
    krb5_error_code ret;
    krb5_cc_cursor cursor;

    if (id->ops->retrieve != NULL) {
	return id->ops->retrieve(context, id, whichfields,
				 mcreds, creds);
    }

    krb5_cc_start_seq_get(context, id, &cursor);
    while((ret = krb5_cc_next_cred(context, id, &cursor, creds)) == 0){
	if(krb5_compare_creds(context, whichfields, mcreds, creds)){
	    ret = 0;
	    break;
	}
	krb5_free_cred_contents (context, creds);
    }
    krb5_cc_end_seq_get(context, id, &cursor);
    return ret;
}

/*
 * Return the principal of `id' in `principal'.
 * Return 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_get_principal(krb5_context context,
		      krb5_ccache id,
		      krb5_principal *principal)
{
    return id->ops->get_princ(context, id, principal);
}

/*
 * Start iterating over `id', `cursor' is initialized to the
 * beginning.
 * Return 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_start_seq_get (krb5_context context,
		       const krb5_ccache id,
		       krb5_cc_cursor *cursor)
{
    return id->ops->get_first(context, id, cursor);
}

/*
 * Retrieve the next cred pointed to by (`id', `cursor') in `creds'
 * and advance `cursor'.
 * Return 0 or an error code.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_next_cred (krb5_context context,
		   const krb5_ccache id,
		   krb5_cc_cursor *cursor,
		   krb5_creds *creds)
{
    return id->ops->get_next(context, id, cursor, creds);
}

/* like krb5_cc_next_cred, but allow for selective retrieval */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_next_cred_match(krb5_context context,
			const krb5_ccache id,
			krb5_cc_cursor * cursor,
			krb5_creds * creds,
			krb5_flags whichfields,
			const krb5_creds * mcreds)
{
    krb5_error_code ret;
    while (1) {
	ret = krb5_cc_next_cred(context, id, cursor, creds);
	if (ret)
	    return ret;
	if (mcreds == NULL || krb5_compare_creds(context, whichfields, mcreds, creds))
	    return 0;
	krb5_free_cred_contents(context, creds);
    }
}

/*
 * Destroy the cursor `cursor'.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_end_seq_get (krb5_context context,
		     const krb5_ccache id,
		     krb5_cc_cursor *cursor)
{
    return id->ops->end_get(context, id, cursor);
}

/*
 * Remove the credential identified by `cred', `which' from `id'.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_remove_cred(krb5_context context,
		    krb5_ccache id,
		    krb5_flags which,
		    krb5_creds *cred)
{
    if(id->ops->remove_cred == NULL) {
	krb5_set_error_string(context,
			      "ccache %s does not support remove_cred",
			      id->ops->prefix);
	return EACCES; /* XXX */
    }
    return (*id->ops->remove_cred)(context, id, which, cred);
}

/*
 * Set the flags of `id' to `flags'.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_set_flags(krb5_context context,
		  krb5_ccache id,
		  krb5_flags flags)
{
    return id->ops->set_flags(context, id, flags);
}
		    
/*
 * Copy the contents of `from' to `to'.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_copy_cache_match(krb5_context context,
			 const krb5_ccache from,
			 krb5_ccache to,
			 krb5_flags whichfields,
			 const krb5_creds * mcreds,
			 unsigned int *matched)
{
    krb5_error_code ret;
    krb5_cc_cursor cursor;
    krb5_creds cred;
    krb5_principal princ;

    ret = krb5_cc_get_principal(context, from, &princ);
    if (ret)
	return ret;
    ret = krb5_cc_initialize(context, to, princ);
    if (ret) {
	krb5_free_principal(context, princ);
	return ret;
    }
    ret = krb5_cc_start_seq_get(context, from, &cursor);
    if (ret) {
	krb5_free_principal(context, princ);
	return ret;
    }
    if (matched)
	*matched = 0;
    while (ret == 0 &&
	   krb5_cc_next_cred_match(context, from, &cursor, &cred,
				   whichfields, mcreds) == 0) {
	if (matched)
	    (*matched)++;
	ret = krb5_cc_store_cred(context, to, &cred);
	krb5_free_cred_contents(context, &cred);
    }
    krb5_cc_end_seq_get(context, from, &cursor);
    krb5_free_principal(context, princ);
    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_copy_cache(krb5_context context,
		   const krb5_ccache from,
		   krb5_ccache to)
{
    return krb5_cc_copy_cache_match(context, from, to, 0, NULL, NULL);
}

/*
 * Return the version of `id'.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_cc_get_version(krb5_context context,
		    const krb5_ccache id)
{
    if(id->ops->get_version)
	return id->ops->get_version(context, id);
    else
	return 0;
}

/*
 * Clear `mcreds' so it can be used with krb5_cc_retrieve_cred
 */

void KRB5_LIB_FUNCTION
krb5_cc_clear_mcred(krb5_creds *mcred)
{
    memset(mcred, 0, sizeof(*mcred));
}

/*
 * Get the cc ops that is registered in `context' to handle the
 * `prefix'. Returns NULL if ops not found.
 */

const krb5_cc_ops *
krb5_cc_get_prefix_ops(krb5_context context, const char *prefix)
{
    int i;

    for(i = 0; i < context->num_cc_ops && context->cc_ops[i].prefix; i++) {
	if(strcmp(context->cc_ops[i].prefix, prefix) == 0)
	    return &context->cc_ops[i];
    }
    return NULL;
}
