#include	<stdlib.h>
#include	<stdio.h>
#include	<assert.h>
#include	<string.h>

char	*UTS_sprintf_wrap();
char	*do_efmt();
char	*do_gfmt();
char	*Fill();

/* main(argc, argv)
 * char	**argv;
 * {
 * 	double	d;
 * 	char	*Fmt, *Ret;
 * 	char	obuf[200];
 * 
 * 	assert(argc > 2);
 * 	Fmt = argv[1];
 * 	d = strtod(argv[2], (char **)0);
 * 
 * 	putchar('{');
 * 	printf(Fmt, d);
 * 	printf("}\n");
 * 
 * 	Ret = UTS_sprintf_wrap(obuf, Fmt, d);
 * 	assert(Ret == obuf);
 * 
 * 	printf("{%s}\n", obuf);
 * }
 */

char *
UTS_sprintf_wrap(obuf, fmt, d,
	a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15)
char	*obuf, *fmt;
double	d;
{
	int	fmtlen, Width=0, Precision=6, Alt=0, Plus=0, Minus=0,
		Zero = 0;
	int	FmtChar, BaseFmt = 0;
	char	*f = fmt, *AfterWidth = 0, *AfterPrecision = 0;
	char	*Dot;

	if(*f++ != '%') {
		return
sprintf(obuf, fmt, d, a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15);
	}
	fmtlen = strlen(fmt);
	FmtChar = fmt[fmtlen - 1];
	switch(FmtChar) {
	case 'f':
	case 'F':
		return
sprintf(obuf, fmt, d, a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15);
	case 'e':
	case 'E':
		BaseFmt = 'e';
		goto BaseFmt_IsSet;
	case 'g':
	case 'G':
		BaseFmt = 'g';
BaseFmt_IsSet:
		if(*f == '#') { Alt = 1; ++f; }   /* Always has '.' */
		if(*f == '+') { Plus = 1; ++f; }  /* Force explicit sign */
		if(*f == '-') { Minus = 1; ++f; } /* Left justify */
		if(*f == '0') { Zero = 1; ++f;} /* Fill using 0s*/
		if(Dot = strchr(f, '.')) {
			Precision = strtol(Dot+1, &AfterPrecision, 0);
		}
		if(!Dot || (Dot && Dot > f)) { /* Next char=='.' => no width*/
			Width = strtol(f, &AfterWidth, 0);
		}
		if(Dot) { f = AfterPrecision; }
		else if(AfterWidth) { f = AfterWidth; }
		if(*f != FmtChar) goto regular_sprintf;
		 /* It doesn't look like a f.p. sprintf call */
		 /* from Perl_sv_vcatpvfn		     */

		if(BaseFmt == 'e') {
			return do_efmt(d, obuf, Width, Precision, Alt,
				Plus, Minus, Zero, FmtChar == 'E');
		} else {
			return do_gfmt(d, obuf, Width, Precision, Alt,
				Plus, Minus, Zero, FmtChar == 'G');
		}
	default:
regular_sprintf:
		return
sprintf(obuf, fmt, d, a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15);
	}
}

char	*
do_efmt(d, obuf, Width, Precision, Alt, Plus, Minus, Zero, UpperCase)
char	*obuf;
double	d;
{
	char	*Ecvt;
	char	*ob;
	int	decpt, sign, E;
	int	len;
	int	AllZeroes = 0;

	Ecvt = ecvt( d , Precision+1, &decpt, &sign);

	/* fprintf(stderr, "decpt=%d, sign=%d\n", decpt, sign); */

	len = strlen(Ecvt);
	if(strspn(Ecvt, "0") == len) AllZeroes = 1;

	ob = obuf;
	if(sign)	*ob++ = '-';
	else if(Plus)	*ob++ = '+';

	*ob++ = Ecvt[0];

	if(Precision > 0 || Alt) *ob++ = '.';
	strcpy(ob, &Ecvt[1]);

	ob += strlen(ob);	/* ADVANCE TO END OF WHAT WE JUST ADDED */
	*ob++ = UpperCase ? 'E' : 'e';

	if(AllZeroes)	E = 0;
	else		E = decpt - 1;

	if(E < 0)	{ *ob++ = '-'; E = -E; }
	else		{ *ob++ = '+'; }

	sprintf(ob, "%.2d", E);	/* Too much horsepower used here */

	if(Width > strlen(obuf)) return Fill(obuf, Width, Minus, Zero);
	else			 return obuf;
}

char	*
do_gfmt(d, obuf, Width, Precision, Alt, Plus, Minus, Zero, UpperCase)
char	*obuf;
double	d;
{
	char	*Ecvt = gcvt(d, Precision ? Precision : 1, obuf);
	int	len = strlen(obuf);

	 /* gcvt fails (maybe give a warning? For now return empty string): */
	if(!Ecvt) { *obuf = '\0'; return obuf; }

	/* printf("Ecvt='%s'\n", Ecvt); */
	if(Plus && (Ecvt[0] != '-')) {
		memmove(obuf+1, obuf, len+1); /* "+1" to get '\0' at end */
		obuf[0] = '+';
		++len;
	}
	if(Alt && !strchr(Ecvt, '.')) {
		int	LenUpTo_E = strcspn(obuf, "eE");
		int	E_etc_len = strlen(&obuf[LenUpTo_E]);
			/* ABOVE: Will be 0 if there's no E/e because */
			/* strcspn will return length of whole string */

		if(E_etc_len)
			memmove(obuf+LenUpTo_E+1, obuf+LenUpTo_E, E_etc_len);
		obuf[LenUpTo_E] = '.';
		obuf[LenUpTo_E + 1 + E_etc_len ] = '\0';
	}
	{ char *E_loc;
	  if(UpperCase && (E_loc = strchr(obuf, 'e'))) { *E_loc = 'E'; }
	}
	if(Width > len)
		return Fill(obuf, Width, Minus, Zero);
	else
		return obuf;
}

char *
Fill(obuf, Width, LeftJustify, Zero)
char	*obuf;
{
	int	W = strlen(obuf);
	int	diff = Width - W;
	 /* LeftJustify means there was a '-' flag, and in that case,	*/
	 /* printf man page (UTS4.4) says ignore '0'			*/
	char	FillChar = (Zero && !LeftJustify) ? '0' : ' ';
	int	i;
	int	LeftFill = ! LeftJustify;

	if(Width <= W) return obuf;

	if(LeftFill) {
		memmove(obuf+diff, obuf, W+1); /* "+1" to get '\0' at end */
		for(i=0 ; i < diff ; ++i) { obuf[i] = FillChar; }
	} else {
		for(i=W ; i < Width ; ++i)
			obuf[i] = FillChar;
		obuf[Width] = '\0';
	}
	return obuf;
}
