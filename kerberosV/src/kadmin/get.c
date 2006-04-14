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

#include "kadmin_locl.h"
#include "kadmin-commands.h"
#include <parse_units.h>
#include <rtbl.h>

RCSID("$KTH: get.c,v 1.23 2005/05/30 20:41:17 lha Exp $");

static struct field_name {
    const char *fieldname;
    unsigned int fieldvalue;
    const char *default_header;
    const char *def_longheader;
    unsigned int flags;
} field_names[] = {
    { "principal", KADM5_PRINCIPAL, "Principal", "Principal", 0 },
    { "princ_expire_time", KADM5_PRINC_EXPIRE_TIME, "Expiration", "Principal expires", 0 },
    { "pw_expiration", KADM5_PW_EXPIRATION, "PW-exp", "Password expires", 0 },
    { "last_pwd_change", KADM5_LAST_PWD_CHANGE, "PW-change", "Last password change", 0 },
    { "max_life", KADM5_MAX_LIFE, "Max life", "Max ticket life", 0 },
    { "max_rlife", KADM5_MAX_RLIFE, "Max renew", "Max renewable life", 0 },
    { "mod_time", KADM5_MOD_TIME, "Mod time", "Last modified", 0 },
    { "mod_name", KADM5_MOD_NAME, "Modifier", "Modifier", 0 },
    { "attributes", KADM5_ATTRIBUTES, "Attributes", "Attributes", 0 },
    { "kvno", KADM5_KVNO, "Kvno", "Kvno", RTBL_ALIGN_RIGHT  },
    { "mkvno", KADM5_MKVNO, "Mkvno", "Mkvno", RTBL_ALIGN_RIGHT },
    { "last_success", KADM5_LAST_SUCCESS, "Last login", "Last successful login", 0 },
    { "last_failed", KADM5_LAST_FAILED, "Last fail", "Last failed login", 0 },
    { "fail_auth_count", KADM5_FAIL_AUTH_COUNT, "Fail count", "Failed login count", RTBL_ALIGN_RIGHT },
    
    { "policy", KADM5_POLICY, "Policy", "Policy", 0 },
    { "keytypes", KADM5_KEY_DATA, "Keytypes", "Keytypes", 0 },
    { NULL }
};

struct field_info {
    struct field_name *ff;
    char *header;
    struct field_info *next;
};

struct get_entry_data {
    void (*format)(struct get_entry_data*, kadm5_principal_ent_t);
    rtbl_t table;
    u_int32_t mask;
    struct field_info *chead, **ctail;
};

static int
add_column(struct get_entry_data *data, struct field_name *ff, const char *header)
{
    struct field_info *f = malloc(sizeof(*f));
    f->ff = ff;
    if(header)
	f->header = strdup(header);
    else
	f->header = NULL;
    f->next = NULL;
    *data->ctail = f;
    data->ctail = &f->next;
    data->mask |= ff->fieldvalue;
    if(data->table != NULL)
	rtbl_add_column_by_id(data->table, ff->fieldvalue, 
			      header ? header : ff->default_header, ff->flags);
    return 0;
}

/*
 * return 0 iff `salt' actually is the same as the current salt in `k'
 */

static int
cmp_salt (const krb5_salt *salt, const krb5_key_data *k)
{
    if (salt->salttype != k->key_data_type[1])
	return 1;
    if (salt->saltvalue.length != k->key_data_length[1])
	return 1;
    return memcmp (salt->saltvalue.data, k->key_data_contents[1],
		   salt->saltvalue.length);
}

static void
format_keytype(krb5_key_data *k, krb5_salt *def_salt, char *buf, size_t buf_len)
{
    krb5_error_code ret;
    char *s;

    ret = krb5_enctype_to_string (context,
				  k->key_data_type[0],
				  &s);
    if (ret)
	asprintf (&s, "unknown(%d)", k->key_data_type[0]);
    strlcpy(buf, s, buf_len);
    free(s);

    strlcat(buf, "(", buf_len);

    ret = krb5_salttype_to_string (context,
				   k->key_data_type[0],
				   k->key_data_type[1],
				   &s);
    if (ret)
	asprintf (&s, "unknown(%d)", k->key_data_type[1]);
    strlcat(buf, s, buf_len);
    free(s);

    if (cmp_salt(def_salt, k) == 0)
	s = strdup("");
    else if(k->key_data_length[1] == 0)
	s = strdup("()");
    else
	asprintf (&s, "(%.*s)", k->key_data_length[1],
		  (char *)k->key_data_contents[1]);
    strlcat(buf, s, buf_len);
    free(s);

    strlcat(buf, ")", buf_len);
}

