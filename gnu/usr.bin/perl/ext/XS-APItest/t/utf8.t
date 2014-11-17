#!perl -w

use strict;
use Test::More;

use XS::APItest;

foreach ([0, '', '', 'empty'],
	 [0, 'N', 'N', '1 char'],
	 [1, 'NN', 'N', '1 char substring'],
	 [-2, 'Perl', 'Rules', 'different'],
	 [0, chr 163, chr 163, 'pound sign'],
	 [1, chr (163) . 10, chr (163) . 1, '10 pounds is more than 1 pound'],
	 [1, chr(163) . chr(163), chr 163, '2 pound signs are more than 1'],
	 [-2, ' $!', " \x{1F42B}!", 'Camels are worth more than 1 dollar'],
	 [-1, '!', "!\x{1F42A}", 'Initial substrings match'],
	) {
    my ($expect, $left, $right, $desc) = @$_;
    my $copy = $right;
    utf8::encode($copy);
    is(bytes_cmp_utf8($left, $copy), $expect, $desc);
    next if $right =~ tr/\0-\377//c;
    utf8::encode($left);
    is(bytes_cmp_utf8($right, $left), -$expect, "$desc reversed");
}

# Test uft8n_to_uvchr().  These provide essentially complete code coverage.

# Copied from utf8.h
my $UTF8_ALLOW_EMPTY            = 0x0001;
my $UTF8_ALLOW_CONTINUATION     = 0x0002;
my $UTF8_ALLOW_NON_CONTINUATION = 0x0004;
my $UTF8_ALLOW_SHORT            = 0x0008;
my $UTF8_ALLOW_LONG             = 0x0010;
my $UTF8_DISALLOW_SURROGATE     = 0x0020;
my $UTF8_WARN_SURROGATE         = 0x0040;
my $UTF8_DISALLOW_NONCHAR       = 0x0080;
my $UTF8_WARN_NONCHAR           = 0x0100;
my $UTF8_DISALLOW_SUPER         = 0x0200;
my $UTF8_WARN_SUPER             = 0x0400;
my $UTF8_DISALLOW_FE_FF         = 0x0800;
my $UTF8_WARN_FE_FF             = 0x1000;
my $UTF8_CHECK_ONLY             = 0x2000;

my $REPLACEMENT = 0xFFFD;

my @warnings;

use warnings 'utf8';
local $SIG{__WARN__} = sub { push @warnings, @_ };

# First test the malformations.  All these raise category utf8 warnings.
foreach my $test (
    [ "zero length string malformation", "", 0,
        $UTF8_ALLOW_EMPTY, 0, 0,
        qr/empty string/
    ],
    [ "orphan continuation byte malformation", "\x80a", 2,
        $UTF8_ALLOW_CONTINUATION, $REPLACEMENT, 1,
        qr/unexpected continuation byte/
    ],
    [ "premature next character malformation (immediate)", "\xc2a", 2,
        $UTF8_ALLOW_NON_CONTINUATION, $REPLACEMENT, 1,
        qr/unexpected non-continuation byte.*immediately after start byte/
    ],
    [ "premature next character malformation (non-immediate)", "\xf0\x80a", 3,
        $UTF8_ALLOW_NON_CONTINUATION, $REPLACEMENT, 2,
        qr/unexpected non-continuation byte .* 2 bytes after start byte/
    ],
    [ "too short malformation", "\xf0\x80a", 2,
        # Having the 'a' after this, but saying there are only 2 bytes also
        # tests that we pay attention to the passed in length
        $UTF8_ALLOW_SHORT, $REPLACEMENT, 2,
        qr/2 bytes, need 4/
    ],
    [ "overlong malformation", "\xc1\xaf", 2,
        $UTF8_ALLOW_LONG, ord('o'), 2,
        qr/2 bytes, need 1/
    ],
    [ "overflow malformation", "\xff\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf", 13,
        0,  # There is no way to allow this malformation
        $REPLACEMENT, 13,
        qr/overflow/
    ],
) {
    my ($testname, $bytes, $length, $allow_flags, $allowed_uv, $expected_len, $message ) = @$test;

    next if ! ok(length($bytes) >= $length, "$testname: Make sure won't read beyond buffer: " . length($bytes) . " >= $length");

    # Test what happens when this malformation is not allowed
    undef @warnings;
    my $ret_ref = test_utf8n_to_uvchr($bytes, $length, 0);
    is($ret_ref->[0], 0, "$testname: disallowed: Returns 0");
    is($ret_ref->[1], $expected_len, "$testname: disallowed: Returns expected length");
    if (is(scalar @warnings, 1, "$testname: disallowed: Got a single warning ")) {
        like($warnings[0], $message, "$testname: disallowed: Got expected warning");
    }
    else {
        if (scalar @warnings) {
            note "The warnings were: " . join(", ", @warnings);
        }
    }

    {   # Next test when disallowed, and warnings are off.
        undef @warnings;
        no warnings 'utf8';
        my $ret_ref = test_utf8n_to_uvchr($bytes, $length, 0);
        is($ret_ref->[0], 0, "$testname: disallowed: no warnings 'utf8': Returns 0");
        is($ret_ref->[1], $expected_len, "$testname: disallowed: no warnings 'utf8': Returns expected length");
        if (!is(scalar @warnings, 0, "$testname: disallowed: no warnings 'utf8': no warnings generated")) {
            note "The warnings were: " . join(", ", @warnings);
        }
    }

    # Test with CHECK_ONLY
    undef @warnings;
    $ret_ref = test_utf8n_to_uvchr($bytes, $length, $UTF8_CHECK_ONLY);
    is($ret_ref->[0], 0, "$testname: CHECK_ONLY: Returns 0");
    is($ret_ref->[1], -1, "$testname: CHECK_ONLY: returns expected length");
    if (! is(scalar @warnings, 0, "$testname: CHECK_ONLY: no warnings generated")) {
        note "The warnings were: " . join(", ", @warnings);
    }

    next if $allow_flags == 0;    # Skip if can't allow this malformation

    # Test when the malformation is allowed
    undef @warnings;
    $ret_ref = test_utf8n_to_uvchr($bytes, $length, $allow_flags);
    is($ret_ref->[0], $allowed_uv, "$testname: allowed: Returns expected uv");
    is($ret_ref->[1], $expected_len, "$testname: allowed: Returns expected length");
    if (!is(scalar @warnings, 0, "$testname: allowed: no warnings generated"))
    {
        note "The warnings were: " . join(", ", @warnings);
    }
}

