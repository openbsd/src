#include "HTUtils.h"
#include "tcp.h"
#include "LYUtils.h"
#include "LYKeymap.h"
#include "LYGlobalDefs.h"
#include "HTAccess.h"
#include "HTFormat.h"
#include "HTAlert.h"

#include "LYLeaks.h"

PRIVATE CONST DocAddress keymap_anchor = {"LYNXKEYMAP", NULL, NULL};

struct _HTStream 
{
  HTStreamClass * isa;
};

/* the character gets 1 added to it before lookup,
 * so that EOF maps to 0
 */
char keymap[] = {

0,
/* EOF */

0,                  LYK_HOME,       LYK_PREV_PAGE,     0,
/* nul */           /* ^A */        /* ^B */       /* ^C */

LYK_ABORT,          LYK_END,        LYK_NEXT_PAGE,     0,
/* ^D */            /* ^E */        /* ^F */       /* ^G */

LYK_HISTORY,      LYK_NEXT_LINK,    LYK_ACTIVATE,  LYK_COOKIE_JAR,
/* bs */            /* ht */        /* nl */       /* ^K */

LYK_REFRESH,      LYK_ACTIVATE,     LYK_DOWN_TWO,      0,
/* ^L */            /* cr */        /* ^N */       /* ^O */

LYK_UP_TWO,             0,          LYK_RELOAD,        0,
/* ^P */            /* XON */       /* ^R */       /* XOFF */

#ifdef NOT_USED
LYK_TRACE_TOGGLE,       0,          LYK_VERSION,   LYK_REFRESH,
/* ^T */            /* ^U */        /* ^V */       /* ^W */
#endif /* NOT_USED */
LYK_TRACE_TOGGLE,       0,        LYK_SWITCH_DTD,  LYK_REFRESH,
/* ^T */            /* ^U */        /* ^V */       /* ^W */

0,                      0,              0,             0,
/* ^X */            /* ^Y */        /* ^Z */       /* ESC */

0,                      0,              0,             0,
/* ^\ */            /* ^] */        /* ^^ */       /* ^_ */

LYK_NEXT_PAGE,       LYK_SHELL,  LYK_SOFT_DQUOTES,  LYK_TOOLBAR,
/* sp */             /* ! */         /* " */        /* # */

0,                      0,              0,          LYK_HISTORICAL,
/* $ */              /* % */         /* & */        /* ' */

LYK_UP_HALF,      LYK_DOWN_HALF, LYK_IMAGE_TOGGLE,  LYK_NEXT_PAGE,
/* ( */              /* ) */         /* * */        /* + */

#ifndef USE_EXTERNALS
LYK_NEXT_PAGE,    LYK_PREV_PAGE,        0,          LYK_WHEREIS,
/* , */              /* - */         /* . */        /* / */
#else
LYK_NEXT_PAGE,    LYK_PREV_PAGE, LYK_EXTERN,        LYK_WHEREIS,
/* , */              /* - */         /* . */        /* / */
#endif

LYK_F_LINK_NUM,      LYK_1,          LYK_2,         LYK_3,
/* 0 */              /* 1 */         /* 2 */        /* 3 */

LYK_4,               LYK_5,          LYK_6,         LYK_7,
/* 4 */              /* 5 */         /* 6 */        /* 7 */

LYK_8,               LYK_9,             0,          LYK_TRACE_LOG,
/* 8 */              /* 9 */         /* : */        /* ; */

LYK_UP_LINK,         LYK_INFO,     LYK_DOWN_LINK,   LYK_HELP,
/* < */              /* = */         /* > */        /* ? */

LYK_RAW_TOGGLE,  LYK_ADD_BOOKMARK, LYK_PREV_PAGE,   LYK_COMMENT,
/* @ */              /* A */         /* B */        /* C */

LYK_DOWNLOAD,        LYK_ELGOTO,             
/* D */              /* E */         

#if defined(DIRED_SUPPORT) || defined(VMS)
LYK_DIRED_MENU,
#else
0,          
#endif /* DIRED_SUPPORT || VMS */
/* F */        

LYK_ECGOTO,
/* G */

LYK_HELP,            LYK_INDEX,      LYK_JUMP,      LYK_KEYMAP,
/* H */              /* I */         /* J */        /* K */

LYK_LIST,          LYK_MAIN_MENU,    LYK_NEXT,      LYK_OPTIONS,
/* L */              /* M */         /* N */        /* O */

LYK_PRINT,          LYK_ABORT,    LYK_DEL_BOOKMARK, LYK_INDEX_SEARCH,
/* P */              /* Q */         /* R */        /* S */

#ifdef DIRED_SUPPORT
LYK_TAG_LINK,     
#else
0,
#endif /* DIRED_SUPPORT */
/* T */

 	          LYK_PREV_DOC,    LYK_VLINKS,         0,
                     /* U */         /* V */        /* W */

#ifdef NOT_USED
LYK_FORM_UP,            0,        LYK_FORM_DOWN,    LYK_INLINE_TOGGLE,
/* X */              /* Y */         /* Z */        /* [ */
#endif /* NOT_USED */
LYK_NOCACHE,            0,        LYK_INTERRUPT,    LYK_INLINE_TOGGLE,
/* X */              /* Y */         /* Z */        /* [ */

LYK_SOURCE,          LYK_HEAD,          0,          LYK_CLEAR_AUTH,
/* \ */              /* ] */         /* ^ */        /* _ */

LYK_MINIMAL,   LYK_ADD_BOOKMARK,  LYK_PREV_PAGE,    LYK_COMMENT,
/* ` */              /* a */         /* b */        /* c */

LYK_DOWNLOAD,        LYK_EDIT,             
/* d */              /* e */         

#if defined(DIRED_SUPPORT) || defined(VMS)
LYK_DIRED_MENU,
#else
0,          
#endif /* DIRED_SUPPORT || VMS */
/* f */        

LYK_GOTO,
/* g */

LYK_HELP,            LYK_INDEX,      LYK_JUMP,      LYK_KEYMAP,
/* h */              /* i */         /* j */        /* k */

LYK_LIST,         LYK_MAIN_MENU,     LYK_NEXT,      LYK_OPTIONS,
/* l */              /* m */         /* n */        /* o */

LYK_PRINT,           LYK_QUIT,    LYK_DEL_BOOKMARK, LYK_INDEX_SEARCH,
/* p */              /* q */         /* r */        /* s */

#ifdef DIRED_SUPPORT
LYK_TAG_LINK,     
#else
0,
#endif /* DIRED_SUPPORT */
/* t */

                    LYK_PREV_DOC,   LYK_VIEW_BOOKMARK,   0,
                     /* u */         /* v */         /* w */

#ifdef NOT_USED
LYK_FORM_UP,            0,          LYK_FORM_DOWN,     0,
/* x */              /* y */          /* z */       /* { */
#endif /* NOT_USED */
LYK_NOCACHE,            0,          LYK_INTERRUPT,     0,
/* x */              /* y */          /* z */       /* { */

LYK_PIPE,               0,              0,          LYK_HISTORY,
/* | */               /* } */         /* ~ */       /* del */

/* 80..9F (illegal ISO-8859-1) 8-bit characters. */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,

/* A0..FF (permissible ISO-8859-1) 8-bit characters. */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,

/* 100..10E function key definitions in LYStrings.h */
LYK_PREV_LINK,    LYK_NEXT_LINK,    LYK_ACTIVATE,   LYK_PREV_DOC,
/* UPARROW */     /* DNARROW */     /* RTARROW */   /* LTARROW */

LYK_NEXT_PAGE,    LYK_PREV_PAGE,    LYK_HOME,       LYK_END,
/* PGDOWN */      /* PGUP */        /* HOME */      /* END */

LYK_HELP,         LYK_ACTIVATE,     LYK_HOME,       LYK_END,
/* F1*/ 	  /* Do key */      /* Find key */  /* Select key */

LYK_UP_TWO,       LYK_DOWN_TWO,
/* Insert key */  /* Remove key */

LYK_DO_NOTHING,
/* DO_NOTHING*/
};

