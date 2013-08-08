#	$OpenBSD: Makefile.inc,v 1.6 2013/08/08 06:55:42 jmc Exp $

# asr sources
.PATH: ${LIBCSRCDIR}/asr

SRCS+=	asr.c asr_debug.c asr_utils.c \
	res_send_async.c res_search_async.c getrrsetbyname_async.c \
	gethostnamadr_async.c getnetnamadr_async.c \
	getaddrinfo_async.c getnameinfo_async.c

MLINKS+ = \
	asr_resolver.3	asr_resolver_done.3	\
	asr_resolver.3	asr_async_run.3		\
	asr_resolver.3	asr_async_run_sync.3	\
	asr_resolver.3	asr_async_abort.3		\
	asr_resolver.3	res_send_async.3	\
	asr_resolver.3	res_query_async.3	\
	asr_resolver.3	res_search_async.3	\
	asr_resolver.3	getrrsetbyname_async.3	\
	asr_resolver.3	gethostbyname_async.3	\
	asr_resolver.3	gethostbyname2_async.3	\
	asr_resolver.3	gethostbyaddr_async.3	\
	asr_resolver.3	getnetbyname_async.3	\
	asr_resolver.3	getnetbyaddr_async.3	\
	asr_resolver.3	getaddrinfo_async.3	\
	asr_resolver.3	getnameinfo_async.3