static void
format_field(kadm5_principal_ent_t princ, unsigned int field, 
	     char *buf, size_t buf_len, int condensed)
{
    switch(field) {
    case KADM5_PRINCIPAL:
	if(condensed)
	    krb5_unparse_name_fixed_short(context, princ->principal, buf, buf_len);
	else
	    krb5_unparse_name_fixed(context, princ->principal, buf, buf_len);
	break;
    
    case KADM5_PRINC_EXPIRE_TIME:
	time_t2str(princ->princ_expire_time, buf, buf_len, !condensed);
	break;
	    
    case KADM5_PW_EXPIRATION:
	time_t2str(princ->pw_expiration, buf, buf_len, !condensed);
	break;
	    
    case KADM5_LAST_PWD_CHANGE:
	time_t2str(princ->last_pwd_change, buf, buf_len, !condensed);
	break;
	    
    case KADM5_MAX_LIFE:
	deltat2str(princ->max_life, buf, buf_len);
	break;
	    
    case KADM5_MAX_RLIFE:
	deltat2str(princ->max_renewable_life, buf, buf_len);
	break;
	    
    case KADM5_MOD_TIME:
	time_t2str(princ->mod_date, buf, buf_len, !condensed);
	break;

    case KADM5_MOD_NAME:
	if (princ->mod_name == NULL)
	    strlcpy(buf, "unknown", buf_len);
	else if(condensed)
	    krb5_unparse_name_fixed_short(context, princ->mod_name, buf, buf_len);
	else
	    krb5_unparse_name_fixed(context, princ->mod_name, buf, buf_len);
	break;
    case KADM5_ATTRIBUTES:
	attributes2str (princ->attributes, buf, buf_len);
	break;
    case KADM5_KVNO:
	snprintf(buf, buf_len, "%d", princ->kvno);
	break;
    case KADM5_MKVNO:
	snprintf(buf, buf_len, "%d", princ->mkvno);
	break;
    case KADM5_LAST_SUCCESS:
	time_t2str(princ->last_success, buf, buf_len, !condensed);
	break;
    case KADM5_LAST_FAILED:
	time_t2str(princ->last_failed, buf, buf_len, !condensed);
	break;
    case KADM5_FAIL_AUTH_COUNT:
	snprintf(buf, buf_len, "%d", princ->fail_auth_count);
	break;
    case KADM5_POLICY:
	if(princ->policy != NULL)
	    strlcpy(buf, princ->policy, buf_len);
	else
	    strlcpy(buf, "none", buf_len);
	break;
    case KADM5_KEY_DATA:{
	krb5_salt def_salt;
	int i;
	char buf2[1024];
	krb5_get_pw_salt (context, princ->principal, &def_salt);

	*buf = '\0';
	for (i = 0; i < princ->n_key_data; ++i) {
	    format_keytype(&princ->key_data[i], &def_salt, buf2, sizeof(buf2));
	    if(i > 0)
		strlcat(buf, ", ", buf_len);
	    strlcat(buf, buf2, buf_len);
	}
	krb5_free_salt (context, def_salt);
	break;
    }
    default:
	strlcpy(buf, "<unknown>", buf_len);
	break;
    }
}

static void
print_entry_short(struct get_entry_data *data, kadm5_principal_ent_t princ)
{
    char buf[1024];
    struct field_info *f;
    
    for(f = data->chead; f != NULL; f = f->next) {
	format_field(princ, f->ff->fieldvalue, buf, sizeof(buf), 1);
	rtbl_add_column_entry_by_id(data->table, f->ff->fieldvalue, buf);
    }
}

static void
print_entry_long(struct get_entry_data *data, kadm5_principal_ent_t princ)
{
    char buf[1024];
    struct field_info *f;
    int width = 0;
    
    for(f = data->chead; f != NULL; f = f->next) {
	int w = strlen(f->header ? f->header : f->ff->def_longheader);
	if(w > width)
	    width = w;
    }
    for(f = data->chead; f != NULL; f = f->next) {
	format_field(princ, f->ff->fieldvalue, buf, sizeof(buf), 0);
	printf("%*s: %s\n", width, f->header ? f->header : f->ff->def_longheader, buf);
    }
    printf("\n");
}