#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
/*
 * This table is used to override the standard keyboard assignments
 * when lynx_edit_mode is in effect and keyboard overrides have been
 * allowed at compile time.
 */

char override[] = {

    0,
/* EOF */

    0,                  0,              0,            0,
/* nul */           /* ^A */        /* ^B */      /* ^C */

    0,                  0,              0,            0,
/* ^D */            /* ^E */        /* ^F */      /* ^G */

    0,                  0,              0,            0,
/* bs */            /* ht */        /* nl */      /* ^K */

    0,                  0,              0,            0,
/* ^L */            /* cr */        /* ^N */      /* ^O */

    0,                  0,              0,            0,
/* ^P */            /* XON */       /* ^R */      /* XOFF */

    0,            LYK_PREV_DOC,         0,            0,
/* ^T */            /* ^U */        /* ^V */      /* ^W */

    0,                  0,              0,            0,
/* ^X */            /* ^Y */        /* ^Z */      /* ESC */

    0,                  0,              0,            0,
/* ^\ */            /* ^] */        /* ^^ */      /* ^_ */

    0,                 0,              0,            0,
/* sp */            /* ! */         /* " */       /* # */

   0,                  0,              0,            0,
/* $ */             /* % */         /* & */       /* ' */

    0,                 0,              0,            0,
/* ( */             /* ) */         /* * */       /* + */

    0,                 0,         LYK_TAG_LINK,      0,
/* , */             /* - */         /* . */       /* / */

   0,                  0,              0,            0,
/* 0 */             /* 1 */         /* 2 */       /* 3 */

   0,                  0,              0,            0,
/* 4 */             /* 5 */         /* 6 */       /* 7 */

   0,                  0,              0,             0,
/* 8 */             /* 9 */         /* : */        /* ; */

   0,                  0,              0,             0,
/* < */             /* = */         /* > */        /* ? */

   0,                  0,              0,         LYK_CREATE,
/* @ */             /* A */         /* B */        /* C */

   0,                  0,        LYK_DIRED_MENU,       0,
/* D */             /* E */         /* F */        /* G */

   0,                  0,              0,             0,
/* H */             /* I */         /* J */        /* K */

   0,             LYK_MODIFY,          0,             0,
/* L */             /* M */         /* N */        /* O */

   0,                  0,         LYK_REMOVE,         0,
/* P */             /* Q */         /* R */        /* S */

LYK_TAG_LINK,     LYK_UPLOAD,          0,             0,
/* T */             /* U */         /* V */        /* W */

   0,                  0,              0,             0,
/* X */             /* Y */         /* Z */        /* [ */

   0,                  0,              0,             0,
/* \ */             /* ] */         /* ^ */        /* _ */

0,                     0,              0,         LYK_CREATE,
/* ` */             /* a */         /* b */        /* c */

   0,                  0,       LYK_DIRED_MENU,       0,
/* d */             /* e */         /* f */        /* g */

   0,                  0,              0,             0,
/* h */             /* i */         /* j */        /* k */

0,                LYK_MODIFY,          0,             0,
/* l */             /* m */         /* n */        /* o */

   0,                  0,          LYK_REMOVE,        0,
/* p */             /* q */         /* r */        /* s */

LYK_TAG_LINK,      LYK_UPLOAD,         0,             0,
/* t */             /* u */         /* v */         /* w */

   0,                  0,               0,            0,
/* x */             /* y */          /* z */       /* { */

   0,                   0,             0,              0,
/* | */              /* } */         /* ~ */       /* del */

/* 80..9F (illegal ISO-8859-1) 8-bit characters. */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,

/* A0..FF (permissible ISO-8859-1) 8-bit characters. */
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,
   0,                  0,              0,             0,

/* 100..10E function key definitions in LYStrings.h */
   0,                   0,             0,              0,
/* UPARROW */     /* DNARROW */     /* RTARROW */   /* LTARROW */

   0,                  0,              0,              0,
/* PGDOWN */      /* PGUP */        /* HOME */      /* END */

   0,                  0,              0,              0,
/* F1*/ 	  /* Do key */      /* Find key */  /* Select key */

   0,                  0,
/* Insert key */  /* Remove key */

LYK_DO_NOTHING,
/* DO_NOTHING*/
};
#endif /* DIRED_SUPPORT && OK_OVERRIDE */

