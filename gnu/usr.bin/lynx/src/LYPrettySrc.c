/* HTML source syntax highlighting
   by Vlad Harchev <hvv@hippo.ru>
   March 1999
*/
#include <HTUtils.h>
#include <LYHash.h>
#include <LYPrettySrc.h>

#ifdef USE_PSRC
BOOL psrc_convert_string = FALSE;
PUBLIC BOOL psrc_view = FALSE;/* this is read by SGML_put_character - TRUE
	when viewing pretty source */
PUBLIC BOOL LYpsrc = FALSE; /* this tells what will be shown on '\':
  if TRUE, then pretty source, normal source view otherwise. Toggled by
  -prettysrc commandline option.  */
PUBLIC BOOL sgml_in_psrc_was_initialized;
PUBLIC BOOL psrc_nested_call;
PUBLIC BOOL psrc_first_tag;

  /* tagspecs from lynx.cfg are read here. After .lss file is read (is with lss
     support), the style cache and markup are created before entering the
     mainloop. */
PUBLIC char* HTL_tagspecs[HTL_num_lexems] = {
 /* these values are defaults. They are also listed in comments of distibution's
     lynx.cfg.*/
#ifdef USE_COLOR_STYLE
    "span.htmlsrc_comment:!span",
    "span.htmlsrc_tag:!span",
    "span.htmlsrc_attrib:!span",
    "span.htmlsrc_attrval:!span",
    "span.htmlsrc_abracket:!span",
    "span.htmlsrc_entity:!span",
    "span.htmlsrc_href:!span",
    "span.htmlsrc_entire:!span",
    "span.htmlsrc_badseq:!span",
    "span.htmlsrc_badtag:!span",
    "span.htmlsrc_badattr:!span",
    "span.htmlsrc_sgmlspecial:!span"
#else
    "b:!b",
    "b:!b",
    ":",
    "!b:b",
    "b:!b",
    "b:!b",
    ":",
    ":",
    "b:!b",
    ":",
    "!b:b",
    "b:!b"
#endif
};


 /* these are pointers since tagspec can be empty (the pointer will be NULL
    in that case) */
PUBLIC HT_tagspec* lexem_start[HTL_num_lexems];
PUBLIC HT_tagspec* lexem_end[HTL_num_lexems];

PUBLIC int tagname_transform = 2;
PUBLIC int attrname_transform = 2;


PRIVATE int html_src_tag_index ARGS1(
	    char*, tagname)
{
    HTTag* tag = SGMLFindTag(&HTML_dtd, tagname);
    return (tag && tag != &HTTag_unrecognized ) ? tag - HTML_dtd.tags : -1;
}

typedef enum _html_src_check_state
{
    HTSRC_CK_normal,
    HTSRC_CK_seen_excl,
    HTSRC_CK_after_tagname,
    HTSRC_CK_seen_dot
} html_src_check_state;

PRIVATE void append_close_tag ARGS3(
	    char*,	  tagname,
	    HT_tagspec**, head,
	    HT_tagspec**,  tail)
{
    int idx, nattr;
    HTTag* tag;
    HT_tagspec* subj;

    idx = html_src_tag_index(tagname);
    tag = HTML_dtd.tags+idx;
    nattr = tag->number_of_attributes;

    if (idx == -1) {
	fprintf(stderr,
	"internal error: previous check didn't find bad HTML tag %s", tagname);
	exit_immediately(-1);
    }

    subj = (HT_tagspec*) calloc( sizeof(*subj), 1);
    subj->element = idx;
    subj->present = (BOOL*)calloc( nattr*sizeof (BOOL), 1);
    subj->value = (char**)calloc( nattr*sizeof (char*), 1);
    subj->start = FALSE;

    if (!*head) {
	*head = subj; *tail = subj;
    } else {
	(*tail)->next = subj; *tail = subj;
    }
}

/* this will allocate node, initialize all members, and node
   append to the list, possibly modifying head and modifying tail */
PRIVATE void append_open_tag ARGS4(
	    char*,	  tagname,
	    char*,	  classname GCC_UNUSED,
	    HT_tagspec**, head,
	    HT_tagspec**,  tail)
{
    HT_tagspec* subj;
    HTTag* tag;
#ifdef USE_COLOR_STYLE
    int hcode;
#endif

    append_close_tag(tagname, head, tail); /* initialize common members*/
    subj = *tail;
    subj->start = TRUE;

    tag = HTML_dtd.tags+subj->element;

#ifdef USE_COLOR_STYLE
    hcode = hash_code_lowercase_on_fly(tagname);
    if (classname && *classname) {

#  if 0
	/*
	 * we don't provide a classname as attribute of that tag, since for plain
	 * formatting tags they are not used directly for anything except style -
	 * and we provide style value directly.
	 */
	int class_attr_idx = 0;
	int n = tag->number_of_attributes;
	attr* attrs = tag->attributes;
	 /*.... */ /* this is not implemented though it's easy */
#  endif

	hcode = hash_code_aggregate_char('.', hcode);
	hcode = hash_code_aggregate_lower_str(classname, hcode);
	StrAllocCopy(subj->class_name, classname);
    } else {
	subj->class_name = "";
    }
    subj->style = hcode;
#endif
}


