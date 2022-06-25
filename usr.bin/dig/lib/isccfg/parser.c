/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*! \file */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <isc/lex.h>
#include <isc/log.h>
#include <isc/symtab.h>
#include <isc/util.h>

#include <isccfg/cfg.h>
#include <isccfg/grammar.h>

/*!
 * Pass one of these flags to cfg_parser_error() to include the
 * token text in log message.
 */
#define CFG_LOG_NEAR    0x00000001	/*%< Say "near <token>" */
#define CFG_LOG_BEFORE  0x00000002	/*%< Say "before <token>" */
#define CFG_LOG_NOPREP  0x00000004	/*%< Say just "<token>" */

isc_logcategory_t cfg_category = { "config",     0 };
isc_logmodule_t cfg_module = { "isccfg/parser",      0 };

/* Shorthand */
#define CAT &cfg_category
#define MOD &cfg_module

#define MAP_SYM 1 	/* Unique type for isc_symtab */

#define TOKEN_STRING(pctx) (pctx->token.value.as_textregion.base)

#define CFG_LEXOPT_QSTRING (ISC_LEXOPT_QSTRING | ISC_LEXOPT_QSTRINGMULTILINE)

/* Check a return value. */
#define CHECK(op) 						\
	do { result = (op); 					\
		if (result != ISC_R_SUCCESS) goto cleanup; 	\
	} while (0)

/* Clean up a configuration object if non-NULL. */
#define CLEANUP_OBJ(obj) \
	do { if ((obj) != NULL) cfg_obj_destroy(pctx, &(obj)); } while (0)

/* Forward declarations of variables */
cfg_rep_t cfg_rep_string;
cfg_rep_t cfg_rep_list;

cfg_type_t cfg_type_qstring;
cfg_type_t cfg_type_sstring;
cfg_type_t cfg_type_token;
cfg_type_t cfg_type_unsupported;

/*
 * Forward declarations of static functions.
 */

static isc_result_t
cfg_gettoken(cfg_parser_t *pctx, int options);

static isc_result_t
cfg_peektoken(cfg_parser_t *pctx, int options);

static void
cfg_ungettoken(cfg_parser_t *pctx);

static isc_result_t
cfg_create_obj(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **objp);

