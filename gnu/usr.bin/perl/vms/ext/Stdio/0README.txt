This directory contains the source code for the Perl extension
VMS::Stdio, which provides access from Perl to VMS-specific
stdio functions.  For more specific documentation of its
function, please see the pod section of Stdio.pm.

                       *** Please Note ***

This package is the direct descendant of VMS::stdio, but as of Perl
5.002, the name has been changed to VMS::Stdio, in order to conform
to the Perl naming convention that extensions whose name begins
with a lowercase letter represent compile-time "pragmas", while
extensions which provide added functionality have names whose parts
begin with uppercase letters.  In addition, the functions
vmsfopen and fgetname have been renamed vmsopen and getname,
respectively, in order to more closely resemble related Perl
I/O operators, which do not retain the 'f' from corresponding
C routine names.

A transitional interface to the old routine names has been
provided, so that calls to these routines will generate a
warning, and be routed to the corresponding VMS::Stdio
routine.  This interface will be removed in a future release,
so please update your code to use the new names.


===> Installation

This extension, like most Perl extensions, should be installed
by copying the files in this directory to a location *outside*
the Perl distribution tree, and then saying

    $ perl Makefile.PL  ! Build Descrip.MMS for this extension
    $ MMK               ! Build the extension
    $ MMK test          ! Run its regression tests
    $ MMK install       ! Install required files in public Perl tree


===> Revision History

1.0  29-Nov-1994  Charles Bailey  bailey@genetics.upenn.edu
     original version - vmsfopen
1.1  09-Mar-1995  Charles Bailey  bailey@genetics.upenn.edu
     changed calling sequence to return FH/undef - like POSIX::open
     added fgetname and tmpnam
2.0  28-Feb-1996  Charles Bailey  bailey@genetics.upenn.edu
     major rewrite for Perl 5.002: name changed to VMS::Stdio,
     new functions added, and prototypes incorporated
