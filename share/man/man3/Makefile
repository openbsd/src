#	$OpenBSD: Makefile,v 1.4 1999/11/17 04:15:04 millert Exp $
#	@(#)Makefile	8.2 (Berkeley) 12/13/93

MAN=	assert.3 bitstring.3 dlfcn.3 end.3 intro.3 queue.3 stdarg.3
MLINKS+=bitstring.3 bit_alloc.3 bitstring.3 bit_clear.3 \
	bitstring.3 bit_decl.3 bitstring.3 bit_ffc.3 bitstring.3 bit_ffs.3 \
	bitstring.3 bit_nclear.3 bitstring.3 bit_set.3 \
	bitstring.3 bit_size.3  bitstring.3 bit_test.3 bitstring.3 bit_nset.3 \
	bitstring.3 bitstr_size.3
MLINKS+=end.3 edata.3 end.3 etext.3
MLINKS+=queue.3 list_entry.3 queue.3 list_head.3 \
	queue.3 list_head_initializer.3 queue.3 list_first.3 \
	queue.3 list_next.3 queue.3 list_end.3 queue.3 list_init.3 \
	queue.3 list_insert_after.3 queue.3 list_insert_before.3 \
	queue.3 list_insert_head.3 queue.3 list_remove.3 
MLINKS+=queue.3 simpleq_entry.3 queue.3 simpleq_head.3 \
	queue.3 simpleq_head_initializer.3 queue.3 simpleq_first.3 \
	queue.3 simpleq_next.3 queue.3 simpleq_end.3 queue.3 simpleq_init.3 \
	queue.3 simpleq_insert_head.3 queue.3 simpleq_insert_tail.3 \
	queue.3 simpleq_insert_after.3 queue.3 simpleq_remove_head.3
MLINKS+=queue.3 tailq_entry.3 queue.3 tailq_head.3 \
	queue.3 tailq_head_initializer.3 queue.3 tailq_first.3 \
	queue.3 tailq_next.3 queue.3 tailq_end.3 queue.3 tailq_last.3 \
	queue.3 tailq_prev.3 queue.3 tailq_init.3 queue.3 tailq_insert_after.3 \
	queue.3 tailq_insert_before.3 queue.3 tailq_insert_head.3 \
	queue.3 tailq_insert_tail.3 queue.3 tailq_remove.3 
MLINKS+=queue.3 circleq_entry.3 queue.3 circleq_head.3 \
	queue.3 circleq_head_initializer.3 queue.3 circleq_first.3 \
	queue.3 circleq_last.3 queue.3 circleq_end.3 queue.3 circleq_next.3 \
	queue.3 circleq_prev.3 queue.3 circleq_init.3 \
	queue.3 circleq_insert_after.3 queue.3 circleq_insert_before.3 \
	queue.3 circleq_insert_head.3 queue.3 circleq_insert_tail.3 \
	queue.3 circleq_remove.3
MLINKS+=stdarg.3 varargs.3 stdarg.3 va_arg.3 stdarg.3 va_end.3
MLINKS+=stdarg.3 va_start.3
MLINKS+=dlfcn.3 dlopen.3 dlfcn.3 dlclose.3 dlfcn.3 dlsym.3 dlfcn.3 dlctl.3 \
	dlfcn.3 dlerror.3

.include <bsd.prog.mk>
