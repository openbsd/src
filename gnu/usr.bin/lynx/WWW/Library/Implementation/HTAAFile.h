/*                       FILE ROUTINES FOR ACCESS AUTHORIZATION PACKAGE
                                             
   This module implements the routines used for accessing (and parsing) the files used in
   the access authorization:
   
      password file
      
      group file
      
      access control list (ACL) file
      
 */


#ifndef HTAAFILE_H
#define HTAAFILE_H

#ifndef HTUTILS_H
#include "HTUtils.h"            /* BOOL, PARAMS, ARGS */
#endif /* HTUTILS_H */
/*#include <stdio.h> included by HTUtils.h -- FM *//* FILE */
#include "HTList.h"             /* HTList */

#ifdef SHORT_NAMES
#define HTAAFnRe        HTAAFile_nextRec
#define HTAAFrFi        HTAAFile_readField
#define HTAAFrLi        HTAAFile_readList
#endif /*SHORT_NAMES*/


/* Used field separators */

#define FIELD_SEPARATOR ':'     /* Used to separate fields              */
#define LIST_SEPARATOR  ','     /* Used to separate items in a list     */
                                /* in group and ALC files.              */

/*

Naming conventions

  Record                 is an entire line in file.
                         
  Field                  is an entity separated by colons and/or by end-of-line.
                         
  List                   is a field in which there are items separated by commas.
                         
Record-oriented Read Routines

   Password, group and ACL are internally read in by the following functions:
   
  HTAAFile_nextRec()      skips to the beginning of the next record (must be called even
                         after the last field of a record is read to proceed to the next
                         record).
                         
  HTAAFile_readField()    reads a field (separated by colons).
                         
  HTAAFile_readList()     reads a field containing a comma-separated list of items.
                         
 */

/* PUBLIC                                               HTAAFile_nextRec()
**                      GO TO THE BEGINNING OF THE NEXT RECORD
** ON ENTRY:
**      fp      is the file from which records are read from.
**
** ON EXIT:
**      returns nothing. File read pointer is located at the beginning
**              of the next record.
**
*/
PUBLIC void HTAAFile_nextRec PARAMS((FILE * fp));


/* PUBLIC                                               HTAAFile_readField()
**              READ A FIELD FROM A PASSWORD, GROUP
**              OR ACCESS CONTROL LIST FILE
**              i.e. an item terminated by colon,
**              end-of-line, or end-of-file.
** ON ENTRY:
**      fp              is the file to read the characters from
**      contents        is the character array to put the characters
**      max_len         is the maximum number of characters that may
**                      be read (i.e. the size of dest minus one for
**                      terminating null).
** ON EXIT:
**      returns         the terminating character
**                      (i.e. either separator or CR or LF or EOF).
**      contents        contains a null-terminated string representing
**                      the read field.
** NOTE 1:
**                      Ignores leading and trailing blanks and tabs.
** NOTE 2:
**                      If the field is more than max_len characters
**                      long, the rest of the characters in that item
**                      are ignored.  However, contents is always
**                      null-terminated!
*/
PUBLIC int HTAAFile_readField PARAMS((FILE * fp,
                                      char * contents,
                                      int    max_len));


/* PUBLIC                                               HTAAFile_readList()
**
**                      READ A LIST OF STRINGS SEPARATED BY COMMAS
**                      (FROM A GROUP OR ACCESS CONTROL LIST FILE)
** ON ENTRY:
**      fp              is a pointer to the input file.
**      result          is the list to which append the read items.
**      max_len         is the maximum number of characters in each
**                      list entry (extra characters are ignored).
** ON EXIT:
**      returns         the number of items read.
**
*/
PUBLIC int HTAAFile_readList PARAMS((FILE *     fp,
                                     HTList *   result,
                                     int        max_len));
/*

 */

#endif /* not HTAAFILE_H */
/*

   End of file HTAAFile.h.  */
