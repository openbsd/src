/*
 * Copyright (C) 2000-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: parser.c,v 1.70.2.14.4.2 2003/02/17 07:05:10 marka Exp $ */

#include <config.h>

#include <isc/buffer.h>
#include <isc/dir.h>
#include <isc/formatcheck.h>
#include <isc/lex.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/net.h>
#include <isc/netaddr.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/sockaddr.h>
#include <isc/util.h>
#include <isc/symtab.h>

#include <isccfg/cfg.h>
#include <isccfg/log.h>

/* Shorthand */
#define CAT CFG_LOGCATEGORY_CONFIG
#define MOD CFG_LOGMODULE_PARSER

#define QSTRING (ISC_LEXOPT_QSTRING | ISC_LEXOPT_QSTRINGMULTILINE)

/*
 * Pass one of these flags to parser_error() to include the
 * token text in log message.
 */
#define LOG_NEAR    0x00000001	/* Say "near <token>" */
#define LOG_BEFORE  0x00000002	/* Say "before <token>" */
#define LOG_NOPREP  0x00000004	/* Say just "<token>" */

#define MAP_SYM 1 	/* Unique type for isc_symtab */

/* Clause may occur multiple times (e.g., "zone") */
#define CFG_CLAUSEFLAG_MULTI 		0x00000001
/* Clause is obsolete */
#define CFG_CLAUSEFLAG_OBSOLETE 	0x00000002
/* Clause is not implemented, and may never be */
#define CFG_CLAUSEFLAG_NOTIMP	 	0x00000004
/* Clause is not implemented yet */
#define CFG_CLAUSEFLAG_NYI 		0x00000008
/* Default value has changed since earlier release */
#define CFG_CLAUSEFLAG_NEWDEFAULT	0x00000010
/*
 * Clause needs to be interpreted during parsing
 * by calling a callback function, like the
 * "directory" option.
 */
#define CFG_CLAUSEFLAG_CALLBACK		0x00000020

/*
 * Flags defining whether to accept certain types of network addresses.
 */
#define V4OK 		0x00000001
#define V4PREFIXOK 	0x00000002
#define V6OK 		0x00000004
#define WILDOK		0x00000008

/* Check a return value. */
#define CHECK(op) 						\
     	do { result = (op); 					\
		if (result != ISC_R_SUCCESS) goto cleanup; 	\
	} while (0)

/* Clean up a configuration object if non-NULL. */
#define CLEANUP_OBJ(obj) \
	do { if ((obj) != NULL) cfg_obj_destroy(pctx, &(obj)); } while (0)


typedef struct cfg_clausedef cfg_clausedef_t;
typedef struct cfg_tuplefielddef cfg_tuplefielddef_t;
typedef struct cfg_printer cfg_printer_t;
typedef ISC_LIST(cfg_listelt_t) cfg_list_t;
typedef struct cfg_map cfg_map_t;
typedef struct cfg_rep cfg_rep_t;

/*
 * Function types for configuration object methods
 */

typedef isc_result_t (*cfg_parsefunc_t)(cfg_parser_t *, const cfg_type_t *type,
					cfg_obj_t **);
typedef void	     (*cfg_printfunc_t)(cfg_printer_t *, cfg_obj_t *);
typedef void	     (*cfg_freefunc_t)(cfg_parser_t *, cfg_obj_t *);


/*
 * Structure definitions
 */

/* The parser object. */
struct cfg_parser {
	isc_mem_t *	mctx;
	isc_log_t *	lctx;
	isc_lex_t *	lexer;
	unsigned int    errors;
	unsigned int    warnings;
	isc_token_t     token;

	/* We are at the end of all input. */
	isc_boolean_t	seen_eof;

	/* The current token has been pushed back. */
	isc_boolean_t	ungotten;

	/*
	 * The stack of currently active files, represented
	 * as a configuration list of configuration strings.
	 * The head is the top-level file, subsequent elements 
	 * (if any) are the nested include files, and the 
	 * last element is the file currently being parsed.
	 */
	cfg_obj_t *	open_files;

	/*
	 * Names of files that we have parsed and closed
	 * and were previously on the open_file list.
	 * We keep these objects around after closing
	 * the files because the file names may still be
	 * referenced from other configuration objects
	 * for use in reporting semantic errors after
	 * parsing is complete.
	 */
	cfg_obj_t *	closed_files;

	/*
	 * Current line number.  We maintain our own
	 * copy of this so that it is available even
	 * when a file has just been closed.
	 */
	unsigned int	line;

	cfg_parsecallback_t callback;
	void *callbackarg;
};

/*
 * A configuration printer object.  This is an abstract
 * interface to a destination to which text can be printed
 * by calling the function 'f'.
 */
struct cfg_printer {
	void (*f)(void *closure, const char *text, int textlen);
	void *closure;
	int indent;
};

/* A clause definition. */

struct cfg_clausedef {
	const char      *name;
	cfg_type_t      *type;
	unsigned int	flags;
};

/* A tuple field definition. */

struct cfg_tuplefielddef {
	const char      *name;
	cfg_type_t      *type;
	unsigned int	flags;
};

/* A configuration object type definition. */
struct cfg_type {
	const char *name;	/* For debugging purposes only */
	cfg_parsefunc_t  parse;
	cfg_printfunc_t  print;
	cfg_rep_t *	 rep;	/* Data representation */
	const void *	 of;	/* For meta-types */
};

/* A keyword-type definition, for things like "port <integer>". */

typedef struct {
	const char *name;
	const cfg_type_t *type;
} keyword_type_t;

struct cfg_map {
	cfg_obj_t	 *id; /* Used for 'named maps' like keys, zones, &c */
	const cfg_clausedef_t * const *clausesets; /* The clauses that
						      can occur in this map;
						      used for printing */
	isc_symtab_t     *symtab;
};

typedef struct cfg_netprefix cfg_netprefix_t;

struct cfg_netprefix {
	isc_netaddr_t address; /* IP4/IP6 */
	unsigned int prefixlen;
};

/*
 * A configuration data representation.
 */
struct cfg_rep {
	const char *	name;	/* For debugging only */
	cfg_freefunc_t 	free;	/* How to free this kind of data. */
};

/*
 * A configuration object.  This is the main building block
 * of the configuration parse tree.
 */

struct cfg_obj {
	const cfg_type_t *type;
	union {
		isc_uint32_t  	uint32;
		isc_uint64_t  	uint64;
		isc_textregion_t string; /* null terminated, too */
		isc_boolean_t 	boolean;
		cfg_map_t	map;
		cfg_list_t	list;
		cfg_obj_t **	tuple;
		isc_sockaddr_t	sockaddr;
		cfg_netprefix_t netprefix;
	}               value;
	char *		file;
	unsigned int    line;
};


/* A list element. */

struct cfg_listelt {
	cfg_obj_t               *obj;
	ISC_LINK(cfg_listelt_t)  link;
};

/*
 * Forward declarations of static functions.
 */

static isc_result_t
create_cfgobj(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **objp);

static isc_result_t
create_list(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **objp);

static isc_result_t
create_listelt(cfg_parser_t *pctx, cfg_listelt_t **eltp);

static void
free_list(cfg_parser_t *pctx, cfg_obj_t *obj);

static isc_result_t
create_string(cfg_parser_t *pctx, const char *contents, const cfg_type_t *type,
	      cfg_obj_t **ret);

static void
free_string(cfg_parser_t *pctx, cfg_obj_t *obj);

static isc_result_t
create_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **objp);

static isc_result_t
create_tuple(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **objp);

static void
free_map(cfg_parser_t *pctx, cfg_obj_t *obj);

static isc_result_t
get_addr(cfg_parser_t *pctx, unsigned int flags, isc_netaddr_t *na);

static void
print(cfg_printer_t *pctx, const char *text, int len);

static void
print_void(cfg_printer_t *pctx, cfg_obj_t *obj);

static isc_result_t
parse_enum_or_other(cfg_parser_t *pctx, const cfg_type_t *enumtype,
		    const cfg_type_t *othertype, cfg_obj_t **ret);

