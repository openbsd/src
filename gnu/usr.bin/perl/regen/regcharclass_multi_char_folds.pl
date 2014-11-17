package regcharclass_multi_char_folds;
use 5.015;
use strict;
use warnings;
use Unicode::UCD "prop_invmap";

# This returns an array of strings of the form
#   "\x{foo}\x{bar}\x{baz}"
# of the sequences of code points that are multi-character folds in the
# current Unicode version.  If the parameter is 1, all such folds are
# returned.  If the parameters is 0, only the ones containing exclusively
# Latin1 characters are returned.  In the latter case all combinations of
# Latin1 characters that can fold to the base one are returned.  Thus for
# 'ss', it would return in addition, 'Ss', 'sS', and 'SS'.  This is because
# this code is designed to help regcomp.c, and EXACTFish regnodes.  For
# non-UTF-8 patterns, the strings are not folded, so we need to check for the
# upper and lower case versions.  For UTF-8 patterns, the strings are folded,
# so we only need to worry about the fold version.  There are no non-ASCII
# Latin1 multi-char folds currently, and none likely to be ever added.  Thus
# the output is the same as if it were just asking for ASCII characters, not
# full Latin1.  Hence, it is suitable for generating things that match
# EXACTFA.  It does check for and croak if there ever were to be an upper
# Latin1 range multi-character fold.
#
# This is designed for input to regen/regcharlass.pl.

sub gen_combinations ($;) {
    # Generate all combinations for the first parameter which is an array of
    # arrays.

    my ($fold_ref, $string, $i) = @_;
    $string = "" unless $string;
    $i = 0 unless $i;

    my @ret;

    # Look at each element in this level's array.
    foreach my $j (0 .. @{$fold_ref->[$i]} - 1) {

        # Append its representation to what we have currently
        my $new_string = sprintf "$string\\x{%X}", $fold_ref->[$i][$j];

        if ($i >=  @$fold_ref - 1) {    # Final level: just return it
            push @ret, "\"$new_string\"";
        }
        else {  # Generate the combinations for the next level with this one's
            push @ret, &gen_combinations($fold_ref, $new_string, $i + 1);
        }
    }

    return @ret;
}

sub multi_char_folds ($) {
    my $all_folds = shift;  # The single parameter is true if wants all
                            # multi-char folds; false if just the ones that
                            # are all ascii

    my ($cp_ref, $folds_ref, $format) = prop_invmap("Case_Folding");
    die "Could not find inversion map for Case_Folding" unless defined $format;
    die "Incorrect format '$format' for Case_Folding inversion map"
                                                        unless $format eq 'al';
    my @folds;

    for my $i (0 .. @$folds_ref - 1) {
        next unless ref $folds_ref->[$i];   # Skip single-char folds

        # The code in regcomp.c currently assumes that no multi-char fold
        # folds to the upper Latin1 range.  It's not a big deal to add; we
        # just have to forbid such a fold in EXACTFL nodes, like we do already
        # for ascii chars in EXACTFA (and EXACTFL) nodes.  But I (khw) doubt
        # that there will ever be such a fold created by Unicode, so the code
        # isn't there to occupy space and time; instead there is this check.
        die sprintf("regcomp.c can't cope with a latin1 multi-char fold (found in the fold of 0x%X", $cp_ref->[$i]) if grep { $_ < 256 && chr($_) !~ /[[:ascii:]]/ } @{$folds_ref->[$i]};

        # Create a line that looks like "\x{foo}\x{bar}\x{baz}" of the code
        # points that make up the fold.
        my $fold = join "", map { sprintf "\\x{%X}", $_ } @{$folds_ref->[$i]};
        $fold = "\"$fold\"";

        # Skip if something else already has this fold
        next if grep { $_ eq $fold } @folds;

        if ($all_folds) {
            push @folds, $fold
        }   # Skip if wants only all-ascii folds, and there is a non-ascii
        elsif (! grep { chr($_) =~ /[^[:ascii:]]/ } @{$folds_ref->[$i]}) {

            # If the fold is to a cased letter, replace the entry with an
            # array which also includes its upper case.
            my $this_fold_ref = $folds_ref->[$i];
            for my $j (0 .. @$this_fold_ref - 1) {
                my $this_ord = $this_fold_ref->[$j];
                if (chr($this_ord) =~ /\p{Cased}/) {
                    my $uc = ord(uc(chr($this_ord)));
                    undef $this_fold_ref->[$j];
                    @{$this_fold_ref->[$j]} = ( $this_ord, $uc);
                }
            }

            # Then generate all combinations of upper/lower case of the fold.
            push @folds, gen_combinations($this_fold_ref);

        }
    }

    # \x17F is the small LONG S, which folds to 's'.  Both Capital and small
    # LATIN SHARP S fold to 'ss'.  Therefore, they should also match two 17F's
    # in a row under regex /i matching.  But under /iaa regex matching, all
    # three folds to 's' are prohibited, but the sharp S's should still match
    # two 17F's.  This prohibition causes our regular regex algorithm that
    # would ordinarily allow this match to fail.  This is the only instance in
    # all Unicode of this kind of issue.  By adding a special case here, we
    # can use the regular algorithm (with some other changes elsewhere as
    # well).
    #
    # It would be possible to re-write the above code to automatically detect
    # and handle this case, and any others that might eventually get added to
    # the Unicode standard, but I (khw) don't think it's worth it.  I believe
    # that it's extremely unlikely that more folds to ASCII characters are
    # going to be added, and if I'm wrong, fold_grind.t has the intelligence
    # to detect them, and test that they work, at which point another special
    # case could be added here if necessary.
    #
    # No combinations of this with 's' need be added, as any of these
    # containing 's' are prohibted under /iaa.
    push @folds, "\"\x{17F}\x{17F}\"";


    return @folds;
}

1
