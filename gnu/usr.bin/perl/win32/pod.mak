CONVERTERS = pod2html pod2latex pod2man pod2text checkpods \
		pod2usage podchecker podselect

HTMLROOT = /	# Change this to fix cross-references in HTML
POD2HTML = pod2html \
	    --htmlroot=$(HTMLROOT) \
	    --podroot=.. --podpath=pod:lib:ext:vms \
	    --libpods=perlfunc:perlguts:perlvar:perlrun:perlop

all: $(CONVERTERS) html

converters: $(CONVERTERS)

PERL = ..\miniperl.exe
REALPERL = ..\perl.exe

POD = \
	perl.pod	\
	perl5004delta.pod	\
	perl5005delta.pod	\
	perl561delta.pod	\
	perl56delta.pod	\
	perl570delta.pod	\
	perl571delta.pod	\
	perl572delta.pod	\
	perl573delta.pod	\
	perl581delta.pod	\
	perl582delta.pod	\
	perl583delta.pod	\
	perl584delta.pod	\
	perl585delta.pod	\
	perl586delta.pod	\
	perl58delta.pod	\
	perlapi.pod	\
	perlapio.pod	\
	perlartistic.pod	\
	perlbook.pod	\
	perlboot.pod	\
	perlbot.pod	\
	perlcall.pod	\
	perlcheat.pod	\
	perlclib.pod	\
	perlcompile.pod	\
	perldata.pod	\
	perldbmfilter.pod	\
	perldebguts.pod	\
	perldebtut.pod	\
	perldebug.pod	\
	perldelta.pod	\
	perldiag.pod	\
	perldoc.pod	\
	perldsc.pod	\
	perlebcdic.pod	\
	perlembed.pod	\
	perlfaq.pod	\
	perlfaq1.pod	\
	perlfaq2.pod	\
	perlfaq3.pod	\
	perlfaq4.pod	\
	perlfaq5.pod	\
	perlfaq6.pod	\
	perlfaq7.pod	\
	perlfaq8.pod	\
	perlfaq9.pod	\
	perlfilter.pod	\
	perlfork.pod	\
	perlform.pod	\
	perlfunc.pod	\
	perlgpl.pod	\
	perlguts.pod	\
	perlhack.pod	\
	perlhist.pod	\
	perlintern.pod	\
	perlintro.pod	\
	perliol.pod	\
	perlipc.pod	\
	perllexwarn.pod	\
	perllocale.pod	\
	perllol.pod	\
	perlmod.pod	\
	perlmodinstall.pod	\
	perlmodlib.pod	\
	perlmodstyle.pod	\
	perlnewmod.pod	\
	perlnumber.pod	\
	perlobj.pod	\
	perlop.pod	\
	perlopentut.pod	\
	perlothrtut.pod	\
	perlpacktut.pod	\
	perlpod.pod	\
	perlpodspec.pod	\
	perlport.pod	\
	perlre.pod	\
	perlref.pod	\
	perlreftut.pod	\
	perlrequick.pod	\
	perlreref.pod	\
	perlretut.pod	\
	perlrun.pod	\
	perlsec.pod	\
	perlstyle.pod	\
	perlsub.pod	\
	perlsyn.pod	\
	perlthrtut.pod	\
	perltie.pod	\
	perltoc.pod	\
	perltodo.pod	\
	perltooc.pod	\
	perltoot.pod	\
	perltrap.pod	\
	perlunicode.pod	\
	perluniintro.pod	\
	perlutil.pod	\
	perlvar.pod	\
	perlxs.pod	\
	perlxstut.pod	

