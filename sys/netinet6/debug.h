/*
%%% portions-copyright-nrl-95
Portions of this software are Copyright 1995-1998 by Randall Atkinson,
Ronald Lee, Daniel McDonald, Bao Phan, and Chris Winters. All Rights
Reserved. All rights under this copyright have been assigned to the US
Naval Research Laboratory (NRL). The NRL Copyright Notice and License
Agreement Version 1.1 (January 17, 1995) applies to these portions of the
software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

*/

#ifndef _SYS_DEBUG_H
#define _SYS_DEBUG_H 1

#ifdef DEBUG_NRL_SYS
#include <sys/osdep.h>
#endif /* DEBUG_NRL_SYS */
#ifdef DEBUG_NRL_NETINET6
#include <netinet6/osdep.h>
#endif /* DEBUG_NRL_NETINET6 */

/* Non-ANSI compilers don't stand a chance. You PROBABLY need GNU C. */
#ifndef __STDC__
#error An ANSI C compiler is required here.
#endif /* __STDC__ */

#ifndef _KERN_DEBUG_GENERIC_C
extern int debug_level;
#endif /* _KERN_DEBUG_GENERIC_DEBUG_C */

/* Debugging levels */

#define __DEBUG_LEVEL_ALL (INT_MAX-1)  /* Report all messages. */
#define __DEBUG_LEVEL_NONE 0          /* Report no messages.  */

#define __DEBUG_LEVEL_CRITICAL 3
#define __DEBUG_LEVEL_ERROR 7
#define __DEBUG_LEVEL_MAJOREVENT 10
#define __DEBUG_LEVEL_EVENT 15
#define __DEBUG_LEVEL_GROSSEVENT 20
#define __DEBUG_LEVEL_FINISHED 1000

/* Compatibility macros */

#define __DEBUG_LEVEL_MAJOR_EVENT __DEBUG_LEVEL_MAJOREVENT
#define __DEBUG_LEVEL_GROSS_EVENT __DEBUG_LEVEL_GROSSEVENT
#define __DEBUG_LEVEL_IDL_CRITICAL __DEBUG_LEVEL_CRITICAL
#define __DEBUG_LEVEL_IDL_ERROR __DEBUG_LEVEL_ERROR
#define __DEBUG_LEVEL_IDL_MAJOR_EVENT __DEBUG_LEVEL_MAJOREVENT
#define __DEBUG_LEVEL_IDL_EVENT __DEBUG_LEVEL_EVENT
#define __DEBUG_LEVEL_IDL_GROSS_EVENT __DEBUG_LEVEL_GROSSEVENT
#define __DEBUG_LEVEL_IDL_FINISHED __DEBUG_LEVEL_FINISHED

/* Unless you have optimization turned off and your compiler is drain bamaged,
   this will turn in to a syntactically inert no-op - cmetz */
#define __DEBUG_NOP do { } while (0)

#ifdef DEBUG_NRL
/*
 * Make sure argument for DPRINTF is in parentheses.
 *
 * For both DPRINTF and DDO, and attempt was made to make both macros
 * be usable as normal C statments.  There is a small amount of compiler
 * trickery (if-else clauses with effectively null statements), which may
 * cause a few compilers to complain.
 */

#ifndef __GENERIC_DEBUG_LEVEL
#define __GENERIC_DEBUG_LEVEL debug_level
#endif /* __GENERIC_DEBUG_LEVEL */

/*
 * DPRINTF() is a general printf statement.  The "arg" is literally what
 * would follow the function name printf, which means it has to be in
 * parenthesis.  Unlimited arguments can be used this way.
 *
 * EXAMPLE:
 *        DPRINTF(IDL_MAJOR_EVENT,("Hello, world.  IP version %d.\n",vers));
 */
#undef DPRINTF
#define DPRINTF(lev,arg) \
  if (__DEBUG_LEVEL_ ## lev <= __GENERIC_DEBUG_LEVEL) { \
    printf arg; \
  } else \
    __DEBUG_NOP

/*
 * DDO() executes a series of statements at a certain debug level.  The
 * "stmt" argument is a statement in the sense of a "statement list" in a
 * C grammar.  "stmt" does not have to end with a semicolon.
 *
 * EXAMPLE:
 *        DDO(IDL_CRITICAL,dump_ipv6(header), dump_inpcb(inp));
 */