my $FF_ret;

use Unicode::UCD;
my $has_quad = ($Unicode::UCD::MAX_CP > 0xFFFF_FFFF);
if ($has_quad) {
    no warnings qw{portable overflow};
    $FF_ret = 0x1000000000;
}
else {  # The above overflows unless a quad platform
    $FF_ret = 0;
}

# Now test the cases where a legal code point is generated, but may or may not
# be allowed/warned on.
my @tests = (
    [ "surrogate", "\xed\xa4\x8d",
        $UTF8_WARN_SURROGATE, $UTF8_DISALLOW_SURROGATE, 'surrogate', 0xD90D, 3,
        qr/surrogate/
    ],
    [ "non_unicode", "\xf4\x90\x80\x80",
        $UTF8_WARN_SUPER, $UTF8_DISALLOW_SUPER, 'non_unicode', 0x110000, 4,
        qr/not Unicode/
    ],
    [ "non-character code point", "\xEF\xB7\x90",
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR, 'nonchar', 0xFDD0, 3,
        qr/Unicode non-character.*is illegal for open interchange/
    ],
    [ "begins with FE", "\xfe\x82\x80\x80\x80\x80\x80",

        # This code point is chosen so that it is representable in a UV on
        # 32-bit machines
        $UTF8_WARN_FE_FF, $UTF8_DISALLOW_FE_FF, 'utf8', 0x80000000, 7,
        qr/Code point 0x80000000 is not Unicode, and not portable/
    ],
    [ "overflow with FE/FF",
        # This tests the interaction of WARN_FE_FF/DISALLOW_FE_FF with
        # overflow.  The overflow malformation is never allowed, so preventing
        # it takes precedence if the FE_FF options would otherwise allow in an
        # overflowing value.  These two code points (1 for 32-bits; 1 for 64)
        # were chosen because the old overflow detection algorithm did not
        # catch them; this means this test also checks for that fix.
        ($has_quad)
            ? "\xff\x80\x90\x90\x90\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf"
            : "\xfe\x86\x80\x80\x80\x80\x80",

        # We include both warning categories to make sure the FE_FF one has
        # precedence
        "$UTF8_WARN_FE_FF|$UTF8_WARN_SUPER", "$UTF8_DISALLOW_FE_FF", 'utf8', 0,
        ($has_quad) ? 13 : 7,
        qr/overflow at byte .*, after start byte 0xf/
    ],
);

if ($has_quad) {    # All FF's will overflow on 32 bit
    push @tests,
        [ "begins with FF", "\xff\x80\x80\x80\x80\x80\x81\x80\x80\x80\x80\x80\x80",
            $UTF8_WARN_FE_FF, $UTF8_DISALLOW_FE_FF, 'utf8', $FF_ret, 13,
            qr/Code point 0x.* is not Unicode, and not portable/
        ];
}