struct rmap {
	char *name;
	char *doc;
};
PRIVATE struct rmap revmap[] = {
{ "UNMAPPED",		NULL },
{ "1",			NULL },
{ "2",			NULL },
{ "3",			NULL },
{ "4",			NULL },
{ "5",			NULL },
{ "6",			NULL },
{ "7",			NULL },
{ "8",			NULL },
{ "9",			NULL },
{ "SOURCE",		"toggle source/presentation for current document" },
{ "RELOAD",		"reload the current document" },
{ "PIPE",		"pipe the current document to an external command" },
{ "QUIT",		"quit the browser" },
{ "ABORT",		"quit the browser unconditionally" },
{ "NEXT_PAGE",		"view the next page of the document" },
{ "PREV_PAGE",		"view the previous page of the document" },
{ "UP_TWO",		"go back two lines in the document" },
{ "DOWN_TWO",		"go forward two lines in the document" },
{ "UP_HALF",		"go back half a page in the document" },
{ "DOWN_HALF",		"go forward half a page in the document" },
{ "REFRESH",		"refresh the screen to clear garbled text" },
{ "HOME",		"go to the beginning of the current document" },
{ "END",		"go to the end of the current document" },
{ "PREV_LINK",		"make the previous link current" },
{ "NEXT_LINK",		"make the next link current" },
{ "UP_LINK",		"move up the page to a previous link" },
{ "DOWN_LINK",		"move down the page to another link" },
{ "RIGHT_LINK",		"move right to another link" },
{ "LEFT_LINK",		"move left to a previous link" },
{ "HISTORY",		"display stack of currently-suspended documents" },
{ "PREV_DOC",		"go back to the previous document" },
{ "ACTIVATE",		"go to the document given by the current link" },
{ "GOTO",		"go to a document given as a URL" },
{ "ECGOTO",		"edit the current document's URL and go to it" },
{ "HELP",		"display help on using the browser" },
{ "INDEX",		"display an index of potentially useful documents" },
{ "NOCACHE",		"force submission of form or link with no-cache" },
{ "INTERRUPT",		"interrupt network transmission" },
{ "MAIN_MENU",		"return to the first screen (home page)" },
{ "OPTIONS",		"display and change option settings" },
{ "INDEX_SEARCH",	"allow searching of an index" },
{ "WHEREIS",		"search within the current document" },
{ "NEXT",		"search for the next occurence" },
{ "COMMENT",		"send a comment to the author of the current document" },
{ "EDIT",		"edit the current document" },
{ "INFO",		"display information on the current document and link" },
{ "PRINT",		"display choices for printing the current document" },
{ "ADD_BOOKMARK",	"add to your personal bookmark list" },
{ "DEL_BOOKMARK",	"delete from your personal bookmark list" },
{ "VIEW_BOOKMARK",	"view your personal bookmark list" },
{ "VLINKS",		"list links visited during the current Lynx session" },
{ "SHELL",		"escape from the browser to the system" },
{ "DOWNLOAD",		"download the current link to your computer" },
{ "TRACE_TOGGLE",	"toggle tracing of browser operations" },
{ "TRACE_LOG",		"view trace log if started in the current session" },
{ "IMAGE_TOGGLE",	"toggle handling of all images as links" },
{ "INLINE_TOGGLE",	"toggle pseudo-ALTs for inlines with no ALT string" },
{ "HEAD",		"send a HEAD request for the current document or link" },
{ "DO_NOTHING",		NULL },
{ "TOGGLE_HELP",	"show other commands in the novice help menu" },
{ "JUMP",		"go directly to a target document or action" },
{ "KEYMAP",		"display the current key map" },
{ "LIST",		"list the references (links) in the current document" },
{ "TOOLBAR",		"go to Toolbar or Banner in the current document" },
{ "HISTORICAL",		"toggle historical vs. valid/minimal comment parsing" },
{ "MINIMAL",		"toggle minimal vs. valid comment parsing" },
{ "SOFT_DQUOTES",	"toggle valid vs. soft double-quote parsing" },
{ "RAW_TOGGLE",		"toggle raw 8-bit translations or CJK mode ON or OFF" },
{ "COOKIE_JAR",		"examine the Cookie Jar" },
{ "F_LINK_NUM",		"invoke the 'Follow link (or page) number:' prompt" },
{ "CLEAR_AUTH",		"clear all authorization info for this session" },
{ "SWITCH_DTD",		"switch between two ways of parsing HTML" },
{ "ELGOTO",		"edit the current link's URL or ACTION and go to it" },
#ifdef USE_EXTERNALS
{ "EXTERN",		"run external program with url" },
#endif
#ifdef VMS
{ "DIRED_MENU",		"invoke File/Directory Manager, if available" },
#else
#ifdef DIRED_SUPPORT
{ "DIRED_MENU",		"display a full menu of file operations" },
{ "CREATE",		"create a new file or directory" },
{ "REMOVE",		"remove a file or directory" },
{ "MODIFY",		"modify the name or location of a file or directory" },
{ "TAG_LINK",		"tag a file or directory for later action" },
{ "UPLOAD",		"upload from your computer to the current directory" },
{ "INSTALL",		"install file or tagged files into a system area" },
#endif /* DIRED_SUPPORT */
#endif /* VMS */
#ifdef NOT_USED
{ "VERSION",		"report version of lynx"},
{ "FORM_UP",		"toggle a checkbox" },
{ "FORM_DOWN",		"toggle a checkbox" },
#endif /* NOT_USED */
{ NULL,			"" }
};

