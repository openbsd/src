#ifndef LYSTRUCTS_H
#define LYSTRUCTS_H

#ifndef HTANCHOR_H
#include <HTAnchor.h>
#endif /* HTANCHOR_H */

typedef struct link {
    char *lname;
    char *target;
    char *hightext;
    char *hightext2;
    int hightext2_offset;
    BOOL inUnderline;	/* TRUE when this link is in underlined context. */
    int lx;
    int ly;
    int type;		/* Type of link, Forms, WWW, etc. */
    int anchor_number;	/* The anchor number within the HText structure.  */
    int anchor_line_num;/* The anchor line number in the HText structure. */
    struct _FormInfo *form;	/* Pointer to form info. */
} linkstruct;
extern linkstruct links[MAXLINKS];
extern int nlinks;

typedef struct _document {
   char * title;
   char * address;
   char * post_data;
   char * post_content_type;
   char * bookmark;
   BOOL   safe;
   BOOL   isHEAD;
   int    link;
   int    line;
   BOOL   internal_link;	/* whether doc was reached via an internal
				 (fragment) link. - kw */
#ifdef USE_COLOR_STYLE
   char * style;
#endif
} document;

#ifndef HTFORMS_H
#include <HTForms.h>
#endif /* HTFORMS_H */

typedef struct _histstruct {
    char * title;
    char * address;
    char * post_data;
    char * post_content_type;
    char * bookmark;
    BOOL   safe;
    BOOL   isHEAD;
    int    link;
    int    line;
    BOOL   internal_link;	/* whether doc was reached via an internal
				 (fragment) link. - kw */
    int    intern_seq_start;	/* indicates which element on the history
				   is the start of this sequence of
				   "internal links", otherwise -1 */
} histstruct;

extern int Visited_Links_As;

#define VISITED_LINKS_AS_FIRST_V 0
#define VISITED_LINKS_AS_TREE    1
#define VISITED_LINKS_AS_LATEST  2
#define VISITED_LINKS_REVERSE    4

typedef struct _VisitedLink {
    char * title;
    char * address;
    int level;
    struct _VisitedLink *next_tree;
    struct _VisitedLink *prev_latest;
    struct _VisitedLink *next_latest;
    struct _VisitedLink *prev_first;
} VisitedLink;

extern histstruct history[MAXHIST];
extern int nhist;

/******************************************************************************/

typedef struct _lynx_list_item_type {
    struct _lynx_list_item_type *next;  /* the next item in the linked list */
    char *name; 			/* a description of the item */
    char *command;			/* the command to execute */
    int  always_enabled;		/* a constant to tell whether or
					* not to disable the printer
					* when the no_print option is on
					*/
    /* HTML lists: */
    BOOL override_primary_action;	/* whether primary action will be
					* overridden by this - e.g. this allows
					* invoking user's MUA when mailto: link
					* is activated using normal "activate"
					* command. This field is only examined
					* by code that handles EXTERNAL command.
					*/
    /* PRINTER lists: */
    int pagelen;			/* an integer to store the printer's
					* page length
					*/
} lynx_list_item_type;

extern lynx_list_item_type *printers;

/* for download commands */
extern lynx_list_item_type *downloaders;

/* for upload commands */
extern lynx_list_item_type *uploaders;

#ifdef USE_EXTERNALS
/* for external commands */
extern lynx_list_item_type *externals;
#endif

/******************************************************************************/

typedef struct
{
    CONST char *name;
    int value;
}
Config_Enum;

typedef int (*ParseFunc) PARAMS((char *));

#define ParseUnionMembers \
	lynx_list_item_type** add_value; \
	BOOLEAN * set_value; \
	int *     int_value; \
	char **   str_value; \
	ParseFunc fun_value; \
	long	  def_value

typedef union {
	ParseUnionMembers;
} ParseUnion;

#ifdef	PARSE_DEBUG
#define ParseUnionPtr Config_Type *
#define ParseUnionOf(tbl) tbl
#define ParseData ParseUnionMembers
#define UNION_ADD(v) &v,  0,  0,  0,  0,  0
#define UNION_SET(v)  0, &v,  0,  0,  0,  0
#define UNION_INT(v)  0,  0, &v,  0,  0,  0
#define UNION_STR(v)  0,  0,  0, &v,  0,  0
#define UNION_ENV(v)  0,  0,  0,  v,  0,  0
#define UNION_FUN(v)  0,  0,  0,  0,  v,  0
#define UNION_DEF(v)  0,  0,  0,  0,  0,  v
#else
#define ParseUnionPtr ParseUnion *
#define ParseUnionOf(tbl) (ParseUnionPtr)(&(tbl->value))
#define ParseData long value
#define UNION_ADD(v) (long)&(v)
#define UNION_SET(v) (long)&(v)
#define UNION_INT(v) (long)&(v)
#define UNION_STR(v) (long)&(v)
#define UNION_ENV(v) (long) (v)
#define UNION_FUN(v) (long) (v)
#define UNION_DEF(v) (long) (v)
#endif

#endif /* LYSTRUCTS_H */
