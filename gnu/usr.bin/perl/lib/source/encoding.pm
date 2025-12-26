package source::encoding;

use v5.40;

our $VERSION = '0.01';

our $ascii_hint_bits = 0x00000010;

sub import {
    unimport();     # Get rid of any 'use utf8'
    my (undef, $arg) = @_;
    if ($arg eq 'utf8') {
        require utf8;
        utf8->import;
        return;
    }
    elsif ($arg eq 'ascii') {
        $^H |= $ascii_hint_bits;
        return;
    }

    die "Bad argument for source::encoding: '$arg'";
}

sub unimport {
    $^H &= ~$ascii_hint_bits;
    utf8->unimport;
}

1;
__END__

=head1 NAME

source::encoding -- Declare Perl source code encoding

=head1 SYNOPSIS

 use source::encoding 'ascii';
 use source::encoding 'utf8';
 no source::encoding;

=head1 DESCRIPTION

These days, Perl code either generally contains only ASCII characters with
C<\x{}> and similar escapes to represent non-ASCII, or C<S<use utf8>> is used
to indicate that the source code itself contains characters encoded as UTF-8.

That means that a character in the source code not meeting these criteria is
often a typographical error.  This pragma is used to tell Perl to raise an
error when this happens.

S<C<use source::encoding 'utf8'>> is a synonym for S<C<use utf8>>.  They may
be used interchangeably.

S<C<use source::encoding 'ascii'>> turns off any UTF-8 expectations, and
raises a fatal error if any character within its scope in the input source
code is not ASCII (or ASCII-equivalent on EBCDIC systems).

S<C<no source::encoding>> turns off any UTF-8/ASCII expectations for the
remainder of its scope.  The meaning of non-ASCII characters is then
undefined.

S<C<use source::encoding 'ascii'>> is automatically enabled within the
lexical scope of a S<C<use v5.41.0>> or higher.

=head1 SEE ALSO

L<utf8>

=cut