PRIVATE char *funckey[] = {
  "Up Arrow",
  "Down Arrow",
  "Right Arrow",
  "Left Arrow",
  "Page Down",
  "Page Up",
  "Home",
  "End",
  "F1",
  "Do key",
  "Find key",
  "Select key",
  "Insert key",
  "Remove key"
};

PRIVATE char *pretty ARGS1 (int, c)
{
	static char buf[30];

	if (c == '\t')
		sprintf(buf, "&lt;tab&gt;       ");
	else if (c == '\r')
		sprintf(buf, "&lt;return&gt;    ");
	else if (c == ' ')
		sprintf(buf, "&lt;space&gt;     ");
	else if (c == '<')
		sprintf(buf, "&lt;           ");
	else if (c == '>')
		sprintf(buf, "&gt;           ");
	else if (c == 0177)
		sprintf(buf, "&lt;delete&gt;    ");
	else if (c > ' ' && c <= 0377)
		sprintf(buf, "%c", c);
	else if (c < ' ')
		sprintf(buf, "^%c", c|0100);
	else
		sprintf(buf, "%s", funckey[c-0400]);
	
	return buf;
}

PRIVATE void print_binding ARGS3(HTStream *, target, char *, buf, int, i)
{
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
    if (prev_lynx_edit_mode && !no_dired_support &&
        override[i] && revmap[(unsigned char)override[i]].doc) {
	sprintf(buf, "%-12s%-14s%s\n", pretty(i-1),
		revmap[(unsigned char)override[i]].name,
		revmap[(unsigned char)override[i]].doc);
	(*target->isa->put_block)(target, buf, strlen(buf));
    } else
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
    if (keymap[i] && revmap[(unsigned char)keymap[i]].doc) {
	sprintf(buf, "%-12s%-14s%s\n", pretty(i-1),
		revmap[(unsigned char)keymap[i]].name,
		revmap[(unsigned char)keymap[i]].doc);
	(*target->isa->put_block)(target, buf, strlen(buf));
    }
}

