#ifndef UUENCODE_H
#define UUENCODE_H
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>

#define	uuencode(src, srclength, target,targsize) \
	__b64_ntop(src, srclength, target, targsize)
#define	uudecode(src, target, targsize) \
	__b64_pton(src, target, targsize)
#endif
