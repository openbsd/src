#!perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
    skip_all_if_miniperl("no dynamic loading on miniperl, no File::Spec");
}

use strict;
use warnings;
my @tests;

my $file="../lib/unicore/CaseFolding.txt";
open my $fh,"<",$file or die "Failed to read '$file': $!";
while (<$fh>) {
    chomp;
    my ($line,$comment)= split/\s+#\s+/, $_;
    my ($cp,$type,@folded)=split/[\s;]+/,$line||'';
    next unless $type and ($type eq 'F' or $type eq 'C');
    my $fold_above_latin1 = grep { hex("0x$_") > 255 } @folded;
    $_="\\x{$_}" for @folded;
    my $cpv=hex("0x$cp");
    my $chr="\\x{$cp}";
    my @str;
    foreach my $swap (0, 1) {   # swap lhs and rhs, or not.
        foreach my $charclass (0, 1) {   # Put rhs in [...], or not
            my $lhs;
            my $rhs;
            if ($swap) {
                $lhs = join "", @folded;
                $rhs = $chr;
                $rhs = "[$rhs]" if $charclass;
            } else {
                #next if $charclass && @folded > 1;
                $lhs = $chr;
                $rhs = "";
                foreach my $rhs_char (@folded) {
                    $rhs .= '[' if $charclass;
                    $rhs .=  $rhs_char;
                    $rhs .= ']' if $charclass;
                }
            }
            $lhs = "\"$lhs\"";
            $rhs = "/^$rhs\$/iu";

            # Try both Latin1 and Unicode for code points below 256
            foreach my $upgrade ("", 'utf8::upgrade($c); ') {
                if ($upgrade) { # No need to upgrade if already must be in
                                # utf8
                    next if $swap && $fold_above_latin1;
                    next if !$swap && $cpv > 255;
                }
                my $eval = "my \$c = $lhs; $upgrade\$c =~ $rhs";
                #print __LINE__, ": $eval\n";
                push @tests, qq[ok(eval '$eval', '$eval - $comment')];
                if (! $swap && $charclass && @folded > 1)
		{
                    $tests[-1]="TODO: { local \$::TODO='A multi-char fold \"foo\", doesnt work for /[f][o][o]/i';\n$tests[-1] }"
                }
            }
        }
    }
}

# Now verify the case folding tables.  First compute the mappings without
# resorting to the functions we're testing.

# Initialize the array so each $i maps to itself.
my @fold_ascii;
for my $i (0 .. 255) {
    $fold_ascii[$i] = $i;
}
my @fold_latin1 = @fold_ascii;

# Override the uppercase elements to fold to their lower case equivalents,
# using the fact that 'A' in ASCII is 0x41, 'a' is 0x41+32, 'B' is 0x42, and
# so on.  The same paradigm applies for most of the Latin1 range cased
# characters, but in posix anything outside ASCII maps to itself, as we've
# already set up.
for my $i (0x41 .. 0x5A, 0xC0 .. 0xD6, 0xD8 .. 0xDE) {
    my $upper_ord = ord_latin1_to_native($i);
    my $lower_ord = ord_latin1_to_native($i + 32);

    $fold_latin1[$upper_ord] = $lower_ord;

    next if $i > 127;
    $fold_ascii[$upper_ord] = $lower_ord;
}

# Same for folding lower to the upper equivalents
for my $i (0x61 .. 0x7A, 0xE0 .. 0xF6, 0xF8 .. 0xFE) {
    my $lower_ord = ord_latin1_to_native($i);
    my $upper_ord = ord_latin1_to_native($i - 32);

    $fold_latin1[$lower_ord] = $upper_ord;

    next if $i > 127;
    $fold_ascii[$lower_ord] = $upper_ord;
}

# Test every latin1 character for the correct values in both /u and /d
for my $i (0 .. 255) {
    my $chr = sprintf "\\x%02X", $i;
    my $hex_fold_ascii = sprintf "0x%02X", $fold_ascii[$i];
    my $hex_fold_latin1 = sprintf "0x%02X", $fold_latin1[$i];
    push @tests, qq[like chr($hex_fold_ascii), qr/(?d:$chr)/i, 'chr($hex_fold_ascii) =~ qr/(?d:$chr)/i'];
    push @tests, qq[like chr($hex_fold_latin1), qr/(?u:$chr)/i, 'chr($hex_fold_latin1) =~ qr/(?u:$chr)/i'];
}


push @tests, qq[like chr(0x0430), qr/[=\x{0410}-\x{0411}]/i, 'Bug #71752 Unicode /i char in a range'];
push @tests, qq[like 'a', qr/\\p{Upper}/i, "'a' =~ /\\\\p{Upper}/i"];
push @tests, q[my $c = "\x{212A}"; my $p = qr/(?:^[\x{004B}_]+$)/i; utf8::upgrade($p); like $c, $p, 'Bug #78994: my $c = "\x{212A}"; my $p = qr/(?:^[\x{004B}_]+$)/i; utf8::upgrade($p); $c =~ $p'];

use charnames ":full";
push @tests, q[my $re1 = "\N{WHITE SMILING FACE}";like "\xE8", qr/[\w$re1]/, 'my $re = "\N{WHITE SMILING FACE}"; "\xE8" =~ qr/[\w$re]/'];
push @tests, q[my $re2 = "\N{WHITE SMILING FACE}";like "\xE8", qr/\w|$re2/, 'my $re = "\N{WHITE SMILING FACE}"; "\xE8" =~ qr/\w|$re/'];

eval join ";\n","plan tests=>". (scalar @tests), @tests, "1"
    or die $@;
__DATA__
