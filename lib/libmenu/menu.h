
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

#ifndef ETI_MENU
#define ETI_MENU

#include <curses.h>
#include <term.h>
#include <eti.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int Menu_Options;
typedef int Item_Options;

/* Menu options: */
#define O_ONEVALUE      (0x01)
#define O_SHOWDESC      (0x02)
#define O_ROWMAJOR      (0x04)
#define O_IGNORECASE    (0x08)
#define O_SHOWMATCH     (0x10)
#define O_NONCYCLIC     (0x20)

/* Item options: */
#define O_SELECTABLE    (0x01)

typedef struct
{
  char*          str;
  unsigned short length;
} TEXT;

typedef struct tagITEM 
{
  TEXT           name;        /* name of menu item                         */
  TEXT           description; /* description of item, optional in display  */ 
  struct tagMENU *imenu;      /* Pointer to parent menu                    */
  void           *userptr;    /* Pointer to user defined per item data     */ 
  Item_Options   opt;         /* Item options                              */ 
  short          index;       /* Item number if connected to a menu        */
  short          y;           /* y and x location of item in menu          */
  short          x;
  bool           value;       /* Selection value                           */
                             
  struct tagITEM *left;       /* neighbour items                           */
  struct tagITEM *right;
  struct tagITEM *up;
  struct tagITEM *down;

} ITEM;

typedef void (*Menu_Hook)(struct tagMENU *);

typedef struct tagMENU 
{
  short          height;                /* Nr. of chars high               */
  short          width;                 /* Nr. of chars wide               */
  short          rows;                  /* Nr. of items high               */
  short          cols;                  /* Nr. of items wide               */
  short          frows;                 /* Nr. of formatted items high     */
  short          fcols;                 /* Nr. of formatted items wide     */
  short          namelen;               /* Max. name length                */
  short          desclen;               /* Max. description length         */
  short          marklen;               /* Length of mark, if any          */
  short          itemlen;               /* Length of one item              */
  char          *pattern;               /* Buffer to store match chars     */
  short          pindex;                /* Index into pattern buffer       */
  WINDOW        *win;                   /* Window containing menu          */
  WINDOW        *sub;                   /* Subwindow for menu display      */
  WINDOW        *userwin;               /* User's window                   */
  WINDOW        *usersub;               /* User's subwindow                */
  ITEM          **items;                /* array of items                  */ 
  short          nitems;                /* Nr. of items in menu            */
  ITEM          *curitem;               /* Current item                    */
  short          toprow;                /* Top row of menu                 */
  chtype         fore;                  /* Selection attribute             */
  chtype         back;                  /* Nonselection attribute          */
  chtype         grey;                  /* Inactive attribute              */
  unsigned char  pad;                   /* Pad character                   */

  Menu_Hook      menuinit;              /* User hooks                      */
  Menu_Hook      menuterm;
  Menu_Hook      iteminit;
  Menu_Hook      itemterm;

  void          *userptr;               /* Pointer to menus user data      */
  char          *mark;                  /* Pointer to marker string        */

  Menu_Options   opt;                   /* Menu options                    */
  unsigned short status;                /* Internal state of menu          */

} MENU;


/* Define keys */

#define REQ_LEFT_ITEM           (KEY_MAX + 1)
#define REQ_RIGHT_ITEM          (KEY_MAX + 2)
#define REQ_UP_ITEM             (KEY_MAX + 3)
#define REQ_DOWN_ITEM           (KEY_MAX + 4)
#define REQ_SCR_ULINE           (KEY_MAX + 5)
#define REQ_SCR_DLINE           (KEY_MAX + 6)
#define REQ_SCR_DPAGE           (KEY_MAX + 7)
#define REQ_SCR_UPAGE           (KEY_MAX + 8)
#define REQ_FIRST_ITEM          (KEY_MAX + 9)
#define REQ_LAST_ITEM           (KEY_MAX + 10)
#define REQ_NEXT_ITEM           (KEY_MAX + 11)
#define REQ_PREV_ITEM           (KEY_MAX + 12)
#define REQ_TOGGLE_ITEM         (KEY_MAX + 13)
#define REQ_CLEAR_PATTERN       (KEY_MAX + 14)
#define REQ_BACK_PATTERN        (KEY_MAX + 15)
#define REQ_NEXT_MATCH          (KEY_MAX + 16)
#define REQ_PREV_MATCH          (KEY_MAX + 17)
#define MAX_MENU_COMMAND        (KEY_MAX + 17)

/*
 * Some AT&T code expects MAX_COMMAND to be out-of-band not
 * just for meny commands but for forms ones as well.
 */
#define MAX_COMMAND             (KEY_MAX + 128)

/* --------- prototypes for libmenu functions ----------------------------- */

extern ITEM     **menu_items(const MENU *),
                *current_item(const MENU *),
                *new_item(char *,char *);

extern MENU     *new_menu(ITEM **);

extern Item_Options  item_opts(const ITEM *);
extern Menu_Options  menu_opts(const MENU *);

Menu_Hook       item_init(const MENU *),
                item_term(const MENU *),
                menu_init(const MENU *),
                menu_term(const MENU *);

extern WINDOW   *menu_sub(const MENU *),
                *menu_win(const MENU *);

extern char     *item_description(const ITEM *),
                *item_name(const ITEM *),
                *menu_mark(const MENU *),
                *menu_pattern(const MENU *);

extern char     *item_userptr(const ITEM *),
                *menu_userptr(const MENU *);
  
extern chtype   menu_back(const MENU *),
                menu_fore(const MENU *),
                menu_grey(const MENU *);

extern int      free_item(ITEM *),
                free_menu(MENU *),
                item_count(const MENU *),
                item_index(const ITEM *),
                item_opts_off(ITEM *,Item_Options),
                item_opts_on(ITEM *,Item_Options),
                menu_driver(MENU *,int),
                menu_opts_off(MENU *,Menu_Options),
                menu_opts_on(MENU *,Menu_Options),
                menu_pad(const MENU *),
                pos_menu_cursor(const MENU *),
                post_menu(MENU *),
                scale_menu(const MENU *,int *,int *),
                set_current_item(MENU *menu,ITEM *item),
                set_item_init(MENU *,void(*)(MENU *)),
                set_item_opts(ITEM *,Item_Options),
                set_item_term(MENU *,void(*)(MENU *)),
                set_item_userptr(ITEM *, char *),
                set_item_value(ITEM *,bool),
                set_menu_back(MENU *,chtype),
                set_menu_fore(MENU *,chtype),
                set_menu_format(MENU *,int,int),
                set_menu_grey(MENU *,chtype),
                set_menu_init(MENU *,void(*)(MENU *)),
                set_menu_items(MENU *,ITEM **),
                set_menu_mark(MENU *, char *),
                set_menu_opts(MENU *,Menu_Options),
                set_menu_pad(MENU *,int),
                set_menu_pattern(MENU *,const char *),
                set_menu_sub(MENU *,WINDOW *),
                set_menu_term(MENU *,void(*)(MENU *)),
                set_menu_userptr(MENU *,char *),
                set_menu_win(MENU *,WINDOW *),
                set_top_row(MENU *,int),
                top_row(const MENU *),
                unpost_menu(MENU *);

extern bool     item_value(const ITEM *),
                item_visible(const ITEM *);

void            menu_format(const MENU *,int *,int *);

#ifdef __cplusplus
  }
#endif

#endif /* ETI_MENU */
