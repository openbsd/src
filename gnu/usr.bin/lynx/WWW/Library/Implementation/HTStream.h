/*                                                      The Stream class definition -- libwww
                                 STREAM OBJECT DEFINITION
                                             
   A Stream object is something which accepts a stream of text.
   
   The creation methods will vary on the type of Stream Object.   All creation methods
   return a pointer to the stream type below.
   
   As you can see, but the methods used to write to the stream and close it are pointed to
   be the object itself.
   
 */
#ifndef HTSTREAM_H
#define HTSTREAM_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif
 
typedef struct _HTStream HTStream;

/*

   These are the common methods of all streams.  They should be self-explanatory, except
   for end_document which must be called before free.  It should be merged with free in
   fact:  it should be dummy for new streams.
   
   The put_block method was write, but this upset systems which had macros for write().
   
 */
typedef struct _HTStreamClass {

        char*  name;                            /* Just for diagnostics */
                
        void (*_free) PARAMS((
                HTStream*       me));

        void (*_abort) PARAMS((
                HTStream*       me,
                HTError         e));
                
        void (*put_character) PARAMS((
                HTStream*       me,
                char            ch));
                                
        void (*put_string) PARAMS((
                HTStream*       me,
                CONST char *    str));
                
        void (*put_block) PARAMS((
                HTStream*       me,
                CONST char *    str,
                int             len));

}HTStreamClass;

/*

  Generic Error Stream

   The Error stream simply signals an error on all output methods.
   This can be used to stop a stream as soon as data arrives, for
   example from the network.

 */
extern HTStream * HTErrorStream NOPARAMS;

#endif /* HTSTREAM_H */

/*

   end of HTStream.h */
