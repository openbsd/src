#	$OpenBSD: Makefile,v 1.22 2011/07/03 16:08:40 matthew Exp $
#	@(#)Makefile	8.2 (Berkeley) 12/13/93

MAN=	assert.3 bitstring.3 CMSG_DATA.3 dlfcn.3 dl_iterate_phdr.3 end.3 \
	intro.3 offsetof.3 queue.3 stdarg.3 sysexits.3 tree.3
MLINKS+=bitstring.3 bit_alloc.3 bitstring.3 bit_clear.3 \
	bitstring.3 bit_decl.3 bitstring.3 bit_ffc.3 bitstring.3 bit_ffs.3 \
	bitstring.3 bit_nclear.3 bitstring.3 bit_set.3 \
	bitstring.3 bit_size.3  bitstring.3 bit_test.3 bitstring.3 bit_nset.3 \
	bitstring.3 bitstr_size.3
MLINKS+=CMSG_DATA.3 CMSG_FIRSTHDR.3 CMSG_DATA.3 CMSG_LEN.3 \
	CMSG_DATA.3 CMSG_NXTHDR.3 CMSG_DATA.3 CMSG_SPACE.3
MLINKS+=end.3 edata.3 end.3 etext.3
MLINKS+=queue.3 SLIST_ENTRY.3 queue.3 SLIST_HEAD.3 \
	queue.3 SLIST_HEAD_INITIALIZER.3 queue.3 SLIST_FIRST.3 \
	queue.3 SLIST_NEXT.3 queue.3 SLIST_END.3 queue.3 SLIST_EMPTY.3 \
	queue.3 SLIST_FOREACH.3 queue.3 SLIST_FOREACH_PREVPTR.3 \
	queue.3 SLIST_INIT.3 \
	queue.3 SLIST_INSERT_AFTER.3 queue.3 SLIST_INSERT_HEAD.3 \
	queue.3 SLIST_REMOVE_HEAD.3 queue.3 SLIST_REMOVE_NEXT.3 \
	queue.3 SLIST_REMOVE.3
MLINKS+=queue.3 LIST_ENTRY.3 queue.3 LIST_HEAD.3 \
	queue.3 LIST_HEAD_INITIALIZER.3 queue.3 LIST_FIRST.3 \
	queue.3 LIST_NEXT.3 queue.3 LIST_END.3 \
	queue.3 LIST_EMPTY.3 queue.3 LIST_FOREACH.3 \
	queue.3 LIST_INIT.3 \
	queue.3 LIST_INSERT_AFTER.3 queue.3 LIST_INSERT_BEFORE.3 \
	queue.3 LIST_INSERT_HEAD.3 queue.3 LIST_REMOVE.3 \
	queue.3 LIST_REPLACE.3
MLINKS+=queue.3 SIMPLEQ_ENTRY.3 queue.3 SIMPLEQ_HEAD.3 \
	queue.3 SIMPLEQ_HEAD_INITIALIZER.3 queue.3 SIMPLEQ_FIRST.3 \
	queue.3 SIMPLEQ_NEXT.3 queue.3 SIMPLEQ_END.3 \
	queue.3 SIMPLEQ_EMPTY.3 queue.3 SIMPLEQ_FOREACH.3 \
	queue.3 SIMPLEQ_INIT.3 \
	queue.3 SIMPLEQ_INSERT_HEAD.3 queue.3 SIMPLEQ_INSERT_TAIL.3 \
	queue.3 SIMPLEQ_INSERT_AFTER.3 queue.3 SIMPLEQ_REMOVE_HEAD.3 \
	queue.3 SIMPLEQ_REMOVE_NEXT.3