static isc_result_t
parse_mapbody(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static void
print_mapbody(cfg_printer_t *pctx, cfg_obj_t *obj);

static isc_result_t
parse_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static void
print_map(cfg_printer_t *pctx, cfg_obj_t *obj);

static isc_result_t
parse_named_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static isc_result_t
parse_addressed_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static isc_result_t
parse_list(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static void
print_list(cfg_printer_t *pctx, cfg_obj_t *obj);

static isc_result_t
parse_tuple(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static void
print_tuple(cfg_printer_t *pctx, cfg_obj_t *obj);

static void
free_tuple(cfg_parser_t *pctx, cfg_obj_t *obj);

static isc_result_t
parse_spacelist(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static void
print_spacelist(cfg_printer_t *pctx, cfg_obj_t *obj);

static void
print_sockaddr(cfg_printer_t *pctx, cfg_obj_t *obj);

static isc_result_t
parse_addrmatchelt(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static isc_result_t
parse_bracketed_list(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static void
print_bracketed_list(cfg_printer_t *pctx, cfg_obj_t *obj);

static isc_result_t
parse_keyvalue(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static isc_result_t
parse_optional_keyvalue(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

static void
print_keyvalue(cfg_printer_t *pctx, cfg_obj_t *obj);

static isc_result_t
parse_symtab_elt(cfg_parser_t *pctx, const char *name,
		 cfg_type_t *elttype, isc_symtab_t *symtab,
		 isc_boolean_t callback);

static void
free_noop(cfg_parser_t *pctx, cfg_obj_t *obj);

static isc_result_t
cfg_gettoken(cfg_parser_t *pctx, int options);

static void
cfg_ungettoken(cfg_parser_t *pctx);

static isc_result_t
cfg_peektoken(cfg_parser_t *pctx, int options);

static isc_result_t
cfg_getstringtoken(cfg_parser_t *pctx);

static void
parser_error(cfg_parser_t *pctx, unsigned int flags,
	     const char *fmt, ...) ISC_FORMAT_PRINTF(3, 4);

static void
parser_warning(cfg_parser_t *pctx, unsigned int flags,
	       const char *fmt, ...) ISC_FORMAT_PRINTF(3, 4);

static void
parser_complain(cfg_parser_t *pctx, isc_boolean_t is_warning,
		unsigned int flags, const char *format, va_list args);

static void
print_uint32(cfg_printer_t *pctx, cfg_obj_t *obj);

static void
print_ustring(cfg_printer_t *pctx, cfg_obj_t *obj);

static isc_result_t
parse_enum(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret);

/*
 * Data representations.  These correspond to members of the
 * "value" union in struct cfg_obj (except "void", which does
 * not need a union member).
 */

cfg_rep_t cfg_rep_uint32 = { "uint32", free_noop };
cfg_rep_t cfg_rep_uint64 = { "uint64", free_noop };
cfg_rep_t cfg_rep_string = { "string", free_string };
cfg_rep_t cfg_rep_boolean = { "boolean", free_noop };
cfg_rep_t cfg_rep_map = { "map", free_map };
cfg_rep_t cfg_rep_list = { "list", free_list };
cfg_rep_t cfg_rep_tuple = { "tuple", free_tuple };
cfg_rep_t cfg_rep_sockaddr = { "sockaddr", free_noop };
cfg_rep_t cfg_rep_netprefix = { "netprefix", free_noop };
cfg_rep_t cfg_rep_void = { "void", free_noop };

/*
 * Forward declarations of configuration type definitions.
 * Additional types are declared publicly in cfg.h.
 */

static cfg_type_t cfg_type_boolean;
static cfg_type_t cfg_type_uint32;
static cfg_type_t cfg_type_qstring;
static cfg_type_t cfg_type_astring;
static cfg_type_t cfg_type_ustring;
static cfg_type_t cfg_type_optional_port;
static cfg_type_t cfg_type_bracketed_aml;
static cfg_type_t cfg_type_acl;
static cfg_type_t cfg_type_portiplist;
static cfg_type_t cfg_type_bracketed_sockaddrlist;
static cfg_type_t cfg_type_sockaddr;
static cfg_type_t cfg_type_netaddr;
static cfg_type_t cfg_type_optional_keyref;
static cfg_type_t cfg_type_options;
static cfg_type_t cfg_type_view;
static cfg_type_t cfg_type_viewopts;
static cfg_type_t cfg_type_key;
static cfg_type_t cfg_type_server;
static cfg_type_t cfg_type_controls;
static cfg_type_t cfg_type_bracketed_sockaddrkeylist;
static cfg_type_t cfg_type_querysource4;
static cfg_type_t cfg_type_querysource6;
static cfg_type_t cfg_type_querysource;
static cfg_type_t cfg_type_sockaddr4wild;
static cfg_type_t cfg_type_sockaddr6wild;
static cfg_type_t cfg_type_sockaddr;
static cfg_type_t cfg_type_netprefix;
static cfg_type_t cfg_type_zone;
static cfg_type_t cfg_type_zoneopts;
static cfg_type_t cfg_type_logging;
static cfg_type_t cfg_type_optional_facility;
static cfg_type_t cfg_type_void;
static cfg_type_t cfg_type_optional_class;
static cfg_type_t cfg_type_destinationlist;
static cfg_type_t cfg_type_size;
static cfg_type_t cfg_type_sizenodefault;
static cfg_type_t cfg_type_negated;
static cfg_type_t cfg_type_addrmatchelt;
static cfg_type_t cfg_type_unsupported;
static cfg_type_t cfg_type_token;
static cfg_type_t cfg_type_server_key_kludge;
static cfg_type_t cfg_type_optional_facility;
static cfg_type_t cfg_type_logseverity;
static cfg_type_t cfg_type_logfile;
static cfg_type_t cfg_type_lwres;
static cfg_type_t cfg_type_controls_sockaddr;
static cfg_type_t cfg_type_notifytype;
static cfg_type_t cfg_type_dialuptype;

/*
 * Configuration type definitions.
 */

/* tkey-dhkey */

static cfg_tuplefielddef_t tkey_dhkey_fields[] = {
	{ "name", &cfg_type_qstring, 0 },
	{ "keyid", &cfg_type_uint32, 0 },
	{ NULL, NULL, 0 }
};

static cfg_type_t cfg_type_tkey_dhkey = {
	"tkey-dhkey", parse_tuple, print_tuple, &cfg_rep_tuple,
	tkey_dhkey_fields
};

/* listen-on */

static cfg_tuplefielddef_t listenon_fields[] = {
	{ "port", &cfg_type_optional_port, 0 },
	{ "acl", &cfg_type_bracketed_aml, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_listenon = {
	"listenon", parse_tuple, print_tuple, &cfg_rep_tuple, listenon_fields };

/* acl */

static cfg_tuplefielddef_t acl_fields[] = {
	{ "name", &cfg_type_astring, 0 },
	{ "value", &cfg_type_bracketed_aml, 0 },
	{ NULL, NULL, 0 }
};

static cfg_type_t cfg_type_acl = {
	"acl", parse_tuple, print_tuple, &cfg_rep_tuple, acl_fields };


/*
 * "sockaddrkeylist", a list of socket addresses with optional keys
 * and an optional default port, as used in the masters option.
 * E.g.,
 *   "port 1234 { 10.0.0.1 key foo; 1::2 port 69; }"
 */

static cfg_tuplefielddef_t sockaddrkey_fields[] = {
	{ "sockaddr", &cfg_type_sockaddr, 0 },
	{ "key", &cfg_type_optional_keyref, 0 },
	{ NULL, NULL, 0 },
};

static cfg_type_t cfg_type_sockaddrkey = {
	"sockaddrkey", parse_tuple, print_tuple, &cfg_rep_tuple,
	sockaddrkey_fields
};

static cfg_type_t cfg_type_bracketed_sockaddrkeylist = {
	"bracketed_sockaddrkeylist", parse_bracketed_list,
	print_bracketed_list, &cfg_rep_list, &cfg_type_sockaddrkey
};

static cfg_tuplefielddef_t sockaddrkeylist_fields[] = {
	{ "port", &cfg_type_optional_port, 0 },
	{ "addresses", &cfg_type_bracketed_sockaddrkeylist, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_sockaddrkeylist = {
	"sockaddrkeylist", parse_tuple, print_tuple, &cfg_rep_tuple,
	sockaddrkeylist_fields
};

/*
 * A list of socket addresses with an optional default port,
 * as used in the also-notify option.  E.g.,
 * "port 1234 { 10.0.0.1; 1::2 port 69; }"
 */
static cfg_tuplefielddef_t portiplist_fields[] = {
	{ "port", &cfg_type_optional_port, 0 },
	{ "addresses", &cfg_type_bracketed_sockaddrlist, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_portiplist = {
	"portiplist", parse_tuple, print_tuple, &cfg_rep_tuple,
	portiplist_fields
};

/*
 * A public key, as in the "pubkey" statement.
 */
static cfg_tuplefielddef_t pubkey_fields[] = {
	{ "flags", &cfg_type_uint32, 0 },
	{ "protocol", &cfg_type_uint32, 0 },
	{ "algorithm", &cfg_type_uint32, 0 },
	{ "key", &cfg_type_qstring, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_pubkey = {
	"pubkey", parse_tuple, print_tuple, &cfg_rep_tuple, pubkey_fields };


/*
 * A list of RR types, used in grant statements.
 * Note that the old parser allows quotes around the RR type names.
 */
static cfg_type_t cfg_type_rrtypelist = {
	"rrtypelist", parse_spacelist, print_spacelist, &cfg_rep_list,
	&cfg_type_astring
};

static const char *mode_enums[] = { "grant", "deny", NULL };
static cfg_type_t cfg_type_mode = {
	"mode", parse_enum, print_ustring, &cfg_rep_string,
	&mode_enums
};

static const char *matchtype_enums[] = {
	"name", "subdomain", "wildcard", "self", NULL };
static cfg_type_t cfg_type_matchtype = {
	"matchtype", parse_enum, print_ustring, &cfg_rep_string,
	&matchtype_enums
};

/*
 * A grant statement, used in the update policy.
 */
static cfg_tuplefielddef_t grant_fields[] = {
	{ "mode", &cfg_type_mode, 0 },
	{ "identity", &cfg_type_astring, 0 }, /* domain name */ 
	{ "matchtype", &cfg_type_matchtype, 0 },
	{ "name", &cfg_type_astring, 0 }, /* domain name */
	{ "types", &cfg_type_rrtypelist, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_grant = {
	"grant", parse_tuple, print_tuple, &cfg_rep_tuple, grant_fields };

static cfg_type_t cfg_type_updatepolicy = {
	"update_policy", parse_bracketed_list, print_bracketed_list,
	&cfg_rep_list, &cfg_type_grant
};

/*
 * A view statement.
 */
static cfg_tuplefielddef_t view_fields[] = {
	{ "name", &cfg_type_astring, 0 },
	{ "class", &cfg_type_optional_class, 0 },
	{ "options", &cfg_type_viewopts, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_view = {
	"view", parse_tuple, print_tuple, &cfg_rep_tuple, view_fields };

/*
 * A zone statement.
 */
static cfg_tuplefielddef_t zone_fields[] = {
	{ "name", &cfg_type_astring, 0 },
	{ "class", &cfg_type_optional_class, 0 },
	{ "options", &cfg_type_zoneopts, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_zone = {
	"zone", parse_tuple, print_tuple, &cfg_rep_tuple, zone_fields };

/*
 * A "category" clause in the "logging" statement.
 */
static cfg_tuplefielddef_t category_fields[] = {
	{ "name", &cfg_type_astring, 0 },
	{ "destinations", &cfg_type_destinationlist,0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_category = {
	"category", parse_tuple, print_tuple, &cfg_rep_tuple, category_fields };


/*
 * A trusted key, as used in the "trusted-keys" statement.
 */
static cfg_tuplefielddef_t trustedkey_fields[] = {
	{ "name", &cfg_type_astring, 0 },
	{ "flags", &cfg_type_uint32, 0 },
	{ "protocol", &cfg_type_uint32, 0 },
	{ "algorithm", &cfg_type_uint32, 0 },
	{ "key", &cfg_type_qstring, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_trustedkey = {
	"trustedkey", parse_tuple, print_tuple, &cfg_rep_tuple,
	trustedkey_fields
};


static keyword_type_t wild_class_kw = { "class", &cfg_type_ustring };

static cfg_type_t cfg_type_optional_wild_class = {
	"optional_wild_class", parse_optional_keyvalue,
	print_keyvalue, &cfg_rep_string, &wild_class_kw
};

static keyword_type_t wild_type_kw = { "type", &cfg_type_ustring };

static cfg_type_t cfg_type_optional_wild_type = {
	"optional_wild_type", parse_optional_keyvalue,
	print_keyvalue, &cfg_rep_string, &wild_type_kw
};

static keyword_type_t wild_name_kw = { "name", &cfg_type_qstring };

static cfg_type_t cfg_type_optional_wild_name = {
	"optional_wild_name", parse_optional_keyvalue,
	print_keyvalue, &cfg_rep_string, &wild_name_kw
};

/*
 * An rrset ordering element.
 */
static cfg_tuplefielddef_t rrsetorderingelement_fields[] = {
	{ "class", &cfg_type_optional_wild_class, 0 },
	{ "type", &cfg_type_optional_wild_type, 0 },
	{ "name", &cfg_type_optional_wild_name, 0 },
	{ "order", &cfg_type_ustring, 0 }, /* must be literal "order" */ 
	{ "ordering", &cfg_type_ustring, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_rrsetorderingelement = {
	"rrsetorderingelement", parse_tuple, print_tuple, &cfg_rep_tuple,
	rrsetorderingelement_fields
};

/*
 * A global or view "check-names" option.  Note that the zone
 * "check-names" option has a different syntax.
 */
static cfg_tuplefielddef_t checknames_fields[] = {
	{ "type", &cfg_type_ustring, 0 },
	{ "mode", &cfg_type_ustring, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_checknames = {
	"checknames", parse_tuple, print_tuple, &cfg_rep_tuple,
	checknames_fields
};

static cfg_type_t cfg_type_bracketed_sockaddrlist = {
	"bracketed_sockaddrlist", parse_bracketed_list, print_bracketed_list,
	&cfg_rep_list, &cfg_type_sockaddr
};

static cfg_type_t cfg_type_rrsetorder = {
	"rrsetorder", parse_bracketed_list, print_bracketed_list,
	&cfg_rep_list, &cfg_type_rrsetorderingelement
};

static keyword_type_t port_kw = { "port", &cfg_type_uint32 };

static cfg_type_t cfg_type_optional_port = {
	"optional_port", parse_optional_keyvalue, print_keyvalue,
	&cfg_rep_uint32, &port_kw
};

/* A list of keys, as in the "key" clause of the controls statement. */
static cfg_type_t cfg_type_keylist = {
	"keylist", parse_bracketed_list, print_bracketed_list, &cfg_rep_list,
	&cfg_type_astring
};

static cfg_type_t cfg_type_trustedkeys = {
	"trusted-keys", parse_bracketed_list, print_bracketed_list, &cfg_rep_list,
	&cfg_type_trustedkey
};

/*
 * An implicit list.  These are formed by clauses that occur multiple times.
 */
static cfg_type_t cfg_type_implicitlist = {
	"implicitlist", NULL, print_list, &cfg_rep_list, NULL };

static const char *forwardtype_enums[] = { "first", "only", NULL };
static cfg_type_t cfg_type_forwardtype = {
	"forwardtype", parse_enum, print_ustring, &cfg_rep_string,
	&forwardtype_enums
};

static const char *zonetype_enums[] = {
	"master", "slave", "stub", "hint", "forward", NULL };
static cfg_type_t cfg_type_zonetype = {
	"zonetype", parse_enum, print_ustring, &cfg_rep_string,
	&zonetype_enums
};

static const char *loglevel_enums[] = {
	"critical", "error", "warning", "notice", "info", "dynamic", NULL };
static cfg_type_t cfg_type_loglevel = {
	"loglevel", parse_enum, print_ustring, &cfg_rep_string,
	&loglevel_enums
};

static const char *transferformat_enums[] = {
	"many-answers", "one-answer", NULL };
static cfg_type_t cfg_type_transferformat = {
	"transferformat", parse_enum, print_ustring, &cfg_rep_string,
	&transferformat_enums
};

/*
 * Clauses that can be found within the top level of the named.conf
 * file only.
 */
static cfg_clausedef_t
namedconf_clauses[] = {
	{ "options", &cfg_type_options, 0 },
	{ "controls", &cfg_type_controls, CFG_CLAUSEFLAG_MULTI },
	{ "acl", &cfg_type_acl, CFG_CLAUSEFLAG_MULTI },
	{ "logging", &cfg_type_logging, 0 },
	{ "view", &cfg_type_view, CFG_CLAUSEFLAG_MULTI },
	{ "lwres", &cfg_type_lwres, CFG_CLAUSEFLAG_MULTI },
	{ NULL, NULL, 0 }
};

/*
 * Clauses that can occur at the top level or in the view
 * statement, but not in the options block.
 */
static cfg_clausedef_t
namedconf_or_view_clauses[] = {
	{ "key", &cfg_type_key, CFG_CLAUSEFLAG_MULTI },
	{ "zone", &cfg_type_zone, CFG_CLAUSEFLAG_MULTI },
	{ "server", &cfg_type_server, CFG_CLAUSEFLAG_MULTI },
#ifdef ISC_RFC2535
	{ "trusted-keys", &cfg_type_trustedkeys, CFG_CLAUSEFLAG_MULTI },
#else
	{ "trusted-keys", &cfg_type_trustedkeys,
		 CFG_CLAUSEFLAG_MULTI|CFG_CLAUSEFLAG_OBSOLETE },
#endif
	{ NULL, NULL, 0 }
};

/*
 * Clauses that can be found within the 'options' statement.
 */
static cfg_clausedef_t
options_clauses[] = {
	{ "blackhole", &cfg_type_bracketed_aml, 0 },
	{ "coresize", &cfg_type_size, 0 },
	{ "datasize", &cfg_type_size, 0 },
	{ "deallocate-on-exit", &cfg_type_boolean, CFG_CLAUSEFLAG_OBSOLETE },
	{ "directory", &cfg_type_qstring, CFG_CLAUSEFLAG_CALLBACK },
	{ "dump-file", &cfg_type_qstring, 0 },
	{ "fake-iquery", &cfg_type_boolean, CFG_CLAUSEFLAG_OBSOLETE },
	{ "files", &cfg_type_size, 0 },
	{ "has-old-clients", &cfg_type_boolean, CFG_CLAUSEFLAG_OBSOLETE },
	{ "heartbeat-interval", &cfg_type_uint32, 0 },
	{ "host-statistics", &cfg_type_boolean, CFG_CLAUSEFLAG_NOTIMP },
	{ "interface-interval", &cfg_type_uint32, 0 },
	{ "listen-on", &cfg_type_listenon, CFG_CLAUSEFLAG_MULTI },
	{ "listen-on-v6", &cfg_type_listenon, CFG_CLAUSEFLAG_MULTI },
	{ "match-mapped-addresses", &cfg_type_boolean, 0 },
	{ "memstatistics-file", &cfg_type_qstring, CFG_CLAUSEFLAG_NOTIMP },
	{ "multiple-cnames", &cfg_type_boolean, CFG_CLAUSEFLAG_OBSOLETE },
	{ "named-xfer", &cfg_type_qstring, CFG_CLAUSEFLAG_OBSOLETE },
	{ "pid-file", &cfg_type_qstring, 0 },
	{ "port", &cfg_type_uint32, 0 },
	{ "random-device", &cfg_type_qstring, 0 },
	{ "recursive-clients", &cfg_type_uint32, 0 },
	{ "rrset-order", &cfg_type_rrsetorder, CFG_CLAUSEFLAG_NOTIMP },
	{ "serial-queries", &cfg_type_uint32, CFG_CLAUSEFLAG_OBSOLETE },
	{ "serial-query-rate", &cfg_type_uint32, 0 },
	{ "stacksize", &cfg_type_size, 0 },
	{ "statistics-file", &cfg_type_qstring, 0 },
	{ "statistics-interval", &cfg_type_uint32, CFG_CLAUSEFLAG_NYI },
	{ "tcp-clients", &cfg_type_uint32, 0 },
	{ "tkey-dhkey", &cfg_type_tkey_dhkey, 0 },
	{ "tkey-gssapi-credential", &cfg_type_qstring, 0 },
	{ "tkey-domain", &cfg_type_qstring, 0 },
	{ "transfers-per-ns", &cfg_type_uint32, 0 },
	{ "transfers-in", &cfg_type_uint32, 0 },
	{ "transfers-out", &cfg_type_uint32, 0 },
	{ "treat-cr-as-space", &cfg_type_boolean, CFG_CLAUSEFLAG_OBSOLETE },
	{ "use-id-pool", &cfg_type_boolean, CFG_CLAUSEFLAG_OBSOLETE },
	{ "use-ixfr", &cfg_type_boolean, 0 },
	{ "version", &cfg_type_qstring, 0 },
	{ NULL, NULL, 0 }
};

/*
 * Clauses that can be found within the 'view' statement,
 * with defaults in the 'options' statement.
 */

static cfg_clausedef_t
view_clauses[] = {
	{ "allow-recursion", &cfg_type_bracketed_aml, 0 },
	{ "allow-v6-synthesis", &cfg_type_bracketed_aml, 0 },
	{ "sortlist", &cfg_type_bracketed_aml, 0 },
	{ "topology", &cfg_type_bracketed_aml, CFG_CLAUSEFLAG_NOTIMP },
	{ "auth-nxdomain", &cfg_type_boolean, CFG_CLAUSEFLAG_NEWDEFAULT },
	{ "minimal-responses", &cfg_type_boolean, 0 },
	{ "recursion", &cfg_type_boolean, 0 },
	{ "provide-ixfr", &cfg_type_boolean, 0 },
	{ "request-ixfr", &cfg_type_boolean, 0 },
	{ "fetch-glue", &cfg_type_boolean, CFG_CLAUSEFLAG_OBSOLETE },
	{ "rfc2308-type1", &cfg_type_boolean, CFG_CLAUSEFLAG_NYI },
	{ "additional-from-auth", &cfg_type_boolean, 0 },
	{ "additional-from-cache", &cfg_type_boolean, 0 },
	/*
	 * Note that the query-source option syntax is different
	 * from the other -source options.
	 */
	{ "query-source", &cfg_type_querysource4, 0 },
	{ "query-source-v6", &cfg_type_querysource6, 0 },
	{ "cleaning-interval", &cfg_type_uint32, 0 },
	{ "min-roots", &cfg_type_uint32, CFG_CLAUSEFLAG_NOTIMP },
	{ "lame-ttl", &cfg_type_uint32, 0 },
	{ "max-ncache-ttl", &cfg_type_uint32, 0 },
	{ "max-cache-ttl", &cfg_type_uint32, 0 },
	{ "transfer-format", &cfg_type_transferformat, 0 },
	{ "max-cache-size", &cfg_type_sizenodefault, 0 },
	{ "check-names", &cfg_type_checknames,
	  CFG_CLAUSEFLAG_MULTI | CFG_CLAUSEFLAG_NOTIMP },
	{ "cache-file", &cfg_type_qstring, 0 },
	{ NULL, NULL, 0 }
};

/*
 * Clauses that can be found within the 'view' statement only.
 */
static cfg_clausedef_t
view_only_clauses[] = {
	{ "match-clients", &cfg_type_bracketed_aml, 0 },
	{ "match-destinations", &cfg_type_bracketed_aml, 0 },
	{ "match-recursive-only", &cfg_type_boolean, 0 },
	{ NULL, NULL, 0 }
};

/*
 * Clauses that can be found in a 'zone' statement,
 * with defaults in the 'view' or 'options' statement.
 */
static cfg_clausedef_t
zone_clauses[] = {
	{ "allow-query", &cfg_type_bracketed_aml, 0 },
	{ "allow-transfer", &cfg_type_bracketed_aml, 0 },
	{ "allow-update-forwarding", &cfg_type_bracketed_aml, 0 },
	{ "allow-notify", &cfg_type_bracketed_aml, 0 },
	{ "notify", &cfg_type_notifytype, 0 },
	{ "notify-source", &cfg_type_sockaddr4wild, 0 },
	{ "notify-source-v6", &cfg_type_sockaddr6wild, 0 },
	{ "also-notify", &cfg_type_portiplist, 0 },
	{ "dialup", &cfg_type_dialuptype, 0 },
	{ "forward", &cfg_type_forwardtype, 0 },
	{ "forwarders", &cfg_type_portiplist, 0 },
	{ "maintain-ixfr-base", &cfg_type_boolean, CFG_CLAUSEFLAG_OBSOLETE },
	{ "max-ixfr-log-size", &cfg_type_size, CFG_CLAUSEFLAG_OBSOLETE },
	{ "transfer-source", &cfg_type_sockaddr4wild, 0 },
	{ "transfer-source-v6", &cfg_type_sockaddr6wild, 0 },
	{ "max-transfer-time-in", &cfg_type_uint32, 0 },
	{ "max-transfer-time-out", &cfg_type_uint32, 0 },
	{ "max-transfer-idle-in", &cfg_type_uint32, 0 },
	{ "max-transfer-idle-out", &cfg_type_uint32, 0 },
	{ "max-retry-time", &cfg_type_uint32, 0 },
	{ "min-retry-time", &cfg_type_uint32, 0 },
	{ "max-refresh-time", &cfg_type_uint32, 0 },
	{ "min-refresh-time", &cfg_type_uint32, 0 },
	{ "sig-validity-interval", &cfg_type_uint32, 0 },
	{ "zone-statistics", &cfg_type_boolean, 0 },
	{ NULL, NULL, 0 }
};

/*
 * Clauses that can be found in a 'zone' statement
 * only.
 */
static cfg_clausedef_t
zone_only_clauses[] = {
	{ "type", &cfg_type_zonetype, 0 },
	{ "allow-update", &cfg_type_bracketed_aml, 0 },
	{ "file", &cfg_type_qstring, 0 },
	{ "ixfr-base", &cfg_type_qstring, CFG_CLAUSEFLAG_OBSOLETE },
	{ "ixfr-tmp-file", &cfg_type_qstring, CFG_CLAUSEFLAG_OBSOLETE },
	{ "masters", &cfg_type_sockaddrkeylist, 0 },
	{ "pubkey", &cfg_type_pubkey,
	  CFG_CLAUSEFLAG_MULTI | CFG_CLAUSEFLAG_OBSOLETE },
	{ "update-policy", &cfg_type_updatepolicy, 0 },
	{ "database", &cfg_type_astring, 0 },
	/*
	 * Note that the format of the check-names option is different between
	 * the zone options and the global/view options.  Ugh.
	 */
	{ "check-names", &cfg_type_ustring, CFG_CLAUSEFLAG_NOTIMP },
	{ NULL, NULL, 0 }
};


/* The top-level named.conf syntax. */

static cfg_clausedef_t *
namedconf_clausesets[] = {
	namedconf_clauses,
	namedconf_or_view_clauses,
	NULL
};

LIBISCCFG_EXTERNAL_DATA cfg_type_t cfg_type_namedconf = {
	"namedconf", parse_mapbody, print_mapbody, &cfg_rep_map,
	namedconf_clausesets
};

/* The "options" statement syntax. */

static cfg_clausedef_t *
options_clausesets[] = {
	options_clauses,
	view_clauses,
	zone_clauses,
	NULL
};
static cfg_type_t cfg_type_options = {
	"options", parse_map, print_map, &cfg_rep_map, options_clausesets };

/* The "view" statement syntax. */

static cfg_clausedef_t *
view_clausesets[] = {
	view_only_clauses,
	namedconf_or_view_clauses,
	view_clauses,
	zone_clauses,
	NULL
};
static cfg_type_t cfg_type_viewopts = {
	"view", parse_map, print_map, &cfg_rep_map, view_clausesets };

/* The "zone" statement syntax. */

static cfg_clausedef_t *
zone_clausesets[] = {
	zone_only_clauses,
	zone_clauses,
	NULL
};
static cfg_type_t cfg_type_zoneopts = {
	"zoneopts", parse_map, print_map, &cfg_rep_map, zone_clausesets };

/*
 * Clauses that can be found within the 'key' statement.
 */
static cfg_clausedef_t
key_clauses[] = {
	{ "algorithm", &cfg_type_astring, 0 },
	{ "secret", &cfg_type_astring, 0 },
	{ NULL, NULL, 0 }
};

static cfg_clausedef_t *
key_clausesets[] = {
	key_clauses,
	NULL
};
static cfg_type_t cfg_type_key = {
	"key", parse_named_map, print_map, &cfg_rep_map, key_clausesets };


/*
 * Clauses that can be found in a 'server' statement.
 */
static cfg_clausedef_t
server_clauses[] = {
	{ "bogus", &cfg_type_boolean, 0 },
	{ "provide-ixfr", &cfg_type_boolean, 0 },
	{ "request-ixfr", &cfg_type_boolean, 0 },
	{ "support-ixfr", &cfg_type_boolean, CFG_CLAUSEFLAG_OBSOLETE },
	{ "transfers", &cfg_type_uint32, 0 },
	{ "transfer-format", &cfg_type_transferformat, 0 },
	{ "keys", &cfg_type_server_key_kludge, 0 },
	{ "edns", &cfg_type_boolean, 0 },
	{ NULL, NULL, 0 }
};
static cfg_clausedef_t *
server_clausesets[] = {
	server_clauses,
	NULL
};
static cfg_type_t cfg_type_server = {
	"server", parse_addressed_map, print_map, &cfg_rep_map,
	server_clausesets
};


/*
 * Clauses that can be found in a 'channel' clause in the
 * 'logging' statement.
 *
 * These have some additional constraints that need to be
 * checked after parsing:
 *  - There must exactly one of file/syslog/null/stderr
 *
 */
static cfg_clausedef_t
channel_clauses[] = {
	/* Destinations.  We no longer require these to be first. */
	{ "file", &cfg_type_logfile, 0 },
	{ "syslog", &cfg_type_optional_facility, 0 },
	{ "null", &cfg_type_void, 0 },
	{ "stderr", &cfg_type_void, 0 },
	/* Options.  We now accept these for the null channel, too. */
	{ "severity", &cfg_type_logseverity, 0 },
	{ "print-time", &cfg_type_boolean, 0 },
	{ "print-severity", &cfg_type_boolean, 0 },
	{ "print-category", &cfg_type_boolean, 0 },
	{ NULL, NULL, 0 }
};
static cfg_clausedef_t *
channel_clausesets[] = {
	channel_clauses,
	NULL
};
static cfg_type_t cfg_type_channel = {
	"channel", parse_named_map, print_map,
	&cfg_rep_map, channel_clausesets
};

/* A list of log destination, used in the "category" clause. */
static cfg_type_t cfg_type_destinationlist = {
	"destinationlist", parse_bracketed_list, print_bracketed_list,
	&cfg_rep_list, &cfg_type_astring };

/*
 * Clauses that can be found in a 'logging' statement.
 */
static cfg_clausedef_t
logging_clauses[] = {
	{ "channel", &cfg_type_channel, CFG_CLAUSEFLAG_MULTI },
	{ "category", &cfg_type_category, CFG_CLAUSEFLAG_MULTI },
	{ NULL, NULL, 0 }
};
static cfg_clausedef_t *
logging_clausesets[] = {
	logging_clauses,
	NULL
};
static cfg_type_t cfg_type_logging = {
	"logging", parse_map, print_map, &cfg_rep_map, logging_clausesets };


/* Functions. */

static void
print_obj(cfg_printer_t *pctx, cfg_obj_t *obj) {
	obj->type->print(pctx, obj);
}

static void
print(cfg_printer_t *pctx, const char *text, int len) {
	pctx->f(pctx->closure, text, len);
}

static void
print_open(cfg_printer_t *pctx) {
	print(pctx, "{\n", 2);
	pctx->indent++;
}

static void
print_indent(cfg_printer_t *pctx) {
	int indent = pctx->indent;
	while (indent > 0) {
		print(pctx, "\t", 1);
		indent--;
	}
}

static void
print_close(cfg_printer_t *pctx) {
	pctx->indent--;
	print_indent(pctx);
	print(pctx, "}", 1);
}

static isc_result_t
parse(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;
	INSIST(ret != NULL && *ret == NULL);
	result = type->parse(pctx, type, ret);
	if (result != ISC_R_SUCCESS)
		return (result);
	INSIST(*ret != NULL);
	return (ISC_R_SUCCESS);
}

void
cfg_print(cfg_obj_t *obj,
	  void (*f)(void *closure, const char *text, int textlen),
	  void *closure)
{
	cfg_printer_t pctx;
	pctx.f = f;
	pctx.closure = closure;
	pctx.indent = 0;
	obj->type->print(&pctx, obj);
}


/* Tuples. */
  
static isc_result_t
create_tuple(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;
	const cfg_tuplefielddef_t *fields = type->of;
	const cfg_tuplefielddef_t *f;
	cfg_obj_t *obj = NULL;
	unsigned int nfields = 0;
	int i;

	for (f = fields; f->name != NULL; f++)
		nfields++;

	CHECK(create_cfgobj(pctx, type, &obj));
	obj->value.tuple = isc_mem_get(pctx->mctx,
				       nfields * sizeof(cfg_obj_t *));
	if (obj->value.tuple == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}
	for (f = fields, i = 0; f->name != NULL; f++, i++)
		obj->value.tuple[i] = NULL;
	*ret = obj;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(obj);
	return (result);
}

static isc_result_t
parse_tuple(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret)
{
	isc_result_t result;
	const cfg_tuplefielddef_t *fields = type->of;
	const cfg_tuplefielddef_t *f;
	cfg_obj_t *obj = NULL;
	unsigned int i;

	CHECK(create_tuple(pctx, type, &obj));
	for (f = fields, i = 0; f->name != NULL; f++, i++)
		CHECK(parse(pctx, f->type, &obj->value.tuple[i]));

	*ret = obj;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(obj);
	return (result);
}

static void
print_tuple(cfg_printer_t *pctx, cfg_obj_t *obj) {
	unsigned int i;
	const cfg_tuplefielddef_t *fields = obj->type->of;
	const cfg_tuplefielddef_t *f;
	isc_boolean_t need_space = ISC_FALSE;

	for (f = fields, i = 0; f->name != NULL; f++, i++) {
		cfg_obj_t *fieldobj = obj->value.tuple[i];
		if (need_space)
			print(pctx, " ", 1);
		print_obj(pctx, fieldobj);
		need_space = ISC_TF(fieldobj->type->print != print_void);
	}
}

static void
free_tuple(cfg_parser_t *pctx, cfg_obj_t *obj) {
	unsigned int i;
	const cfg_tuplefielddef_t *fields = obj->type->of;
	const cfg_tuplefielddef_t *f;
	unsigned int nfields = 0;

	if (obj->value.tuple == NULL)
		return;

	for (f = fields, i = 0; f->name != NULL; f++, i++) {
		CLEANUP_OBJ(obj->value.tuple[i]);
		nfields++;
	}
	isc_mem_put(pctx->mctx, obj->value.tuple,
		    nfields * sizeof(cfg_obj_t *));
}

isc_boolean_t
cfg_obj_istuple(cfg_obj_t *obj) {
	REQUIRE(obj != NULL);
	return (ISC_TF(obj->type->rep == &cfg_rep_tuple));
}

cfg_obj_t *
cfg_tuple_get(cfg_obj_t *tupleobj, const char* name) {
	unsigned int i;
	const cfg_tuplefielddef_t *fields;
	const cfg_tuplefielddef_t *f;
	
	REQUIRE(tupleobj != NULL && tupleobj->type->rep == &cfg_rep_tuple);

	fields = tupleobj->type->of;
	for (f = fields, i = 0; f->name != NULL; f++, i++) {
		if (strcmp(f->name, name) == 0)
			return (tupleobj->value.tuple[i]);
	}
	INSIST(0);
	return (NULL);
}

/*
 * Parse a required special character.
 */
static isc_result_t
parse_special(cfg_parser_t *pctx, int special) {
        isc_result_t result;
	CHECK(cfg_gettoken(pctx, 0));
	if (pctx->token.type == isc_tokentype_special &&
	    pctx->token.value.as_char == special)
		return (ISC_R_SUCCESS);

	parser_error(pctx, LOG_NEAR, "'%c' expected", special);
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

	parser_error(pctx, LOG_BEFORE, "missing ';'");
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

	parser_error(pctx, LOG_NEAR, "syntax error");
	return (ISC_R_UNEXPECTEDTOKEN);
 cleanup:
	return(result);
}

/* A list of files, used internally for pctx->files. */

static cfg_type_t cfg_type_filelist = {
	"filelist", NULL, print_list, &cfg_rep_list,
	&cfg_type_qstring
};

isc_result_t
cfg_parser_create(isc_mem_t *mctx, isc_log_t *lctx, cfg_parser_t **ret)
{
	isc_result_t result;
	cfg_parser_t *pctx;
	isc_lexspecials_t specials;

	REQUIRE(mctx != NULL);
	REQUIRE(ret != NULL && *ret == NULL);

	pctx = isc_mem_get(mctx, sizeof(*pctx));
	if (pctx == NULL)
		return (ISC_R_NOMEMORY);

	pctx->mctx = mctx;
	pctx->lctx = lctx;
	pctx->lexer = NULL;
	pctx->seen_eof = ISC_FALSE;
	pctx->ungotten = ISC_FALSE;
	pctx->errors = 0;
	pctx->warnings = 0;
	pctx->open_files = NULL;
	pctx->closed_files = NULL;
	pctx->line = 0;
	pctx->callback = NULL;
	pctx->callbackarg = NULL;
	pctx->token.type = isc_tokentype_unknown;

	memset(specials, 0, sizeof(specials));
	specials['{'] = 1;
	specials['}'] = 1;
	specials[';'] = 1;
	specials['/'] = 1;
	specials['"'] = 1;
	specials['!'] = 1;

	CHECK(isc_lex_create(pctx->mctx, 1024, &pctx->lexer));

	isc_lex_setspecials(pctx->lexer, specials);
	isc_lex_setcomments(pctx->lexer, (ISC_LEXCOMMENT_C |
					 ISC_LEXCOMMENT_CPLUSPLUS |
					 ISC_LEXCOMMENT_SHELL));

	CHECK(create_list(pctx, &cfg_type_filelist, &pctx->open_files));
	CHECK(create_list(pctx, &cfg_type_filelist, &pctx->closed_files));

	*ret = pctx;
	return (ISC_R_SUCCESS);

 cleanup:
	if (pctx->lexer != NULL)
		isc_lex_destroy(&pctx->lexer);
	CLEANUP_OBJ(pctx->open_files);
	CLEANUP_OBJ(pctx->closed_files);
	isc_mem_put(mctx, pctx, sizeof(*pctx));
	return (result);
}

static isc_result_t
parser_openfile(cfg_parser_t *pctx, const char *filename) {
	isc_result_t result;
	cfg_listelt_t *elt = NULL;
	cfg_obj_t *stringobj = NULL;

	result = isc_lex_openfile(pctx->lexer, filename);
	if (result != ISC_R_SUCCESS) {
		parser_error(pctx, 0, "open: %s: %s",
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

void
cfg_parser_setcallback(cfg_parser_t *pctx,
		       cfg_parsecallback_t callback,
		       void *arg)
{
	pctx->callback = callback;
	pctx->callbackarg = arg;
}

/*
 * Parse a configuration using a pctx where a lexer has already
 * been set up with a source.
 */
static isc_result_t
parse2(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;
	cfg_obj_t *obj = NULL;

	result = parse(pctx, type, &obj);

	if (pctx->errors != 0) {
		/* Errors have been logged. */
		if (result == ISC_R_SUCCESS)
			result = ISC_R_FAILURE;
		goto cleanup;
	}

	if (result != ISC_R_SUCCESS) {
		/* Parsing failed but no errors have been logged. */
		parser_error(pctx, 0, "parsing failed");
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

	REQUIRE(filename != NULL);

	CHECK(parser_openfile(pctx, filename));
	CHECK(parse2(pctx, type, ret));
 cleanup:
	return (result);
}


isc_result_t
cfg_parse_buffer(cfg_parser_t *pctx, isc_buffer_t *buffer,
	const cfg_type_t *type, cfg_obj_t **ret)
{
	isc_result_t result;
	REQUIRE(buffer != NULL);
	CHECK(isc_lex_openbuffer(pctx->lexer, buffer));	
	CHECK(parse2(pctx, type, ret));
 cleanup:
	return (result);
}

void
cfg_parser_destroy(cfg_parser_t **pctxp) {
	cfg_parser_t *pctx = *pctxp;
	isc_lex_destroy(&pctx->lexer);
	/*
	 * Cleaning up open_files does not
	 * close the files; that was already done
	 * by closing the lexer.
	 */
	CLEANUP_OBJ(pctx->open_files);
	CLEANUP_OBJ(pctx->closed_files);
	isc_mem_put(pctx->mctx, pctx, sizeof(*pctx));
	*pctxp = NULL;
}

/*
 * void
 */
static isc_result_t
parse_void(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	UNUSED(type);
	return (create_cfgobj(pctx, &cfg_type_void, ret));
}

static void
print_void(cfg_printer_t *pctx, cfg_obj_t *obj) {
	UNUSED(pctx);
	UNUSED(obj);
}

isc_boolean_t
cfg_obj_isvoid(cfg_obj_t *obj) {
	REQUIRE(obj != NULL);
	return (ISC_TF(obj->type->rep == &cfg_rep_void));
}

static cfg_type_t cfg_type_void = {
	"void", parse_void, print_void, &cfg_rep_void, NULL };


/*
 * uint32
 */
static isc_result_t
parse_uint32(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
        isc_result_t result;
	cfg_obj_t *obj = NULL;
	UNUSED(type);

	CHECK(cfg_gettoken(pctx, ISC_LEXOPT_NUMBER | ISC_LEXOPT_CNUMBER));
	if (pctx->token.type != isc_tokentype_number) {
		parser_error(pctx, LOG_NEAR, "expected number");
		return (ISC_R_UNEXPECTEDTOKEN);
	}

	CHECK(create_cfgobj(pctx, &cfg_type_uint32, &obj));

	obj->value.uint32 = pctx->token.value.as_ulong;
	*ret = obj;
 cleanup:
	return (result);
}

static void
print_cstr(cfg_printer_t *pctx, const char *s) {
	print(pctx, s, strlen(s));
}

static void
print_uint(cfg_printer_t *pctx, unsigned int u) {
	char buf[32];
	snprintf(buf, sizeof(buf), "%u", u);
	print_cstr(pctx, buf);
}

static void
print_uint32(cfg_printer_t *pctx, cfg_obj_t *obj) {
	print_uint(pctx, obj->value.uint32);
}

isc_boolean_t
cfg_obj_isuint32(cfg_obj_t *obj) {
	REQUIRE(obj != NULL);
	return (ISC_TF(obj->type->rep == &cfg_rep_uint32));
}

isc_uint32_t
cfg_obj_asuint32(cfg_obj_t *obj) {
	REQUIRE(obj != NULL && obj->type->rep == &cfg_rep_uint32);
	return (obj->value.uint32);
}

static cfg_type_t cfg_type_uint32 = {
	"integer", parse_uint32, print_uint32, &cfg_rep_uint32, NULL };


/*
 * uint64
 */
isc_boolean_t
cfg_obj_isuint64(cfg_obj_t *obj) {
	REQUIRE(obj != NULL);
	return (ISC_TF(obj->type->rep == &cfg_rep_uint64));
}

isc_uint64_t
cfg_obj_asuint64(cfg_obj_t *obj) {
	REQUIRE(obj != NULL && obj->type->rep == &cfg_rep_uint64);
	return (obj->value.uint64);
}

static isc_result_t
parse_unitstring(char *str, isc_resourcevalue_t *valuep) {
	char *endp;
	unsigned int len;
	isc_uint64_t value;
	isc_uint64_t unit;

	value = isc_string_touint64(str, &endp, 10);
	if (*endp == 0) {
		*valuep = value;
		return (ISC_R_SUCCESS);
	}

	len = strlen(str);
	if (len < 2 || endp[1] != '\0')
		return (ISC_R_FAILURE);

	switch (str[len - 1]) {
	case 'k':
	case 'K':
		unit = 1024;
		break;
	case 'm':
	case 'M':
		unit = 1024 * 1024;
		break;
	case 'g':
	case 'G':
		unit = 1024 * 1024 * 1024;
		break;
	default:
		return (ISC_R_FAILURE);
	}
	if (value > ISC_UINT64_MAX / unit)
		return (ISC_R_FAILURE);
	*valuep = value * unit;
	return (ISC_R_SUCCESS);
}

static void
print_uint64(cfg_printer_t *pctx, cfg_obj_t *obj) {
	char buf[32];
	sprintf(buf, "%" ISC_PRINT_QUADFORMAT "u", obj->value.uint64);
	print_cstr(pctx, buf);
}

static cfg_type_t cfg_type_uint64 = {
	"64_bit_integer", NULL, print_uint64, &cfg_rep_uint64, NULL };

static isc_result_t
parse_sizeval(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;
	cfg_obj_t *obj = NULL;
	isc_uint64_t val;

	UNUSED(type);

	CHECK(cfg_gettoken(pctx, 0));
	CHECK(parse_unitstring(pctx->token.value.as_pointer, &val));

	CHECK(create_cfgobj(pctx, &cfg_type_uint64, &obj));
	obj->value.uint64 = val;
	*ret = obj;
	return (ISC_R_SUCCESS);

 cleanup:
	parser_error(pctx, LOG_NEAR, "expected integer and optional unit");
	return (result);
}

/*
 * A size value (number + optional unit).
 */
static cfg_type_t cfg_type_sizeval = {
	"sizeval", parse_sizeval, print_uint64, &cfg_rep_uint64, NULL };

/*
 * A size, "unlimited", or "default".
 */

static isc_result_t
parse_size(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	return (parse_enum_or_other(pctx, type, &cfg_type_sizeval, ret));
}

static const char *size_enums[] = { "unlimited", "default", NULL };
static cfg_type_t cfg_type_size = {
	"size", parse_size, print_ustring, &cfg_rep_string, size_enums
};

/*
 * A size or "unlimited", but not "default".
 */
static const char *sizenodefault_enums[] = { "unlimited", NULL };
static cfg_type_t cfg_type_sizenodefault = {
	"size_no_default", parse_size, print_ustring, &cfg_rep_string,
	sizenodefault_enums
};

/*
 * optional_keyvalue
 */
static isc_result_t
parse_maybe_optional_keyvalue(cfg_parser_t *pctx, const cfg_type_t *type,
			      isc_boolean_t optional, cfg_obj_t **ret)
{
        isc_result_t result;
	cfg_obj_t *obj = NULL;
	const keyword_type_t *kw = type->of;

	CHECK(cfg_peektoken(pctx, 0));
	if (pctx->token.type == isc_tokentype_string &&
	    strcasecmp(pctx->token.value.as_pointer, kw->name) == 0) {
		CHECK(cfg_gettoken(pctx, 0));
		CHECK(kw->type->parse(pctx, kw->type, &obj));
		obj->type = type; /* XXX kludge */
	} else {
		if (optional) {
			CHECK(parse_void(pctx, NULL, &obj));
		} else {
			parser_error(pctx, LOG_NEAR, "expected '%s'",
				     kw->name);
			result = ISC_R_UNEXPECTEDTOKEN;
			goto cleanup;
		}
	}
	*ret = obj;
 cleanup:
	return (result);
}

static isc_result_t
parse_keyvalue(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	return (parse_maybe_optional_keyvalue(pctx, type, ISC_FALSE, ret));
}

static isc_result_t
parse_optional_keyvalue(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	return (parse_maybe_optional_keyvalue(pctx, type, ISC_TRUE, ret));
}

static void
print_keyvalue(cfg_printer_t *pctx, cfg_obj_t *obj) {
	const keyword_type_t *kw = obj->type->of;
	print_cstr(pctx, kw->name);
	print(pctx, " ", 1);
	kw->type->print(pctx, obj);
}

/*
 * qstring, ustring, astring
 */

/* Create a string object from a null-terminated C string. */
static isc_result_t
create_string(cfg_parser_t *pctx, const char *contents, const cfg_type_t *type,
	      cfg_obj_t **ret)
{
	isc_result_t result;
	cfg_obj_t *obj = NULL;
	int len;

	CHECK(create_cfgobj(pctx, type, &obj));
	len = strlen(contents);
	obj->value.string.length = len;
	obj->value.string.base = isc_mem_get(pctx->mctx, len + 1);
	if (obj->value.string.base == 0) {
		CLEANUP_OBJ(obj);
		return (ISC_R_NOMEMORY);
	}
	memcpy(obj->value.string.base, contents, len);
	obj->value.string.base[len] = '\0';

	*ret = obj;
 cleanup:
	return (result);
}

static isc_result_t
parse_qstring(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
        isc_result_t result;
	UNUSED(type);

	CHECK(cfg_gettoken(pctx, QSTRING));
	if (pctx->token.type != isc_tokentype_qstring) {
		parser_error(pctx, LOG_NEAR, "expected quoted string");
		return (ISC_R_UNEXPECTEDTOKEN);
	}
	return (create_string(pctx,
			      pctx->token.value.as_pointer,
			      &cfg_type_qstring,
			      ret));
 cleanup:
	return (result);
}

static isc_result_t
parse_ustring(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
        isc_result_t result;
	UNUSED(type);

	CHECK(cfg_gettoken(pctx, 0));
	if (pctx->token.type != isc_tokentype_string) {
		parser_error(pctx, LOG_NEAR, "expected unquoted string");
		return (ISC_R_UNEXPECTEDTOKEN);
	}
	return (create_string(pctx,
			      pctx->token.value.as_pointer,
			      &cfg_type_ustring,
			      ret));
 cleanup:
	return (result);
}

static isc_result_t
parse_astring(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
        isc_result_t result;
	UNUSED(type);

	CHECK(cfg_getstringtoken(pctx));
	return (create_string(pctx,
			      pctx->token.value.as_pointer,
			      &cfg_type_qstring,
			      ret));
 cleanup:
	return (result);
}

static isc_boolean_t
is_enum(const char *s, const char *const *enums) {
	const char * const *p;
	for (p = enums; *p != NULL; p++) {
		if (strcasecmp(*p, s) == 0)
			return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

static isc_result_t
check_enum(cfg_parser_t *pctx, cfg_obj_t *obj, const char *const *enums) {
	const char *s = obj->value.string.base;
	if (is_enum(s, enums))
		return (ISC_R_SUCCESS);
	parser_error(pctx, 0, "'%s' unexpected", s);
	return (ISC_R_UNEXPECTEDTOKEN);
}

static isc_result_t
parse_enum(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
        isc_result_t result;
	cfg_obj_t *obj = NULL;
	CHECK(parse_ustring(pctx, NULL, &obj));
	CHECK(check_enum(pctx, obj, type->of));
	*ret = obj;
	return (ISC_R_SUCCESS);
 cleanup:
	CLEANUP_OBJ(obj);	
	return (result);
}

static isc_result_t
parse_enum_or_other(cfg_parser_t *pctx, const cfg_type_t *enumtype,
		    const cfg_type_t *othertype, cfg_obj_t **ret)
{
        isc_result_t result;
	CHECK(cfg_peektoken(pctx, 0));
	if (pctx->token.type == isc_tokentype_string &&
	    is_enum(pctx->token.value.as_pointer, enumtype->of)) {
		CHECK(parse_enum(pctx, enumtype, ret));
	} else {
		CHECK(parse(pctx, othertype, ret));
	}
 cleanup:
	return (result);
}


/*
 * Print a string object.
 */
static void
print_ustring(cfg_printer_t *pctx, cfg_obj_t *obj) {
	print(pctx, obj->value.string.base, obj->value.string.length);
}

static void
print_qstring(cfg_printer_t *pctx, cfg_obj_t *obj) {
	print(pctx, "\"", 1);
	print_ustring(pctx, obj);
	print(pctx, "\"", 1);
}

static void
free_string(cfg_parser_t *pctx, cfg_obj_t *obj) {
	isc_mem_put(pctx->mctx, obj->value.string.base,
		    obj->value.string.length + 1);
}

isc_boolean_t
cfg_obj_isstring(cfg_obj_t *obj) {
	REQUIRE(obj != NULL);
	return (ISC_TF(obj->type->rep == &cfg_rep_string));
}

char *
cfg_obj_asstring(cfg_obj_t *obj) {
	REQUIRE(obj != NULL && obj->type->rep == &cfg_rep_string);
	return (obj->value.string.base);
}

isc_boolean_t
cfg_obj_isboolean(cfg_obj_t *obj) {
	REQUIRE(obj != NULL);
	return (ISC_TF(obj->type->rep == &cfg_rep_boolean));
}

isc_boolean_t
cfg_obj_asboolean(cfg_obj_t *obj) {
	REQUIRE(obj != NULL && obj->type->rep == &cfg_rep_boolean);
	return (obj->value.boolean);
}

/* Quoted string only */
static cfg_type_t cfg_type_qstring = {
	"quoted_string", parse_qstring, print_qstring, &cfg_rep_string, NULL };

/* Unquoted string only */
static cfg_type_t cfg_type_ustring = {
	"string", parse_ustring, print_ustring, &cfg_rep_string, NULL };

/* Any string (quoted or unquoted); printed with quotes */
static cfg_type_t cfg_type_astring = {
	"string", parse_astring, print_qstring, &cfg_rep_string, NULL };


/*
 * boolean
 */
static isc_result_t
parse_boolean(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret)
{
        isc_result_t result;
	isc_boolean_t value;
	cfg_obj_t *obj = NULL;
	UNUSED(type);

	result = cfg_gettoken(pctx, 0);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (pctx->token.type != isc_tokentype_string)
		goto bad_boolean;

	if ((strcasecmp(pctx->token.value.as_pointer, "true") == 0) ||
	    (strcasecmp(pctx->token.value.as_pointer, "yes") == 0) ||
	    (strcmp(pctx->token.value.as_pointer, "1") == 0)) {
		value = ISC_TRUE;
	} else if ((strcasecmp(pctx->token.value.as_pointer, "false") == 0) ||
		   (strcasecmp(pctx->token.value.as_pointer, "no") == 0) ||
		   (strcmp(pctx->token.value.as_pointer, "0") == 0)) {
		value = ISC_FALSE;
	} else {
		goto bad_boolean;
	}

	CHECK(create_cfgobj(pctx, &cfg_type_boolean, &obj));
	obj->value.boolean = value;
	*ret = obj;
	return (result);

 bad_boolean:
	parser_error(pctx, LOG_NEAR, "boolean expected");
	return (ISC_R_UNEXPECTEDTOKEN);

 cleanup:
	return (result);
}

static void
print_boolean(cfg_printer_t *pctx, cfg_obj_t *obj) {
	if (obj->value.boolean)
		print(pctx, "yes", 3);
	else
		print(pctx, "no", 2);
}

static cfg_type_t cfg_type_boolean = {
	"boolean", parse_boolean, print_boolean, &cfg_rep_boolean, NULL };

static const char *dialup_enums[] = {
	"notify", "notify-passive", "refresh", "passive", NULL };
static isc_result_t
parse_dialup_type(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	return (parse_enum_or_other(pctx, type, &cfg_type_boolean, ret));
}
static cfg_type_t cfg_type_dialuptype = {
	"dialuptype", parse_dialup_type, print_ustring, 
	&cfg_rep_string, dialup_enums
};

static const char *notify_enums[] = { "explicit", NULL };
static isc_result_t
parse_notify_type(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	return (parse_enum_or_other(pctx, type, &cfg_type_boolean, ret));
}
static cfg_type_t cfg_type_notifytype = {
	"notifytype", parse_notify_type, print_ustring, 
	&cfg_rep_string, notify_enums,
};

static keyword_type_t key_kw = { "key", &cfg_type_astring };

LIBISCCFG_EXTERNAL_DATA cfg_type_t cfg_type_keyref = {
	"keyref", parse_keyvalue, print_keyvalue,
	&cfg_rep_string, &key_kw
};

static cfg_type_t cfg_type_optional_keyref = {
	"optional_keyref", parse_optional_keyvalue, print_keyvalue,
	&cfg_rep_string, &key_kw
};


/*
 * Lists.
 */

static isc_result_t
create_list(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **obj) {
	isc_result_t result;
	CHECK(create_cfgobj(pctx, type, obj));
	ISC_LIST_INIT((*obj)->value.list);
 cleanup:
	return (result);
}

static isc_result_t
create_listelt(cfg_parser_t *pctx, cfg_listelt_t **eltp) {
	cfg_listelt_t *elt;
	elt = isc_mem_get(pctx->mctx, sizeof(*elt));
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
	isc_mem_put(pctx->mctx, elt, sizeof(*elt));
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
parse_list_elt(cfg_parser_t *pctx, const cfg_type_t *elttype,
	       cfg_listelt_t **ret)
{
	isc_result_t result;
	cfg_listelt_t *elt = NULL;
	cfg_obj_t *value = NULL;

	CHECK(create_listelt(pctx, &elt));

	result = parse(pctx, elttype, &value);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	elt->obj = value;

	*ret = elt;
	return (ISC_R_SUCCESS);

 cleanup:
	isc_mem_put(pctx->mctx, elt, sizeof(*elt));
	return (result);
}

/*
 * Parse a homogeneous list whose elements are of type 'elttype'
 * and where each element is terminated by a semicolon.
 */
static isc_result_t
parse_list(cfg_parser_t *pctx, const cfg_type_t *listtype, cfg_obj_t **ret)
{
	cfg_obj_t *listobj = NULL;
	const cfg_type_t *listof = listtype->of;
	isc_result_t result;

	CHECK(create_list(pctx, listtype, &listobj));

	for (;;) {
		cfg_listelt_t *elt = NULL;

		CHECK(cfg_peektoken(pctx, 0));
		if (pctx->token.type == isc_tokentype_special &&
		    pctx->token.value.as_char == '}')
			break;
		CHECK(parse_list_elt(pctx, listof, &elt));
		CHECK(parse_semicolon(pctx));
		ISC_LIST_APPEND(listobj->value.list, elt, link);
	}
	*ret = listobj;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(listobj);
	return (result);
}

static void
print_list(cfg_printer_t *pctx, cfg_obj_t *obj) {
	cfg_list_t *list = &obj->value.list;
	cfg_listelt_t *elt;

	for (elt = ISC_LIST_HEAD(*list);
	     elt != NULL;
	     elt = ISC_LIST_NEXT(elt, link)) {
		print_indent(pctx);
		print_obj(pctx, elt->obj);
		print(pctx, ";\n", 2);
	}
}

static isc_result_t
parse_bracketed_list(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret)
{
	isc_result_t result;
	CHECK(parse_special(pctx, '{'));
	CHECK(parse_list(pctx, type, ret));
	CHECK(parse_special(pctx, '}'));
 cleanup:
	return (result);
}

static void
print_bracketed_list(cfg_printer_t *pctx, cfg_obj_t *obj) {
	print_open(pctx);
	print_list(pctx, obj);
	print_close(pctx);
}

/*
 * Parse a homogeneous list whose elements are of type 'elttype'
 * and where elements are separated by space.  The list ends
 * before the first semicolon.
 */
static isc_result_t
parse_spacelist(cfg_parser_t *pctx, const cfg_type_t *listtype, cfg_obj_t **ret)
{
	cfg_obj_t *listobj = NULL;
	const cfg_type_t *listof = listtype->of;
	isc_result_t result;

	CHECK(create_list(pctx, listtype, &listobj));

	for (;;) {
		cfg_listelt_t *elt = NULL;

		CHECK(cfg_peektoken(pctx, 0));
		if (pctx->token.type == isc_tokentype_special &&
		    pctx->token.value.as_char == ';')
			break;
		CHECK(parse_list_elt(pctx, listof, &elt));
		ISC_LIST_APPEND(listobj->value.list, elt, link);
	}
	*ret = listobj;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(listobj);
	return (result);
}

static void
print_spacelist(cfg_printer_t *pctx, cfg_obj_t *obj) {
	cfg_list_t *list = &obj->value.list;
	cfg_listelt_t *elt;

	for (elt = ISC_LIST_HEAD(*list);
	     elt != NULL;
	     elt = ISC_LIST_NEXT(elt, link)) {
		print_obj(pctx, elt->obj);
		if (ISC_LIST_NEXT(elt, link) != NULL)
			print(pctx, " ", 1);
	}
}

isc_boolean_t
cfg_obj_islist(cfg_obj_t *obj) {
	REQUIRE(obj != NULL);
	return (ISC_TF(obj->type->rep == &cfg_rep_list));
}

cfg_listelt_t *
cfg_list_first(cfg_obj_t *obj) {
	REQUIRE(obj == NULL || obj->type->rep == &cfg_rep_list);
	if (obj == NULL)
		return (NULL);
	return (ISC_LIST_HEAD(obj->value.list));
}

cfg_listelt_t *
cfg_list_next(cfg_listelt_t *elt) {
	REQUIRE(elt != NULL);
	return (ISC_LIST_NEXT(elt, link));
}

cfg_obj_t *
cfg_listelt_value(cfg_listelt_t *elt) {
	REQUIRE(elt != NULL);
	return (elt->obj);
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
static isc_result_t
parse_mapbody(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret)
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
	cfg_list_t *list = NULL;

	CHECK(create_map(pctx, type, &obj));

	obj->value.map.clausesets = clausesets;

	for (;;) {
		cfg_listelt_t *elt;

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
		if (strcasecmp(pctx->token.value.as_pointer, "include") == 0) {
			/*
			 * Turn the file name into a temporary configuration
			 * object just so that it is not overwritten by the
			 * semicolon token.
			 */
			CHECK(parse(pctx, &cfg_type_qstring, &includename));
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
				if (strcasecmp(pctx->token.value.as_pointer,
					   clause->name) == 0)
					goto done;
			}
		}
	done:
		if (clause == NULL || clause->name == NULL) {
			parser_error(pctx, LOG_NOPREP, "unknown option");
			/*
			 * Try to recover by parsing this option as an unknown
			 * option and discarding it.
			 */
			 CHECK(parse(pctx, &cfg_type_unsupported, &eltobj));
			 cfg_obj_destroy(pctx, &eltobj);
			 CHECK(parse_semicolon(pctx));
			 continue;
		}

		/* Clause is known. */

		/* Issue warnings if appropriate */
		if ((clause->flags & CFG_CLAUSEFLAG_OBSOLETE) != 0)
			parser_warning(pctx, 0, "option '%s' is obsolete",
				       clause->name);
		if ((clause->flags & CFG_CLAUSEFLAG_NOTIMP) != 0)
			parser_warning(pctx, 0, "option '%s' is "
				       "not implemented", clause->name);
		if ((clause->flags & CFG_CLAUSEFLAG_NYI) != 0)
			parser_warning(pctx, 0, "option '%s' is "
				       "not implemented", clause->name);
		/*
		 * Don't log options with CFG_CLAUSEFLAG_NEWDEFAULT
		 * set here - we need to log the *lack* of such an option,
		 * not its presence.
		 */

		/* See if the clause already has a value; if not create one. */
		result = isc_symtab_lookup(obj->value.map.symtab,
					   clause->name, 0, &symval);

		if ((clause->flags & CFG_CLAUSEFLAG_MULTI) != 0) {
			/* Multivalued clause */
			cfg_obj_t *listobj = NULL;
			if (result == ISC_R_NOTFOUND) {
				CHECK(create_list(pctx,
						  &cfg_type_implicitlist,
						  &listobj));
				symval.as_pointer = listobj;
				result = isc_symtab_define(obj->value.
						   map.symtab,
						   clause->name,
						   1, symval,
						   isc_symexists_reject);
				if (result != ISC_R_SUCCESS) {
					parser_error(pctx, LOG_NEAR,
						     "isc_symtab_define(%s) "
						     "failed", clause->name);
					isc_mem_put(pctx->mctx, list,
						    sizeof(cfg_list_t));
					goto cleanup;
				}
			} else {
				INSIST(result == ISC_R_SUCCESS);
				listobj = symval.as_pointer;
			}

			elt = NULL;
			CHECK(parse_list_elt(pctx, clause->type, &elt));
			CHECK(parse_semicolon(pctx));

			ISC_LIST_APPEND(listobj->value.list, elt, link);
		} else {
			/* Single-valued clause */
			if (result == ISC_R_NOTFOUND) {
				isc_boolean_t callback =
					ISC_TF((clause->flags &
						CFG_CLAUSEFLAG_CALLBACK) != 0);
				CHECK(parse_symtab_elt(pctx, clause->name,
						       clause->type,
						       obj->value.map.symtab,
						       callback));
				CHECK(parse_semicolon(pctx));
			} else if (result == ISC_R_SUCCESS) {
				parser_error(pctx, LOG_NEAR, "'%s' redefined",
					     clause->name);
				result = ISC_R_EXISTS;
				goto cleanup;
			} else {
				parser_error(pctx, LOG_NEAR,
					     "isc_symtab_define() failed");
				goto cleanup;
			}
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
		 cfg_type_t *elttype, isc_symtab_t *symtab,
		 isc_boolean_t callback)
{
	isc_result_t result;
	cfg_obj_t *obj = NULL;
	isc_symvalue_t symval;

	CHECK(parse(pctx, elttype, &obj));

	if (callback && pctx->callback != NULL)
		CHECK(pctx->callback(name, obj, pctx->callbackarg));
	
	symval.as_pointer = obj;
	CHECK(isc_symtab_define(symtab, name,
				1, symval,
				isc_symexists_reject));
	obj = NULL;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(obj);
	return (result);
}

/*
 * Parse a map; e.g., "{ foo 1; bar { glub; }; zap true; zap false; }"
 */
static isc_result_t
parse_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret)
{
	isc_result_t result;
	CHECK(parse_special(pctx, '{'));
	CHECK(parse_mapbody(pctx, type, ret));
	CHECK(parse_special(pctx, '}'));
 cleanup:
	return (result);
}

/*
 * Subroutine for parse_named_map() and parse_addressed_map().
 */
static isc_result_t
parse_any_named_map(cfg_parser_t *pctx, cfg_type_t *nametype, const cfg_type_t *type,
		    cfg_obj_t **ret)
{
	isc_result_t result;
	cfg_obj_t *idobj = NULL;
	cfg_obj_t *mapobj = NULL;

	CHECK(parse(pctx, nametype, &idobj));
	CHECK(parse_map(pctx, type, &mapobj));
	mapobj->value.map.id = idobj;
	idobj = NULL;
	*ret = mapobj;
 cleanup:
	CLEANUP_OBJ(idobj);
	return (result);
}

/*
 * Parse a map identified by a string name.  E.g., "name { foo 1; }".  
 * Used for the "key" and "channel" statements.
 */
static isc_result_t
parse_named_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	return (parse_any_named_map(pctx, &cfg_type_astring, type, ret));
}

/*
 * Parse a map identified by a network address.
 * Used for the "server" statement.
 */
static isc_result_t
parse_addressed_map(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	return (parse_any_named_map(pctx, &cfg_type_netaddr, type, ret));
}

static void
print_mapbody(cfg_printer_t *pctx, cfg_obj_t *obj) {
	isc_result_t result = ISC_R_SUCCESS;

	const cfg_clausedef_t * const *clauseset;

	for (clauseset = obj->value.map.clausesets;
	     *clauseset != NULL;
	     clauseset++)
	{
		isc_symvalue_t symval;
		const cfg_clausedef_t *clause;

		for (clause = *clauseset;
		     clause->name != NULL;
		     clause++) {
			result = isc_symtab_lookup(obj->value.map.symtab,
						   clause->name, 0, &symval);
			if (result == ISC_R_SUCCESS) {
				cfg_obj_t *obj = symval.as_pointer;
				if (obj->type == &cfg_type_implicitlist) {
					/* Multivalued. */
					cfg_list_t *list = &obj->value.list;
					cfg_listelt_t *elt;
					for (elt = ISC_LIST_HEAD(*list);
					     elt != NULL;
					     elt = ISC_LIST_NEXT(elt, link)) {
						print_indent(pctx);
						print_cstr(pctx, clause->name);
						print(pctx, " ", 1);
						print_obj(pctx, elt->obj);
						print(pctx, ";\n", 2);
					}
				} else {
					/* Single-valued. */
					print_indent(pctx);
					print_cstr(pctx, clause->name);
					print(pctx, " ", 1);
					print_obj(pctx, obj);
					print(pctx, ";\n", 2);
				}
			} else if (result == ISC_R_NOTFOUND) {
				; /* do nothing */
			} else {
				INSIST(0);
			}
		}
	}
}

static void
print_map(cfg_printer_t *pctx, cfg_obj_t *obj) {
	if (obj->value.map.id != NULL) {
		print_obj(pctx, obj->value.map.id);
		print(pctx, " ", 1);
	}
	print_open(pctx);
	print_mapbody(pctx, obj);
	print_close(pctx);
}

isc_boolean_t
cfg_obj_ismap(cfg_obj_t *obj) {
	REQUIRE(obj != NULL);
	return (ISC_TF(obj->type->rep == &cfg_rep_map));
}

isc_result_t
cfg_map_get(cfg_obj_t *mapobj, const char* name, cfg_obj_t **obj) {
	isc_result_t result;
	isc_symvalue_t val;
	cfg_map_t *map;
	
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

cfg_obj_t *
cfg_map_getname(cfg_obj_t *mapobj) {
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

	CHECK(create_cfgobj(pctx, &cfg_type_token, &obj));
	CHECK(cfg_gettoken(pctx, QSTRING));
	if (pctx->token.type == isc_tokentype_eof) {
		cfg_ungettoken(pctx);
		result = ISC_R_EOF;
		goto cleanup;
	}

	isc_lex_getlasttokentext(pctx->lexer, &pctx->token, &r);

	obj->value.string.base = isc_mem_get(pctx->mctx, r.length + 1);
	obj->value.string.length = r.length;
	memcpy(obj->value.string.base, r.base, r.length);
	obj->value.string.base[r.length] = '\0';
	*ret = obj;

 cleanup:
	return (result);
}

static cfg_type_t cfg_type_token = {
	"token", parse_token, print_ustring, &cfg_rep_string, NULL };

/*
 * An unsupported option.  This is just a list of tokens with balanced braces
 * ending in a semicolon.
 */

static isc_result_t
parse_unsupported(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	cfg_obj_t *listobj = NULL;
	isc_result_t result;
	int braces = 0;

	CHECK(create_list(pctx, type, &listobj));

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
			parser_error(pctx, LOG_NEAR, "unexpected token");
			result = ISC_R_UNEXPECTEDTOKEN;
			goto cleanup;
		}

		CHECK(parse_list_elt(pctx, &cfg_type_token, &elt));
		ISC_LIST_APPEND(listobj->value.list, elt, link);
	}
	INSIST(braces == 0);
	*ret = listobj;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(listobj);
	return (result);
}

static cfg_type_t cfg_type_unsupported = {
	"unsupported", parse_unsupported, print_spacelist,
	&cfg_rep_list, NULL
};

/*
 * A "controls" statement is represented as a map with the multivalued
 * "inet" and "unix" clauses.  Inet controls are tuples; unix controls
 * are cfg_unsupported_t objects.
 */

static keyword_type_t controls_allow_kw = {
	"allow", &cfg_type_bracketed_aml };
static cfg_type_t cfg_type_controls_allow = {
	"controls_allow", parse_keyvalue,
	print_keyvalue, &cfg_rep_list, &controls_allow_kw
};

static keyword_type_t controls_keys_kw = {
	"keys", &cfg_type_keylist };
static cfg_type_t cfg_type_controls_keys = {
	"controls_keys", parse_optional_keyvalue,
	print_keyvalue, &cfg_rep_list, &controls_keys_kw
};

static cfg_tuplefielddef_t inetcontrol_fields[] = {
	{ "address", &cfg_type_controls_sockaddr, 0 },
	{ "allow", &cfg_type_controls_allow, 0 },
	{ "keys", &cfg_type_controls_keys, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_inetcontrol = {
	"inetcontrol", parse_tuple, print_tuple, &cfg_rep_tuple,
	inetcontrol_fields
};

static cfg_clausedef_t
controls_clauses[] = {
	{ "inet", &cfg_type_inetcontrol, CFG_CLAUSEFLAG_MULTI },
	{ "unix", &cfg_type_unsupported,
	  CFG_CLAUSEFLAG_MULTI|CFG_CLAUSEFLAG_NOTIMP },
	{ NULL, NULL, 0 }
};
static cfg_clausedef_t *
controls_clausesets[] = {
	controls_clauses,
	NULL
};
static cfg_type_t cfg_type_controls = {
	"controls", parse_map, print_map, &cfg_rep_map,	&controls_clausesets
};

/*
 * An optional class, as used in view and zone statements.
 */
static isc_result_t
parse_optional_class(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;
	UNUSED(type);
	CHECK(cfg_peektoken(pctx, 0));
	if (pctx->token.type == isc_tokentype_string)
		CHECK(parse(pctx, &cfg_type_ustring, ret));
	else
		CHECK(parse(pctx, &cfg_type_void, ret));
 cleanup:
	return (result);
}

static cfg_type_t cfg_type_optional_class = {
	"optional_class", parse_optional_class, NULL, NULL, NULL };


/*
 * Try interpreting the current token as a network address.
 *
 * If WILDOK is set in flags, "*" can be used as a wildcard
 * and at least one of V4OK and V6OK must also be set.  The
 * "*" is interpreted as the IPv4 wildcard address if V4OK is 
 * set (including the case where V4OK and V6OK are both set),
 * and the IPv6 wildcard address otherwise.
 */
static isc_result_t
token_addr(cfg_parser_t *pctx, unsigned int flags, isc_netaddr_t *na) {
	char *s;
	struct in_addr in4a;
	struct in6_addr in6a;

	if (pctx->token.type != isc_tokentype_string)
		return (ISC_R_UNEXPECTEDTOKEN);

	s = pctx->token.value.as_pointer;
	if ((flags & WILDOK) != 0 && strcmp(s, "*") == 0) {
		if ((flags & V4OK) != 0) {
			isc_netaddr_any(na);
			return (ISC_R_SUCCESS);
		} else if ((flags & V6OK) != 0) {
			isc_netaddr_any6(na);
			return (ISC_R_SUCCESS);
		} else {
			INSIST(0);
		}
	} else {
		if ((flags & (V4OK | V4PREFIXOK)) != 0) {
			if (inet_pton(AF_INET, s, &in4a) == 1) {
				isc_netaddr_fromin(na, &in4a);
				return (ISC_R_SUCCESS);
			}
		}
		if ((flags & V4PREFIXOK) != 0 &&
		    strlen(s) <= 15) {
			char buf[64];
			int i;

			strcpy(buf, s);
			for (i = 0; i < 3; i++) {
				strcat(buf, ".0");
				if (inet_pton(AF_INET, buf, &in4a) == 1) {
					isc_netaddr_fromin(na, &in4a);
					return (ISC_R_SUCCESS);
				}
			}
		}
		if (flags & V6OK) {
			if (inet_pton(AF_INET6, s, &in6a) == 1) {
				isc_netaddr_fromin6(na, &in6a);
				return (ISC_R_SUCCESS);
			}
		}
	}
	return (ISC_R_UNEXPECTEDTOKEN);
}

static isc_result_t
get_addr(cfg_parser_t *pctx, unsigned int flags, isc_netaddr_t *na) {
	isc_result_t result;
	CHECK(cfg_gettoken(pctx, 0));
	result = token_addr(pctx, flags, na);
	if (result == ISC_R_UNEXPECTEDTOKEN)
		parser_error(pctx, LOG_NEAR, "expected IP address");
 cleanup:
	return (result);
}

static isc_boolean_t
looking_at_netaddr(cfg_parser_t *pctx, unsigned int flags) {
	isc_result_t result;
	isc_netaddr_t na_dummy;
	result = token_addr(pctx, flags, &na_dummy);
	return (ISC_TF(result == ISC_R_SUCCESS));
}

static isc_result_t
get_port(cfg_parser_t *pctx, unsigned int flags, in_port_t *port) {
	isc_result_t result;

	CHECK(cfg_gettoken(pctx, ISC_LEXOPT_NUMBER));

	if ((flags & WILDOK) != 0 &&
	    pctx->token.type == isc_tokentype_string &&
	    strcmp(pctx->token.value.as_pointer, "*") == 0) {
		*port = 0;
		return (ISC_R_SUCCESS);
	}
	if (pctx->token.type != isc_tokentype_number) {
		parser_error(pctx, LOG_NEAR,
			     "expected port number or '*'");
		return (ISC_R_UNEXPECTEDTOKEN);
	}
	if (pctx->token.value.as_ulong >= 65536) {
		parser_error(pctx, LOG_NEAR,
			     "port number out of range");
		return (ISC_R_UNEXPECTEDTOKEN);
	}
	*port = (in_port_t)(pctx->token.value.as_ulong);
	return (ISC_R_SUCCESS);
 cleanup:
	return (result);
}

static isc_result_t
parse_querysource(cfg_parser_t *pctx, int flags, cfg_obj_t **ret) {
	isc_result_t result;
	cfg_obj_t *obj = NULL;
	isc_netaddr_t netaddr;
	in_port_t port;
	unsigned int have_address = 0;
	unsigned int have_port = 0;

	if ((flags & V4OK) != 0)
		isc_netaddr_any(&netaddr);
	else if ((flags & V6OK) != 0)
		isc_netaddr_any6(&netaddr);
	else
		INSIST(0);

	port = 0;

	CHECK(create_cfgobj(pctx, &cfg_type_querysource, &obj));
	for (;;) {
		CHECK(cfg_peektoken(pctx, 0));
		if (pctx->token.type == isc_tokentype_string) {
			if (strcasecmp(pctx->token.value.as_pointer,
				       "address") == 0)
			{
				/* read "address" */
				CHECK(cfg_gettoken(pctx, 0)); 
				CHECK(get_addr(pctx, flags|WILDOK, &netaddr));
				have_address++;
			} else if (strcasecmp(pctx->token.value.as_pointer,
					      "port") == 0)
			{
				/* read "port" */
				CHECK(cfg_gettoken(pctx, 0)); 
				CHECK(get_port(pctx, WILDOK, &port));
				have_port++;
			} else {
				parser_error(pctx, LOG_NEAR,
					     "expected 'address' or 'port'");
				return (ISC_R_UNEXPECTEDTOKEN);
			}
		} else
			break;
	}
	if (have_address > 1 || have_port > 1 ||
	    have_address + have_port == 0) {
		parser_error(pctx, 0, "expected one address and/or port");
		return (ISC_R_UNEXPECTEDTOKEN);
	}

	isc_sockaddr_fromnetaddr(&obj->value.sockaddr, &netaddr, port);
	*ret = obj;
	return (ISC_R_SUCCESS);

 cleanup:
	parser_error(pctx, LOG_NEAR, "invalid query source");
	CLEANUP_OBJ(obj);
	return (result);
}

static isc_result_t
parse_querysource4(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	UNUSED(type);
	return (parse_querysource(pctx, V4OK, ret));
}

static isc_result_t
parse_querysource6(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	UNUSED(type);
	return (parse_querysource(pctx, V6OK, ret));
}

static void
print_isc_netaddr(cfg_printer_t *pctx, isc_netaddr_t *na) {
	isc_result_t result;
	char text[128];
	isc_buffer_t buf;

	isc_buffer_init(&buf, text, sizeof(text));
	result = isc_netaddr_totext(na, &buf);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	print(pctx, isc_buffer_base(&buf), isc_buffer_usedlength(&buf));
}

static void
print_querysource(cfg_printer_t *pctx, cfg_obj_t *obj) {
	isc_netaddr_t na;
	isc_netaddr_fromsockaddr(&na, &obj->value.sockaddr);
	print(pctx, "address ", 8);
	print_isc_netaddr(pctx, &na);
	print(pctx, " port ", 6);
	print_uint(pctx, isc_sockaddr_getport(&obj->value.sockaddr));
}

static cfg_type_t cfg_type_querysource4 = {
	"querysource4", parse_querysource4, NULL, NULL, NULL };
static cfg_type_t cfg_type_querysource6 = {
	"querysource6", parse_querysource6, NULL, NULL, NULL };
static cfg_type_t cfg_type_querysource = {
	"querysource", NULL, print_querysource, &cfg_rep_sockaddr, NULL };

/* netaddr */

static isc_result_t
parse_netaddr(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;
	cfg_obj_t *obj = NULL;
	isc_netaddr_t netaddr;
	UNUSED(type);
	CHECK(create_cfgobj(pctx, type, &obj));
	CHECK(get_addr(pctx, V4OK|V6OK, &netaddr));
	isc_sockaddr_fromnetaddr(&obj->value.sockaddr, &netaddr, 0);
	*ret = obj;
	return (ISC_R_SUCCESS);
 cleanup:
	CLEANUP_OBJ(obj);
	return (result);
}

static cfg_type_t cfg_type_netaddr = {
	"netaddr", parse_netaddr, print_sockaddr, &cfg_rep_sockaddr, NULL };

/* netprefix */

static isc_result_t
parse_netprefix(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	cfg_obj_t *obj = NULL;
	isc_result_t result;
	isc_netaddr_t netaddr;
	unsigned int addrlen, prefixlen;
	UNUSED(type);

	CHECK(get_addr(pctx, V4OK|V4PREFIXOK|V6OK, &netaddr));
	switch (netaddr.family) {
	case AF_INET:
		addrlen = 32;
		break;
	case AF_INET6:
		addrlen = 128;
		break;
	default:
		addrlen = 0;
		INSIST(0);
		break;
	}
	CHECK(cfg_peektoken(pctx, 0));
	if (pctx->token.type == isc_tokentype_special &&
	    pctx->token.value.as_char == '/') {
		CHECK(cfg_gettoken(pctx, 0)); /* read "/" */
		CHECK(cfg_gettoken(pctx, ISC_LEXOPT_NUMBER));
		if (pctx->token.type != isc_tokentype_number) {
			parser_error(pctx, LOG_NEAR,
				     "expected prefix length");
			return (ISC_R_UNEXPECTEDTOKEN);
		}
		prefixlen = pctx->token.value.as_ulong;
		if (prefixlen > addrlen) {
			parser_error(pctx, LOG_NOPREP,
				     "invalid prefix length");
			return (ISC_R_RANGE);
		}
	} else {
		prefixlen = addrlen;
	}
	CHECK(create_cfgobj(pctx, &cfg_type_netprefix, &obj));
	obj->value.netprefix.address = netaddr;
	obj->value.netprefix.prefixlen = prefixlen;
	*ret = obj;
	return (ISC_R_SUCCESS);
 cleanup:
	parser_error(pctx, LOG_NEAR, "expected network prefix");
	return (result);
}

static void
print_netprefix(cfg_printer_t *pctx, cfg_obj_t *obj) {
	cfg_netprefix_t *p = &obj->value.netprefix;
	print_isc_netaddr(pctx, &p->address);
	print(pctx, "/", 1);
	print_uint(pctx, p->prefixlen);
}

isc_boolean_t
cfg_obj_isnetprefix(cfg_obj_t *obj) {
	REQUIRE(obj != NULL);
	return (ISC_TF(obj->type->rep == &cfg_rep_netprefix));
}

void
cfg_obj_asnetprefix(cfg_obj_t *obj, isc_netaddr_t *netaddr,
		    unsigned int *prefixlen) {
	REQUIRE(obj != NULL && obj->type->rep == &cfg_rep_netprefix);
	*netaddr = obj->value.netprefix.address;
	*prefixlen = obj->value.netprefix.prefixlen;
}

static cfg_type_t cfg_type_netprefix = {
	"netprefix", parse_netprefix, print_netprefix, &cfg_rep_netprefix, NULL };

/* addrmatchelt */

static isc_result_t
parse_addrmatchelt(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
        isc_result_t result;
	UNUSED(type);

	CHECK(cfg_peektoken(pctx, QSTRING));

	if (pctx->token.type == isc_tokentype_string ||
	    pctx->token.type == isc_tokentype_qstring) {
		if (pctx->token.type == isc_tokentype_string &&
		    (strcasecmp(pctx->token.value.as_pointer, "key") == 0)) {
			CHECK(parse(pctx, &cfg_type_keyref, ret));
		} else {
			if (looking_at_netaddr(pctx, V4OK|V4PREFIXOK|V6OK)) {
				CHECK(parse_netprefix(pctx, NULL, ret));
			} else {
				CHECK(parse_astring(pctx, NULL, ret));
			}
		}
	} else if (pctx->token.type == isc_tokentype_special) {
		if (pctx->token.value.as_char == '{') {
			/* Nested match list. */
			CHECK(parse(pctx, &cfg_type_bracketed_aml, ret));
		} else if (pctx->token.value.as_char == '!') {
			CHECK(cfg_gettoken(pctx, 0)); /* read "!" */
			CHECK(parse(pctx, &cfg_type_negated, ret));
		} else {
			goto bad;
		}
	} else {
	bad:
		parser_error(pctx, LOG_NEAR,
			     "expected IP match list element");
		return (ISC_R_UNEXPECTEDTOKEN);
	}
 cleanup:
	return (result);
}

/*
 * A negated address match list element (like "! 10.0.0.1").
 * Somewhat sneakily, the caller is expected to parse the
 * "!", but not to print it.
 */

static cfg_tuplefielddef_t negated_fields[] = {
	{ "value", &cfg_type_addrmatchelt, 0 },
	{ NULL, NULL, 0 }
};

static void
print_negated(cfg_printer_t *pctx, cfg_obj_t *obj) {
	print(pctx, "!", 1);
	print_tuple(pctx, obj);
}

static cfg_type_t cfg_type_negated = {
	"negated", parse_tuple, print_negated, &cfg_rep_tuple,
	&negated_fields
};

/* an address match list element */

static cfg_type_t cfg_type_addrmatchelt = {
	"address_match_element", parse_addrmatchelt, NULL, NULL, NULL };
static cfg_type_t cfg_type_bracketed_aml = {
	"bracketed_aml", parse_bracketed_list, print_bracketed_list,
	&cfg_rep_list, &cfg_type_addrmatchelt
};

static isc_result_t
parse_sockaddrsub(cfg_parser_t *pctx, const cfg_type_t *type,
		  int flags, cfg_obj_t **ret)
{
	isc_result_t result;
	isc_netaddr_t netaddr;
	in_port_t port = 0;
	cfg_obj_t *obj = NULL;

	CHECK(create_cfgobj(pctx, type, &obj));
	CHECK(get_addr(pctx, flags, &netaddr));
	CHECK(cfg_peektoken(pctx, 0));
	if (pctx->token.type == isc_tokentype_string &&
	    strcasecmp(pctx->token.value.as_pointer, "port") == 0) {
		CHECK(cfg_gettoken(pctx, 0)); /* read "port" */
		CHECK(get_port(pctx, flags, &port));
	}
	isc_sockaddr_fromnetaddr(&obj->value.sockaddr, &netaddr, port);
	*ret = obj;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(obj);
	return (result);
}

static isc_result_t
parse_sockaddr(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	const unsigned int *flagp = type->of;
	return (parse_sockaddrsub(pctx, &cfg_type_sockaddr4wild, *flagp, ret));
}

static void
print_sockaddr(cfg_printer_t *pctx, cfg_obj_t *obj) {
	isc_netaddr_t netaddr;
	in_port_t port;
	char buf[ISC_NETADDR_FORMATSIZE];

	isc_netaddr_fromsockaddr(&netaddr, &obj->value.sockaddr);
	isc_netaddr_format(&netaddr, buf, sizeof(buf));
	print_cstr(pctx, buf);
	port = isc_sockaddr_getport(&obj->value.sockaddr);
	if (port != 0) {
		print(pctx, " port ", 6);
		print_uint(pctx, port);
	}
}

isc_boolean_t
cfg_obj_issockaddr(cfg_obj_t *obj) {
	REQUIRE(obj != NULL);
	return (ISC_TF(obj->type->rep == &cfg_rep_sockaddr));
}

isc_sockaddr_t *
cfg_obj_assockaddr(cfg_obj_t *obj) {
	REQUIRE(obj != NULL && obj->type->rep == &cfg_rep_sockaddr);
	return (&obj->value.sockaddr);
}

/* An IPv4/IPv6 address with optional port, "*" accepted as wildcard. */
static unsigned int sockaddr4wild_flags = WILDOK|V4OK;
static cfg_type_t cfg_type_sockaddr4wild = {
	"sockaddr4wild", parse_sockaddr, print_sockaddr,
	&cfg_rep_sockaddr, &sockaddr4wild_flags
};

static unsigned int sockaddr6wild_flags = WILDOK|V6OK;
static cfg_type_t cfg_type_sockaddr6wild = {
	"v6addrportwild", parse_sockaddr, print_sockaddr,
	&cfg_rep_sockaddr, &sockaddr6wild_flags
};

static unsigned int sockaddr_flags = V4OK|V6OK;
static cfg_type_t cfg_type_sockaddr = {
	"sockaddr", parse_sockaddr, print_sockaddr,
	&cfg_rep_sockaddr, &sockaddr_flags
};

/*
 * The socket address syntax in the "controls" statement is silly.
 * It allows both socket address families, but also allows "*",
 * whis is gratuitously interpreted as the IPv4 wildcard address.
 */
static unsigned int controls_sockaddr_flags = V4OK|V6OK|WILDOK;
static cfg_type_t cfg_type_controls_sockaddr = {
	"controls_sockaddr", parse_sockaddr, print_sockaddr,
	&cfg_rep_sockaddr, &controls_sockaddr_flags };


/*
 * Handle the special kludge syntax of the "keys" clause in the "server"
 * statement, which takes a single key with our without braces and semicolon.
 */
static isc_result_t
parse_server_key_kludge(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret)
{
	isc_result_t result;
	isc_boolean_t braces = ISC_FALSE;
	UNUSED(type);

	/* Allow opening brace. */
	CHECK(cfg_peektoken(pctx, 0));
	if (pctx->token.type == isc_tokentype_special &&
	    pctx->token.value.as_char == '{') {
		result = cfg_gettoken(pctx, 0);
		braces = ISC_TRUE;
	}

	CHECK(parse(pctx, &cfg_type_astring, ret));

	if (braces) {
		/* Skip semicolon if present. */
		CHECK(cfg_peektoken(pctx, 0));
		if (pctx->token.type == isc_tokentype_special &&
		    pctx->token.value.as_char == ';')
			CHECK(cfg_gettoken(pctx, 0));

		CHECK(parse_special(pctx, '}'));
	}
 cleanup:
	return (result);
}
static cfg_type_t cfg_type_server_key_kludge = {
	"server_key", parse_server_key_kludge, NULL, NULL, NULL };


/*
 * An optional logging facility.
 */

static isc_result_t
parse_optional_facility(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret)
{
	isc_result_t result;
	UNUSED(type);

	CHECK(cfg_peektoken(pctx, QSTRING));
	if (pctx->token.type == isc_tokentype_string ||
	    pctx->token.type == isc_tokentype_qstring) {
		CHECK(parse(pctx, &cfg_type_astring, ret));
	} else {
		CHECK(parse(pctx, &cfg_type_void, ret));
	}
 cleanup:
	return (result);
}

static cfg_type_t cfg_type_optional_facility = {
	"optional_facility", parse_optional_facility, NULL, NULL, NULL };


/*
 * A log severity.  Return as a string, except "debug N",
 * which is returned as a keyword object.
 */

static keyword_type_t debug_kw = { "debug", &cfg_type_uint32 };
static cfg_type_t cfg_type_debuglevel = {
	"debuglevel", parse_keyvalue,
	print_keyvalue, &cfg_rep_uint32, &debug_kw
};

static isc_result_t
parse_logseverity(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;
	UNUSED(type);

	CHECK(cfg_peektoken(pctx, 0));
	if (pctx->token.type == isc_tokentype_string &&
	    strcasecmp(pctx->token.value.as_pointer, "debug") == 0) {
		CHECK(cfg_gettoken(pctx, 0)); /* read "debug" */
		CHECK(cfg_peektoken(pctx, ISC_LEXOPT_NUMBER));
		if (pctx->token.type == isc_tokentype_number) {
			CHECK(parse_uint32(pctx, NULL, ret));
		} else {
			/*
			 * The debug level is optional and defaults to 1.
			 * This makes little sense, but we support it for
			 * compatibility with BIND 8.
			 */
			CHECK(create_cfgobj(pctx, &cfg_type_uint32, ret));
			(*ret)->value.uint32 = 1;
		}
		(*ret)->type = &cfg_type_debuglevel; /* XXX kludge */
	} else {
		CHECK(parse(pctx, &cfg_type_loglevel, ret));
	}
 cleanup:
	return (result);
}

static cfg_type_t cfg_type_logseverity = {
	"logseverity", parse_logseverity, NULL, NULL, NULL };

/*
 * The "file" clause of the "channel" statement.
 * This is yet another special case.
 */

static const char *logversions_enums[] = { "unlimited", NULL };
static isc_result_t
parse_logversions(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	return (parse_enum_or_other(pctx, type, &cfg_type_uint32, ret));
}
static cfg_type_t cfg_type_logversions = {
	"logversions", parse_logversions, print_ustring, 
	&cfg_rep_string, logversions_enums
};

static cfg_tuplefielddef_t logfile_fields[] = {
	{ "file", &cfg_type_qstring, 0 },
	{ "versions", &cfg_type_logversions, 0 },
	{ "size", &cfg_type_size, 0 },
	{ NULL, NULL, 0 }
};

static isc_result_t
parse_logfile(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	isc_result_t result;
	cfg_obj_t *obj = NULL;
	const cfg_tuplefielddef_t *fields = type->of;	

	CHECK(create_tuple(pctx, type, &obj));	

	/* Parse the mandatory "file" field */
	CHECK(parse(pctx, fields[0].type, &obj->value.tuple[0]));

	/* Parse "versions" and "size" fields in any order. */
	for (;;) {
		CHECK(cfg_peektoken(pctx, 0));
		if (pctx->token.type == isc_tokentype_string) {
			CHECK(cfg_gettoken(pctx, 0));		
			if (strcasecmp(pctx->token.value.as_pointer,
				       "versions") == 0 &&
			    obj->value.tuple[1] == NULL) {
				CHECK(parse(pctx, fields[1].type,
					    &obj->value.tuple[1]));
			} else if (strcasecmp(pctx->token.value.as_pointer,
					      "size") == 0 &&
				   obj->value.tuple[2] == NULL) {
				CHECK(parse(pctx, fields[2].type,
					    &obj->value.tuple[2]));
			} else {
				break;
			}
		} else {
			break;
		}
	}

	/* Create void objects for missing optional values. */
	if (obj->value.tuple[1] == NULL)
		CHECK(parse_void(pctx, NULL, &obj->value.tuple[1]));
	if (obj->value.tuple[2] == NULL)
		CHECK(parse_void(pctx, NULL, &obj->value.tuple[2]));

	*ret = obj;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(obj);	
	return (result);
}

static void
print_logfile(cfg_printer_t *pctx, cfg_obj_t *obj) {
	print_obj(pctx, obj->value.tuple[0]); /* file */
	if (obj->value.tuple[1]->type->print != print_void) {
		print(pctx, " versions ", 10);
		print_obj(pctx, obj->value.tuple[1]);
	}
	if (obj->value.tuple[2]->type->print != print_void) {
		print(pctx, " size ", 6);
		print_obj(pctx, obj->value.tuple[2]);
	}
}

static cfg_type_t cfg_type_logfile = {
	"logfile", parse_logfile, print_logfile, &cfg_rep_tuple,
	logfile_fields
};


/*
 * lwres
 */

static cfg_tuplefielddef_t lwres_view_fields[] = {
	{ "name", &cfg_type_astring, 0 },
	{ "class", &cfg_type_optional_class, 0 },
	{ NULL, NULL, 0 }
};
static cfg_type_t cfg_type_lwres_view = {
	"lwres_view", parse_tuple, print_tuple, &cfg_rep_tuple,
	lwres_view_fields
};

static cfg_type_t cfg_type_lwres_searchlist = {
	"lwres_searchlist", parse_bracketed_list, print_bracketed_list,
	&cfg_rep_list, &cfg_type_astring };

static cfg_clausedef_t
lwres_clauses[] = {
	{ "listen-on", &cfg_type_portiplist, 0 },
	{ "view", &cfg_type_lwres_view, 0 },
	{ "search", &cfg_type_lwres_searchlist, 0 },
	{ "ndots", &cfg_type_uint32, 0 },
	{ NULL, NULL, 0 }
};

static cfg_clausedef_t *
lwres_clausesets[] = {
	lwres_clauses,
	NULL
};
static cfg_type_t cfg_type_lwres = {
	"lwres", parse_map, print_map, &cfg_rep_map, lwres_clausesets };

/*
 * rndc
 */

static cfg_clausedef_t
rndcconf_options_clauses[] = {
	{ "default-server", &cfg_type_astring, 0 },
	{ "default-key", &cfg_type_astring, 0 },
	{ "default-port", &cfg_type_uint32, 0 },
	{ NULL, NULL, 0 }
};

static cfg_clausedef_t *
rndcconf_options_clausesets[] = {
	rndcconf_options_clauses,
	NULL
};

static cfg_type_t cfg_type_rndcconf_options = {
	"rndcconf_options", parse_map, print_map, &cfg_rep_map,
	rndcconf_options_clausesets
};

static cfg_clausedef_t
rndcconf_server_clauses[] = {
	{ "key", &cfg_type_astring, 0 },
	{ "port", &cfg_type_uint32, 0 },
	{ NULL, NULL, 0 }
};

static cfg_clausedef_t *
rndcconf_server_clausesets[] = {
	rndcconf_server_clauses,
	NULL
};

static cfg_type_t cfg_type_rndcconf_server = {
	"rndcconf_server", parse_named_map, print_map, &cfg_rep_map,
	rndcconf_server_clausesets
};

static cfg_clausedef_t
rndcconf_clauses[] = {
	{ "key", &cfg_type_key, CFG_CLAUSEFLAG_MULTI },
	{ "server", &cfg_type_rndcconf_server, CFG_CLAUSEFLAG_MULTI },
	{ "options", &cfg_type_rndcconf_options, 0 },
	{ NULL, NULL, 0 }
};

static cfg_clausedef_t *
rndcconf_clausesets[] = {
	rndcconf_clauses,
	NULL
};

LIBISCCFG_EXTERNAL_DATA cfg_type_t cfg_type_rndcconf = {
	"rndcconf", parse_mapbody, print_mapbody, &cfg_rep_map,
	rndcconf_clausesets
};

static cfg_clausedef_t
rndckey_clauses[] = {
	{ "key", &cfg_type_key, 0 },
	{ NULL, NULL, 0 }
};

static cfg_clausedef_t *
rndckey_clausesets[] = {
	rndckey_clauses,
	NULL
};

LIBISCCFG_EXTERNAL_DATA cfg_type_t cfg_type_rndckey = {
	"rndckey", parse_mapbody, print_mapbody, &cfg_rep_map,
	rndckey_clausesets
};


static isc_result_t
cfg_gettoken(cfg_parser_t *pctx, int options) {
	isc_result_t result;

	if (pctx->seen_eof)
		return (ISC_R_SUCCESS);

	options |= (ISC_LEXOPT_EOF | ISC_LEXOPT_NOMORE);

 redo:
	pctx->token.type = isc_tokentype_unknown;
	result = isc_lex_gettoken(pctx->lexer, options, &pctx->token);
	pctx->ungotten = ISC_FALSE;
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
			pctx->seen_eof = ISC_TRUE;
		}
		break;

	case ISC_R_NOSPACE:
		/* More understandable than "ran out of space". */
		parser_error(pctx, LOG_NEAR, "token too big");
		break;

	case ISC_R_IOERROR:
		parser_error(pctx, 0, "%s",
				 isc_result_totext(result));
		break;

	default:
		parser_error(pctx, LOG_NEAR, "%s",
			     isc_result_totext(result));
		break;
	}
	return (result);
}

static void
cfg_ungettoken(cfg_parser_t *pctx) {
	if (pctx->seen_eof)
		return;
	isc_lex_ungettoken(pctx->lexer, &pctx->token);
	pctx->ungotten = ISC_TRUE;
}

static isc_result_t
cfg_peektoken(cfg_parser_t *pctx, int options) {
	isc_result_t result;
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

	result = cfg_gettoken(pctx, QSTRING);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (pctx->token.type != isc_tokentype_string &&
	    pctx->token.type != isc_tokentype_qstring) {
		parser_error(pctx, LOG_NEAR, "expected string");
		return (ISC_R_UNEXPECTEDTOKEN);
	}
	return (ISC_R_SUCCESS);
}

static void
parser_error(cfg_parser_t *pctx, unsigned int flags, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	parser_complain(pctx, ISC_FALSE, flags, fmt, args);
	va_end(args);
	pctx->errors++;
}

static void
parser_warning(cfg_parser_t *pctx, unsigned int flags, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	parser_complain(pctx, ISC_TRUE, flags, fmt, args);
	va_end(args);
	pctx->warnings++;
}

#define MAX_LOG_TOKEN 30 /* How much of a token to quote in log messages. */

static char *
current_file(cfg_parser_t *pctx) {
	static char none[] = "none";
	cfg_listelt_t *elt;
	cfg_obj_t *fileobj;

	if (pctx->open_files == NULL)
		return (none);
	elt = ISC_LIST_TAIL(pctx->open_files->value.list);
	if (elt == NULL)
	      return (none);

	fileobj = elt->obj;
	INSIST(fileobj->type == &cfg_type_qstring);
	return (fileobj->value.string.base);
}

static void
parser_complain(cfg_parser_t *pctx, isc_boolean_t is_warning,
		unsigned int flags, const char *format,
		va_list args)
{
	char tokenbuf[MAX_LOG_TOKEN + 10];
	static char where[ISC_DIR_PATHMAX + 100];
	static char message[2048];
	int level = ISC_LOG_ERROR;
	const char *prep = "";

	if (is_warning)
		level = ISC_LOG_WARNING;

	sprintf(where, "%s:%u: ", current_file(pctx), pctx->line);

	if ((unsigned int)vsprintf(message, format, args) >= sizeof message)
		FATAL_ERROR(__FILE__, __LINE__,
			    "error message would overflow");

	if ((flags & (LOG_NEAR|LOG_BEFORE|LOG_NOPREP)) != 0) {
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
		if (flags & LOG_NEAR)
			prep = " near ";
		else if (flags & LOG_BEFORE)
			prep = " before ";
		else
			prep = " ";
	} else {
		tokenbuf[0] = '\0';
	}
	isc_log_write(pctx->lctx, CAT, MOD, level,
		      "%s%s%s%s", where, message, prep, tokenbuf);
}

void
cfg_obj_log(cfg_obj_t *obj, isc_log_t *lctx, int level, const char *fmt, ...) {
	va_list ap;
	char msgbuf[2048];

	if (! isc_log_wouldlog(lctx, level))
		return;

	va_start(ap, fmt);

	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	isc_log_write(lctx, CAT, MOD, level,
		      "%s:%u: %s",
		      obj->file == NULL ? "<unknown file>" : obj->file,
		      obj->line, msgbuf);
	va_end(ap);
}

static isc_result_t
create_cfgobj(cfg_parser_t *pctx, const cfg_type_t *type, cfg_obj_t **ret) {
	cfg_obj_t *obj;

	obj = isc_mem_get(pctx->mctx, sizeof(cfg_obj_t));
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

	CHECK(create_cfgobj(pctx, type, &obj));
	CHECK(isc_symtab_create(pctx->mctx, 5, /* XXX */
				map_symtabitem_destroy,
				pctx, ISC_FALSE, &symtab));

	obj->value.map.symtab = symtab;
	obj->value.map.id = NULL;

	*ret = obj;
	return (ISC_R_SUCCESS);

 cleanup:
	CLEANUP_OBJ(obj);
	return (result);
}

static void
free_map(cfg_parser_t *pctx, cfg_obj_t *obj) {
	CLEANUP_OBJ(obj->value.map.id);
	isc_symtab_destroy(&obj->value.map.symtab);
}

isc_boolean_t
cfg_obj_istype(cfg_obj_t *obj, const cfg_type_t *type) {
	return (ISC_TF(obj->type == type));
}

/*
 * Destroy 'obj', a configuration object created in 'pctx'.
 */
void
cfg_obj_destroy(cfg_parser_t *pctx, cfg_obj_t **objp) {
	cfg_obj_t *obj = *objp;
	obj->type->rep->free(pctx, obj);
	isc_mem_put(pctx->mctx, obj, sizeof(cfg_obj_t));
	*objp = NULL;
}

static void
free_noop(cfg_parser_t *pctx, cfg_obj_t *obj) {
	UNUSED(pctx);
	UNUSED(obj);
}

/*
 * Data and functions for printing grammar summaries.
 */
static struct flagtext {
	unsigned int flag;
	const char *text;
} flagtexts[] = {
	{ CFG_CLAUSEFLAG_NOTIMP, "not implemented" },
	{ CFG_CLAUSEFLAG_NYI, "not yet implemented" },
	{ CFG_CLAUSEFLAG_OBSOLETE, "obsolete" },
	{ CFG_CLAUSEFLAG_NEWDEFAULT, "default changed" },
	{ 0, NULL }
};

static void
print_clause_flags(cfg_printer_t *pctx, unsigned int flags) {
	struct flagtext *p;
	isc_boolean_t first = ISC_TRUE;
	for (p = flagtexts; p->flag != 0; p++) {
		if ((flags & p->flag) != 0) {
			if (first)
				print(pctx, " // ", 4);
			else
				print(pctx, ", ", 2);
			print_cstr(pctx, p->text);
			first = ISC_FALSE;
		}
	}
}

static void
print_grammar(cfg_printer_t *pctx, const cfg_type_t *type) {
	if (type->print == print_mapbody) {
		const cfg_clausedef_t * const *clauseset;
		const cfg_clausedef_t *clause;

		for (clauseset = type->of; *clauseset != NULL; clauseset++) {
			for (clause = *clauseset;
			     clause->name != NULL;
		     clause++) {
				print_cstr(pctx, clause->name);
				print(pctx, " ", 1);
				print_grammar(pctx, clause->type);
				print(pctx, ";", 1);
				/* XXX print flags here? */
				print(pctx, "\n\n", 2);
			}
		}
	} else if (type->print == print_map) {
		const cfg_clausedef_t * const *clauseset;
		const cfg_clausedef_t *clause;

		if (type->parse == parse_named_map) {
			print_grammar(pctx, &cfg_type_astring);
			print(pctx, " ", 1);
		}
		
		print_open(pctx);

		for (clauseset = type->of; *clauseset != NULL; clauseset++) {
			for (clause = *clauseset;
			     clause->name != NULL;
			     clause++) {
				print_indent(pctx);
				print_cstr(pctx, clause->name);
				if (clause->type->print != print_void)
					print(pctx, " ", 1);
				print_grammar(pctx, clause->type);
				print(pctx, ";", 1);
				print_clause_flags(pctx, clause->flags);
				print(pctx, "\n", 1);
			}
		}
		print_close(pctx);
	} else if (type->print == print_tuple) {
		const cfg_tuplefielddef_t *fields = type->of;
		const cfg_tuplefielddef_t *f;
		isc_boolean_t need_space = ISC_FALSE;

		for (f = fields; f->name != NULL; f++) {
			if (need_space)
				print(pctx, " ", 1);
			print_grammar(pctx, f->type);
			need_space = ISC_TF(f->type->print != print_void);
		}
	} else if (type->parse == parse_enum) {
		const char * const *p;
		print(pctx, "( ", 2);
		for (p = type->of; *p != NULL; p++) {
			print_cstr(pctx, *p);
			if (p[1] != NULL)
				print(pctx, " | ", 3);
		}
		print(pctx, " )", 2);
	} else if (type->print == print_bracketed_list) {
		print(pctx, "{ ", 2);
		print_grammar(pctx, type->of);
		print(pctx, "; ... }", 7);
	} else if (type->parse == parse_keyvalue) {
		const keyword_type_t *kw = type->of;
		print_cstr(pctx, kw->name);
		print(pctx, " ", 1);
		print_grammar(pctx, kw->type);
	} else if (type->parse == parse_optional_keyvalue) {
		const keyword_type_t *kw = type->of;
		print(pctx, "[ ", 2);
		print_cstr(pctx, kw->name);
		print(pctx, " ", 1);
		print_grammar(pctx, kw->type);
		print(pctx, " ]", 2);
	} else if (type->parse == parse_sockaddr) {
		const unsigned int *flagp = type->of;
		int n = 0;
		print(pctx, "( ", 2);
		if (*flagp & V4OK) {
			if (n != 0)
				print(pctx, " | ", 3);
			print_cstr(pctx, "<ipv4_address>");
			n++;
		}
		if (*flagp & V6OK) {
			if (n != 0)
				print(pctx, " | ", 3);
			print_cstr(pctx, "<ipv6_address>");
			n++;			
		}
		if (*flagp & WILDOK) {
			if (n != 0)
				print(pctx, " | ", 3);
			print(pctx, "*", 1);
			n++;
		}
		print(pctx, " ) ", 3);
		if (*flagp & WILDOK) {
			print_cstr(pctx, "[ port ( <integer> | * ) ]");
		} else {
			print_cstr(pctx, "[ port <integer> ]");
		}
	} else if (type->print == print_void) {
		/* Print nothing. */
	} else {
		print(pctx, "<", 1);
		print_cstr(pctx, type->name);
		print(pctx, ">", 1);
	}
}

void
cfg_print_grammar(const cfg_type_t *type,
	void (*f)(void *closure, const char *text, int textlen),
	void *closure)
{
	cfg_printer_t pctx;
	pctx.f = f;
	pctx.closure = closure;
	pctx.indent = 0;
	print_grammar(&pctx, type);
}
