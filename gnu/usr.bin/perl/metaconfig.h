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
 * BOOTSTRAP_CHARSET
 * CHARBITS
 * HAS_ASCTIME64
 * HAS_CTIME64
 * HAS_DIFFTIME64
 * HAS_GMTIME64
 * HAS_ISBLANK
 * HAS_LOCALTIME64
 * HAS_IP_MREQ
 * HAS_IP_MREQ_SOURCE
 * HAS_IPV6_MREQ
 * HAS_IPV6_MREQ_SOURCE
 * HAS_MKTIME64
 * HAS_PRCTL
 * HAS_PSEUDOFORK
 * HAS_TIMEGM
 * HAS_SOCKADDR_IN6
 * I16SIZE
 * I64SIZE
 * I8SIZE
 * LOCALTIME_R_NEEDS_TZSET
 * U8SIZE
 * USE_KERN_PROC_PATHNAME
 * USE_NSGETEXECUTABLEPATH
 *
 */
