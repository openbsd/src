package Digest;

use strict;
use vars qw($VERSION %MMAP $AUTOLOAD);

$VERSION = "1.05";

%MMAP = (
  "SHA-1"      => ["Digest::SHA1", ["Digest::SHA", 1], ["Digest::SHA2", 1]],
  "SHA-256"    => [["Digest::SHA", 256], ["Digest::SHA2", 256]],
  "SHA-384"    => [["Digest::SHA", 384], ["Digest::SHA2", 384]],
  "SHA-512"    => [["Digest::SHA", 512], ["Digest::SHA2", 512]],
  "HMAC-MD5"   => "Digest::HMAC_MD5",
  "HMAC-SHA-1" => "Digest::HMAC_SHA1",
);

sub new
{
    shift;  # class ignored
    my $algorithm = shift;
    my $impl = $MMAP{$algorithm} || do {
	$algorithm =~ s/\W+//;
	"Digest::$algorithm";
    };
    $impl = [$impl] unless ref($impl);
    my $err;
    for  (@$impl) {
	my $class = $_;
	my @args;
	($class, @args) = @$class if ref($class);
	no strict 'refs';
	unless (exists ${"$class\::"}{"VERSION"}) {
	    eval "require $class";
	    if ($@) {
		$err ||= $@;
		next;
	    }
	}
	return $class->new(@args, @_);
    }
    die $err;
}

sub AUTOLOAD
{
    my $class = shift;
    my $algorithm = substr($AUTOLOAD, rindex($AUTOLOAD, '::')+2);
    $class->new($algorithm, @_);
}

1;

__END__

=head1 NAME

Digest - Modules that calculate message digests

=head1 SYNOPSIS

  $md5  = Digest->new("MD5");
  $sha1 = Digest->new("SHA-1");
  $sha256 = Digest->new("SHA-256");
  $sha384 = Digest->new("SHA-384");
  $sha512 = Digest->new("SHA-512");

  $hmac = Digest->HMAC_MD5($key);

=head1 DESCRIPTION

The C<Digest::> modules calculate digests, also called "fingerprints"
or "hashes", of some data, called a message.  The digest is (usually)
some small/fixed size string.  The actual size of the digest depend of
the algorithm used.  The message is simply a sequence of arbitrary
bytes or bits.

An important property of the digest algorithms is that the digest is
I<likely> to change if the message change in some way.  Another
property is that digest functions are one-way functions, i.e. it
should be I<hard> to find a message that correspond to some given
digest.  Algorithms differ in how "likely" and how "hard", as well as
how efficient they are to compute.

All C<Digest::> modules provide the same programming interface.  A
functional interface for simple use, as well as an object oriented
interface that can handle messages of arbitrary length and which can
read files directly.

The digest can be delivered in three formats:

=over 8

=item I<binary>

This is the most compact form, but it is not well suited for printing
or embedding in places that can't handle arbitrary data.

=item I<hex>

A twice as long string of lowercase hexadecimal digits.

=item I<base64>

A string of portable printable characters.  This is the base64 encoded
representation of the digest with any trailing padding removed.  The
string will be about 30% longer than the binary version.
L<MIME::Base64> tells you more about this encoding.

=back


The functional interface is simply importable functions with the same
name as the algorithm.  The functions take the message as argument and
return the digest.  Example:

  use Digest::MD5 qw(md5);
  $digest = md5($message);

There are also versions of the functions with "_hex" or "_base64"
appended to the name, which returns the digest in the indicated form.

=head1 OO INTERFACE

The following methods are available for all C<Digest::> modules:

=over 4

=item $ctx = Digest->XXX($arg,...)

=item $ctx = Digest->new(XXX => $arg,...)

=item $ctx = Digest::XXX->new($arg,...)

The constructor returns some object that encapsulate the state of the
message-digest algorithm.  You can add data to the object and finally
ask for the digest.  The "XXX" should of course be replaced by the proper
name of the digest algorithm you want to use.

The two first forms are simply syntactic sugar which automatically
load the right module on first use.  The second form allow you to use
algorithm names which contains letters which are not legal perl
identifiers, e.g. "SHA-1".

If new() is called as an instance method (i.e. $ctx->new) it will just
reset the state the object to the state of a newly created object.  No
new object is created in this case, and the return value is the
reference to the object (i.e. $ctx).

