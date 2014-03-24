/*    mydtrace.h
 *
 *    Copyright (C) 2008, 2010, 2011 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 *	Provides macros that wrap the various DTrace probes we use. We add
 *	an extra level of wrapping to encapsulate the _ENABLED tests.
 */

#if defined(USE_DTRACE) && defined(PERL_CORE)

#  include "perldtrace.h"

#  if defined(STAP_PROBE_ADDR) && !defined(DEBUGGING)

/* SystemTap 1.2 uses a construct that chokes on passing a char array
 * as a char *, in this case hek_key in struct hek.  Workaround it
 * with a temporary.
 */

#    define ENTRY_PROBE(func, file, line, stash)  	\
    if (PERL_SUB_ENTRY_ENABLED()) {	        	\
	const char *tmp_func = func;			\
	PERL_SUB_ENTRY(tmp_func, file, line, stash); 	\
    }

#    define RETURN_PROBE(func, file, line, stash) 	\
    if (PERL_SUB_RETURN_ENABLED()) {    		\
	const char *tmp_func = func;			\
	PERL_SUB_RETURN(tmp_func, file, line, stash);	\
    }

#    define LOADING_FILE_PROBE(name) 	                        \
    if (PERL_LOADING_FILE_ENABLED()) {    		        \
	const char *tmp_name = name;			\
	PERL_LOADING_FILE(tmp_name);	                        \
    }

#    define LOADED_FILE_PROBE(name) 	                        \
    if (PERL_LOADED_FILE_ENABLED()) {    		        \
	const char *tmp_name = name;			\
	PERL_LOADED_FILE(tmp_name);	                        \
    }

#  else

#    define ENTRY_PROBE(func, file, line, stash) 	\
    if (PERL_SUB_ENTRY_ENABLED()) {	        	\
	PERL_SUB_ENTRY(func, file, line, stash); 	\
    }

#    define RETURN_PROBE(func, file, line, stash)	\
    if (PERL_SUB_RETURN_ENABLED()) {    		\
	PERL_SUB_RETURN(func, file, line, stash); 	\
    }

#    define LOADING_FILE_PROBE(name)	                        \
    if (PERL_LOADING_FILE_ENABLED()) {    		        \
	PERL_LOADING_FILE(name); 	                                \
    }

#    define LOADED_FILE_PROBE(name)	                        \
    if (PERL_LOADED_FILE_ENABLED()) {    		        \
	PERL_LOADED_FILE(name); 	                                \
    }

#  endif

#  define OP_ENTRY_PROBE(name)	                \
    if (PERL_OP_ENTRY_ENABLED()) {    		        \
	PERL_OP_ENTRY(name); 	                        \
    }

#  define PHASE_CHANGE_PROBE(new_phase, old_phase)      \
    if (PERL_PHASE_CHANGE_ENABLED()) {                  \
	PERL_PHASE_CHANGE(new_phase, old_phase);        \
    }

#else

/* NOPs */
#  define ENTRY_PROBE(func, file, line, stash)
#  define RETURN_PROBE(func, file, line, stash)
#  define PHASE_CHANGE_PROBE(new_phase, old_phase)
#  define OP_ENTRY_PROBE(name)
#  define LOADING_FILE_PROBE(name)
#  define LOADED_FILE_PROBE(name)

#endif

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
