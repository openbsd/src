/*
 * $LynxId: LYEditmap.c,v 1.73 2014/02/08 01:12:58 Gisle.Vanem Exp $
 *
 * LYEditMap.c
 * Keybindings for line and form editing.
 */

#include <HTUtils.h>
#include <HTAlert.h>
#include <HTFile.h>
#include <LYGlobalDefs.h>
#include <LYCharUtils.h>
#include <LYStrings.h>
#include <LYKeymap.h>		/* KEYMAP_SIZE, LKC_*, LYK_* - kw */

#define PUTS(buf)    (*target->isa->put_string)(target, buf)

/* * * * * LynxEditactionCodes * * * * */
#ifdef USE_ALT_BINDINGS

/* Last valid index for the (lynxkeycode+modifier -> lynxeditactioncode)
 * tables.  Currently all three tables are the same.  - kw
 */
#define LAST_MOD1_LKC	0x111
#define LAST_MOD2_LKC	0x111
#define LAST_MOD3_LKC	0x111

/*  Get (lynxkeycode+modifier -> lynxeditactioncode) mapping, intermediate.
 */
#define LKC_TO_LEC_M1(c) ((c)>LAST_MOD1_LKC? (int)LYE_UNMOD: Mod1Binding[c])
#define LKC_TO_LEC_M2(c) ((c)>LAST_MOD2_LKC? (int)LYE_UNMOD: Mod2Binding[c])
#define LKC_TO_LEC_M3(c) ((c)>LAST_MOD3_LKC? (int)LYE_UNMOD: Mod3Binding[c])

#endif /* USE_ALT_BINDINGS */

int current_lineedit = 0;	/* Index into LYLineEditors[]   */

int escape_bound = 0;		/* User wanted Escape to perform actions?  */

/*
 * See LYStrings.h for the LYE definitions.
 */
/* *INDENT-OFF* */
struct emap {
    const char *name;
    const int   code;
    const char *descr;
};

#define SEPARATOR {"", -1, ""}

static struct emap ekmap[] = {
  {"NOP",	LYE_NOP,	"Do Nothing"},
  {"CHAR",	LYE_CHAR,	"Insert printable char"},
  SEPARATOR,
  {"ENTER",	LYE_ENTER,	"Input complete, return char"},
  {"TAB",	LYE_TAB,	"Input complete, return TAB"},
  {"STOP",	LYE_STOP,	"Input deactivated"},
  {"ABORT",	LYE_ABORT,	"Input cancelled"},
  SEPARATOR,
  {"PASS",	LYE_FORM_PASS,  "Fields only: input complete"},
  SEPARATOR,
  {"DELBL",	LYE_DELBL,	"Delete back to BOL"},
  {"DELEL",	LYE_DELEL,	"Delete thru EOL"},
  {"DELN",	LYE_DELN,	"Delete next/curr char"},
  {"DELP",	LYE_DELP,	"Delete prev      char"},
  {"DELNW",	LYE_DELNW,	"Delete next word"},
  {"DELPW",	LYE_DELPW,	"Delete prev word"},
  SEPARATOR,

  {"ERASE",	LYE_ERASE,	"Erase the line"},
  SEPARATOR,
  {"BOL",	LYE_BOL,	"Go to begin of line"},
  {"EOL",	LYE_EOL,	"Go to end   of line"},
  {"FORW",	LYE_FORW,	"Cursor forwards"},
  {"FORW_RL",	LYE_FORW_RL,	"Cursor forwards or right link"},
  {"BACK",	LYE_BACK,	"Cursor backwards"},
  {"BACK_LL",	LYE_BACK_LL,	"Cursor backwards or left link"},
  {"FORWW",	LYE_FORWW,	"Word forward"},
  {"BACKW",	LYE_BACKW,	"Word back"},
  SEPARATOR,
  {"LOWER",	LYE_LOWER,	"Lower case the line"},
  {"UPPER",	LYE_UPPER,	"Upper case the line"},
  SEPARATOR,
  {"LKCMD",	LYE_LKCMD,	"Invoke command prompt"},
  {"SWMAP",	LYE_SWMAP,	"Switch input keymap"},
  SEPARATOR,
  {"C1CHAR",	LYE_C1CHAR,	"Insert C1 char if printable"},
  {"SETM1",	LYE_SETM1,	"Set modifier 1 flag"},
  {"SETM2",	LYE_SETM2,	"Set modifier 2 flag"},
  {"UNMOD",	LYE_UNMOD,	"Fall back to no-modifier command"},
  SEPARATOR,
  {"TPOS",	LYE_TPOS,	"Transpose characters"},
  {"SETMARK",	LYE_SETMARK,	"emacs-like set-mark-command"},
  {"XPMARK",	LYE_XPMARK,	"emacs-like exchange-point-and-mark"},
  {"KILLREG",	LYE_KILLREG,	"emacs-like kill-region"},
  {"YANK",	LYE_YANK,	"emacs-like yank"},
#ifdef CAN_CUT_AND_PASTE
  SEPARATOR,
  {"PASTE",	LYE_PASTE,	"ClipBoard to Lynx"},
#endif
  SEPARATOR,
  {"AIX",	LYE_AIX,	"Hex 97"},
  {0,           -1,             0},
};
#undef SEPARATOR
/* *INDENT-ON* */

static LYEditCode DefaultEditBinding[KEYMAP_SIZE];

#ifdef USE_ALT_BINDINGS
static LYEditCode BetterEditBinding[KEYMAP_SIZE];
static LYEditCode BashlikeEditBinding[KEYMAP_SIZE];

/*
 * If a modifier bit is set in a lynxkeycode, it is first looked up here.
 *
 * Currently this table isn't specific to the current_lineedit value, it is
 * shared by all alternative "Bindings" to save space.  However, if the
 * modifier flag is set only by a LYE_SETMn lynxeditaction, this table can have
 * effect only for those Bindings that map a lynxkeycode to LYE_SETMn.  ( This
 * doesn't apply if the modifier is already being set in LYgetch().  ) - kw
 */
static LYEditCode Mod1Binding[KEYMAP_SIZE];

/*  Two more tables here, but currently they are all the same.
    In other words, we are cheating to save space, until there
    is a need for different tables. - kw */
static LYEditCode *Mod2Binding = Mod1Binding;
static LYEditCode *Mod3Binding = Mod1Binding;