=item $other_ctx = $ctx->clone

The clone method creates a copy of the digest state object and returns
a reference to the copy.

=item $ctx->reset

This is just an alias for $ctx->new.

=item $ctx->add( $data, ... )

The $data provided as argument are appended to the message we
calculate the digest for.  The return value is the $ctx object itself.

=item $ctx->addfile( $io_handle )

The $io_handle is read until EOF and the content is appended to the
message we calculate the digest for.  The return value is the $ctx
object itself.

=item $ctx->add_bits( $data, $nbits )

=item $ctx->add_bits( $bitstring )

The bits provided are appended to the message we calculate the digest
for.  The return value is the $ctx object itself.

The two argument form of add_bits() will add the first $nbits bits
from data.  For the last potentially partial byte only the high order
C<< $nbits % 8 >> bits are used.  If $nbits is greater than C<<
length($data) * 8 >>, then this method would do the same as C<<
$ctx->add($data) >>, i.e. $nbits is silently ignored.

The one argument form of add_bits() takes a $bitstring of "1" and "0"
chars as argument.  It's a shorthand for C<< $ctx->add_bits(pack("B*",
$bitstring), length($bitstring)) >>.

This example shows two calls that should have the same effect:

   $ctx->add_bits("111100001010");
   $ctx->add_bits("\xF0\xA0", 12);

Most digest algorithms are byte based.  For those it is not possible
to add bits that are not a multiple of 8, and the add_bits() method
will croak if you try.

=item $ctx->digest

Return the binary digest for the message.

Note that the C<digest> operation is effectively a destructive,
read-once operation. Once it has been performed, the $ctx object is
automatically C<reset> and can be used to calculate another digest
value.  Call $ctx->clone->digest if you want to calculate the digest
without reseting the digest state.

=item $ctx->hexdigest

Same as $ctx->digest, but will return the digest in hexadecimal form.

=item $ctx->b64digest

Same as $ctx->digest, but will return the digest as a base64 encoded
string.

=back

=head1 Digest speed

This table should give some indication on the relative speed of
different algorithms.  It is sorted by throughput based on a benchmark
done with of some implementations of this API:

 Algorithm	Size	Implementation			MB/s

 MD4		128	Digest::MD4 v1.1		24.9
 MD5		128	Digest::MD5 v2.30		18.7
 Haval-256	256	Digest::Haval256 v1.0.4		17.0
 SHA-1		160	Digest::SHA1 v2.06		15.3
 SHA-1		160	Digest::SHA v4.0.0		10.1
 SHA-256	256	Digest::SHA2 v1.0.0	 	 7.6
 SHA-256	256	Digest::SHA v4.0.0	 	 6.5
 SHA-384	384	Digest::SHA2 v1.0.0	 	 2.7
 SHA-384	384	Digest::SHA v4.0.0	 	 2.7
 SHA-512	512	Digest::SHA2 v1.0.0		 2.7
 SHA-512	512	Digest::SHA v4.0.0		 2.7
 Whirlpool	512	Digest::Whirlpool v1.0.2	 1.4
 MD2		128	Digest::MD2 v2.03		 1.1

 Adler-32	 32	Digest::Adler32 v0.03	 	 0.2
 MD5		128	Digest::Perl::MD5 v1.5		 0.1

These numbers was achieved Nov 2003 with ActivePerl-5.8.1 running
under Linux on a P-II 350 MHz CPU.  The last 2 entries differ by being
pure perl implementations of the algorithms, which explains why they
are so slow.

=head1 SEE ALSO

L<Digest::Adler32>, L<Digest::Haval256>, L<Digest::HMAC>, L<Digest::MD2>, L<Digest::MD4>, L<Digest::MD5>, L<Digest::SHA>, L<Digest::SHA1>, L<Digest::SHA2>, L<Digest::Whirlpool>

New digest implementations should consider subclassing from L<Digest::base>.

L<MIME::Base64>

=head1 AUTHOR

Gisle Aas <gisle@aas.no>

The C<Digest::> interface is based on the interface originally
developed by Neil Winton for his C<MD5> module.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

    Copyright 1998-2001,2003 Gisle Aas.
    Copyright 1995-1996 Neil Winton.

=cut
