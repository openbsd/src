#	$OpenBSD: Makefile,v 1.2 2001/01/29 01:58:34 niklas Exp $


SRCS=	vi.1
DOCS=	vi.0 vi.0.ps

all: ${DOCS}

vi.0: vi.1
	groff -man -Tascii < vi.1 > $@
vi.0.ps: vi.1
	groff -man < vi.1 > $@

clean:
	rm -f ${DOCS}
