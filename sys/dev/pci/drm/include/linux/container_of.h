/* Public domain. */

#ifndef _LINUX_CONTAINER_OF_H
#define _LINUX_CONTAINER_OF_H

#define container_of(ptr, type, member) ({			\
	const __typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define typeof_member(s, e)	typeof(((s *)0)->e)

#endif
