#!perl -w
use 5.015;
use strict;
use warnings;
use Unicode::UCD qw(prop_invlist prop_invmap);
require 'regen/regen_lib.pl';

# This program outputs charclass_invlists.h, which contains various inversion
# lists in the form of C arrays that are to be used as-is for inversion lists.
# Thus, the lists it contains are essentially pre-compiled, and need only a
# light-weight fast wrapper to make them usable at run-time.

# As such, this code knows about the internal structure of these lists, and
# any change made to that has to be done here as well.  A random number stored
# in the headers is used to minimize the possibility of things getting
# out-of-sync, or the wrong data structure being passed.  Currently that
# random number is:
my $VERSION_DATA_STRUCTURE_TYPE = 290655244;

my $out_fh = open_new('charclass_invlists.h', '>',
		      {style => '*', by => $0,
                      from => "Unicode::UCD"});

print $out_fh "/* See the generating file for comments */\n\n";

my %include_in_ext_re = ( NonL1_Perl_Non_Final_Folds => 1 );

sub output_invlist ($$) {
    my $name = shift;
    my $invlist = shift;     # Reference to inversion list array

    die "No inversion list for $name" unless defined $invlist
                                             && ref $invlist eq 'ARRAY'
                                             && @$invlist;

    # Output the inversion list $invlist using the name $name for it.
    # It is output in the exact internal form for inversion lists.

    my $zero_or_one;    # Is the last element of the header 0, or 1 ?

    # If the first element is 0, it goes in the header, instead of the body
    if ($invlist->[0] == 0) {
        shift @$invlist;

        $zero_or_one = 0;

        # Add a dummy 0 at the end so that the length is constant.  inversion
        # lists are always stored with enough room so that if they change from
        # beginning with 0, they don't have to grow.
        push @$invlist, 0;
    }
    else {
        $zero_or_one = 1;
    }

    print $out_fh "\n#ifndef PERL_IN_XSUB_RE\n" unless exists $include_in_ext_re{$name};
    print $out_fh "\nstatic UV ${name}_invlist[] = {\n";

    print $out_fh "\t", scalar @$invlist, ",\t/* Number of elements */\n";

    # This should be UV_MAX, but I (khw) am not confident that the suffixes
    # for specifying the constant are portable, e.g.  'ull' on a 32 bit
    # machine that is configured to use 64 bits; might need a Configure probe
    print $out_fh "\t0,\t/* Current iteration position */\n";
    print $out_fh "\t0,\t/* Cache of previous search index result */\n";
    print $out_fh "\t$VERSION_DATA_STRUCTURE_TYPE, /* Version and data structure type */\n";
    print $out_fh "\t", $zero_or_one,
                  ",\t/* 0 if this is the first element of the list proper;",
                  "\n\t\t   1 if the next element is the first */\n";

    # The main body are the UVs passed in to this routine.  Do the final
    # element separately
    for my $i (0 .. @$invlist - 1 - 1) {
        print $out_fh "\t$invlist->[$i],\n";
    }

    # The final element does not have a trailing comma, as C can't handle it.
    print $out_fh "\t$invlist->[-1]\n";

    print $out_fh "};\n";
    print $out_fh "\n#endif\n" unless exists $include_in_ext_re{$name};

}

sub mk_invlist_from_cp_list {

    # Returns an inversion list constructed from the sorted input array of
    # code points

    my $list_ref = shift;

    # Initialize to just the first element
    my @invlist = ( $list_ref->[0], $list_ref->[0] + 1);

    # For each succeeding element, if it extends the previous range, adjust
    # up, otherwise add it.
    for my $i (1 .. @$list_ref - 1) {
        if ($invlist[-1] == $list_ref->[$i]) {
            $invlist[-1]++;
        }
        else {
            push @invlist, $list_ref->[$i], $list_ref->[$i] + 1;
        }
    }
    return @invlist;
}

# Read in the Case Folding rules, and construct arrays of code points for the
# properties we need.
my ($cp_ref, $folds_ref, $format) = prop_invmap("Case_Folding");
die "Could not find inversion map for Case_Folding" unless defined $format;
die "Incorrect format '$format' for Case_Folding inversion map"
                                                    unless $format eq 'al';
my @has_multi_char_fold;
my @is_non_final_fold;

for my $i (0 .. @$folds_ref - 1) {
    next unless ref $folds_ref->[$i];   # Skip single-char folds
    push @has_multi_char_fold, $cp_ref->[$i];

    # Add to the the non-finals list each code point that is in a non-final
    # position
    for my $j (0 .. @{$folds_ref->[$i]} - 2) {
        push @is_non_final_fold, $folds_ref->[$i][$j]
                unless grep { $folds_ref->[$i][$j] == $_ } @is_non_final_fold;
    }
}

sub _Perl_Multi_Char_Folds {
    @has_multi_char_fold = sort { $a <=> $b } @has_multi_char_fold;
    return mk_invlist_from_cp_list(\@has_multi_char_fold);
}

