#!perl -w
#
# This script is written intentionally in Shift JIS
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
use encoding 'shiftjis';

my @hiragana =  map {chr} ord("��")..ord("��");
my @katakana =  map {chr} ord("�@")..ord("��");
my $hiragana = join('' => @hiragana);
my $katakana = join('' => @katakana);
my %h2k; @h2k{@hiragana} = @katakana;
my %k2h; @k2h{@katakana} = @hiragana;

# print @hiragana, "\n";

my $str;

$str = $hiragana; $str =~ tr/��-��/�@-��/;
is($str, $katakana, "tr// # hiragana -> katakana");
$str = $katakana; $str =~ tr/�@-��/��-��/;
is($str, $hiragana, "tr// # hiragana -> katakana");

$str = $hiragana; eval qq(\$str =~ tr/��-��/�@-��/);
is($str, $katakana, "eval qq(tr//) # hiragana -> katakana");
$str = $katakana; eval qq(\$str =~ tr/�@-��/��-��/);
is($str, $hiragana, "eval qq(tr//) # hiragana -> katakana");

$str = $hiragana; $str =~ s/([��-��])/$h2k{$1}/go;
is($str, $katakana, "s/// # hiragana -> katakana");
$str = $katakana; $str =~ s/([�@-��])/$k2h{$1}/go;
is($str, $hiragana, "s/// # hiragana -> katakana");
__END__
