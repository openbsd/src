#	$OpenBSD: Makefile,v 1.5 2004/01/14 23:40:01 millert Exp $


DIR=	usd/13.viref
SRCS=	vi.ref ex.cmd.roff set.opt.roff vi.cmd.roff ref.so
EXTRA=	merge.awk spell.ok
MACROS=	-me
CLEANFILES+=vi.ref.txt index index.so

paper.ps: vi.ref index.so
	soelim vi.ref | ${TBL} | ${ROFF} -U > ${.TARGET}

vi.ref.txt: vi.ref index.so
	soelim vi.ref | ${TBL} | groff -U ${MACROS} -Tascii > $@

index.so: vi.ref
	# Build index.so, side-effect of building the paper.
	sed '/^\.so index.so/s/^/.\\\" /' vi.ref | soelim | \
	    ${TBL} | ${ROFF} -U > /dev/null
	sed -e 's/MINUSSIGN/\\-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f merge.awk | \
	sed -e 's/__SPACE/ /g' > index.so
	rm -f index

.include <bsd.doc.mk>
