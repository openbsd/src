#   VMS::Stdio - VMS extensions to Perl's stdio calls
#
#   Author:  Charles Bailey  bailey@genetics.upenn.edu
#   Version: 2.0
#   Revised: 28-Feb-1996

package VMS::Stdio;

require 5.002;
use vars qw( $VERSION @EXPORT @EXPORT_OK %EXPORT_TAGS @ISA );
use Carp '&croak';
use DynaLoader ();
use Exporter ();
 
$VERSION = '2.0';
@ISA = qw( Exporter DynaLoader FileHandle );
@EXPORT = qw( &O_APPEND &O_CREAT &O_EXCL  &O_NDELAY &O_NOWAIT
              &O_RDONLY &O_RDWR  &O_TRUNC &O_WRONLY );
@EXPORT_OK = qw( &flush &getname &remove &rewind &sync &tmpnam
                 &vmsopen &vmssysopen &waitfh );
%EXPORT_TAGS = ( CONSTANTS => [ qw( &O_APPEND &O_CREAT &O_EXCL  &O_NDELAY
                                    &O_NOWAIT &O_RDONLY &O_RDWR &O_TRUNC
                                    &O_WRONLY ) ],
                 FUNCTIONS => [ qw( &flush &getname &remove &rewind &sync
                                     &tmpnam &vmsopen &vmssysopen &waitfh ) ] );

bootstrap VMS::Stdio $VERSION;

sub AUTOLOAD {
    my($constname) = $AUTOLOAD;
    $constname =~ s/.*:://;
    if ($constname =~ /^O_/) {
      my($val) = constant($constname);
      defined $val or croak("Unknown VMS::Stdio constant $constname");
      *$AUTOLOAD = sub { $val };
    }
    else { # We don't know about it; hand off to FileHandle
      require FileHandle;
      my($obj) = shift(@_);
      $obj->FileHandle::$constname(@_);
    }
    goto &$AUTOLOAD;
}

sub DESTROY { close($_[0]); }


################################################################################
# Intercept calls to old VMS::stdio package, complain, and hand off
# This will be removed in a future version of VMS::Stdio

package VMS::stdio;

sub AUTOLOAD {
  my($func) = $AUTOLOAD;
  $func =~ s/.*:://;
  # Cheap trick: we know DynaLoader has required Carp.pm
  Carp::carp("Old package VMS::stdio is now VMS::Stdio; please update your code");
  if ($func eq 'vmsfopen') {
    Carp::carp("Old function &vmsfopen is now &vmsopen");
    goto &VMS::Stdio::vmsopen;
  }
  elsif ($func eq 'fgetname') {
    Carp::carp("Old function &fgetname is now &getname");
    goto &VMS::Stdio::getname;
  }
  else { goto &{"VMS::Stdio::$func"}; }
}

package VMS::Stdio;  # in case we ever use AutoLoader

1;

__END__

=head1 NAME

VMS::Stdio

=head1 SYNOPSIS

use VMS::Stdio qw( &flush &getname &remove &rewind &sync &tmpnam
                   &vmsopen &vmssysopen &waitfh );
$uniquename = tmpnam;
$fh = vmsopen("my.file","rfm=var","alq=100",...) or die $!;
$name = getname($fh);
print $fh "Hello, world!\n";
flush($fh);
sync($fh);
rewind($fh);
$line = <$fh>;
undef $fh;  # closes file
$fh = vmssysopen("another.file", O_RDONLY | O_NDELAY, 0, "ctx=bin");
sysread($fh,$data,128);
waitfh($fh);
close($fh);
remove("another.file");

=head1 DESCRIPTION

This package gives Perl scripts access to VMS extensions to several
C stdio operations not available through Perl's CORE I/O functions.
The specific routines are described below.  These functions are
prototyped as unary operators, with the exception of C<vmsopen>
and C<vmssysopen>, which can take any number of arguments, and
C<tmpnam>, which takes none.

All of the routines are available for export, though none are
exported by default.  All of the constants used by C<vmssysopen>
to specify access modes are exported by default.  The routines
are associated with the Exporter tag FUNCTIONS, and the constants
are associated with the Exporter tag CONSTANTS, so you can more
easily choose what you'd like to import:

    # import constants, but not functions
    use VMS::Stdio;  # same as use VMS::Stdio qw( :DEFAULT );
    # import functions, but not constants
    use VMS::Stdio qw( !:CONSTANTS :FUNCTIONS ); 
    # import both
    use VMS::Stdio qw( :CONSTANTS :FUNCTIONS ); 
    # import neither
    use VMS::Stdio ();

