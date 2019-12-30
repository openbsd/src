/* This is a placeholder file for symbols that should be exported
 * into config_h.SH and Porting/Glossary. See also metaconfig.SH
 *
 * First version was created from the part in handy.h
 * H.Merijn Brand 21 Dec 2010 (Tux)
 *
 * Mentioned variables are forced to be included into config_h.SH
 * as they are only included if meta finds them referenced. That
 * implies that noone can use them unless they are available and
 * they won't be available unless used. When new symbols are probed
 * in Configure, this is the way to force them into availability.
 *
 * Symbols should only be here temporarily. Once they are actually used,
 * they should be removed from here.
 *
 * HAS_BUILTIN_ADD_OVERFLOW
 * HAS_BUILTIN_MUL_OVERFLOW
 * HAS_BUILTIN_SUB_OVERFLOW
 * HAS_LOCALECONV_L
 * HAS_MBRLEN
 * HAS_MBRTOWC
 * HAS_NANOSLEEP
 * HAS_STRTOD_L
 * HAS_STRTOLD_L
 * I_WCHAR
 * I_WCTYPE
 * HAS_TOWLOWER
 * HAS_TOWUPPER
 * SETLOCALE_ACCEPTS_ANY_LOCALE_NAME
 */