static const LYEditInit initMod1Binding[] =
{
    {1, LYE_BOL},		/* ^B */
    {2, LYE_BACKW},		/* ^C */
    {3, LYE_UNMOD},		/* ^D */
    {4, LYK_NEXT_LINK | LYE_FORM_LAC},	/* ^E */
    {5, LYK_EDITTEXTAREA | LYE_FORM_LAC},	/* ^F */
    {6, LYE_FORWW},		/* ^G */
    {7, LYE_ABORT},		/* ^H */
    {8, LYE_DELPW},		/* ^I */
    {9, LYE_UNMOD},		/* ^J */
    {10, LYE_ENTER},		/* ^K */
    {11, LYK_LPOS_NEXT_LINK | LYE_FORM_LAC},	/* ^L */
    {12, LYE_FORM_PASS},	/* ^M */
    {13, LYE_ENTER},		/* ^N */
    {14, LYE_FORWW},		/* ^O */
    {15, LYE_UNMOD},		/* ^P */
    {16, LYE_BACKW},		/* ^Q */
    {18, LYE_BACKW},		/* ^S */
    {21, LYE_FORM_PASS},	/* ^V */
    {23, LYE_KILLREG},		/* ^X */
    {24, LYE_XPMARK},		/* ^Y */
    {25, LYE_UNMOD},		/* ^Z */
    {26, LYE_FORM_PASS},	/* ^[ */
    {28, LYE_FORM_PASS},	/* ^] */
    {29, LYE_FORM_PASS},	/* ^^ */
    {30, LYE_UNMOD},		/* ^_ */
    {32, LYE_UNMOD},		/*   */
    {33, LYE_UNMOD},		/* ! */
    {34, LYE_UNMOD},		/* " */
    {35, LYE_UNMOD},		/* # */
    {36, LYE_UNMOD},		/* $ */
    {37, LYE_UNMOD},		/* % */
    {38, LYE_UNMOD},		/* & */
    {39, LYE_UNMOD},		/* ' */
    {40, LYE_UNMOD},		/* ( */
    {41, LYE_UNMOD},		/* ) */
    {42, LYE_UNMOD},		/* * */
    {43, LYE_UNMOD},		/* + */
    {44, LYE_UNMOD},		/* , */
    {45, LYE_UNMOD},		/* - */
    {46, LYE_UNMOD},		/* . */
    {47, LYE_FORM_PASS},	/* / */
    {48, LYE_UNMOD},		/* 0 */
    {49, LYE_UNMOD},		/* 1 */
    {50, LYE_UNMOD},		/* 2 */
    {51, LYE_UNMOD},		/* 3 */
    {52, LYE_UNMOD},		/* 4 */
    {53, LYE_UNMOD},		/* 5 */
    {54, LYE_UNMOD},		/* 6 */
    {55, LYE_UNMOD},		/* 7 */
    {56, LYE_UNMOD},		/* 8 */
    {57, LYE_UNMOD},		/* 9 */
    {58, LYE_UNMOD},		/* : */
    {59, LYE_UNMOD},		/* ; */
    {60, LYK_HOME | LYE_FORM_LAC},	/* < */
    {61, LYE_UNMOD},		/* = */
    {62, LYK_END | LYE_FORM_LAC},	/* > */
    {63, LYE_UNMOD},		/* ? */
    {64, LYE_C1CHAR},		/* @ */
    {65, LYE_C1CHAR},		/* A */
    {66, LYE_C1CHAR},		/* B */
    {67, LYE_C1CHAR},		/* C */
    {68, LYE_C1CHAR},		/* D */
    {69, LYE_C1CHAR},		/* E */
    {70, LYE_C1CHAR},		/* F */
    {71, LYE_C1CHAR},		/* G */
    {72, LYE_C1CHAR},		/* H */
    {73, LYE_C1CHAR},		/* I */
    {74, LYE_C1CHAR},		/* J */
    {75, LYE_C1CHAR},		/* K */
    {76, LYE_C1CHAR},		/* L */
    {77, LYE_C1CHAR},		/* M */
    {78, LYE_C1CHAR},		/* N */
    {79, LYE_C1CHAR},		/* O */
    {80, LYE_C1CHAR},		/* P */
    {81, LYE_C1CHAR},		/* Q */
    {82, LYE_C1CHAR},		/* R */
    {83, LYE_C1CHAR},		/* S */
    {84, LYE_C1CHAR},		/* T */
    {85, LYE_C1CHAR},		/* U */
    {86, LYE_C1CHAR},		/* V */
    {87, LYE_C1CHAR},		/* W */
    {88, LYE_C1CHAR},		/* X */
    {89, LYE_C1CHAR},		/* Y */
    {90, LYE_C1CHAR},		/* Z */
    {91, LYE_C1CHAR},		/* [ */
    {92, LYE_C1CHAR},		/* \ */
    {93, LYE_C1CHAR},		/* ] */
    {94, LYE_C1CHAR},		/* ^ */
    {95, LYE_C1CHAR},		/* _ */
    {96, LYE_UNMOD},		/* ` */
    {97, LYE_BOL},		/* a */
    {98, LYE_BACKW},		/* b */
    {99, LYE_UNMOD},		/* c */
    {100, LYE_DELNW},		/* d */
    {101, LYK_EDITTEXTAREA | LYE_FORM_LAC},	/* e */
    {102, LYE_FORWW},		/* f */
    {103, LYK_GROWTEXTAREA | LYE_FORM_LAC},	/* g */
    {104, LYE_CHAR},		/* h */
    {105, LYK_INSERTFILE | LYE_FORM_LAC},	/* i */
    {106, LYE_CHAR},		/* j */
    {107, LYE_ERASE},		/* k */
    {108, LYE_LOWER},		/* l */
    {109, LYE_CHAR},		/* m */
    {110, LYE_FORM_PASS},	/* n */
    {111, LYE_UNMOD},		/* o */
    {112, LYE_CHAR},		/* p */
    {117, LYE_UPPER},		/* u */
    {122, LYE_UNMOD},		/* z */
    {123, LYE_UNMOD},		/* { */
    {124, LYE_UNMOD},		/* | */
    {125, LYE_UNMOD},		/* } */
    {126, LYE_UNMOD},		/* ~ */
    {127, LYE_DELPW},
    {160, LYE_UNMOD},
    {161, LYE_UNMOD},
    {162, LYE_UNMOD},
    {163, LYE_UNMOD},
    {164, LYE_UNMOD},
    {165, LYE_UNMOD},
    {166, LYE_UNMOD},
    {167, LYE_UNMOD},
    {168, LYE_UNMOD},
    {169, LYE_UNMOD},
    {170, LYE_UNMOD},
    {171, LYE_UNMOD},
    {172, LYE_UNMOD},
    {173, LYE_UNMOD},
    {174, LYE_UNMOD},
    {175, LYE_UNMOD},
    {176, LYE_UNMOD},
    {177, LYE_UNMOD},
    {178, LYE_UNMOD},
    {179, LYE_UNMOD},
    {180, LYE_UNMOD},
    {181, LYE_UNMOD},
    {182, LYE_UNMOD},
    {183, LYE_UNMOD},
    {184, LYE_UNMOD},
    {185, LYE_UNMOD},
    {186, LYE_UNMOD},
    {187, LYE_UNMOD},
    {188, LYE_UNMOD},
    {189, LYE_UNMOD},
    {190, LYE_UNMOD},
    {191, LYE_UNMOD},
    {192, LYE_UNMOD},
    {193, LYE_UNMOD},
    {194, LYE_UNMOD},
    {195, LYE_UNMOD},
    {196, LYE_UNMOD},
    {197, LYE_UNMOD},
    {198, LYE_UNMOD},
    {199, LYE_UNMOD},
    {200, LYE_UNMOD},
    {201, LYE_UNMOD},
    {202, LYE_UNMOD},
    {203, LYE_UNMOD},
    {204, LYE_UNMOD},
    {205, LYE_UNMOD},
    {206, LYE_UNMOD},
    {207, LYE_UNMOD},
    {208, LYE_UNMOD},
    {209, LYE_UNMOD},
    {210, LYE_UNMOD},
    {211, LYE_UNMOD},
    {212, LYE_UNMOD},
    {213, LYE_UNMOD},
    {214, LYE_UNMOD},
    {215, LYE_UNMOD},
    {216, LYE_UNMOD},
    {217, LYE_UNMOD},
    {218, LYE_UNMOD},
    {219, LYE_UNMOD},
    {220, LYE_UNMOD},
    {221, LYE_UNMOD},
    {222, LYE_UNMOD},
    {223, LYE_UNMOD},
    {224, LYE_UNMOD},
    {225, LYE_UNMOD},
    {226, LYE_UNMOD},
    {227, LYE_UNMOD},
    {228, LYE_UNMOD},
    {229, LYE_UNMOD},
    {230, LYE_UNMOD},
    {231, LYE_UNMOD},
    {232, LYE_UNMOD},
    {233, LYE_UNMOD},
    {234, LYE_UNMOD},
    {235, LYE_UNMOD},
    {236, LYE_UNMOD},
    {237, LYE_UNMOD},
    {238, LYE_UNMOD},
    {239, LYE_UNMOD},
    {240, LYE_UNMOD},
    {241, LYE_UNMOD},
    {242, LYE_UNMOD},
    {243, LYE_UNMOD},
    {244, LYE_UNMOD},
    {245, LYE_UNMOD},
    {246, LYE_UNMOD},
    {247, LYE_UNMOD},
    {248, LYE_UNMOD},
    {249, LYE_UNMOD},
    {250, LYE_UNMOD},
    {251, LYE_UNMOD},
    {252, LYE_UNMOD},
    {253, LYE_UNMOD},
    {254, LYE_UNMOD},
    {255, LYE_UNMOD},
    {256, LYE_UNMOD},		/* UPARROW_KEY */
    {257, LYE_UNMOD},		/* DNARROW_KEY */
    {258, LYE_UNMOD},		/* RTARROW_KEY */
    {259, LYE_UNMOD},		/* LTARROW_KEY */
    {260, LYE_UNMOD},		/* PGDOWN_KEY */
    {261, LYE_UNMOD},		/* PGUP_KEY */
    {262, LYE_FORM_PASS},	/* HOME_KEY */
    {263, LYE_FORM_PASS},	/* END_KEY */
    {264, LYK_DWIMHELP | LYE_FORM_LAC},		/* F1 */
    {265, LYE_UNMOD},		/* DO_KEY */
#if (defined(_WINDOWS) || defined(__DJGPP__))
    {266, LYE_UNMOD},		/* FIND_KEY */
    {267, LYE_UNMOD},		/* SELECT_KEY */
#else
    {266, LYK_WHEREIS | LYE_FORM_LAC},	/* FIND_KEY */
    {267, LYK_NEXT | LYE_FORM_LAC},	/* SELECT_KEY */
#endif
    {268, LYE_UNMOD},		/* INSERT_KEY */
    {270, LYE_UNMOD},		/* DO_NOTHING */
    {271, LYE_UNMOD},		/* BACKTAB_KEY */
#if (defined(_WINDOWS) || defined(__DJGPP__)) && defined(USE_SLANG) && !defined(DJGPP_KEYHANDLER)
    {272, LYE_DELPW},
#else
    {272, LYE_UNMOD},
#endif
    {273, LYE_UNMOD},
    {-1, LYE_UNKNOWN}
};

LYEditConfig LYModifierBindings[] =
{
    {"Modifier Binding", initMod1Binding, Mod1Binding},
};

#endif /* USE_ALT_BINDINGS */

