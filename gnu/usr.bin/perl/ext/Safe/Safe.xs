#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/* maxo should never differ from MAXO but leave some room anyway */
#define OP_MASK_BUF_SIZE (MAXO + 100)

MODULE = Safe	PACKAGE = Safe

void
safe_call_sv(package, mask, codesv)
	char *	package
	SV *	mask
	SV *	codesv
    CODE:
	int i;
	char *str;
	STRLEN len;
	char op_mask_buf[OP_MASK_BUF_SIZE];

	assert(maxo < OP_MASK_BUF_SIZE);
	ENTER;
	SAVETMPS;
	save_hptr(&defstash);
	save_aptr(&endav);
	SAVEPPTR(op_mask);
	op_mask = &op_mask_buf[0];
	str = SvPV(mask, len);
	if (maxo != len)
	    croak("Bad mask length");
	for (i = 0; i < maxo; i++)
	    op_mask[i] = str[i];
	defstash = gv_stashpv(package, TRUE);
	endav = (AV*)sv_2mortal((SV*)newAV()); /* Ignore END blocks for now */
	GvHV(gv_fetchpv("main::", TRUE, SVt_PVHV)) = defstash;
	PUSHMARK(sp);
	i = perl_call_sv(codesv, G_SCALAR|G_EVAL|G_KEEPERR);
	SPAGAIN;
	ST(0) = i ? newSVsv(POPs) : &sv_undef;
	PUTBACK;
	FREETMPS;
	LEAVE;
	sv_2mortal(ST(0));

void
op_mask()
    CODE:
	ST(0) = sv_newmortal();
	if (op_mask)
	    sv_setpvn(ST(0), op_mask, maxo);

void
mask_to_ops(mask)
	SV *	mask
    PPCODE:
	STRLEN len;
	char *maskstr = SvPV(mask, len);
	int i;
	if (maxo != len)
	    croak("Bad mask length");
	for (i = 0; i < maxo; i++)
	    if (maskstr[i])
		XPUSHs(sv_2mortal(newSVpv(op_name[i], 0)));

void
ops_to_mask(...)
    CODE:
	int i, j;
	char mask[OP_MASK_BUF_SIZE], *op;
	Zero(mask, sizeof mask, char);
	for (i = 0; i < items; i++)
	{
	    op = SvPV(ST(i), na);
	    for (j = 0; j < maxo && strNE(op, op_name[j]); j++) /* nothing */ ;
	    if (j < maxo)
		mask[j] = 1;
	    else
	    {
		Safefree(mask);
		croak("bad op name \"%s\" in mask", op);
	    }
	}
	ST(0) = sv_2mortal(newSVpv(mask,maxo));

void
opname(...)
    PPCODE:
	int i, myopcode;
	for (i = 0; i < items; i++)
	{
	    myopcode = SvIV(ST(i));
	    if (myopcode < 0 || myopcode >= maxo)
		croak("opcode out of range");
	    XPUSHs(sv_2mortal(newSVpv(op_name[myopcode], 0)));
	}

void
opdesc(...)
    PPCODE:
	int i, myopcode;
	for (i = 0; i < items; i++)
	{
	    myopcode = SvIV(ST(i));
	    if (myopcode < 0 || myopcode >= maxo)
		croak("opcode out of range");
	    XPUSHs(sv_2mortal(newSVpv(op_desc[myopcode], 0)));
	}

void
opcode(...)
    PPCODE:
	int i, j;
	char *op;
	for (i = 0; i < items; i++)
	{
	    op = SvPV(ST(i), na);
	    for (j = 0; j < maxo; j++) {
		if (strEQ(op, op_name[j]) || strEQ(op, op_desc[j]))
		    break;
	    }
	    if (j == maxo)
		croak("bad op name \"%s\"", op);
	    XPUSHs(sv_2mortal(newSViv(j)));
	}

int
MAXO()
    CODE:
	RETVAL = maxo;
    OUTPUT:
	RETVAL
