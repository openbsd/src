
/* MODULE							HTAAFile.c
**		FILE ROUTINES FOR AUTHENTICATION
**		(PASSWD AND GROUP FILES) AND
**		ACCESS CONTROL LIST (.www_acl)
** AUTHORS:
**	AL	Ari Luotonen	luotonen@dxcern.cern.ch
**
** HISTORY:
**
**
** BUGS:
**
**
*/


#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */

#include "tcp.h"		/* Macro FROMASCII() */
/*#include <stdio.h> included by HTUtils.h -- FM *//* FILE */
#include <string.h>
#include "HTAAUtil.h"		/* Common utilities used in AA */
#include "HTAAFile.h"		/* Implemented here */

#include "LYLeaks.h"

#define SPACE			' '
#define TAB			'\t'



/* PUBLIC						HTAAFile_nextRec()
**		GO TO THE BEGINNING OF THE NEXT RECORD
** ON ENTRY:
**	fp	is the file from which records are read from.
**
** ON EXIT:
**	returns	nothing. File read pointer is located at the beginning
**		of the next record. Handles continuation lines
**		(lines ending in comma indicate a following
**		continuation line).
**
*/
PUBLIC void HTAAFile_nextRec ARGS1(FILE *, fp)
{
    int ch = getc(fp);
    int last = (char)0;

    do {
	while (ch != EOF  &&  ch != CR  &&  ch != LF) {
	    if (ch != ' '  && ch != '\t')
		last = ch;		/* Last non-whitespace */
	    ch = getc(fp);		/* Skip until end-of-line */
	}
	while (ch != EOF &&
	       (ch == CR  ||  ch == LF))/*Skip carriage returns and linefeeds*/
	    ch = getc(fp);
	if (ch != EOF)
	    ungetc(ch, fp);
    } while (last == ',' && ch != EOF);	/* Skip also continuation lines */
}


/* PRIVATE							read_item()
**		READ AN ITEM FROM A PASSWORD, GROUP
**		OR ACCESS CONTROL LIST FILE
**		i.e. either a field, or a list item.
** ON ENTRY:
**	fp		is the file to read the characters from
**	contents	is the character array to put the characters
**	reading_list	if TRUE, read a list item (ends either in
**			acomma or acolon),
**			if FALSE, read a field (ends in acolon).
**	max_len		is the maximum number of characters that may
**			be read (i.e. the size of dest minus one for
**			terminating null).
** ON EXIT:
**	returns		the terminating character
**			(i.e. either separator or CR or LF or EOF).
**	contents	contains a null-terminated string representing
**			the read field.
** NOTE 1:
**			Ignores leading and trailing blanks and tabs.
** NOTE 2:
**			If the item is more than max_len characters
**			long, the rest of the characters in that item
**			are ignored.  However, contents is always
**			null-terminated!
*/
PRIVATE int read_item ARGS4(FILE *,	fp,
			    char *,	contents,
			    BOOL,	reading_list,
			    int,	max_len)
{
    char * dest = contents;
    char * end = contents;
    int cnt = 0;
    int ch = getc(fp);

    while (SPACE == ch || TAB == ch)	/* Skip spaces and tabs */
	ch = getc(fp);

    while (ch != FIELD_SEPARATOR &&
	   (!reading_list || ch != LIST_SEPARATOR) &&
	   ch != CR  &&  ch != LF  &&  ch != EOF  &&  cnt < max_len) {
	*(dest++) = ch;
	cnt++;
	if (ch != SPACE && ch != TAB)
	    end = dest;
	ch = getc(fp);
    } /* while not eol or eof or too many read */

    if (cnt == max_len)	{
	/* If the field was too long (or exactly maximum) ignore the rest */
	while (ch != FIELD_SEPARATOR &&
	       (!reading_list || ch != LIST_SEPARATOR) &&
	       ch != CR  &&  ch != LF  &&  ch != EOF)
	    ch = getc(fp);
    }

    if (ch == CR || ch == LF)
	ungetc(ch, fp);	/* Push back the record separator (NL or LF) */

    /* Terminate the string, truncating trailing whitespace off.
    ** Otherwise (if whitespace would be included), here would
    ** be *dest='\0'; and  cnt -= ... would be left out.
    */
    *end = '\0';
    cnt -= dest-end;

    return ch;		/* Return the terminating character */
}



/* PUBLIC						HTAAFile_readField()
**		READ A FIELD FROM A PASSWORD, GROUP
**		OR ACCESS CONTROL LIST FILE
**		i.e. an item terminated by colon,
**		end-of-line, or end-of-file. 
** ON ENTRY:
**	fp		is the file to read the characters from
**	contents	is the character array to put the characters
**	max_len		is the maximum number of characters that may
**			be read (i.e. the size of dest minus one for
**			terminating null).
** ON EXIT:
**	returns		the terminating character
**			(i.e. either separator or CR or LF or EOF).
**	contents	contains a null-terminated string representing
**			the read field.
** NOTE 1:
**			Ignores leading and trailing blanks and tabs.
** NOTE 2:
**			If the field is more than max_len characters
**			long, the rest of the characters in that item
**			are ignored.  However, contents is always
**			null-terminated!
*/
PUBLIC int HTAAFile_readField ARGS3(FILE *, fp,
				    char *, contents,
				    int,    max_len)
{
    return read_item(fp, contents, NO, max_len);
}




/* PUBLIC						HTAAFile_readList()
**
**			READ A LIST OF STRINGS SEPARATED BY COMMAS
**			(FROM A GROUP OR ACCESS CONTROL LIST FILE)
** ON ENTRY:
**	fp		is a pointer to the input file.
**	result		is the list to which append the read items.
**	max_len		is the maximum number of characters in each
**			list entry (extra characters are ignored).
** ON EXIT:
**	returns		the number of items read.
**
*/
PUBLIC int HTAAFile_readList ARGS3(FILE *,	fp,
				   HTList *,	result,
				   int,		max_len)
{
    char *item = NULL;
    int terminator;
    int cnt = 0;

    do {
	if (!item  &&  !(item = (char*)malloc(max_len+1)))
	    outofmem(__FILE__, "HTAAFile_readList");
	terminator = read_item(fp, item, YES, max_len);
	if (strlen(item) > 0) {
	    cnt++;
	    HTList_addObject(result, (void*)item);
	    item = NULL;
	}
    } while (terminator != FIELD_SEPARATOR  &&
	     terminator != CR  &&  terminator != LF  &&
	     terminator != EOF);

    FREE(item);	/* This was not needed */
    return cnt;
}

