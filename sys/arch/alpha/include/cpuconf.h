/*	$NetBSD: cpuconf.h,v 1.1 1996/11/12 05:14:40 cgd Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct cpusw {
	const char	*family, *option;
	int		present;
	const char	*(*model_name) __P((void));
	void		(*cons_init) __P((void));
	const char	*(*iobus_name) __P((void));
	void		(*device_register) __P((struct device *dev,
			    void *aux));
};

#define	CONCAT(a,b)		__CONCAT(a,b)

#define	cpu_fn_name(p,f)	CONCAT(CONCAT(p,_),f)

#define	cpu_decl(p)							\
	extern const char	*cpu_fn_name(p,model_name) __P((void));	\
	extern void		cpu_fn_name(p,cons_init) __P((void));	\
	extern const char	*cpu_fn_name(p,iobus_name) __P((void));	\
	extern void		cpu_fn_name(p,device_register)		\
				    __P((struct device *, void*));

#define	cpu_unknown()	{ NULL, NULL, 0, }
#define cpu_notdef(f)	{ f, NULL, 0 }

#define	cpu_option_string(o)	__STRING(o)
#define	cpu_option_present(o)	(CONCAT(N,o) > NULL)
#define	cpu_function_init(o,p,f)					\
	    (cpu_option_present(o) ? cpu_fn_name(p,f) : 0)
#define	cpu_init(f,o,p)							\
	{								\
		f, cpu_option_string(o) , cpu_option_present(o),	\
		cpu_function_init(o,p,model_name),			\
		cpu_function_init(o,p,cons_init),			\
		cpu_function_init(o,p,iobus_name),			\
		cpu_function_init(o,p,device_register),			\
	}

#ifdef _KERNEL
extern const struct cpusw cpusw[];
extern const int ncpusw;
#endif