static const LYEditInit initDefaultEditor[] =
{
    {1, LYE_BOL},		/* ^B */
    {2, LYE_DELPW},		/* ^C */
    {3, LYE_ABORT},		/* ^D */
    {4, LYE_DELN},		/* ^E */
    {5, LYE_EOL},		/* ^F */
    {6, LYE_DELNW},		/* ^G */
    {7, LYE_ABORT},		/* ^H */
    {8, LYE_DELP},		/* ^I */
    {9, LYE_TAB},		/* ^J */
    {10, LYE_ENTER},		/* ^K */
    {11, LYE_LOWER},		/* ^L */
    {13, LYE_ENTER},		/* ^N */
    {14, LYE_FORWW},		/* ^O */
    {15, LYE_ABORT},		/* ^P */
    {16, LYE_BACKW},		/* ^Q */
    {18, LYE_DELN},		/* ^S */
    {20, LYE_UPPER},		/* ^U */
    {21, LYE_ERASE},		/* ^V */
    {22, LYE_LKCMD},		/* ^W */
#ifdef CAN_CUT_AND_PASTE
    {23, LYE_PASTE},		/* ^X */
#endif
    {24, LYE_SETM1},		/* ^Y */
    {30, LYE_SWMAP},		/* ^_ */
    {31, LYE_DELEL},		/* ^` */
    {32, LYE_CHAR},		/*   */
    {33, LYE_CHAR},		/* ! */
    {34, LYE_CHAR},		/* " */
    {35, LYE_CHAR},		/* # */
    {36, LYE_CHAR},		/* $ */
    {37, LYE_CHAR},		/* % */
    {38, LYE_CHAR},		/* & */
    {39, LYE_CHAR},		/* ' */
    {40, LYE_CHAR},		/* ( */
    {41, LYE_CHAR},		/* ) */
    {42, LYE_CHAR},		/* * */
    {43, LYE_CHAR},		/* + */
    {44, LYE_CHAR},		/* , */
    {45, LYE_CHAR},		/* - */
    {46, LYE_CHAR},		/* . */
    {47, LYE_CHAR},		/* / */
    {48, LYE_CHAR},		/* 0 */
    {49, LYE_CHAR},		/* 1 */
    {50, LYE_CHAR},		/* 2 */
    {51, LYE_CHAR},		/* 3 */
    {52, LYE_CHAR},		/* 4 */
    {53, LYE_CHAR},		/* 5 */
    {54, LYE_CHAR},		/* 6 */
    {55, LYE_CHAR},		/* 7 */
    {56, LYE_CHAR},		/* 8 */
    {57, LYE_CHAR},		/* 9 */
    {58, LYE_CHAR},		/* : */
    {59, LYE_CHAR},		/* ; */
    {60, LYE_CHAR},		/* < */
    {61, LYE_CHAR},		/* = */
    {62, LYE_CHAR},		/* > */
    {63, LYE_CHAR},		/* ? */
    {64, LYE_CHAR},		/* @ */
    {65, LYE_CHAR},		/* A */
    {66, LYE_CHAR},		/* B */
    {67, LYE_CHAR},		/* C */
    {68, LYE_CHAR},		/* D */
    {69, LYE_CHAR},		/* E */
    {70, LYE_CHAR},		/* F */
    {71, LYE_CHAR},		/* G */
    {72, LYE_CHAR},		/* H */
    {73, LYE_CHAR},		/* I */
    {74, LYE_CHAR},		/* J */
    {75, LYE_CHAR},		/* K */
    {76, LYE_CHAR},		/* L */
    {77, LYE_CHAR},		/* M */
    {78, LYE_CHAR},		/* N */
    {79, LYE_CHAR},		/* O */
    {80, LYE_CHAR},		/* P */
    {81, LYE_CHAR},		/* Q */
    {82, LYE_CHAR},		/* R */
    {83, LYE_CHAR},		/* S */
    {84, LYE_CHAR},		/* T */
    {85, LYE_CHAR},		/* U */
    {86, LYE_CHAR},		/* V */
    {87, LYE_CHAR},		/* W */
    {88, LYE_CHAR},		/* X */
    {89, LYE_CHAR},		/* Y */
    {90, LYE_CHAR},		/* Z */
    {91, LYE_CHAR},		/* [ */
    {92, LYE_CHAR},		/* \ */
    {93, LYE_CHAR},		/* ] */
    {94, LYE_CHAR},		/* ^ */
    {95, LYE_CHAR},		/* _ */
    {96, LYE_CHAR},		/* ` */
    {97, LYE_CHAR},		/* a */
    {98, LYE_CHAR},		/* b */
    {99, LYE_CHAR},		/* c */
    {100, LYE_CHAR},		/* d */
    {101, LYE_CHAR},		/* e */
    {102, LYE_CHAR},		/* f */
    {103, LYE_CHAR},		/* g */
    {104, LYE_CHAR},		/* h */
    {105, LYE_CHAR},		/* i */
    {106, LYE_CHAR},		/* j */
    {107, LYE_CHAR},		/* k */
    {108, LYE_CHAR},		/* l */
    {109, LYE_CHAR},		/* m */
    {110, LYE_CHAR},		/* n */
    {111, LYE_CHAR},		/* o */
    {112, LYE_CHAR},		/* p */
    {113, LYE_CHAR},		/* q */
    {114, LYE_CHAR},		/* r */
    {115, LYE_CHAR},		/* s */
    {116, LYE_CHAR},		/* t */
    {117, LYE_CHAR},		/* u */
    {118, LYE_CHAR},		/* v */
    {119, LYE_CHAR},		/* w */
    {120, LYE_CHAR},		/* x */
    {121, LYE_CHAR},		/* y */
    {122, LYE_CHAR},		/* z */
    {123, LYE_CHAR},		/* { */
    {124, LYE_CHAR},		/* | */
    {125, LYE_CHAR},		/* } */
    {126, LYE_CHAR},		/* ~ */
    {127, LYE_DELP},
    {128, LYE_CHAR},
    {129, LYE_CHAR},
    {130, LYE_CHAR},
    {131, LYE_CHAR},
    {132, LYE_CHAR},
    {133, LYE_CHAR},
    {134, LYE_CHAR},
    {135, LYE_CHAR},
    {136, LYE_CHAR},
    {137, LYE_CHAR},
    {138, LYE_CHAR},
    {139, LYE_CHAR},
    {140, LYE_CHAR},
    {141, LYE_CHAR},
#ifdef CJK_EX			/* 1997/11/03 (Mon) 20:30:54 */
    {142, LYE_CHAR},
#else
    {142, LYE_AIX},
#endif
    {143, LYE_CHAR},
    {144, LYE_CHAR},
    {145, LYE_CHAR},
    {146, LYE_CHAR},
    {147, LYE_CHAR},
    {148, LYE_CHAR},
    {149, LYE_CHAR},
    {150, LYE_CHAR},
    {151, LYE_CHAR},
    {152, LYE_CHAR},
    {153, LYE_CHAR},
    {154, LYE_CHAR},
    {155, LYE_CHAR},
    {156, LYE_CHAR},
    {157, LYE_CHAR},
    {158, LYE_CHAR},
    {159, LYE_CHAR},
    {160, LYE_CHAR},
    {161, LYE_CHAR},
    {162, LYE_CHAR},
    {163, LYE_CHAR},
    {164, LYE_CHAR},
    {165, LYE_CHAR},
    {166, LYE_CHAR},
    {167, LYE_CHAR},
    {168, LYE_CHAR},
    {169, LYE_CHAR},
    {170, LYE_CHAR},
    {171, LYE_CHAR},
    {172, LYE_CHAR},
    {173, LYE_CHAR},
    {174, LYE_CHAR},
    {175, LYE_CHAR},
    {176, LYE_CHAR},
    {177, LYE_CHAR},
    {178, LYE_CHAR},
    {179, LYE_CHAR},
    {180, LYE_CHAR},
    {181, LYE_CHAR},
    {182, LYE_CHAR},
    {183, LYE_CHAR},
    {184, LYE_CHAR},
    {185, LYE_CHAR},
    {186, LYE_CHAR},
    {187, LYE_CHAR},
    {188, LYE_CHAR},
    {189, LYE_CHAR},
    {190, LYE_CHAR},
    {191, LYE_CHAR},
    {192, LYE_CHAR},
    {193, LYE_CHAR},
    {194, LYE_CHAR},
    {195, LYE_CHAR},
    {196, LYE_CHAR},
    {197, LYE_CHAR},
    {198, LYE_CHAR},
    {199, LYE_CHAR},
    {200, LYE_CHAR},
    {201, LYE_CHAR},
    {202, LYE_CHAR},
    {203, LYE_CHAR},
    {204, LYE_CHAR},
    {205, LYE_CHAR},
    {206, LYE_CHAR},
    {207, LYE_CHAR},
    {208, LYE_CHAR},
    {209, LYE_CHAR},
    {210, LYE_CHAR},
    {211, LYE_CHAR},
    {212, LYE_CHAR},
    {213, LYE_CHAR},
    {214, LYE_CHAR},
    {215, LYE_CHAR},
    {216, LYE_CHAR},
    {217, LYE_CHAR},
    {218, LYE_CHAR},
    {219, LYE_CHAR},
    {220, LYE_CHAR},
    {221, LYE_CHAR},
    {222, LYE_CHAR},
    {223, LYE_CHAR},
    {224, LYE_CHAR},
    {225, LYE_CHAR},
    {226, LYE_CHAR},
    {227, LYE_CHAR},
    {228, LYE_CHAR},
    {229, LYE_CHAR},
    {230, LYE_CHAR},
    {231, LYE_CHAR},
    {232, LYE_CHAR},
    {233, LYE_CHAR},
    {234, LYE_CHAR},
    {235, LYE_CHAR},
    {236, LYE_CHAR},
    {237, LYE_CHAR},
    {238, LYE_CHAR},
    {239, LYE_CHAR},
    {240, LYE_CHAR},
    {241, LYE_CHAR},
    {242, LYE_CHAR},
    {243, LYE_CHAR},
    {244, LYE_CHAR},
    {245, LYE_CHAR},
    {246, LYE_CHAR},
    {247, LYE_CHAR},
    {248, LYE_CHAR},
    {249, LYE_CHAR},
    {250, LYE_CHAR},
    {251, LYE_CHAR},
    {252, LYE_CHAR},
    {253, LYE_CHAR},
    {254, LYE_CHAR},
    {255, LYE_CHAR},
    {256, LYE_FORM_PASS},	/* UPARROW_KEY */
    {257, LYE_FORM_PASS},	/* DNARROW_KEY */
    {258, LYE_FORW},		/* RTARROW_KEY */
    {259, LYE_BACK},		/* LTARROW_KEY */
    {260, LYE_FORM_PASS},	/* PGDOWN_KEY */
    {261, LYE_FORM_PASS},	/* PGUP_KEY */
    {262, LYE_BOL},		/* HOME_KEY */
    {263, LYE_EOL},		/* END_KEY */
    {264, LYE_FORM_PASS},	/* F1_KEY */
#if !(defined(_WINDOWS) || defined(__DJGPP__))
    {265, LYE_TAB},		/* DO_KEY */
    {266, LYE_BOL},		/* FIND_KEY */
    {267, LYE_EOL},		/* SELECT_KEY */
#endif
    {269, LYE_DELP},		/* REMOVE_KEY */
    {271, LYE_FORM_PASS},	/* BACKTAB_KEY */
#if (defined(_WINDOWS) || defined(__DJGPP__)) && defined(USE_SLANG) && !defined(DJGPP_KEYHANDLER)
    {272, LYE_DELP},
    {273, LYE_ENTER},
#endif
    {-1, LYE_UNKNOWN}
};

