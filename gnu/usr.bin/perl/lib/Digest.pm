package Digest;

use strict;
use vars qw($VERSION %MMAP $AUTOLOAD);

$VERSION = "1.00";

%MMAP = (
  "SHA-1"      => "Digest::SHA1",
  "HMAC-MD5"   => "Digest::HMAC_MD5",
  "HMAC-SHA-1" => "Digest::HMAC_SHA1",
);

sub new
{
    shift;  # class ignored
    my $algorithm = shift;
    my $class = $MMAP{$algorithm} || "Digest::$algorithm";
    no strict 'refs';
    unless (exists ${"$class\::"}{"VERSION"}) {
	eval "require $class";
	die $@ if $@;
    }
    $class->new(@_);
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

Digest:: - Modules that calculate message digests

=head1 SYNOPSIS

  $md2 = Digest->MD2;
  $md5 = Digest->MD5;

  $sha1 = Digest->SHA1;
  $sha1 = Digest->new("SHA-1");

  $hmac = Digest->HMAC_MD5($key);

=head1 DESCRIPTION

The C<Digest::> modules calculate digests, also called "fingerprints"
or "hashes", of some data, called a message.  The digest is (usually)
some small/fixed size string.  The actual size of the digest depend of
the algorithm used.  The message is simply a sequence of arbitrary
bytes.

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

A twice as long string of (lowercase) hexadecimal digits.

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

=item $ctx->reset

This is just an alias for $ctx->new.

=item $ctx->add($data,...)

The $data provided as argument are appended to the message we
calculate the digest for.  The return value is the $ctx object itself.

=item $ctx->addfile($io_handle)

The $io_handle is read until EOF and the content is appended to the
message we calculate the digest for.  The return value is the $ctx
object itself.

=item $ctx->digest

Return the binary digest for the message.

Note that the C<digest> operation is effectively a destructive,
read-once operation. Once it has been performed, the $ctx object is
automatically C<reset> and can be used to calculate another digest
value.

=item $ctx->hexdigest

Same as $ctx->digest, but will return the digest in hexadecimal form.

=item $ctx->b64digest

Same as $ctx->digest, but will return the digest as a base64 encoded
string.

=back

=head1 SEE ALSO

L<Digest::MD5>, L<Digest::SHA1>, L<Digest::HMAC>, L<Digest::MD2>

L<MIME::Base64>

=head1 AUTHOR

Gisle Aas <gisle@aas.no>

The C<Digest::> interface is based on the interface originally
developed by Neil Winton for his C<MD5> module.

=cut
