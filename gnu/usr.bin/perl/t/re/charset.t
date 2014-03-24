# Test the /a, /d, etc regex modifiers

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;
use warnings;
use Config;

plan('no_plan');

# Each case is a valid element of its hash key.  Choose, where available, an
# ASCII-range, Latin-1 non-ASCII range, and above Latin1 range code point.
my %testcases = (
    '\w' => [ ord("A"), 0xE2, 0x16B ],   # Below expects these to all be alpha
    '\d' => [ ord("0"), 0x0662 ],
    '\s' => [ ord("\t"), 0xA0, 0x1680 ],  # Below expects these to be [:blank:]
    '[:cntrl:]' => [ 0x00, 0x88 ],
    '[:graph:]' => [ ord("&"), 0xF7, 0x02C7 ], # Below expects these to be
                                               # [:print:]
    '[:lower:]' => [ ord("g"), 0xE3, 0x0127 ],
    '[:punct:]' => [ ord("!"), 0xBF, 0x055C ],
    '[:upper:]' => [ ord("G"), 0xC3, 0x0126 ],
    '[:xdigit:]' => [ ord("4"), 0xFF15 ],
);

$testcases{'[:digit:]'} = $testcases{'\d'};
$testcases{'[:alnum:]'} = $testcases{'\w'};
$testcases{'[:alpha:]'} = $testcases{'\w'};
$testcases{'[:blank:]'} = $testcases{'\s'};
$testcases{'[:print:]'} = $testcases{'[:graph:]'};
$testcases{'[:space:]'} = $testcases{'\s'};
$testcases{'[:word:]'} = $testcases{'\w'};

my @charsets = qw(a d u aa);
if (! is_miniperl() && $Config{d_setlocale}) {
    require POSIX;
    my $current_locale = POSIX::setlocale( &POSIX::LC_ALL, "C") // "";
    if ($current_locale eq 'C') {

        # test for d_setlocale is repeated here because this one is compile
        # time, and the one above is run time
        use if $Config{d_setlocale}, 'locale';

        # Some implementations don't have the 128-255 range characters all
        # mean nothing under the C locale (an example being VMS).  This is
        # legal, but since we don't know what the right answers should be,
        # skip the locale tests in that situation.
        for my $i (128 .. 255) {
            goto untestable_locale if chr($i) =~ /[[:print:]]/;
        }
        push @charsets, 'l';
    untestable_locale:
    }
}