PRIVATE int LYLoadKeymap ARGS4 (
	CONST char *, 		arg GCC_UNUSED,
	HTParentAnchor *,	anAnchor,
	HTFormat,		format_out,
	HTStream*,		sink)
{
    HTFormat format_in = WWW_HTML;
    HTStream *target;
    char buf[256];
    int i;

    /*
     *  Set up the stream. - FM
     */
    target = HTStreamStack(format_in, format_out, sink, anAnchor);
    if (!target || target == NULL) {
	sprintf(buf, CANNOT_CONVERT_I_TO_O,
		     HTAtom_name(format_in), HTAtom_name(format_out));
	HTAlert(buf);
	return(HT_NOT_LOADED);
    }
    anAnchor->no_cache = TRUE;

    sprintf(buf, "<head>\n<title>%s</title>\n</head>\n<body>\n",
    		  CURRENT_KEYMAP_TITLE);
    (*target->isa->put_block)(target, buf, strlen(buf));
	
    sprintf(buf, "<h1>%s (%s Version %s)</h1>\n<pre>",
		 CURRENT_KEYMAP_TITLE, LYNX_NAME, LYNX_VERSION);
    (*target->isa->put_block)(target, buf, strlen(buf));

    for (i = 'a'+1; i <= 'z'+1; i++) {
	print_binding(target, buf, i);
	if (keymap[i - ' '] != keymap[i]) {
	    print_binding(target, buf,
			  i-' ');  /* uppercase mapping is different */
	}
    }
    for (i = 1; i < (int) sizeof(keymap); i++) {
	/*
	 *  LYK_PIPE not implemented yet.
	 */
	if ((i > 127 || i <= ' ' || !isalpha(i-1)) &&
	    strcmp(revmap[(unsigned char)keymap[i]].name, "PIPE")) {
	    print_binding(target, buf, i);
	}
    }

    sprintf(buf,"</pre>\n</body>\n");
    (*target->isa->put_block)(target, buf, strlen(buf));

    (*target->isa->_free)(target);
    return(HT_LOADED);
}