static int
do_get_entry(krb5_principal principal, void *data)
{
    kadm5_principal_ent_rec princ;
    krb5_error_code ret;
    struct get_entry_data *e = data;
    
    memset(&princ, 0, sizeof(princ));
    ret = kadm5_get_principal(kadm_handle, principal, 
			      &princ,
			      e->mask);
    if(ret)
	return ret;
    else {
	(e->format)(e, &princ);
	kadm5_free_principal_ent(kadm_handle, &princ);
    }
    return 0;
}

static void
free_columns(struct get_entry_data *data)
{
    struct field_info *f, *next;
    for(f = data->chead; f != NULL; f = next) {
	free(f->header);
	next = f->next;
	free(f);
    }
    data->chead = NULL;
    data->ctail = &data->chead;
}

static int
setup_columns(struct get_entry_data *data, const char *column_info)
{
    char buf[1024], *q;
    char *field, *header;
    struct field_name *f;

    while(strsep_copy(&column_info, ",", buf, sizeof(buf)) != -1) {
	q = buf;
	field = strsep(&q, "=");
	header = strsep(&q, "=");
	for(f = field_names; f->fieldname != NULL; f++) {
	    if(strcasecmp(field, f->fieldname) == 0) {
		add_column(data, f, header);
		break;
	    }
	}
	if(f->fieldname == NULL) {
	    krb5_warnx(context, "unknown field name \"%s\"", field);
	    free_columns(data);
	    return -1;
	}
    }
    return 0;
}

#define DEFAULT_COLUMNS_SHORT "principal,princ_expire_time,pw_expiration,last_pwd_change,max_life,max_rlife"
#define DEFAULT_COLUMNS_LONG "principal,princ_expire_time,pw_expiration,last_pwd_change,max_life,max_rlife,kvno,mkvno,last_success,last_failed,fail_auth_count,mod_time,mod_name,attributes,keytypes"
#define DEFAULT_COLUMNS_TERSE "principal="

static int
getit(struct get_options *opt, const char *name, int argc, char **argv)
{
    int i;
    krb5_error_code ret;
    struct get_entry_data data;
    
    if(opt->long_flag == -1 && (opt->short_flag == 1 || opt->terse_flag == 1))
	opt->long_flag = 0;
    if(opt->short_flag == -1 && (opt->long_flag == 1 || opt->terse_flag == 1))
	opt->short_flag = 0;
    if(opt->terse_flag == -1 && (opt->long_flag == 1 || opt->short_flag == 1))
	opt->terse_flag = 0;
    if(opt->long_flag == 0 && opt->short_flag == 0 && opt->terse_flag == 0)
	opt->short_flag = 1;

    data.table = NULL;
    data.chead = NULL;
    data.ctail = &data.chead;
    data.mask = 0;

    if(opt->short_flag || opt->terse_flag) {
	data.table = rtbl_create();
	rtbl_set_separator(data.table, "  ");
	data.format = print_entry_short;
    } else
	data.format = print_entry_long;
    if(opt->column_info_string == NULL) {
	if(opt->long_flag)
	    ret = setup_columns(&data, DEFAULT_COLUMNS_LONG);
	else if(opt->short_flag)
	    ret = setup_columns(&data, DEFAULT_COLUMNS_SHORT);
	else {
	    ret = setup_columns(&data, DEFAULT_COLUMNS_TERSE);
	    rtbl_set_flags(data.table, RTBL_HEADER_STYLE_NONE);
	}
    } else
	ret = setup_columns(&data, opt->column_info_string);
	
    if(ret != 0) {
	if(data.table != NULL)
	    rtbl_destroy(data.table);
	return 0;
    }
    
    for(i = 0; i < argc; i++)
	ret = foreach_principal(argv[i], do_get_entry, "get", &data);
    
    if(data.table != NULL) {
	rtbl_format(data.table, stdout);
	rtbl_destroy(data.table);
    }
    free_columns(&data);
    return 0;
}

int
get_entry(struct get_options *opt, int argc, char **argv)
{
    return getit(opt, "get", argc, argv);
}

int
list_princs(struct list_options *opt, int argc, char **argv)
{
    if(sizeof(struct get_options) != sizeof(struct list_options)) {
	krb5_warnx(context, "programmer error: sizeof(struct get_options) != sizeof(struct list_options)");
	return 0;
    }
    return getit((struct get_options*)opt, "list", argc, argv);
}
