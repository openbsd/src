package PerlIO;

our $VERSION = '1.01';

# Map layer name to package that defines it
our %alias;

sub import
{
 my $class = shift;
 while (@_)
  {
   my $layer = shift;
   if (exists $alias{$layer})
    {
     $layer = $alias{$layer}
    }
   else
    {
     $layer = "${class}::$layer";
    }
   eval "require $layer";
   warn $@ if $@;
  }
}

1;
__END__

=head1 NAME

PerlIO - On demand loader for PerlIO layers and root of PerlIO::* name space

=head1 SYNOPSIS

  open($fh,"<:crlf", "my.txt"); # portably open a text file for reading

  open($fh,"<","his.jpg");      # portably open a binary file for reading
  binmode($fh);

  Shell:
    PERLIO=perlio perl ....

=head1 DESCRIPTION

When an undefined layer 'foo' is encountered in an C<open> or
C<binmode> layer specification then C code performs the equivalent of:

  use PerlIO 'foo';

The perl code in PerlIO.pm then attempts to locate a layer by doing

  require PerlIO::foo;

Otherwise the C<PerlIO> package is a place holder for additional
PerlIO related functions.

The following layers are currently defined:

=over 4

=item unix

Low level layer which calls C<read>, C<write> and C<lseek> etc.

=item stdio

Layer which calls C<fread>, C<fwrite> and C<fseek>/C<ftell> etc.  Note
that as this is "real" stdio it will ignore any layers beneath it and
got straight to the operating system via the C library as usual.

=item perlio

This is a re-implementation of "stdio-like" buffering written as a
PerlIO "layer".  As such it will call whatever layer is below it for
its operations.

=item crlf

A layer which does CRLF to "\n" translation distinguishing "text" and
"binary" files in the manner of MS-DOS and similar operating systems.
(It currently does I<not> mimic MS-DOS as far as treating of Control-Z
as being an end-of-file marker.)

=item utf8

Declares that the stream accepts perl's internal encoding of
characters.  (Which really is UTF-8 on ASCII machines, but is
UTF-EBCDIC on EBCDIC machines.)  This allows any character perl can
represent to be read from or written to the stream. The UTF-X encoding
is chosen to render simple text parts (i.e.  non-accented letters,
digits and common punctuation) human readable in the encoded file.

Here is how to write your native data out using UTF-8 (or UTF-EBCDIC)
and then read it back in.

	open(F, ">:utf8", "data.utf");
	print F $out;
	close(F);

	open(F, "<:utf8", "data.utf");
	$in = <F>;
	close(F);

=item bytes

This is the inverse of C<:utf8> layer. It turns off the flag
on the layer below so that data read from it is considered to
be "octets" i.e. characters in range 0..255 only. Likewise
on output perl will warn if a "wide" character is written
to a such a stream.

=item raw

The C<:raw> layer is I<defined> as being identical to calling
C<binmode($fh)> - the stream is made suitable for passing binary
data i.e. each byte is passed as-is. The stream will still be
buffered. Unlike earlier versions of perl C<:raw> is I<not> just the
inverse of C<:crlf> - other layers which would affect the binary nature of
the stream are also removed or disabled.

The implementation of C<:raw> is as a pseudo-layer which when "pushed"
pops itself and then any layers which do not declare themselves as suitable
for binary data. (Undoing :utf8 and :crlf are implemented by clearing
flags rather than poping layers but that is an implementation detail.)

As a consequence of the fact that C<:raw> normally pops layers
it usually only makes sense to have it as the only or first element in a
layer specification.  When used as the first element it provides
a known base on which to build e.g.

    open($fh,":raw:utf8",...)

will construct a "binary" stream, but then enable UTF-8 translation.

=item pop

A pseudo layer that removes the top-most layer. Gives perl code
a way to manipulate the layer stack. Should be considered
as experimental. Note that C<:pop> only works on real layers
and will not undo the effects of pseudo layers like C<:utf8>.
An example of a possible use might be:

    open($fh,...)
    ...
    binmode($fh,":encoding(...)");  # next chunk is encoded
    ...
    binmode($fh,":pop");            # back to un-encocded

A more elegant (and safer) interface is needed.

=back

=head2 Alternatives to raw

To get a binary stream an alternate method is to use:

    open($fh,"whatever")
    binmode($fh);

this has advantage of being backward compatible with how such things have
had to be coded on some platforms for years.

To get an un-buffered stream specify an unbuffered layer (e.g. C<:unix>)
in the open call:

    open($fh,"<:unix",$path)

=head2 Defaults and how to override them

If the platform is MS-DOS like and normally does CRLF to "\n"
translation for text files then the default layers are :

  unix crlf

(The low level "unix" layer may be replaced by a platform specific low
level layer.)

Otherwise if C<Configure> found out how to do "fast" IO using system's
stdio, then the default layers are :

  unix stdio

Otherwise the default layers are

  unix perlio

These defaults may change once perlio has been better tested and tuned.

The default can be overridden by setting the environment variable
PERLIO to a space separated list of layers (unix or platform low level
layer is always pushed first).

This can be used to see the effect of/bugs in the various layers e.g.

  cd .../perl/t
  PERLIO=stdio  ./perl harness
  PERLIO=perlio ./perl harness

=head1 AUTHOR

Nick Ing-Simmons E<lt>nick@ing-simmons.netE<gt>

=head1 SEE ALSO

L<perlfunc/"binmode">, L<perlfunc/"open">, L<perlunicode>, L<Encode>

=cut

