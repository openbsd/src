CONVERTERS = pod2html pod2latex pod2man pod2text

all: $(CONVERTERS) man
PERL = ../miniperl

POD = 	\
	perl.pod	\
	perlbook.pod	\
	perlbot.pod	\
	perlcall.pod	\
	perldata.pod	\
	perldebug.pod	\
	perldiag.pod	\
	perldsc.pod	\
	perlembed.pod	\
	perlform.pod	\
	perlfunc.pod	\
	perlguts.pod	\
	perlipc.pod	\
	perllol.pod	\
	perlmod.pod	\
	perlobj.pod	\
	perlop.pod	\
	perlovl.pod	\
	perlpod.pod	\
	perlre.pod	\
	perlref.pod	\
	perlrun.pod	\
	perlsec.pod	\
	perlstyle.pod	\
	perlsub.pod	\
	perlsyn.pod	\
	perltie.pod	\
	perltoc.pod	\
	perltrap.pod	\
	perlvar.pod	\
	perlxs.pod	\
	perlxstut.pod

MAN = 	\
	perl.man	\
	perlbook.man	\
	perlbot.man	\
	perlcall.man	\
	perldata.man	\
	perldebug.man	\
	perldiag.man	\
	perldsc.man	\
	perlembed.man	\
	perlform.man	\
	perlfunc.man	\
	perlguts.man	\
	perlipc.man	\
	perllol.man	\
	perlmod.man	\
	perlobj.man	\
	perlop.man	\
	perlovl.man	\
	perlpod.man	\
	perlre.man	\
	perlref.man	\
	perlrun.man	\
	perlsec.man	\
	perlstyle.man	\
	perlsub.man	\
	perlsyn.man	\
	perltie.man	\
	perltoc.man	\
	perltrap.man	\
	perlvar.man	\
	perlxs.man	\
	perlxstut.man

HTML = 	\
	perl.html	\
	perlbook.html	\
	perlbot.html	\
	perlcall.html	\
	perldata.html	\
	perldebug.html	\
	perldiag.html	\
	perldsc.html	\
	perlembed.html	\
	perlform.html	\
	perlfunc.html	\
	perlguts.html	\
	perlipc.html	\
	perllol.html	\
	perlmod.html	\
	perlobj.html	\
	perlop.html	\
	perlovl.html	\
	perlpod.html	\
	perlre.html	\
	perlref.html	\
	perlrun.html	\
	perlsec.html	\
	perlstyle.html	\
	perlsub.html	\
	perlsyn.html	\
	perltie.html	\
	perltoc.html	\
	perltrap.html	\
	perlvar.html	\
	perlxs.html	\
	perlxstut.html

TEX = 	\
	perl.tex	\
	perlbook.tex	\
	perlbot.tex	\
	perlcall.tex	\
	perldata.tex	\
	perldebug.tex	\
	perldiag.tex	\
	perldsc.tex	\
	perlembed.tex	\
	perlform.tex	\
	perlfunc.tex	\
	perlguts.tex	\
	perlipc.tex	\
	perllol.tex	\
	perlmod.tex	\
	perlobj.tex	\
	perlop.tex	\
	perlovl.tex	\
	perlpod.tex	\
	perlre.tex	\
	perlref.tex	\
	perlrun.tex	\
	perlsec.tex	\
	perlstyle.tex	\
	perlsub.tex	\
	perlsyn.tex	\
	perltie.tex	\
	perltoc.tex	\
	perltrap.tex	\
	perlvar.tex	\
	perlxs.tex	\
	perlxstut.tex

man:  pod2man $(MAN)

# pod2html normally runs on all the pods at once in order to build up
# cross-references.
html: pod2html
	$(PERL) -I../lib pod2html $(POD)

tex:	pod2latex $(TEX)

.SUFFIXES: .pm .pod .man

.pm.man:     pod2man
	$(PERL) -I../lib pod2man $*.pm >$*.man

.pod.man:     pod2man
	$(PERL) -I../lib pod2man $*.pod >$*.man

.SUFFIXES: .mp .pod .html

.pm.html:    pod2html
	$(PERL) -I../lib pod2html $*.pod

.pod.html:    pod2html
	$(PERL) -I../lib pod2html $*.pod

.SUFFIXES: .pm .pod .tex

.pod.tex: pod2latex
	$(PERL) -I../lib pod2latex $*.pod

.pm.tex: pod2latex
	$(PERL) -I../lib pod2latex $*.pod

clean:
	rm -f $(MAN) $(HTML) $(TEX)

realclean:	clean
	rm -f $(CONVERTERS)

distclean:	realclean

# Dependencies.
pod2latex:	pod2latex.PL ../lib/Config.pm
	$(PERL) -I../lib pod2latex.PL

pod2html:	pod2html.PL ../lib/Config.pm
	$(PERL) -I ../lib pod2html.PL

pod2man:	pod2man.PL ../lib/Config.pm
	$(PERL) -I ../lib pod2man.PL

pod2text:	pod2text.PL ../lib/Config.pm
	$(PERL) -I ../lib pod2text.PL
