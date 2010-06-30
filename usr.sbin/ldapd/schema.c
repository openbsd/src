/*	$OpenBSD: schema.c,v 1.2 2010/06/30 04:14:59 martinh Exp $ */

/*
 * Copyright (c) 2010 Martin Hedenfalk <martinh@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "ldapd.h"

#define ERROR	-1
#define STRING	 1

static int
attr_oid_cmp(struct attr_type *a, struct attr_type *b)
{
	return strcasecmp(a->oid, b->oid);
}

static int
obj_oid_cmp(struct object *a, struct object *b)
{
	return strcasecmp(a->oid, b->oid);
}

static int
oidname_cmp(struct oidname *a, struct oidname *b)
{
	return strcasecmp(a->on_name, b->on_name);
}

static int
symoid_cmp(struct symoid *a, struct symoid *b)
{
	return strcasecmp(a->name, b->name);
}

RB_GENERATE(attr_type_tree, attr_type, link, attr_oid_cmp);
RB_GENERATE(object_tree, object, link, obj_oid_cmp);
RB_GENERATE(oidname_tree, oidname, link, oidname_cmp);
RB_GENERATE(symoid_tree, symoid, link, symoid_cmp);

static struct attr_list	*push_attr(struct attr_list *alist, struct attr_type *a);
static struct obj_list	*push_obj(struct obj_list *olist, struct object *obj);
static struct name_list *push_name(struct name_list *nl, const char *name);
int			 is_oidstr(const char *oidstr);

struct attr_type *
lookup_attribute_by_name(struct schema *schema, char *name)
{
	struct oidname		*on, find;

	find.on_name = name;
	on = RB_FIND(oidname_tree, &schema->attr_names, &find);

	if (on)
		return on->on_attr_type;
	return NULL;
}

struct attr_type *
lookup_attribute_by_oid(struct schema *schema, char *oid)
{
	struct attr_type	 find;

	find.oid = oid;
	return RB_FIND(attr_type_tree, &schema->attr_types, &find);
}

struct attr_type *
lookup_attribute(struct schema *schema, char *oid_or_name)
{
	if (is_oidstr(oid_or_name))
		return lookup_attribute_by_oid(schema, oid_or_name);
	return lookup_attribute_by_name(schema, oid_or_name);
}

struct object *
lookup_object_by_oid(struct schema *schema, char *oid)
{
	struct object	 find;

	find.oid = oid;
	return RB_FIND(object_tree, &schema->objects, &find);
}

struct object *
lookup_object_by_name(struct schema *schema, char *name)
{
	struct oidname		*on, find;

	find.on_name = name;
	on = RB_FIND(oidname_tree, &schema->object_names, &find);

	if (on)
		return on->on_object;
	return NULL;
}

struct object *
lookup_object(struct schema *schema, char *oid_or_name)
{
	if (is_oidstr(oid_or_name))
		return lookup_object_by_oid(schema, oid_or_name);
	return lookup_object_by_name(schema, oid_or_name);
}

/* Looks up a symbolic OID, optionally with a suffix OID, so if
 *   SYMBOL = 1.2.3.4
 * then
 *   SYMBOL:5.6 = 1.2.3.4.5.6
 *
 * Returned string must be freed by the caller.
 * Modifies the name argument.
 */
static char *
lookup_symbolic_oid(struct schema *schema, char *name)
{
	struct symoid	*symoid, find;
	char		*colon, *oid;
	size_t		 sz;

	colon = strchr(name, ':');
	if (colon != NULL) {
		if (!is_oidstr(colon + 1)) {
			log_warnx("invalid OID after colon: %s", colon + 1);
			return NULL;
		}
		*colon = '\0';
	}

	find.name = name;
	symoid = RB_FIND(symoid_tree, &schema->symbolic_oids, &find);
	if (symoid == NULL)
		return NULL;

	if (colon == NULL)
		return strdup(symoid->oid);

	/* Expand SYMBOL:OID.
	 */
	sz = strlen(symoid->oid) + 1 + strlen(colon + 1) + 1;
	if ((oid = malloc(sz)) == NULL) {
		log_warnx("malloc");
		return NULL;
	}

	strlcpy(oid, symoid->oid, sz);
	strlcat(oid, ".", sz);
	strlcat(oid, colon + 1, sz);

	return oid;
}