#ifdef USE_ALT_BINDINGS
static const LYEditInit initBetterEditor[] =
{
    {1, LYE_BOL},		/* ^B */
    {2, LYE_BACK},		/* ^C */
    {3, LYE_ABORT},		/* ^D */
    {4, LYE_DELN},		/* ^E */
    {5, LYE_EOL},		/* ^F */
    {6, LYE_FORW},		/* ^G */
    {7, LYE_ABORT},		/* ^H */
    {8, LYE_DELP},		/* ^I */
    {9, LYE_ENTER},		/* ^J */
    {10, LYE_ENTER},		/* ^K */
    {11, LYE_DELEL},		/* ^L */
    {13, LYE_ENTER},		/* ^N */
    {14, LYE_FORWW},		/* ^O */
    {15, LYE_ABORT},		/* ^P */
    {16, LYE_BACKW},		/* ^Q */
    {18, LYE_DELPW},		/* ^S */
    {20, LYE_DELNW},		/* ^U */
    {21, LYE_ERASE},		/* ^V */
    {22, LYE_LKCMD},		/* ^W */
#ifdef CAN_CUT_AND_PASTE
    {23, LYE_PASTE},		/* ^X */
#endif
    {24, LYE_SETM1},		/* ^Y */
    {30, LYE_UPPER},		/* ^_ */
    {31, LYE_LOWER},		/* ^` */
    {32, LYE_CHAR},		/*   */
    {33, LYE_CHAR},		/* ! */
    {34, LYE_CHAR},		/* " */
    {35, LYE_CHAR},		/* # */
    {36, LYE_CHAR},		/* $ */
    {37, LYE_CHAR},		/* % */
    {38, LYE_CHAR},		/* & */
    {39, LYE_CHAR},		/* ' */
    {40, LYE_CHAR},		/* ( */
    {41, LYE_CHAR},		/* ) */
    {42, LYE_CHAR},		/* * */
    {43, LYE_CHAR},		/* + */
    {44, LYE_CHAR},		/* , */
    {45, LYE_CHAR},		/* - */
    {46, LYE_CHAR},		/* . */
    {47, LYE_CHAR},		/* / */
    {48, LYE_CHAR},		/* 0 */
    {49, LYE_CHAR},		/* 1 */
    {50, LYE_CHAR},		/* 2 */
    {51, LYE_CHAR},		/* 3 */
    {52, LYE_CHAR},		/* 4 */
    {53, LYE_CHAR},		/* 5 */
    {54, LYE_CHAR},		/* 6 */
    {55, LYE_CHAR},		/* 7 */
    {56, LYE_CHAR},		/* 8 */
    {57, LYE_CHAR},		/* 9 */
    {58, LYE_CHAR},		/* : */
    {59, LYE_CHAR},		/* ; */
    {60, LYE_CHAR},		/* < */
    {61, LYE_CHAR},		/* = */
    {62, LYE_CHAR},		/* > */
    {63, LYE_CHAR},		/* ? */
    {64, LYE_CHAR},		/* @ */
    {65, LYE_CHAR},		/* A */
    {66, LYE_CHAR},		/* B */
    {67, LYE_CHAR},		/* C */
    {68, LYE_CHAR},		/* D */
    {69, LYE_CHAR},		/* E */
    {70, LYE_CHAR},		/* F */
    {71, LYE_CHAR},		/* G */
    {72, LYE_CHAR},		/* H */
    {73, LYE_CHAR},		/* I */
    {74, LYE_CHAR},		/* J */
    {75, LYE_CHAR},		/* K */
    {76, LYE_CHAR},		/* L */
    {77, LYE_CHAR},		/* M */
    {78, LYE_CHAR},		/* N */
    {79, LYE_CHAR},		/* O */
    {80, LYE_CHAR},		/* P */
    {81, LYE_CHAR},		/* Q */
    {82, LYE_CHAR},		/* R */
    {83, LYE_CHAR},		/* S */
    {84, LYE_CHAR},		/* T */
    {85, LYE_CHAR},		/* U */
    {86, LYE_CHAR},		/* V */
    {87, LYE_CHAR},		/* W */
    {88, LYE_CHAR},		/* X */
    {89, LYE_CHAR},		/* Y */
    {90, LYE_CHAR},		/* Z */
    {91, LYE_CHAR},		/* [ */
    {92, LYE_CHAR},		/* \ */
    {93, LYE_CHAR},		/* ] */
    {94, LYE_CHAR},		/* ^ */
    {95, LYE_CHAR},		/* _ */
    {96, LYE_CHAR},		/* ` */
    {97, LYE_CHAR},		/* a */
    {98, LYE_CHAR},		/* b */
    {99, LYE_CHAR},		/* c */
    {100, LYE_CHAR},		/* d */
    {101, LYE_CHAR},		/* e */
    {102, LYE_CHAR},		/* f */
    {103, LYE_CHAR},		/* g */
    {104, LYE_CHAR},		/* h */
    {105, LYE_CHAR},		/* i */
    {106, LYE_CHAR},		/* j */
    {107, LYE_CHAR},		/* k */
    {108, LYE_CHAR},		/* l */
    {109, LYE_CHAR},		/* m */
    {110, LYE_CHAR},		/* n */
    {111, LYE_CHAR},		/* o */
    {112, LYE_CHAR},		/* p */
    {113, LYE_CHAR},		/* q */
    {114, LYE_CHAR},		/* r */
    {115, LYE_CHAR},		/* s */
    {116, LYE_CHAR},		/* t */
    {117, LYE_CHAR},		/* u */
    {118, LYE_CHAR},		/* v */
    {119, LYE_CHAR},		/* w */
    {120, LYE_CHAR},		/* x */
    {121, LYE_CHAR},		/* y */
    {122, LYE_CHAR},		/* z */
    {123, LYE_CHAR},		/* { */
    {124, LYE_CHAR},		/* | */
    {125, LYE_CHAR},		/* } */
    {126, LYE_CHAR},		/* ~ */
    {127, LYE_DELP},
    {128, LYE_CHAR},
    {129, LYE_CHAR},
    {130, LYE_CHAR},
    {131, LYE_CHAR},
    {132, LYE_CHAR},
    {133, LYE_CHAR},
    {134, LYE_CHAR},
    {135, LYE_CHAR},
    {136, LYE_CHAR},
    {137, LYE_CHAR},
    {138, LYE_CHAR},
    {139, LYE_CHAR},
    {140, LYE_CHAR},
    {141, LYE_CHAR},
#ifdef CJK_EX			/* 1997/11/03 (Mon) 20:30:54 */
    {142, LYE_CHAR},
#else
    {142, LYE_AIX},
#endif
    {143, LYE_CHAR},
    {144, LYE_CHAR},
    {145, LYE_CHAR},
    {146, LYE_CHAR},
    {147, LYE_CHAR},
    {148, LYE_CHAR},
    {149, LYE_CHAR},
    {150, LYE_CHAR},
    {151, LYE_CHAR},
    {152, LYE_CHAR},
    {153, LYE_CHAR},
    {154, LYE_CHAR},
    {155, LYE_CHAR},
    {156, LYE_CHAR},
    {157, LYE_CHAR},
    {158, LYE_CHAR},
    {159, LYE_CHAR},
    {160, LYE_CHAR},
    {161, LYE_CHAR},
    {162, LYE_CHAR},
    {163, LYE_CHAR},
    {164, LYE_CHAR},
    {165, LYE_CHAR},
    {166, LYE_CHAR},
    {167, LYE_CHAR},
    {168, LYE_CHAR},
    {169, LYE_CHAR},
    {170, LYE_CHAR},
    {171, LYE_CHAR},
    {172, LYE_CHAR},
    {173, LYE_CHAR},
    {174, LYE_CHAR},
    {175, LYE_CHAR},
    {176, LYE_CHAR},
    {177, LYE_CHAR},
    {178, LYE_CHAR},
    {179, LYE_CHAR},
    {180, LYE_CHAR},
    {181, LYE_CHAR},
    {182, LYE_CHAR},
    {183, LYE_CHAR},
    {184, LYE_CHAR},
    {185, LYE_CHAR},
    {186, LYE_CHAR},
    {187, LYE_CHAR},
    {188, LYE_CHAR},
    {189, LYE_CHAR},
    {190, LYE_CHAR},
    {191, LYE_CHAR},
    {192, LYE_CHAR},
    {193, LYE_CHAR},
    {194, LYE_CHAR},
    {195, LYE_CHAR},
    {196, LYE_CHAR},
    {197, LYE_CHAR},
    {198, LYE_CHAR},
    {199, LYE_CHAR},
    {200, LYE_CHAR},
    {201, LYE_CHAR},
    {202, LYE_CHAR},
    {203, LYE_CHAR},
    {204, LYE_CHAR},
    {205, LYE_CHAR},
    {206, LYE_CHAR},
    {207, LYE_CHAR},
    {208, LYE_CHAR},
    {209, LYE_CHAR},
    {210, LYE_CHAR},
    {211, LYE_CHAR},
    {212, LYE_CHAR},
    {213, LYE_CHAR},
    {214, LYE_CHAR},
    {215, LYE_CHAR},
    {216, LYE_CHAR},
    {217, LYE_CHAR},
    {218, LYE_CHAR},
    {219, LYE_CHAR},
    {220, LYE_CHAR},
    {221, LYE_CHAR},
    {222, LYE_CHAR},
    {223, LYE_CHAR},
    {224, LYE_CHAR},
    {225, LYE_CHAR},
    {226, LYE_CHAR},
    {227, LYE_CHAR},
    {228, LYE_CHAR},
    {229, LYE_CHAR},
    {230, LYE_CHAR},
    {231, LYE_CHAR},
    {232, LYE_CHAR},
    {233, LYE_CHAR},
    {234, LYE_CHAR},
    {235, LYE_CHAR},
    {236, LYE_CHAR},
    {237, LYE_CHAR},
    {238, LYE_CHAR},
    {239, LYE_CHAR},
    {240, LYE_CHAR},
    {241, LYE_CHAR},
    {242, LYE_CHAR},
    {243, LYE_CHAR},
    {244, LYE_CHAR},
    {245, LYE_CHAR},
    {246, LYE_CHAR},
    {247, LYE_CHAR},
    {248, LYE_CHAR},
    {249, LYE_CHAR},
    {250, LYE_CHAR},
    {251, LYE_CHAR},
    {252, LYE_CHAR},
    {253, LYE_CHAR},
    {254, LYE_CHAR},
    {255, LYE_CHAR},
    {256, LYE_FORM_PASS},	/* UPARROW_KEY */
    {257, LYE_FORM_PASS},	/* DNARROW_KEY */
    {258, LYE_FORW},		/* RTARROW_KEY */
    {259, LYE_BACK},		/* LTARROW_KEY */
    {260, LYE_FORM_PASS},	/* PGDOWN_KEY */
    {261, LYE_FORM_PASS},	/* PGUP_KEY */
    {262, LYE_BOL},		/* HOME_KEY */
    {263, LYE_EOL},		/* END_KEY */
    {264, LYE_FORM_PASS},	/* F1_KEY */
#if !(defined(_WINDOWS) || defined(__DJGPP__))
    {265, LYE_TAB},		/* DO_KEY */
    {266, LYE_BOL},		/* FIND_KEY */
    {267, LYE_EOL},		/* SELECT_KEY */
#endif
    {269, LYE_DELP},		/* REMOVE_KEY */
    {271, LYE_FORM_PASS},	/* BACKTAB_KEY */
#if (defined(_WINDOWS) || defined(__DJGPP__)) && defined(USE_SLANG) && !defined(DJGPP_KEYHANDLER)
    {272, LYE_DELP},
    {273, LYE_ENTER},
#endif
    {-1, LYE_UNKNOWN}
};

