#define PERL_NO_GET_CONTEXT
#define PERL_EXT
#define PERL_EXT_LANGINFO

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef I_LANGINFO
#   define __USE_GNU 1 /* Enables YESSTR, otherwise only __YESSTR. */
#   include <langinfo.h>
#else
#   include <perl_langinfo.h>
#endif

#include "const-c.inc"

MODULE = I18N::Langinfo	PACKAGE = I18N::Langinfo

PROTOTYPES: ENABLE

INCLUDE: const-xs.inc

SV*
langinfo(code)
	int	code
  PREINIT:
        const   char * value;
        STRLEN  len;
  PROTOTYPE: _
  CODE:
#ifdef HAS_NL_LANGINFO
	if (code < 0) {
	    SETERRNO(EINVAL, LIB_INVARG);
	    RETVAL = &PL_sv_undef;
	} else
#endif
        {
            value = Perl_langinfo(code);
            len = strlen(value);
            RETVAL = newSVpvn(Perl_langinfo(code), len);

            /* Now see if the UTF-8 flag should be turned on */
#ifdef USE_LOCALE_CTYPE     /* No utf8 strings if not using LC_CTYPE */

            /* If 'value' is ASCII or not legal UTF-8, the flag doesn't get
             * turned on, so skip the followin code */
            if (is_utf8_non_invariant_string((U8 *) value, len)) {
                int category;

                /* Check if the locale is a UTF-8 one.  The returns from
                 * Perl_langinfo() are in different locale categories, so check the
                 * category corresponding to this item */
                switch (code) {

                    /* This should always return ASCII, so we could instead
                     * legitimately panic here, but soldier on */
                    case CODESET:
                        category = LC_CTYPE;
                        break;

                    case RADIXCHAR:
                    case THOUSEP:
#  ifdef USE_LOCALE_NUMERIC
                        category = LC_NUMERIC;
#  else
                        /* Not ideal, but the best we can do on such a platform */
                        category = LC_CTYPE;
#  endif
                        break;

                    case CRNCYSTR:
#  ifdef USE_LOCALE_MONETARY
                        category = LC_MONETARY;
#  else
                        category = LC_CTYPE;
#  endif
                        break;

                    default:
#  ifdef USE_LOCALE_TIME
                        category = LC_TIME;
#  else
                        category = LC_CTYPE;
#  endif
                        break;
                }

                /* Here the return is legal UTF-8.  Turn on that flag if the
                 * locale is UTF-8.  (Otherwise, could just be a coincidence.)
                 * */
                if (_is_cur_LC_category_utf8(category)) {
                    SvUTF8_on(RETVAL);
                }
            }
#endif /* USE_LOCALE_CTYPE */
        }

  OUTPUT:
        RETVAL
