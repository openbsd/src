/*                                             Configuration Manager for libwww
 *                            CONFIGURATION MANAGER
 *
 * Author Tim Berners-Lee/CERN.  Public domain.  Please mail changes to
 * timbl@info.cern.ch.
 *
 * The configuration information loaded includes tables (file suffixes,
 * presentation methods) in other modules.  The most likely routines needed by
 * developers will be:
 *
 * HTSetConfiguration	to load configuration information.
 *
 * HTLoadRules		to load a whole file of configuration information
 *
 * HTTranslate		to translate a URL using the rule table.
 *
 */
#ifndef HTRULE_H
#define HTRULE_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif
 
typedef enum {
        HT_Invalid,
        HT_Map,
        HT_Pass,
        HT_Fail,
        HT_DefProt,
        HT_Protect,
        HT_Progress,
        HT_InfoMsg,
        HT_UserMsg,
        HT_Alert,
        HT_AlwaysAlert,
        HT_Redirect,
        HT_RedirectPerm,
        HT_PermitRedir,
        HT_UseProxy
} HTRuleOp;

#ifndef NO_RULES

/*

Server Side Script Execution

   If a URL starts with /htbin/ it is understood to mean a script execution request on
   server.  This feature needs to be turned on by setting HTBinDir by the htbin rule.
   Index searching is enabled by setting HTSearchScript into the name of script in BinDir
   doing the actual search by search rule (BinDir must also be set in this case, of
   course).

 */

extern char * HTBinDir;         /* Physical /htbin location */
extern char * HTSearchScript;   /* Search script name */

/*

HTAddRule:  Add rule to the list

  ON ENTRY,

  pattern                points to 0-terminated string containing a single "*"

  equiv                  points to the equivalent string with * for the place where the
                         text matched by * goes; or to other 2nd parameter
                         meaning depends on op).			 

  cond_op,               additional condition for applying rule; cond_op should
  cond                   be either NULL (no additional condition), or one of
                         the strings "if" or "unless"; if cond_op is not NULL,
                         cond should point to a recognized condition keyword
                         (as a string) such as "userspec", "redirected".

  ON EXIT,

  returns                0 if success, -1 if error.

   Note that if BYTE_ADDRESSING is set, the three blocks required are allocated and
   deallocated as one.  This will save time and storage, when malloc's allocation units are
   large.

 */
extern int HTAddRule PARAMS((
    HTRuleOp op,
    CONST char * pattern,
    CONST char * equiv,
    CONST char * cond_op,
    CONST char * cond));


/*

HTClearRules: Clear all rules

  ON EXIT,

  Rule file               There are no rules

 */

extern void HTClearRules PARAMS((void));

/*

HTTranslate: Translate by rules

 */

/*

  ON ENTRY,

  required                points to a string whose equivalent value is neeed

  ON EXIT,

  returns                 the address of the equivalent string allocated from the heap
                         which the CALLER MUST FREE. If no translation occured, then it is
                         a copy of the original.

 */
extern char * HTTranslate PARAMS((CONST char * required));

/*

HTSetConfiguration:  Load one line of configuration information

  ON ENTRY,

  config                  is a string in the syntax of a rule file line.

   This routine may be used for loading configuration information from sources other than
   the  rule file, for example INI files for X resources.

 */
extern int HTSetConfiguration PARAMS((char * config));


/*

HtLoadRules:  Load the rules from a file

  ON ENTRY,

  Rule table              Rules can be in any state

  ON EXIT,

  Rule table              Any existing rules will have been kept.  Any new rules will have
                         been loaded on top, so as to be tried first.

  Returns                 0 if no error.

 */

extern int HTLoadRules PARAMS((CONST char * filename));
/*

 */

#endif /* NO_RULES */
#endif /* HTRULE_H */
