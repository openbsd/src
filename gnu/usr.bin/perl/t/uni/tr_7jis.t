#! perl -w
#
# This script is written intentionally in ISO-2022-JP
# requires Encode 1.83 or better to work
# -- dankogai

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    skip_all_without_dynamic_extension('Encode');
    skip_all("no encoding pragma in EBCDIC") if $::IS_EBCDIC;
    skip_all_without_perlio();
}

use strict;
plan(tests => 6);
no warnings 'deprecated';
use encoding 'iso-2022-jp';

my @hiragana =  map {chr} ord("$B$!(B")..ord("$B$s(B");
my @katakana =  map {chr} ord("$B%!(B")..ord("$B%s(B");
my $hiragana = join('' => @hiragana);
my $katakana = join('' => @katakana);
my %h2k; @h2k{@hiragana} = @katakana;
my %k2h; @k2h{@katakana} = @hiragana;

# print @hiragana, "\n";

my $str;

$str = $hiragana; $str =~ tr/$B$!(B-$B$s(B/$B%!(B-$B%s(B/;
is($str, $katakana, "tr// # hiragana -> katakana");
$str = $katakana; $str =~ tr/$B%!(B-$B%s(B/$B$!(B-$B$s(B/;
is($str, $hiragana, "tr// # hiragana -> katakana");

$str = $hiragana; eval qq(\$str =~ tr/$B$!(B-$B$s(B/$B%!(B-$B%s(B/);
is($str, $katakana, "eval qq(tr//) # hiragana -> katakana");
$str = $katakana; eval qq(\$str =~ tr/$B%!(B-$B%s(B/$B$!(B-$B$s(B/);
is($str, $hiragana, "eval qq(tr//) # hiragana -> katakana");

$str = $hiragana; $str =~ s/([$B$!(B-$B$s(B])/$h2k{$1}/go;
is($str, $katakana, "s/// # hiragana -> katakana");
$str = $katakana; $str =~ s/([$B%!(B-$B%s(B])/$k2h{$1}/go;
is($str, $hiragana, "s/// # hiragana -> katakana");
__END__
