
/*
   XS code to test the typemap entries

   Copyright (C) 2001 Tim Jenness.
   All Rights Reserved

*/

#include "EXTERN.h"   /* std perl include */
#include "perl.h"     /* std perl include */
#include "XSUB.h"     /* XSUB include */

/* Prototypes for external functions */
FILE * xsfopen( const char * );
int xsfclose( FILE * );
int xsfprintf( FILE *, const char *);

/* Type definitions required for the XS typemaps */
typedef SV * SVREF; /* T_SVREF */
typedef int SysRet; /* T_SYSRET */
typedef int Int;    /* T_INT */
typedef int intRef; /* T_PTRREF */
typedef int intObj; /* T_PTROBJ */
typedef int intRefIv; /* T_REF_IV_PTR */
typedef int intArray; /* T_ARRAY */
typedef short shortOPQ;   /* T_OPAQUE */
typedef int intOpq;   /* T_OPAQUEPTR */

/* A structure to test T_OPAQUEPTR */
struct t_opaqueptr {
  int a;
  int b;
  double c;
};

typedef struct t_opaqueptr astruct;

/* Some static memory for the tests */
static I32 xst_anint;
static intRef xst_anintref;
static intObj xst_anintobj;
static intRefIv xst_anintrefiv;
static intOpq xst_anintopq;

/* Helper functions */

/* T_ARRAY - allocate some memory */
intArray * intArrayPtr( int nelem ) {
    intArray * array;
    New(0, array, nelem, intArray);
    return array;
}


MODULE = XS::Typemap   PACKAGE = XS::Typemap

PROTOTYPES: DISABLE

=head1 TYPEMAPS

Each C type is represented by an entry in the typemap file that
is responsible for converting perl variables (SV, AV, HV and CV) to
and from that type.

=over 4

=item T_SV

This simply passes the C representation of the Perl variable (an SV*)
in and out of the XS layer. This can be used if the C code wants
to deal directly with the Perl variable.

=cut

SV *
T_SV( sv )
  SV * sv
 CODE:
  /* create a new sv for return that is a copy of the input
     do not simply copy the pointer since the SV will be marked
     mortal by the INPUT typemap when it is pushed back onto the stack */
  RETVAL = sv_mortalcopy( sv );
  /* increment the refcount since the default INPUT typemap mortalizes
     by default and we don't want to decrement the ref count twice
     by mistake */
  SvREFCNT_inc(RETVAL);
 OUTPUT:
  RETVAL

=item T_SVREF

Used to pass in and return a reference to an SV.

=cut

SVREF
T_SVREF( svref )
  SVREF svref
 CODE:
  RETVAL = svref;
 OUTPUT:
  RETVAL

=item T_AVREF

From the perl level this is a reference to a perl array.
From the C level this is a pointer to an AV.

=cut

AV *
T_AVREF( av )
  AV * av
 CODE:
  RETVAL = av;
 OUTPUT:
  RETVAL

=item T_HVREF

From the perl level this is a reference to a perl hash.
From the C level this is a pointer to an HV.

=cut

HV *
T_HVREF( hv )
  HV * hv
 CODE:
  RETVAL = hv;
 OUTPUT:
  RETVAL

=item T_CVREF

From the perl level this is a reference to a perl subroutine
(e.g. $sub = sub { 1 };). From the C level this is a pointer
to a CV.

=cut

CV *
T_CVREF( cv )
  CV * cv
 CODE:
  RETVAL = cv;
 OUTPUT:
  RETVAL


=item T_SYSRET

The T_SYSRET typemap is used to process return values from system calls.
It is only meaningful when passing values from C to perl (there is
no concept of passing a system return value from Perl to C).

System calls return -1 on error (setting ERRNO with the reason)
and (usually) 0 on success. If the return value is -1 this typemap
returns C<undef>. If the return value is not -1, this typemap
translates a 0 (perl false) to "0 but true" (which
is perl true) or returns the value itself, to indicate that the
command succeeded.

The L<POSIX|POSIX> module makes extensive use of this type.

=cut

# Test a successful return

SysRet
T_SYSRET_pass()
 CODE:
  RETVAL = 0;
 OUTPUT:
  RETVAL

# Test failure

SysRet
T_SYSRET_fail()
 CODE:
  RETVAL = -1;
 OUTPUT:
  RETVAL

=item T_UV

An unsigned integer.

=cut

unsigned int
T_UV( uv )
  unsigned int uv
 CODE:
  RETVAL = uv;
 OUTPUT:
  RETVAL

=item T_IV

A signed integer. This is cast to the required  integer type when
passed to C and converted to an IV when passed back to Perl.

=cut

