/*	$OpenBSD: extattr.h,v 1.2 2002/05/16 16:42:15 drahn Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: extattr.h,v 1.6 2001/03/31 16:20:05 rwatson Exp $
 */
/*
 * Userland/kernel interface for Extended File System Attributes
 *
 * The POSIX.1e implementation page may be reached at:
 *   http://www.watson.org/fbsd-hardening/posix1e/
 */

#ifndef _SYS_EXTATTR_H_
#define	_SYS_EXTATTR_H_

#define	EXTATTR_NAMESPACE_USER		0x00000001
#define	EXTATTR_NAMESPACE_USER_STRING	"user"
#define	EXTATTR_NAMESPACE_SYSTEM	0x00000002
#define	EXTATTR_NAMESPACE_SYSTEM_STRING	"system"

#ifdef _KERNEL

#define	EXTATTR_MAXNAMELEN	NAME_MAX

#else
#include <sys/cdefs.h>

__BEGIN_DECLS
int	extattrctl(const char *_path, int _cmd, const char *_filename,
	    int _attrnamespace, const char *_attrname);
int	extattr_delete_fd(int _fd, int _attrnamespace, const char *_attrname);
int	extattr_delete_file(const char *_path, int _attrnamespace,
	    const char *_attrname);
ssize_t	extattr_get_fd(int _fd, int _attrnamespace, const char *_attrname,
	    void *_data, size_t _nbytes);
ssize_t	extattr_get_file(const char *_path, int _attrnamespace,
	    const char *_attrname, void *_data, size_t _nbytes);
int	extattr_set_fd(int _fd, int _attrnamespace, const char *_attrname,
	    const void *_data, size_t _nbytes);
int	extattr_set_file(const char *_path, int _attrnamespace,
	    const char *_attrname, const void *_data, size_t _nbytes);
int	 extattr_namespace_to_string(int attrnamespace, char **string);
int	 extattr_string_to_namespace(const char *string, int *attrnamespace);

__END_DECLS

#endif /* !_KERNEL */
#endif /* !_SYS_EXTATTR_H_ */
