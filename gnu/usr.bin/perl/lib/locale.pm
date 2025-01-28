package locale;

use strict;
use warnings;

our $VERSION = '1.12';
use Config;

$Carp::Internal{ (__PACKAGE__) } = 1;

=head1 NAME

locale - Perl pragma to use or avoid POSIX locales for built-in operations

=head1 SYNOPSIS

 my @x1 = sort @y;      # Native-platform/Unicode code point sort order
 {
     use locale;
     my @x2 = sort @y;  # Locale-defined sort order
 }
 my @x3 = sort @y;      # Native-platform/Unicode code point sort order
                        # again

 # Parameters to the pragma are to work around deficiencies in locale
 # handling that have since been fixed, and hence these are likely no
 # longer useful
 use locale qw(:ctype :collate);    # Only use the locale for character
                                    # classification (\w, \d, etc.), and
                                    # for string comparison operations
                                    # like '$a le $b' and sorting.
 use locale ':not_characters';      # Use the locale for everything but
                                    # character classification and string
                                    # comparison operations

 use locale ':!numeric';            # Use the locale for everything but
                                    # numeric-related operations
 use locale ':not_numeric';         # Same

 no locale;             # Turn off locale handling for the remainder of
                        # the scope.

=head1 DESCRIPTION

This pragma tells the compiler to enable (or disable) the use of POSIX
locales for built-in operations (for example, C<LC_CTYPE> for regular
expressions, C<LC_COLLATE> for string comparison, and C<LC_NUMERIC> for number
formatting).  Each C<use locale> or C<no locale>
affects statements to the end of the enclosing BLOCK.

The pragma is documented as part of L<perllocale>.

=cut

# A separate bit is used for each of the two forms of the pragma, to save
# having to look at %^H for the normal case of a plain 'use locale' without an
# argument.

$locale::hint_bits = 0x4;
$locale::partial_hint_bits = 0x10;  # If pragma has an argument

# The pseudo-category :characters consists of 2 real ones; but it also is
# given its own number, -1, because in the complement form it also has the
# side effect of "use feature 'unicode_strings'"

sub import {
    shift;  # should be 'locale'; not checked

    $^H{locale} = 0 unless defined $^H{locale};
    if (! @_) { # If no parameter, use the plain form that changes all categories
        $^H |= $locale::hint_bits;

    }
    else {
        my @categories = ( qw(:ctype :collate :messages
                              :numeric :monetary :time) );
        for (my $i = 0; $i < @_; $i++) {
            my $arg = $_[$i];
            my $complement = $arg =~ s/ : ( ! | not_ ) /:/x;
            if (! grep { $arg eq $_ } @categories, ":characters") {
                require Carp;
                Carp::croak("Unknown parameter '$_[$i]' to 'use locale'");
            }

            if ($complement) {
                if ($i != 0 || $i < @_ - 1)  {
                    require Carp;
                    Carp::croak("Only one argument to 'use locale' allowed"
                                . "if is $complement");
                }

                if ($arg eq ':characters') {
                    push @_, grep { $_ ne ':ctype' && $_ ne ':collate' }
                                  @categories;
                    # We add 1 to the category number;  This category number
                    # is -1
                    $^H{locale} |= (1 << 0);
                }
                else {
                    push @_, grep { $_ ne $arg } @categories;
                }
                next;
            }
            elsif ($arg eq ':characters') {
                push @_, ':ctype', ':collate';
                next;
            }

            $^H |= $locale::partial_hint_bits;

            # This form of the pragma overrides the other
            $^H &= ~$locale::hint_bits;

            $arg =~ s/^://;

            eval { require POSIX; POSIX->import('locale_h'); };

            # Map our names to the ones defined by POSIX
            my $LC = "LC_" . uc($arg);

            my $bit = eval "&POSIX::$LC";
            if (defined $bit) { # XXX Should we warn that this category isn't
                                # supported on this platform, or make it
                                # always be the C locale?

                # Verify our assumption.
                if (! ($bit >= 0 && $bit < 31)) {
                    require Carp;
                    Carp::croak("Cannot have ':$arg' parameter to 'use locale'"
                              . " on this platform.  Use the 'perlbug' utility"
                              . " to report this problem, or send email to"
                              . " 'perlbug\@perl.org'.  $LC=$bit");
                }

                # 1 is added so that the pseudo-category :characters, which is
                # -1, comes out 0.
                $^H{locale} |= 1 << ($bit + 1);
            }
        }
    }

}

sub unimport {
    $^H &= ~($locale::hint_bits | $locale::partial_hint_bits);
    $^H{locale} = 0;
}

1;
