#include <eikenv.h>
#include <e32std.h>

#include "etelbgsm.h" // From Symbian 6.1 SDK (the Communicator SDK)

#ifdef __cplusplus
extern "C" {
#endif
#include "PerlBase.h"
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef __cplusplus
}
#endif

_LIT(KTsyName, "phonetsy.tsy");

#define XS_SYMBIAN_OR_EMPTY(e, c) errno = (e) = (c); if ((e) != KErrNone) XSRETURN_EMPTY

MODULE = Moped::Msg	PACKAGE = Moped::Msg

PROTOTYPES: ENABLE

extern "C" void
get_gsm_network_info()
    PREINIT:
	TInt			error;
	TInt			enumphone;
	RTelServer		server;
	RBasicGsmPhone		phone;
	RTelServer::TPhoneInfo	info;
	MBasicGsmPhoneNetwork::TCurrentNetworkInfo networkinfo;
    PPCODE:
	if (GIMME != G_ARRAY)
	    XSRETURN_UNDEF;
	XS_SYMBIAN_OR_EMPTY(error, server.Connect());
	XS_SYMBIAN_OR_EMPTY(error, server.LoadPhoneModule(KTsyName));
	XS_SYMBIAN_OR_EMPTY(error, server.EnumeratePhones(enumphone));
	if (enumphone < 1)
	    XSRETURN_EMPTY;
	XS_SYMBIAN_OR_EMPTY(error, server.GetPhoneInfo(0, info));
	XS_SYMBIAN_OR_EMPTY(error, phone.Open(server, info.iName));
	XS_SYMBIAN_OR_EMPTY(error, phone.GetCurrentNetworkInfo(networkinfo));
	EXTEND(SP, 4);
	PUSHs(sv_2mortal(newSViv(networkinfo.iNetworkInfo.iId.iMCC)));
	PUSHs(sv_2mortal(newSViv(networkinfo.iNetworkInfo.iId.iMNC)));
	PUSHs(sv_2mortal(newSViv(networkinfo.iLocationAreaCode)));
	PUSHs(sv_2mortal(newSViv(networkinfo.iCellId)));


