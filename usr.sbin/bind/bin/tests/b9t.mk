# Copyright (C) 1999-2001  Internet Software Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# $ISC: b9t.mk,v 1.10 2001/01/09 21:40:53 bwelling Exp $

#
# makefile to configure, build and test bind9
# this is run by cron (user wpk) on aa, sol, irix, hp and aix
# $PLATFORM is set in the environment by cron
#

BASE	= /build
BDIR	= $(BASE)
MODULE	= bind9
SDIR	= $(HOME)/b9t/src

# as it says
CVSROOT	= /proj/cvs/isc

# where the config, build and test output goes
RDIR	= /proj/build-reports/$(MODULE)/hosts/$(PLATFORM)

all:	clobber populate config build test

clobber:
	@echo "CLOBBBER `date`"
	@if test ! -d $(BDIR) ; then mkdir -p $(BDIR) > /dev/null 2>&1 ; fi
	@( cd $(BDIR) && rm -fr $(MODULE) )
	@echo "DONE `date`"

populate:
	@echo "POPULATE `date`"
	@( cd $(BDIR) && tar -xvf $(SDIR)/$(MODULE).tar ) > $(RDIR)/.populate 2>&1
	@echo "DONE `date`"

config:
	@echo "CONFIG `date`"
	@( cd $(BDIR)/$(MODULE) && ./configure ) > $(RDIR)/.config 2>&1
	@echo "DONE `date`"

build:
	@echo "BUILD `date`"
	@( cd $(BDIR)/$(MODULE) && $(MAKE) -k all ) > $(RDIR)/.build 2>&1
	@echo "DONE `date`"

test:
	@echo "TEST `date`"
	-@( cd $(BDIR)/$(MODULE)/bin/tests && $(MAKE) test ) > $(RDIR)/.test 2>&1
	@echo "DONE `date`"

tarsrc:
	@echo "TARSRC `date`"
	@rm -fr $(SDIR)/$(MODULE)
	@( cd $(SDIR) && cvs -d $(CVSROOT) checkout $(MODULE) && tar -cvf $(MODULE).tar $(MODULE) )
	@echo "DONE `date`"

