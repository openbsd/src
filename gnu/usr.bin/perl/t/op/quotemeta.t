#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib .);
    require Config; import Config;
    require "test.pl";
}

plan tests => 22;

if ($Config{ebcdic} eq 'define') {
    $_ = join "", map chr($_), 129..233;

    # 105 characters - 52 letters = 53 backslashes
    # 105 characters + 53 backslashes = 158 characters
    $_ = quotemeta $_;
    is(length($_), 158, "quotemeta string");
    # 104 non-backslash characters
    is(tr/\\//cd, 104, "tr count non-backslashed");
} else { # some ASCII descendant, then.
    $_ = join "", map chr($_), 32..127;

    # 96 characters - 52 letters - 10 digits - 1 underscore = 33 backslashes
    # 96 characters + 33 backslashes = 129 characters
    $_ = quotemeta $_;
    is(length($_), 129, "quotemeta string");
    # 95 non-backslash characters
    is(tr/\\//cd, 95, "tr count non-backslashed");
}

is(length(quotemeta ""), 0, "quotemeta empty string");

is("aA\UbB\LcC\EdD", "aABBccdD", 'aA\UbB\LcC\EdD');
is("aA\LbB\UcC\EdD", "aAbbCCdD", 'aA\LbB\UcC\EdD');
is("\L\upERL", "Perl", '\L\upERL');
is("\u\LpERL", "Perl", '\u\LpERL');
is("\U\lPerl", "pERL", '\U\lPerl');
is("\l\UPerl", "pERL", '\l\UPerl');
is("\u\LpE\Q#X#\ER\EL", "Pe\\#x\\#rL", '\u\LpE\Q#X#\ER\EL');
is("\l\UPe\Q!x!\Er\El", "pE\\!X\\!Rl", '\l\UPe\Q!x!\Er\El');
is("\Q\u\LpE.X.R\EL\E.", "Pe\\.x\\.rL.", '\Q\u\LpE.X.R\EL\E.');
is("\Q\l\UPe*x*r\El\E*", "pE\\*X\\*Rl*", '\Q\l\UPe*x*r\El\E*');
is("\U\lPerl\E\E\E\E", "pERL", '\U\lPerl\E\E\E\E');
is("\l\UPerl\E\E\E\E", "pERL", '\l\UPerl\E\E\E\E');

is(quotemeta("\x{263a}"), "\x{263a}", "quotemeta Unicode");
is(length(quotemeta("\x{263a}")), 1, "quotemeta Unicode length");

$a = "foo|bar";
is("a\Q\Ec$a", "acfoo|bar", '\Q\E');
is("a\L\Ec$a", "acfoo|bar", '\L\E');
is("a\l\Ec$a", "acfoo|bar", '\l\E');
is("a\U\Ec$a", "acfoo|bar", '\U\E');
is("a\u\Ec$a", "acfoo|bar", '\u\E');
