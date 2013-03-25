MODULE = XS::APItest		PACKAGE = XS::APItest::numeric

void
grok_number(number)
	SV *number
    PREINIT:
	STRLEN len;
	const char *pv = SvPV(number, len);
	UV value;
	int result;
    PPCODE:
	EXTEND(SP,2);
	result = grok_number(pv, len, &value);
	PUSHs(sv_2mortal(newSViv(result)));
	if (result & IS_NUMBER_IN_UV)
	    PUSHs(sv_2mortal(newSVuv(value)));
