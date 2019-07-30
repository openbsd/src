# Tools to aid testing across platforms with different character sets.

$::IS_ASCII  = ord 'A' ==  65;
$::IS_EBCDIC = ord 'A' == 193;

# The following functions allow tests to work on both EBCDIC and ASCII-ish
# platforms.  They convert string scalars between the native character set and
# the set of 256 characters which is usually called Latin1.  However, they
# will work properly with any character input, not just Latin1.

sub native_to_uni($) {
    my $string = shift;

    return $string if $::IS_ASCII;
    my $output = "";
    for my $i (0 .. length($string) - 1) {
        $output .= chr(utf8::native_to_unicode(ord(substr($string, $i, 1))));
    }
    # Preserve utf8ness of input onto the output, even if it didn't need to be
    # utf8
    utf8::upgrade($output) if utf8::is_utf8($string);

    return $output;
}

sub uni_to_native($) {
    my $string = shift;

    return $string if $::IS_ASCII;
    my $output = "";
    for my $i (0 .. length($string) - 1) {
        $output .= chr(utf8::unicode_to_native(ord(substr($string, $i, 1))));
    }
    # Preserve utf8ness of input onto the output, even if it didn't need to be
    # utf8
    utf8::upgrade($output) if utf8::is_utf8($string);

    return $output;
}

sub byte_utf8a_to_utf8n {
    # Convert a UTF-8 byte sequence into the platform's native UTF-8
    # equivalent, currently only UTF-8 and UTF-EBCDIC.

    my @utf8_skip = (
    # This translates a utf-8-encoded byte into how many bytes the full utf8
    # character occupies.

      # 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  # 0
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  # 1
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  # 2
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  # 3
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  # 4
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  # 5
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  # 6
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  # 7
       -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  # 8
       -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  # 9
       -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  # A
       -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  # B
       -1,-1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  # C
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  # D
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  # E
        4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7,13,  # F
    );

    my $string = shift;
    die "Input to byte_utf8a-to_utf8n() must not be flagged UTF-8"
                                                    if utf8::is_utf8($string);
    return $string if $::IS_ASCII;
    die "Expecting ASCII or EBCDIC" unless $::IS_EBCDIC;

    my $length = length($string);
    #diag($string);
    #diag($length);
    my $out = "";
    for ($i = 0; $i < $length; $i++) {
        my $byte = ord substr($string, $i, 1);
        my $byte_count = $utf8_skip[$byte];
        #diag($byte);
        #diag($byte_count);

        die "Illegal start byte" if $byte_count < 0;
        if ($i + $byte_count > $length) {
            die "Attempt to read " . $i + $byte_count - $length . " beyond end-of-string";
        }

        # Just translate UTF-8 invariants directly.
        if ($byte_count == 1) {
            $out .= chr utf8::unicode_to_native($byte);
            next;
        }

        # Otherwise calculate the code point ordinal represented by the
        # sequence beginning with this byte, using the algorithm adapted from
        # utf8.c.  We absorb each byte in the sequence as we go along
        my $ord = $byte & (0x1F >> ($byte_count - 2));
        my $bytes_remaining = $byte_count - 1;
        while ($bytes_remaining > 0) {
            $byte = ord substr($string, ++$i, 1);
            unless (($byte & 0xC0) == 0x80) {
                die sprintf "byte '%X' is not a valid continuation", $byte;
            }
            $ord = $ord << 6 | ($byte & 0x3f);
            $bytes_remaining--;
        }
        #diag($byte);
        #diag($ord);

        my $expected_bytes = $ord < 0x80
                             ? 1
                             : $ord < 0x800
                               ? 2
                               : $ord < 0x10000
                                 ? 3
                                 : $ord < 0x200000
                                   ? 4
                                   : $ord < 0x4000000
                                     ? 5
                                     : $ord < 0x80000000
                                       ? 6
                                       : 7;
                                       #: (uv) < UTF8_QUAD_MAX ? 7 : 13 )

        # Make sure is not an overlong sequence
        if ($byte_count != $expected_bytes) {
            die sprintf "character U+%X should occupy %d bytes, not %d",
                                            $ord, $expected_bytes, $byte_count;
        }

        # Now that we have found the code point the original UTF-8 meant, we
        # use the native chr function to get its native string equivalent.
        $out .= chr utf8::unicode_to_native($ord);
    }

    utf8::encode($out); # Turn off utf8 flag.
    #diag($out);
    return $out;
}

1
