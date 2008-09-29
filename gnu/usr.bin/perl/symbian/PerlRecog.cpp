/* Copyright (c) 2004-2005 Nokia. All rights reserved. */
 
/* The PerlRecog application is licensed under the same terms as Perl itself. */

#include <apmrec.h>
#include <apmstd.h>
#include <f32file.h>

const TUid KUidPerlRecog = { 0x102015F7 };
_LIT8(KPerlMimeType, "x-application/x-perl");
_LIT8(KPerlSig, "#!/usr/bin/perl");
const TInt KPerlSigLen = 15;

class CApaPerlRecognizer : public CApaDataRecognizerType {
  public:
    CApaPerlRecognizer():CApaDataRecognizerType(KUidPerlRecog, EHigh) {
        iCountDataTypes = 1;
    }
    virtual TUint PreferredBufSize() { return KPerlSigLen; }
    virtual TDataType SupportedDataTypeL(TInt /* aIndex */) const {
        return TDataType(KPerlMimeType);
    }
  private:
    virtual void DoRecognizeL(const TDesC& aName, const TDesC8& aBuffer);
};

void CApaPerlRecognizer::DoRecognizeL(const TDesC& aName, const TDesC8& aBuffer)
{
    iConfidence = ENotRecognized;

    if (aBuffer.Length() >= KPerlSigLen &&
        aBuffer.Left(KPerlSigLen).Compare(KPerlSig) == 0) {
        iConfidence = ECertain;
        iDataType   = TDataType(KPerlMimeType);
    } else {
        TParsePtrC p(aName);

        if ((p.Ext().CompareF(_L(".pl"))  == 0) ||
            (p.Ext().CompareF(_L(".pm"))  == 0)) {
            iConfidence = ECertain;
            iDataType = TDataType(KPerlMimeType);
        }
    }
}

EXPORT_C CApaDataRecognizerType* CreateRecognizer()
{
    return new CApaPerlRecognizer;
}

GLDEF_C TInt E32Dll(TDllReason /* aReason */)
{
    return KErrNone;
}


    
