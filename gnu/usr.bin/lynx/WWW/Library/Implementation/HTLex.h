/*                         LEXICAL ANALYSOR (MAINLY FOR CONFIG FILES)

 */

#ifndef HTLEX_H
#define HTLEX_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif
 
typedef enum {
    LEX_NONE,		/* Internally used      */
    LEX_EOF,		/* End of file          */
    LEX_REC_SEP,	/* Record separator     */
    LEX_FIELD_SEP,	/* Field separator      */
    LEX_ITEM_SEP,	/* List item separator  */
    LEX_OPEN_PAREN,	/* Group start tag      */
    LEX_CLOSE_PAREN,	/* Group end tag        */
    LEX_AT_SIGN,	/* Address qualifier    */
    LEX_ALPH_STR,	/* Alphanumeric string  */
    LEX_TMPL_STR	/* Template string      */
} LexItem;

extern char HTlex_buffer[];	/* Read lexical string          */
extern int HTlex_line;		/* Line number in source file   */

/*

Get Next Lexical Item

   If returns LEX_ALPH_STR or LEX_TMPL_STR the string is in global buffer lex_buffer.

 */

PUBLIC LexItem lex PARAMS((FILE * fp));
/*

Push Back Latest Item

 */

PUBLIC void unlex PARAMS((LexItem lex_item));
/*

Get the Name for Lexical Item

 */

PUBLIC char *lex_verbose PARAMS((LexItem lex_item));
/*

 */

#endif /* not HTLEX_H */