#ifdef GLOBALDEF_IS_MACRO
#define _LYKEYMAP_C_GLOBALDEF_1_INIT { "LYNXKEYMAP", LYLoadKeymap, 0}
GLOBALDEF (HTProtocol,LYLynxKeymap,_LYKEYMAP_C_GLOBALDEF_1_INIT);
#else
GLOBALDEF PUBLIC HTProtocol LYLynxKeymap = {"LYNXKEYMAP", LYLoadKeymap, 0};
#endif /* GLOBALDEF_IS_MACRO */

/*
 * install func as the mapping for key.
 * func must be present in the revmap table.
 * returns TRUE if the mapping was made, FALSE if not.
 */
PUBLIC int remap ARGS2(char *,key, char *,func)
 {
       int i;
       struct rmap *mp;
       int c = 0;

       if (func == NULL)
	       return 0;
       if (strlen(key) == 1)
               c = *key;
       else if (strlen(key) == 2 && *key == '^')
               c = key[1] & 037;
       else if (strlen(key) >= 2 && isdigit(*key))
               if (sscanf(key, "%i", &c) != 1)
                       return 0;
       for (i = 0, mp = revmap; (*mp).name != NULL; mp++, i++) {
               if (strcmp((*mp).name, func) == 0) {
                       keymap[c+1] = i;
                       return c;
               }
       }
       return 0;
}


PUBLIC void set_vms_keys NOARGS
{
      keymap[26+1] = LYK_ABORT;  /* control-Z */
      keymap['$'+1] = LYK_SHELL;  
} 

static char saved_vi_keys[4];
static BOOLEAN did_vi_keys;

PUBLIC void set_vi_keys NOARGS
{
      saved_vi_keys[0] = keymap['h'+1];
      keymap['h'+1] = LYK_PREV_DOC;
      saved_vi_keys[1] = keymap['j'+1];
      keymap['j'+1] = LYK_NEXT_LINK;
      saved_vi_keys[2] = keymap['k'+1];
      keymap['k'+1] = LYK_PREV_LINK;
      saved_vi_keys[3] = keymap['l'+1];
      keymap['l'+1] = LYK_ACTIVATE;

      did_vi_keys = TRUE;
}

PUBLIC void reset_vi_keys NOARGS
{
      if (!did_vi_keys)
              return;

      keymap['h'+1] = saved_vi_keys[0];
      keymap['j'+1] = saved_vi_keys[1];
      keymap['k'+1] = saved_vi_keys[2];
      keymap['l'+1] = saved_vi_keys[3];

      did_vi_keys = FALSE;
}

static char saved_emacs_keys[4];
static BOOLEAN did_emacs_keys;

