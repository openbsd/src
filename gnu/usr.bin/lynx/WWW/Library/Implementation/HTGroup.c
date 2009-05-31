/* MODULE							HTGroup.c
 *		GROUP FILE ROUTINES
 *
 *	Contains group file parser and routines to match IP
 *	address templates and to find out group membership.
 *
 *
 * AUTHORS:
 *	AL	Ari Luotonen	luotonen@dxcern.cern.ch
 *
 * HISTORY:
 *
 *
 * BUGS:
 *
 *
 *
 * GROUP DEFINITION GRAMMAR:
 *
 *	string = "sequence of alphanumeric characters"
 *	user_name ::= string
 *	group_name ::= string
 *	group_ref ::= group_name
 *	user_def ::= user_name | group_ref
 *	user_def_list ::= user_def { ',' user_def }
 *	user_part = user_def | '(' user_def_list ')'
 *
 *	templ = "sequence of alphanumeric characters and '*'s"
 *	ip_number_mask ::= templ '.' templ '.' templ '.' templ
 *	domain_name_mask ::= templ { '.' templ }
 *	address ::= ip_number_mask | domain_name_mask
 *	address_def ::= address
 *	address_def_list ::= address_def { ',' address_def }
 *	address_part = address_def | '(' address_def_list ')'
 *
 *	item ::= [user_part] ['@' address_part]
 *	item_list ::= item { ',' item }
 *	group_def ::= item_list
 *	group_decl ::= group_name ':' group_def
 *
 */

#include <HTUtils.h>

#include <HTAAUtil.h>
#include <HTLex.h>		/* Lexical analysor     */
#include <HTGroup.h>		/* Implemented here     */

#include <LYUtils.h>
#include <LYLeaks.h>

/*
 * Group file parser
 */

typedef HTList UserDefList;
typedef HTList AddressDefList;

typedef struct {
    UserDefList *user_def_list;
    AddressDefList *address_def_list;
} Item;

typedef struct {
    char *name;
    GroupDef *translation;
} Ref;

static void syntax_error(FILE *fp, const char *msg,
			 LexItem lex_item)
{
    char buffer[41];
    int cnt = 0;
    int ch;

    while ((ch = getc(fp)) != EOF && ch != '\n')
	if (cnt < 40)
	    buffer[cnt++] = (char) ch;
    buffer[cnt] = (char) 0;

    CTRACE((tfp, "%s %d before: '%s'\nHTGroup.c: %s (got %s)\n",
	    "HTGroup.c: Syntax error in rule file at line",
	    HTlex_line, buffer, msg, lex_verbose(lex_item)));
    HTlex_line++;
}

static AddressDefList *parse_address_part(FILE *fp)
{
    AddressDefList *address_def_list = NULL;
    LexItem lex_item;
    BOOL only_one = NO;

    lex_item = lex(fp);
    if (lex_item == LEX_ALPH_STR || lex_item == LEX_TMPL_STR)
	only_one = YES;
    else if (lex_item != LEX_OPEN_PAREN ||
	     ((lex_item = lex(fp)) != LEX_ALPH_STR &&
	      lex_item != LEX_TMPL_STR)) {
	syntax_error(fp, "Expecting a single address or '(' beginning list",
		     lex_item);
	return NULL;
    }
    address_def_list = HTList_new();

    for (;;) {
	Ref *ref = typecalloc(Ref);

	if (ref == NULL)
	    outofmem(__FILE__, "parse_address_part");
	ref->name = NULL;
	ref->translation = NULL;
	StrAllocCopy(ref->name, HTlex_buffer);

	HTList_addObject(address_def_list, (void *) ref);

	if (only_one || (lex_item = lex(fp)) != LEX_ITEM_SEP)
	    break;
	/*
	 * Here lex_item == LEX_ITEM_SEP; after item separator it
	 * is ok to have one or more newlines (LEX_REC_SEP) and
	 * they are ignored (continuation line).
	 */
	do {
	    lex_item = lex(fp);
	} while (lex_item == LEX_REC_SEP);

	if (lex_item != LEX_ALPH_STR && lex_item != LEX_TMPL_STR) {
	    syntax_error(fp, "Expecting an address template", lex_item);
	    HTList_delete(address_def_list);
	    address_def_list = NULL;
	    return NULL;
	}
    }

    if (!only_one && lex_item != LEX_CLOSE_PAREN) {
	HTList_delete(address_def_list);
	address_def_list = NULL;
	syntax_error(fp, "Expecting ')' closing address list", lex_item);
	return NULL;
    }
    return address_def_list;
}

