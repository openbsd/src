#	$OpenBSD: Makefile.inc,v 1.7 2014/03/26 18:13:15 eric Exp $

# asr sources
.PATH: ${LIBCSRCDIR}/asr

SRCS+=	asr.c asr_debug.c asr_utils.c \
	res_send_async.c res_search_async.c getrrsetbyname_async.c \
	gethostnamadr_async.c getnetnamadr_async.c \
	getaddrinfo_async.c getnameinfo_async.c

MAN+=	asr_run.3

MLINKS+= \
	asr_run.3	asr_run_sync.3		\
	asr_run.3	asr_abort.3		\
	asr_run.3	res_send_async.3	\
	asr_run.3	res_query_async.3	\
	asr_run.3	res_search_async.3	\
	asr_run.3	getrrsetbyname_async.3	\
	asr_run.3	gethostbyname_async.3	\
	asr_run.3	gethostbyname2_async.3	\
	asr_run.3	gethostbyaddr_async.3	\
	asr_run.3	getnetbyname_async.3	\
	asr_run.3	getnetbyaddr_async.3	\
	asr_run.3	getaddrinfo_async.3	\
	asr_run.3	getnameinfo_async.3