static const LYEditInit initBashlikeEditor[] =
{
    {0, LYE_SETMARK},		/* nul */
    {1, LYE_BOL},		/* ^B */
    {2, LYE_BACK},		/* ^C */
    {3, LYE_ABORT},		/* ^D */
    {4, LYE_DELN},		/* ^E */
    {5, LYE_EOL | LYE_DF},	/* ^F */
    {6, LYE_FORW},		/* ^G */
    {7, LYE_ABORT},		/* ^H */
    {8, LYE_DELP},		/* ^I */
    {9, LYE_TAB},		/* ^J */
    {10, LYE_ENTER},		/* ^K */
    {11, LYE_DELEL | LYE_DF},	/* ^L */
    {12, LYE_FORM_PASS},	/* ^M */
    {13, LYE_ENTER},		/* ^N */
    {14, LYE_FORM_PASS},	/* ^O */
    {15, LYE_FORM_PASS},	/* ^P */
    {16, LYE_FORM_PASS},	/* ^Q */
    {18, LYE_BACKW},		/* ^S */
    {19, LYE_FORWW},		/* XOFF */
    {20, LYE_TPOS},		/* ^U */
    {21, LYE_DELBL},		/* ^V */
    {22, LYE_LKCMD},		/* ^W */
    {23, LYE_DELPW},		/* ^X */
    {24, LYE_SETM1},		/* ^Y */
    {25, LYE_YANK},		/* ^Z */
    {26, LYE_FORM_PASS},	/* ^[ */
    {27, LYE_SETM2},		/* ^\ */
    {28, LYE_FORM_PASS},	/* ^] */
    {29, LYE_FORM_PASS},	/* ^^ */
    {30, LYE_SWMAP},		/* ^_ */
    {31, LYE_ABORT},		/* ^` */
    {32, LYE_CHAR},		/*   */
    {33, LYE_CHAR},		/* ! */
    {34, LYE_CHAR},		/* " */
    {35, LYE_CHAR},		/* # */
    {36, LYE_CHAR},		/* $ */
    {37, LYE_CHAR},		/* % */
    {38, LYE_CHAR},		/* & */
    {39, LYE_CHAR},		/* ' */
    {40, LYE_CHAR},		/* ( */
    {41, LYE_CHAR},		/* ) */
    {42, LYE_CHAR},		/* * */
    {43, LYE_CHAR},		/* + */
    {44, LYE_CHAR},		/* , */
    {45, LYE_CHAR},		/* - */
    {46, LYE_CHAR},		/* . */
    {47, LYE_CHAR},		/* / */
    {48, LYE_CHAR},		/* 0 */
    {49, LYE_CHAR},		/* 1 */
    {50, LYE_CHAR},		/* 2 */
    {51, LYE_CHAR},		/* 3 */
    {52, LYE_CHAR},		/* 4 */
    {53, LYE_CHAR},		/* 5 */
    {54, LYE_CHAR},		/* 6 */
    {55, LYE_CHAR},		/* 7 */
    {56, LYE_CHAR},		/* 8 */
    {57, LYE_CHAR},		/* 9 */
    {58, LYE_CHAR},		/* : */
    {59, LYE_CHAR},		/* ; */
    {60, LYE_CHAR},		/* < */
    {61, LYE_CHAR},		/* = */
    {62, LYE_CHAR},		/* > */
    {63, LYE_CHAR},		/* ? */
    {64, LYE_CHAR},		/* @ */
    {65, LYE_CHAR},		/* A */
    {66, LYE_CHAR},		/* B */
    {67, LYE_CHAR},		/* C */
    {68, LYE_CHAR},		/* D */
    {69, LYE_CHAR},		/* E */
    {70, LYE_CHAR},		/* F */
    {71, LYE_CHAR},		/* G */
    {72, LYE_CHAR},		/* H */
    {73, LYE_CHAR},		/* I */
    {74, LYE_CHAR},		/* J */
    {75, LYE_CHAR},		/* K */
    {76, LYE_CHAR},		/* L */
    {77, LYE_CHAR},		/* M */
    {78, LYE_CHAR},		/* N */
    {79, LYE_CHAR},		/* O */
    {80, LYE_CHAR},		/* P */
    {81, LYE_CHAR},		/* Q */
    {82, LYE_CHAR},		/* R */
    {83, LYE_CHAR},		/* S */
    {84, LYE_CHAR},		/* T */
    {85, LYE_CHAR},		/* U */
    {86, LYE_CHAR},		/* V */
    {87, LYE_CHAR},		/* W */
    {88, LYE_CHAR},		/* X */
    {89, LYE_CHAR},		/* Y */
    {90, LYE_CHAR},		/* Z */
    {91, LYE_CHAR},		/* [ */
    {92, LYE_CHAR},		/* \ */
    {93, LYE_CHAR},		/* ] */
    {94, LYE_CHAR},		/* ^ */
    {95, LYE_CHAR},		/* _ */
    {96, LYE_CHAR},		/* ` */
    {97, LYE_CHAR},		/* a */
    {98, LYE_CHAR},		/* b */
    {99, LYE_CHAR},		/* c */
    {100, LYE_CHAR},		/* d */
    {101, LYE_CHAR},		/* e */
    {102, LYE_CHAR},		/* f */
    {103, LYE_CHAR},		/* g */
    {104, LYE_CHAR},		/* h */
    {105, LYE_CHAR},		/* i */
    {106, LYE_CHAR},		/* j */
    {107, LYE_CHAR},		/* k */
    {108, LYE_CHAR},		/* l */
    {109, LYE_CHAR},		/* m */
    {110, LYE_CHAR},		/* n */
    {111, LYE_CHAR},		/* o */
    {112, LYE_CHAR},		/* p */
    {113, LYE_CHAR},		/* q */
    {114, LYE_CHAR},		/* r */
    {115, LYE_CHAR},		/* s */
    {116, LYE_CHAR},		/* t */
    {117, LYE_CHAR},		/* u */
    {118, LYE_CHAR},		/* v */
    {119, LYE_CHAR},		/* w */
    {120, LYE_CHAR},		/* x */
    {121, LYE_CHAR},		/* y */
    {122, LYE_CHAR},		/* z */
    {123, LYE_CHAR},		/* { */
    {124, LYE_CHAR},		/* | */
    {125, LYE_CHAR},		/* } */
    {126, LYE_CHAR},		/* ~ */
    {127, LYE_DELP},
    {128, LYE_CHAR},
    {129, LYE_CHAR},
    {130, LYE_CHAR},
    {131, LYE_CHAR},
    {132, LYE_CHAR},
    {133, LYE_CHAR},
    {134, LYE_CHAR},
    {135, LYE_CHAR},
    {136, LYE_CHAR},
    {137, LYE_CHAR},
    {138, LYE_CHAR},
    {139, LYE_CHAR},
    {140, LYE_CHAR},
    {141, LYE_CHAR},
    {142, LYE_CHAR},
    {143, LYE_CHAR},
    {144, LYE_CHAR},
    {145, LYE_CHAR},
    {146, LYE_CHAR},
    {147, LYE_CHAR},
    {148, LYE_CHAR},
    {149, LYE_CHAR},
    {150, LYE_CHAR},
    {151, LYE_AIX},
    {152, LYE_CHAR},
    {153, LYE_CHAR},
    {154, LYE_CHAR},
    {155, LYE_CHAR},
    {156, LYE_CHAR},
    {157, LYE_CHAR},
    {158, LYE_CHAR},
    {159, LYE_CHAR},
    {160, LYE_CHAR},
    {161, LYE_CHAR},
    {162, LYE_CHAR},
    {163, LYE_CHAR},
    {164, LYE_CHAR},
    {165, LYE_CHAR},
    {166, LYE_CHAR},
    {167, LYE_CHAR},
    {168, LYE_CHAR},
    {169, LYE_CHAR},
    {170, LYE_CHAR},
    {171, LYE_CHAR},
    {172, LYE_CHAR},
    {173, LYE_CHAR},
    {174, LYE_CHAR},
    {175, LYE_CHAR},
    {176, LYE_CHAR},
    {177, LYE_CHAR},
    {178, LYE_CHAR},
    {179, LYE_CHAR},
    {180, LYE_CHAR},
    {181, LYE_CHAR},
    {182, LYE_CHAR},
    {183, LYE_CHAR},
    {184, LYE_CHAR},
    {185, LYE_CHAR},
    {186, LYE_CHAR},
    {187, LYE_CHAR},
    {188, LYE_CHAR},
    {189, LYE_CHAR},
    {190, LYE_CHAR},
    {191, LYE_CHAR},
    {192, LYE_CHAR},
    {193, LYE_CHAR},
    {194, LYE_CHAR},
    {195, LYE_CHAR},
    {196, LYE_CHAR},
    {197, LYE_CHAR},
    {198, LYE_CHAR},
    {199, LYE_CHAR},
    {200, LYE_CHAR},
    {201, LYE_CHAR},
    {202, LYE_CHAR},
    {203, LYE_CHAR},
    {204, LYE_CHAR},
    {205, LYE_CHAR},
    {206, LYE_CHAR},
    {207, LYE_CHAR},
    {208, LYE_CHAR},
    {209, LYE_CHAR},
    {210, LYE_CHAR},
    {211, LYE_CHAR},
    {212, LYE_CHAR},
    {213, LYE_CHAR},
    {214, LYE_CHAR},
    {215, LYE_CHAR},
    {216, LYE_CHAR},
    {217, LYE_CHAR},
    {218, LYE_CHAR},
    {219, LYE_CHAR},
    {220, LYE_CHAR},
    {221, LYE_CHAR},
    {222, LYE_CHAR},
    {223, LYE_CHAR},
    {224, LYE_CHAR},
    {225, LYE_CHAR},
    {226, LYE_CHAR},
    {227, LYE_CHAR},
    {228, LYE_CHAR},
    {229, LYE_CHAR},
    {230, LYE_CHAR},
    {231, LYE_CHAR},
    {232, LYE_CHAR},
    {233, LYE_CHAR},
    {234, LYE_CHAR},
    {235, LYE_CHAR},
    {236, LYE_CHAR},
    {237, LYE_CHAR},
    {238, LYE_CHAR},
    {239, LYE_CHAR},
    {240, LYE_CHAR},
    {241, LYE_CHAR},
    {242, LYE_CHAR},
    {243, LYE_CHAR},
    {244, LYE_CHAR},
    {245, LYE_CHAR},
    {246, LYE_CHAR},
    {247, LYE_CHAR},
    {248, LYE_CHAR},
    {249, LYE_CHAR},
    {250, LYE_CHAR},
    {251, LYE_CHAR},
    {252, LYE_CHAR},
    {253, LYE_CHAR},
    {254, LYE_CHAR},
    {255, LYE_CHAR},
    {256, LYE_FORM_PASS},	/* UPARROW_KEY */
    {257, LYE_FORM_PASS},	/* DNARROW_KEY */
    {258, LYE_FORW},		/* RTARROW_KEY */
    {259, LYE_BACK},		/* LTARROW_KEY */
    {260, LYE_FORM_PASS},	/* PGDOWN_KEY */
    {261, LYE_FORM_PASS},	/* PGUP_KEY */
    {262, LYE_BOL},		/* HOME_KEY */
    {263, LYE_EOL},		/* END_KEY */
    {264, LYE_FORM_PASS},	/* F1_KEY */
#if !(defined(_WINDOWS) || defined(__DJGPP__))
    {265, LYE_TAB},		/* DO_KEY */
    {266, LYE_BOL},		/* FIND_KEY */
    {267, LYE_EOL},		/* SELECT_KEY */
#endif
    {269, LYE_DELN},		/* REMOVE_KEY */
    {271, LYE_FORM_PASS},	/* BACKTAB_KEY */
#if (defined(_WINDOWS) || defined(__DJGPP__)) && defined(USE_SLANG) && !defined(DJGPP_KEYHANDLER)
    {272, LYE_DELP},
    {273, LYE_ENTER},
#endif
    {-1, LYE_UNKNOWN}
};
#endif /* USE_ALT_BINDINGS */

