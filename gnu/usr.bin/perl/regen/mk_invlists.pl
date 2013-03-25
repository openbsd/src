#!perl -w
use 5.015;
use strict;
use warnings;
use Unicode::UCD "prop_invlist";
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
my $VERSION_DATA_STRUCTURE_TYPE = 1064334010;

my $out_fh = open_new('charclass_invlists.h', '>',
		      {style => '*', by => $0,
                      from => "Unicode::UCD"});

print $out_fh "/* See the generating file for comments */\n\n";

sub output_invlist ($$) {
    my $name = shift;
    my $invlist = shift;     # Reference to inversion list array

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

    print $out_fh "\nUV ${name}_invlist[] = {\n";

    print $out_fh "\t", scalar @$invlist, ",\t/* Number of elements */\n";
    print $out_fh "\t0,\t/* Current iteration position */\n";
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
    my $lookup_prop = $prop;
    $lookup_prop =~ s/^L1Posix/XPosix/ or $lookup_prop =~ s/^L1//;
    my @invlist = prop_invlist($lookup_prop);

    if ($lookup_prop ne $prop) {
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

    output_invlist($prop, \@invlist);
}

read_only_bottom_close_and_rename($out_fh)
