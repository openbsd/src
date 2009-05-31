
/* MODULE							HTLex.c
 *		LEXICAL ANALYSOR
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
 */

#include <HTUtils.h>

#include <HTLex.h>		/* Implemented here */

#include <LYLeaks.h>

/*
 * Global variables
 */
char HTlex_buffer[40];		/* Read lexical string          */
int HTlex_line = 1;		/* Line number in source file   */

/*
 * Module-wide variables
 */
static int lex_cnt;
static BOOL lex_template;
static LexItem lex_pushed_back = LEX_NONE;
static FILE *cache = NULL;

void unlex(LexItem lex_item)
{
    lex_pushed_back = lex_item;
}

LexItem lex(FILE *fp)
{
    int ch = 0;

    if (fp != cache) {		/* This cache doesn't work ok because the system  */
	cache = fp;		/* often assign same FILE structure the next open */
	HTlex_line = 1;		/* file.  So, if there are syntax errors in setup *
				   files it may confuse things later on.      */
    }
    if (lex_pushed_back != LEX_NONE) {
	LexItem ret = lex_pushed_back;

	lex_pushed_back = LEX_NONE;
	return ret;
    }

    lex_cnt = 0;
    lex_template = NO;

    for (;;) {
	switch (ch = getc(fp)) {
	case EOF:
	case ' ':
	case '\t':
	case '\r':
	case '\n':
	case ':':
	case ',':
	case '(':
	case ')':
	case '@':
	    if (lex_cnt > 0) {
		if (ch != EOF)
		    ungetc(ch, fp);
		if (lex_template)
		    return LEX_TMPL_STR;
		else
		    return LEX_ALPH_STR;
	    } else
		switch (ch) {
		case EOF:
		    return LEX_EOF;
		case '\n':
		    HTlex_line++;
		    return LEX_REC_SEP;
		case ':':
		    return LEX_FIELD_SEP;
		case ',':
		    return LEX_ITEM_SEP;
		case '(':
		    return LEX_OPEN_PAREN;
		case ')':
		    return LEX_CLOSE_PAREN;
		case '@':
		    return LEX_AT_SIGN;
		default:	/* Leading white space ignored (SP,TAB,CR) */
		    break;
		}
	    break;
	default:
	    if (lex_cnt < (int) (sizeof(HTlex_buffer) - 1))
		HTlex_buffer[lex_cnt++] = (char) ch;
	    HTlex_buffer[lex_cnt] = '\0';
	    if ('*' == ch)
		lex_template = YES;
	}			/* switch ch */
    }				/* forever */
}

const char *lex_verbose(LexItem lex_item)
{
    static char msg[sizeof(HTlex_buffer) + 30];		/* @@@@@@@@ */

    switch (lex_item) {
    case LEX_NONE:		/* Internally used      */
	return "NO-LEX-ITEM";
    case LEX_EOF:		/* End of file          */
	return "end-of-file";
    case LEX_REC_SEP:		/* Record separator     */
	return "record separator (newline)";
    case LEX_FIELD_SEP:	/* Field separator      */
	return "field separator ':'";
    case LEX_ITEM_SEP:		/* List item separator  */
	return "item separator ','";
    case LEX_OPEN_PAREN:	/* Group start tag      */
	return "'('";
    case LEX_CLOSE_PAREN:	/* Group end tag        */
	return "')'";
    case LEX_AT_SIGN:		/* Address qualifier    */
	return "address qualifier '@'";
    case LEX_ALPH_STR:		/* Alphanumeric string  */
	sprintf(msg, "alphanumeric string '%.*s'",
		(int) sizeof(HTlex_buffer), HTlex_buffer);
	return msg;
    case LEX_TMPL_STR:		/* Template string      */
	sprintf(msg, "template string '%.*s'",
		(int) sizeof(HTlex_buffer), HTlex_buffer);
	return msg;
    default:
	return "UNKNOWN-LEX-ITEM";
    }
}