long
T_IV( iv )
  long iv
 CODE:
  RETVAL = iv;
 OUTPUT:
  RETVAL

=item T_INT

A signed integer. This typemap converts the Perl value to a native
integer type (the C<int> type on the current platform). When returning
the value to perl it is processed in the same way as for T_IV.

Its behaviour is identical to using an C<int> type in XS with T_IV.

=item T_ENUM

An enum value. Used to transfer an enum component
from C. There is no reason to pass an enum value to C since
it is stored as an IV inside perl.

=cut

# The test should return the value for SVt_PVHV.
# 11 at the present time but we can't not rely on this
# for testing purposes.

svtype
T_ENUM()
 CODE:
  RETVAL = SVt_PVHV;
 OUTPUT:
  RETVAL

=item T_BOOL

A boolean type. This can be used to pass true and false values to and
from C.

=cut

bool
T_BOOL( in )
  bool in
 CODE:
  RETVAL = in;
 OUTPUT:
  RETVAL

=item T_U_INT

This is for unsigned integers. It is equivalent to using T_UV
but explicitly casts the variable to type C<unsigned int>.
The default type for C<unsigned int> is T_UV.

=item T_SHORT

Short integers. This is equivalent to T_IV but explicitly casts
the return to type C<short>. The default typemap for C<short>
is T_IV.

=item T_U_SHORT

Unsigned short integers. This is equivalent to T_UV but explicitly
casts the return to type C<unsigned short>. The default typemap for
C<unsigned short> is T_UV.

T_U_SHORT is used for type C<U16> in the standard typemap.

=cut

U16
T_U_SHORT( in )
  U16 in
 CODE:
  RETVAL = in;
 OUTPUT:
  RETVAL


=item T_LONG

Long integers. This is equivalent to T_IV but explicitly casts
the return to type C<long>. The default typemap for C<long>
is T_IV.

=item T_U_LONG

Unsigned long integers. This is equivalent to T_UV but explicitly
casts the return to type C<unsigned long>. The default typemap for
C<unsigned long> is T_UV.

T_U_LONG is used for type C<U32> in the standard typemap.

=cut

U32
T_U_LONG( in )
  U32 in
 CODE:
  RETVAL = in;
 OUTPUT:
  RETVAL

=item T_CHAR

Single 8-bit characters.

=cut

char
T_CHAR( in );
  char in
 CODE:
  RETVAL = in;
 OUTPUT:
  RETVAL


=item T_U_CHAR

An unsigned byte.

=cut

unsigned char
T_U_CHAR( in );
  unsigned char in
 CODE:
  RETVAL = in;
 OUTPUT:
  RETVAL


=item T_FLOAT

A floating point number. This typemap guarantees to return a variable
cast to a C<float>.

=cut

float
T_FLOAT( in )
  float in
 CODE:
  RETVAL = in;
 OUTPUT:
  RETVAL

=item T_NV

A Perl floating point number. Similar to T_IV and T_UV in that the
return type is cast to the requested numeric type rather than
to a specific type.

=cut

NV
T_NV( in )
  NV in
 CODE:
  RETVAL = in;
 OUTPUT:
  RETVAL

=item T_DOUBLE

A double precision floating point number. This typemap guarantees to
return a variable cast to a C<double>.

=cut

double
T_DOUBLE( in )
  double in
 CODE:
  RETVAL = in;
 OUTPUT:
  RETVAL

=item T_PV

A string (char *).

=cut

char *
T_PV( in )
  char * in
 CODE:
  RETVAL = in;
 OUTPUT:
  RETVAL

=item T_PTR

A memory address (pointer). Typically associated with a C<void *>
type.

=cut

# Pass in a value. Store the value in some static memory and
# then return the pointer

void *
T_PTR_OUT( in )
  int in;
 CODE:
  xst_anint = in;
  RETVAL = &xst_anint;
 OUTPUT:
  RETVAL

# pass in the pointer and return the value

int
T_PTR_IN( ptr )
  void * ptr
 CODE:
  RETVAL = *(int *)ptr;
 OUTPUT:
  RETVAL

=item T_PTRREF

Similar to T_PTR except that the pointer is stored in a scalar and the
reference to that scalar is returned to the caller. This can be used
to hide the actual pointer value from the programmer since it is usually
not required directly from within perl.

The typemap checks that a scalar reference is passed from perl to XS.

=cut

# Similar test to T_PTR
# Pass in a value. Store the value in some static memory and
# then return the pointer

intRef *
T_PTRREF_OUT( in )
  intRef in;
 CODE:
  xst_anintref = in;
  RETVAL = &xst_anintref;
 OUTPUT:
  RETVAL

# pass in the pointer and return the value

