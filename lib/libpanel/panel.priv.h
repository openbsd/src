/* $Id: panel.priv.h,v 1.1 1997/11/26 03:56:06 millert Exp $ */

#ifndef _PANEL_PRIV_H
#define _PANEL_PRIV_H

#if HAVE_CONFIG_H
#  include <ncurses_cfg.h>
#endif

#include <stdlib.h>
#include <assert.h>

#if HAVE_LIBDMALLOC
#include <dmalloc.h>    /* Gray Watson's library */
#endif

#if HAVE_LIBDBMALLOC
#include <dbmalloc.h>   /* Conor Cahill's library */
#endif

#include "panel.h"


#if ( CC_HAS_INLINE_FUNCS && !defined(TRACE) )
#  define INLINE inline
#else
#  define INLINE
#endif


typedef struct panelcons
{
  struct panelcons *above;
  struct panel *pan;
} PANELCONS;

#ifdef TRACE
#  define dBug(x) _tracef x
#else
#  define dBug(x)
#endif

#ifdef USE_RCS_IDS
#define MODULE_ID(id) static const char Ident[] = id;
#else
#define MODULE_ID(id) /*nothing*/
#endif

#define P_TOUCH  (0)
#define P_UPDATE (1)

#endif
