/* Copyright (c) 2004-2005 Nokia. All rights reserved. */
 
/* The PerlUtil class is licensed under the same terms as Perl itself. */

/* See PerlUtil.pod for documentation. */

#define PERLUTIL_CPP

#include "PerlUtil.h"

#include <utf.h>

#undef Copy
#undef New

EXPORT_C SV* PerlUtil::newSvPVfromTDesC8(const TDesC8& aDesC8) {
  dTHX;
  return newSVpvn((const char *)aDesC8.Ptr(), aDesC8.Size());
}

EXPORT_C void PerlUtil::setSvPVfromTDesC8(SV* sv, const TDesC8& aDesC8) {
  dTHX;
  sv_setpvn(sv, (const char *)aDesC8.Ptr(), aDesC8.Size());
}

EXPORT_C HBufC8* PerlUtil::newHBufC8fromSvPV(SV* sv) {
  dTHX;
  return PerlUtil::newHBufC8fromPVn((const U8 *)SvPV_nolen(sv), SvLEN(sv));
}

EXPORT_C void PerlUtil::setTDes8fromSvPV(TDes8& aDes8, SV* sv) {
  dTHX;
  PerlUtil::setTDes8fromPVn(aDes8, (const U8 *)SvPV_nolen(sv), SvLEN(sv));
}

EXPORT_C SV* PerlUtil::newSvPVfromTDesC16(const TDesC16& aDesC16) {
  dTHX;
  SV* sv = NULL;
  HBufC8* hBuf8 = HBufC8::New(aDesC16.Length() * 3);

  if (hBuf8) {
    TPtr8 aPtr8(hBuf8->Des());
    CnvUtfConverter::ConvertFromUnicodeToUtf8(aPtr8, aDesC16);
    sv = newSVpvn((const char *)(hBuf8->Ptr()), hBuf8->Size());
    delete hBuf8;
    hBuf8 = NULL;
    SvUTF8_on(sv);
  }

  return sv;
}

EXPORT_C void PerlUtil::setSvPVfromTDesC16(SV* sv, const TDesC16& aDesC16) {
  dTHX;
  HBufC8* hBuf8 = HBufC8::New(aDesC16.Length() * 3);

  if (hBuf8) {
    TPtr8 aPtr8(hBuf8->Des());
    CnvUtfConverter::ConvertFromUnicodeToUtf8(aPtr8, aDesC16);
    sv_setpvn(sv, (const char *)(hBuf8->Ptr()), hBuf8->Size());
    delete hBuf8;
    hBuf8 = NULL;
    SvUTF8_on(sv);
  }
}

EXPORT_C HBufC16* PerlUtil::newHBufC16fromSvPV(SV* sv) {
  dTHX;
  return PerlUtil::newHBufC16fromPVn((const U8 *)SvPV_nolen(sv), SvLEN(sv), SvUTF8(sv));
}

void PerlUtil::setTDes16fromSvPV(TDes16& aDes16, SV* sv) {
  dTHX;
  PerlUtil::setTDes16fromPVn(aDes16, (const U8 *)SvPV_nolen(sv), SvLEN(sv), SvUTF8(sv));
}

EXPORT_C HBufC8* PerlUtil::newHBufC8fromPVn(const U8* s, STRLEN n) {
  HBufC8* aBuf8 = HBufC8::New(n);
  if (aBuf8) {
    TPtr8 ptr8 = aBuf8->Des();
    ptr8.Copy(s, n);
  }
  return aBuf8;
}

EXPORT_C void PerlUtil::setTDes8fromPVn(TDes8& aDes8, const U8* s, STRLEN n) {
  // TODO: grow aDes8 if needed
  aDes8.Copy(s, n);
}

EXPORT_C HBufC16* PerlUtil::newHBufC16fromPVn(const U8 *s, STRLEN n, bool utf8) {
  HBufC16 *hBuf16 = HBufC16::New(n); // overallocate

  if (hBuf16) {
    if (utf8) {
      TPtr16 aPtr16(hBuf16->Des());
      TPtrC8 aPtrC8(s, n);
      CnvUtfConverter::ConvertToUnicodeFromUtf8(aPtr16, aPtrC8);
    } else {
      TPtrC8 aPtrC8(s, n);
      hBuf16->Des().Copy(aPtrC8);
    }
  }

  return hBuf16;
}

EXPORT_C void PerlUtil::setTDes16fromPVn(TDes16& aDes16, const U8 *s, STRLEN n, bool utf8) {
  // TODO: grow aDes16 if needed
  if (utf8) {
    TPtrC8 aPtrC8(s, n);
    CnvUtfConverter::ConvertToUnicodeFromUtf8(aDes16, aPtrC8);
  } else {
    TPtrC8 aPtrC8(s, n);
    aDes16.Copy(aPtrC8);
  }
}