intRef
T_PTRREF_IN( ptr )
  intRef * ptr
 CODE:
  RETVAL = *ptr;
 OUTPUT:
  RETVAL



=item T_PTROBJ

Similar to T_PTRREF except that the reference is blessed into a class.
This allows the pointer to be used as an object. Most commonly used to
deal with C structs. The typemap checks that the perl object passed
into the XS routine is of the correct class (or part of a subclass).

The pointer is blessed into a class that is derived from the name
of type of the pointer but with all '*' in the name replaced with
'Ptr'.

=cut

# Similar test to T_PTRREF
# Pass in a value. Store the value in some static memory and
# then return the pointer

intObj *
T_PTROBJ_OUT( in )
  intObj in;
 CODE:
  xst_anintobj = in;
  RETVAL = &xst_anintobj;
 OUTPUT:
  RETVAL

# pass in the pointer and return the value

MODULE = XS::Typemap  PACKAGE = intObjPtr

intObj
T_PTROBJ_IN( ptr )
  intObj * ptr
 CODE:
  RETVAL = *ptr;
 OUTPUT:
  RETVAL

MODULE = XS::Typemap PACKAGE = XS::Typemap

=item T_REF_IV_REF

NOT YET

=item T_REF_IV_PTR

Similar to T_PTROBJ in that the pointer is blessed into a scalar object.
The difference is that when the object is passed back into XS it must be
of the correct type (inheritance is not supported).

The pointer is blessed into a class that is derived from the name
of type of the pointer but with all '*' in the name replaced with
'Ptr'.

=cut

# Similar test to T_PTROBJ
# Pass in a value. Store the value in some static memory and
# then return the pointer

intRefIv *
T_REF_IV_PTR_OUT( in )
  intRefIv in;
 CODE:
  xst_anintrefiv = in;
  RETVAL = &xst_anintrefiv;
 OUTPUT:
  RETVAL

# pass in the pointer and return the value

MODULE = XS::Typemap  PACKAGE = intRefIvPtr

intRefIv
T_REF_IV_PTR_IN( ptr )
  intRefIv * ptr
 CODE:
  RETVAL = *ptr;
 OUTPUT:
  RETVAL


MODULE = XS::Typemap PACKAGE = XS::Typemap

=item T_PTRDESC

NOT YET

=item T_REFREF

NOT YET

=item T_REFOBJ

NOT YET

=item T_OPAQUEPTR

This can be used to store bytes in the string component of the
SV. Here the representation of the data is irrelevant to perl and the
bytes themselves are just stored in the SV. It is assumed that the C
variable is a pointer (the bytes are copied from that memory
location).  If the pointer is pointing to something that is
represented by 8 bytes then those 8 bytes are stored in the SV (and
length() will report a value of 8). This entry is similar to T_OPAQUE.

In principal the unpack() command can be used to convert the bytes
back to a number (if the underlying type is known to be a number).

This entry can be used to store a C structure (the number
of bytes to be copied is calculated using the C C<sizeof> function)
and can be used as an alternative to T_PTRREF without having to worry
about a memory leak (since Perl will clean up the SV).

=cut

intOpq *
T_OPAQUEPTR_IN( val )
  intOpq val
 CODE:
  xst_anintopq = val;
  RETVAL = &xst_anintopq;
 OUTPUT:
  RETVAL

intOpq
T_OPAQUEPTR_OUT( ptr )
  intOpq * ptr
 CODE:
  RETVAL = *ptr;
 OUTPUT:
  RETVAL

short
T_OPAQUEPTR_OUT_short( ptr )
  shortOPQ * ptr
 CODE:
  RETVAL = *ptr;
 OUTPUT:
  RETVAL

# Test it with a structure
astruct *
T_OPAQUEPTR_IN_struct( a,b,c )
  int a
  int b
  double c
 PREINIT:
  struct t_opaqueptr test;
 CODE:
  test.a = a;
  test.b = b;
  test.c = c;
  RETVAL = &test;
 OUTPUT:
  RETVAL

void
T_OPAQUEPTR_OUT_struct( test )
  astruct * test
 PPCODE:
  XPUSHs(sv_2mortal(newSViv(test->a)));
  XPUSHs(sv_2mortal(newSViv(test->b)));
  XPUSHs(sv_2mortal(newSVnv(test->c)));


=item T_OPAQUE

This can be used to store data from non-pointer types in the string
part of an SV. It is similar to T_OPAQUEPTR except that the
typemap retrieves the pointer directly rather than assuming it
is being supplied. For example if an integer is imported into
Perl using T_OPAQUE rather than T_IV the underlying bytes representing
the integer will be stored in the SV but the actual integer value will not
be available. i.e. The data is opaque to perl.