/* Push a symbol-OID pair on the tree. Name and OID must be valid pointers
 * during the lifetime of the tree.
 */
static struct symoid *
push_symbolic_oid(struct schema *schema, char *name, char *oid)
{
	struct symoid	*symoid, find;

	find.name = name;
	symoid = RB_FIND(symoid_tree, &schema->symbolic_oids, &find);

	if (symoid == NULL) {
		symoid = calloc(1, sizeof(*symoid));
		if (symoid == NULL) {
			log_warnx("calloc");
			return NULL;
		}

		symoid->name = name;
		RB_INSERT(symoid_tree, &schema->symbolic_oids, symoid);
	}

	free(symoid->oid);
	symoid->oid = oid;

	return symoid;
}

static struct attr_list *
push_attr(struct attr_list *alist, struct attr_type *a)
{
	struct attr_ptr		*aptr;

	if (alist == NULL) {
		if ((alist = calloc(1, sizeof(*alist))) == NULL) {
			log_warn("calloc");
			return NULL;
		}
		SLIST_INIT(alist);
	}

	if ((aptr = calloc(1, sizeof(*aptr))) == NULL) {
		log_warn("calloc");
		return NULL;
	}
	aptr->attr_type = a;
	SLIST_INSERT_HEAD(alist, aptr, next);

	return alist;
}

static struct obj_list *
push_obj(struct obj_list *olist, struct object *obj)
{
	struct obj_ptr		*optr;

	if (olist == NULL) {
		if ((olist = calloc(1, sizeof(*olist))) == NULL) {
			log_warn("calloc");
			return NULL;
		}
		SLIST_INIT(olist);
	}

	if ((optr = calloc(1, sizeof(*optr))) == NULL) {
		log_warn("calloc");
		return NULL;
	}
	optr->object = obj;
	SLIST_INSERT_HEAD(olist, optr, next);

	return olist;
}

int
is_oidstr(const char *oidstr)
{
	struct ber_oid	 oid;
	return (ber_string2oid(oidstr, &oid) == 0);
}

static struct name_list *
push_name(struct name_list *nl, const char *name)
{
	struct name	*n;

	if (nl == NULL) {
		if ((nl = calloc(1, sizeof(*nl))) == NULL) {
			log_warn("calloc");
			return NULL;
		}
		SLIST_INIT(nl);
	}
	if ((n = calloc(1, sizeof(*n))) == NULL) {
		log_warn("calloc");
		return NULL;
	}
	n->name = name;
	SLIST_INSERT_HEAD(nl, n, next);

	return nl;
}

static int
schema_getc(struct schema *schema, int quotec)
{
	int		c, next;

	if (schema->pushback_index)
		return (schema->pushback_buffer[--schema->pushback_index]);

	if (quotec) {
		if ((c = getc(schema->fp)) == EOF) {
			log_warnx("reached end of file while parsing "
			    "quoted string");
			return EOF;
		}
		return (c);
	}

	while ((c = getc(schema->fp)) == '\\') {
		next = getc(schema->fp);
		if (next != '\n') {
			c = next;
			break;
		}
		schema->lineno++;
	}

	return (c);
}

static int
schema_ungetc(struct schema *schema, int c)
{
	if (c == EOF)
		return EOF;

	if (schema->pushback_index < SCHEMA_MAXPUSHBACK-1)
		return (schema->pushback_buffer[schema->pushback_index++] = c);
	else
		return (EOF);
}

static int
findeol(struct schema *schema)
{
	int	c;

	/* skip to either EOF or the first real EOL */
	while (1) {
		if (schema->pushback_index)
			c = schema->pushback_buffer[--schema->pushback_index];
		else
			c = schema_getc(schema, 0);
		if (c == '\n') {
			schema->lineno++;
			break;
		}
		if (c == EOF)
			break;
	}
	return (ERROR);
}

