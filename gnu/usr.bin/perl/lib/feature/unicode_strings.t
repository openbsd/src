use warnings;
use strict;

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan(13312);    # Determined by experimentation

# Test the upper/lower/title case mappings for all characters 0-255.

# First compute the case mappings without resorting to the functions we're
# testing.

# Initialize the arrays so each $i maps to itself.
my @posix_to_upper;
for my $i (0 .. 255) {
    $posix_to_upper[$i] = chr($i);
}
my @posix_to_lower
= my @posix_to_title
= my @latin1_to_upper
= my @latin1_to_lower
= my @latin1_to_title
= @posix_to_upper;

# Override the elements in the to_lower arrays that have different lower case 
# mappings
for my $i (0x41 .. 0x5A) {
    $posix_to_lower[$i] = chr(ord($posix_to_lower[$i]) + 32);
    $latin1_to_lower[$i] = chr(ord($latin1_to_lower[$i]) + 32);
}

# Same for upper and title
for my $i (0x61 .. 0x7A) {
    $posix_to_upper[$i] = chr(ord($posix_to_upper[$i]) - 32);
    $latin1_to_upper[$i] = chr(ord($latin1_to_upper[$i]) - 32);
    $posix_to_title[$i] = chr(ord($posix_to_title[$i]) - 32);
    $latin1_to_title[$i] = chr(ord($latin1_to_title[$i]) - 32);
}

# And the same for those in the latin1 range
for my $i (0xC0 .. 0xD6, 0xD8 .. 0xDE) {
    $latin1_to_lower[$i] = chr(ord($latin1_to_lower[$i]) + 32);
}
for my $i (0xE0 .. 0xF6, 0xF8 .. 0xFE) {
    $latin1_to_upper[$i] = chr(ord($latin1_to_upper[$i]) - 32);
    $latin1_to_title[$i] = chr(ord($latin1_to_title[$i]) - 32);
}

# Override the abnormal cases.
$latin1_to_upper[0xB5] = chr(0x39C);
$latin1_to_title[0xB5] = chr(0x39C);
$latin1_to_upper[0xDF] = 'SS';
$latin1_to_title[0xDF] = 'Ss';
$latin1_to_upper[0xFF] = chr(0x178);
$latin1_to_title[0xFF] = chr(0x178);

my $repeat = 25;    # Length to make strings.

# Create hashes of strings in several ranges, both for uc and lc.
my %posix;
$posix{'uc'} = 'A' x $repeat;
$posix{'lc'} = 'a' x $repeat ;

my %cyrillic;
$cyrillic{'uc'} = chr(0x42F) x $repeat;
$cyrillic{'lc'} = chr(0x44F) x $repeat;

my %latin1;
$latin1{'uc'} = chr(0xD8) x $repeat;
$latin1{'lc'} = chr(0xF8) x $repeat;

my %empty;
$empty{'lc'} = $empty{'uc'} = "";

# Loop so prefix each character being tested with nothing, and the various
# strings; then loop for suffixes of those strings as well.
for my  $prefix (\%empty, \%posix, \%cyrillic, \%latin1) {
    for my  $suffix (\%empty, \%posix, \%cyrillic, \%latin1) {
        for my $i (0 .. 255) {  # For each possible posix or latin1 character
            my $cp = sprintf "U+%04X", $i;

            # First try using latin1 (Unicode) semantics.
            use feature "unicode_strings";    

            my $phrase = 'with uni8bit';
            my $char = chr($i);
            my $pre_lc = $prefix->{'lc'};
            my $pre_uc = $prefix->{'uc'};
            my $post_lc = $suffix->{'lc'};
            my $post_uc = $suffix->{'uc'};
            my $to_upper = $pre_lc . $char . $post_lc;
            my $expected_upper = $pre_uc . $latin1_to_upper[$i] . $post_uc;
            my $to_lower = $pre_uc . $char . $post_uc;
            my $expected_lower = $pre_lc . $latin1_to_lower[$i] . $post_lc;

            is (uc($to_upper), $expected_upper,
                display("$cp: $phrase: uc($to_upper) eq $expected_upper"));
            is (lc($to_lower), $expected_lower,
                display("$cp: $phrase: lc($to_lower) eq $expected_lower"));

            if ($pre_uc eq "") {    # Title case if null prefix.
                my $expected_title = $latin1_to_title[$i] . $post_lc;
                is (ucfirst($to_upper), $expected_title,
                    display("$cp: $phrase: ucfirst($to_upper) eq $expected_title"));
                my $expected_lcfirst = $latin1_to_lower[$i] . $post_uc;
                is (lcfirst($to_lower), $expected_lcfirst,
                    display("$cp: $phrase: lcfirst($to_lower) eq $expected_lcfirst"));
            }

            # Then try with posix semantics.
            no feature "unicode_strings";
            $phrase = 'no uni8bit';

            # These don't contribute anything in this case.
            next if $suffix == \%cyrillic;
            next if $suffix == \%latin1;
            next if $prefix == \%cyrillic;
            next if $prefix == \%latin1;

            $expected_upper = $pre_uc . $posix_to_upper[$i] . $post_uc;
            $expected_lower = $pre_lc . $posix_to_lower[$i] . $post_lc;

            is (uc($to_upper), $expected_upper,
                display("$cp: $phrase: uc($to_upper) eq $expected_upper"));
            is (lc($to_lower), $expected_lower,
                display("$cp: $phrase: lc($to_lower) eq $expected_lower"));

            if ($pre_uc eq "") {
                my $expected_title = $posix_to_title[$i] . $post_lc;
                is (ucfirst($to_upper), $expected_title,
                    display("$cp: $phrase: ucfirst($to_upper) eq $expected_title"));
                my $expected_lcfirst = $posix_to_lower[$i] . $post_uc;
                is (lcfirst($to_lower), $expected_lcfirst,
                    display("$cp: $phrase: lcfirst($to_lower) eq $expected_lcfirst"));
            }
        }
    }
}