foreach my $test (@tests) {
    my ($testname, $bytes, $warn_flags, $disallow_flags, $category, $allowed_uv, $expected_len, $message ) = @$test;

    my $length = length $bytes;
    my $will_overflow = $testname =~ /overflow/;

    # This is more complicated than the malformations tested earlier, as there
    # are several orthogonal variables involved.  We test all the subclasses
    # of utf8 warnings to verify they work with and without the utf8 class,
    # and don't have effects on other sublass warnings
    foreach my $warning ('utf8', 'surrogate', 'nonchar', 'non_unicode') {
        foreach my $warn_flag (0, $warn_flags) {
            foreach my $disallow_flag (0, $disallow_flags) {
                foreach my $do_warning (0, 1) {

                    my $eval_warn = $do_warning
                                  ? "use warnings '$warning'"
                                  : $warning eq "utf8"
                                  ? "no warnings 'utf8'"
                                  : "use warnings 'utf8'; no warnings '$warning'";

                    # is effectively disallowed if will overflow, even if the
                    # flag indicates it is allowed, fix up test name to
                    # indicate this as well
                    my $disallowed = $disallow_flag || $will_overflow;

                    my $this_name = "$testname: " . (($disallow_flag)
                                                    ? 'disallowed'
                                                    : ($disallowed)
                                                        ? 'FE_FF allowed'
                                                        : 'allowed');
                    $this_name .= ", $eval_warn";
                    $this_name .= ", " . (($warn_flag)
                                          ? 'with warning flag'
                                          : 'no warning flag');

                    undef @warnings;
                    my $ret_ref;
                    #note __LINE__ . ": $eval_warn; \$ret_ref = test_utf8n_to_uvchr('$bytes', $length, $warn_flag|$disallow_flag)";
                    my $eval_text = "$eval_warn; \$ret_ref = test_utf8n_to_uvchr('$bytes', $length, $warn_flag|$disallow_flag)";
                    eval "$eval_text";
                    if (! ok ("$@ eq ''", "$this_name: eval succeeded")) {
                        note "\$!='$!'; eval'd=\"$eval_text\"";
                        next;
                    }
                    if ($disallowed) {
                        is($ret_ref->[0], 0, "$this_name: Returns 0");
                    }
                    else {
                        is($ret_ref->[0], $allowed_uv,
                                            "$this_name: Returns expected uv");
                    }
                    is($ret_ref->[1], $expected_len,
                                        "$this_name: Returns expected length");

                    if (! $do_warning
                        && ($warning eq 'utf8' || $warning eq $category))
                    {
                        if (!is(scalar @warnings, 0,
                                            "$this_name: No warnings generated"))
                        {
                            note "The warnings were: " . join(", ", @warnings);
                        }
                    }
                    elsif ($will_overflow
                           && ! $disallow_flag
                           && $warning eq 'utf8')
                    {

                        # Will get the overflow message instead of the expected
                        # message under these circumstances, as they would
                        # otherwise accept an overflowed value, which the code
                        # should not allow, so falls back to overflow.
                        if (is(scalar @warnings, 1,
                               "$this_name: Got a single warning "))
                        {
                            like($warnings[0], qr/overflow/,
                                            "$this_name: Got overflow warning");
                        }
                        else {
                            if (scalar @warnings) {
                                note "The warnings were: "
                                                        . join(", ", @warnings);
                            }
                        }
                    }
                    elsif ($warn_flag
                           && ($warning eq 'utf8' || $warning eq $category))
                    {
                        if (is(scalar @warnings, 1,
                               "$this_name: Got a single warning "))
                        {
                            like($warnings[0], $message,
                                            "$this_name: Got expected warning");
                        }
                        else {
                            if (scalar @warnings) {
                                note "The warnings were: "
                                                        . join(", ", @warnings);
                            }
                        }
                    }

                    # Check CHECK_ONLY results when the input is disallowed.  Do
                    # this when actually disallowed, not just when the
                    # $disallow_flag is set
                    if ($disallowed) {
                        undef @warnings;
                        $ret_ref = test_utf8n_to_uvchr($bytes, $length,
                                                $disallow_flag|$UTF8_CHECK_ONLY);
                        is($ret_ref->[0], 0, "$this_name, CHECK_ONLY: Returns 0");
                        is($ret_ref->[1], -1,
                            "$this_name: CHECK_ONLY: returns expected length");
                        if (! is(scalar @warnings, 0,
                            "$this_name, CHECK_ONLY: no warnings generated"))
                        {
                            note "The warnings were: " . join(", ", @warnings);
                        }
                    }
                }
            }
        }
    }
}

done_testing;