LYEditConfig LYLineEditors[] =
{
    {"Default Binding", initDefaultEditor, DefaultEditBinding},
#ifdef USE_ALT_BINDINGS
    {"Alternate Bindings", initBetterEditor, BetterEditBinding},
    {"Bash-like Bindings", initBashlikeEditor, BashlikeEditBinding},
#endif
};

const char *LYEditorNames[TABLESIZE(LYLineEditors) + 1];

/*
 * Add the URL (relative to helpfilepath) used for context-dependent
 * help on form field editing.
 *
 * The order must correspond to that of LYLineditNames.
 */
const char *LYLineeditHelpURLs[] =
{
    EDIT_HELP,
#ifdef USE_ALT_BINDINGS
    ALT_EDIT_HELP,
    BASHLIKE_EDIT_HELP,
#endif
    (char *) 0
};

static struct emap *name2emap(const char *name)
{
    struct emap *mp;
    struct emap *result = 0;

    if (non_empty(name)) {
	for (mp = ekmap; mp->name != NULL; mp++) {
	    if (strcasecomp(mp->name, name) == 0) {
		result = mp;
		break;
	    }
	}
    }
    return result;
}

static struct emap *code2emap(int code)
{
    struct emap *mp;
    struct emap *result = 0;

    for (mp = ekmap; mp->name != NULL; mp++) {
	if (mp->code == code) {
	    result = mp;
	    break;
	}
    }
    return result;
}

/*
 * Return editactioncode whose name is the string func.  func must be present
 * in the ekmap table.  returns -1 if not found.  - kw
 */
int lecname_to_lec(const char *func)
{
    struct emap *mp;
    int result = -1;

    if ((mp = name2emap(func)) != 0) {
	result = mp->code;
    }
    return result;
}

const char *lec_to_lecname(int code)
{
    struct emap *mp;
    const char *result = 0;

    if ((mp = code2emap(code)) != 0) {
	result = mp->name;
    }
    return result;
}

int EditBinding(int xlkc)
{
    int editaction, xleac = LYE_UNMOD;
    int c = xlkc & LKC_MASK;

    if (xlkc == -1)
	return LYE_NOP;		/* maybe LYE_ABORT? or LYE_FORM_LAC|LYK_UNKNOWN? */
#ifdef NOT_ASCII
    if (c < 256) {
	c = TOASCII(c);
    }
#endif
#ifdef USE_ALT_BINDINGS
    /*
     * Get intermediate code from one of the lynxkeycode+modifier tables if
     * applicable, otherwise get the lynxeditactioncode directly.  If we have
     * more than one modifier bits, the first currently wins.  - kw
     */
    if (xlkc & LKC_ISLECLAC) {
	return LKC2_TO_LEC(xlkc);
    } else if (xlkc & LKC_MOD1) {
	xleac = LKC_TO_LEC_M1(c);
    } else if (xlkc & LKC_MOD2) {
	xleac = LKC_TO_LEC_M2(c);
    } else if (xlkc & LKC_MOD3) {
	xleac = LKC_TO_LEC_M3(c);
    } else {
	xleac = UCH(CurrentLineEditor()[c]);
    }
#endif
    /*
     * If we have an intermediate code that says "same as without modifier",
     * look that up now; otherwise we are already done.  - kw
     */
    if (xleac == LYE_UNMOD) {
	editaction = CurrentLineEditor()[c];
    } else {
	editaction = xleac;
    }
    return editaction;
}