/* returns 1 if incorrect */
PUBLIC int html_src_parse_tagspec ARGS4(
	char*,		ts,
	HTlexem,	lexem,
	BOOL,		checkonly,
	BOOL,		isstart)
{
    char *p = ts;
    char *tagstart = 0;
    char *tagend = 0;
    char *classstart;
    char *classend;
    char stop = FALSE, after_excl = FALSE;
    html_src_check_state state = HTSRC_CK_normal;
    HT_tagspec* head = NULL, *tail = NULL;
    HT_tagspec** slot = ( isstart ? lexem_start : lexem_end ) +lexem;

    while (!stop) {
	switch (state) {
	    case HTSRC_CK_normal:
	    case HTSRC_CK_seen_excl:
		switch (*p) {
		    case '\0': stop = TRUE; break;
		    case ' ': case '\t': break;
		    case '!':
			if (state == HTSRC_CK_seen_excl)
			    return 1;	/*second '!'*/
			state = HTSRC_CK_seen_excl;
			after_excl = TRUE;
			break;
		    default:
			if (isalpha(*p) || *p == '_') {
			    tagstart = p;
			    while (*p && ( isalpha(*p) || *p == '_') )
				 ++p;
			    tagend = p;
			    state = HTSRC_CK_after_tagname;
			} else
			    return 1;
			continue;
		    }
		break;
	    case HTSRC_CK_after_tagname:
		switch (*p) {
		    case '\0': stop = TRUE;
		    case ' ':
		    case '\t':
			{
			    char save = *tagend;
			    *tagend = '\0';
			    classstart = 0;
			    if (checkonly) {
				int idx = html_src_tag_index(tagstart);
				*tagend = save;
				if (idx == -1)
				    return 1;
			    } else {
				if (after_excl)
				    append_close_tag(tagstart, &head, &tail);
				else
				    append_open_tag(tagstart, NULL, &head, &tail);
			    }
			    state = HTSRC_CK_normal;
			    after_excl = FALSE;
			}
			break;
		    case '.':
			if (after_excl)
			    return 1;
			state = HTSRC_CK_seen_dot;
			break;
		    default:
			return 1;
		}
		break;
	    case HTSRC_CK_seen_dot: {
		switch (*p) {
		    case ' ':
		    case '\t':
			break;
		    case '\0':
			return 1;
		    default: {
			char save, save1;
			if ( isalpha(*p) || *p == '_' ) {
			    classstart = p;
			    while (*p && ( isalpha(*p) || *p == '_') ) ++p;
			    classend = p;
			    save = *classend;
			    *classend = '\0';
			    save1 = *tagend;
			    *tagend = '\0';
			    if (checkonly) {
				int idx = html_src_tag_index(tagstart);
				*tagend = save1; *classend = save;
				if (idx == -1)
				return 1;
			    } else {
				append_open_tag(tagstart, classstart, &head, &tail);
			    }
			    state = HTSRC_CK_normal;after_excl = FALSE;
			    continue;
			} else
			    return 1;
		    }
		}/*of switch(*p)*/
		break;
	    } /* of case HTSRC_CK_seen_dot: */
	}/* of switch */
	++p;
    }

    if (!checkonly)
	*slot = head;
    return 0;
}

/*it shouldn't fail anyway - since everything is checked before it's called. */
PUBLIC void HTMLSRC_init_caches NOARGS
{
    int i;
    char* p;
    char buf[1000];

    for (i = 0; i < HTL_num_lexems; ++i) {
	if (HTL_tagspecs[i])
	    strcpy(buf, HTL_tagspecs[i]);
	else
	    buf[0] = '\0';
	if ((p = strchr(buf, ':')) != 0)
	    *p = '\0';
	if (html_src_parse_tagspec(buf, i, FALSE, TRUE) ) {
	    fprintf(stderr, "internal error while caching 1st tagspec of %d lexem", i);
	    exit_immediately(-1);
	}
	if (html_src_parse_tagspec( p ? p+1 : NULL , i, FALSE, FALSE) )	 {
	    fprintf(stderr, "internal error while caching 2nd tagspec of %d lexem", i);
	    exit_immediately(-1);
	}
    }
}

#endif /* ifdef USE_PSRC */
