#ifndef LYCHARSETS_H
#define LYCHARSETS_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#include <UCDefs.h>

#ifndef UCMAP_H
#include <UCMap.h>
#endif /* !UCMAP_H */

extern BOOLEAN LYHaveCJKCharacterSet;
extern BOOLEAN DisplayCharsetMatchLocale;

/*
 *  currently active character set (internal handler)
 */
extern int current_char_set;

/*
 *  Initializer, calls initialization function for the
 *  CHARTRANS handling. - KW
 */
extern int LYCharSetsDeclared NOPARAMS;


extern CONST char ** LYCharSets[];
extern CONST char * SevenBitApproximations[];
extern CONST char ** p_entity_values;
extern CONST char * LYchar_set_names[];  /* Full name, not MIME */
extern int LYlowest_eightbit[];
extern int LYNumCharsets;
extern LYUCcharset LYCharSet_UC[];
extern int UCGetLYhndl_byAnyName PARAMS((char *value));
extern void HTMLSetCharacterHandling PARAMS((int i));
extern void HTMLSetUseDefaultRawMode PARAMS((int i, BOOLEAN modeflag));
extern void HTMLUseCharacterSet PARAMS((int i));
extern UCode_t HTMLGetEntityUCValue PARAMS((CONST char *name));
extern void Set_HTCJK PARAMS((CONST char *inMIMEname, CONST char *outMIMEname));

extern CONST char * HTMLGetEntityName PARAMS((UCode_t code));
		/*
		** HTMLGetEntityName calls LYEntityNames for iso-8859-1 entity
		** names only.	This is an obsolete technique but widely used in
		** the code.  Note that unicode number in general may have
		** several equivalent entity names because of synonyms.
		*/


extern BOOL force_old_UCLYhndl_on_reload;
extern int forced_UCLYhdnl;
#endif /* LYCHARSETS_H */