/*
 * Install lec as the lynxeditaction for lynxkeycode xlkc.  func must be
 * present in the revmap table.  For normal (non-modifier) lynxkeycodes,
 * select_edi selects which of the alternative line-editor binding tables is
 * modified.  If select_edi is positive, only the table given by it is modified
 * (the DefaultEditBinding table is numbered 1).  If select_edi is 0, all
 * tables are modified.  If select_edi is negative, all tables except the one
 * given by abs(select_edi) are modified.  returns TRUE if the mapping was
 * made, FALSE if not.  Note that this remapping cannot be undone (as might be
 * desirable as a result of re-parsing lynx.cfg), we don't remember the
 * original editaction from the Bindings tables anywhere.  - kw
 */
BOOL LYRemapEditBinding(int xlkc,
			int lec,
			int select_edi)
{
    int j;
    int c = xlkc & LKC_MASK;
    BOOLEAN success = FALSE;

    if (xlkc >= 0 && !(xlkc & LKC_ISLAC) && (c < KEYMAP_SIZE)) {
	LYEditCode code = (LYEditCode) lec;

#ifdef USE_ALT_BINDINGS
	if (xlkc & LKC_MOD1) {
	    if (c <= LAST_MOD1_LKC) {
		Mod1Binding[c] = code;
		success = TRUE;
	    }
	} else if (xlkc & LKC_MOD2) {
	    if (c <= LAST_MOD2_LKC) {
		Mod2Binding[c] = code;
		success = TRUE;
	    }
	} else if (xlkc & LKC_MOD3) {
	    if (c <= LAST_MOD3_LKC) {
		Mod3Binding[c] = code;
		success = TRUE;
	    }
	} else
#endif /* USE_ALT_BINDINGS */
	{
#ifndef UCHAR_MAX
#define UCHAR_MAX 255
#endif
	    if ((unsigned int) lec <= UCHAR_MAX) {
		if (select_edi > 0) {
		    if ((unsigned int) select_edi < TABLESIZE(LYLineEditors)) {
			LYLineEditors[select_edi - 1].used[c] = code;
			success = TRUE;
		    }
		} else {
		    for (j = 0; j < (int) TABLESIZE(LYLineEditors); j++) {
			success = TRUE;
			if ((select_edi < 0) && ((j + 1 + select_edi) == 0))
			    continue;
			LYLineEditors[j].used[c] = code;
		    }
		}
	    }
	}
    }
    return success;
}

/*
 * Macro to walk through lkc-indexed tables up to imax, in the (ASCII) order
 *     97 - 122  ('a' - 'z'),
 *     32 -  96  (' ' - '`', includes 'A' - 'Z'),
 *    123 - 126  ('{' - '~'),
 *      0 -  31  (^@  - ^_),
 *    256 - imax,
 *    127 - 255
 */
#define NEXT_I(i,imax) ((i==122) ? 32 : (i==96) ? 123 : (i==126) ? 0 :\
			(i==31) ? 256 : (i==imax) ? 127 :\
			(i==255) ? (-1) :i+1)
#define FIRST_I 97

int LYKeyForEditAction(int lec)
{
    int editaction, i;

    for (i = FIRST_I; i >= 0; i = NEXT_I(i, KEYMAP_SIZE - 1)) {
	editaction = CurrentLineEditor()[i];
	if (editaction == lec) {
#ifdef NOT_ASCII
	    if (i < 256) {
		return FROMASCII(i);
	    } else
#endif
		return i;
	}
    }
    return (-1);
}

/*
 * Given a lynxactioncode, return a key (lynxkeycode) or sequence of two keys
 * that results in the given action while forms-editing.  The main keycode is
 * returned as function value, possibly with modifier bits set; in addition, if
 * applicable, a key that sets the required modifier flag is returned in
 * *pmodkey if (pmodkey!=NULL).  Non-lineediting bindings that would require
 * typing LYE_LKCMD (default ^V) to activate are not checked here, the caller
 * should do that separately if required.  If no key is bound by current
 * line-editor bindings to the action, -1 is returned.
 *
 * This is all a bit long - it is general enough to continue to work should the
 * three Mod<N>Binding[] become different tables.  - kw
 */
int LYEditKeyForAction(int lac,
		       int *pmodkey)
{
    int editaction, i, c;
    int mod1found = -1, mod2found = -1, mod3found = -1;

    if (pmodkey)
	*pmodkey = -1;
    for (i = FIRST_I; i >= 0; i = NEXT_I(i, KEYMAP_SIZE - 1)) {
	editaction = CurrentLineEditor()[i];
#ifdef NOT_ASCII
	if (i < 256) {
	    c = FROMASCII(i);
	} else
#endif
	    c = i;
	if (editaction == (lac | LYE_FORM_LAC))
	    return c;
	if (editaction == LYE_FORM_PASS) {
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
	    if (lynx_edit_mode && !no_dired_support && lac &&
		LKC_TO_LAC(key_override, c) == lac)
		return c;
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
	    if (LKC_TO_LAC(keymap, c) == lac)
		return c;
	}
	if (editaction == LYE_TAB) {
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
	    if (lynx_edit_mode && !no_dired_support && lac &&
		LKC_TO_LAC(key_override, '\t') == lac)
		return c;
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
	    if (LKC_TO_LAC(keymap, '\t') == lac)
		return c;
	}
	if (editaction == LYE_SETM1 && mod1found < 0)
	    mod1found = i;
	if (editaction == LYE_SETM2 && mod2found < 0)
	    mod2found = i;
	if ((editaction & LYE_DF) && mod3found < 0)
	    mod3found = i;
    }
#ifdef USE_ALT_BINDINGS
    if (mod3found >= 0) {
	for (i = mod3found; i >= 0; i = NEXT_I(i, LAST_MOD3_LKC)) {
	    editaction = CurrentLineEditor()[i];
	    if (!(editaction & LYE_DF))
		continue;
	    editaction = Mod3Binding[i];
#ifdef NOT_ASCII
	    if (i < 256) {
		c = FROMASCII(i);
	    } else
#endif
		c = i;
	    if (pmodkey)
		*pmodkey = c;
	    if (editaction == (lac | LYE_FORM_LAC))
		return (c | LKC_MOD3);
	    if (editaction == LYE_FORM_PASS) {
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
		if (lynx_edit_mode && !no_dired_support && lac &&
		    LKC_TO_LAC(key_override, c) == lac)
		    return (c | LKC_MOD3);
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
		if (LKC_TO_LAC(keymap, c) == lac)
		    return (c | LKC_MOD3);
	    }
	    if (editaction == LYE_TAB) {
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
		if (lynx_edit_mode && !no_dired_support && lac &&
		    LKC_TO_LAC(key_override, '\t') == lac)
		    return (c | LKC_MOD3);
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
		if (LKC_TO_LAC(keymap, '\t') == lac)
		    return (c | LKC_MOD3);
	    }
	}
    }
    if (mod1found >= 0) {
	if (pmodkey) {
#ifdef NOT_ASCII
	    if (mod1found < 256) {
		*pmodkey = FROMASCII(mod1found);
	    } else
#endif
		*pmodkey = mod1found;
	}
	for (i = FIRST_I; i >= 0; i = NEXT_I(i, LAST_MOD1_LKC)) {
	    editaction = Mod1Binding[i];
#ifdef NOT_ASCII
	    if (i < 256) {
		c = FROMASCII(i);
	    } else
#endif
		c = i;
	    if (editaction == (lac | LYE_FORM_LAC))
		return (c | LKC_MOD1);
	    if (editaction == LYE_FORM_PASS) {
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
		if (lynx_edit_mode && !no_dired_support && lac &&
		    LKC_TO_LAC(key_override, c) == lac)
		    return (c | LKC_MOD1);
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
		if (LKC_TO_LAC(keymap, c) == lac)
		    return (c | LKC_MOD1);
	    }
	    if (editaction == LYE_TAB) {
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
		if (lynx_edit_mode && !no_dired_support && lac &&
		    LKC_TO_LAC(key_override, '\t') == lac)
		    return (c | LKC_MOD1);
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
		if (LKC_TO_LAC(keymap, '\t') == lac)
		    return (c | LKC_MOD1);
	    }
	}
    }
    if (mod2found >= 0) {
	if (pmodkey) {
#ifdef NOT_ASCII
	    if (mod1found < 256) {
		*pmodkey = FROMASCII(mod1found);
	    } else
#endif
		*pmodkey = mod1found;
	}
	for (i = FIRST_I; i >= 0; i = NEXT_I(i, LAST_MOD2_LKC)) {
	    editaction = Mod2Binding[i];
#ifdef NOT_ASCII
	    if (i < 256) {
		c = FROMASCII(i);
	    } else
#endif
		c = i;
	    if (editaction == (lac | LYE_FORM_LAC))
		return (c | LKC_MOD2);
	    if (editaction == LYE_FORM_PASS) {
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
		if (lynx_edit_mode && !no_dired_support && lac &&
		    LKC_TO_LAC(key_override, c) == lac)
		    return (c | LKC_MOD2);
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
		if (LKC_TO_LAC(keymap, c) == lac)
		    return (c | LKC_MOD2);
	    }
	    if (editaction == LYE_TAB) {
#if defined(DIRED_SUPPORT) && defined(OK_OVERRIDE)
		if (lynx_edit_mode && !no_dired_support && lac &&
		    LKC_TO_LAC(key_override, '\t') == lac)
		    return (c | LKC_MOD2);
#endif /* DIRED_SUPPORT && OK_OVERRIDE */
		if (LKC_TO_LAC(keymap, '\t') == lac)
		    return (c | LKC_MOD2);
	    }
	}
    }
#endif /* USE_ALT_BINDINGS */
    if (pmodkey)
	*pmodkey = -1;
    return (-1);
}

