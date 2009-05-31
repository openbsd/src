/*		The portable font concept (!?*)
*/

/*	Line mode browser version:
*/
#ifndef HTFONT_H
#define HTFONT_H

typedef long int HTMLFont;	/* For now */

#define HT_FONT		0
#define HT_CAPITALS	1
#define HT_BOLD		2
#define HT_UNDERLINE	4
#define HT_INVERSE	8
#define HT_DOUBLE	0x10

#define HT_BLACK	0
#define HT_WHITE	1

/*
 *  Lynx internal character representations.
 */
#define HT_NON_BREAK_SPACE      ((char)1)
#define HT_EN_SPACE             ((char)2)
#define LY_UNDERLINE_START_CHAR	'\003'
#define LY_UNDERLINE_END_CHAR	'\004'

/* Turn about is fair play ASCII platforms use EBCDIC tab;
   EBCDIC platforms use ASCII tab for LY_BOLD_START_CHAR.
*/
#ifdef EBCDIC
#define LY_BOLD_START_CHAR	'\011'
#else
#define LY_BOLD_START_CHAR	'\005'
#endif

#define LY_BOLD_END_CHAR	'\006'
#define LY_SOFT_HYPHEN		((char)7)
#define LY_SOFT_NEWLINE		((char)8)

#ifdef EBCDIC
#define IsSpecialAttrChar(a)	(((a) > '\002') && ((a) <= '\011') && ((a)!='\t'))
#else
#define IsSpecialAttrChar(a)	(((a) > '\002') && ((a) <= '\010'))
#endif

#define IsNormalChar(a)		((a) != '\0' && !IsSpecialAttrChar(a))

#endif /* HTFONT_H */