MAN = \
	perl.man	\
	perl5004delta.man	\
	perl5005delta.man	\
	perl561delta.man	\
	perl56delta.man	\
	perl570delta.man	\
	perl571delta.man	\
	perl572delta.man	\
	perl573delta.man	\
	perl581delta.man	\
	perl582delta.man	\
	perl583delta.man	\
	perl584delta.man	\
	perl585delta.man	\
	perl586delta.man	\
	perl58delta.man	\
	perlapi.man	\
	perlapio.man	\
	perlartistic.man	\
	perlbook.man	\
	perlboot.man	\
	perlbot.man	\
	perlcall.man	\
	perlcheat.man	\
	perlclib.man	\
	perlcompile.man	\
	perldata.man	\
	perldbmfilter.man	\
	perldebguts.man	\
	perldebtut.man	\
	perldebug.man	\
	perldelta.man	\
	perldiag.man	\
	perldoc.man	\
	perldsc.man	\
	perlebcdic.man	\
	perlembed.man	\
	perlfaq.man	\
	perlfaq1.man	\
	perlfaq2.man	\
	perlfaq3.man	\
	perlfaq4.man	\
	perlfaq5.man	\
	perlfaq6.man	\
	perlfaq7.man	\
	perlfaq8.man	\
	perlfaq9.man	\
	perlfilter.man	\
	perlfork.man	\
	perlform.man	\
	perlfunc.man	\
	perlgpl.man	\
	perlguts.man	\
	perlhack.man	\
	perlhist.man	\
	perlintern.man	\
	perlintro.man	\
	perliol.man	\
	perlipc.man	\
	perllexwarn.man	\
	perllocale.man	\
	perllol.man	\
	perlmod.man	\
	perlmodinstall.man	\
	perlmodlib.man	\
	perlmodstyle.man	\
	perlnewmod.man	\
	perlnumber.man	\
	perlobj.man	\
	perlop.man	\
	perlopentut.man	\
	perlothrtut.man	\
	perlpacktut.man	\
	perlpod.man	\
	perlpodspec.man	\
	perlport.man	\
	perlre.man	\
	perlref.man	\
	perlreftut.man	\
	perlrequick.man	\
	perlreref.man	\
	perlretut.man	\
	perlrun.man	\
	perlsec.man	\
	perlstyle.man	\
	perlsub.man	\
	perlsyn.man	\
	perlthrtut.man	\
	perltie.man	\
	perltoc.man	\
	perltodo.man	\
	perltooc.man	\
	perltoot.man	\
	perltrap.man	\
	perlunicode.man	\
	perluniintro.man	\
	perlutil.man	\
	perlvar.man	\
	perlxs.man	\
	perlxstut.man	

HTML = \
	perl.html	\
	perl5004delta.html	\
	perl5005delta.html	\
	perl561delta.html	\
	perl56delta.html	\
	perl570delta.html	\
	perl571delta.html	\
	perl572delta.html	\
	perl573delta.html	\
	perl581delta.html	\
	perl582delta.html	\
	perl583delta.html	\
	perl584delta.html	\
	perl585delta.html	\
	perl586delta.html	\
	perl58delta.html	\
	perlapi.html	\
	perlapio.html	\
	perlartistic.html	\
	perlbook.html	\
	perlboot.html	\
	perlbot.html	\
	perlcall.html	\
	perlcheat.html	\
	perlclib.html	\
	perlcompile.html	\
	perldata.html	\
	perldbmfilter.html	\
	perldebguts.html	\
	perldebtut.html	\
	perldebug.html	\
	perldelta.html	\
	perldiag.html	\
	perldoc.html	\
	perldsc.html	\
	perlebcdic.html	\
	perlembed.html	\
	perlfaq.html	\
	perlfaq1.html	\
	perlfaq2.html	\
	perlfaq3.html	\
	perlfaq4.html	\
	perlfaq5.html	\
	perlfaq6.html	\
	perlfaq7.html	\
	perlfaq8.html	\
	perlfaq9.html	\
	perlfilter.html	\
	perlfork.html	\
	perlform.html	\
	perlfunc.html	\
	perlgpl.html	\
	perlguts.html	\
	perlhack.html	\
	perlhist.html	\
	perlintern.html	\
	perlintro.html	\
	perliol.html	\
	perlipc.html	\
	perllexwarn.html	\
	perllocale.html	\
	perllol.html	\
	perlmod.html	\
	perlmodinstall.html	\
	perlmodlib.html	\
	perlmodstyle.html	\
	perlnewmod.html	\
	perlnumber.html	\
	perlobj.html	\
	perlop.html	\
	perlopentut.html	\
	perlothrtut.html	\
	perlpacktut.html	\
	perlpod.html	\
	perlpodspec.html	\
	perlport.html	\
	perlre.html	\
	perlref.html	\
	perlreftut.html	\
	perlrequick.html	\
	perlreref.html	\
	perlretut.html	\
	perlrun.html	\
	perlsec.html	\
	perlstyle.html	\
	perlsub.html	\
	perlsyn.html	\
	perlthrtut.html	\
	perltie.html	\
	perltodo.html	\
	perltooc.html	\
	perltoot.html	\
	perltrap.html	\
	perlunicode.html	\
	perluniintro.html	\
	perlutil.html	\
	perlvar.html	\
	perlxs.html	\
	perlxstut.html	
# not perltoc.html