static int
schema_lex(struct schema *schema, char **kw)
{
	char	 buf[8096];
	char	*p;
	int	 quotec, next, c;

	if (kw)
		*kw = NULL;

top:
	p = buf;
	while ((c = schema_getc(schema, 0)) == ' ' || c == '\t')
		; /* nothing */

	if (c == '#')
		while ((c = schema_getc(schema, 0)) != '\n' && c != EOF)
			; /* nothing */

	switch (c) {
	case '\'':
	case '"':
		quotec = c;
		while (1) {
			if ((c = schema_getc(schema, quotec)) == EOF)
				return (0);
			if (c == '\n') {
				schema->lineno++;
				continue;
			} else if (c == '\\') {
				if ((next = schema_getc(schema, quotec)) == EOF)
					return (0);
				if (next == quotec || c == ' ' || c == '\t')
					c = next;
				else if (next == '\n')
					continue;
				else
					schema_ungetc(schema, next);
			} else if (c == quotec) {
				*p = '\0';
				break;
			}
			if (p + 1 >= buf + sizeof(buf) - 1) {
				log_warnx("string too long");
				return (findeol(schema));
			}
			*p++ = (char)c;
		}
		if (kw != NULL && (*kw = strdup(buf)) == NULL)
			fatal("schema_lex: strdup");
		return (STRING);
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':' || c == '_' || c == '*') {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				log_warnx("string too long");
				return (findeol(schema));
			}
		} while ((c = schema_getc(schema, 0)) != EOF && (allowed_in_string(c)));
		schema_ungetc(schema, c);
		*p = '\0';
		if (kw != NULL && (*kw = strdup(buf)) == NULL)
			fatal("schema_lex: strdup");
		return STRING;
	}
	if (c == '\n') {
		schema->lineno++;
		goto top;
	}
	if (c == EOF)
		return (0);
	return (c);
}

struct schema *
schema_new(void)
{
	struct schema	*schema;

	if ((schema = calloc(1, sizeof(*schema))) == NULL)
		return NULL;

	RB_INIT(&schema->attr_types);
	RB_INIT(&schema->attr_names);
	RB_INIT(&schema->objects);
	RB_INIT(&schema->object_names);
	RB_INIT(&schema->symbolic_oids);

	return schema;
}

static void
schema_err(struct schema *schema, const char *fmt, ...)
{
	va_list		 ap;
	char		*nfmt;

	va_start(ap, fmt);
	if (asprintf(&nfmt, "%s:%d: %s", schema->filename, schema->lineno,
	    fmt) == -1)
		fatal("asprintf");
	vlog(LOG_CRIT, nfmt, ap);
	va_end(ap);
	free(nfmt);

	schema->error++;
}

static int
schema_link_attr_name(struct schema *schema, const char *name, struct attr_type *attr)
{
	struct oidname		*oidname, *prev;

	if ((oidname = calloc(1, sizeof(*oidname))) == NULL) {
		log_warn("calloc");
		return -1;
	}

	oidname->on_name = name;
	oidname->on_attr_type = attr;
	prev = RB_INSERT(oidname_tree, &schema->attr_names, oidname);
	if (prev != NULL) {
		schema_err(schema, "attribute type name '%s'"
		    " already defined for oid %s",
		    name, prev->on_attr_type->oid);
		free(oidname);
		return -1;
	}

	return 0;
}

static int
schema_link_attr_names(struct schema *schema, struct attr_type *attr)
{
	struct name	*name;

	SLIST_FOREACH(name, attr->names, next) {
		if (schema_link_attr_name(schema, name->name, attr) != 0)
			return -1;
	}
	return 0;
}

static int
schema_link_obj_name(struct schema *schema, const char *name, struct object *obj)
{
	struct oidname		*oidname, *prev;

	if ((oidname = calloc(1, sizeof(*oidname))) == NULL) {
		log_warn("calloc");
		return -1;
	}

	oidname->on_name = name;
	oidname->on_object = obj;
	prev = RB_INSERT(oidname_tree, &schema->object_names, oidname);
	if (prev != NULL) {
		schema_err(schema, "object class name '%s'"
		    " already defined for oid %s",
		    name, prev->on_object->oid);
		free(oidname);
		return -1;
	}

	return 0;
}

static int
schema_link_obj_names(struct schema *schema, struct object *obj)
{
	struct name	*name;

	SLIST_FOREACH(name, obj->names, next) {
		if (schema_link_obj_name(schema, name->name, obj) != 0)
			return -1;
	}
	return 0;
}

