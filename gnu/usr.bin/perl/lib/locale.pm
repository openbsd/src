package locale;

our $VERSION = '1.03';
use Config;

$Carp::Internal{ (__PACKAGE__) } = 1;

=head1 NAME

locale - Perl pragma to use or avoid POSIX locales for built-in operations

=head1 SYNOPSIS

    @x = sort @y;      # Native-platform/Unicode code point sort order
    {
        use locale;
        @x = sort @y;  # Locale-defined sort order
    }
    @x = sort @y;      # Native-platform/Unicode code point sort order
                       # again

=head1 DESCRIPTION

This pragma tells the compiler to enable (or disable) the use of POSIX
locales for built-in operations (for example, LC_CTYPE for regular
expressions, LC_COLLATE for string comparison, and LC_NUMERIC for number
formatting).  Each "use locale" or "no locale"
affects statements to the end of the enclosing BLOCK.

See L<perllocale> for more detailed information on how Perl supports
locales.

On systems that don't have locales, this pragma will cause your operations
to behave as if in the "C" locale; attempts to change the locale will fail.

=cut

# A separate bit is used for each of the two forms of the pragma, as they are
# mostly independent, and interact with each other and the unicode_strings
# feature.  This allows for fast determination of which one(s) of the three
# are to be used at any given point, and no code has to be written to deal
# with coming in and out of scopes--it falls automatically out from the hint
# handling

$locale::hint_bits = 0x4;
$locale::not_chars_hint_bits = 0x10;

sub import {
    shift;  # should be 'locale'; not checked

    my $found_not_chars = 0;
    while (defined (my $arg = shift)) {
        if ($arg eq ":not_characters") {
            $^H |= $locale::not_chars_hint_bits;

            # This form of the pragma overrides the other
            $^H &= ~$locale::hint_bits;
            $found_not_chars = 1;
        }
        else {
            require Carp;
            Carp::croak("Unknown parameter '$arg' to 'use locale'");
        }
    }

    # Use the plain form if not doing the :not_characters one.
    $^H |= $locale::hint_bits unless $found_not_chars;
}

sub unimport {
    $^H &= ~($locale::hint_bits|$locale::not_chars_hint_bits);
}

1;