static UserDefList *parse_user_part(FILE *fp)
{
    UserDefList *user_def_list = NULL;
    LexItem lex_item;
    BOOL only_one = NO;

    lex_item = lex(fp);
    if (lex_item == LEX_ALPH_STR)
	only_one = YES;
    else if (lex_item != LEX_OPEN_PAREN ||
	     (lex_item = lex(fp)) != LEX_ALPH_STR) {
	syntax_error(fp, "Expecting a single name or '(' beginning list",
		     lex_item);
	return NULL;
    }
    user_def_list = HTList_new();

    for (;;) {
	Ref *ref = typecalloc(Ref);

	if (ref == NULL)
	    outofmem(__FILE__, "parse_user_part");
	ref->name = NULL;
	ref->translation = NULL;
	StrAllocCopy(ref->name, HTlex_buffer);

	HTList_addObject(user_def_list, (void *) ref);

	if (only_one || (lex_item = lex(fp)) != LEX_ITEM_SEP)
	    break;
	/*
	 * Here lex_item == LEX_ITEM_SEP; after item separator it
	 * is ok to have one or more newlines (LEX_REC_SEP) and
	 * they are ignored (continuation line).
	 */
	do {
	    lex_item = lex(fp);
	} while (lex_item == LEX_REC_SEP);

	if (lex_item != LEX_ALPH_STR) {
	    syntax_error(fp, "Expecting user or group name", lex_item);
	    HTList_delete(user_def_list);
	    user_def_list = NULL;
	    return NULL;
	}
    }

    if (!only_one && lex_item != LEX_CLOSE_PAREN) {
	HTList_delete(user_def_list);
	user_def_list = NULL;
	syntax_error(fp, "Expecting ')' closing user/group list", lex_item);
	return NULL;
    }
    return user_def_list;
}

static Item *parse_item(FILE *fp)
{
    Item *item = NULL;
    UserDefList *user_def_list = NULL;
    AddressDefList *address_def_list = NULL;
    LexItem lex_item;

    lex_item = lex(fp);
    if (lex_item == LEX_ALPH_STR || lex_item == LEX_OPEN_PAREN) {
	unlex(lex_item);
	user_def_list = parse_user_part(fp);
	lex_item = lex(fp);
    }

    if (lex_item == LEX_AT_SIGN) {
	lex_item = lex(fp);
	if (lex_item == LEX_ALPH_STR || lex_item == LEX_TMPL_STR ||
	    lex_item == LEX_OPEN_PAREN) {
	    unlex(lex_item);
	    address_def_list = parse_address_part(fp);
	} else {
	    if (user_def_list) {
		HTList_delete(user_def_list);	/* @@@@ */
		user_def_list = NULL;
	    }
	    syntax_error(fp, "Expected address part (single address or list)",
			 lex_item);
	    return NULL;
	}
    } else
	unlex(lex_item);

    if (!user_def_list && !address_def_list) {
	syntax_error(fp, "Empty item not allowed", lex_item);
	return NULL;
    }
    item = typecalloc(Item);
    if (item == NULL)
	outofmem(__FILE__, "parse_item");
    item->user_def_list = user_def_list;
    item->address_def_list = address_def_list;
    return item;
}

static ItemList *parse_item_list(FILE *fp)
{
    ItemList *item_list = HTList_new();
    Item *item;
    LexItem lex_item;

    for (;;) {
	if (!(item = parse_item(fp))) {
	    HTList_delete(item_list);	/* @@@@ */
	    item_list = NULL;
	    return NULL;
	}
	HTList_addObject(item_list, (void *) item);
	lex_item = lex(fp);
	if (lex_item != LEX_ITEM_SEP) {
	    unlex(lex_item);
	    return item_list;
	}
	/*
	 * Here lex_item == LEX_ITEM_SEP; after item separator it
	 * is ok to have one or more newlines (LEX_REC_SEP) and
	 * they are ignored (continuation line).
	 */
	do {
	    lex_item = lex(fp);
	} while (lex_item == LEX_REC_SEP);
	unlex(lex_item);
    }
}

GroupDef *HTAA_parseGroupDef(FILE *fp)
{
    ItemList *item_list = NULL;
    GroupDef *group_def = NULL;
    LexItem lex_item;

    if (!(item_list = parse_item_list(fp))) {
	return NULL;
    }
    group_def = typecalloc(GroupDef);
    if (group_def == NULL)
	outofmem(__FILE__, "HTAA_parseGroupDef");
    group_def->group_name = NULL;
    group_def->item_list = item_list;

    if ((lex_item = lex(fp)) != LEX_REC_SEP) {
	syntax_error(fp, "Garbage after group definition", lex_item);
    }

    return group_def;
}