static struct name_list *
schema_parse_names(struct schema *schema)
{
	struct name_list	*nlist = NULL;
	char			*kw;
	int			 token;

	token = schema_lex(schema, &kw);
	if (token == STRING)
		return push_name(NULL, kw);

	if (token != '(')
		goto fail;

	for (;;) {
		token = schema_lex(schema, &kw);
		if (token == ')')
			break;
		if (token != STRING)
			goto fail;
		nlist = push_name(nlist, kw);
	}

	return nlist;

fail:
	free(kw);
	/* FIXME: leaks nlist here */
	return NULL;
}

static struct attr_list *
schema_parse_attrlist(struct schema *schema)
{
	struct attr_list	*alist = NULL;
	struct attr_type	*attr;
	char			*kw;
	int			 token, want_dollar = 0;

	token = schema_lex(schema, &kw);
	if (token == STRING) {
		if ((attr = lookup_attribute(schema, kw)) == NULL) {
			schema_err(schema, "undeclared attribute type '%s'", kw);
			goto fail;
		}
		free(kw);
		return push_attr(NULL, attr);
	}

	if (token != '(')
		goto fail;

	for (;;) {
		token = schema_lex(schema, &kw);
		if (token == ')')
			break;
		if (token == '$') {
			if (!want_dollar)
				goto fail;
			want_dollar = 0;
			continue;
		}
		if (token != STRING)
			goto fail;
		if ((attr = lookup_attribute(schema, kw)) == NULL)
			goto fail;
		alist = push_attr(alist, attr);
		want_dollar = 1;
	}

	return alist;

fail:
	free(kw);
	/* FIXME: leaks alist here */
	return NULL;
}

static struct obj_list *
schema_parse_objlist(struct schema *schema)
{
	struct obj_list	*olist = NULL;
	struct object	*obj;
	char		*kw;
	int		 token, want_dollar = 0;

	token = schema_lex(schema, &kw);
	if (token == STRING) {
		if ((obj = lookup_object(schema, kw)) == NULL) {
			schema_err(schema, "undeclared object class '%s'", kw);
			goto fail;
		}
		free(kw);
		return push_obj(NULL, obj);
	}

	if (token != '(')
		goto fail;

	for (;;) {
		token = schema_lex(schema, &kw);
		if (token == ')')
			break;
		if (token == '$') {
			if (!want_dollar)
				goto fail;
			want_dollar = 0;
			continue;
		}
		if (token != STRING)
			goto fail;
		if ((obj = lookup_object(schema, kw)) == NULL)
			goto fail;
		olist = push_obj(olist, obj);
		want_dollar = 1;
	}

	return olist;

fail:
	free(kw);
	/* FIXME: leaks olist here */
	return NULL;
}

