# Hints for the Power MAX OS operating system (formerly PowerUX - hence the
# name) running on Concurrent (formerly Harris) NightHawk machines.  Written
# by Tom.Horsley@ccur.com
#
# This hint uses dynamic linking and the new Concurrent C compiler (based
# on the Edison front end).  This hint file was produced for a build of the
# 5.7.3 development release of perl running on a PowerMAX_OS 5.1SR2 system
# (but it should work on any Power MAX release using the newer "ec" (versus
# "cc") compiler, and hopefully will also work for the upcoming 5.8
# development release of perl).

# First find out where the root of the source tree is located.

SRCROOT=""
if [ -f ./INSTALL ]
then
   SRCROOT="."
else
   if [ -f ../INSTALL ]
   then
      SRCROOT=".."
   fi
fi
if [ -z "$SRCROOT" ]
then
   echo "powerux hint file cannot locate root perl source!" 1>&2
   exit 2
fi

# We DO NOT want -lmalloc or -lPW, we DO need -lgen to follow -lnsl, so
# fixup libswanted to reflect that desire (also need -lresolv if you want
# DNS name lookup to work, which seems desirable :-).
#
libswanted=`echo ' '$libswanted' ' | sed -e 's/ malloc / /' -e 's/ PW / /' -e 's/ nsl / nsl gen resolv /'`

# We DO NOT want /usr/ucblib in glibpth
#
glibpth=`echo ' '$glibpth' ' | sed -e 's@ /usr/ucblib @ @'`

# Yes, csh exists, but doesn't work worth beans, if perl tries to use it,
# the glob test fails, so just pretend it isn't there...
#
d_csh='undef'

# Need to use Concurrent ec for most of these options to be meaningful (if you
# want to get this to work with gcc, you're on your own :-). Passing
# -Bexport to the linker when linking perl is important because it leaves
# the interpreter internal symbols visible to the shared libs that will be
# loaded on demand (and will try to reference those symbols). The -usys_nerr
# drags in some stuff from libc that perl proper doesn't reference but
# some dynamically linked extension will need to be in the static part
# of perl (there are probably more of these that might be useful, but
# for the extensions I build, this turned out to be enough). The -uldexp
# makes sure the custom ldexp.o I add to archobjs actually gets pulled
# into perl from libperl.a
#
cc='/usr/ccs/bin/ec'
cccdlflags='-Zpic'
ccdlflags='-Zlink=dynamic -Wl,-usys_nerr -Wl,-uldexp -Wl,-Bexport'
lddlflags='-Zlink=so'

# Sigh... Various versions of Power MAX went out with a broken ldexp runtime
# routine in libc (it is fixed for sure in the upcoming SR4 release, but
# that hasn't made it out the door yet). Since libc is linked dynamically,
# and the perl you build might try to run on one of the broken systems, we
# need to statically link a corrected copy of ldexp.o into perl. What the
# following code does is determine if the ldexp.o on the current system
# works right. If it does, it simply extracts the ldexp.o from the system C
# library and uses that .o file. If the system .o is broken, the btoa
# encoded copy of a correct ldexp.o file included in this hint file is used
# (what a pain...)
#
if [ ! -f $SRCROOT/ldexp.o ]
then
   echo Finding a correct copy of ldexp.o to link with... 1>&2
   cat > $SRCROOT/UU/ldexptest.c <<'EOF'
#include <stdio.h>
#include <math.h>
#include <string.h>
int
main(int argc, char ** argv) {
   double result = pow(2.0, 38.0);
   char buf[100];
   sprintf(buf, "%g", result);
   if (strncmp(buf, "inf", 3) == 0) {
      exit(2);
   }
   return 0;
}
EOF
   GOODLDEXP="no"
   $cc -v -Zlink=static -o $SRCROOT/UU/ldexptest $SRCROOT/UU/ldexptest.c -lm > $SRCROOT/UU/ldexptest.lo 2>&1
   if [ $? -eq 0 ]
   then
      $SRCROOT/UU/ldexptest
      if [ $? -eq 0 ]
      then
         LDEXPLIB=`fgrep libc.a $SRCROOT/UU/ldexptest.lo | tail -1 | sed -e 's@^[^/]*@@'`
         if [ -s "$LDEXPLIB" ]
         then
            if [ -f "$LDEXPLIB" ]
            then
               GOODLDEXP="yes"
            fi
         fi
      fi
   fi
   if [ "$GOODLDEXP" = "yes" ]
   then
      echo Congratulations! The ldexp.o on this system looks good! 1>&2
      echo Using ldexp.o from $LDEXPLIB 1>&2
      ( cd $SRCROOT ; ar x $LDEXPLIB ldexp.o )
   else
      echo Sorry, the ldexp.o on this system is busted. 1>&2
      echo Using the ldexp.o from the powerux hint file 1>&2
      atob > $SRCROOT/ldexp.o << 'EOF'
