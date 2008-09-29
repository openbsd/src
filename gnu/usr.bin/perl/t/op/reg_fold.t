#!perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict;
use warnings;
use Test::More;
my $count=1;
my @tests;

my $file="../lib/unicore/CaseFolding.txt";
open my $fh,"<",$file or die "Failed to read '$file': $!";
while (<$fh>) {
    chomp;
    my ($line,$comment)= split/\s+#\s+/, $_;
    my ($cp,$type,@fc)=split/[\s;]+/,$line||'';
    next unless $type and ($type eq 'F' or $type eq 'C');
    $_="\\x{$_}" for @fc;
    my $cpv=hex("0x$cp");
    my $chr="chr(0x$cp)";
    my @str;
    push @str,$chr if $cpv<128 or $cpv>256;
    if ($cpv<256) {
        push @str,"do{my \$c=$chr; utf8::upgrade(\$c); \$c}"
    }

    foreach my $str ( @str ) {
        my $expr="$str=~/@fc/ix";
        my $t=($cpv > 256 || $str=~/^do/) ? "unicode" : "latin";
        push @tests,
            qq[ok($expr,'$chr=~/@fc/ix - $comment ($t string)')];
        $tests[-1]="TODO: { local \$TODO='[13:41] <BinGOs> cue *It is all Greek to me* joke.';\n$tests[-1] }"
            if $cp eq '0390' or $cp eq '03B0';
        $count++;
    }
}
eval join ";\n","plan tests=>".($count-1),@tests,"1"
    or die $@;
__DATA__