static isc_result_t
cfg_parse_qstring(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static isc_result_t
cfg_parse_astring(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static isc_result_t
cfg_parse_sstring(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static isc_result_t
cfg_parse_special(cfg_parser_t *pctx, int special);
/*%< Parse a required special character 'special'. */

static isc_result_t
cfg_create_list(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **objp);

static isc_result_t
cfg_parse_listelt(cfg_parser_t *pctx, const cfg_type_t *elttype,
		  cfg_listelt_t **ret);

static isc_result_t
cfg_parse_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static isc_result_t
cfg_parse_obj(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static void
cfg_parser_error(cfg_parser_t *pctx, unsigned int flags,
		 const char *fmt, ...) __attribute__((__format__(__printf__, 3, 4)));

static void
free_list(cfg_parser_t *pctx, cfg_obj_t *obj);

static isc_result_t
create_listelt(cfg_parser_t *pctx, cfg_listelt_t **eltp);

static isc_result_t
create_string(cfg_parser_t *pctx, const char *contents, const cfg_type_t *type,
	      cfg_obj_t **ret);

static void
free_string(cfg_parser_t *pctx, cfg_obj_t *obj);

static isc_result_t
create_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **objp);

static void
free_map(cfg_parser_t *pctx, cfg_obj_t *obj);

static isc_result_t
parse_symtab_elt(cfg_parser_t *pctx, const char *name,
		 cfg_type_t *elttype, isc_symtab_t *symtab);

static isc_result_t
cfg_getstringtoken(cfg_parser_t *pctx);

static void
parser_complain(cfg_parser_t *pctx, int is_warning,
		unsigned int flags, const char *format, va_list args);

/*
 * Data representations.  These correspond to members of the
 * "value" union in struct cfg_obj (except "void", which does
 * not need a union member).
 */

cfg_rep_t cfg_rep_string = { "string", free_string };
cfg_rep_t cfg_rep_map = { "map", free_map };
cfg_rep_t cfg_rep_list = { "list", free_list };

/*
 * Configuration type definitions.
 */

/* Functions. */

static isc_result_t
cfg_parse_obj(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;

	REQUIRE(pctx != NULL);
	REQUIRE(type != NULL);
	REQUIRE(ret != NULL && *ret == NULL);

	result = type->parse(pctx, type, ret);
	if (result != ISC_R_SUCCESS)
		return (result);
	ENSURE(*ret != NULL);
	return (ISC_R_SUCCESS);
}

static isc_result_t
cfg_parse_special(cfg_parser_t *pctx, int special) {
	isc_result_t result;

	REQUIRE(pctx != NULL);

	CHECK(cfg_gettoken(pctx, 0));
	if (pctx->token.type == isc_tokentype_special &&
	    pctx->token.value.as_char == special)
		return (ISC_R_SUCCESS);

	cfg_parser_error(pctx, CFG_LOG_NEAR, "'%c' expected", special);
	return (ISC_R_UNEXPECTEDTOKEN);
 cleanup:
	return (result);
}

/*
 * Parse a required semicolon.  If it is not there, log
 * an error and increment the error count but continue
 * parsing.  Since the next token is pushed back,
 * care must be taken to make sure it is eventually
 * consumed or an infinite loop may result.
 */
static isc_result_t
parse_semicolon(cfg_parser_t *pctx) {
	isc_result_t result;

	CHECK(cfg_gettoken(pctx, 0));
	if (pctx->token.type == isc_tokentype_special &&
	    pctx->token.value.as_char == ';')
		return (ISC_R_SUCCESS);

	cfg_parser_error(pctx, CFG_LOG_BEFORE, "missing ';'");
	cfg_ungettoken(pctx);
 cleanup:
	return (result);
}

/*
 * Parse EOF, logging and returning an error if not there.
 */
static isc_result_t
parse_eof(cfg_parser_t *pctx) {
	isc_result_t result;

	CHECK(cfg_gettoken(pctx, 0));

	if (pctx->token.type == isc_tokentype_eof)
		return (ISC_R_SUCCESS);

	cfg_parser_error(pctx, CFG_LOG_NEAR, "syntax error");
	return (ISC_R_UNEXPECTEDTOKEN);
 cleanup:
	return (result);
}

/* A list of files, used internally for pctx->files. */

static cfg_type_t cfg_type_filelist = {
	"filelist", NULL, &cfg_rep_list,
	&cfg_type_qstring
};

isc_result_t
cfg_parser_create(isc_log_t *lctx, cfg_parser_t **ret) {
	isc_result_t result;
	cfg_parser_t *pctx;
	isc_lexspecials_t specials;

	REQUIRE(ret != NULL && *ret == NULL);

	pctx = malloc(sizeof(*pctx));
	if (pctx == NULL)
		return (ISC_R_NOMEMORY);

	pctx->lctx = lctx;
	pctx->lexer = NULL;
	pctx->seen_eof = 0;
	pctx->ungotten = 0;
	pctx->errors = 0;
	pctx->open_files = NULL;
	pctx->closed_files = NULL;
	pctx->line = 0;
	pctx->token.type = isc_tokentype_unknown;
	pctx->flags = 0;

	memset(specials, 0, sizeof(specials));
	specials['{'] = 1;
	specials['}'] = 1;
	specials[';'] = 1;
	specials['/'] = 1;
	specials['"'] = 1;
	specials['!'] = 1;

	CHECK(isc_lex_create(1024, &pctx->lexer));

	isc_lex_setspecials(pctx->lexer, specials);
	isc_lex_setcomments(pctx->lexer, (ISC_LEXCOMMENT_C |
					 ISC_LEXCOMMENT_CPLUSPLUS |
					 ISC_LEXCOMMENT_SHELL));

	CHECK(cfg_create_list(pctx, &cfg_type_filelist, &pctx->open_files));
	CHECK(cfg_create_list(pctx, &cfg_type_filelist, &pctx->closed_files));

	*ret = pctx;
	return (ISC_R_SUCCESS);

 cleanup:
	if (pctx->lexer != NULL)
		isc_lex_destroy(&pctx->lexer);
	CLEANUP_OBJ(pctx->open_files);
	CLEANUP_OBJ(pctx->closed_files);
	free(pctx);
	return (result);
}

static isc_result_t
parser_openfile(cfg_parser_t *pctx, const char *filename) {
	isc_result_t result;
	cfg_listelt_t *elt = NULL;
	cfg_obj_t *stringobj = NULL;

	result = isc_lex_openfile(pctx->lexer, filename);
	if (result != ISC_R_SUCCESS) {
		cfg_parser_error(pctx, 0, "open: %s: %s",
			     filename, isc_result_totext(result));
		goto cleanup;
	}

	CHECK(create_string(pctx, filename, &cfg_type_qstring, &stringobj));
	CHECK(create_listelt(pctx, &elt));
	elt->obj = stringobj;
	ISC_LIST_APPEND(pctx->open_files->value.list, elt, link);

	return (ISC_R_SUCCESS);
 cleanup:
	CLEANUP_OBJ(stringobj);
	return (result);
}

/*
 * Parse a configuration using a pctx where a lexer has already
 * been set up with a source.
 */
static isc_result_t
parse2(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;
	cfg_obj_t *obj = NULL;

	result = cfg_parse_obj(pctx, type, &obj);

	if (pctx->errors != 0) {
		/* Errors have been logged. */
		if (result == ISC_R_SUCCESS)
			result = ISC_R_FAILURE;
		goto cleanup;
	}

	if (result != ISC_R_SUCCESS) {
		/* Parsing failed but no errors have been logged. */
		cfg_parser_error(pctx, 0, "parsing failed: %s",
				 isc_result_totext(result));
		goto cleanup;
	}

	CHECK(parse_eof(pctx));

	*ret = obj;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(obj);
	return (result);
}

isc_result_t
cfg_parse_file(cfg_parser_t *pctx, const char *filename,
	       const cfg_type_t *type, cfg_obj_t **ret)
{
	isc_result_t result;

	REQUIRE(pctx != NULL);
	REQUIRE(filename != NULL);
	REQUIRE(type != NULL);
	REQUIRE(ret != NULL && *ret == NULL);

	CHECK(parser_openfile(pctx, filename));
	CHECK(parse2(pctx, type, ret));
 cleanup:
	return (result);
}

void
cfg_parser_destroy(cfg_parser_t **pctxp) {
	cfg_parser_t *pctx;

	REQUIRE(pctxp != NULL && *pctxp != NULL);

	pctx = *pctxp;
	*pctxp = NULL;

	isc_lex_destroy(&pctx->lexer);
	/*
	 * Cleaning up open_files does not
	 * close the files; that was already done
	 * by closing the lexer.
	 */
	CLEANUP_OBJ(pctx->open_files);
	CLEANUP_OBJ(pctx->closed_files);
	free(pctx);
}

/*
 * qstring (quoted string), ustring (unquoted string), astring
 * (any string)
 */

/* Create a string object from a null-terminated C string. */
static isc_result_t
create_string(cfg_parser_t *pctx, const char *contents, const cfg_type_t *type,
	      cfg_obj_t **ret)
{
	isc_result_t result;
	cfg_obj_t *obj = NULL;
	int len;

	CHECK(cfg_create_obj(pctx, type, &obj));
	len = strlen(contents);
	obj->value.string.length = len;
	obj->value.string.base = malloc(len + 1);
	if (obj->value.string.base == NULL) {
		free(obj);
		return (ISC_R_NOMEMORY);
	}
	memmove(obj->value.string.base, contents, len);
	obj->value.string.base[len] = '\0';

	*ret = obj;
 cleanup:
	return (result);
}

static isc_result_t
cfg_parse_qstring(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;

	REQUIRE(pctx != NULL);
	REQUIRE(ret != NULL && *ret == NULL);

	UNUSED(type);

	CHECK(cfg_gettoken(pctx, CFG_LEXOPT_QSTRING));
	if (pctx->token.type != isc_tokentype_qstring) {
		cfg_parser_error(pctx, CFG_LOG_NEAR, "expected quoted string");
		return (ISC_R_UNEXPECTEDTOKEN);
	}
	return (create_string(pctx, TOKEN_STRING(pctx),
			      &cfg_type_qstring, ret));
 cleanup:
	return (result);
}

static isc_result_t
cfg_parse_astring(cfg_parser_t *pctx, const cfg_type_t *type,
		  cfg_obj_t **ret)
{
	isc_result_t result;

	REQUIRE(pctx != NULL);
	REQUIRE(ret != NULL && *ret == NULL);

	UNUSED(type);

	CHECK(cfg_getstringtoken(pctx));
	return (create_string(pctx,
			      TOKEN_STRING(pctx),
			      &cfg_type_qstring,
			      ret));
 cleanup:
	return (result);
}

static isc_result_t
cfg_parse_sstring(cfg_parser_t *pctx, const cfg_type_t *type,
		  cfg_obj_t **ret)
{
	isc_result_t result;

	REQUIRE(pctx != NULL);
	REQUIRE(ret != NULL && *ret == NULL);

	UNUSED(type);

	CHECK(cfg_getstringtoken(pctx));
	return (create_string(pctx,
			      TOKEN_STRING(pctx),
			      &cfg_type_sstring,
			      ret));
 cleanup:
	return (result);
}

static void
free_string(cfg_parser_t *pctx, cfg_obj_t *obj) {
	UNUSED(pctx);
	free(obj->value.string.base);
}

const char *
cfg_obj_asstring(const cfg_obj_t *obj) {
	REQUIRE(obj != NULL && obj->type->rep == &cfg_rep_string);
	return (obj->value.string.base);
}

/* Quoted string only */
cfg_type_t cfg_type_qstring = {
	"quoted_string", cfg_parse_qstring, &cfg_rep_string, NULL
};

/* Any string (quoted or unquoted); printed with quotes */
cfg_type_t cfg_type_astring = {
	"string", cfg_parse_astring, &cfg_rep_string, NULL
};

/*
 * Any string (quoted or unquoted); printed with quotes.
 * If CFG_PRINTER_XKEY is set when printing the string will be '?' out.
 */
cfg_type_t cfg_type_sstring = {
	"string", cfg_parse_sstring, &cfg_rep_string, NULL
};

/*
 * Booleans
 */

/*
 * Lists.
 */

static isc_result_t
cfg_create_list(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **obj) {
	isc_result_t result;

	REQUIRE(pctx != NULL);
	REQUIRE(type != NULL);
	REQUIRE(obj != NULL && *obj == NULL);

	CHECK(cfg_create_obj(pctx, type, obj));
	ISC_LIST_INIT((*obj)->value.list);
 cleanup:
	return (result);
}

static isc_result_t
create_listelt(cfg_parser_t *pctx, cfg_listelt_t **eltp) {
	UNUSED(pctx);
	cfg_listelt_t *elt;

	elt = malloc(sizeof(*elt));
	if (elt == NULL)
		return (ISC_R_NOMEMORY);
	elt->obj = NULL;
	ISC_LINK_INIT(elt, link);
	*eltp = elt;
	return (ISC_R_SUCCESS);
}

static void
free_list_elt(cfg_parser_t *pctx, cfg_listelt_t *elt) {
	cfg_obj_destroy(pctx, &elt->obj);
	free(elt);
}

static void
free_list(cfg_parser_t *pctx, cfg_obj_t *obj) {
	cfg_listelt_t *elt, *next;
	for (elt = ISC_LIST_HEAD(obj->value.list);
	     elt != NULL;
	     elt = next)
	{
		next = ISC_LIST_NEXT(elt, link);
		free_list_elt(pctx, elt);
	}
}

static isc_result_t
cfg_parse_listelt(cfg_parser_t *pctx, const cfg_type_t *elttype,
		  cfg_listelt_t **ret)
{
	isc_result_t result;
	cfg_listelt_t *elt = NULL;
	cfg_obj_t *value = NULL;

	REQUIRE(pctx != NULL);
	REQUIRE(elttype != NULL);
	REQUIRE(ret != NULL && *ret == NULL);

	CHECK(create_listelt(pctx, &elt));

	result = cfg_parse_obj(pctx, elttype, &value);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	elt->obj = value;

	*ret = elt;
	return (ISC_R_SUCCESS);

 cleanup:
	free(elt);
	return (result);
}

/*
 * Maps.
 */

/*
 * Parse a map body.  That's something like
 *
 *   "foo 1; bar { glub; }; zap true; zap false;"
 *
 * i.e., a sequence of option names followed by values and
 * terminated by semicolons.  Used for the top level of
 * the named.conf syntax, as well as for the body of the
 * options, view, zone, and other statements.
 */
isc_result_t
cfg_parse_mapbody(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret)
{
	const cfg_clausedef_t * const *clausesets = type->of;
	isc_result_t result;
	const cfg_clausedef_t * const *clauseset;
	const cfg_clausedef_t *clause;
	cfg_obj_t *value = NULL;
	cfg_obj_t *obj = NULL;
	cfg_obj_t *eltobj = NULL;
	cfg_obj_t *includename = NULL;
	isc_symvalue_t symval;

	REQUIRE(pctx != NULL);
	REQUIRE(type != NULL);
	REQUIRE(ret != NULL && *ret == NULL);

	CHECK(create_map(pctx, type, &obj));

	obj->value.map.clausesets = clausesets;

	for (;;) {
	redo:
		/*
		 * Parse the option name and see if it is known.
		 */
		CHECK(cfg_gettoken(pctx, 0));

		if (pctx->token.type != isc_tokentype_string) {
			cfg_ungettoken(pctx);
			break;
		}

		/*
		 * We accept "include" statements wherever a map body
		 * clause can occur.
		 */
		if (strcasecmp(TOKEN_STRING(pctx), "include") == 0) {
			/*
			 * Turn the file name into a temporary configuration
			 * object just so that it is not overwritten by the
			 * semicolon token.
			 */
			CHECK(cfg_parse_obj(pctx, &cfg_type_qstring, &includename));
			CHECK(parse_semicolon(pctx));
			CHECK(parser_openfile(pctx, includename->
					      value.string.base));
			 cfg_obj_destroy(pctx, &includename);
			 goto redo;
		}

		clause = NULL;
		for (clauseset = clausesets; *clauseset != NULL; clauseset++) {
			for (clause = *clauseset;
			     clause->name != NULL;
			     clause++) {
				if (strcasecmp(TOKEN_STRING(pctx),
					   clause->name) == 0)
					goto done;
			}
		}
	done:
		if (clause == NULL || clause->name == NULL) {
			cfg_parser_error(pctx, CFG_LOG_NOPREP,
					 "unknown option");
			/*
			 * Try to recover by parsing this option as an unknown
			 * option and discarding it.
			 */
			CHECK(cfg_parse_obj(pctx, &cfg_type_unsupported,
					    &eltobj));
			cfg_obj_destroy(pctx, &eltobj);
			CHECK(parse_semicolon(pctx));
			continue;
		}

		/* Clause is known. */

		/* See if the clause already has a value; if not create one. */
		result = isc_symtab_lookup(obj->value.map.symtab,
					   clause->name, 0, &symval);

		/* Single-valued clause */
		if (result == ISC_R_NOTFOUND) {
			CHECK(parse_symtab_elt(pctx, clause->name,
					       clause->type,
					       obj->value.map.symtab));
			CHECK(parse_semicolon(pctx));
		} else if (result == ISC_R_SUCCESS) {
			cfg_parser_error(pctx, CFG_LOG_NEAR, "'%s' redefined",
				     clause->name);
			result = ISC_R_EXISTS;
			goto cleanup;
		} else {
			cfg_parser_error(pctx, CFG_LOG_NEAR,
				     "isc_symtab_define() failed");
			goto cleanup;
		}
	}

	*ret = obj;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(value);
	CLEANUP_OBJ(obj);
	CLEANUP_OBJ(eltobj);
	CLEANUP_OBJ(includename);
	return (result);
}

static isc_result_t
parse_symtab_elt(cfg_parser_t *pctx, const char *name,
		 cfg_type_t *elttype, isc_symtab_t *symtab)
{
	isc_result_t result;
	cfg_obj_t *obj = NULL;
	isc_symvalue_t symval;

	CHECK(cfg_parse_obj(pctx, elttype, &obj));

	symval.as_pointer = obj;
	CHECK(isc_symtab_define(symtab, name,
				1, symval,
				isc_symexists_reject));
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(obj);
	return (result);
}

/*
 * Parse a map; e.g., "{ foo 1; bar { glub; }; zap true; zap false; }"
 */
static isc_result_t
cfg_parse_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;

	REQUIRE(pctx != NULL);
	REQUIRE(type != NULL);
	REQUIRE(ret != NULL && *ret == NULL);

	CHECK(cfg_parse_special(pctx, '{'));
	CHECK(cfg_parse_mapbody(pctx, type, ret));
	CHECK(cfg_parse_special(pctx, '}'));
 cleanup:
	return (result);
}

/*
 * Subroutine for cfg_parse_named_map() and cfg_parse_addressed_map().
 */
static isc_result_t
parse_any_named_map(cfg_parser_t *pctx, cfg_type_t *nametype,
		    const cfg_type_t *type, cfg_obj_t **ret)
{
	isc_result_t result;
	cfg_obj_t *idobj = NULL;
	cfg_obj_t *mapobj = NULL;

	REQUIRE(pctx != NULL);
	REQUIRE(nametype != NULL);
	REQUIRE(type != NULL);
	REQUIRE(ret != NULL && *ret == NULL);

	CHECK(cfg_parse_obj(pctx, nametype, &idobj));
	CHECK(cfg_parse_map(pctx, type, &mapobj));
	mapobj->value.map.id = idobj;
	*ret = mapobj;
	return (result);
 cleanup:
	CLEANUP_OBJ(idobj);
	CLEANUP_OBJ(mapobj);
	return (result);
}

/*
 * Parse a map identified by a string name.  E.g., "name { foo 1; }".
 * Used for the "key" and "channel" statements.
 */
isc_result_t
cfg_parse_named_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	return (parse_any_named_map(pctx, &cfg_type_astring, type, ret));
}

isc_result_t
cfg_map_get(const cfg_obj_t *mapobj, const char* name, const cfg_obj_t **obj) {
	isc_result_t result;
	isc_symvalue_t val;
	const cfg_map_t *map;

	REQUIRE(mapobj != NULL && mapobj->type->rep == &cfg_rep_map);
	REQUIRE(name != NULL);
	REQUIRE(obj != NULL && *obj == NULL);

	map = &mapobj->value.map;

	result = isc_symtab_lookup(map->symtab, name, MAP_SYM, &val);
	if (result != ISC_R_SUCCESS)
		return (result);
	*obj = val.as_pointer;
	return (ISC_R_SUCCESS);
}

const cfg_obj_t *
cfg_map_getname(const cfg_obj_t *mapobj) {
	REQUIRE(mapobj != NULL && mapobj->type->rep == &cfg_rep_map);
	return (mapobj->value.map.id);
}

/* Parse an arbitrary token, storing its raw text representation. */
static isc_result_t
parse_token(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	cfg_obj_t *obj = NULL;
	isc_result_t result;
	isc_region_t r;

	UNUSED(type);

	CHECK(cfg_create_obj(pctx, &cfg_type_token, &obj));
	CHECK(cfg_gettoken(pctx, CFG_LEXOPT_QSTRING));
	if (pctx->token.type == isc_tokentype_eof) {
		cfg_ungettoken(pctx);
		result = ISC_R_EOF;
		goto cleanup;
	}

	isc_lex_getlasttokentext(pctx->lexer, &pctx->token, &r);

	obj->value.string.base = malloc(r.length + 1);
	if (obj->value.string.base == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}
	obj->value.string.length = r.length;
	memmove(obj->value.string.base, r.base, r.length);
	obj->value.string.base[r.length] = '\0';
	*ret = obj;
	return (result);

 cleanup:
	if (obj != NULL)
		free(obj);
	return (result);
}

cfg_type_t cfg_type_token = {
	"token", parse_token, &cfg_rep_string, NULL
};

/*
 * An unsupported option.  This is just a list of tokens with balanced braces
 * ending in a semicolon.
 */

static isc_result_t
parse_unsupported(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	cfg_obj_t *listobj = NULL;
	isc_result_t result;
	int braces = 0;

	CHECK(cfg_create_list(pctx, type, &listobj));

	for (;;) {
		cfg_listelt_t *elt = NULL;

		CHECK(cfg_peektoken(pctx, 0));
		if (pctx->token.type == isc_tokentype_special) {
			if (pctx->token.value.as_char == '{')
				braces++;
			else if (pctx->token.value.as_char == '}')
				braces--;
			else if (pctx->token.value.as_char == ';')
				if (braces == 0)
					break;
		}
		if (pctx->token.type == isc_tokentype_eof || braces < 0) {
			cfg_parser_error(pctx, CFG_LOG_NEAR, "unexpected token");
			result = ISC_R_UNEXPECTEDTOKEN;
			goto cleanup;
		}

		CHECK(cfg_parse_listelt(pctx, &cfg_type_token, &elt));
		ISC_LIST_APPEND(listobj->value.list, elt, link);
	}
	INSIST(braces == 0);
	*ret = listobj;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(listobj);
	return (result);
}

cfg_type_t cfg_type_unsupported = {
	"unsupported", parse_unsupported, &cfg_rep_list, NULL
};

static isc_result_t
cfg_gettoken(cfg_parser_t *pctx, int options) {
	isc_result_t result;

	REQUIRE(pctx != NULL);

	if (pctx->seen_eof)
		return (ISC_R_SUCCESS);

	options |= (ISC_LEXOPT_EOF | ISC_LEXOPT_NOMORE);

 redo:
	pctx->token.type = isc_tokentype_unknown;
	result = isc_lex_gettoken(pctx->lexer, options, &pctx->token);
	pctx->ungotten = 0;
	pctx->line = isc_lex_getsourceline(pctx->lexer);

	switch (result) {
	case ISC_R_SUCCESS:
		if (pctx->token.type == isc_tokentype_eof) {
			result = isc_lex_close(pctx->lexer);
			INSIST(result == ISC_R_NOMORE ||
			       result == ISC_R_SUCCESS);

			if (isc_lex_getsourcename(pctx->lexer) != NULL) {
				/*
				 * Closed an included file, not the main file.
				 */
				cfg_listelt_t *elt;
				elt = ISC_LIST_TAIL(pctx->open_files->
						    value.list);
				INSIST(elt != NULL);
				ISC_LIST_UNLINK(pctx->open_files->
						value.list, elt, link);
				ISC_LIST_APPEND(pctx->closed_files->
						value.list, elt, link);
				goto redo;
			}
			pctx->seen_eof = 1;
		}
		break;

	case ISC_R_NOSPACE:
		/* More understandable than "ran out of space". */
		cfg_parser_error(pctx, CFG_LOG_NEAR, "token too big");
		break;

	case ISC_R_IOERROR:
		cfg_parser_error(pctx, 0, "%s",
				 isc_result_totext(result));
		break;

	default:
		cfg_parser_error(pctx, CFG_LOG_NEAR, "%s",
				 isc_result_totext(result));
		break;
	}
	return (result);
}

static void
cfg_ungettoken(cfg_parser_t *pctx) {
	REQUIRE(pctx != NULL);

	if (pctx->seen_eof)
		return;
	isc_lex_ungettoken(pctx->lexer, &pctx->token);
	pctx->ungotten = 1;
}

static isc_result_t
cfg_peektoken(cfg_parser_t *pctx, int options) {
	isc_result_t result;

	REQUIRE(pctx != NULL);

	CHECK(cfg_gettoken(pctx, options));
	cfg_ungettoken(pctx);
 cleanup:
	return (result);
}

/*
 * Get a string token, accepting both the quoted and the unquoted form.
 * Log an error if the next token is not a string.
 */
static isc_result_t
cfg_getstringtoken(cfg_parser_t *pctx) {
	isc_result_t result;

	result = cfg_gettoken(pctx, CFG_LEXOPT_QSTRING);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (pctx->token.type != isc_tokentype_string &&
	    pctx->token.type != isc_tokentype_qstring) {
		cfg_parser_error(pctx, CFG_LOG_NEAR, "expected string");
		return (ISC_R_UNEXPECTEDTOKEN);
	}
	return (ISC_R_SUCCESS);
}

static void
cfg_parser_error(cfg_parser_t *pctx, unsigned int flags, const char *fmt, ...) {
	va_list args;

	REQUIRE(pctx != NULL);
	REQUIRE(fmt != NULL);

	va_start(args, fmt);
	parser_complain(pctx, 0, flags, fmt, args);
	va_end(args);
	pctx->errors++;
}

#define MAX_LOG_TOKEN 30 /* How much of a token to quote in log messages. */

static int
have_current_file(cfg_parser_t *pctx) {
	cfg_listelt_t *elt;
	if (pctx->open_files == NULL)
		return (0);

	elt = ISC_LIST_TAIL(pctx->open_files->value.list);
	if (elt == NULL)
	      return (0);

	return (1);
}

static char *
current_file(cfg_parser_t *pctx) {
	static char none[] = "none";
	cfg_listelt_t *elt;
	cfg_obj_t *fileobj;

	if (!have_current_file(pctx))
		return (none);

	elt = ISC_LIST_TAIL(pctx->open_files->value.list);
	if (elt == NULL)	/* shouldn't be possible, but... */
	      return (none);

	fileobj = elt->obj;
	INSIST(fileobj->type == &cfg_type_qstring);
	return (fileobj->value.string.base);
}

static void
parser_complain(cfg_parser_t *pctx, int is_warning,
		unsigned int flags, const char *format,
		va_list args)
{
	char tokenbuf[MAX_LOG_TOKEN + 10];
	static char where[PATH_MAX + 100];
	static char message[2048];
	int level = ISC_LOG_ERROR;
	const char *prep = "";
	size_t len;

	if (is_warning)
		level = ISC_LOG_WARNING;

	where[0] = '\0';
	if (have_current_file(pctx))
		snprintf(where, sizeof(where), "%s:%u: ",
			 current_file(pctx), pctx->line);

	len = vsnprintf(message, sizeof(message), format, args);
#define ELIPSIS " ... "
	if (len >= sizeof(message)) {
		message[sizeof(message) - sizeof(ELIPSIS)] = 0;
		strlcat(message, ELIPSIS, sizeof(message));
	}

	if ((flags & (CFG_LOG_NEAR|CFG_LOG_BEFORE|CFG_LOG_NOPREP)) != 0) {
		isc_region_t r;

		if (pctx->ungotten)
			(void)cfg_gettoken(pctx, 0);

		if (pctx->token.type == isc_tokentype_eof) {
			snprintf(tokenbuf, sizeof(tokenbuf), "end of file");
		} else if (pctx->token.type == isc_tokentype_unknown) {
			flags = 0;
			tokenbuf[0] = '\0';
		} else {
			isc_lex_getlasttokentext(pctx->lexer,
						 &pctx->token, &r);
			if (r.length > MAX_LOG_TOKEN)
				snprintf(tokenbuf, sizeof(tokenbuf),
					 "'%.*s...'", MAX_LOG_TOKEN, r.base);
			else
				snprintf(tokenbuf, sizeof(tokenbuf),
					 "'%.*s'", (int)r.length, r.base);
		}

		/* Choose a preposition. */
		if (flags & CFG_LOG_NEAR)
			prep = " near ";
		else if (flags & CFG_LOG_BEFORE)
			prep = " before ";
		else
			prep = " ";
	} else {
		tokenbuf[0] = '\0';
	}
	isc_log_write(pctx->lctx, CAT, MOD, level,
		      "%s%s%s%s", where, message, prep, tokenbuf);
}

static isc_result_t
cfg_create_obj(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	cfg_obj_t *obj;

	REQUIRE(pctx != NULL);
	REQUIRE(type != NULL);
	REQUIRE(ret != NULL && *ret == NULL);

	obj = malloc(sizeof(cfg_obj_t));
	if (obj == NULL)
		return (ISC_R_NOMEMORY);
	obj->type = type;
	obj->file = current_file(pctx);
	obj->line = pctx->line;
	*ret = obj;
	return (ISC_R_SUCCESS);
}

static void
map_symtabitem_destroy(char *key, unsigned int type,
		       isc_symvalue_t symval, void *userarg)
{
	cfg_obj_t *obj = symval.as_pointer;
	cfg_parser_t *pctx = (cfg_parser_t *)userarg;

	UNUSED(key);
	UNUSED(type);

	cfg_obj_destroy(pctx, &obj);
}

static isc_result_t
create_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;
	isc_symtab_t *symtab = NULL;
	cfg_obj_t *obj = NULL;

	CHECK(cfg_create_obj(pctx, type, &obj));
	CHECK(isc_symtab_create(5, /* XXX */
				map_symtabitem_destroy,
				pctx, 0, &symtab));
	obj->value.map.symtab = symtab;
	obj->value.map.id = NULL;

	*ret = obj;
	return (ISC_R_SUCCESS);

 cleanup:
	if (obj != NULL)
		free(obj);
	return (result);
}

static void
free_map(cfg_parser_t *pctx, cfg_obj_t *obj) {
	CLEANUP_OBJ(obj->value.map.id);
	isc_symtab_destroy(&obj->value.map.symtab);
}

/*
 * Destroy 'obj', a configuration object created in 'pctx'.
 */
void
cfg_obj_destroy(cfg_parser_t *pctx, cfg_obj_t **objp) {
	cfg_obj_t *obj;

	REQUIRE(objp != NULL && *objp != NULL);
	REQUIRE(pctx != NULL);

	obj = *objp;

	obj->type->rep->free(pctx, obj);
	free(obj);
	*objp = NULL;
}
