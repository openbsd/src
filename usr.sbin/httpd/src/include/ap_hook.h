/* ====================================================================
 * Copyright (c) 1998 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/*
**  Generic Hook Interface for Apache
**  Written by Ralf S. Engelschall <rse@engelschall.com> 
**
**  SYNOPSIS
**
**    Main Setup:
**      void ap_hook_init (void);
**      void ap_hook_kill (void);
**
**    Hook Configuration and Registration:
**      int  ap_hook_configure (char *hook, int sig, int modeid, ap_hook_value modeval);
**      int  ap_hook_register  (char *hook, void *func, void *ctx);
**      int  ap_hook_unregister(char *hook, void *func);
**
**    Hook Usage:
**      int  ap_hook_configured(char *hook);
**      int  ap_hook_registered(char *hook);
**      int  ap_hook_call      (char *hook, ...);
**
**  DESCRIPTION
**
**    This implements a generic hook interface for Apache which can be used
**    for loosely couple code through arbitrary hooks. There are two use cases
**    for this mechanism:
**
**    1. Inside a specific code section you want to perform a specific
**       function call. But you want to allow one or even more modules to
**       override this function call by registering hook functions.  Those
**       functions are registered on a stack and can be configured to have a
**       `decline' return value. As long as there are functions which return
**       the `decline' value the next function on the stack is tried.  When a
**       function doesn't return the `decline' value the hook call stops.  The
**       intent of this usage is to not hard-code function calls.
**
**    2. Inside a specific code you have a function you want to export.
**       But you first want to allow other code to override this function.
**       And second you want to export this function without real linker
**       symbol references. Instead you want to register the function and let
**       the users call this function via name. The intent of this usage is to
**       allow inter-module communication without direct symbol references,
**       which are a big NO-NO for the DSO situation.
**
**    And we have one major design goal: The hook call should be very similar
**    to the corresponding direct function call while still providing maximum
**    flexiblity, i.e. any function signature (the set of types for the return
**    value and the arguments) should be supported.  And it should be possible
**    to register always a context (ctx) variable with a function which is
**    passed to the corresponding function when the hook call is performed.
**
**    Using this hook interface is always a four-step process:
**
**    1. Initialize or destroy the hook mechanism inside your main program:
**
**       ap_hook_init();
**           :
**       ap_hook_kill();
**
**    2. Configure a particular hook by specifing its name, signature
**       and return type semantic:
**
**       ap_hook_configure("lookup", AP_HOOK_SIG2(ptr,ptr,ctx), AP_HOOK_DECLINE(NULL));
**       ap_hook_configure("echo", AP_HOOK_SIG2(int,ptr), AP_HOOK_TOPMOST);
**
**       This configures two hooks:
**       - A hook named "lookup" with the signature "void *lookup(void *)"
**         and a return code semantic which says: As long as a registered hook
**         function returns NULL and more registered functions exists we we
**         proceed.
**       - A hook named "echo" with the signature "int echo(void *)" and a
**         return code semantic which says: Only the top most function on the
**         registered function stack is tried, independed what value it
**         returns.
**
**    3. Register the actual functions which should be used by the hook:
**
**       ap_hook_register("lookup", mylookup, mycontext);
**       ap_hook_register("echo", myecho);
**
**       This registers the function mylookup() under the "lookup" hook with
**       the context given by the variable mycontext. And it registers the
**       function myecho() under the "echo" hook without any context.
**
**    4. Finally use the hook, i.e. instead of using direct function calls
**       like
**          
**          vp = mylookup("foo", mycontext);
**          n  = myecho("bar");
**
**       you now can use:
**
**          ap_hook_call("lookup", &vp, "foo");
**          ap_hook_call("echo", &n, "bar");
**
**       Notice two things: First the context for the mylookup() function is
**       automatically added by the hook mechanism. And it is a different and
**       not fixed context for each registered function, of course.  Second,
**       return values always have to be pushed into variables and they a
**       pointer to them has to be given as the second argument (except for
**       functions which have a void return type, of course).
**
**       BTW, the return value of ap_hook_call() is TRUE or FALSE.  TRUE when
**       at least one function call was successful (always the case for
**       AP_HOOK_TOPMOST).  FALSE when all functions returned the decline
**       value or no functions are registered at all.
**
**  RESTRICTIONS
**
**    To make the hook implementation efficient and to not bloat up the code a
**    few restrictions have to make:
**
**    1. Only function calls with up to 4 arguments are supported.
**       When more are needed you can either extend the hook implementation by
**       using more bits for the signature configuration or you can do a
**       workaround when the functions is your own one: Put the remaining
**       (N-5) arguments into a structure and pass only a pointer (one
**       argument) as the forth argument.
**
**    2. Only the following types are supported:
**       - For the return value: 
**         void (= none), char, int, float, double, ptr (= void*)
**       - For the arguments:
**         ctx  (= context), char, int, float, double, ptr (= void*)
**       This means in theory there are 6^5 (=7776) signature combinations are
**       possible. But because we don't need all of them inside Apache and it
**       would bloat up the code dramatically we implement only a subset of
**       those combinations. The to be implemented signatures can be specified
**       inside ap_hook.c and the corresponding code can be automatically
**       generated by running `perl ap_hook.c' (yeah, no joke ;-).  So when
**       you need a hook with a different still not implemented signature you
**       either have to again use a workaround as above (i.e. use a structure)
**       or just add the signature to the ap_hook.c file.
**
**  EXAMPLE
** 
**    We want to call `ssize_t read(int, void *, size_t)' through hooks in
**    order to allow modules to override this call.  So, somewhere we have a
**    replacement function for read() defined:
**
**      ssize_t my_read(int, void *, size_t);
**
**    We now configure a `read' hook.  Here the AP_HOOK_SIGx() macro defines
**    the signature of the read()-like callback functions and has to match the
**    prototype of read().  But we have to replace typedefs with the physical
**    types.  And AP_HOOK_DECLINE() sets the return value of the read()-like
**    functions which forces the next hook to be called (here -1).  And we
**    register the original read function.
**
**      ap_hook_configure("read", AP_HOOK_SIG4(int,int,ptr,int), 
**                                AP_HOOK_DECLINE(-1));
**      ap_hook_register("read", read);
**
**    Now a module wants to override the read() call and registers one more
**    function (which has to match the same prototype as read() of course):
**
**      ap_hook_register("read", my_read);
**
**    The function logically gets pushed onto a stack, so the execution order
**    is the reverse register order, i.e. last registered - first called. Now
**    we can replace the standard read() call
**
**      bytes = read(fd, buf, bufsize);
**      if (bytes == -1)
**         ...error...
**
**    with the hook-call:
**
**       rc = ap_hook_call("read", &bytes, fd, buf, bufsize);
**       if (rc == FALSE)
**          ...error...
**
**    Now internally the following is done: The call "bytes = my_read(fd, buf,
**    bufsize)" is done. When it returns not -1 (the decline value) nothing
**    more is done. But when my_read() returned -1 the next function is tried:
**    "bytes = read(fd, buf, bufsize)". When this one returns -1 again you get
**    rc == FALSE. When it finally returns not -1 you get rc == TRUE.
*/