#undef DDO
#define DDO(lev,stmt) \
  if (__DEBUG_LEVEL_ ## lev <= __GENERIC_DEBUG_LEVEL) { \
    stmt ; \
  } else \
    __DEBUG_NOP

/*
 * DP() is a shortcut for DPRINTF().  Basically:
 *
 *        DP(lev, var, fmt)   ==   DPRINTF(IDL_lev, ("var = %fmt\n", var))
 *
 * It is handy for printing single variables without a lot of typing.
 *
 * EXAMPLE:
 *
 *        DP(CRITICAL,length,d);
 * same as DPRINTF(IDL_CRITICAL, ("length = %d\n", length))
 */
#undef DP
#define DP(lev, var, fmt) \
  DPRINTF(lev, (#var " = %" #fmt "\n", var))

#undef DEBUG_STATUS
#if defined(__GNUC__) && (__GNUC__ >= 2) 
#define DEBUG_STATUS debug_status(__FILE__ ":" __FUNCTION__, __LINE__, __builtin_return_address(0))
#else /* defined(__GNUC__) && (__GNUC__ >= 2) */
#define DEBUG_STATUS debug_status(__FILE__, __LINE__, (void *)0)
#endif /* defined(__GNUC__) && (__GNUC__ >= 2) */

/* Call as:

   DS();
*/
#undef DS
#define DS() DPRINTF(IDL_CRITICAL, ("%s\n", DEBUG_STATUS))
#else /* DEBUG_NRL */
#undef DPRINTF
#define DPRINTF(lev,arg) __DEBUG_NOP
#undef DDO
#define DDO(lev, stmt) __DEBUG_NOP
#undef DP
#define DP(x, y, z) __DEBUG_NOP
#undef DS
#define DS() __DEBUG_NOP
#endif /* DEBUG_NRL */

#ifdef DEBUG_MALLOC
void *debug_malloc_malloc(unsigned int n, char *creator);
void debug_malloc_free(void *p);
void debug_malloc_dump(void);
void debug_malloc_flush(void);
#endif /* DEBUG_MALLOC */

#ifdef DEBUG_NRL
char *debug_status(char *filefunction, unsigned int line, void *returnaddress);
void dump_buf_small(void *, int);
void debug_dump_buf(void *, int);
void dump_packet(void *buf, int len);

struct dump_flags {
  int val;
  char *name;
};
void dump_flags(struct dump_flags *, int);

struct sockaddr;
void dump_sockaddr(struct sockaddr *);
void dump_smart_sockaddr(void *);

#ifdef __linux__
struct sk_buff;
void dump_skb(struct sk_buff *);
#endif /* __linux__ */

#ifdef OSDEP_BSD
struct sockaddr_dl;
void dump_sockaddr_dl(struct sockaddr_dl *);
struct mbuf;
void dump_mbuf_flags(struct mbuf *);
void dump_mbuf_hdr(struct mbuf *);
void dump_mbuf(struct mbuf *);
void dump_mchain_hdr(struct mbuf *);
void dump_mchain(struct mbuf *);
void dump_mbuf_tcpdump(struct mbuf *);
struct ifaddr;
void dump_ifa(struct ifaddr *);
struct ifnet;
void dump_ifp(struct ifnet *);
struct route;
void dump_route(struct route *);
struct rtentry;
void dump_rtentry(struct rtentry *);
struct inpcb;
void dump_inpcb(struct inpcb *);
#if __NetBSD__ || __OpenBSD__
struct inpcbtable;
void dump_inpcbs(struct inpcbtable *);
#else /* __NetBSD__ || __OpenBSD__ */
void dump_inpcbs(struct inpcb *);
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* OSDEP_BSD */

#ifdef INET
struct in_addr;
void dump_in_addr(struct in_addr *);
struct sockaddr_in;
void dump_sockaddr_in(struct sockaddr_in *);
#endif /* INET */

#ifdef INET6
#include <netinet6/debug_inet6.h>
#endif /* INET6 */
#endif /* DEBUG_NRL */

#endif /* _SYS_DEBUG_H */
