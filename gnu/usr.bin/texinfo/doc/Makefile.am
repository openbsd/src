# $Id: Makefile.am,v 1.4 2002/06/10 13:51:02 espie Exp $
# Makefile.am for texinfo/doc.
# Run automake in .. to produce Makefile.in from this.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# Put texinfo.txi first because that's the most important.
info_TEXINFOS = texinfo.txi info-stnd.texi info.texi
DISTCLEANFILES = texinfo texinfo-* info*.info*

man_MANS = info.1 install-info.1 makeinfo.1 texindex.1 texi2dvi.1 \
           info.5 texinfo.5
HELP2MAN = help2man
noinst_SCRIPTS = $(HELP2MAN)

# Use the programs built in our distribution.
MAKEINFO = ../makeinfo/makeinfo
INSTALL_INFO = ../util/install-info

TXI_XLATE = txi-cs.tex txi-de.tex txi-en.tex txi-es.tex txi-fr.tex \
	    txi-nl.tex txi-no.tex txi-pt.tex

# Include our texinfo.tex, not Automake's.
EXTRA_DIST = epsf.tex pdfcolor.tex texinfo.tex \
             fdl.texi macro.texi userdoc.texi \
	     $(man_MANS) $(noinst_SCRIPTS) $(TXI_XLATE)

install-data-local:
	@echo
	@echo "WARNING: You must install texinfo.tex and txi-??.tex manually,"
	@echo "         perhaps in TEXMF/tex/texinfo/,"
	@echo "         where TEXMF is a root of your TeX tree."
	@echo "         See doc/README for some considerations."
	@echo "         You can run make TEXMF=/your/texmf install-tex to do this."
	@echo
	@echo "         You may also need to install epsf.tex and (if you"
	@echo "         use pdftex) pdfcolor.tex in TEXMF/tex/generic/dvips"
	@echo "         and TEXMF/pdftex/plain/misc/pdfcolor.tex respectively,"
	@echo "         if your TeX installation did not include them."

texmf_texinfo = $(TEXMF)/tex/texinfo
texmf_dvips = $(TEXMF)/tex/generic/dvips
texmf_pdftex_misc = $(TEXMF)/pdftex/plain/misc
install-tex:
	test -n "$(TEXMF)" || (echo "TEXMF must be set." >&2; exit 1)
	$(mkinstalldirs) $(DESTDIR)$(texmf_texinfo) $(DESTDIR)$(texmf_dvips) $(DESTDIR)$(texmf_pdftex_misc)
	$(INSTALL_DATA) $(srcdir)/texinfo.tex $(DESTDIR)$(texmf_texinfo)/texinfo.tex
	$(INSTALL_DATA) $(srcdir)/epsf.tex $(DESTDIR)$(texmf_dvips)/epsf.tex
	$(INSTALL_DATA) $(srcdir)/pdfcolor.tex $(DESTDIR)$(texmf_pdftex_misc)/pdfcolor.tex
	for f in $(TXI_XLATE); do \
	  $(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(texmf_texinfo)/$$f; done

# Don't want to run help2man at every installation.
if TEXINFO_MAINT
info.1: ../info/ginfo $(HELP2MAN)
	$(HELP2MAN) --name="read Info documents" $< >$@
install-info.1: $(INSTALL_INFO) $(HELP2MAN)
	$(HELP2MAN) --name="update info/dir entries" $< >$@
makeinfo.1: $(MAKEINFO) $(HELP2MAN)
	$(HELP2MAN) --name="translate Texinfo documents" $< >$@
texi2dvi.1: ../util/texi2dvi $(HELP2MAN)
	$(HELP2MAN) --name="print Texinfo documents" $< >$@
texindex.1: ../util/texindex $(HELP2MAN)
	$(HELP2MAN) --name="sort Texinfo index files" $< >$@
endif

# Do not create info files for distribution, but do create man pages.
dist-info:

# Do not try to build the info files in $(srcdir),
# since we don't distribute them.
.texi.info:
	$(MAKEINFO) -I$(srcdir) `echo $< | sed 's,.*/,,'`
texinfo: $(srcdir)/texinfo.txi
	$(MAKEINFO) -I$(srcdir) texinfo.txi