#if 0
/*
 * This function was useful in converting the hand-crafted key-bindings to
 * their reusable form in 2.8.8 -TD
 */
static void checkEditMap(LYEditConfig * table)
{
    unsigned j, k;
    char comment[80];
    int first = TRUE;

    for (j = 0; table->init[j].code >= 0; ++j) {
	int code = table->init[j].code;

	if (table->init[j].edit != table->used[code]) {
	    if (first) {
		printf("TABLE %s\n", table->name);
		first = FALSE;
	    }
	    printf("%u: init %d vs used %d\n",
		   j,
		   table->init[j].edit,
		   table->used[code]);
	}
    }
    for (j = 0; j < KEYMAP_SIZE; ++j) {
	int code = (int) j;
	BOOL found = FALSE;

	for (k = 0; table->init[k].code >= 0; ++k) {
	    if (code == table->init[k].code) {
		found = TRUE;
		break;
	    }
	}
	if (!found) {
	    if (table->used[j] != 0) {
		int edit = table->used[j];
		int has_DF = (edit & LYE_DF);
		int has_LAC = (edit & LYE_FORM_LAC);
		const char *prefix = "LYE_";
		const char *name = 0;

		edit &= 0x7f;
		if (has_LAC) {
		    Kcmd *cmd = LYKeycodeToKcmd(edit);

		    if (cmd != 0) {
			prefix = "LYK_";
			name = cmd->name;
		    }
		} else {
		    name = lec_to_lecname(edit);
		}

		if (j < 32) {
		    char temp[80];
		    const char *what = 0;

		    switch (j) {
		    case 0:
			what = "nul";
			break;
		    case 17:
			what = "XON";
			break;
		    case 19:
			what = "XOFF";
			break;
		    default:
			sprintf(temp, "^%c", j + 'A');
			what = temp;
			break;
		    }
		    sprintf(comment, "\t/* %s */", what);
		} else if (j < 127) {
		    sprintf(comment, "\t/* %c */", j);
		} else {
		    const char *what = LYextraKeysToName(j);

		    if (Non_Empty(what)) {
			sprintf(comment, "\t/* %s%s */", what,
				((StrChr(what, '_') != 0)
				 ? ""
				 : "_KEY"));
		    } else {
			strcpy(comment, "");
		    }
		}
		if (name == 0) {
		    name = "XXX";
		} else if (!strcasecomp(name, "PASS")) {
		    name = "FORM_PASS";
		}
		if (first) {
		    printf("TABLE %s\n", table->name);
		    first = FALSE;
		}
		printf("\t{ %d, %s%s%s%s },%s\n", code, prefix, name,
		       has_DF ? "|LYE_DF" : "",
		       has_LAC ? "|LYE_FORM_LAC" : "",
		       comment);
	    }
	}
    }
}

#else
#define checkEditMap(table)	/* nothing */
#endif

static void initLineEditor(LYEditConfig * table)
{
    unsigned k;
    LYEditCode *used = table->used;
    const LYEditInit *init = table->init;

    memset(used, 0, sizeof(LYEditCode) * KEYMAP_SIZE);
    for (k = 0; init[k].code >= 0; ++k) {
	int code = init[k].code;

	used[code] = init[k].edit;
    }
    checkEditMap(table);
}

/*
 * Reset the editor bindings to their default values.
 */
void LYinitEditmap(void)
{
    unsigned j;

    for (j = 0; j < TABLESIZE(LYLineEditors); ++j) {
	LYEditorNames[j] = LYLineEditors[j].name;
	initLineEditor(&LYLineEditors[j]);
    }
#ifdef USE_ALT_BINDINGS
    for (j = 0; j < TABLESIZE(LYModifierBindings); ++j) {
	initLineEditor(&LYModifierBindings[j]);
    }
#endif
}

static char *showRanges(int *state)
{
    char *result = 0;
    int range[2];
    int i;

    range[0] = range[1] = -1;
    for (i = 0; i < KEYMAP_SIZE; ++i) {
	if (!state[i]) {
	    int code = CurrentLineEditor()[i];

	    if (code == LYE_CHAR) {
		if (range[0] < 0)
		    range[0] = i;
		range[1] = i;
		state[i] = 3;
	    } else if (range[0] >= 0) {
		if (non_empty(result))
		    StrAllocCat(result, ", ");
		HTSprintf(&result, "%d-%d", range[0], range[1]);
		range[0] = range[1] = -1;
	    }
	}
    }
    return result;
}

static int LYLoadEditmap(const char *arg GCC_UNUSED,
			 HTParentAnchor *anAnchor,
			 HTFormat format_out,
			 HTStream *sink)
{
#define FORMAT "  %-*s  %-*s  -  %s\n"
    HTFormat format_in = WWW_HTML;
    HTStream *target;
    int state[KEYMAP_SIZE];
    int width[2];
    char *buf = 0;
    char *ranges = 0;
    struct emap *mp;
    int i;
    int hanging;
    int wrapped;
    int had_output = FALSE;
    int result;

    if ((target = HTStreamStack(format_in, format_out, sink, anAnchor)) != 0) {
	anAnchor->no_cache = TRUE;

	HTSprintf0(&buf,
		   "<html>\n<head>\n<title>%s</title>\n</head>\n<body>\n",
		   CURRENT_EDITMAP_TITLE);
	PUTS(buf);
	HTSprintf0(&buf, "<pre>\n");
	PUTS(buf);

	/* determine the column-widths we will use for showing bindings */
	width[0] = 0;
	width[1] = 0;
	for (i = 0; i < KEYMAP_SIZE; ++i) {
	    int code = CurrentLineEditor()[i];

	    if (code == LYE_NOP) {
		state[i] = 1;
	    } else {
		int need;

		if ((mp = code2emap(code)) != 0) {
		    state[i] = 0;
		    if ((need = (int) strlen(mp->name)) > width[0])
			width[0] = need;
		    if ((need = (int) strlen(mp->descr)) > width[1])
			width[1] = need;
		} else {
		    state[i] = 2;
		}
	    }
	}
	hanging = 2 + width[0] + 2 + width[1] + 5;
	wrapped = hanging;

	/*
	 * Tell which set of bindings we are showing, and link to the
	 * handcrafted page, which adds explanations.
	 */
	PUTS(gettext("These are the current edit-bindings:"));
	HTSprintf0(&buf,
		   " <a href=\"%s\">%s</a>\n\n",
		   LYLineeditHelpURL(),
		   LYEditorNames[current_lineedit]);
	PUTS(buf);

	/* Show by groups to match the arrangement in the handmade files. */
	for (mp = ekmap; mp->name != 0; ++mp) {
	    if (isEmpty(mp->name)) {
		if (had_output) {
		    PUTS("\n");
		    had_output = FALSE;
		}
	    } else if (mp->code == LYE_CHAR) {
		ranges = showRanges(state);
		HTSprintf0(&buf, FORMAT,
			   width[0], mp->name,
			   width[1], mp->descr,
			   ranges);
		FREE(ranges);
		PUTS(buf);
		had_output = TRUE;
	    } else {
		for (i = 0; i < KEYMAP_SIZE; ++i) {
		    int code = CurrentLineEditor()[i];

		    if ((code == mp->code) && !state[i]) {
			char *value = LYKeycodeToString(i, (i >= 160 &&
							    i <= 255));
			int before = wrapped + (ranges ? ((int)
							  strlen(ranges)) : 0);
			int after = before;

			if (non_empty(ranges)) {
			    StrAllocCat(ranges, ", ");
			    after += 2;
			}
			after += (int) strlen(value) + 2;
			if ((before / LYcols) != (after / LYcols)) {
			    wrapped += (LYcols - (before % LYcols));
			    HTSprintf(&ranges, "\n%-*s", hanging, " ");
			}
			StrAllocCat(ranges, value);
		    }
		}
		if (non_empty(ranges)) {
		    LYEntify(&ranges, TRUE);
		    HTSprintf0(&buf, FORMAT,
			       width[0], mp->name,
			       width[1], mp->descr,
			       ranges);
		    PUTS(buf);
		    FREE(ranges);
		    had_output = TRUE;
		}
	    }
	}

	HTSprintf0(&buf, "</pre>\n</body>\n</html>\n");
	PUTS(buf);

	(*target->isa->_free) (target);
	result = HT_LOADED;
    } else {
	HTSprintf0(&buf, CANNOT_CONVERT_I_TO_O,
		   HTAtom_name(format_in), HTAtom_name(format_out));
	HTAlert(buf);
	result = HT_NOT_LOADED;
    }
    FREE(ranges);
    FREE(buf);
    return result;
#undef FORMAT
}

#ifdef GLOBALDEF_IS_MACRO
#define _LYEDITMAP_C_GLOBALDEF_1_INIT { "LYNXEDITMAP", LYLoadEditmap, 0}
GLOBALDEF(HTProtocol, LYLynxEditmap, _LYEDITMAP_C_GLOBALDEF_1_INIT);
#else
GLOBALDEF HTProtocol LYLynxEditmap =
{
    "LYNXEDITMAP", LYLoadEditmap, 0
};
#endif /* GLOBALDEF_IS_MACRO */