#if 0
static GroupDef *parse_group_decl(FILE *fp)
{
    char *group_name = NULL;
    GroupDef *group_def = NULL;
    LexItem lex_item;

    do {
	lex_item = lex(fp);
    } while (lex_item == LEX_REC_SEP);	/* Ignore empty lines */

    if (lex_item != LEX_ALPH_STR) {
	if (lex_item != LEX_EOF)
	    syntax_error(fp, "Expecting group name", lex_item);
	return NULL;
    }
    StrAllocCopy(group_name, HTlex_buffer);

    if (LEX_FIELD_SEP != (lex_item = lex(fp))) {
	syntax_error(fp, "Expecting field separator", lex_item);
	FREE(group_name);
	return NULL;
    }

    if (!(group_def = HTAA_parseGroupDef(fp))) {
	FREE(group_name);
	return NULL;
    }
    group_def->group_name = group_name;

    return group_def;
}

/*
 * Group manipulation routines
 */

static GroupDef *find_group_def(GroupDefList *group_list,
				const char *group_name)
{
    if (group_list && group_name) {
	GroupDefList *cur = group_list;
	GroupDef *group_def;

	while (NULL != (group_def = (GroupDef *) HTList_nextObject(cur))) {
	    if (!strcmp(group_name, group_def->group_name)) {
		return group_def;
	    }
	}
    }
    return NULL;
}

void HTAA_resolveGroupReferences(GroupDef *group_def,
				 GroupDefList *group_def_list)
{
    if (group_def && group_def->item_list && group_def_list) {
	ItemList *cur1 = group_def->item_list;
	Item *item;

	while (NULL != (item = (Item *) HTList_nextObject(cur1))) {
	    UserDefList *cur2 = item->user_def_list;
	    Ref *ref;

	    while (NULL != (ref = (Ref *) HTList_nextObject(cur2)))
		ref->translation = find_group_def(group_def_list, ref->name);

	    /* Does NOT translate address_def_list */
	}
    }
}

static void add_group_def(GroupDefList *group_def_list,
			  GroupDef *group_def)
{
    HTAA_resolveGroupReferences(group_def, group_def_list);
    HTList_addObject(group_def_list, (void *) group_def);
}

static GroupDefList *parse_group_file(FILE *fp)
{
    GroupDefList *group_def_list = HTList_new();
    GroupDef *group_def;

    while (NULL != (group_def = parse_group_decl(fp)))
	add_group_def(group_def_list, group_def);

    return group_def_list;
}
#endif

/*
 * Trace functions
 */

static void print_item(Item *item)
{
    if (!item)
	fprintf(tfp, "\tNULL-ITEM\n");
    else {
	UserDefList *cur1 = item->user_def_list;
	AddressDefList *cur2 = item->address_def_list;
	Ref *user_ref = (Ref *) HTList_nextObject(cur1);
	Ref *addr_ref = (Ref *) HTList_nextObject(cur2);

	if (user_ref) {
	    fprintf(tfp, "\t[%s%s", user_ref->name,
		    (user_ref->translation ? "*REF*" : ""));
	    while (NULL != (user_ref = (Ref *) HTList_nextObject(cur1)))
		fprintf(tfp, "; %s%s", user_ref->name,
			(user_ref->translation ? "*REF*" : ""));
	    fprintf(tfp, "] ");
	} else
	    fprintf(tfp, "\tANYBODY ");

	if (addr_ref) {
	    fprintf(tfp, "@ [%s", addr_ref->name);
	    while (NULL != (addr_ref = (Ref *) HTList_nextObject(cur2)))
		fprintf(tfp, "; %s", addr_ref->name);
	    fprintf(tfp, "]\n");
	} else
	    fprintf(tfp, "@ ANYADDRESS\n");
    }
}

static void print_item_list(ItemList *item_list)
{
    ItemList *cur = item_list;
    Item *item;

    if (!item_list)
	fprintf(tfp, "EMPTY");
    else
	while (NULL != (item = (Item *) HTList_nextObject(cur)))
	    print_item(item);
}

void HTAA_printGroupDef(GroupDef *group_def)
{
    if (!group_def) {
	fprintf(tfp, "\nNULL RECORD\n");
	return;
    }

    fprintf(tfp, "\nGroup %s:\n",
	    (group_def->group_name ? group_def->group_name : "NULL"));

    print_item_list(group_def->item_list);
    fprintf(tfp, "\n");
}