sub _Perl_Non_Final_Folds {
    @is_non_final_fold = sort { $a <=> $b } @is_non_final_fold;
    return mk_invlist_from_cp_list(\@is_non_final_fold);
}

output_invlist("Latin1", [ 0, 256 ]);
output_invlist("AboveLatin1", [ 256 ]);

# We construct lists for all the POSIX and backslash sequence character
# classes in two forms:
#   1) ones which match only in the ASCII range
#   2) ones which match either in the Latin1 range, or the entire Unicode range
#
# These get compiled in, and hence affect the memory footprint of every Perl
# program, even those not using Unicode.  To minimize the size, currently
# the Latin1 version is generated for the beyond ASCII range except for those
# lists that are quite small for the entire range, such as for \s, which is 22
# UVs long plus 4 UVs (currently) for the header.
#
# To save even more memory, the ASCII versions could be derived from the
# larger ones at runtime, saving some memory (minus the expense of the machine
# instructions to do so), but these are all small anyway, so their total is
# about 100 UVs.
#
# In the list of properties below that get generated, the L1 prefix is a fake
# property that means just the Latin1 range of the full property (whose name
# has an X prefix instead of L1).
#
# An initial & means to use the subroutine from this file instead of an
# official inversion list.

for my $prop (qw(
                ASCII
                L1Cased
		VertSpace
                PerlSpace
                    XPerlSpace
                PosixAlnum
                    L1PosixAlnum
                PosixAlpha
                    L1PosixAlpha
                PosixBlank
                    XPosixBlank
                PosixCntrl
                    XPosixCntrl
                PosixDigit
                PosixGraph
                    L1PosixGraph
                PosixLower
                    L1PosixLower
                PosixPrint
                    L1PosixPrint
                PosixPunct
                    L1PosixPunct
                PosixSpace
                    XPosixSpace
                PosixUpper
                    L1PosixUpper
                PosixWord
                    L1PosixWord
                PosixXDigit
                    XPosixXDigit
                &NonL1_Perl_Non_Final_Folds
                &_Perl_Multi_Char_Folds
    )
) {

    # For the Latin1 properties, we change to use the eXtended version of the
    # base property, then go through the result and get rid of everything not
    # in Latin1 (above 255).  Actually, we retain the element for the range
    # that crosses the 255/256 boundary if it is one that matches the
    # property.  For example, in the Word property, there is a range of code
    # points that start at U+00F8 and goes through U+02C1.  Instead of
    # artifically cutting that off at 256 because 256 is the first code point
    # above Latin1, we let the range go to its natural ending.  That gives us
    # extra information with no added space taken.  But if the range that
    # crosses the boundary is one that doesn't match the property, we don't
    # start a new range above 255, as that could be construed as going to
    # infinity.  For example, the Upper property doesn't include the character
    # at 255, but does include the one at 256.  We don't include the 256 one.
    my $prop_name = $prop;
    my $is_local_sub = $prop_name =~ s/^&//;
    my $lookup_prop = $prop_name;
    my $l1_only = ($lookup_prop =~ s/^L1Posix/XPosix/ or $lookup_prop =~ s/^L1//);
    my $nonl1_only = 0;
    $nonl1_only = $lookup_prop =~ s/^NonL1// unless $l1_only;

    my @invlist;
    if ($is_local_sub) {
        @invlist = eval $lookup_prop;
    }
    else {
        @invlist = prop_invlist($lookup_prop, '_perl_core_internal_ok');
    }
    die "Could not find inversion list for '$lookup_prop'" unless @invlist;

    if ($l1_only) {
        for my $i (0 .. @invlist - 1 - 1) {
            if ($invlist[$i] > 255) {

                # In an inversion list, even-numbered elements give the code
                # points that begin ranges that match the property;
                # odd-numbered give ones that begin ranges that don't match.
                # If $i is odd, we are at the first code point above 255 that
                # doesn't match, which means the range it is ending does
                # match, and crosses the 255/256 boundary.  We want to include
                # this ending point, so increment $i, so the splice below
                # includes it.  Conversely, if $i is even, it is the first
                # code point above 255 that matches, which means there was no
                # matching range that crossed the boundary, and we don't want
                # to include this code point, so splice before it.
                $i++ if $i % 2 != 0;

                # Remove everything past this.
                splice @invlist, $i;
                last;
            }
        }
    }
    elsif ($nonl1_only) {
        my $found_nonl1 = 0;
        for my $i (0 .. @invlist - 1 - 1) {
            next if $invlist[$i] < 256;

            # Here, we have the first element in the array that indicates an
            # element above Latin1.  Get rid of all previous ones.
            splice @invlist, 0, $i;

            # If this one's index is not divisible by 2, it means that this
            # element is inverting away from being in the list, which means
            # all code points from 256 to this one are in this list.
            unshift @invlist, 256 if $i % 2 != 0;
            $found_nonl1 = 1;
            last;
        }
        die "No non-Latin1 code points in $lookup_prop" unless $found_nonl1;
    }

    output_invlist($prop_name, \@invlist);
}

read_only_bottom_close_and_rename($out_fh)
