#!/usr/bin/perl -w
#
# Regenerate (overwriting only if changed):
#
#    locale_table.h
#
# Also accepts the standard regen_lib -q and -v args.
#
# This script is normally invoked from regen.pl.

BEGIN {
    require './regen/regen_lib.pl';
}

use strict;
use warnings;

sub open_print_header {
    my ($file, $quote) = @_;
    return open_new($file, '>',
                    { by => 'regen/locale.pl',
                      from => 'data in regen/locale.pl',
                      file => $file, style => '*',
                      copyright => [2023..2024],
                      quote => $quote });
}

my $l = open_print_header('locale_table.h');
print $l <<EOF;
/* This defines a macro for each individual locale category used on the this
 * system.  (The congomerate category LC_ALL is not included.)  This
 * file will be #included as the interior of various parallel arrays and in
 * other constructs; each usage will re-#define the macro to generate its
 * appropriate data.
 *
 * This guarantees the arrays will be parallel, and populated in the order
 * given here.  That order is mostly arbitrary.  LC_CTYPE is first because when
 * we are setting multiple categories, CTYPE often needs to match the other(s),
 * and the way the code is constructed, if we set the other category first, we
 * might otherwise have to set CTYPE twice.
 *
 * Each entry takes the token giving the category name, and either the name of
 * a function to call that does specialized set up for this category when it is
 * changed into, or NULL if no such set up is needed
 */

EOF

# It's not worth generalizing these further.
my %comment = ( COLLATE => <<EOF

        /* Perl outsources all its collation efforts to the libc strxfrm(), so
         * if it isn't available on the system, default "C" locale collation
         * gets used */
EOF
              );
my %extra_conditional = (
                          COLLATE => " || ! defined(HAS_STRXFRM)",
                        );


while (<DATA>) { # Read in the categories
    chomp;
    my ($name, $function) = split /\s*\|\s*/, $_;

    my $macro_arg = $function ? $function : "NULL";
    my $macro_with_func = "PERL_LOCALE_TABLE_ENTRY($name, $macro_arg)";
    my $macro_sans_func = "PERL_LOCALE_TABLE_ENTRY($name, NULL)";
    my ($common_macro, $macro_if_name, $macro_unless_name);
    if ($macro_with_func eq $macro_sans_func) {
        $common_macro = $macro_with_func;
        $macro_if_name = "";
        $macro_unless_name = "";
    }
    else {
        $common_macro = "";
        $macro_if_name = $macro_with_func;
        $macro_unless_name = $macro_sans_func;
    }

    print $l "#ifdef LC_${name}\n";
    print $l $comment{$name} if defined $comment{$name};
    print $l "\n    $common_macro\n\n" if $common_macro;

    my $extra = "";
    $extra = $extra_conditional{$name} if defined $extra_conditional{$name};
    print $l "#  if defined(NO_LOCALE) || defined(NO_LOCALE_${name})$extra\n";

    print $l "\n    $macro_unless_name\n\n" if $macro_unless_name;
    print $l <<~EOF;
        #    define HAS_IGNORED_LOCALE_CATEGORIES_
        #    define LC_${name}_AVAIL_  0
        #  else
        EOF
    print $l "\n    $macro_if_name\n\n" if $macro_if_name;
    print $l <<~EOF;
        #    define LC_${name}_AVAIL_  1
        #    define USE_LOCALE_${name}
        #  endif
        #else
        #  define LC_${name}_AVAIL_  0
        #endif
        EOF
}

close DATA;

read_only_bottom_close_and_rename($l);

# category | update function
__DATA__
CTYPE   | S_new_ctype
NUMERIC | S_new_numeric
COLLATE | S_new_collate
TIME
MESSAGES
MONETARY
ADDRESS
IDENTIFICATION
MEASUREMENT
PAPER
TELEPHONE
NAME
SYNTAX
TOD
