/* $OpenBSD: config_helper.h,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */
/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
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
 */
#ifndef CONFIG_HELPER_H
#define CONFIG_HELPER_H 1

#ifdef	CONFIG_HELPER_NO_INLINE
#define	INLINE
#else
#define	INLINE inline
#endif

#define CONFIG_DECL(fn_pre, basetype, prop)				\
	INLINE const char *fn_pre ## _str(basetype *, const char *);	\
	INLINE int fn_pre ## _int(basetype *, const char *, int);	\
	INLINE int fn_pre ## _str_equal(basetype *, const char *,	\
	    const char *, int);						\
	INLINE int fn_pre ## _str_equali(basetype *, const char *,	\
	    const char *, int);

#define PREFIXED_CONFIG_DECL(fn_pre, basetype, prop, prefix)		\
	INLINE const char *fn_pre ## _str(basetype *, const char *);	\
	INLINE int fn_pre ## _int(basetype *, const char *, int);	\
	INLINE int fn_pre ## _str_equal(basetype *, const char *,	\
	    const char *, int);						\
	INLINE int fn_pre ## _str_equali(basetype *, const char *,	\
	    const char *, int);

#define NAMED_PREFIX_CONFIG_DECL(fn_pre, basetype, prop, prefix, name)	\
	INLINE const char *fn_pre ## _str(basetype *, const char *);	\
	INLINE int fn_pre ## _int(basetype *, const char *, int);	\
	INLINE int fn_pre ## _str_equal(basetype *, const char *,	\
	    const char *, int);						\
	INLINE int fn_pre ## _str_equali(basetype *, const char *,	\
	    const char *, int);

#define CONFIG_FUNCTIONS(fn_pre, basetype, prop)			\
	INLINE const char *						\
	fn_pre ## _str(basetype *_this, const char *confKey) {		\
		return config_str(_this->prop, confKey);		\
	}								\
	INLINE int							\
	fn_pre ## _int(basetype *_this, const char *confKey, 		\
		    int defVal) {					\
		return config_int(_this->prop, confKey, defVal);	\
	}								\
	int 								\
	fn_pre ## _str_equal(basetype *_this, const char *confKey,	\
	    const char *confVal, int defVal) {				\
		return config_str_equal(_this->prop, confKey, confVal,	\
		    defVal);						\
	}								\
	int 								\
	fn_pre ## _str_equali(basetype *_this, const char *confKey,	\
	    const char *confVal, int defVal) {				\
		return config_str_equali(_this->prop, confKey, confVal,	\
		    defVal);						\
	}
#define PREFIXED_CONFIG_FUNCTIONS(fn_pre, basetype, prop, prefix)	\
	INLINE const char *						\
	fn_pre ## _str(basetype *_this, const char *confKey) {		\
		return config_prefixed_str(_this->prop, _this->prefix,	\
		    confKey);						\
	}								\
	INLINE int							\
	fn_pre ## _int(basetype *_this, const char *confKey, 		\
		    int defVal) {					\
		return config_prefixed_int(_this->prop, _this->prefix, 	\
		    confKey, defVal);					\
	}								\
	int 								\
	fn_pre ## _str_equal(basetype *_this, const char *confKey,	\
	    const char *confVal, int defVal) {				\
		return config_prefixed_str_equal(_this->prop,		\
		    _this->prefix, confKey, confVal, defVal);		\
	}								\
	int 								\
	fn_pre ## _str_equali(basetype *_this, const char *confKey,	\
	    const char *confVal, int defVal) {				\
		return config_prefixed_str_equali(_this->prop,		\
		    _this->prefix, confKey, confVal, defVal);		\
	}

#define NAMED_PREFIX_CONFIG_FUNCTIONS(fn_pre, basetype, prop, prefix, 	\
	    name) 							\
	INLINE const char *						\
	fn_pre ## _str(basetype *_this, const char *confKey) {		\
		return config_named_prefix_str(_this->prop,		\
		    prefix, _this->name,				\
		    confKey);						\
	}								\
	INLINE int							\
	fn_pre ## _int(basetype *_this, const char *confKey, 		\
		    int defVal) {					\
		return config_named_prefix_int(_this->prop, prefix, 	\
		    _this->name, confKey, defVal);			\
	}								\
	int 								\
	fn_pre ## _str_equal(basetype *_this, const char *confKey,	\
	    const char *confVal, int defVal) {				\
		return config_named_prefix_str_equal(_this->prop,	\
		    prefix, _this->name, confKey, confVal, defVal);	\
	}								\
	int 								\
	fn_pre ## _str_equali(basetype *_this, const char *confKey,	\
	    const char *confVal, int defVal) {				\
		return config_named_prefix_str_equali(_this->prop,	\
		    prefix, _this->name, confKey, confVal, defVal);	\
	}

#ifdef __cplusplus
extern "C" {
#endif

const char  *config_key_prefix (const char *, const char *);
const char  *config_str (struct properties *, const char *);
int         config_int (struct properties *, const char *, int);
int         config_str_equal (struct properties *, const char *, const char *, int);
int         config_str_equali (struct properties *, const char *, const char *, int);
const char  *config_prefixed_str (struct properties *, const char *, const char *);
int         config_prefixed_int (struct properties *, const char *, const char *, int);
int         config_prefixed_str_equal (struct properties *, const char *, const char *, const char *, int);
int         config_prefixed_str_equali (struct properties *, const char *, const char *, const char *, int);
const char  *config_named_prefix_str (struct properties *, const char *, const char *, const char *);
int         config_named_prefix_int (struct properties *, const char *, const char *, const char *, int);
int         config_named_prefix_str_equal (struct properties *, const char *, const char *, const char *, const char *, int);
int         config_named_prefix_str_equali (struct properties *, const char *, const char *, const char *, const char *, int);

#ifdef __cplusplus
}
#endif
#endif
