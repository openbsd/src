#
# $Id: Base64.pm,v 2.16 2001/02/24 06:28:10 gisle Exp $

package MIME::Base64;

=head1 NAME

MIME::Base64 - Encoding and decoding of base64 strings

=head1 SYNOPSIS

 use MIME::Base64;

 $encoded = encode_base64('Aladdin:open sesame');
 $decoded = decode_base64($encoded);

=head1 DESCRIPTION

This module provides functions to encode and decode strings into the
Base64 encoding specified in RFC 2045 - I<MIME (Multipurpose Internet
Mail Extensions)>. The Base64 encoding is designed to represent
arbitrary sequences of octets in a form that need not be humanly
readable. A 65-character subset ([A-Za-z0-9+/=]) of US-ASCII is used,
enabling 6 bits to be represented per printable character.

The following functions are provided:

=over 4

=item encode_base64($str, [$eol])

Encode data by calling the encode_base64() function.  The first
argument is the string to encode.  The second argument is the line
ending sequence to use (it is optional and defaults to C<"\n">).  The
returned encoded string is broken into lines of no more than 76
characters each and it will end with $eol unless it is empty.  Pass an
empty string as second argument if you do not want the encoded string
broken into lines.

=item decode_base64($str)

Decode a base64 string by calling the decode_base64() function.  This
function takes a single argument which is the string to decode and
returns the decoded data.

Any character not part of the 65-character base64 subset set is
silently ignored.  Characters occuring after a '=' padding character
are never decoded.

If the length of the string to decode (after ignoring
non-base64 chars) is not a multiple of 4 or padding occurs too early,
then a warning is generated if perl is running under C<-w>.

=back

If you prefer not to import these routines into your namespace you can
call them as:

    use MIME::Base64 ();
    $encoded = MIME::Base64::encode($decoded);
    $decoded = MIME::Base64::decode($encoded);

=head1 DIAGNOSTICS

The following warnings might be generated if perl is invoked with the
C<-w> switch:

=over 4

=item Premature end of base64 data

The number of characters to decode is not a multiple of 4.  Legal
base64 data should be padded with one or two "=" characters to make
its length a multiple of 4.  The decoded result will anyway be as if
the padding was there.

=item Premature padding of base64 data

The '=' padding character occurs as the first or second character
in a base64 quartet.

=back

=head1 EXAMPLES

If you want to encode a large file, you should encode it in chunks
that are a multiple of 57 bytes.  This ensures that the base64 lines
line up and that you do not end up with padding in the middle. 57
bytes of data fills one complete base64 line (76 == 57*4/3):

   use MIME::Base64 qw(encode_base64);

   open(FILE, "/var/log/wtmp") or die "$!";
   while (read(FILE, $buf, 60*57)) {
       print encode_base64($buf);
   }

or if you know you have enough memory

   use MIME::Base64 qw(encode_base64);
   local($/) = undef;  # slurp
   print encode_base64(<STDIN>);

The same approach as a command line:

   perl -MMIME::Base64 -0777 -ne 'print encode_base64($_)' <file

Decoding does not need slurp mode if all the lines contains a multiple
of 4 base64 chars:

   perl -MMIME::Base64 -ne 'print decode_base64($_)' <file

=head1 COPYRIGHT

Copyright 1995-1999, 2001 Gisle Aas.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

Distantly based on LWP::Base64 written by Martijn Koster
<m.koster@nexor.co.uk> and Joerg Reichelt <j.reichelt@nexor.co.uk> and
code posted to comp.lang.perl <3pd2lp$6gf@wsinti07.win.tue.nl> by Hans
Mulder <hansm@wsinti07.win.tue.nl>

The XS implementation use code from metamail.  Copyright 1991 Bell
Communications Research, Inc. (Bellcore)

=cut

use strict;
use vars qw(@ISA @EXPORT $VERSION $OLD_CODE);

require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
@EXPORT = qw(encode_base64 decode_base64);

$VERSION = '2.12';

eval { bootstrap MIME::Base64 $VERSION; };
if ($@) {
    # can't bootstrap XS implementation, use perl implementation
    *encode_base64 = \&old_encode_base64;
    *decode_base64 = \&old_decode_base64;

    $OLD_CODE = $@;
    #warn $@ if $^W;
}

# Historically this module has been implemented as pure perl code.
# The XS implementation runs about 20 times faster, but the Perl
# code might be more portable, so it is still here.

use integer;

sub old_encode_base64 ($;$)
{
    my $res = "";
    my $eol = $_[1];
    $eol = "\n" unless defined $eol;
    pos($_[0]) = 0;                          # ensure start at the beginning

    $res = join '', map( pack('u',$_)=~ /^.(\S*)/, ($_[0]=~/(.{1,45})/gs));

    $res =~ tr|` -_|AA-Za-z0-9+/|;               # `# help emacs
    # fix padding at the end
    my $padding = (3 - length($_[0]) % 3) % 3;
    $res =~ s/.{$padding}$/'=' x $padding/e if $padding;
    # break encoded string into lines of no more than 76 characters each
    if (length $eol) {
	$res =~ s/(.{1,76})/$1$eol/g;
    }
    return $res;
}


sub old_decode_base64 ($)
{
    local($^W) = 0; # unpack("u",...) gives bogus warning in 5.00[123]

    my $str = shift;
    $str =~ tr|A-Za-z0-9+=/||cd;            # remove non-base64 chars
    if (length($str) % 4) {
	require Carp;
	Carp::carp("Length of base64 data not a multiple of 4")
    }
    $str =~ s/=+$//;                        # remove padding
    $str =~ tr|A-Za-z0-9+/| -_|;            # convert to uuencoded format

    return join'', map( unpack("u", chr(32 + length($_)*3/4) . $_),
	                $str =~ /(.{1,60})/gs);
}

# Set up aliases so that these functions also can be called as
#
#    MIME::Base64::encode();
#    MIME::Base64::decode();

*encode = \&encode_base64;
*decode = \&decode_base64;

1;
