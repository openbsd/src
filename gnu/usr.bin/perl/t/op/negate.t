#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 16;

# Some of these will cause warnings if left on.  Here we're checking the
# functionality, not the warnings.
no warnings "numeric";

# test cases based on [perl #36675] -'-10' eq '+10'
is(- 10, -10, "Simple numeric negation to negative");
is(- -10, 10, "Simple numeric negation to positive");
is(-"10", -10, "Negation of a positive string to negative");
is(-"10.0", -10, "Negation of a positive decimal sting to negative");
is(-"10foo", -10, "Negation of a numeric-lead string returns negation of numeric");
is(-"-10", "+10", 'Negation of string starting with "-" returns a string starting with "+" - numeric');
is(-"-10.0", "+10.0", 'Negation of string starting with "-" returns a string starting with "+" - decimal');
is(-"-10foo", "+10foo", 'Negation of string starting with "-" returns a string starting with "+" - non-numeric');
is(-"xyz", "-xyz", 'Negation of a negative string adds "-" to the front');
is(-"-xyz", "+xyz", "Negation of a negative string to positive");
is(-"+xyz", "-xyz", "Negation of a positive string to negative");
is(-bareword, "-bareword", "Negation of bareword treated like a string");
is(- -bareword, "+bareword", "Negation of -bareword returns string +bareword");
is(-" -10", 10, "Negation of a whitespace-lead numeric string");
is(-" -10.0", 10, "Negation of a whitespace-lead decimal string");
is(-" -10foo", 10, "Negation of a whitespace-lead sting starting with a numeric")