MLINKS+=queue.3 TAILQ_ENTRY.3 queue.3 TAILQ_HEAD.3 \
	queue.3 TAILQ_HEAD_INITIALIZER.3 queue.3 TAILQ_FIRST.3 \
	queue.3 TAILQ_NEXT.3 queue.3 TAILQ_END.3 queue.3 TAILQ_LAST.3 \
	queue.3 TAILQ_PREV.3 queue.3 TAILQ_EMPTY.3 \
	queue.3 TAILQ_FOREACH.3 queue.3 TAILQ_FOREACH_REVERSE.3 \
	queue.3 TAILQ_INIT.3 \
	queue.3 TAILQ_INSERT_AFTER.3 queue.3 TAILQ_INSERT_BEFORE.3 \
	queue.3 TAILQ_INSERT_HEAD.3 queue.3 TAILQ_INSERT_TAIL.3 \
	queue.3 TAILQ_REMOVE.3 queue.3 TAILQ_REPLACE.3
MLINKS+=queue.3 CIRCLEQ_ENTRY.3 queue.3 CIRCLEQ_HEAD.3 \
	queue.3 CIRCLEQ_HEAD_INITIALIZER.3 queue.3 CIRCLEQ_FIRST.3 \
	queue.3 CIRCLEQ_LAST.3 queue.3 CIRCLEQ_END.3 queue.3 CIRCLEQ_NEXT.3 \
	queue.3 CIRCLEQ_PREV.3 queue.3 CIRCLEQ_EMPTY.3 \
	queue.3 CIRCLEQ_FOREACH.3 queue.3 CIRCLEQ_FOREACH_REVERSE.3 \
	queue.3 CIRCLEQ_INIT.3 \
	queue.3 CIRCLEQ_INSERT_AFTER.3 queue.3 CIRCLEQ_INSERT_BEFORE.3 \
	queue.3 CIRCLEQ_INSERT_HEAD.3 queue.3 CIRCLEQ_INSERT_TAIL.3 \
	queue.3 CIRCLEQ_REMOVE.3 queue.3 CIRCLEQ_REPLACE.3
MLINKS+=stdarg.3 varargs.3 stdarg.3 va_arg.3 stdarg.3 va_end.3
MLINKS+=stdarg.3 va_start.3 stdarg.3 va_copy.3
MLINKS+=dlfcn.3 dlopen.3 dlfcn.3 dlclose.3 dlfcn.3 dlsym.3 dlfcn.3 dlctl.3 \
	dlfcn.3 dlerror.3 dlfcn.3 dladdr.3
MLINKS+=tree.3 SPLAY_PROTOTYPE.3 tree.3 SPLAY_GENERATE.3 \
	tree.3 SPLAY_ENTRY.3 tree.3 SPLAY_HEAD.3 \
	tree.3 SPLAY_INITIALIZER.3 tree.3 SPLAY_ROOT.3 \
	tree.3 SPLAY_EMPTY.3 tree.3 SPLAY_NEXT.3 \
	tree.3 SPLAY_MIN.3 tree.3 SPLAY_MAX.3 tree.3 SPLAY_FIND.3 \
	tree.3 SPLAY_LEFT.3 tree.3 SPLAY_RIGHT.3 tree.3 SPLAY_INIT.3 \
	tree.3 SPLAY_INSERT.3 tree.3 SPLAY_REMOVE.3 \
	tree.3 SPLAY_FOREACH.3
MLINKS+=tree.3 RB_PROTOTYPE.3 tree.3 RB_PROTOTYPE_STATIC.3 \
	tree.3 RB_GENERATE.3 tree.3 RB_GENERATE_STATIC.3 tree.3 RB_ENTRY.3 \
	tree.3 RB_HEAD.3 tree.3 RB_INITIALIZER.3 tree.3 RB_ROOT.3 \
	tree.3 RB_EMPTY.3 tree.3 RB_NEXT.3 tree.3 RB_PREV.3 \
	tree.3 RB_MIN.3 tree.3 RB_MAX.3 tree.3 RB_FIND.3 tree.3 RB_NFIND.3 \
	tree.3 RB_LEFT.3 tree.3 RB_RIGHT.3 tree.3 RB_PARENT.3 \
	tree.3 RB_INIT.3 tree.3 RB_INSERT.3 tree.3 RB_REMOVE.3 \
	tree.3 RB_FOREACH.3 tree.3 RB_FOREACH_REVERSE.3

.include <bsd.prog.mk>
