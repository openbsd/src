#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:20:05 deraadt Exp $
#	$NetBSD: Makefile,v 1.11 1995/03/21 09:05:14 cgd Exp $

SHELL=	/bin/sh
ED=	../obj/ed

all: check
	@:

check: build test
	@if grep -h '\*\*\*' errs.o scripts.o; then :; else \
		echo "tests completed successfully."; \
	fi

build: mkscripts.sh
	@if [ -f errs.o ]; then :; else \
		echo "building test scripts for $(ED) ..."; \
		$(SHELL) mkscripts.sh $(ED); \
	fi

test: build ckscripts.sh
	@echo testing $(ED) ...
	@$(SHELL) ckscripts.sh $(ED)

clean:
	rm -f *.ed *.red *.[oz] *~
