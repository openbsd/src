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

# $ISC: b8t.mk,v 1.8 2001/01/09 21:40:50 bwelling Exp $

#
# bind 8 multi-host make
# PLATFORM set in the environment by cron
#

MODULE	= bind
BASE	= /build
BDIR	= $(BASE)/$(MODULE)
RDIR	= /proj/build-reports/bind8/hosts/$(PLATFORM)
SDIR	= $(HOME)/b8t/src
CVSROOT	= /proj/cvs/isc

all:	clobber populate config build

clobber:
	@echo "CLOBBBER `date`"
	@if test ! -d $(BASE) ; then mkdir -p $(BASE) ; fi
	@rm -fr $(BDIR)
	@echo "DONE `date`"

populate:
	@echo "POPULATE `date`"
	@( cd $(BASE) && tar -xvf $(SDIR)/$(MODULE).tar ) > $(RDIR)/.populate 2>&1
	@echo "DONE `date`"

tarsrc:
	@echo "TARSRC `date`"
	@rm -fr $(SDIR)/$(MODULE)
	@( cd $(SDIR) && cvs -d $(CVSROOT) checkout $(MODULE) )
	@( cd $(SDIR) && tar -cvf $(MODULE).tar $(MODULE) )
	@echo "DONE `date`"

config:
	@echo "CONFIG `date`"
	@( cd $(BDIR)/src && make SRC=$(BDIR)/src DST=$(BDIR)/dst links ) > $(RDIR)/.config 2>&1
	@echo "DONE `date`"

build:
	@echo "BUILD `date`"
	@( cd $(BDIR)/dst && make -k clean depend all ) > $(RDIR)/.build 2>&1
	@echo "DONE `date`"

test:
	@echo "TEST `date`"
	@touch $(RDIR)/.test
	@echo "DONE `date`"