The data may be retrieved using the C<unpack> function if the
underlying type of the byte stream is known.

T_OPAQUE supports input and output of simple types.
T_OPAQUEPTR can be used to pass these bytes back into C if a pointer
is acceptable.

=cut

shortOPQ
T_OPAQUE_IN( val )
  int val
 CODE:
  RETVAL = (shortOPQ)val;
 OUTPUT:
  RETVAL

IV
T_OPAQUE_OUT( val )
  shortOPQ val
 CODE:
  RETVAL = (IV)val;
 OUTPUT:
  RETVAL

=item Implicit array

xsubpp supports a special syntax for returning
packed C arrays to perl. If the XS return type is given as

  array(type, nelem)

xsubpp will copy the contents of C<nelem * sizeof(type)> bytes from
RETVAL to an SV and push it onto the stack. This is only really useful
if the number of items to be returned is known at compile time and you
don't mind having a string of bytes in your SV.  Use T_ARRAY to push a
variable number of arguments onto the return stack (they won't be
packed as a single string though).

This is similar to using T_OPAQUEPTR but can be used to process more than
one element.

=cut

array(int,3)
T_OPAQUE_array( a,b,c)
  int a
  int b
  int c
 PREINIT:
  int array[3];
 CODE:
  array[0] = a;
  array[1] = b;
  array[2] = c;
  RETVAL = array;
 OUTPUT:
  RETVAL


=item T_PACKED

NOT YET

=item T_PACKEDARRAY

NOT YET

=item T_DATAUNIT

NOT YET

=item T_CALLBACK

NOT YET

=item T_ARRAY

This is used to convert the perl argument list to a C array
and for pushing the contents of a C array onto the perl
argument stack.

The usual calling signature is

  @out = array_func( @in );

Any number of arguments can occur in the list before the array but
the input and output arrays must be the last elements in the list.

When used to pass a perl list to C the XS writer must provide a
function (named after the array type but with 'Ptr' substituted for
'*') to allocate the memory required to hold the list. A pointer
should be returned. It is up to the XS writer to free the memory on
exit from the function. The variable C<ix_$var> is set to the number
of elements in the new array.

When returning a C array to Perl the XS writer must provide an integer
variable called C<size_$var> containing the number of elements in the
array. This is used to determine how many elements should be pushed
onto the return argument stack. This is not required on input since
Perl knows how many arguments are on the stack when the routine is
called. Ordinarily this variable would be called C<size_RETVAL>.

Additionally, the type of each element is determined from the type of
the array. If the array uses type C<intArray *> xsubpp will
automatically work out that it contains variables of type C<int> and
use that typemap entry to perform the copy of each element. All
pointer '*' and 'Array' tags are removed from the name to determine
the subtype.

=cut

# Test passes in an integer array and returns it along with
# the number of elements
# Pass in a dummy value to test offsetting

# Problem is that xsubpp does XSRETURN(1) because we arent
# using PPCODE. This means that only the first element
# is returned. KLUGE this by using CLEANUP to return before the
# end.

intArray *
T_ARRAY( dummy, array, ... )
  int dummy = 0;
  intArray * array
 PREINIT:
  U32 size_RETVAL;
 CODE:
  dummy += 0; /* Fix -Wall */
  size_RETVAL = ix_array;
  RETVAL = array;
 OUTPUT:
  RETVAL
 CLEANUP:
  Safefree(array);
  XSRETURN(size_RETVAL);


=item T_STDIO

This is used for passing perl filehandles to and from C using
C<FILE *> structures.

=cut

FILE *
T_STDIO_open( file )
  const char * file
 CODE:
  RETVAL = xsfopen( file );
 OUTPUT:
  RETVAL

SysRet
T_STDIO_close( f )
  PerlIO * f
 PREINIT:
  FILE * stream;
 CODE:
  /* Get the FILE* */
  stream = PerlIO_findFILE( f );  
  RETVAL = xsfclose( stream );
  /* Release the FILE* from the PerlIO system so that we do
     not close the file twice */
  PerlIO_releaseFILE(f,stream);
 OUTPUT:
  RETVAL

int
T_STDIO_print( stream, string )
  FILE * stream
  const char * string
 CODE:
  RETVAL = xsfprintf( stream, string );
 OUTPUT:
  RETVAL


=item T_IN

NOT YET

=item T_INOUT

This is used for passing perl filehandles to and from C using
C<PerlIO *> structures. The file handle can used for reading and
writing.

See L<perliol> for more information on the Perl IO abstraction
layer. Perl must have been built with C<-Duseperlio>.

=item T_OUT

NOT YET

=back

=cut

