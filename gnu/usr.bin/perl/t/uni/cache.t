BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib .);
    require "test.pl";
}

plan tests => 1;

my $count = 0;
unshift @INC, sub {
       $count++ if $_[1] eq 'unicore/lib/gc_sc/Hira.pl';
};

my $s = 'foo';

$s =~ m/[\p{Hiragana}]/;
$s =~ m/[\p{Hiragana}]/;
$s =~ m/[\p{Hiragana}]/;
$s =~ m/[\p{Hiragana}]/;

is($count, 1, "Swatch hash caching kept us from reloading swatch hash.");