static int
schema_parse_attributetype(struct schema *schema)
{
	struct attr_type	*attr = NULL, *prev;
	char			*kw, *arg = NULL;
	int			 token, ret = 0, c;

	if (schema_lex(schema, NULL) != '(')
		goto fail;

	if (schema_lex(schema, &kw) != STRING)
		goto fail;

	if ((attr = calloc(1, sizeof(*attr))) == NULL) {
		log_warn("calloc");
		goto fail;
	}

	if (is_oidstr(kw))
		attr->oid = kw;
	else {
		attr->oid = lookup_symbolic_oid(schema, kw);
		if (attr->oid == NULL)
			goto fail;
		free(kw);
	}
	kw = NULL;

	prev = RB_INSERT(attr_type_tree, &schema->attr_types, attr);
	if (prev != NULL) {
		schema_err(schema, "attribute type %s already defined", attr->oid);
		goto fail;
	}

	while (ret == 0) {
		token = schema_lex(schema, &kw);
		if (token == ')')
			break;
		else if (token != STRING)
			goto fail;
		if (strcasecmp(kw, "NAME") == 0) {
			attr->names = schema_parse_names(schema);
			if (attr->names == NULL)
				goto fail;
			schema_link_attr_names(schema, attr);
		} else if (strcasecmp(kw, "DESC") == 0) {
			if (schema_lex(schema, &attr->desc) != STRING)
				goto fail;
		} else if (strcasecmp(kw, "OBSOLETE") == 0) {
			attr->obsolete = 1;
		} else if (strcasecmp(kw, "SUP") == 0) {
			if (schema_lex(schema, &arg) != STRING)
				goto fail;
			if ((attr->sup = lookup_attribute(schema, arg)) == NULL) {
				schema_err(schema, "%s: no such attribute", arg);
				goto fail;
			}
		} else if (strcasecmp(kw, "EQUALITY") == 0) {
			if (schema_lex(schema, &attr->equality) != STRING)
				goto fail;
		} else if (strcasecmp(kw, "ORDERING") == 0) {
			if (schema_lex(schema, &attr->ordering) != STRING)
				goto fail;
		} else if (strcasecmp(kw, "SUBSTR") == 0) {
			if (schema_lex(schema, &attr->substr) != STRING)
				goto fail;
		} else if (strcasecmp(kw, "SYNTAX") == 0) {
			if (schema_lex(schema, &attr->syntax) != STRING ||
			    !is_oidstr(attr->syntax))
				goto fail;
			if ((c = schema_getc(schema, 0)) == '{') {
				if (schema_lex(schema, NULL) != STRING ||
				    schema_lex(schema, NULL) != '}')
					goto fail;
			} else
				schema_ungetc(schema, c);
		} else if (strcasecmp(kw, "SINGLE-VALUE") == 0) {
			attr->single = 1;
		} else if (strcasecmp(kw, "COLLECTIVE") == 0) {
			attr->collective = 1;
		} else if (strcasecmp(kw, "NO-USER-MODIFICATION") == 0) {
			attr->immutable = 1;
		} else if (strcasecmp(kw, "USAGE") == 0) {
			if (schema_lex(schema, &arg) != STRING)
				goto fail;
			if (strcasecmp(arg, "dSAOperation") == 0)
				attr->usage = USAGE_DSA_OP;
			else if (strcasecmp(arg, "directoryOperation") == 0)
				attr->usage = USAGE_DIR_OP;
			else if (strcasecmp(arg, "distributedOperation") == 0)
				attr->usage = USAGE_DIST_OP;
			else if (strcasecmp(arg, "userApplications") == 0)
				attr->usage = USAGE_USER_APP;
			else {
				schema_err(schema, "invalid usage '%s'", arg);
				goto fail;
			}
		}
	}

	return 0;

fail:
	free(kw);
	free(arg);
	if (attr != NULL) {
		if (attr->oid != NULL) {
			RB_REMOVE(attr_type_tree, &schema->attr_types, attr);
			free(attr->oid);
		}
		free(attr->desc);
		free(attr);
	}
	return -1;
}

