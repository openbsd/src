/* Copyright (C) 1997 Free Software Foundation, Inc.
 * This is part of the G77 manual.
 * For copying conditions, see the file g77.texi. */

/* This is the file containing the verbage for the
   intrinsics.  It consists of a data base built up
   via DEFDOC macros of the form:

   DEFDOC (IMP, SUMMARY, DESCRIPTION)

   IMP is the implementation keyword used in the intrin module.
   SUMMARY is the short summary to go in the "* Menu:" section
   of the Info document.  DESCRIPTION is the longer description
   to go in the documentation itself.

   Note that IMP is leveraged across multiple intrinsic names.

   To make for more accurate and consistent documentation,
   the translation made by intdoc.c of the text in SUMMARY
   and DESCRIPTION includes the special sequence

   @ARGNO@

   where ARGNO is a series of digits forming a number that
   is substituted by intdoc.c as follows:

   0     The initial-caps form of the intrinsic name (e.g. Float).
   1-98  The initial-caps form of the ARGNO'th argument.
   99    (SUMMARY only) a newline plus the appropriate # of spaces.

   Hope this info is enough to encourage people to feel free to
   add documentation to this file!

*/

DEFDOC (ABS, "Absolute value.", "\
Returns the absolute value of @var{@1@}.

If @var{@1@} is type @code{COMPLEX}, the absolute
value is computed as:

@example
SQRT(REALPART(@var{@1@})**2, IMAGPART(@var{@1@})**2)
@end example

@noindent
Otherwise, it is computed by negating the @var{@1@} if
it is negative, or returning @var{@1@}.

@xref{Sign Intrinsic}, for how to explicitly
compute the positive or negative form of the absolute
value of an expression.
")

DEFDOC (CABS, "Absolute value (archaic).", "\
Archaic form of @code{ABS()} that is specific
to one type for @var{@1@}.
@xref{Abs Intrinsic}.
")

DEFDOC (DABS, "Absolute value (archaic).", "\
Archaic form of @code{ABS()} that is specific
to one type for @var{@1@}.
@xref{Abs Intrinsic}.
")

DEFDOC (IABS, "Absolute value (archaic).", "\
Archaic form of @code{ABS()} that is specific
to one type for @var{@1@}.
@xref{Abs Intrinsic}.
")

DEFDOC (CDABS, "Absolute value (archaic).", "\
Archaic form of @code{ABS()} that is specific
to one type for @var{@1@}.
@xref{Abs Intrinsic}.
")

DEFDOC (ACHAR, "ASCII character from code.", "\
Returns the ASCII character corresponding to the
code specified by @var{@1@}.

@xref{IAChar Intrinsic}, for the inverse function.

@xref{Char Intrinsic}, for the function corresponding
to the system's native character set.
")

DEFDOC (IACHAR, "ASCII code for character.", "\
Returns the code for the ASCII character in the
first character position of @var{@1@}.

@xref{AChar Intrinsic}, for the inverse function.

@xref{IChar Intrinsic}, for the function corresponding
to the system's native character set.
")

DEFDOC (CHAR, "Character from code.", "\
Returns the character corresponding to the
code specified by @var{@1@}, using the system's
native character set.

Because the system's native character set is used,
the correspondence between character and their codes
is not necessarily the same between GNU Fortran
implementations.

@xref{IChar Intrinsic}, for the inverse function.

@xref{AChar Intrinsic}, for the function corresponding
to the ASCII character set.
")

DEFDOC (ICHAR, "Code for character.", "\
Returns the code for the character in the
first character position of @var{@1@}.

Because the system's native character set is used,
the correspondence between character and their codes
is not necessarily the same between GNU Fortran
implementations.

@xref{Char Intrinsic}, for the inverse function.

@xref{IAChar Intrinsic}, for the function corresponding
to the ASCII character set.
")

DEFDOC (ACOS, "Arc cosine.", "\
Returns the arc-cosine (inverse cosine) of @var{@1@}
in radians.

@xref{Cos Intrinsic}, for the inverse function.
")

DEFDOC (DACOS, "Arc cosine (archaic).", "\
Archaic form of @code{ACOS()} that is specific
to one type for @var{@1@}.
@xref{ACos Intrinsic}.
")

DEFDOC (AIMAG, "Convert/extract imaginary part of complex.", "\
Returns the (possibly converted) imaginary part of @var{@1@}.

Use of @code{@0@()} with an argument of a type
other than @code{COMPLEX(KIND=1)} is restricted to the following case:

@example
REAL(AIMAG(@1@))
@end example

@noindent
This expression converts the imaginary part of @1@ to
@code{REAL(KIND=1)}.

@xref{REAL() and AIMAG() of Complex}, for more information.
")

DEFDOC (AINT, "Truncate to whole number.", "\
Returns @var{@1@} with the fractional portion of its
magnitude truncated and its sign preserved.
(Also called ``truncation towards zero''.)

@xref{ANInt Intrinsic}, for how to round to nearest
whole number.

@xref{Int Intrinsic}, for how to truncate and then convert
number to @code{INTEGER}.
")

DEFDOC (DINT, "Truncate to whole number (archaic).", "\
Archaic form of @code{AINT()} that is specific
to one type for @var{@1@}.
@xref{AInt Intrinsic}.
")

DEFDOC (INT, "Convert to @code{INTEGER} value truncated@99@to whole number.", "\
Returns @var{@1@} with the fractional portion of its
magnitude truncated and its sign preserved, converted
to type @code{INTEGER(KIND=1)}.

If @var{@1@} is type @code{COMPLEX}, its real part is
truncated and converted.

@xref{NInt Intrinsic}, for how to convert, rounded to nearest
whole number.

@xref{AInt Intrinsic}, for how to truncate to whole number
without converting.
")

DEFDOC (IDINT, "Convert to @code{INTEGER} value truncated@99@to whole number (archaic).", "\
Archaic form of @code{INT()} that is specific
to one type for @var{@1@}.
@xref{Int Intrinsic}.
")

DEFDOC (ANINT, "Round to nearest whole number.", "\
Returns @var{@1@} with the fractional portion of its
magnitude eliminated by rounding to the nearest whole
number and with its sign preserved.

A fractional portion exactly equal to
@samp{.5} is rounded to the whole number that
is larger in magnitude.
(Also called ``Fortran round''.)

@xref{AInt Intrinsic}, for how to truncate to
whole number.

@xref{NInt Intrinsic}, for how to round and then convert
number to @code{INTEGER}.
")

DEFDOC (DNINT, "Round to nearest whole number (archaic).", "\
Archaic form of @code{ANINT()} that is specific
to one type for @var{@1@}.
@xref{ANInt Intrinsic}.
")

DEFDOC (NINT, "Convert to @code{INTEGER} value rounded@99@to nearest whole number.", "\
Returns @var{@1@} with the fractional portion of its
magnitude eliminated by rounding to the nearest whole
number and with its sign preserved, converted
to type @code{INTEGER(KIND=1)}.

If @var{@1@} is type @code{COMPLEX}, its real part is
rounded and converted.

A fractional portion exactly equal to
@samp{.5} is rounded to the whole number that
is larger in magnitude.
(Also called ``Fortran round''.)

@xref{Int Intrinsic}, for how to convert, truncate to
whole number.

@xref{ANInt Intrinsic}, for how to round to nearest whole number
without converting.
")

DEFDOC (IDNINT, "Convert to @code{INTEGER} value rounded@99@to nearest whole number (archaic).", "\
Archaic form of @code{NINT()} that is specific
to one type for @var{@1@}.
@xref{NInt Intrinsic}.
")

DEFDOC (LOG, "Natural logarithm.", "\
Returns the natural logarithm of @var{@1@}, which must
be greater than zero or, if type @code{COMPLEX}, must not
be zero.

@xref{Exp Intrinsic}, for the inverse function.

@xref{Log10 Intrinsic}, for the base-10 logarithm function.
")

DEFDOC (ALOG, "Natural logarithm (archaic).", "\
Archaic form of @code{LOG()} that is specific
to one type for @var{@1@}.
@xref{Log Intrinsic}.
")

DEFDOC (CLOG, "Natural logarithm (archaic).", "\
Archaic form of @code{LOG()} that is specific
to one type for @var{@1@}.
@xref{Log Intrinsic}.
")

DEFDOC (DLOG, "Natural logarithm (archaic).", "\
Archaic form of @code{LOG()} that is specific
to one type for @var{@1@}.
@xref{Log Intrinsic}.
")

DEFDOC (CDLOG, "Natural logarithm (archaic).", "\
Archaic form of @code{LOG()} that is specific
to one type for @var{@1@}.
@xref{Log Intrinsic}.
")

DEFDOC (LOG10, "Natural logarithm.", "\
Returns the natural logarithm of @var{@1@}, which must
be greater than zero or, if type @code{COMPLEX}, must not
be zero.

The inverse function is @samp{10. ** LOG10(@var{@1@})}.

@xref{Log Intrinsic}, for the natural logarithm function.
")

DEFDOC (ALOG10, "Natural logarithm (archaic).", "\
Archaic form of @code{LOG10()} that is specific
to one type for @var{@1@}.
@xref{Log10 Intrinsic}.
")

DEFDOC (DLOG10, "Natural logarithm (archaic).", "\
Archaic form of @code{LOG10()} that is specific
to one type for @var{@1@}.
@xref{Log10 Intrinsic}.
")

DEFDOC (MAX, "Maximum value.", "\
Returns the argument with the largest value.

@xref{Min Intrinsic}, for the opposite function.
")

DEFDOC (AMAX0, "Maximum value (archaic).", "\
Archaic form of @code{MAX()} that is specific
to one type for @var{@1@} and a different return type.
@xref{Max Intrinsic}.
")

DEFDOC (AMAX1, "Maximum value (archaic).", "\
Archaic form of @code{MAX()} that is specific
to one type for @var{@1@}.
@xref{Max Intrinsic}.
")

DEFDOC (DMAX1, "Maximum value (archaic).", "\
Archaic form of @code{MAX()} that is specific
to one type for @var{@1@}.
@xref{Max Intrinsic}.
")

DEFDOC (MAX0, "Maximum value (archaic).", "\
Archaic form of @code{MAX()} that is specific
to one type for @var{@1@}.
@xref{Max Intrinsic}.
")

DEFDOC (MAX1, "Maximum value (archaic).", "\
Archaic form of @code{MAX()} that is specific
to one type for @var{@1@} and a different return type.
@xref{Max Intrinsic}.
")

DEFDOC (MIN, "Minimum value.", "\
Returns the argument with the smallest value.

@xref{Max Intrinsic}, for the opposite function.
")

DEFDOC (AMIN0, "Minimum value (archaic).", "\
Archaic form of @code{MIN()} that is specific
to one type for @var{@1@} and a different return type.
@xref{Min Intrinsic}.
")

DEFDOC (AMIN1, "Minimum value (archaic).", "\
Archaic form of @code{MIN()} that is specific
to one type for @var{@1@}.
@xref{Min Intrinsic}.
")

DEFDOC (DMIN1, "Minimum value (archaic).", "\
Archaic form of @code{MIN()} that is specific
to one type for @var{@1@}.
@xref{Min Intrinsic}.
")

DEFDOC (MIN0, "Minimum value (archaic).", "\
Archaic form of @code{MIN()} that is specific
to one type for @var{@1@}.
@xref{Min Intrinsic}.
")

DEFDOC (MIN1, "Minimum value (archaic).", "\
Archaic form of @code{MIN()} that is specific
to one type for @var{@1@} and a different return type.
@xref{Min Intrinsic}.
")

DEFDOC (MOD, "Remainder.", "\
Returns remainder calculated as:

@smallexample
@var{@1@} - (INT(@var{@1@} / @var{@2@}) * @var{@2@})
@end smallexample

@var{@2@} must not be zero.
")

DEFDOC (AMOD, "Remainder (archaic).", "\
Archaic form of @code{MOD()} that is specific
to one type for @var{@1@}.
@xref{Mod Intrinsic}.
")

DEFDOC (DMOD, "Remainder (archaic).", "\
Archaic form of @code{MOD()} that is specific
to one type for @var{@1@}.
@xref{Mod Intrinsic}.
")

DEFDOC (AND, "Boolean AND.", "\
Returns value resulting from boolean AND of
pair of bits in each of @var{@1@} and @var{@2@}.
")

DEFDOC (IAND, "Boolean AND.", "\
Returns value resulting from boolean AND of
pair of bits in each of @var{@1@} and @var{@2@}.
")

DEFDOC (OR, "Boolean OR.", "\
Returns value resulting from boolean OR of
pair of bits in each of @var{@1@} and @var{@2@}.
")

DEFDOC (IOR, "Boolean OR.", "\
Returns value resulting from boolean OR of
pair of bits in each of @var{@1@} and @var{@2@}.
")

DEFDOC (XOR, "Boolean XOR.", "\
Returns value resulting from boolean exclusive-OR of
pair of bits in each of @var{@1@} and @var{@2@}.
")

DEFDOC (IEOR, "Boolean XOR.", "\
Returns value resulting from boolean exclusive-OR of
pair of bits in each of @var{@1@} and @var{@2@}.
")

DEFDOC (NOT, "Boolean NOT.", "\
Returns value resulting from boolean NOT of each bit
in @var{@1@}.
")

DEFDOC (ASIN, "Arc sine.", "\
Returns the arc-sine (inverse sine) of @var{@1@}
in radians.

@xref{Sin Intrinsic}, for the inverse function.
")

DEFDOC (DASIN, "Arc sine (archaic).", "\
Archaic form of @code{ASIN()} that is specific
to one type for @var{@1@}.
@xref{ASin Intrinsic}.
")

DEFDOC (ATAN, "Arc tangent.", "\
Returns the arc-tangent (inverse tangent) of @var{@1@}
in radians.

@xref{Tan Intrinsic}, for the inverse function.
")

DEFDOC (DATAN, "Arc tangent (archaic).", "\
Archaic form of @code{ATAN()} that is specific
to one type for @var{@1@}.
@xref{ATan Intrinsic}.
")

DEFDOC (ATAN2, "Arc tangent.", "\
Returns the arc-tangent (inverse tangent) of the complex
number (@var{@1@}, @var{@2@}) in radians.

@xref{Tan Intrinsic}, for the inverse function.
")

DEFDOC (DATAN2, "Arc tangent (archaic).", "\
Archaic form of @code{ATAN2()} that is specific
to one type for @var{@1@} and @var{@2@}.
@xref{ATan2 Intrinsic}.
")

DEFDOC (BIT_SIZE, "Number of bits in argument's type.", "\
Returns the number of bits (integer precision plus sign bit)
represented by the type for @var{@1@}.

@xref{BTest Intrinsic}, for how to test the value of a
bit in a variable or array.

@xref{IBSet Intrinsic}, for how to set a bit in a
variable or array to 1.
")

DEFDOC (BTEST, "Test bit.", "\
Returns @code{.TRUE.} if bit @var{@2@} in @var{@1@} is
1, @code{.FALSE.} otherwise.

(Bit 0 is the low-order bit, adding the value 2**0, or 1,
to the number if set to 1;
bit 1 is the next-higher-order bit, adding 2**1, or 2;
bit 2 adds 2**2, or 4; and so on.)

@xref{Bit_Size Intrinsic}, for how to obtain the number of bits
in a type.
")

DEFDOC (CMPLX, "Construct @code{COMPLEX(KIND=1)} value.", "\
If @var{@1@} is not type @code{COMPLEX},
constructs a value of type @code{COMPLEX(KIND=1)} from the
real and imaginary values specified by @var{@1@} and
@var{@2@}, respectively.
If @var{@2@} is omitted, @samp{0.} is assumed.

If @var{@1@} is type @code{COMPLEX},
converts it to type @code{COMPLEX(KIND=1)}.

@xref{Complex Intrinsic}, for information on easily constructing
a @code{COMPLEX} value of arbitrary precision from @code{REAL}
arguments.
")

DEFDOC (CONJG, "Complex conjugate.", "\
Returns the complex conjugate:

@example
COMPLEX(REALPART(@var{@1@}), -IMAGPART(@var{@1@}))
@end example
")

DEFDOC (DCONJG, "Complex conjugate (archaic).", "\
Archaic form of @code{CONJG()} that is specific
to one type for @var{@1@}.
@xref{ATan2 Intrinsic}.
")

/* ~~~~~ to do:
   COS
   COSH
   SQRT
   DBLE
   DIM
   ERF
   DPROD
   SIGN
   EXP
   FLOAT
   IBCLR
   IBITS
   IBSET
   IFIX
   INDEX
   ISHFT
   ISHFTC
   LEN
   LGE
   LONG
   SHORT
   LSHIFT
   RSHIFT
   MVBITS
   SIN
   SINH
   SNGL
   TAN
   TANH
*/

DEFDOC (REAL, "Convert value to type @code{REAL(KIND=1)}.", "\
Converts @var{@1@} to @code{REAL(KIND=1)}.

Use of @code{@0@()} with a @code{COMPLEX} argument
(other than @code{COMPLEX(KIND=1)}) is restricted to the following case:

@example
REAL(REAL(@1@))
@end example

@noindent
This expression converts the real part of @1@ to
@code{REAL(KIND=1)}.

@xref{REAL() and AIMAG() of Complex}, for more information.
")

DEFDOC (IMAGPART, "Extract imaginary part of complex.", "\
The imaginary part of @var{@1@} is returned, without conversion.

@emph{Note:} The way to do this in standard Fortran 90
is @samp{AIMAG(@var{@1@})}.
However, when, for example, @var{@1@} is @code{DOUBLE COMPLEX},
@samp{AIMAG(@var{@1@})} means something different for some compilers
that are not true Fortran 90 compilers but offer some
extensions standardized by Fortran 90 (such as the
@code{DOUBLE COMPLEX} type, also known as @code{COMPLEX(KIND=2)}).

The advantage of @code{@0@()} is that, while not necessarily
more or less portable than @code{AIMAG()}, it is more likely to
cause a compiler that doesn't support it to produce a diagnostic
than generate incorrect code.

@xref{REAL() and AIMAG() of Complex}, for more information.
")

DEFDOC (COMPLEX, "Build complex value from real and@99@imaginary parts.", "\
Returns a @code{COMPLEX} value that has @samp{@1@} and @samp{@2@} as its
real and imaginary parts, respectively.

If @var{@1@} and @var{@2@} are the same type, and that type is not
@code{INTEGER}, no data conversion is performed, and the type of
the resulting value has the same kind value as the types
of @var{@1@} and @var{@2@}.

If @var{@1@} and @var{@2@} are not the same type, the usual type-promotion
rules are applied to both, converting either or both to the
appropriate @code{REAL} type.
The type of the resulting value has the same kind value as the
type to which both @var{@1@} and @var{@2@} were converted, in this case.

If @var{@1@} and @var{@2@} are both @code{INTEGER}, they are both converted
to @code{REAL(KIND=1)}, and the result of the @code{@0@()}
invocation is type @code{COMPLEX(KIND=1)}.

@emph{Note:} The way to do this in standard Fortran 90
is too hairy to describe here, but it is important to
note that @samp{CMPLX(D1,D2)} returns a @code{COMPLEX(KIND=1)}
result even if @samp{D1} and @samp{D2} are type @code{REAL(KIND=2)}.
Hence the availability of @code{COMPLEX()} in GNU Fortran.
")

DEFDOC (LOC, "Address of entity in core.", "\
The @code{LOC()} intrinsic works the
same way as the @code{%LOC()} construct.
@xref{%LOC(),,The @code{%LOC()} Construct}, for
more information.
")

DEFDOC (REALPART, "Extract real part of complex.", "\
The real part of @var{@1@} is returned, without conversion.

@emph{Note:} The way to do this in standard Fortran 90
is @samp{REAL(@var{@1@})}.
However, when, for example, @var{@1@} is @code{COMPLEX(KIND=2)},
@samp{REAL(@var{@1@})} means something different for some compilers
that are not true Fortran 90 compilers but offer some
extensions standardized by Fortran 90 (such as the
@code{DOUBLE COMPLEX} type, also known as @code{COMPLEX(KIND=2)}).

The advantage of @code{@0@()} is that, while not necessarily
more or less portable than @code{REAL()}, it is more likely to
cause a compiler that doesn't support it to produce a diagnostic
than generate incorrect code.

@xref{REAL() and AIMAG() of Complex}, for more information.
")

DEFDOC (GETARG, "Obtain command-line argument.", "\
Sets @var{@2@} to the @var{@1@}-th command-line argument (or to all
blanks if there are fewer than @var{@2@} command-line arguments);
@code{CALL @0@(0, @var{value})} sets @var{value} to the name of the
program (on systems that support this feature).

@xref{IArgC Intrinsic}, for information on how to get the number
of arguments.
")

DEFDOC (ABORT, "Abort the program.", "\
Prints a message and potentially causes a core dump via @code{abort(3)}.
")

DEFDOC (EXIT, "Terminate the program.", "\
Exit the program with status @var{@1@} after closing open Fortran
i/o units and otherwise behaving as @code{exit(2)}.  If @var{@1@}
is omitted the canonical `success' value will be returned to the
system.
")

DEFDOC (IARGC, "Obtain count of command-line arguments.", "\
Returns the number of command-line arguments.

This count does not include the specification of the program
name itself.
")

DEFDOC (CTIME, "Convert time to Day Mon dd hh:mm:ss yyyy.", "\
Converts @var{@1@}, a system time value, such as returned by
@code{TIME()}, to a string of the form @samp{Sat Aug 19 18:13:14 1995}.

@xref{Time Intrinsic}.
")

DEFDOC (DATE, "Get current date as dd-Mon-yy.", "\
Returns @var{@1@} in the form @samp{@var{dd}-@var{mmm}-@var{yy}},
representing the numeric day of the month @var{dd}, a three-character
abbreviation of the month name @var{mmm} and the last two digits of
the year @var{yy}, e.g.@ @samp{25-Nov-96}.

This intrinsic is not recommended, due to the year 2000 approaching.
@xref{CTime Intrinsic}, for information on obtaining more digits
for the current (or any) date.
")

DEFDOC (DTIME, "Get elapsed time since last time.", "\
Initially, return in seconds the runtime (since the start of the
process' execution) as the function value and the user and system
components of this in @samp{@var{@1@}(1)} and @samp{@var{@1@}(2)}
respectively.
The functions' value is equal to @samp{@var{@1@}(1) + @samp{@1@}(2)}.

Subsequent invocations of @samp{@0@()} return values accumulated since the
previous invocation.
")

DEFDOC (ETIME, "Get elapsed time for process.", "\
Return in seconds the runtime (since the start of the process'
execution) as the function value and the user and system components of
this in @samp{@var{@1@}(1)} and @samp{@var{@1@}(2)} respectively.
The functions' value is equal to @samp{@var{@1@}(1) + @var{@1@}(2)}.
")

DEFDOC (FDATE, "Get current time as Day Mon dd hh:mm:ss yyyy.", "\
Returns the current date in the same format as @code{CTIME()}.

Equivalent to:

@example
CTIME(TIME())
@end example

@xref{CTime Intrinsic}.
")

DEFDOC (GMTIME, "Convert time to GMT time info.", "\
Given a system time value @var{@1@}, fills @var{@2@} with values
extracted from it appropriate to the GMT time zone using
@code{gmtime(3)}.

The array elements are as follows:

@enumerate
@item
Seconds after the minute, range 0--59 or 0--61 to allow for leap
seconds

@item
Minutes after the hour, range 0--59

@item
Hours past midnight, range 0--23

@item
Day of month, range 0--31

@item
Number of months since January, range 0--12

@item
Number of days since Sunday, range 0--6

@item
Years since 1900

@item
Days since January 1

@item
Daylight savings indicator: positive if daylight savings is in effect,
zero if not, and negative if the information isn't available.
@end enumerate
")

DEFDOC (LTIME, "Convert time to local time info.", "\
Given a system time value @var{@1@}, fills @var{@2@} with values
extracted from it appropriate to the GMT time zone using
@code{localtime(3)}.

The array elements are as follows:

@enumerate
@item
Seconds after the minute, range 0--59 or 0--61 to allow for leap
seconds

@item
Minutes after the hour, range 0--59

@item
Hours past midnight, range 0--23

@item
Day of month, range 0--31

@item
Number of months since January, range 0--12

@item
Number of days since Sunday, range 0--6

@item
Years since 1900

@item
Days since January 1

@item
Daylight savings indicator: positive if daylight savings is in effect,
zero if not, and negative if the information isn't available.
@end enumerate
")

DEFDOC (IDATE, "Get local time info.", "\
Fills @var{@1@} with the numerical values at the current local time
of day, month (in the range 1--12), and year in elements 1, 2, and 3,
respectively.
The year has four significant digits.
")

DEFDOC (IDATEVXT, "Get local time info (VAX/VMS).", "\
Returns the numerical values of the current local time.
The date is returned in @var{@1@},
the month in @var{@2@} (in the range 1--12),
and the year in @var{@3@} (in the range 0--99).

This intrinsic is not recommended, due to the year 2000 approaching.
@xref{IDate Intrinsic}, for information on obtaining more digits
for the current local date.
")

DEFDOC (ITIME, "Get local time of day.", "\
Returns the current local time hour, minutes, and seconds in elements
1, 2, and 3 of @var{@1@}, respectively.
")

DEFDOC (MCLOCK, "Get number of clock ticks for process.", "\
Returns the number of clock ticks since the start of the process.
Only defined on systems with @code{clock(3)} (q.v.).
")

DEFDOC (SECNDS, "Get local time offset since midnight.", "\
Returns the local time in seconds since midnight minus the value
@var{@1@}.
")

DEFDOC (SECONDFUNC, "Get CPU time for process in seconds.", "\
Returns the process' runtime in seconds---the same value as the
UNIX function @code{etime} returns.

This routine is known from Cray Fortran.
")

DEFDOC (SECONDSUBR, "Get CPU time for process@99@in seconds.", "\
Returns the process' runtime in seconds in @var{@1@}---the same value
as the UNIX function @code{etime} returns.

This routine is known from Cray Fortran.
")

DEFDOC (SYSTEM_CLOCK, "Get current system clock value.", "\
Returns in @var{@1@} the current value of the system clock; this is
the value returned by the UNIX function @code{times(2)}
in this implementation, but
isn't in general.
@var{@2@} is the number of clock ticks per second and
@var{@3@} is the maximum value this can take, which isn't very useful
in this implementation since it's just the maximum C @code{unsigned
int} value.
")

DEFDOC (TIME, "Get current time as time value.", "\
Returns the current time encoded as an integer in the manner of
the UNIX function @code{time(3)}.
This value is suitable for passing to @code{CTIME},
@code{GMTIME}, and @code{LTIME}.
")

#define BES(num,n) "\
Calculates the Bessel function of the " #num " kind of \
order " #n ".\n\
See @code{bessel(3m)}, on whose implementation the \
function depends.\
"

DEFDOC (BESJ0, "Bessel function.", BES (first, 0))
DEFDOC (BESJ1, "Bessel function.", BES (first, 1))
DEFDOC (BESJN, "Bessel function.", BES (first, @var{N}))
DEFDOC (BESY0, "Bessel function.", BES (second, 0))
DEFDOC (BESY1, "Bessel function.", BES (second, 1))
DEFDOC (BESYN, "Bessel function.", BES (second, @var{N}))

DEFDOC (ERF, "Error function.", "\
Returns the error function of @var{@1@}.
See @code{erf(3m)}, which provides the implementation.
")

DEFDOC (ERFC, "Complementary error function.", "\
Returns the complementary error function of @var{@1@}:
@code{ERFC(R) = 1 - ERF(R)} (except that the result may be more
accurate than explicitly evaluating that formulae would give).
See @code{erfc(3m)}, which provides the implementation.
")

DEFDOC (IRAND, "Random number.", "\
Returns a uniform quasi-random number up to a system-dependent limit.
If @var{@1@} is 0, the next number in sequence is returned; if
@var{@1@} is 1, the generator is restarted by calling the UNIX function
@samp{srand(0)}; if @var{@1@} has any other value,
it is used as a new seed with @code{srand()}.

@xref{SRand Intrinsic}.

@emph{Note:} As typically implemented (by the routine of the same
name in the C library), this random number generator is a very poor
one, though the BSD and GNU libraries provide a much better
implementation than the `traditional' one.
On a different system you almost certainly want to use something better.
")

DEFDOC (RAND, "Random number.", "\
Returns a uniform quasi-random number between 0 and 1.
If @var{@1@} is 0, the next number in sequence is returned; if
@var{@1@} is 1, the generator is restarted by calling @samp{srand(0)};
if @var{@1@} has any other value, it is used as a new seed with
@code{srand}.

@xref{SRand Intrinsic}.

@emph{Note:} As typically implemented (by the routine of the same
name in the C library), this random number generator is a very poor
one, though the BSD and GNU libraries provide a much better
implementation than the `traditional' one.
On a different system you
almost certainly want to use something better.
")

DEFDOC (SRAND, "Random seed.", "\
Reinitialises the generator with the seed in @var{@1@}.
@xref{IRand Intrinsic}.  @xref{Rand Intrinsic}.
")

DEFDOC (ACCESS, "Check file accessibility.", "\
Checks file @var{@1@} for accessibility in the mode specified by @var{@2@} and
returns 0 if the file is accessible in that mode, otherwise an error
code if the file is inaccessible or @var{@2@} is invalid.  See
@code{access(2)}.  @var{@2@} may be a concatenation of any of the
following characters:

@table @samp
@item r
Read permission

@item w
Write permission

@item x
Execute permission

@item @kbd{SPC}
Existence
@end table
")

DEFDOC (CHDIR, "Change directory.", "\
Sets the current working directory to be @var{@1@}.
If the @var{@2@} argument is supplied, it contains 0
on success or an error code otherwise upon return.
See @code{chdir(3)}.
")

DEFDOC (CHMOD, "Change file modes.", "\
Changes the access mode of file @var{@1@} according to the
specification @var{@2@}, which is given in the format of
@code{chmod(1)}.
If the @var{Status} argument is supplied, it contains 0
on success or an error code otherwise upon return.
Note that this currently works
by actually invoking @code{/bin/chmod} (or the @code{chmod} found when
the library was configured) and so may fail in some circumstances and
will, anyway, be slow.
")

DEFDOC (GETCWD, "Get current working directory.", "\
Places the current working directory in @var{@1@}.
Returns 0 on
success, otherwise an error code.
")

DEFDOC (FSTAT, "Get file information.", "\
Obtains data about the file open on Fortran I/O unit @var{@1@} and
places them in the array @var{@2@}.
The values in this array are
extracted from the @code{stat} structure as returned by
@code{fstat(2)} q.v., as follows:

@enumerate
@item
File mode

@item
Inode number

@item
ID of device containing directory entry for file

@item
Device id (if relevant)

@item
Number of links

@item
Owner's uid

@item
Owner's gid

@item
File size (bytes)

@item
Last access time

@item
Last modification time

@item
Last file status change time

@item
Preferred i/o block size

@item
Number of blocks allocated
@end enumerate

Not all these elements are relevant on all systems.
If an element is not relevant, it is returned as 0.

Returns 0 on success, otherwise an error number.
")

DEFDOC (LSTAT, "Get file information.", "\
Obtains data about the given @var{@1@} and places them in the array
@var{@2@}.
If @var{@1@} is a symbolic link it returns data on the
link itself, so the routine is available only on systems that support
symbolic links.
The values in this array are extracted from the
@code{stat} structure as returned by @code{fstat(2)} q.v., as follows:

@enumerate
@item
File mode

@item
Inode number

@item
ID of device containing directory entry for file

@item
Device id (if relevant)

@item
Number of links

@item
Owner's uid

@item
Owner's gid

@item
File size (bytes)

@item
Last access time

@item
Last modification time

@item
Last file status change time

@item
Preferred i/o block size

@item
Number of blocks allocated
@end enumerate

Not all these elements are relevant on all systems.
If an element is not relevant, it is returned as 0.

Returns 0 on success, otherwise an error number.
")

DEFDOC (STAT, "Get file information.", "\
Obtains data about the given @var{@1@} and places them in the array
@var{@2@}.
The values in this array are extracted from the
@code{stat} structure as returned by @code{fstat(2)} q.v., as follows:

@enumerate
@item
File mode

@item
Inode number

@item
ID of device containing directory entry for file

@item
Device id (if relevant)

@item
Number of links

@item
Owner's uid

@item
Owner's gid

@item
File size (bytes)

@item
Last access time

@item
Last modification time

@item
Last file status change time

@item
Preferred i/o block size

@item
Number of blocks allocated
@end enumerate

Not all these elements are relevant on all systems.
If an element is not relevant, it is returned as 0.

Returns 0 on success, otherwise an error number.
")

DEFDOC (LINK, "Make hard link in file system.", "\
Makes a (hard) link from @var{@1@} to @var{@2@}.
If the
@var{@3@} argument is supplied, it contains 0 on success or an error
code otherwise.
See @code{link(2)}.
")

DEFDOC (SYMLNK, "Make symbolic link in file system.", "\
Makes a symbolic link from @var{@1@} to @var{@2@}.
If the
@var{@3@} argument is supplied, it contains 0 on success or an error
code otherwise.
Available only on systems that support symbolic
links (see @code{symlink(2)}).
")

DEFDOC (RENAME, "Rename file.", "\
Renames the file @var{@1@} to @var{@2@}.
See @code{rename(2)}.
If the @var{@3@} argument is supplied, it contains 0 on success or an
error code otherwise upon return.
")

DEFDOC (UMASK, "Set file creation permissions mask.", "\
Sets the file creation mask to @var{@2@} and returns the old value in
argument @var{@2@} if it is supplied.
See @code{umask(2)}.
")

DEFDOC (UNLINK, "Unlink file.", "\
Unlink the file @var{@1@}.
If the @var{@2@} argument is supplied, it
contains 0 on success or an error code otherwise.
See @code{unlink(2)}.
")

DEFDOC (GERROR, "Get error message for last error.", "\
Returns the system error message corresponding to the last system
error (C @code{errno}).
")

DEFDOC (IERRNO, "Get error number for last error.", "\
Returns the last system error number (corresponding to the C
@code{errno}).
")

DEFDOC (PERROR, "Print error message for last error.", "\
Prints (on the C @code{stderr} stream) a newline-terminated error
message corresponding to the last system error.
This is prefixed by @var{@1@}, a colon and a space.
See @code{perror(3)}.
")
 
DEFDOC (GETGID, "Get process group id.", "\
Returns the group id for the current process.
")
 
DEFDOC (GETUID, "Get process user id.", "\
Returns the user id for the current process.
")
 
DEFDOC (GETPID, "Get process id.", "\
Returns the process id for the current process.
")

DEFDOC (GETENV, "Get environment variable.", "\
Sets @var{@2@} to the value of environment variable given by the
value of @var{@1@} (@code{$name} in shell terms) or to blanks if
@code{$name} has not been set.
")

DEFDOC (GETLOG, "Get login name.", "\
Returns the login name for the process in @var{@1@}.
")

DEFDOC (HOSTNM, "Get host name.", "\
Fills @var{@1@} with the system's host name returned by
@code{gethostname(2)}, returning 0 on success or an error code. 
This function is not available on all systems.
")

/* Fixme: stream i/o */

DEFDOC (FLUSH, "Flush buffered output.", "\
Flushes Fortran unit(s) currently open for output.
Without the optional argument, all such units are flushed,
otherwise just the unit specified by @var{@1@}.
")

DEFDOC (FNUM, "Get file descriptor from Fortran unit number.", "\
Returns the Unix file descriptor number corresponding to the open
Fortran I/O unit @var{@1@}.
This could be passed to an interface to C I/O routines.
")

DEFDOC (FSEEK, "Position file (low-level).", "\
Attempts to move Fortran unit @var{@1@} to the specified
@var{Offset}: absolute offset if @var{@2@}=0; relative to the
current offset if @var{@2@}=1; relative to the end of the file if
@var{@2@}=2.
It branches to label @var{@3@} if @var{@1@} is
not open or if the call otherwise fails.
")

DEFDOC (FTELL, "Get file position (low-level).", "\
Returns the current offset of Fortran unit @var{@1@} (or @minus{}1 if
@var{@1@} is not open).
")

DEFDOC (ISATTY, "Is unit connected to a terminal?", "\
Returns @code{.TRUE.} if and only if the Fortran I/O unit
specified by @var{@1@} is connected
to a terminal device.
See @code{isatty(3)}.
")

DEFDOC (TTYNAM, "Get name of terminal device for unit.", "\
Returns the name of the terminal device open on logical unit
@var{@1@} or a blank string if @var{@1@} is not connected to a
terminal.
")

DEFDOC (SIGNAL, "Muck with signal handling.", "\
If @var{@2@} is a an @code{EXTERNAL} routine, arranges for it to be
invoked with a single integer argument (of system-dependent length)
when signal @var{@1@} occurs.
If @var{@1@} is an integer it can be
used to turn off handling of signal @var{@2@} or revert to its default
action.
See @code{signal(2)}.

Note that @var{@2@} will be called with C conventions, so its value in
Fortran terms is obtained by applying @code{%loc} (or @var{loc}) to it.
")

DEFDOC (KILL, "Signal a process.", "\
Sends the signal specified by @var{@2@} to the process @var{@1@}.  Returns zero
on success, otherwise an error number.
See @code{kill(2)}.
")

DEFDOC (LNBLNK, "Get last non-blank character in string.", "\
Returns the index of the last non-blank character in @var{@1@}.
@code{LNBLNK} and @code{LEN_TRIM} are equivalent.
")

DEFDOC (SLEEP, "Sleep for a specified time.", "\
Causes the process to pause for @var{@1@} seconds.
See @code{sleep(2)}.
")

DEFDOC (SYSTEM, "Invoke shell (system) command.", "\
Passes the command @var{@1@} to a shell (see @code{system(3)}).
If argument @var{@2@} is present, it contains the value returned by
@code{system(3)}, presumably 0 if the shell command succeeded.
Note that which shell is used to invoke the command is system-dependent
and environment-dependent.
")
