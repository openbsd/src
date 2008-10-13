#! /bin/sed -nf
#
#  dc.sed - an arbitrary precision RPN calculator
#  Created by Greg Ubben <gsu@romulus.ncsc.mil> early 1995, late 1996
#
#  Dedicated to MAC's memory of the IBM 1620 ("CADET") computer.
#  @(#)GSU dc.sed 1.1 06-Mar-1999 [non-explanatory]
#  $OpenBSD: dc.sed,v 1.1 2008/10/13 13:22:10 millert Exp $
#
#  From http://sed.sourceforge.net/grabbag/scripts/
#  Public Domain
#
#  Examples:
#	sqrt(2) to 10 digits:	echo "10k 2vp" | dc.sed
#	20 factorial:		echo "[d1-d1<!*]s! 20l!xp" | dc.sed
#	sin(ln(7)):		echo "s(l(7))" | bc -c /usr/lib/lib.b | dc.sed
#	hex to base 60:		echo "60o16i 6B407.CAFE p" | dc.sed
#	tests most of dc.sed:	echo 16oAk2vp | dc.sed
#
#  To debug or analyze, give the dc Y command as input or add it to
#  embedded dc routines, or add the sed p command to the beginning of
#  the main loop or at various points in the low-level sed routines.
#  If you need to allow [|~] characters in the input, filter this
#  script through "tr '|~' '\36\37'" first (or use dc.pl).
#
#  Not implemented:	! \
#  But implemented:	K Y t # !< !> != fractional-bases
#  SunOS limits:	199/199 commands (though could pack in 10-20 more)
#  Limitations:		scale <= 999; |obase| >= 1; input digits in [0..F]
#  Completed:		1am Feb 4, 1997

s/^/|P|K0|I10|O10|?~/