static int
schema_parse_objectclass(struct schema *schema)
{
	struct object		*obj = NULL, *prev;
	struct obj_ptr		*optr;
	char			*kw;
	int			 token, ret = 0;

	if (schema_lex(schema, NULL) != '(')
		goto fail;

	if (schema_lex(schema, &kw) != STRING)
		goto fail;

	if ((obj = calloc(1, sizeof(*obj))) == NULL) {
		log_warn("calloc");
		goto fail;
	}
	obj->kind = KIND_STRUCTURAL;

	if (is_oidstr(kw))
		obj->oid = kw;
	else {
		obj->oid = lookup_symbolic_oid(schema, kw);
		if (obj->oid == NULL)
			goto fail;
		free(kw);
	}
	kw = NULL;

	prev = RB_INSERT(object_tree, &schema->objects, obj);
	if (prev != NULL) {
		schema_err(schema, "object class %s already defined", obj->oid);
		goto fail;
	}

	while (ret == 0) {
		token = schema_lex(schema, &kw);
		if (token == ')')
			break;
		else if (token != STRING)
			goto fail;
		if (strcasecmp(kw, "NAME") == 0) {
			obj->names = schema_parse_names(schema);
			if (obj->names == NULL)
				goto fail;
			schema_link_obj_names(schema, obj);
		} else if (strcasecmp(kw, "DESC") == 0) {
			if (schema_lex(schema, &obj->desc) != STRING)
				goto fail;
		} else if (strcasecmp(kw, "OBSOLETE") == 0) {
			obj->obsolete = 1;
		} else if (strcasecmp(kw, "SUP") == 0) {
			obj->sup = schema_parse_objlist(schema);
			if (obj->sup == NULL)
				goto fail;
		} else if (strcasecmp(kw, "ABSTRACT") == 0) {
			obj->kind = KIND_ABSTRACT;
		} else if (strcasecmp(kw, "STRUCTURAL") == 0) {
			obj->kind = KIND_STRUCTURAL;
		} else if (strcasecmp(kw, "AUXILIARY") == 0) {
			obj->kind = KIND_AUXILIARY;
		} else if (strcasecmp(kw, "MUST") == 0) {
			obj->must = schema_parse_attrlist(schema);
			if (obj->must == NULL)
				goto fail;
		} else if (strcasecmp(kw, "MAY") == 0) {
			obj->may = schema_parse_attrlist(schema);
			if (obj->may == NULL)
				goto fail;
		}
	}

	/* Verify the subclassing is allowed.
	 *
	 * Structural object classes cannot subclass auxiliary object classes.
	 * Auxiliary object classes cannot subclass structural object classes.
	 * Abstract object classes cannot derive from structural or auxiliary
	 *   object classes.
	 */
	if (obj->sup != NULL) {
		SLIST_FOREACH(optr, obj->sup, next) {
			if (obj->kind == KIND_STRUCTURAL &&
			    optr->object->kind == KIND_AUXILIARY) {
				log_warnx("structural object class '%s' cannot"
				    " subclass auxiliary object class '%s'", 
				    OBJ_NAME(obj), OBJ_NAME(optr->object));
				goto fail;
			}

			if (obj->kind == KIND_AUXILIARY &&
			    optr->object->kind == KIND_STRUCTURAL) {
				log_warnx("auxiliary object class '%s' cannot"
				    " subclass structural object class '%s'", 
				    OBJ_NAME(obj), OBJ_NAME(optr->object));
				goto fail;
			}

			if (obj->kind == KIND_ABSTRACT &&
			    optr->object->kind != KIND_ABSTRACT) {
				log_warnx("abstract object class '%s' cannot"
				    " subclass non-abstract object class '%s'", 
				    OBJ_NAME(obj), OBJ_NAME(optr->object));
				goto fail;
			}
		}
	}

	return 0;

fail:
	free(kw);
	if (obj != NULL) {
		if (obj->oid != NULL) {
			RB_REMOVE(object_tree, &schema->objects, obj);
			free(obj->oid);
		}
		free(obj->desc);
		free(obj);
	}
	return -1;
}

static int
schema_parse_objectidentifier(struct schema *schema)
{
	char		*symname = NULL, *symoid = NULL;
	char		*oid = NULL;

	if (schema_lex(schema, &symname) != STRING)
		goto fail;
	if (schema_lex(schema, &symoid) != STRING)
		goto fail;

	if (is_oidstr(symoid)) {
		oid = symoid;
		symoid = NULL;
	} else if ((oid = lookup_symbolic_oid(schema, symoid)) == NULL)
		goto fail;

	if (push_symbolic_oid(schema, symname, oid) == NULL)
		goto fail;

	free(symoid);
	return 0;

fail:
	free(symname);
	free(symoid);
	free(oid);
	return -1;
}

int
schema_parse(struct schema *schema, const char *filename)
{
	char	*kw;
	int	 token, ret = 0;

	log_debug("parsing schema file '%s'", filename);

	if ((schema->fp = fopen(filename, "r")) == NULL)
		return -1;
	schema->filename = filename;
	schema->lineno = 1;

	while (ret == 0) {
		token = schema_lex(schema, &kw);
		if (token == STRING) {
			if (strcasecmp(kw, "attributetype") == 0)
				ret = schema_parse_attributetype(schema);
			else if (strcasecmp(kw, "objectclass") == 0)
				ret = schema_parse_objectclass(schema);
			else if (strcasecmp(kw, "objectidentifier") == 0)
				ret = schema_parse_objectidentifier(schema);
			else {
				schema_err(schema, "syntax error at '%s'", kw);
				ret = -1;
			}
			if (ret == -1 && schema->error == 0)
				schema_err(schema, "syntax error");
			free(kw);
		} else if (token == 0) {	/* EOF */
			break;
		} else {
			schema_err(schema, "syntax error");
			ret = -1;
		}
	}

	fclose(schema->fp);
	schema->fp = NULL;
	schema->filename = NULL;

	return ret;
}