# For each possible character set...
foreach my $charset (@charsets) {

    # And in utf8 or not
    foreach my $upgrade ("", 'utf8::upgrade($a); ') {

        # reverse gets the, \w, \s, \d first.
        for my $class (reverse sort keys %testcases) {

            # The complement of \w is \W; of [:posix:] is [:^posix:]
            my $complement = $class;
            if ($complement !~ s/ ( \[: ) /$1^/x) {
                $complement = uc($class);
            }

            # For each test case
            foreach my $ord (@{$testcases{$class}}) {
                my $char = display(chr($ord));

                # > 255 already implies upgraded.  Skip the ones that don't
                # have an explicit upgrade.  This shows more clearly in the
                # output which tests are in utf8, or not.
                next if $ord > 255 && ! $upgrade;

                my $reason = "";    # Explanation output with each test
                my $neg_reason = "";
                my $match = 1;      # Calculated whether test regex should
                                    # match or not

                # Everything always matches in ASCII, or under /u
                if ($ord < 128 || $charset eq 'u') {
                    $reason = "\"$char\" is a $class under /$charset";
                    $neg_reason = "\"$char\" is not a $complement under /$charset";
                }
                elsif ($charset eq "a" || $charset eq "aa") {
                    $match = 0;
                    $reason = "\"$char\" is non-ASCII, which can't be a $class under /a";
                    $neg_reason = "\"$char\" is non-ASCII, which is a $complement under /a";
                }
                elsif ($ord > 255) {
                    $reason = "\"$char\" is a $class under /$charset";
                    $neg_reason = "\"$char\" is not a $complement under /$charset";
                }
                elsif ($charset eq 'l') {

                    # We are using the C locale, which is essentially ASCII,
                    # but under utf8, the above-latin1 chars are treated as
                    # Unicode)
                    $reason = "\"$char\" is not a $class in this locale under /l";
                    $neg_reason = "\"$char\" is a $complement in this locale under /l";
                    $match = 0;
                }
                elsif ($upgrade) {
                    $reason = "\"$char\" is a $class in utf8 under /d";
                    $neg_reason = "\"$char\" is not a $complement in utf8 under /d";
                }
                else {
                    $reason = "\"$char\" is above-ASCII latin1, which requires utf8 to be a $class under /d";
                    $neg_reason = "\"$char\" is above-ASCII latin1, which is a $complement under /d (unless in utf8)";
                    $match = 0;
                }
                $reason = "; $reason" if $reason;
                $neg_reason = "; $neg_reason" if $neg_reason;

                my $op;
                my $neg_op;
                if ($match) {
                    $op = '=~';
                    $neg_op = '!~';
                }
                else {
                    $op = '!~';
                    $neg_op = '=~';
                }

                # In [...] or not
                foreach my $bracketed (0, 1) {
                    my $lb = "";
                    my $rb = "";
                    if ($bracketed) {

                        # Adds an extra char to the character class to make sure
                        # that the class doesn't get optimized away.
                        $lb = ($bracketed) ? '[_' : "";
                        $rb = ($bracketed) ? ']' : "";
                    }
                    else {  # [:posix:] must be inside outer [ ]
                        next if $class =~ /\[/;
                    }

                    my $length = 10;    # For regexec.c regrepeat() cases by
                                        # matching more than one item
                    # Test both class and its complement, and with one or more
                    # than one item to match.
                    foreach my $eval (
                        qq[my \$a = "$char"; $upgrade\$a $op qr/ (?$charset: $lb$class$rb ) /x],
                        qq[my \$a = "$char" x $length; $upgrade\$a $op qr/ (?$charset: $lb$class$rb\{$length} ) /x],
                    ) {
                        ok (eval $eval, $eval . $reason);
                    }
                    foreach my $eval (
                        qq[my \$a = "$char"; $upgrade\$a $neg_op qr/ (?$charset: $lb$complement$rb ) /x],
                        qq[my \$a = "$char" x $length; $upgrade\$a $neg_op qr/ (?$charset: $lb$complement$rb\{$length} ) /x],
                    ) {
                        ok (eval $eval, $eval . $neg_reason);
                    }
                }

                next if $class ne '\w';

                # Test \b, \B at beginning and end of string
                foreach my $eval (
                    qq[my \$a = "$char"; $upgrade\$a $op qr/ (?$charset: ^ \\b . ) /x],
                    qq[my \$a = "$char"; $upgrade\$a $op qr/ (?$charset: . \\b \$) /x],
                ) {
                    ok (eval $eval, $eval . $reason);
                }
                foreach my $eval (
                    qq[my \$a = "$char"; $upgrade\$a $neg_op qr/(?$charset: ^ \\B . ) /x],
                    qq[my \$a = "$char"; $upgrade\$a $neg_op qr/(?$charset: . \\B \$ ) /x],
                ) {
                    ok (eval $eval, $eval . $neg_reason);
                }

                # Test \b, \B adjacent to a non-word char, both before it and
                # after.  We test with ASCII, Latin1 and Unicode non-word chars
                foreach my $space_ord (@{$testcases{'\s'}}) {

                    # Useless to try to test non-utf8 when the ord itself
                    # forces utf8
                    next if $space_ord > 255 && ! $upgrade;

                    my $space = display(chr $space_ord);

                    foreach my $eval (
                        qq[my \$a = "$space$char"; $upgrade\$a $op qr/ (?$charset: . \\b . ) /x],
                        qq[my \$a = "$char$space"; $upgrade\$a $op qr/ (?$charset: . \\b . ) /x],
                    ) {
                        ok (eval $eval, $eval . $reason . "; \"$space\" is not a \\w");
                    }
                    foreach my $eval (
                        qq[my \$a = "$space$char"; $upgrade\$a $neg_op qr/ (?$charset: . \\B . ) /x],
                        qq[my \$a = "$char$space"; $upgrade\$a $neg_op qr/ (?$charset: . \\B . ) /x],
                    ) {
                        ok (eval $eval, $eval . $neg_reason . "; \"$space\" is not a \\w");
                    }
                }

                # Test \b, \B in the middle of two nominally word chars, but
                # one or both may be considered non-word depending on range
                # and charset.
                foreach my $other_ord (@{$testcases{'\w'}}) {
                    next if $other_ord > 255 && ! $upgrade;
                    my $other = display(chr $other_ord);

                    # Determine if the other char is a word char in current
                    # circumstances
                    my $other_is_word = 1;
                    my $other_reason = "\"$other\" is a $class under /$charset";
                    my $other_neg_reason = "\"$other\" is not a $complement under /$charset";
                    if ($other_ord > 127
                        && $charset ne 'u'
                        && (($charset eq "a" || $charset eq "aa")
                            || ($other_ord < 256 && ($charset eq 'l' || ! $upgrade))))
                    {
                        $other_is_word = 0;
                        $other_reason = "\"$other\" is not a $class under /$charset";
                        $other_neg_reason = "\"$other\" is a $complement under /$charset";
                    }
                    my $both_reason = $reason;
                    $both_reason .= "; $other_reason" if $other_ord != $ord;
                    my $both_neg_reason = $neg_reason;
                    $both_neg_reason .= "; $other_neg_reason" if $other_ord != $ord;

                    # If both are the same wordness, then \b will fail; \B
                    # succeed
                    if ($match == $other_is_word) {
                        $op = '!~';
                        $neg_op = '=~';
                    }
                    else {
                        $op = '=~';
                        $neg_op = '!~';
                    }

                    foreach my $eval (
                        qq[my \$a = "$other$char"; $upgrade\$a $op qr/ (?$charset: $other \\b $char ) /x],
                        qq[my \$a = "$char$other"; $upgrade\$a $op qr/ (?$charset: $char \\b $other ) /x],
                    ) {
                        ok (eval $eval, $eval . $both_reason);
                    }
                    foreach my $eval (
                        qq[my \$a = "$other$char"; $upgrade\$a $neg_op qr/ (?$charset: $other \\B $char ) /x],
                        qq[my \$a = "$char$other"; $upgrade\$a $neg_op qr/ (?$charset: $char \\B $other ) /x],
                    ) {
                        ok (eval $eval, $eval . $both_neg_reason);
                    }

                    next if $other_ord == $ord;

                    # These start with the \b or \B.  They are included, based
                    # on source code analysis, to force the testing of the FBC
                    # (find_by_class) portions of regexec.c.
                    foreach my $eval (
                        qq[my \$a = "$other$char"; $upgrade\$a $op qr/ (?$charset: \\b $char ) /x],
                        qq[my \$a = "$char$other"; $upgrade\$a $op qr/ (?$charset: \\b $other ) /x],
                    ) {
                        ok (eval $eval, $eval . $both_reason);
                    }
                    foreach my $eval (
                        qq[my \$a = "$other$char"; $upgrade\$a $neg_op qr/ (?$charset: \\B $char ) /x],
                        qq[my \$a = "$char$other"; $upgrade\$a $neg_op qr/ (?$charset: \\B $other ) /x],
                    ) {
                        ok (eval $eval, $eval . $both_neg_reason);
                    }
                }
            } # End of each test case in a class
        } # End of \w, \s, ...
    } # End of utf8 upgraded or not
}

plan(curr_test() - 1);
