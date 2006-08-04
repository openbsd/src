# $Id: Makefile.am,v 1.6 2006/08/04 10:40:02 jmc Exp $
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

man_MANS = info.1 infokey.1 install-info.1 makeinfo.1 texindex.1 texi2dvi.1 \
           info.5 texinfo.5

# Use the programs built in our distribution, taking account of possible
# cross-compiling.
MAKEINFO = $(top_builddir)/$(native_tools)/makeinfo/makeinfo
INSTALL_INFO = $(top_builddir)/$(native_tools)/util/install-info

TXI_XLATE = txi-cs.tex txi-de.tex txi-en.tex txi-es.tex txi-fr.tex \
	    txi-it.tex txi-nl.tex txi-no.tex txi-pl.tex txi-pt.tex txi-tr.tex

# Include our texinfo.tex, not Automake's.
EXTRA_DIST = epsf.tex pdfcolor.tex texinfo.tex \
             fdl.texi \
	     $(man_MANS) $(TXI_XLATE)

if INSTALL_WARNINGS
install-data-local:
	@echo
	@echo "WARNING: If your texmf tree does not already contain"
	@echo "         up-to-date versions, you must install"
	@echo "             texinfo.tex and txi-??.tex manually,"
	@echo "         perhaps in TEXMF/tex/texinfo/,"
	@echo "         where TEXMF is a root of your TeX tree."
	@echo "         See doc/README for some considerations."
	@echo "         You can run make TEXMF=/your/texmf install-tex to do this."
	@echo
	@echo "         You may also need to install epsf.tex and (if you"
	@echo "         use pdftex) pdfcolor.tex in TEXMF/tex/generic/dvips"
	@echo "         and TEXMF/pdftex/plain/misc/pdfcolor.tex respectively,"
	@echo "         if your TeX installation did not include them."
endif

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

# The man pages depend on the --help strings and the version number.
if MAINTAINER_MODE
common_mandeps = $(top_srcdir)/configure.ac
info.1: $(top_srcdir)/info/info.c $(common_mandeps)
	$(HELP2MAN) --name="read Info documents" `basename $@ .1` >$@
install-info.1: $(top_srcdir)/util/install-info.c $(common_mandeps)
	$(HELP2MAN) --name="update info/dir entries" `basename $@ .1` >$@
makeinfo.1: $(top_srcdir)/makeinfo/makeinfo.c $(common_mandeps)
	$(HELP2MAN) --name="translate Texinfo documents" `basename $@ .1` >$@
texi2dvi.1: $(top_srcdir)/util/texi2dvi $(common_mandeps)
	$(HELP2MAN) --name="print Texinfo documents" `basename $@ .1` >$@
texindex.1: $(top_srcdir)/util/texindex.c $(common_mandeps)
	$(HELP2MAN) --name="sort Texinfo index files" `basename $@ .1` >$@
endif

# Do not create info files for distribution.
dist-info:
