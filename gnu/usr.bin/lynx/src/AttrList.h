/*
 * $LynxId: AttrList.h,v 1.17 2013/05/03 20:54:09 tom Exp $
 */
#if !defined(__ATTRLIST_H)
#define __ATTRLIST_H

#include <HText.h>
#include <HTMLDTD.h>

#ifdef __cplusplus
extern "C" {
#endif
    enum {
	ABS_OFF = 0,
	STACK_OFF = 0,
	STACK_ON,
	ABS_ON
    };

#define STARTAT 8

    enum {
	DSTYLE_LINK = HTML_A + STARTAT,
	DSTYLE_STATUS = HTML_ELEMENTS + STARTAT,
	DSTYLE_ALINK,		/* active link */
	DSTYLE_NORMAL,		/* default attributes */
	DSTYLE_OPTION,		/* option on the option screen */
	DSTYLE_VALUE,		/* value on the option screen */
	DSTYLE_CANDY,		/* possibly going to vanish */
	DSTYLE_WHEREIS,		/* whereis search target */
	DSTYLE_ELEMENTS
    };

    typedef struct {
	int color;		/* color highlighting to be done */
	int mono;		/* mono highlighting to be done */
	int cattr;		/* attributes to go with the color */
    } HTCharStyle;

#if 0
#define HText_characterStyle CTRACE((tfp,"HTC called from %s/%d\n",__FILE__,__LINE__));_internal_HTC
#else
#define HText_characterStyle _internal_HTC
#endif

#if defined(USE_COLOR_STYLE)
    extern void _internal_HTC(HText *text, int style, int dir);

#define TEMPSTRINGSIZE 256
    extern char class_string[TEMPSTRINGSIZE + 1];

/* stack of attributes during page rendering */
#define MAX_LAST_STYLES 128
    extern int last_styles[MAX_LAST_STYLES + 1];
    extern int last_colorattr_ptr;

#endif

#ifdef __cplusplus
}
#endif
#endif