:next
s/|?./|?/
s/|?#[	 -}]*/|?/
/|?!*[lLsS;:<>=]\{0,1\}$/N
/|?!*[-+*/%^<>=]/b binop
/^|.*|?[dpPfQXZvxkiosStT;:]/b binop
/|?[_0-9A-F.]/b number
/|?\[/b string
/|?l/b load
/|?L/b Load
/|?[sS]/b save
/|?c/ s/[^|]*//
/|?d/ s/[^~]*~/&&/
/|?f/ s//&[pSbz0<aLb]dSaxsaLa/
/|?x/ s/\([^~]*~\)\(.*|?x\)~*/\2\1/
/|?[KIO]/ s/.*|\([KIO]\)\([^|]*\).*|?\1/\2~&/
/|?T/ s/\.*0*~/~/
#  a slow, non-stackable array implementation in dc, just for completeness
#  A fast, stackable, associative array implementation could be done in sed
#  (format: {key}value{key}value...), but would be longer, like load & save.
/|?;/ s/|?;\([^{}]\)/|?~[s}s{L{s}q]S}[S}l\1L}1-d0>}s\1L\1l{xS\1]dS{xL}/
/|?:/ s/|?:\([^{}]\)/|?~[s}L{s}L{s}L}s\1q]S}S}S{[L}1-d0>}S}l\1s\1L\1l{xS\1]dS{x/
/|?[ ~	cdfxKIOT]/b next
/|?\n/b next
/|?[pP]/b print
/|?k/ s/^\([0-9]\{1,3\}\)\([.~].*|K\)[^|]*/\2\1/
/|?i/ s/^\(-\{0,1\}[0-9]*\.\{0,1\}[0-9]\{1,\}\)\(~.*|I\)[^|]*/\2\1/
/|?o/ s/^\(-\{0,1\}[1-9][0-9]*\.\{0,1\}[0-9]*\)\(~.*|O\)[^|]*/\2\1/
/|?[kio]/b pop
/|?t/b trunc
/|??/b input
/|?Q/b break
/|?q/b quit
h
/|?[XZz]/b count
/|?v/b sqrt
s/.*|?\([^Y]\).*/\1 is unimplemented/
s/\n/\\n/g
l
g
b next

:print
/^-\{0,1\}[0-9]*\.\{0,1\}[0-9]\{1,\}~.*|?p/!b Print
/|O10|/b Print

#  Print a number in a non-decimal output base.  Uses registers a,b,c,d.
#  Handles fractional output bases (O<-1 or O>=1), unlike other dc's.
#  Converts the fraction correctly on negative output bases, unlike
#  UNIX dc.  Also scales the fraction more accurately than UNIX dc.
#
s,|?p,&KSa0kd[[-]Psa0la-]Sad0>a[0P]sad0=a[A*2+]saOtd0>a1-ZSd[[[[ ]P]sclb1\
!=cSbLdlbtZ[[[-]P0lb-sb]sclb0>c1+]sclb0!<c[0P1+dld>c]scdld>cscSdLbP]q]Sb\
[t[1P1-d0<c]scd0<c]ScO_1>bO1!<cO[16]<bOX0<b[[q]sc[dSbdA>c[A]sbdA=c[B]sbd\
B=c[C]sbdC=c[D]sbdD=c[E]sbdE=c[F]sb]xscLbP]~Sd[dtdZOZ+k1O/Tdsb[.5]*[.1]O\
X^*dZkdXK-1+ktsc0kdSb-[Lbdlb*lc+tdSbO*-lb0!=aldx]dsaxLbsb]sad1!>a[[.]POX\
+sb1[SbO*dtdldx-LbO*dZlb!<a]dsax]sadXd0<asbsasaLasbLbscLcsdLdsdLdLak[]pP,
b next

:Print
/|?p/s/[^~]*/&\
~&/
s/\(.*|P\)\([^|]*\)/\
\2\1/
s/\([^~]*\)\n\([^~]*\)\(.*|P\)/\1\3\2/
h
s/~.*//
/./{ s/.//; p; }
#  Just s/.//p would work if we knew we were running under the -n option.
#  Using l vs p would kind of do \ continuations, but would break strings.
g

:pop
s/[^~]*~//
b next

:load
s/\(.*|?.\)\(.\)/\20~\1/
s/^\(.\)0\(.*|r\1\([^~|]*\)~\)/\1\3\2/
s/.//
b next

:Load
s/\(.*|?.\)\(.\)/\2\1/
s/^\(.\)\(.*|r\1\)\([^~|]*~\)/|\3\2/
/^|/!i\
register empty
s/.//
b next

:save
s/\(.*|?.\)\(.\)/\2\1/
/^\(.\).*|r\1/ !s/\(.\).*|/&r\1|/
/|?S/ s/\(.\).*|r\1/&~/
s/\(.\)\([^~]*~\)\(.*|r\1\)[^~|]*~\{0,1\}/\3\2/
b next

:quit
t quit
s/|?[^~]*~[^~]*~/|?q/
t next
#  Really should be using the -n option to avoid printing a final newline.
s/.*|P\([^|]*\).*/\1/
q

:break
s/[0-9]*/&;987654321009;/
:break1
s/^\([^;]*\)\([1-9]\)\(0*\)\([^1]*\2\(.\)[^;]*\3\(9*\).*|?.\)[^~]*~/\1\5\6\4/
t break1
b pop

:input
N
s/|??\(.*\)\(\n.*\)/|?\2~\1/
b next

:count
/|?Z/ s/~.*//
/^-\{0,1\}[0-9]*\.\{0,1\}[0-9]\{1,\}$/ s/[-.0]*\([^.]*\)\.*/\1/
/|?X/ s/-*[0-9A-F]*\.*\([0-9A-F]*\).*/\1/
s/|.*//
/~/ s/[^~]//g

s/./a/g
:count1
	s/a\{10\}/b/g
	s/b*a*/&a9876543210;/
	s/a.\{9\}\(.\).*;/\1/
	y/b/a/
/a/b count1
G
/|?z/ s/\n/&~/
s/\n[^~]*//
b next

:trunc
#  for efficiency, doesn't pad with 0s, so 10k 2 5/ returns just .40
#  The X* here and in a couple other places works around a SunOS 4.x sed bug.
s/\([^.~]*\.*\)\(.*|K\([^|]*\)\)/\3;9876543210009909:\1,\2/
:trunc1
	s/^\([^;]*\)\([1-9]\)\(0*\)\([^1]*\2\(.\)[^:]*X*\3\(9*\)[^,]*\),\([0-9]\)/\1\5\6\4\7,/
t trunc1
s/[^:]*:\([^,]*\)[^~]*/\1/
b normal

:number
s/\(.*|?\)\(_\{0,1\}[0-9A-F]*\.\{0,1\}[0-9A-F]*\)/\2~\1~/
s/^_/-/
/^[^A-F~]*~.*|I10|/b normal
/^[-0.]*~/b normal
s:\([^.~]*\)\.*\([^~]*\):[Ilb^lbk/,\1\2~0A1B2C3D4E5F1=11223344556677889900;.\2:
:digit
    s/^\([^,]*\),\(-*\)\([0-F]\)\([^;]*\(.\)\3[^1;]*\(1*\)\)/I*+\1\2\6\5~,\2\4/
t digit
s:...\([^/]*.\)\([^,]*\)[^.]*\(.*|?.\):\2\3KSb[99]k\1]SaSaXSbLalb0<aLakLbktLbk:
b next

:string
/|?[^]]*$/N
s/\(|?[^]]*\)\[\([^]]*\)]/\1|{\2|}/
/|?\[/b string
s/\(.*|?\)|{\(.*\)|}/\2~\1[/
s/|{/[/g
s/|}/]/g
b next

:binop
/^[^~|]*~[^|]/ !i\
stack empty
//!b next
/^-\{0,1\}[0-9]*\.\{0,1\}[0-9]\{1,\}~/ !s/[^~]*\(.*|?!*[^!=<>]\)/0\1/
/^[^~]*~-\{0,1\}[0-9]*\.\{0,1\}[0-9]\{1,\}~/ !s/~[^~]*\(.*|?!*[^!=<>]\)/~0\1/
h
/|?\*/b mul
/|?\//b div
/|?%/b rem
/|?^/b exp

/|?[+-]/ s/^\(-*\)\([^~]*~\)\(-*\)\([^~]*~\).*|?\(-\{0,1\}\).*/\2\4s\3o\1\3\5/
s/\([^.~]*\)\([^~]*~[^.~]*\)\(.*\)/<\1,\2,\3|=-~.0,123456789<></
/^<\([^,]*,[^~]*\)\.*0*~\1\.*0*~/ s/</=/
:cmp1
	s/^\(<[^,]*\)\([0-9]\),\([^,]*\)\([0-9]\),/\1,\2\3,\4/
t cmp1
/^<\([^~]*\)\([^~]\)[^~]*~\1\(.\).*|=.*\3.*\2/ s/</>/
/|?/{
	s/^\([<>]\)\(-[^~]*~-.*\1\)\(.\)/\3\2/
	s/^\(.\)\(.*|?!*\)\1/\2!\1/
	s/|?![^!]\(.\)/&l\1x/
	s/[^~]*~[^~]*~\(.*|?\)!*.\(.*\)|=.*/\1\2/
	b next
}
s/\(-*\)\1|=.*/;9876543210;9876543210/
/o-/ s/;9876543210/;0123456789/
s/^>\([^~]*~\)\([^~]*~\)s\(-*\)\(-*o\3\(-*\)\)/>\2\1s\5\4/

s/,\([0-9]*\)\.*\([^,]*\),\([0-9]*\)\.*\([0-9]*\)/\1,\2\3.,\4;0/
:right1
	s/,\([0-9]\)\([^,]*\),;*\([0-9]\)\([0-9]*\);*0*/\1,\2\3,\4;0/
t right1
s/.\([^,]*\),~\(.*\);0~s\(-*\)o-*/\1~\30\2~/

:addsub1
	s/\(.\{0,1\}\)\(~[^,]*\)\([0-9]\)\(\.*\),\([^;]*\)\(;\([^;]*\(\3[^;]*\)\).*X*\1\(.*\)\)/\2,\4\5\9\8\7\6/
	s/,\([^~]*~\).\{10\}\(.\)[^;]\{0,9\}\([^;]\{0,1\}\)[^;]*/,\2\1\3/
#	  could be done in one s/// if we could have >9 back-refs...
/^~.*~;/!b addsub1

:endbin
s/.\([^,]*\),\([0-9.]*\).*/\1\2/
G
s/\n[^~]*~[^~]*//

:normal
s/^\(-*\)0*\([0-9.]*[0-9]\)[^~]*/\1\2/
s/^[^1-9~]*~/0~/
b next

:mul
s/\(-*\)\([0-9]*\)\.*\([0-9]*\)~\(-*\)\([0-9]*\)\.*\([0-9]*\).*|K\([^|]*\).*/\1\4\2\5.!\3\6,|\2<\3~\5>\6:\7;9876543210009909/

:mul1
    s/![0-9]\([^<]*\)<\([0-9]\{0,1\}\)\([^>]*\)>\([0-9]\{0,1\}\)/0!\1\2<\3\4>/
    /![0-9]/ s/\(:[^;]*\)\([1-9]\)\(0*\)\([^0]*\2\(.\).*X*\3\(9*\)\)/\1\5\6\4/
/<~[^>]*>:0*;/!t mul1

s/\(-*\)\1\([^>]*\).*/;\2^>:9876543210aaaaaaaaa/

:mul2
    s/\([0-9]~*\)^/^\1/
    s/<\([0-9]*\)\(.*[~^]\)\([0-9]*\)>/\1<\2>\3/

    :mul3
	s/>\([0-9]\)\(.*\1.\{9\}\(a*\)\)/\1>\2;9\38\37\36\35\34\33\32\31\30/
	s/\(;[^<]*\)\([0-9]\)<\([^;]*\).*\2[0-9]*\(.*\)/\4\1<\2\3/
	s/a[0-9]/a/g
	s/a\{10\}/b/g
	s/b\{10\}/c/g
    /|0*[1-9][^>]*>0*[1-9]/b mul3

    s/;/a9876543210;/
    s/a.\{9\}\(.\)[^;]*\([^,]*\)[0-9]\([.!]*\),/\2,\1\3/
    y/cb/ba/
/|<^/!b mul2
b endbin

:div
#  CDDET
/^[-.0]*[1-9]/ !i\
divide by 0
//!b pop
s/\(-*\)\([0-9]*\)\.*\([^~]*~-*\)\([0-9]*\)\.*\([^~]*\)/\2.\3\1;0\4.\5;0/
:div1
	s/^\.0\([^.]*\)\.;*\([0-9]\)\([0-9]*\);*0*/.\1\2.\3;0/
	s/^\([^.]*\)\([0-9]\)\.\([^;]*;\)0*\([0-9]*\)\([0-9]\)\./\1.\2\30\4.\5/
t div1
s/~\(-*\)\1\(-*\);0*\([^;]*[0-9]\)[^~]*/~123456789743222111~\2\3/
s/\(.\(.\)[^~]*\)[^9]*\2.\{8\}\(.\)[^~]*/\3~\1/
s,|?.,&SaSadSaKdlaZ+LaX-1+[sb1]Sbd1>bkLatsbLa[dSa2lbla*-*dLa!=a]dSaxsakLasbLb*t,
b next

:rem
s,|?%,&Sadla/LaKSa[999]k*Lak-,
b next

:exp
#  This decimal method is just a little faster than the binary method done
#  totally in dc:  1LaKLb [kdSb*LbK]Sb [[.5]*d0ktdSa<bkd*KLad1<a]Sa d1<a kk*
/^[^~]*\./i\
fraction in exponent ignored
s,[^-0-9].*,;9d**dd*8*d*d7dd**d*6d**d5d*d*4*d3d*2lbd**1lb*0,
:exp1
	s/\([0-9]\);\(.*\1\([d*]*\)[^l]*\([^*]*\)\(\**\)\)/;dd*d**d*\4\3\5\2/
t exp1
G
s,-*.\{9\}\([^9]*\)[^0]*0.\(.*|?.\),\2~saSaKdsaLb0kLbkK*+k1\1LaktsbkLax,
s,|?.,&SadSbdXSaZla-SbKLaLadSb[0Lb-d1lb-*d+K+0kkSb[1Lb/]q]Sa0>a[dk]sadK<a[Lb],
b next

:sqrt
#  first square root using sed:  8k2v at 1:30am Dec 17, 1996
/^-/i\
square root of negative number
/^[-0]/b next
s/~.*//
/^\./ s/0\([0-9]\)/\1/g
/^\./ !s/[0-9][0-9]/7/g
G
s/\n/~/
s,|?.,&K1+k KSbSb[dk]SadXdK<asadlb/lb+[.5]*[sbdlb/lb+[.5]*dlb>a]dsaxsasaLbsaLatLbk K1-kt,
b next

#  END OF GSU dc.sed