PUBLIC void set_emacs_keys NOARGS
{
      saved_emacs_keys[0] = keymap[2+1];
      keymap[2+1] = LYK_PREV_DOC;       /* ^B */
      saved_emacs_keys[1] = keymap[14+1];
      keymap[14+1] = LYK_NEXT_LINK;     /* ^N */
      saved_emacs_keys[2] = keymap[16+1];
      keymap[16+1] = LYK_PREV_LINK;     /* ^P */
      saved_emacs_keys[3] = keymap[6+1];
      keymap[6+1] = LYK_ACTIVATE;         /* ^F */

      did_emacs_keys = TRUE;
}

PUBLIC void reset_emacs_keys NOARGS
{
      if (!did_emacs_keys)
              return;

      keymap[2+1] = saved_emacs_keys[0];
      keymap[14+1] = saved_emacs_keys[1];
      keymap[16+1] = saved_emacs_keys[2];
      keymap[6+1] = saved_emacs_keys[3];

      did_emacs_keys = FALSE;
}

static char saved_number_keys[9];
static BOOLEAN did_number_keys;

PUBLIC void set_numbers_as_arrows NOARGS
{
    /*
     *  Map numbers to functions as labeled on the
     *  IBM Enhanced keypad, and save their original
     *  mapping for reset_numbers_as_arrows(). - FM
     */
    saved_number_keys[0] = keymap['4'+1];
    keymap['4'+1] = LYK_PREV_DOC;
    saved_number_keys[1] = keymap['2'+1];
    keymap['2'+1] = LYK_NEXT_LINK;
    saved_number_keys[2] = keymap['8'+1];
    keymap['8'+1] = LYK_PREV_LINK;
    saved_number_keys[3] = keymap['6'+1];
    keymap['6'+1] = LYK_ACTIVATE;
    saved_number_keys[4] = keymap['7'+1];
    keymap['7'+1] = LYK_HOME;
    saved_number_keys[5] = keymap['1'+1];
    keymap['1'+1] = LYK_END;
    saved_number_keys[6] = keymap['9'+1];
    keymap['9'+1] = LYK_PREV_PAGE;
    saved_number_keys[7] = keymap['3'+1];
    keymap['3'+1] = LYK_NEXT_PAGE;

    /*
     *  Disable the 5.
     */
    saved_number_keys[8] = keymap['5'+1];
    keymap['5'+1] = LYK_DO_NOTHING;

    did_number_keys = TRUE;
}

PUBLIC void reset_numbers_as_arrows NOARGS
{
    if (!did_number_keys)
	return;

    keymap['4'+1] = saved_number_keys[0];
    keymap['2'+1] = saved_number_keys[1];
    keymap['8'+1] = saved_number_keys[2];
    keymap['6'+1] = saved_number_keys[3];
    keymap['7'+1] = saved_number_keys[4];
    keymap['1'+1] = saved_number_keys[5];
    keymap['9'+1] = saved_number_keys[6];
    keymap['3'+1] = saved_number_keys[7];
    keymap['5'+1] = saved_number_keys[8];

    did_number_keys = FALSE;
}

PUBLIC char *key_for_func ARGS1 (
	int,		func)
{
	static char buf[512];
	size_t i;

	buf[0] = '\0';
	for (i = 1; i < sizeof(keymap); i++) {
		if (keymap[i] == func) {
			if (*buf)
				strcat(buf, " or ");
			strcat(buf, pretty(i-1));
		}
	}
	return buf;
}

/*
 *  This function returns TRUE if the ch is non-alphanumeric
 *  and maps to key_name (LYK_foo in the keymap[] array). - FM
 */ 
PUBLIC BOOL LYisNonAlnumKeyname ARGS2(
	int,	ch,
	int,	key_name)
{
    if ((ch >= '0' && ch <= '9') ||
        (ch >= 'A' && ch <= 'z') ||
	ch < 0 || ch > 269)
	return (FALSE);

    return(keymap[ch+1] == key_name);
}

#ifdef NOTUSED_FOTEMODS
/*
 *  This function returns the (int)ch mapped to the
 *  LYK_foo value passed to it as an argument. - FM
 */
PUBLIC int LYReverseKeymap ARGS1(
	int,		key_name)
{
    int i;

    for (i = 1; i < sizeof(keymap); i++) {
	if (keymap[i] == key_name) {
	    return(i - 1);
	}
    }

    return(0);
}
#endif
