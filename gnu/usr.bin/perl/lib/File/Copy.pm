# File/Copy.pm. Written in 1994 by Aaron Sherman <ajs@ajs.com>. This
# source code has been placed in the public domain by the author.
# Please be kind and preserve the documentation.
#

package File::Copy;

require Exporter;
use Carp;

@ISA=qw(Exporter);
@EXPORT=qw(copy);
@EXPORT_OK=qw(copy cp);

$File::Copy::VERSION = '1.5';
$File::Copy::Too_Big = 1024 * 1024 * 2;

sub VERSION {
    # Version of File::Copy
    return $File::Copy::VERSION;
}

sub copy {
    croak("Usage: copy( file1, file2 [, buffersize]) ")
      unless(@_ == 2 || @_ == 3);

    if (($^O eq 'VMS' or $^O eq 'os2') && ref(\$to) ne 'GLOB' &&
        !(defined ref $to and (ref($to) eq 'GLOB' ||
          ref($to) eq 'FileHandle' || ref($to) eq 'VMS::Stdio')))
        { return File::Copy::syscopy($_[0],$_[1]) }

    my $from = shift;
    my $to = shift;
    my $recsep = $\;
    my $closefrom=0;
    my $closeto=0;
    my ($size, $status, $r, $buf);
    local(*FROM, *TO);

    $\ = '';

    if (ref(\$from) eq 'GLOB') {
	*FROM = $from;
    } elsif (defined ref $from and
	     (ref($from) eq 'GLOB' || ref($from) eq 'FileHandle' ||
	      ref($from) eq 'VMS::Stdio')) {
	*FROM = *$from;
    } else {
	open(FROM,"<$from")||goto(fail_open1);
	binmode FROM;
	$closefrom = 1;
    }

    if (ref(\$to) eq 'GLOB') {
	*TO = $to;
    } elsif (defined ref $to and
	     (ref($to) eq 'GLOB' || ref($to) eq 'FileHandle' ||
	      ref($to) eq 'VMS::Stdio')) {
	*TO = *$to;
    } else {
	open(TO,">$to")||goto(fail_open2);
	binmode TO;
	$closeto=1;
    }

    if (@_) {
	$size = shift(@_) + 0;
	croak("Bad buffer size for copy: $size\n") unless ($size > 0);
    } else {
	$size = -s FROM;
	$size = 1024 if ($size < 512);
	$size = $File::Copy::Too_Big if ($size > $File::Copy::Too_Big);
    }

    $buf = '';
    while(defined($r = read(FROM,$buf,$size)) && $r > 0) {
	if (syswrite (TO,$buf,$r) != $r) {
	    goto fail_inner;    
	}
    }
    goto fail_inner unless(defined($r));
    close(TO) || goto fail_open2 if $closeto;
    close(FROM) || goto fail_open1 if $closefrom;
    $\ = $recsep;
    return 1;
    
    # All of these contortions try to preserve error messages...
  fail_inner:
    if ($closeto) {
	$status = $!;
	$! = 0;
	close TO;
	$! = $status unless $!;
    }
  fail_open2:
    if ($closefrom) {
	$status = $!;
	$! = 0;
	close FROM;
	$! = $status unless $!;
    }
  fail_open1:
    $\ = $recsep;
    return 0;
}


*cp = \&copy;
# &syscopy is an XSUB under OS/2
*syscopy = ($^O eq 'VMS' ? \&rmscopy : \&copy) unless $^O eq 'os2';

1;

__END__

=head1 NAME

File::Copy - Copy files or filehandles

=head1 SYNOPSIS

  	use File::Copy;

	copy("file1","file2");
  	copy("Copy.pm",\*STDOUT);'

  	use POSIX;
	use File::Copy cp;

	$n=FileHandle->new("/dev/null","r");
	cp($n,"x");'

=head1 DESCRIPTION

The File::Copy module provides a basic function C<copy> which takes two
parameters: a file to copy from and a file to copy to. Either
argument may be a string, a FileHandle reference or a FileHandle
glob. Obviously, if the first argument is a filehandle of some
sort, it will be read from, and if it is a file I<name> it will
be opened for reading. Likewise, the second argument will be
written to (and created if need be).  Note that passing in
files as handles instead of names may lead to loss of information
on some operating systems; it is recommended that you use file
names whenever possible.

An optional third parameter can be used to specify the buffer
size used for copying. This is the number of bytes from the
first file, that wil be held in memory at any given time, before
being written to the second file. The default buffer size depends
upon the file, but will generally be the whole file (up to 2Mb), or
1k for filehandles that do not reference files (eg. sockets).

You may use the syntax C<use File::Copy "cp"> to get at the
"cp" alias for this function. The syntax is I<exactly> the same.

File::Copy also provides the C<syscopy> routine, which copies the
file specified in the first parameter to the file specified in the
second parameter, preserving OS-specific attributes and file
structure.  For Unix systems, this is equivalent to the simple
C<copy> routine.  For VMS systems, this calls the C<rmscopy>
routine (see below).  For OS/2 systems, this calls the C<syscopy>
XSUB directly.

=head2 Special behavior under VMS

If the second argument to C<copy> is not a file handle for an
already opened file, then C<copy> will perform an RMS copy of
the input file to a new output file, in order to preserve file
attributes, indexed file structure, I<etc.>  The buffer size
parameter is ignored.  If the second argument to C<copy> is a
Perl handle to an opened file, then data is copied using Perl
operators, and no effort is made to preserve file attributes
or record structure.

The RMS copy routine may also be called directly under VMS
as C<File::Copy::rmscopy> (or C<File::Copy::syscopy>, which
is just an alias for this routine).

=item rmscopy($from,$to[,$date_flag])

The first and second arguments may be strings, typeglobs, or
typeglob references; they are used in all cases to obtain the
I<filespec> of the input and output files, respectively.  The
name and type of the input file are used as defaults for the
output file, if necessary.

A new version of the output file is always created, which
inherits the structure and RMS attributes of the input file,
except for owner and protections (and possibly timestamps;
see below).  All data from the input file is copied to the
output file; if either of the first two parameters to C<rmscopy>
is a file handle, its position is unchanged.  (Note that this
means a file handle pointing to the output file will be
associated with an old version of that file after C<rmscopy>
returns, not the newly created version.)

The third parameter is an integer flag, which tells C<rmscopy>
how to handle timestamps.  If it is < 0, none of the input file's
timestamps are propagated to the output file.  If it is > 0, then
it is interpreted as a bitmask: if bit 0 (the LSB) is set, then
timestamps other than the revision date are propagated; if bit 1
is set, the revision date is propagated.  If the third parameter
to C<rmscopy> is 0, then it behaves much like the DCL COPY command:
if the name or type of the output file was explicitly specified,
then no timestamps are propagated, but if they were taken implicitly
from the input filespec, then all timestamps other than the
revision date are propagated.  If this parameter is not supplied,
it defaults to 0.

Like C<copy>, C<rmscopy> returns 1 on success.  If an error occurs,
it sets C<$!>, deletes the output file, and returns 0.

=head1 RETURN

Returns 1 on success, 0 on failure. $! will be set if an error was
encountered.

=head1 AUTHOR

File::Copy was written by Aaron Sherman I<E<lt>ajs@ajs.comE<gt>> in 1995.
The VMS-specific code was added by Charles Bailey
I<E<lt>bailey@genetics.upenn.eduE<gt>> in March 1996.

=cut
