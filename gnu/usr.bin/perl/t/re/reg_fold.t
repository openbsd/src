#!perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;
use warnings;
my $count=1;
my @tests;

my %todo_pass = map { $_ => 1 }
	    qw(00DF 1E9E FB00 FB01 FB02 FB03 FB04 FB05 FB06);

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
                $lhs = $chr;
                $rhs = "";
                foreach my $rhs_char (@folded) {
                    $rhs .= '[' if $charclass;
                    $rhs .=  $rhs_char;
                    $rhs .= ']' if $charclass;
                }
            }
            $lhs = "\"$lhs\"";
            $rhs = "/^$rhs\$/i";

            # Try both Latin1 and Unicode for code points below 256
            foreach my $upgrade ("", 'utf8::upgrade($c); ') {
                if ($upgrade) {
                    next if $swap && $fold_above_latin1;
                    next if !$swap && $cpv > 255;
                }
                my $eval = "my \$c = $lhs; $upgrade\$c =~ $rhs";
                #print __LINE__, ": $eval\n";
                push @tests, qq[ok(eval '$eval', '$eval - $comment')];
                if (! $swap && ($cp eq '0390' || $cp eq '03B0')) {
                    $tests[-1]="TODO: { local \$::TODO='[13:41] <BinGOs> cue *It is all Greek to me* joke.';\n$tests[-1] }"
                } elsif ($charclass && @folded > 1 && $swap && ! $upgrade && ! $fold_above_latin1) {
                    $tests[-1]="TODO: { local \$::TODO='Multi-char, non-utf8 folded inside character class [ ] doesnt work';\n$tests[-1] }"
                } elsif (! $upgrade && $cpv >= 128 && $cpv <= 255 && $cpv != 0xb5) {
                    $tests[-1]="TODO: { local \$::TODO='Most non-utf8 latin1 doesnt work';\n$tests[-1] }"
                } elsif (! $swap && $charclass && @folded > 1
		    && ! $todo_pass{$cp})
		{
                    # There are a few of these that pass; most fail.
                    $tests[-1]="TODO: { local \$::TODO='Some multi-char, f8 folded inside character class [ ] doesnt work';\n$tests[-1] }"
                }
                $count++;
            }
        }
    }
}
eval join ";\n","plan tests=>".($count-1),@tests,"1"
    or die $@;
__DATA__
