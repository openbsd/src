package utf8;

$utf8::hint_bits = 0x00800000;

our $VERSION = '1.00';

sub import {
    $^H |= $utf8::hint_bits;
    $enc{caller()} = $_[1] if $_[1];
}

sub unimport {
    $^H &= ~$utf8::hint_bits;
}

sub AUTOLOAD {
    require "utf8_heavy.pl";
    goto &$AUTOLOAD if defined &$AUTOLOAD;
    Carp::croak("Undefined subroutine $AUTOLOAD called");
}

1;
__END__

=head1 NAME

utf8 - Perl pragma to enable/disable UTF-8 (or UTF-EBCDIC) in source code

=head1 SYNOPSIS

    use utf8;
    no utf8;

=head1 DESCRIPTION

The C<use utf8> pragma tells the Perl parser to allow UTF-8 in the
program text in the current lexical scope (allow UTF-EBCDIC on EBCDIC based
platforms).  The C<no utf8> pragma tells Perl to switch back to treating
the source text as literal bytes in the current lexical scope.

This pragma is primarily a compatibility device.  Perl versions
earlier than 5.6 allowed arbitrary bytes in source code, whereas
in future we would like to standardize on the UTF-8 encoding for
source text.  Until UTF-8 becomes the default format for source
text, this pragma should be used to recognize UTF-8 in the source.
When UTF-8 becomes the standard source format, this pragma will
effectively become a no-op.  For convenience in what follows the
term I<UTF-X> is used to refer to UTF-8 on ASCII and ISO Latin based
platforms and UTF-EBCDIC on EBCDIC based platforms.

Enabling the C<utf8> pragma has the following effect:

=over 4

=item *

Bytes in the source text that have their high-bit set will be treated
as being part of a literal UTF-8 character.  This includes most
literals such as identifier names, string constants, and constant
regular expression patterns.

On EBCDIC platforms characters in the Latin 1 character set are
treated as being part of a literal UTF-EBCDIC character.

=back

Note that if you have bytes with the eighth bit on in your script
(for example embedded Latin-1 in your string literals), C<use utf8>
will be unhappy since the bytes are most probably not well-formed
UTF-8.  If you want to have such bytes and use utf8, you can disable
utf8 until the end the block (or file, if at top level) by C<no utf8;>.

=head2 Utility functions

The following functions are defined in the C<utf8::> package by the perl core.

=over 4

=item * $num_octets = utf8::upgrade($string);

Converts (in-place) internal representation of string to Perl's internal
I<UTF-X> form.  Returns the number of octets necessary to represent
the string as I<UTF-X>.  Can be used to make sure that the
UTF-8 flag is on, so that C<\w> or C<lc()> work as expected on strings
containing characters in the range 0x80-0xFF.  Note that this should
not be used to convert
a legacy byte encoding to Unicode: use Encode for that.  Affected
by the encoding pragma.

=item * utf8::downgrade($string[, FAIL_OK])

Converts (in-place) internal representation of string to be un-encoded
bytes.  Returns true on success. On failure dies or, if the value of
FAIL_OK is true, returns false.  Can be used to make sure that the
UTF-8 flag is off, e.g. when you want to make sure that the substr()
or length() function works with the usually faster byte algorithm.
Note that this should not be used to convert Unicode back to a legacy
byte encoding: use Encode for that.  B<Not> affected by the encoding
pragma.

=item * utf8::encode($string)

Converts (in-place) I<$string> from logical characters to octet
sequence representing it in Perl's I<UTF-X> encoding. Same as
Encode::encode_utf8(). Note that this should not be used to convert
a legacy byte encoding to Unicode: use Encode for that.

=item * $flag = utf8::decode($string)

Attempts to convert I<$string> in-place from Perl's I<UTF-X> encoding
into logical characters. Same as Encode::decode_utf8(). Note that this
should not be used to convert Unicode back to a legacy byte encoding:
use Encode for that.

=item * $flag = utf8::valid(STRING)

[INTERNAL] Test whether STRING is in a consistent state.  Will return
true if string is held as bytes, or is well-formed UTF-8 and has the
UTF-8 flag on.  Main reason for this routine is to allow Perl's
testsuite to check that operations have left strings in a consistent
state.

=back

C<utf8::encode> is like C<utf8::upgrade>, but the UTF8 flag is
cleared.  See L<perlunicode> for more on the UTF8 flag and the C API
functions C<sv_utf8_upgrade>, C<sv_utf8_downgrade>, C<sv_utf8_encode>,
and C<sv_utf8_decode>, which are wrapped by the Perl functions
C<utf8::upgrade>, C<utf8::downgrade>, C<utf8::encode> and
C<utf8::decode>.  Note that in the Perl 5.8.0 implementation the
functions utf8::valid, utf8::encode, utf8::decode, utf8::upgrade,
and utf8::downgrade are always available, without a C<require utf8>
statement-- this may change in future releases.

=head1 BUGS

One can have Unicode in identifier names, but not in package/class or
subroutine names.  While some limited functionality towards this does
exist as of Perl 5.8.0, that is more accidental than designed; use of
Unicode for the said purposes is unsupported.

One reason of this unfinishedness is its (currently) inherent
unportability: since both package names and subroutine names may need
to be mapped to file and directory names, the Unicode capability of
the filesystem becomes important-- and there unfortunately aren't
portable answers.

=head1 SEE ALSO

L<perlunicode>, L<bytes>

=cut