xbtoa Begin
Imm%#!<N9%zz!!*'-!!!!"zz!!!8Jz!&OZU!!!!I!"/c-!%r>7Ecb`!!%rA)G]Wp<Ec5JsFC>/%FC\
s(@fS,lAR]dp?YjFoAH3u00JG4;0JEJZF*VVE@:B4QA7^")/n4k]/hUsNAU&0$@rH4'?Zg7#FC/KgB
5)5`!%om?A7^")?Yj7aG]7#$DI``"/o5'0G]7#+A7^")?N:'+5\stBG]7#+Bl7KhF*(i2F9"RBA7^"
)?YjFoARB"dA,nl2A7^")?YjFoARAnXB5)5`5\stBG]7#/Ec5c4B6@cmASu#Y5\stBG]7#/Ec5c4B6
@cm@V'1dD?'ZQA7^")!+0)TBQ@HkEcQ&9!+p7_G]3XiCh[?cG%G]8Bl@kh?XIJhB4YFn@;GorEb0&q
/p(ZLF9!q6ASbd-FC\s(@fS-%ASbd-A7]4mB4#IhDIieJz3$J<@IAd4EOoYQ5HuL$L3Pb]og;*c.rk
Jf$0+\*`g>N$VfHC6nOeDcBJaNL<r#i5*<UF@H/I_sb5`,PtJ;sU43WK.'.>.[$5ct)L<TXBJ5b\68
8,rVja<:P^38ac\OQ-<@b/"'sb2E>Fr#l&\JY<(2JcPk%3$A9`IAd7F:4N<e<U"H%5b\5i3FDgf;/_
p@OmW2L8,rVjOok[aa<:P^b/"'sb2E>FJY<(2JcPk%3$A9`IAd7F:4N<e6(.iX4J2ZSb2iU's-C.p6
,!Blr#i5*3+2eP8,rVjJH5a9/J%m^4[8uI/!'`P5`#LiIF(:p4CCN1/WKr63FDgf5ck%!8,rVj4[8u
I3T'l]4obQ_OlHEAJP#nB/d_RY5dCAdrg(%ob2E>F4eMcT^b#Nd5a26gb/"'sJY<(2JcPk%3$A9`IA
d7F:4N<eb/"'s4J2ZS^a/s\JY<(2JcPk%3$A9`IAd7F:4N<eaKPXErt`*EJY<(25aVNob/"'sr#dCa
IAd7Fb2E>FJcPk%3$A9`:4N<eb/"'sr#dDL4TGH^IAd7Fb2E>FJcPk%3$A9`:4N<eaQ`a*5b.lp4Wj
_)b,G@@3Y;>Nrmh6n.M)S$6';3>OC8,cI;FEfb2E>FD1mE>OF[C2aT2B<JY<(2JcPk%r5^iGIAd7Fr
&+S]b/"'s3$A9`:4N<eI'>pO3T0qs/VX)J4[;@g3<0$[I1UWg5car>8,rVj35>M<r#iM2OM_%ub/"'
sb2E>FJY<(2JcPk%3$A9`IAd7F:4N<eJY<(25_oC_4hq$tb)QH%3Y29438jhrrmh6.IulWT6(IuEO[
/tTILlM+IAd7Fb2E>FI11W[4obQ_a\Vs;D1mE>OF[CBa^G0WJcPk%r@:\mr&.(kb/"'s3$A9`:4N<e
b/"'sb2E>FJY<(2JcPk%3$A9`IAd7F:4N<ezzzs*t(KzIt.Luz6-oT3z6SJK?J,fQL4qI\oz!!!!s!
!!!$zz!s/HG!!!"\!!!!)s8W,W!!!*$!!!"@!!3-#!"]85q[3`2!<E3%!!!!"!!!!&!WW3#!!WNU!W
rH*If]fT!sSf.!Cp$,"p9>V!<FMOCe,mh8j5@-)[6Co!W`<V"u5N)49bn;!W`<+-`^N""p9>V!<F,D
Bh&@0If]WO"t'Ld!Y>A:>Q=d*zz"98E)zzzzzzzzzz!!!!\zz"9AJl!!!!dzz!rr<&!!!!ezz!!!!#
!!!"(zz!rr<'!!!")!!!)]z!!!!#!!!";!!!!Mz!!!!#!!!"Jzz!rr<(!!!"Kzz!rr<)!!!"Lzz!!!
!&!!!"^!!!"Dz!!!!&!!!"n!!!!%z!!!!&!!!#+!!!!;z!!!!&!!!#?!!!!+z!!!!&!!!#Uzz!rr<*
!!!#Vz!!!)]&c_n5!!!#\zz&-)\1!!!#hzz&-)\1!!!#nzz&-)\1!!!$&zz&-)\1!!!"$!!!Q<z!!!
"h!!!Q<z!!!#A!!!-Gz!!!#E!!!-Hz!!!#W!!!T=z!!!#e!!!-G!!!!A!!!$.!!!Q<z!!!$4!!!WUz
!!!$<!!!ZVz!!!$D!!!WVz!!!$X!!!-G!!!!)!!!$\!!!-H!!!!)!!!%+!!!-G!!!!1!!!%/!!!-H!
!!!1!!!%G!!!ZWz!!!&&!!!ZVz!!!&>!!!-H!!!!A!!!&F!!!-G!!!!9!!!&J!!!-H!!!!9!!!'[!!
!Q<z!!!(<!!!-G!!!!I!!!(@!!!-H!!!!I!!!(l!!!-G!!!!A!!!(p!!!-H!!!!A!!!!)!!!3Gz!!!
!-!!!'C!!!)]!!!!>!!!*Ezzzzzzzzzzz!!!!"!!!!$zz!!!!U!!!$Yzz!!!!"z!!!!*!!!!"!!!!'
z!!!%=!!!)]zz!!!!1z!!!!0!!!!"!!!!#z!!!.(!!!!Qzz!!!!)z!!!!8!!!!"!!!!#z!!!.X!!!!
Ezz!!!!%z!!!!?!!!!"zz!!!/'!!!"Dzz!!!!%z!!!!KJ,fQLzz!!!0J!!!!Ezz!!!!%z!!!!T!!!!
#zz!!!0n!!!$b!!!!"!!!!0!!!!%!!!!1!!!$1!!!!%zz!!!4Z!!!$B!!!!(!!!!#!!!!%!!!!-!!!
$<!!!!%zz!!!8&!!!!9!!!!(!!!!%!!!!%!!!!-!!!$H!!!!%zz!!!8>!!!!-!!!!(!!!!&!!!!%!!
!!-
xbtoa End N 2436 984 E ad S 1bf43 R a7867666
EOF
   fi
   ( cd $SRCROOT/UU ; rm -f ldexptest* )
fi
if [ -f $SRCROOT/ldexp.o ]
then
   archobjs='ldexp.o'
fi

# Configure sometime finds what it believes to be ndbm header files on the
# system and imagines that we have the NDBM library, but we really don't.
# There is something there that once resembled ndbm, but it is purely
# for internal use in some tool and has been hacked beyond recognition
# (or even function :-)
#
i_ndbm='undef'

# I have no clue what perl thinks it wants <sys/mode.h> for, but if you
# include it in a program in PowerMAX without first including <sys/vnode.h>
# the code don't compile (apparently some other operating system has
# something completely different in its sys/mode.h)
#
i_sysmode='undef'

# There was a bug in memcmp (which was fixed a while ago) which sometimes
# fails to provide the correct compare status (it is data dependant). I
# don't wnat to figure out if you are building with the correct version or
# not, so just pretend there is no memcmp (since perl has its own handy
# substitute).
#
d_memcmp='undef'

# Due to problems with dynamic linking (which I also hope will be fixed soon)
# you can't build a libperl.so, the core has to be in the static part of the
# perl executable.
#
useshrplib='false'

# PowerMAX OS has support for a few different kinds of filesystems. The
# newer "xfs" filesystem does *not* report a reasonable value in the
# 'nlinks' field of stat() info for directories (in fact, it is always 1).
# Since xfs is the only filesystem which supports partitions bigger than
# 2gig and you can't hardly buy a disk that small anymore, xfs is coming in
# to greater and greater use, so we pretty much have no choice but to
# abandon all hope that number of links will mean anything.
#
dont_use_nlink=define

# Configure comes up with the wrong type for these for some reason.  The
# pointers shouldn't have const in them. (And it looks like I have to
# provide netdb_hlen_type as well becuase when I predefine the others it
# comes up empty :-).
#
netdb_host_type='char *'
netdb_name_type='char *'
netdb_hlen_type='int'

# Misc other flags that might be able to change, but I know these work right.
#
d_suidsafe='define'
d_isascii='define'
d_mymalloc='undef'
usemymalloc='n'
ssizetype='ssize_t'
usevfork='false'

