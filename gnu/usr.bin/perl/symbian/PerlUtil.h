/* Copyright (c) 2004-2005 Nokia. All rights reserved. */

/* The PerlUtil class is licensed under the same terms as Perl itself. */

/* See PerlUtil.pod for documentation. */

#ifndef __PerlUtil_h__
#define __PerlUtil_h__

#include <e32base.h>

#include "EXTERN.h"
#include "perl.h"

class PerlUtil {
 public:

  IMPORT_C static SV*       newSvPVfromTDesC8(const TDesC8& aDes);
  IMPORT_C static void      setSvPVfromTDesC8(SV* sv, const TDesC8& aDes);
  IMPORT_C static HBufC8*   newHBufC8fromSvPV(SV* sv);
  IMPORT_C static void      setTDes8fromSvPV(TDes8& aDes8, SV* sv);

  IMPORT_C static SV*       newSvPVfromTDesC16(const TDesC16& aDes);
  IMPORT_C static void      setSvPVfromTDesC16(SV* sv, const TDesC16& aDes);
  IMPORT_C static HBufC16*  newHBufC16fromSvPV(SV* sv);
  IMPORT_C static void      setTDes16fromSvPV(TDes16& aDes16, SV* sv);

  IMPORT_C static HBufC8*   newHBufC8fromPVn(const U8* s, STRLEN n);
  IMPORT_C static void      setTDes8fromPVn(TDes8& aDes8, const U8* s, STRLEN n);
  IMPORT_C static HBufC16*  newHBufC16fromPVn(const U8* s, STRLEN n, bool utf8);
  IMPORT_C static void      setTDes16fromPVn(TDes16& aDes16, const U8* s, STRLEN n, bool utf8);
};

#endif /* #ifndef __PerlUtil_h__ */