TEX = \
	perl.tex	\
	perl5004delta.tex	\
	perl5005delta.tex	\
	perl561delta.tex	\
	perl56delta.tex	\
	perl570delta.tex	\
	perl571delta.tex	\
	perl572delta.tex	\
	perl573delta.tex	\
	perl581delta.tex	\
	perl582delta.tex	\
	perl583delta.tex	\
	perl584delta.tex	\
	perl585delta.tex	\
	perl586delta.tex	\
	perl58delta.tex	\
	perlapi.tex	\
	perlapio.tex	\
	perlartistic.tex	\
	perlbook.tex	\
	perlboot.tex	\
	perlbot.tex	\
	perlcall.tex	\
	perlcheat.tex	\
	perlclib.tex	\
	perlcompile.tex	\
	perldata.tex	\
	perldbmfilter.tex	\
	perldebguts.tex	\
	perldebtut.tex	\
	perldebug.tex	\
	perldelta.tex	\
	perldiag.tex	\
	perldoc.tex	\
	perldsc.tex	\
	perlebcdic.tex	\
	perlembed.tex	\
	perlfaq.tex	\
	perlfaq1.tex	\
	perlfaq2.tex	\
	perlfaq3.tex	\
	perlfaq4.tex	\
	perlfaq5.tex	\
	perlfaq6.tex	\
	perlfaq7.tex	\
	perlfaq8.tex	\
	perlfaq9.tex	\
	perlfilter.tex	\
	perlfork.tex	\
	perlform.tex	\
	perlfunc.tex	\
	perlgpl.tex	\
	perlguts.tex	\
	perlhack.tex	\
	perlhist.tex	\
	perlintern.tex	\
	perlintro.tex	\
	perliol.tex	\
	perlipc.tex	\
	perllexwarn.tex	\
	perllocale.tex	\
	perllol.tex	\
	perlmod.tex	\
	perlmodinstall.tex	\
	perlmodlib.tex	\
	perlmodstyle.tex	\
	perlnewmod.tex	\
	perlnumber.tex	\
	perlobj.tex	\
	perlop.tex	\
	perlopentut.tex	\
	perlothrtut.tex	\
	perlpacktut.tex	\
	perlpod.tex	\
	perlpodspec.tex	\
	perlport.tex	\
	perlre.tex	\
	perlref.tex	\
	perlreftut.tex	\
	perlrequick.tex	\
	perlreref.tex	\
	perlretut.tex	\
	perlrun.tex	\
	perlsec.tex	\
	perlstyle.tex	\
	perlsub.tex	\
	perlsyn.tex	\
	perlthrtut.tex	\
	perltie.tex	\
	perltoc.tex	\
	perltodo.tex	\
	perltooc.tex	\
	perltoot.tex	\
	perltrap.tex	\
	perlunicode.tex	\
	perluniintro.tex	\
	perlutil.tex	\
	perlvar.tex	\
	perlxs.tex	\
	perlxstut.tex	

man:	pod2man $(MAN)

html:	pod2html $(HTML)

tex:	pod2latex $(TEX)

toc:
	$(PERL) -I../lib buildtoc >perltoc.pod

.SUFFIXES: .pm .pod

.SUFFIXES: .man

.pm.man:
	$(PERL) -I../lib pod2man $*.pm >$*.man

.pod.man:
	$(PERL) -I../lib pod2man $*.pod >$*.man

.SUFFIXES: .html

.pm.html:
	$(PERL) -I../lib $(POD2HTML) --infile=$*.pm --outfile=$*.html

.pod.html:
	$(PERL) -I../lib $(POD2HTML) --infile=$*.pod --outfile=$*.html

.SUFFIXES: .tex

.pm.tex:
	$(PERL) -I../lib pod2latex $*.pm

.pod.tex:
	$(PERL) -I../lib pod2latex $*.pod

clean:
	rm -f $(MAN)
	rm -f $(HTML)
	rm -f $(TEX)
	rm -f pod2html-*cache
	rm -f *.aux *.log *.exe

realclean:	clean
	rm -f $(CONVERTERS)

distclean:	realclean

check:	checkpods
	@echo "checking..."; \
	$(PERL) -I../lib checkpods $(POD)

# Dependencies.
pod2latex:	pod2latex.PL ../lib/Config.pm
	$(PERL) -I../lib pod2latex.PL

pod2html:	pod2html.PL ../lib/Config.pm
	$(PERL) -I ../lib pod2html.PL

pod2man:	pod2man.PL ../lib/Config.pm
	$(PERL) -I ../lib pod2man.PL

pod2text:	pod2text.PL ../lib/Config.pm
	$(PERL) -I ../lib pod2text.PL

checkpods:	checkpods.PL ../lib/Config.pm
	$(PERL) -I ../lib checkpods.PL

pod2usage:	pod2usage.PL ../lib/Config.pm
	$(PERL) -I ../lib pod2usage.PL

podchecker:	podchecker.PL ../lib/Config.pm
	$(PERL) -I ../lib podchecker.PL

podselect:	podselect.PL ../lib/Config.pm
	$(PERL) -I ../lib podselect.PL

compile: all
	$(REALPERL) -I../lib ../utils/perlcc pod2latex -o pod2latex.exe -v 10 -log ../compilelog
	$(REALPERL) -I../lib ../utils/perlcc pod2man -o pod2man.exe -v 10 -log ../compilelog
	$(REALPERL) -I../lib ../utils/perlcc pod2text -o pod2text.exe -v 10 -log ../compilelog
	$(REALPERL) -I../lib ../utils/perlcc checkpods -o checkpods.exe -v 10 -log ../compilelog
