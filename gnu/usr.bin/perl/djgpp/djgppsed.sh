#! /bin/sh

# Change some files to work under DOS
# Most of this stuff does .xx -> _xx and aa.bb.ccc -> aa_bb.cc conversion

SCONFIG='s=\.\(config\)=_\1=g'
SLIST='s=\.\([a-z]\+list\)=_\1=g'
SGREPTMP='s=\.\(greptmp\)=_\1=g'
SECHOTMP='s=\.\(echotmp\)=_\1=g'
SDDC='s=\.\($$\.c\)=_\1=g'
SOUT='s=\([^a-z1-9?]\)\.\(out\)=\1_\2=g'
SEXISTS='s=\.\(exists\)=_\1=g'
SPOD2HTML='s=pod2html-=pod2html.=g'
SCC='s=\.c\.c=.c_c=g'
SFILEC="s=\(\$file\)\.c=\\1'_c'=g"
SCOR='s=c\\\.c|=c\_c|=g'
SHSED='s=\.\(hsed\)=_\1=g'
SDEPTMP='s=\.\(deptmp\)=_\1=g'
SCPP='s=\.\(cpp\.\)=_\1=g'
SARGV='s=\.\(argv\)\.=_\1_=g'
SABC='s=\.\([abc][^a]\)=_\1=g'
SDBMX='s=\.\(dbmx\)=_\1=g'
SDBHASH='s=dbhash\.tmp=dbhash_tmp=g'
SSTAT='s=\.\(stat\.\)=_\1=g'
STMP2='s=tmp2=tm2=g'
SPACKLIST='s=\.\(packlist\)=_\1=g'
SDOTTMP='s=\.tmp=_tmp=g'

sed -e $SCONFIG -e $SGREPTMP -e $SECHOTMP -e $SDDC -e $SOUT -e 's=\.\( \./\$file\)$=sh\1=g' Configure |tr -d '\r' >s; mv -f s Configure
sed -e $SEXISTS -e $SLIST -e $SCONFIG Makefile.SH |tr -d '\r' >s; mv -f s Makefile.SH
sed -e $SEXISTS -e $SPACKLIST lib/ExtUtils/Install.pm |tr -d '\r' >s; mv -f s lib/ExtUtils/Install.pm
sed -e $SEXISTS -e $SPACKLIST lib/ExtUtils/MM_Unix.pm |tr -d '\r' >s; mv -f s lib/ExtUtils/MM_Unix.pm
sed -e $SEXISTS -e $SPACKLIST installperl >s; mv -f s installperl
sed -e $SPOD2HTML lib/Pod/Html.pm |tr -d '\r' >s; mv -f s lib/Pod/Html.pm
sed -e $SCC -e $SLIST -e $SFILEC -e $SCOR -e $SDEPTMP -e $SHSED makedepend.SH |tr -d '\r' >s; mv -f s makedepend.SH
sed -e $SCPP t/comp/cpp.aux |tr -d '\r' >s; mv -f s t/comp/cpp.aux
sed -e $SARGV -e $SDOTTMP t/io/argv.t >s; mv -f s t/io/argv.t
sed -e $SABC t/io/inplace.t >s; mv -f s t/io/inplace.t
sed -e $SDBMX t/lib/anydbm.t >s; mv -f s t/lib/anydbm.t
sed -e $SDBMX -e $SDBHASH t/lib/gdbm.t >s; mv -f s t/lib/gdbm.t
sed -e $SDBMX -e $SDBHASH t/lib/sdbm.t >s; mv -f s t/lib/sdbm.t
sed -e $SSTAT -e $STMP2 t/op/stat.t >s; mv -f s t/op/stat.t
sed -e $SLIST x2p/Makefile.SH |tr -d '\r' >s; mv -f s x2p/Makefile.SH
sed -e 's=^#define.\([A-Z]\+\)_EXP.*$=#define \1_EXP djgpp_pathexp("\1")=g' config_h.SH >s; mv -f s config_h.SH
sed -e 's=:^/:={^([a-z]:)?[\\\\/]}=g' lib/termcap.pl >s; mv -f s lib/termcap.pl
sed -e $SPACKLIST installman >s; mv -f s installman
sed -e $SPACKLIST lib/ExtUtils/Installed.pm >s; mv -f s lib/ExtUtils/Installed.pm
sed -e $SPACKLIST lib/ExtUtils/Packlist.pm >s; mv -f s lib/ExtUtils/Packlist.pm
sed -e $SPACKLIST lib/ExtUtils/inst >s; mv -f s lib/ExtUtils/inst
sed -e $SABC t/io/iprefix.t >s; mv -f s t/io/iprefix.t
