/* crypt.h (dummy version) -- do not perform encryption
 * Hardly worth copyrighting :-)
 *
 *	$Id: crypt.h,v 1.1.1.1 1995/10/18 08:40:52 deraadt Exp $
 */

#ifdef CRYPT
#  undef CRYPT      /* dummy version */
#endif

#define RAND_HEAD_LEN  12  /* length of encryption random header */

#define zencode
#define zdecode