#if 0
static void print_group_def_list(GroupDefList *group_list)
{
    GroupDefList *cur = group_list;
    GroupDef *group_def;

    while (NULL != (group_def = (GroupDef *) HTList_nextObject(cur)))
	HTAA_printGroupDef(group_def);
}

/*
 * IP address template matching
 */

/* static						part_match()
 *		MATCH ONE PART OF INET ADDRESS AGAIST
 *		A PART OF MASK (inet address has 4 parts)
 * ON ENTRY:
 *	tcur	pointer to the beginning of template part.
 *	icur	pointer to the beginning of actual inet
 *		number part.
 *
 * ON EXIT:
 *	returns	YES, if match.
 */
static BOOL part_match(const char *tcur,
		       const char *icur)
{
    char required[4];
    char actual[4];
    const char *cur;
    int cnt;
    BOOL status;

    if (!tcur || !icur)
	return NO;

    cur = tcur;
    cnt = 0;
    while (cnt < 3 && *cur && *cur != '.')
	required[cnt++] = *(cur++);
    required[cnt] = (char) 0;

    cur = icur;
    cnt = 0;
    while (cnt < 3 && *cur && *cur != '.')
	actual[cnt++] = *(cur++);
    actual[cnt] = (char) 0;

    status = HTAA_templateMatch(required, actual);
    CTRACE((tfp, "part_match: req: '%s' act: '%s' match: %s\n",
	    required, actual, (status ? "yes" : "no")));

    return status;
}

/* static						ip_number_match()
 *		MATCH INET NUMBER AGAINST AN INET NUMBER MASK
 * ON ENTRY:
 *	template	mask to match agaist, e.g., 128.141.*.*
 *	the_inet_addr	actual inet address, e.g., 128.141.201.74
 *
 * ON EXIT:
 *	returns		YES, if match;  NO, if not.
 */
static BOOL ip_number_match(const char *ctemplate,
			    const char *the_inet_addr)
{
    const char *tcur = ctemplate;
    const char *icur = the_inet_addr;
    int cnt;

    for (cnt = 0; cnt < 4; cnt++) {
	if (!tcur || !icur || !part_match(tcur, icur))
	    return NO;
	if (NULL != (tcur = strchr(tcur, '.')))
	    tcur++;
	if (NULL != (icur = strchr(icur, '.')))
	    icur++;
    }
    return YES;
}

/* static						is_domain_mask()
 *		DETERMINE IF A GIVEN MASK IS A
 *		DOMAIN NAME MASK OR AN INET NUMBER MASK
 * ON ENTRY:
 *	mask	either a domain name mask,
 *		e.g.
 *			*.cern.ch
 *
 *		or an inet number mask,
 *		e.g.
 *			128.141.*.*
 *
 * ON EXIT:
 *	returns	YES, if mask is a domain name mask.
 *		NO, if it is an inet number mask.
 */
static BOOL is_domain_mask(const char *mask)
{
    const char *cur = mask;

    if (!mask)
	return NO;

    while (*cur) {
	if (*cur != '.' && *cur != '*' && (*cur < '0' || *cur > '9'))
	    return YES;		/* Even one non-digit makes it a domain name mask */
	cur++;
    }
    return NO;			/* All digits and dots, so it is an inet number mask */
}

/* static							ip_mask_match()
 *		MATCH AN IP NUMBER MASK OR IP NAME MASK
 *		AGAINST ACTUAL IP NUMBER OR IP NAME
 *
 * ON ENTRY:
 *	mask		mask.  Mask may be either an inet number
 *			mask or a domain name mask,
 *			e.g.
 *				128.141.*.*
 *			or
 *				*.cern.ch
 *
 *	ip_number	IP number of connecting host.
 *	ip_name		IP name of the connecting host.
 *
 * ON EXIT:
 *	returns		YES, if hostname/internet number
 *			matches the mask.
 *			NO, if no match (no fire).
 */
static BOOL ip_mask_match(const char *mask,
			  const char *ip_number,
			  const char *ip_name)
{
    if (mask && (ip_number || ip_name)) {
	if (is_domain_mask(mask)) {
	    if (HTAA_templateMatch(mask, ip_name))
		return YES;
	} else {
	    if (ip_number_match(mask, ip_number))
		return YES;
	}
    }
    return NO;
}

static BOOL ip_in_def_list(AddressDefList *address_def_list,
			   char *ip_number,
			   char *ip_name)
{
    if (address_def_list && (ip_number || ip_name)) {
	AddressDefList *cur = address_def_list;
	Ref *ref;

	while (NULL != (ref = (Ref *) HTList_nextObject(cur))) {
	    /* Value of ref->translation is ignored, i.e., */
	    /* no recursion for ip address tamplates.     */
	    if (ip_mask_match(ref->name, ip_number, ip_name))
		return YES;
	}
    }
    return NO;
}