Of course, you can also choose to import specific functions by
name, as usual.

This package C<ISA> FileHandle, so that you can call FileHandle
methods on the handles returned by C<vmsopen> and C<vmssysopen>.
The FileHandle package is not initialized, however, until you
actually call a method that VMS::Stdio doesn't provide.  This
is doen to save startup time for users who don't wish to use
the FileHandle methods.

B<Note:>  In order to conform to naming conventions for Perl
extensions and functions, the name of this package has been
changed to VMS::Stdio as of Perl 5.002, and the names of some
routines have been changed.  Calls to the old VMS::stdio routines
will generate a warning, and will be routed to the equivalent
VMS::Stdio function.  This compatibility interface will be
removed in a future release of this extension, so please
update your code to use the new routines.

=item flush

This function causes the contents of stdio buffers for the specified
file handle to be flushed.  If C<undef> is used as the argument to
C<flush>, all currently open file handles are flushed.  Like the CRTL
fflush() routine, it does not flush any underlying RMS buffers for the
file, so the data may not be flushed all the way to the disk.  C<flush>
returns a true value if successful, and C<undef> if not.

=item getname

The C<getname> function returns the file specification associated
with a Perl FileHandle.  If an error occurs, it returns C<undef>.

=item remove

This function deletes the file named in its argument, returning
a true value if successful and C<undef> if not.  It differs from
the CORE Perl function C<unlink> in that it does not try to
reset file protection if the original protection does not give
you delete access to the file (cf. L<perlvms>).  In other words,
C<remove> is equivalent to

  unlink($file) if VMS::Filespec::candelete($file);

=item rewind

C<rewind> resets the current position of the specified file handle
to the beginning of the file.  It's really just a convenience
method equivalent in effect to C<seek($fh,0,0)>.  It returns a
true value if successful, and C<undef> if it fails.

=item sync

This function flushes buffered data for the specified file handle
from stdio and RMS buffers all the way to disk.  If successful, it
returns a true value; otherwise, it returns C<undef>.

=item tmpnam

The C<tmpnam> function returns a unique string which can be used
as a filename when creating temporary files.  If, for some
reason, it is unable to generate a name, it returns C<undef>.

=item vmsopen

The C<vmsopen> function enables you to specify optional RMS arguments
to the VMS CRTL when opening a file.  It is similar to the built-in
Perl C<open> function (see L<perlfunc> for a complete description),
but will only open normal files; it cannot open pipes or duplicate
existing FileHandles.  Up to 8 optional arguments may follow the
file name.  These arguments should be strings which specify
optional file characteristics as allowed by the CRTL. (See the
CRTL reference manual description of creat() and fopen() for details.)
If successful, C<vmsopen> returns a VMS::Stdio file handle; if an
error occurs, it returns C<undef>.

You can use the file handle returned by C<vmsfopen> just as you
would any other Perl file handle.  The class VMS::Stdio ISA
FileHandle, so you can call FileHandle methods using the handle
returned by C<vmsopen>.  However, C<use>ing VMS::Stdio does not
automatically C<use> FileHandle; you must do so explicitly in
your program if you want to call FileHandle methods.  This is
done to avoid the overhead of initializing the FileHandle package
in programs which intend to use the handle returned by C<vmsopen>
as a normal Perl file handle only.  When the scalar containing
a VMS::Stdio file handle is overwritten, C<undef>d, or goes
out of scope, the associated file is closed automatically.

=item vmssysopen

This function bears the same relationship to the CORE function
C<sysopen> as C<vmsopen> does to C<open>.  Its first three arguments
are the name, access flags, and permissions for the file.  Like
C<vmsopen>, it takes up to 8 additional string arguments which
specify file characteristics.  Its return value is identical to
that of C<vmsopen>.

The symbolic constants for the mode argument are exported by
VMS::Stdio by default, and are also exported by the Fcntl package.

=item waitfh

This function causes Perl to wait for the completion of an I/O
operation on the file handle specified as its argument.  It is
used with handles opened for asynchronous I/O, and performs its
task by calling the CRTL routine fwait().

=head1 REVISION

This document was last revised on 28-Jan-1996, for Perl 5.002.

=cut