#ifndef AP_HOOK_H
#define AP_HOOK_H

/*
 * Function Signature Specification:
 *
 * We encode the complete signature ingredients as a bitfield
 * stored in a single unsigned long integer value, which can be
 * constructed with AP_HOOK_SIGx(...)
 */

/* the type of the signature bitfield */
typedef unsigned long int ap_hook_sig;

/* the mask (bin) 111 (hex 0x7) for the triples in the bitfield */
#define AP_HOOK_SIG_TRIPLE_MASK  0x7

/* the position of the triple */
#define AP_HOOK_SIG_TRIPLE_POS(n) ((n)*3)

/* the constructor for triple #n with value v */
#define AP_HOOK_SIG_TRIPLE(n,v) \
        (((ap_hook_sig)(v))<<((AP_HOOK_##n)*3))

/* the check whether triple #n in sig contains value v */
#define AP_HOOK_SIG_HAS(sig,n,v) \
        ((((ap_hook_sig)(sig))&AP_HOOK_SIG_TRIPLE(n, AP_HOOK_SIG_TRIPLE_MASK)) == (AP_HOOK_##n##_##v))

/* utility function to get triple #n in sig */
#define AP_HOOK_SIG_TRIPLE_GET(sig,n) \
        ((((ap_hook_sig)(sig))>>AP_HOOK_SIG_TRIPLE_POS(n))&(AP_HOOK_SIG_TRIPLE_MASK))

/* utility function to set triple #n in sig to value v */
#define AP_HOOK_SIG_TRIPLE_SET(sig,n,v) \
        ((((ap_hook_sig)(sig))&~(AP_HOOK_SIG_TRIPLE_MASK<<AP_HOOK_SIG_TRIPLE_POS(n)))|((v)<<AP_HOOK_SIG_TRIPLE_POS(n)))

/* define the ingredients for the triple #0: id stuff */
#define AP_HOOK_ID          0
#define AP_HOOK_ID_ok       AP_HOOK_SIG_TRIPLE(ID,0)
#define AP_HOOK_ID_undef    AP_HOOK_SIG_TRIPLE(ID,1)

/* define the ingredients for the triple #1: return code */
#define AP_HOOK_RC          1
#define AP_HOOK_RC_void     AP_HOOK_SIG_TRIPLE(RC,0)
#define AP_HOOK_RC_char     AP_HOOK_SIG_TRIPLE(RC,1)
#define AP_HOOK_RC_int      AP_HOOK_SIG_TRIPLE(RC,2)
#define AP_HOOK_RC_long     AP_HOOK_SIG_TRIPLE(RC,3)
#define AP_HOOK_RC_float    AP_HOOK_SIG_TRIPLE(RC,4)
#define AP_HOOK_RC_double   AP_HOOK_SIG_TRIPLE(RC,5)
#define AP_HOOK_RC_ptr      AP_HOOK_SIG_TRIPLE(RC,6)

/* define the ingredients for the triple #2: argument 1 */
#define AP_HOOK_A1          2
#define AP_HOOK_A1_ctx      AP_HOOK_SIG_TRIPLE(A1,0)
#define AP_HOOK_A1_char     AP_HOOK_SIG_TRIPLE(A1,1)
#define AP_HOOK_A1_int      AP_HOOK_SIG_TRIPLE(A1,2)
#define AP_HOOK_A1_long     AP_HOOK_SIG_TRIPLE(A1,3)
#define AP_HOOK_A1_float    AP_HOOK_SIG_TRIPLE(A1,4)
#define AP_HOOK_A1_double   AP_HOOK_SIG_TRIPLE(A1,5)
#define AP_HOOK_A1_ptr      AP_HOOK_SIG_TRIPLE(A1,6)

/* define the ingredients for the triple #3: argument 2 */
#define AP_HOOK_A2          3
#define AP_HOOK_A2_ctx      AP_HOOK_SIG_TRIPLE(A2,0)
#define AP_HOOK_A2_char     AP_HOOK_SIG_TRIPLE(A2,1)
#define AP_HOOK_A2_int      AP_HOOK_SIG_TRIPLE(A2,2)
#define AP_HOOK_A2_long     AP_HOOK_SIG_TRIPLE(A2,3)
#define AP_HOOK_A2_float    AP_HOOK_SIG_TRIPLE(A2,4)
#define AP_HOOK_A2_double   AP_HOOK_SIG_TRIPLE(A2,5)
#define AP_HOOK_A2_ptr      AP_HOOK_SIG_TRIPLE(A2,6)

/* define the ingredients for the triple #4: argument 3 */
#define AP_HOOK_A3          4
#define AP_HOOK_A3_ctx      AP_HOOK_SIG_TRIPLE(A3,0)
#define AP_HOOK_A3_char     AP_HOOK_SIG_TRIPLE(A3,1)
#define AP_HOOK_A3_int      AP_HOOK_SIG_TRIPLE(A3,2)
#define AP_HOOK_A3_long     AP_HOOK_SIG_TRIPLE(A3,3)
#define AP_HOOK_A3_float    AP_HOOK_SIG_TRIPLE(A3,4)
#define AP_HOOK_A3_double   AP_HOOK_SIG_TRIPLE(A3,5)
#define AP_HOOK_A3_ptr      AP_HOOK_SIG_TRIPLE(A3,6)

/* define the ingredients for the triple #5: argument 4 */
#define AP_HOOK_A4          5
#define AP_HOOK_A4_ctx      AP_HOOK_SIG_TRIPLE(A4,0)
#define AP_HOOK_A4_char     AP_HOOK_SIG_TRIPLE(A4,1)
#define AP_HOOK_A4_int      AP_HOOK_SIG_TRIPLE(A4,2)
#define AP_HOOK_A4_long     AP_HOOK_SIG_TRIPLE(A4,3)
#define AP_HOOK_A4_float    AP_HOOK_SIG_TRIPLE(A4,4)
#define AP_HOOK_A4_double   AP_HOOK_SIG_TRIPLE(A4,5)
#define AP_HOOK_A4_ptr      AP_HOOK_SIG_TRIPLE(A4,6)

/* define the ingredients for the triple #6: argument 5 */
#define AP_HOOK_A5          6
#define AP_HOOK_A5_ctx      AP_HOOK_SIG_TRIPLE(A5,0)
#define AP_HOOK_A5_char     AP_HOOK_SIG_TRIPLE(A5,1)
#define AP_HOOK_A5_int      AP_HOOK_SIG_TRIPLE(A5,2)
#define AP_HOOK_A5_long     AP_HOOK_SIG_TRIPLE(A5,3)
#define AP_HOOK_A5_float    AP_HOOK_SIG_TRIPLE(A5,4)
#define AP_HOOK_A5_double   AP_HOOK_SIG_TRIPLE(A5,5)
#define AP_HOOK_A5_ptr      AP_HOOK_SIG_TRIPLE(A5,6)

/* define the ingredients for the triple #7: argument 6 */
#define AP_HOOK_A6          7
#define AP_HOOK_A6_ctx      AP_HOOK_SIG_TRIPLE(A6,0)
#define AP_HOOK_A6_char     AP_HOOK_SIG_TRIPLE(A6,1)
#define AP_HOOK_A6_int      AP_HOOK_SIG_TRIPLE(A6,2)
#define AP_HOOK_A6_long     AP_HOOK_SIG_TRIPLE(A6,3)
#define AP_HOOK_A6_float    AP_HOOK_SIG_TRIPLE(A6,4)
#define AP_HOOK_A6_double   AP_HOOK_SIG_TRIPLE(A6,5)
#define AP_HOOK_A6_ptr      AP_HOOK_SIG_TRIPLE(A6,6)

/* define the ingredients for the triple #8: argument 7 */
#define AP_HOOK_A7          8
#define AP_HOOK_A7_ctx      AP_HOOK_SIG_TRIPLE(A7,0)
#define AP_HOOK_A7_char     AP_HOOK_SIG_TRIPLE(A7,1)
#define AP_HOOK_A7_int      AP_HOOK_SIG_TRIPLE(A7,2)
#define AP_HOOK_A7_long     AP_HOOK_SIG_TRIPLE(A7,3)
#define AP_HOOK_A7_float    AP_HOOK_SIG_TRIPLE(A7,4)
#define AP_HOOK_A7_double   AP_HOOK_SIG_TRIPLE(A7,5)
#define AP_HOOK_A7_ptr      AP_HOOK_SIG_TRIPLE(A7,6)

/* define the ingredients for the triple #9: argument 8 */
#define AP_HOOK_A8          9
#define AP_HOOK_A8_ctx      AP_HOOK_SIG_TRIPLE(9,0)
#define AP_HOOK_A8_char     AP_HOOK_SIG_TRIPLE(9,1)
#define AP_HOOK_A8_int      AP_HOOK_SIG_TRIPLE(9,2)
#define AP_HOOK_A8_long     AP_HOOK_SIG_TRIPLE(9,3)
#define AP_HOOK_A8_float    AP_HOOK_SIG_TRIPLE(9,4)
#define AP_HOOK_A8_double   AP_HOOK_SIG_TRIPLE(9,5)
#define AP_HOOK_A8_ptr      AP_HOOK_SIG_TRIPLE(9,6)
  
/* the constructor for unknown signatures */
#define AP_HOOK_SIG_UNKNOWN AP_HOOK_ID_undef

/* the constructor for signatures with 1 type */
#define AP_HOOK_SIG1(rc) \
        (AP_HOOK_RC_##rc)

/* the constructor for signatures with 2 types */
#define AP_HOOK_SIG2(rc,a1) \
        (AP_HOOK_RC_##rc|AP_HOOK_A1_##a1)

/* the constructor for signatures with 3 types */
#define AP_HOOK_SIG3(rc,a1,a2) \
        (AP_HOOK_RC_##rc|AP_HOOK_A1_##a1|AP_HOOK_A2_##a2)

/* the constructor for signatures with 4 types */
#define AP_HOOK_SIG4(rc,a1,a2,a3) \
        (AP_HOOK_RC_##rc|AP_HOOK_A1_##a1|AP_HOOK_A2_##a2|AP_HOOK_A3_##a3)

/* the constructor for signatures with 5 types */
#define AP_HOOK_SIG5(rc,a1,a2,a3,a4) \
        (AP_HOOK_RC_##rc|AP_HOOK_A1_##a1|AP_HOOK_A2_##a2|AP_HOOK_A3_##a3|AP_HOOK_A4_##a4)

/* the constructor for signatures with 6 types */
#define AP_HOOK_SIG6(rc,a1,a2,a3,a4,a5) \
        (AP_HOOK_RC_##rc|AP_HOOK_A1_##a1|AP_HOOK_A2_##a2|AP_HOOK_A3_##a3|AP_HOOK_A4_##a4|AP_HOOK_A5_##a5)

/* the constructor for signatures with 7 types */
#define AP_HOOK_SIG7(rc,a1,a2,a3,a4,a5,a6) \
        (AP_HOOK_RC_##rc|AP_HOOK_A1_##a1|AP_HOOK_A2_##a2|AP_HOOK_A3_##a3|AP_HOOK_A4_##a4|AP_HOOK_A5_##a5|AP_HOOK_A6_##a6)

/* the constructor for signatures with 8 types */
#define AP_HOOK_SIG8(rc,a1,a2,a3,a4,a5,a6,a7) \
        (AP_HOOK_RC_##rc|AP_HOOK_A1_##a1|AP_HOOK_A2_##a2|AP_HOOK_A3_##a3|AP_HOOK_A4_##a4|AP_HOOK_A5_##a5|AP_HOOK_A6_##a6|AP_HOOK_A7_##a7)

/* the constructor for signatures with 9 types */
#define AP_HOOK_SIG9(rc,a1,a2,a3,a4,a5,a6,a7,a8) \
        (AP_HOOK_RC_##rc|AP_HOOK_A1_##a1|AP_HOOK_A2_##a2|AP_HOOK_A3_##a3|AP_HOOK_A4_##a4|AP_HOOK_A5_##a5|AP_HOOK_A6_##a6|AP_HOOK_A7_##a7|AP_HOOK_A8_##a8)

/*
 * Return Value Mode Identification
 */

/* the type of the return value modes */
typedef unsigned int ap_hook_mode;

/* the mode of the return value */
#define AP_HOOK_MODE_UNKNOWN  0
#define AP_HOOK_MODE_TOPMOST  1
#define AP_HOOK_MODE_DECLINE  2
#define AP_HOOK_MODE_ALL      3

/* the constructors for the return value modes */
#define AP_HOOK_TOPMOST       AP_HOOK_MODE_TOPMOST
#define AP_HOOK_DECLINE(val)  AP_HOOK_MODE_DECLINE, (val)   
#define AP_HOOK_ALL           AP_HOOK_MODE_ALL

/*
 * Hook State Identification
 */

/* the type of the hook state */
typedef unsigned short int ap_hook_state;

/* the values of the hook state */
#define AP_HOOK_STATE_UNDEF       0
#define AP_HOOK_STATE_NOTEXISTANT 1
#define AP_HOOK_STATE_ESTABLISHED 2
#define AP_HOOK_STATE_CONFIGURED  3
#define AP_HOOK_STATE_REGISTERED  4

/*
 * Hook Context Identification
 *
 * Notice: Null is ok here, because AP_HOOK_NOCTX is just a dummy argument
 *         because we know from the signature whether the argument is a
 *         context value or just the dummy value.
 */

#define AP_HOOK_NOCTX  (void *)(0)
#define AP_HOOK_CTX(v) (void *)(v)

/*
 * Internal Hook Record Definition
 */

/* the union holding the arbitrary decline values */
typedef union {
    char   v_char;
    int    v_int;
    long   v_long;
    float  v_float;
    double v_double;
    void  *v_ptr;
} ap_hook_value;

/* the structure holding one hook function and its context */
typedef struct {
    void *hf_ptr;              /* function pointer       */
    void *hf_ctx;              /* function context       */
} ap_hook_func;

/* the structure holding one hook entry with all its registered functions */
typedef struct {
    char          *he_hook;    /* hook name (=unique id) */
    ap_hook_sig    he_sig;     /* hook signature         */
    int            he_modeid;  /* hook mode id           */
    ap_hook_value  he_modeval; /* hook mode value        */
    ap_hook_func **he_func;    /* hook registered funcs  */
} ap_hook_entry;

/* the maximum number of hooks and functions per hook */
#define AP_HOOK_MAX_ENTRIES 512
#define AP_HOOK_MAX_FUNCS   128

/*
 * Extended Variable Argument (vararg) Support
 *
 * In ANSI C varargs exists, but because the prototypes of function with
 * varargs cannot reflect the types of the varargs, K&R argument passing
 * conventions have to apply for the compiler.  This means mainly a conversion
 * of shorter type variants to the maximum variant (according to sizeof). The
 * above va_type() macro provides this mapping from the wanted types to the
 * physically used ones.
 */

/* the mapping */
#define VA_TYPE_char   int
#define VA_TYPE_short  int
#define VA_TYPE_int    int
#define VA_TYPE_long   long
#define VA_TYPE_float  double
#define VA_TYPE_double double
#define VA_TYPE_ptr    void *
#define VA_TYPE_ctx    void *

/* the constructor */
#ifdef  va_type
#undef  va_type
#endif
#define va_type(type)  VA_TYPE_ ## type

/*
 * Miscellaneous stuff
 */

#ifndef FALSE
#define FALSE 0
#define TRUE  !FALSE
#endif

/*
 * Wrapper macros for the callback-function register/unregister calls.  
 * 
 * Background: Strict ANSI C doesn't allow a function pointer to be treated as
 * a void pointer on argument passing, but we cannot declare the argument as a
 * function prototype, because the functions can have arbitrary signatures. So
 * we have to use a void pointer here. But to not require explicit casts on
 * function pointers for every register/unregister call, we smooth the API a
 * little bit by providing these macros.
 */

#define ap_hook_register(hook,func,ctx) ap_hook_register_I(hook,(void *)(func),ctx)
#define ap_hook_unregister(hook,func)   ap_hook_unregister_I(hook,(void *)(func))

/*
 * Prototypes for the hook API functions
 */

API_EXPORT(void)          ap_hook_init         (void);
API_EXPORT(void)          ap_hook_kill         (void);
API_EXPORT(int)           ap_hook_configure    (char *hook, ap_hook_sig sig, ap_hook_mode modeid, ...);
API_EXPORT(int)           ap_hook_register_I   (char *hook, void *func, void *ctx);
API_EXPORT(int)           ap_hook_unregister_I (char *hook, void *func);
API_EXPORT(ap_hook_state) ap_hook_status       (char *hook);
API_EXPORT(int)           ap_hook_use          (char *hook, ap_hook_sig sig, ap_hook_mode modeid, ...);
API_EXPORT(int)           ap_hook_call         (char *hook, ...);

#endif /* AP_HOOK_H */