/*
 * Group file cached reading
 */

typedef struct {
    char *group_filename;
    GroupDefList *group_list;
} GroupCache;

typedef HTList GroupCacheList;

static GroupCacheList *group_cache_list = NULL;

GroupDefList *HTAA_readGroupFile(const char *filename)
{
    FILE *fp;
    GroupCache *group_cache;

    if (isEmpty(filename))
	return NULL;

    if (!group_cache_list)
	group_cache_list = HTList_new();
    else {
	GroupCacheList *cur = group_cache_list;

	while (NULL != (group_cache = (GroupCache *) HTList_nextObject(cur))) {
	    if (!strcmp(filename, group_cache->group_filename)) {
		CTRACE((tfp, "%s '%s' %s\n",
			"HTAA_readGroupFile: group file",
			filename, "already found in cache"));
		return group_cache->group_list;
	    }			/* if cache match */
	}			/* while cached files remain */
    }				/* cache exists */

    CTRACE((tfp, "HTAA_readGroupFile: reading group file `%s'\n",
	    filename));

    if (!(fp = fopen(filename, TXT_R))) {
	CTRACE((tfp, "%s '%s'\n",
		"HTAA_readGroupFile: unable to open group file",
		filename));
	return NULL;
    }

    if ((group_cache = typecalloc(GroupCache)) == 0)
	outofmem(__FILE__, "HTAA_readGroupFile");

    group_cache->group_filename = NULL;
    StrAllocCopy(group_cache->group_filename, filename);
    group_cache->group_list = parse_group_file(fp);
    HTList_addObject(group_cache_list, (void *) group_cache);
    fclose(fp);

    CTRACE((tfp, "Read group file '%s', results follow:\n", filename));
    if (TRACE)
	print_group_def_list(group_cache->group_list);

    return group_cache->group_list;
}

/* PUBLIC					HTAA_userAndInetInGroup()
 *		CHECK IF USER BELONGS TO TO A GIVEN GROUP
 *		AND THAT THE CONNECTION COMES FROM AN
 *		ADDRESS THAT IS ALLOWED BY THAT GROUP
 * ON ENTRY:
 *	group		the group definition structure.
 *	username	connecting user.
 *	ip_number	browser host IP number, optional.
 *	ip_name		browser host IP name, optional.
 *			However, one of ip_number or ip_name
 *			must be given.
 * ON EXIT:
 *	returns		HTAA_IP_MASK, if IP address mask was
 *			reason for failing.
 *			HTAA_NOT_MEMBER, if user does not belong
 *			to the group.
 *			HTAA_OK if both IP address and user are ok.
 */
HTAAFailReasonType HTAA_userAndInetInGroup(GroupDef *group,
					   char *username,
					   char *ip_number,
					   char *ip_name)
{
    HTAAFailReasonType reason = HTAA_NOT_MEMBER;

    if (group && username) {
	ItemList *cur1 = group->item_list;
	Item *item;

	while (NULL != (item = (Item *) HTList_nextObject(cur1))) {
	    if (!item->address_def_list ||	/* Any address allowed */
		ip_in_def_list(item->address_def_list, ip_number, ip_name)) {

		if (!item->user_def_list)	/* Any user allowed */
		    return HTAA_OK;
		else {
		    UserDefList *cur2 = item->user_def_list;
		    Ref *ref;

		    while (NULL != (ref = (Ref *) HTList_nextObject(cur2))) {

			if (ref->translation) {		/* Group, check recursively */
			    reason = HTAA_userAndInetInGroup(ref->translation,
							     username,
							     ip_number, ip_name);
			    if (reason == HTAA_OK)
				return HTAA_OK;
			} else {	/* Username, check directly */
			    if (username && *username &&
				0 == strcmp(ref->name, username))
				return HTAA_OK;
			}
			/* Every user/group name in this group */
		    }
		    /* search for username */
		}
		/* IP address ok */
	    } else {
		reason = HTAA_IP_MASK;
	    }
	}			/* while items in group */
    }
    /* valid parameters */
    return reason;		/* No match, or invalid parameters */
}

void GroupDef_delete(GroupDef *group_def)
{
    if (group_def) {
	FREE(group_def->group_name);
	if (group_def->item_list) {
	    HTList_delete(group_def->item_list);	/* @@@@ */
	    group_def->item_list = NULL;
	}
	FREE(group_def);
    }
}
#endif
