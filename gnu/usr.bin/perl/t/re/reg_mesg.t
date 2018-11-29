#!./perl -w

$|=1;   # outherwise things get mixed up in output

BEGIN {
	chdir 't' if -d 't';
	@INC = qw '../lib ../ext/re';
	require './test.pl';
	skip_all_without_unicode_tables();
	eval 'require Config'; # assume defaults if this fails
}

use strict;
use open qw(:utf8 :std);

##
## If the markers used are changed (search for "MARKER1" in regcomp.c),
## update only these two regexs, and leave the {#} in the @death/@warning
## arrays below. The {#} is a meta-marker -- it marks where the marker should
## go.
##
## Returns empty string if that is what is expected.  Otherwise, handles
## either a scalar, turning it into a single element array; or a ref to an
## array, adjusting each element.  If called in array context, returns an
## array, otherwise the join of all elements

sub fixup_expect {
    my $expect_ref = shift;
    return "" if $expect_ref eq "";

    my @expect;
    if (ref $expect_ref) {
        @expect = @$expect_ref;
    }
    else {
        @expect = $expect_ref;
    }

    foreach my $element (@expect) {
        $element =~ s/{\#}/in regex; marked by <-- HERE in/;
        $element =~ s/{\#}/ <-- HERE /;
        $element .= " at ";
    }
    return wantarray ? @expect : join "", @expect;
}

## Because we don't "use utf8" in this file, we need to do some extra legwork
## for the utf8 tests: Prepend 'use utf8' to the pattern, and mark the strings
## to check against as UTF-8, but for this all to work properly, the character
## 'ネ' (U+30CD) is required in each pattern somewhere as a marker.
##
## This also creates a second variant of the tests to check if the
## latin1 error messages are working correctly.  Because we don't 'use utf8',
## we can't tell if something is UTF-8 or Latin1, so you need the suffix
## '; no latin1' to not have the second variant.
my $l1   = "\x{ef}";
my $utf8 = "\x{30cd}";
utf8::encode($utf8);

sub mark_as_utf8 {
    my @ret;
    for (my $i = 0; $i < @_; $i += 2) {
        my $pat = $_[$i];
        my $msg = $_[$i+1];
        my $l1_pat = $pat =~ s/$utf8/$l1/gr;
        my $l1_msg;
        $pat = "use utf8; $pat";

        if (ref $msg) {
            $l1_msg = [ map { s/$utf8/$l1/gr } @$msg ];
            @$msg   = map { my $c = $_; utf8::decode($c); $c } @$msg;
        }
        else {
            $l1_msg = $msg =~ s/$utf8/$l1/gr;
            utf8::decode($msg);
        }
        push @ret, $pat => $msg;

        push @ret, $l1_pat => $l1_msg unless $l1_pat =~ /#no latin1/;
    }
    return @ret;
}

my $inf_m1 = ($Config::Config{reg_infty} || 32767) - 1;
my $inf_p1 = $inf_m1 + 2;

my $B_hex = sprintf("\\x%02X", ord "B");
my $low_mixed_alpha = ('A' lt 'a') ? 'A' : 'a';
my $high_mixed_alpha = ('A' lt 'a') ? 'a' : 'A';
my $low_mixed_digit = ('A' lt '0') ? 'A' : '0';
my $high_mixed_digit = ('A' lt '0') ? '0' : 'A';

my $colon_hex = sprintf "%02X", ord(":");
my $tab_hex = sprintf "%02X", ord("\t");

##
## Key-value pairs of code/error of code that should have fatal errors.
##
my @death =
(
 '/[[=foo=]]/' => 'POSIX syntax [= =] is reserved for future extensions {#} m/[[=foo=]{#}]/',

 '/(?<= .*)/' =>  'Variable length lookbehind not implemented in regex m/(?<= .*)/',

 '/(?<= x{1000})/' => 'Lookbehind longer than 255 not implemented in regex m/(?<= x{1000})/',

 '/(?@)/' => 'Sequence (?@...) not implemented {#} m/(?@{#})/',

 '/(?{ 1/' => 'Missing right curly or square bracket',

 '/(?(1x))/' => 'Switch condition not recognized {#} m/(?(1x{#}))/',
 '/(?(1x(?#)))/'=> 'Switch condition not recognized {#} m/(?(1x{#}(?#)))/',

 '/(?(1)/'    => 'Switch (?(condition)... not terminated {#} m/(?(1){#}/',
 '/(?(1)x/'    => 'Switch (?(condition)... not terminated {#} m/(?(1)x{#}/',
 '/(?(1)x|y/'    => 'Switch (?(condition)... not terminated {#} m/(?(1)x|y{#}/',
 '/(?(1)x|y|z)/' => 'Switch (?(condition)... contains too many branches {#} m/(?(1)x|y|{#}z)/',

 '/(?(x)y|x)/' => 'Unknown switch condition (?(...)) {#} m/(?(x{#})y|x)/',
 '/(?(??{}))/' => 'Unknown switch condition (?(...)) {#} m/(?(?{#}?{}))/',
 '/(?(?[]))/' => 'Unknown switch condition (?(...)) {#} m/(?(?{#}[]))/',

 '/(?/' => 'Sequence (? incomplete {#} m/(?{#}/',

 '/(?;x/' => 'Sequence (?;...) not recognized {#} m/(?;{#}x/',
 '/(?<;x/' => 'Group name must start with a non-digit word character {#} m/(?<;{#}x/',
 '/(?\ix/' => 'Sequence (?\...) not recognized {#} m/(?\{#}ix/',
 '/(?\mx/' => 'Sequence (?\...) not recognized {#} m/(?\{#}mx/',
 '/(?\:x/' => 'Sequence (?\...) not recognized {#} m/(?\{#}:x/',
 '/(?\=x/' => 'Sequence (?\...) not recognized {#} m/(?\{#}=x/',
 '/(?\!x/' => 'Sequence (?\...) not recognized {#} m/(?\{#}!x/',
 '/(?\<=x/' => 'Sequence (?\...) not recognized {#} m/(?\{#}<=x/',
 '/(?\<!x/' => 'Sequence (?\...) not recognized {#} m/(?\{#}<!x/',
 '/(?\>x/' => 'Sequence (?\...) not recognized {#} m/(?\{#}>x/',
 '/(?^-i:foo)/' => 'Sequence (?^-...) not recognized {#} m/(?^-{#}i:foo)/',
 '/(?^-i)foo/' => 'Sequence (?^-...) not recognized {#} m/(?^-{#}i)foo/',
 '/(?^d:foo)/' => 'Sequence (?^d...) not recognized {#} m/(?^d{#}:foo)/',
 '/(?^d)foo/' => 'Sequence (?^d...) not recognized {#} m/(?^d{#})foo/',
 '/(?^lu:foo)/' => 'Regexp modifiers "l" and "u" are mutually exclusive {#} m/(?^lu{#}:foo)/',
 '/(?^lu)foo/' => 'Regexp modifiers "l" and "u" are mutually exclusive {#} m/(?^lu{#})foo/',
'/(?da:foo)/' => 'Regexp modifiers "d" and "a" are mutually exclusive {#} m/(?da{#}:foo)/',
'/(?lil:foo)/' => 'Regexp modifier "l" may not appear twice {#} m/(?lil{#}:foo)/',
'/(?aaia:foo)/' => 'Regexp modifier "a" may appear a maximum of twice {#} m/(?aaia{#}:foo)/',
'/(?i-l:foo)/' => 'Regexp modifier "l" may not appear after the "-" {#} m/(?i-l{#}:foo)/',

 '/((x)/' => 'Unmatched ( {#} m/({#}(x)/',
 '/{(}/' => 'Unmatched ( {#} m/{({#}}/',    # [perl #127599]

 "/x{$inf_p1}/" => "Quantifier in {,} bigger than $inf_m1 {#} m/x{{#}$inf_p1}/",


 '/x**/' => 'Nested quantifiers {#} m/x**{#}/',

 '/x[/' => 'Unmatched [ {#} m/x[{#}/',

 '/*/', => 'Quantifier follows nothing {#} m/*{#}/',

 '/\p{x/' => 'Missing right brace on \p{} {#} m/\p{{#}x/',

 '/[\p{x]/' => 'Missing right brace on \p{} {#} m/[\p{{#}x]/',

 '/(x)\2/' => 'Reference to nonexistent group {#} m/(x)\2{#}/',

 '/\g/' => 'Unterminated \g... pattern {#} m/\g{#}/',
 '/\g{1/' => 'Unterminated \g{...} pattern {#} m/\g{1{#}/',

 'my $m = "\\\"; $m =~ $m', => 'Trailing \ in regex m/\/',

 '/\x{1/' => 'Missing right brace on \x{} {#} m/\x{1{#}/',
 '/\x{X/' => 'Missing right brace on \x{} {#} m/\x{{#}X/',

 '/[\x{X]/' => 'Missing right brace on \x{} {#} m/[\x{{#}X]/',
 '/[\x{A]/' => 'Missing right brace on \x{} {#} m/[\x{A{#}]/',

 '/\o{1/' => 'Missing right brace on \o{ {#} m/\o{1{#}/',
 '/\o{X/' => 'Missing right brace on \o{ {#} m/\o{{#}X/',

 '/[\o{X]/' => 'Missing right brace on \o{ {#} m/[\o{{#}X]/',
 '/[\o{7]/' => 'Missing right brace on \o{ {#} m/[\o{7{#}]/',

 '/[[:barf:]]/' => 'POSIX class [:barf:] unknown {#} m/[[:barf:]{#}]/',

 '/[[=barf=]]/' => 'POSIX syntax [= =] is reserved for future extensions {#} m/[[=barf=]{#}]/',

 '/[[.barf.]]/' => 'POSIX syntax [. .] is reserved for future extensions {#} m/[[.barf.]{#}]/',

 '/[z-a]/' => 'Invalid [] range "z-a" {#} m/[z-a{#}]/',

 '/\p/' => 'Empty \p {#} m/\p{#}/',
 '/\P/' => 'Empty \P {#} m/\P{#}/',
 '/\p{}/' => 'Empty \p{} {#} m/\p{{#}}/',
 '/\P{}/' => 'Empty \P{} {#} m/\P{{#}}/',

'/a\b{cde/' => 'Missing right brace on \b{} {#} m/a\b{{#}cde/',
'/a\B{cde/' => 'Missing right brace on \B{} {#} m/a\B{{#}cde/',

 '/\b{}/' => 'Empty \b{} {#} m/\b{}{#}/',
 '/\B{}/' => 'Empty \B{} {#} m/\B{}{#}/',

 '/\b{gc}/' => "'gc' is an unknown bound type {#} m/\\b{gc{#}}/",
 '/\B{gc}/' => "'gc' is an unknown bound type {#} m/\\B{gc{#}}/",


 '/(?[[[::]]])/' => "Unexpected ']' with no following ')' in (?[... {#} m/(?[[[::]]{#}])/",
 '/(?[[[:w:]]])/' => "Unexpected ']' with no following ')' in (?[... {#} m/(?[[[:w:]]{#}])/",
 '/(?[[:w:]])/' => "",
 '/[][[:alpha:]]' => "",    # [perl #127581]
 '/([.].*)[.]/'   => "",    # [perl #127582]
 '/[.].*[.]/'     => "",    # [perl #127604]
 '/(?[a])/' =>  'Unexpected character {#} m/(?[a{#}])/',
 '/(?[ + \t ])/' => 'Unexpected binary operator \'+\' with no preceding operand {#} m/(?[ +{#} \t ])/',
 '/(?[ \cK - ( + \t ) ])/' => 'Unexpected binary operator \'+\' with no preceding operand {#} m/(?[ \cK - ( +{#} \t ) ])/',
 '/(?[ \cK ( \t ) ])/' => 'Unexpected \'(\' with no preceding operator {#} m/(?[ \cK ({#} \t ) ])/',
 '/(?[ \cK \t ])/' => 'Operand with no preceding operator {#} m/(?[ \cK \t{#} ])/',
 '/(?[ \0004 ])/' => 'Need exactly 3 octal digits {#} m/(?[ \0004 {#}])/',
 '/(?[ \05 ])/' => 'Need exactly 3 octal digits {#} m/(?[ \05 {#}])/',
 '/(?[ \o{1038} ])/' => 'Non-octal character {#} m/(?[ \o{1038{#}} ])/',
 '/(?[ \o{} ])/' => 'Number with no digits {#} m/(?[ \o{}{#} ])/',
 '/(?[ \x{defg} ])/' => 'Non-hex character {#} m/(?[ \x{defg{#}} ])/',
 '/(?[ \xabcdef ])/' => 'Use \\x{...} for more than two hex characters {#} m/(?[ \xabc{#}def ])/',
 '/(?[ \x{} ])/' => 'Number with no digits {#} m/(?[ \x{}{#} ])/',
 '/(?[ \cK + ) ])/' => 'Unexpected \')\' {#} m/(?[ \cK + ){#} ])/',
 '/(?[ \cK + ])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[ \cK + {#}])/',
 '/(?[ ( ) ])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[ ( ){#} ])/',
 '/(?[[0]+()+])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[[0]+(){#}+])/',
 '/(?[ \p{foo} ])/' => 'Can\'t find Unicode property definition "foo" {#} m/(?[ \p{foo}{#} ])/',
 '/(?[ \p{ foo = bar } ])/' => 'Can\'t find Unicode property definition "foo = bar" {#} m/(?[ \p{ foo = bar }{#} ])/',
 '/(?[ \8 ])/' => 'Unrecognized escape \8 in character class {#} m/(?[ \8{#} ])/',
 '/(?[ \t ]/' => "Unexpected ']' with no following ')' in (?[... {#} m/(?[ \\t ]{#}/",
 '/(?[ [ \t ]/' => "Syntax error in (?[...]) {#} m/(?[ [ \\t ]{#}/",
 '/(?[ \t ] ]/' => "Unexpected ']' with no following ')' in (?[... {#} m/(?[ \\t ]{#} ]/",
 '/(?[ [ ] ]/' => "Syntax error in (?[...]) {#} m/(?[ [ ] ]{#}/",
 '/(?[ \t + \e # This was supposed to be a comment ])/' =>
    "Syntax error in (?[...]) {#} m/(?[ \\t + \\e # This was supposed to be a comment ]){#}/",
 '/(?[ ])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[ {#}])/',
 'm/(?[[a-\d]])/' => 'False [] range "a-\d" {#} m/(?[[a-\d{#}]])/',
 'm/(?[[\w-x]])/' => 'False [] range "\w-" {#} m/(?[[\w-{#}x]])/',
 'm/(?[[a-\pM]])/' => 'False [] range "a-\pM" {#} m/(?[[a-\pM{#}]])/',
 'm/(?[[\pM-x]])/' => 'False [] range "\pM-" {#} m/(?[[\pM-{#}x]])/',
 'm/(?[[^\N{LATIN CAPITAL LETTER A WITH MACRON AND GRAVE}]])/' => '\N{} in inverted character class or as a range end-point is restricted to one character {#} m/(?[[^\N{U+100.300{#}}]])/',
 'm/(?[ \p{Digit} & (?(?[ \p{Thai} | \p{Lao} ]))])/' => 'Sequence (?(...) not recognized {#} m/(?[ \p{Digit} & (?({#}?[ \p{Thai} | \p{Lao} ]))])/',
 'm/(?[ \p{Digit} & (?:(?[ \p{Thai} | \p{Lao} ]))])/' => 'Expecting \'(?flags:(?[...\' {#} m/(?[ \p{Digit} & (?{#}:(?[ \p{Thai} | \p{Lao} ]))])/',
 'm/\o{/' => 'Missing right brace on \o{ {#} m/\o{{#}/',
 'm/\o/' => 'Missing braces on \o{} {#} m/\o{#}/',
 'm/\o{}/' => 'Number with no digits {#} m/\o{}{#}/',
 'm/[\o{]/' => 'Missing right brace on \o{ {#} m/[\o{{#}]/',
 'm/[\o]/' => 'Missing braces on \o{} {#} m/[\o{#}]/',
 'm/[\o{}]/' => 'Number with no digits {#} m/[\o{}{#}]/',
 'm/(?^-i:foo)/' => 'Sequence (?^-...) not recognized {#} m/(?^-{#}i:foo)/',
 'm/\87/' => 'Reference to nonexistent group {#} m/\87{#}/',
 'm/a\87/' => 'Reference to nonexistent group {#} m/a\87{#}/',
 'm/a\97/' => 'Reference to nonexistent group {#} m/a\97{#}/',
 'm/(*DOOF)/' => 'Unknown verb pattern \'DOOF\' {#} m/(*DOOF){#}/',
 'm/(?&a/'  => 'Sequence (?&... not terminated {#} m/(?&a{#}/',
 'm/(?P=/' => 'Sequence ?P=... not terminated {#} m/(?P={#}/',
 "m/(?'/"  => "Sequence (?'... not terminated {#} m/(?'{#}/",
 "m/(?</"  => "Sequence (?<... not terminated {#} m/(?<{#}/",
 'm/(?&/'  => 'Sequence (?&... not terminated {#} m/(?&{#}/',
 'm/(?(</' => 'Sequence (?(<... not terminated {#} m/(?(<{#}/',
 "m/(?('/" => "Sequence (?('... not terminated {#} m/(?('{#}/",
 'm/\g{/'  => 'Sequence \g{... not terminated {#} m/\g{{#}/',
 'm/\k</'  => 'Sequence \k<... not terminated {#} m/\k<{#}/',
 'm/\cß/' => "Character following \"\\c\" must be printable ASCII",
 '/((?# This is a comment in the middle of a token)?:foo)/' => 'In \'(?...)\', the \'(\' and \'?\' must be adjacent {#} m/((?# This is a comment in the middle of a token)?{#}:foo)/',
 '/((?# This is a comment in the middle of a token)*FAIL)/' => 'In \'(*VERB...)\', the \'(\' and \'*\' must be adjacent {#} m/((?# This is a comment in the middle of a token)*{#}FAIL)/',
 '/(?[\ &!])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[\ &!{#}])/',    # [perl #126180]
 '/(?[\ +!])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[\ +!{#}])/',    # [perl #126180]
 '/(?[\ -!])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[\ -!{#}])/',    # [perl #126180]
 '/(?[\ ^!])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[\ ^!{#}])/',    # [perl #126180]
 '/(?[\ |!])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[\ |!{#}])/',    # [perl #126180]
 '/(?[()-!])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[(){#}-!])/',    # [perl #126204]
 '/(?[!()])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[!(){#}])/',      # [perl #126404]
 '/(?<=/' => 'Sequence (?... not terminated {#} m/(?<={#}/',                        # [perl #128170]
);

# These are messages that are warnings when not strict; death under 'use re
# "strict".  See comment before @warnings as to why some have a \x{100} in
# them.  This array has 3 elements per construct.  [0] is the regex to use;
# [1] is the message under no strict, and [2] is under strict.
my @death_only_under_strict = (
    'm/\xABC/' => "",
               => 'Use \x{...} for more than two hex characters {#} m/\xABC{#}/',
    'm/[\xABC]/' => "",
                 => 'Use \x{...} for more than two hex characters {#} m/[\xABC{#}]/',

    # XXX This is a confusing error message.  The G isn't ignored; it just
    # terminates the \x.  Also some messages below are missing the <-- HERE,
    # aren't all category 'regexp'.  (Hence we have to turn off 'digit'
    # messages as well below)
    'm/\xAG/' => 'Illegal hexadecimal digit \'G\' ignored',
              => 'Non-hex character {#} m/\xAG{#}/',
    'm/[\xAG]/' => 'Illegal hexadecimal digit \'G\' ignored',
                => 'Non-hex character {#} m/[\xAG{#}]/',
    'm/\o{789}/' => 'Non-octal character \'8\'.  Resolved as "\o{7}"',
                 => 'Non-octal character {#} m/\o{78{#}9}/',
    'm/[\o{789}]/' => 'Non-octal character \'8\'.  Resolved as "\o{7}"',
                   => 'Non-octal character {#} m/[\o{78{#}9}]/',
    'm/\x{}/' => "",
              => 'Number with no digits {#} m/\x{}{#}/',
    'm/[\x{}]/' => "",
                => 'Number with no digits {#} m/[\x{}{#}]/',
    'm/\x{ABCDEFG}/' => 'Illegal hexadecimal digit \'G\' ignored',
                     => 'Non-hex character {#} m/\x{ABCDEFG{#}}/',
    'm/[\x{ABCDEFG}]/' => 'Illegal hexadecimal digit \'G\' ignored',
                       => 'Non-hex character {#} m/[\x{ABCDEFG{#}}]/',
    "m'[\\y]\\x{100}'" => 'Unrecognized escape \y in character class passed through {#} m/[\y{#}]\x{100}/',
                       => 'Unrecognized escape \y in character class {#} m/[\y{#}]\x{100}/',
    'm/[a-\d]\x{100}/' => 'False [] range "a-\d" {#} m/[a-\d{#}]\x{100}/',
                       => 'False [] range "a-\d" {#} m/[a-\d{#}]\x{100}/',
    'm/[\w-x]\x{100}/' => 'False [] range "\w-" {#} m/[\w-{#}x]\x{100}/',
                       => 'False [] range "\w-" {#} m/[\w-{#}x]\x{100}/',
    'm/[a-\pM]\x{100}/' => 'False [] range "a-\pM" {#} m/[a-\pM{#}]\x{100}/',
                        => 'False [] range "a-\pM" {#} m/[a-\pM{#}]\x{100}/',
    'm/[\pM-x]\x{100}/' => 'False [] range "\pM-" {#} m/[\pM-{#}x]\x{100}/',
                        => 'False [] range "\pM-" {#} m/[\pM-{#}x]\x{100}/',
    'm/[^\N{LATIN CAPITAL LETTER A WITH MACRON AND GRAVE}]/' => 'Using just the first character returned by \N{} in character class {#} m/[^\N{U+100.300}{#}]/',
                                       => '\N{} in inverted character class or as a range end-point is restricted to one character {#} m/[^\N{U+100.300{#}}]/',
    'm/[\x03-\N{LATIN CAPITAL LETTER A WITH MACRON AND GRAVE}]/' => 'Using just the first character returned by \N{} in character class {#} m/[\x03-\N{U+100.300}{#}]/',
                                            => '\N{} in inverted character class or as a range end-point is restricted to one character {#} m/[\x03-\N{U+100.300{#}}]/',
    'm/[\N{LATIN CAPITAL LETTER A WITH MACRON AND GRAVE}-\x{10FFFF}]/' => 'Using just the first character returned by \N{} in character class {#} m/[\N{U+100.300}{#}-\x{10FFFF}]/',
                                                  => '\N{} in inverted character class or as a range end-point is restricted to one character {#} m/[\N{U+100.300{#}}-\x{10FFFF}]/',
    '/[\08]/'   => '\'\08\' resolved to \'\o{0}8\' {#} m/[\08{#}]/',
                => 'Need exactly 3 octal digits {#} m/[\08{#}]/',
    '/[\018]/'  => '\'\018\' resolved to \'\o{1}8\' {#} m/[\018{#}]/',
                => 'Need exactly 3 octal digits {#} m/[\018{#}]/',
    '/[\_\0]/'  => "",
                => 'Need exactly 3 octal digits {#} m/[\_\0]{#}/',
    '/[\07]/'   => "",
                => 'Need exactly 3 octal digits {#} m/[\07]{#}/',
    '/[\0005]/' => "",
                => 'Need exactly 3 octal digits {#} m/[\0005]{#}/',
    '/[\8\9]\x{100}/' => ['Unrecognized escape \8 in character class passed through {#} m/[\8{#}\9]\x{100}/',
                          'Unrecognized escape \9 in character class passed through {#} m/[\8\9{#}]\x{100}/',
                         ],
                      => 'Unrecognized escape \8 in character class {#} m/[\8{#}\9]\x{100}/',
    '/[a-\d]\x{100}/' => 'False [] range "a-\d" {#} m/[a-\d{#}]\x{100}/',
                      => 'False [] range "a-\d" {#} m/[a-\d{#}]\x{100}/',
    '/[\d-b]\x{100}/' => 'False [] range "\d-" {#} m/[\d-{#}b]\x{100}/',
                      => 'False [] range "\d-" {#} m/[\d-{#}b]\x{100}/',
    '/[\s-\d]\x{100}/' => 'False [] range "\s-" {#} m/[\s-{#}\d]\x{100}/',
                       => 'False [] range "\s-" {#} m/[\s-{#}\d]\x{100}/',
    '/[\d-\s]\x{100}/' => 'False [] range "\d-" {#} m/[\d-{#}\s]\x{100}/',
                       => 'False [] range "\d-" {#} m/[\d-{#}\s]\x{100}/',
    '/[a-[:digit:]]\x{100}/' => 'False [] range "a-[:digit:]" {#} m/[a-[:digit:]{#}]\x{100}/',
                             => 'False [] range "a-[:digit:]" {#} m/[a-[:digit:]{#}]\x{100}/',
    '/[[:digit:]-b]\x{100}/' => 'False [] range "[:digit:]-" {#} m/[[:digit:]-{#}b]\x{100}/',
                             => 'False [] range "[:digit:]-" {#} m/[[:digit:]-{#}b]\x{100}/',
    '/[[:alpha:]-[:digit:]]\x{100}/' => 'False [] range "[:alpha:]-" {#} m/[[:alpha:]-{#}[:digit:]]\x{100}/',
                                     => 'False [] range "[:alpha:]-" {#} m/[[:alpha:]-{#}[:digit:]]\x{100}/',
    '/[[:digit:]-[:alpha:]]\x{100}/' => 'False [] range "[:digit:]-" {#} m/[[:digit:]-{#}[:alpha:]]\x{100}/',
                                     => 'False [] range "[:digit:]-" {#} m/[[:digit:]-{#}[:alpha:]]\x{100}/',
    '/[a\zb]\x{100}/' => 'Unrecognized escape \z in character class passed through {#} m/[a\z{#}b]\x{100}/',
                      => 'Unrecognized escape \z in character class {#} m/[a\z{#}b]\x{100}/',
);

# These need the character 'ネ' as a marker for mark_as_utf8()
my @death_utf8 = mark_as_utf8(
 '/ネ(?<= .*)/' =>  'Variable length lookbehind not implemented in regex m/ネ(?<= .*)/',

 '/(?<= ネ{1000})/' => 'Lookbehind longer than 255 not implemented in regex m/(?<= ネ{1000})/',

 '/ネ(?ネ)ネ/' => 'Sequence (?ネ...) not recognized {#} m/ネ(?ネ{#})ネ/',

 '/ネ(?(1ネ))ネ/' => 'Switch condition not recognized {#} m/ネ(?(1ネ{#}))ネ/',

 '/(?(1)ネ|y|ヌ)/' => 'Switch (?(condition)... contains too many branches {#} m/(?(1)ネ|y|{#}ヌ)/',

 '/(?(ネ)y|ネ)/' => 'Unknown switch condition (?(...)) {#} m/(?(ネ{#})y|ネ)/',

 '/ネ(?/' => 'Sequence (? incomplete {#} m/ネ(?{#}/',

 '/ネ(?;ネ/' => 'Sequence (?;...) not recognized {#} m/ネ(?;{#}ネ/',
 '/ネ(?<;ネ/' => 'Group name must start with a non-digit word character {#} m/ネ(?<;{#}ネ/',
 '/ネ(?\ixネ/' => 'Sequence (?\...) not recognized {#} m/ネ(?\{#}ixネ/',
 '/ネ(?^lu:ネ)/' => 'Regexp modifiers "l" and "u" are mutually exclusive {#} m/ネ(?^lu{#}:ネ)/',
'/ネ(?lil:ネ)/' => 'Regexp modifier "l" may not appear twice {#} m/ネ(?lil{#}:ネ)/',
'/ネ(?aaia:ネ)/' => 'Regexp modifier "a" may appear a maximum of twice {#} m/ネ(?aaia{#}:ネ)/',
'/ネ(?i-l:ネ)/' => 'Regexp modifier "l" may not appear after the "-" {#} m/ネ(?i-l{#}:ネ)/',

 '/ネ((ネ)/' => 'Unmatched ( {#} m/ネ({#}(ネ)/',

 "/ネ{$inf_p1}ネ/" => "Quantifier in {,} bigger than $inf_m1 {#} m/ネ{{#}$inf_p1}ネ/",


 '/ネ**ネ/' => 'Nested quantifiers {#} m/ネ**{#}ネ/',

 '/ネ[ネ/' => 'Unmatched [ {#} m/ネ[{#}ネ/',

 '/*ネ/', => 'Quantifier follows nothing {#} m/*{#}ネ/',

 '/ネ\p{ネ/' => 'Missing right brace on \p{} {#} m/ネ\p{{#}ネ/',

 '/(ネ)\2ネ/' => 'Reference to nonexistent group {#} m/(ネ)\2{#}ネ/',

 '/\g{ネ/; #no latin1' => 'Sequence \g{... not terminated {#} m/\g{ネ{#}/',

 'my $m = "ネ\\\"; $m =~ $m', => 'Trailing \ in regex m/ネ\/',

 '/\x{ネ/' => 'Missing right brace on \x{} {#} m/\x{{#}ネ/',
 '/ネ[\x{ネ]ネ/' => 'Missing right brace on \x{} {#} m/ネ[\x{{#}ネ]ネ/',
 '/ネ[\x{ネ]/' => 'Missing right brace on \x{} {#} m/ネ[\x{{#}ネ]/',

 '/ネ\o{ネ/' => 'Missing right brace on \o{ {#} m/ネ\o{{#}ネ/',
 '/ネ[[:ネ:]]ネ/' => "",

 '/[ネ-a]ネ/' => 'Invalid [] range "ネ-a" {#} m/[ネ-a{#}]ネ/',

 '/ネ\p{}ネ/' => 'Empty \p{} {#} m/ネ\p{{#}}ネ/',

 '/ネ(?[[[:ネ]]])ネ/' => "Unexpected ']' with no following ')' in (?[... {#} m/ネ(?[[[:ネ]]{#}])ネ/",
 '/ネ(?[[[:ネ: ])ネ/' => "Syntax error in (?[...]) {#} m/ネ(?[[[:ネ: ])ネ{#}/",
 '/ネ(?[[[::]]])ネ/' => "Unexpected ']' with no following ')' in (?[... {#} m/ネ(?[[[::]]{#}])ネ/",
 '/ネ(?[[[:ネ:]]])ネ/' => "Unexpected ']' with no following ')' in (?[... {#} m/ネ(?[[[:ネ:]]{#}])ネ/",
 '/ネ(?[[:ネ:]])ネ/' => "",
 '/ネ(?[ネ])ネ/' =>  'Unexpected character {#} m/ネ(?[ネ{#}])ネ/',
 '/ネ(?[ + [ネ] ])/' => 'Unexpected binary operator \'+\' with no preceding operand {#} m/ネ(?[ +{#} [ネ] ])/',
 '/ネ(?[ \cK - ( + [ネ] ) ])/' => 'Unexpected binary operator \'+\' with no preceding operand {#} m/ネ(?[ \cK - ( +{#} [ネ] ) ])/',
 '/ネ(?[ \cK ( [ネ] ) ])/' => 'Unexpected \'(\' with no preceding operator {#} m/ネ(?[ \cK ({#} [ネ] ) ])/',
 '/ネ(?[ \cK [ネ] ])ネ/' => 'Operand with no preceding operator {#} m/ネ(?[ \cK [ネ{#}] ])ネ/',
 '/ネ(?[ \0004 ])ネ/' => 'Need exactly 3 octal digits {#} m/ネ(?[ \0004 {#}])ネ/',
 '/(?[ \o{ネ} ])ネ/' => 'Non-octal character {#} m/(?[ \o{ネ{#}} ])ネ/',
 '/ネ(?[ \o{} ])ネ/' => 'Number with no digits {#} m/ネ(?[ \o{}{#} ])ネ/',
 '/(?[ \x{ネ} ])ネ/' => 'Non-hex character {#} m/(?[ \x{ネ{#}} ])ネ/',
 '/(?[ \p{ネ} ])/' => 'Can\'t find Unicode property definition "ネ" {#} m/(?[ \p{ネ}{#} ])/',
 '/(?[ \p{ ネ = bar } ])/' => 'Can\'t find Unicode property definition "ネ = bar" {#} m/(?[ \p{ ネ = bar }{#} ])/',
 '/ネ(?[ \t ]/' => "Unexpected ']' with no following ')' in (?[... {#} m/ネ(?[ \\t ]{#}/",
 '/(?[ \t + \e # ネ This was supposed to be a comment ])/' =>
    "Syntax error in (?[...]) {#} m/(?[ \\t + \\e # ネ This was supposed to be a comment ]){#}/",
 'm/(*ネ)ネ/' => q<Unknown verb pattern 'ネ' {#} m/(*ネ){#}ネ/>,
 '/\cネ/' => "Character following \"\\c\" must be printable ASCII",
 '/\b{ネ}/' => "'ネ' is an unknown bound type {#} m/\\b{ネ{#}}/",
 '/\B{ネ}/' => "'ネ' is an unknown bound type {#} m/\\B{ネ{#}}/",
);
push @death, @death_utf8;

my @death_utf8_only_under_strict = (
    "m'ネ[\\y]ネ'" => 'Unrecognized escape \y in character class passed through {#} m/ネ[\y{#}]ネ/',
                   => 'Unrecognized escape \y in character class {#} m/ネ[\y{#}]ネ/',
    'm/ネ[ネ-\d]ネ/' => 'False [] range "ネ-\d" {#} m/ネ[ネ-\d{#}]ネ/',
                     => 'False [] range "ネ-\d" {#} m/ネ[ネ-\d{#}]ネ/',
    'm/ネ[\w-ネ]ネ/' => 'False [] range "\w-" {#} m/ネ[\w-{#}ネ]ネ/',
                     => 'False [] range "\w-" {#} m/ネ[\w-{#}ネ]ネ/',
    'm/ネ[ネ-\pM]ネ/' => 'False [] range "ネ-\pM" {#} m/ネ[ネ-\pM{#}]ネ/',
                      => 'False [] range "ネ-\pM" {#} m/ネ[ネ-\pM{#}]ネ/',
    '/ネ[ネ-[:digit:]]ネ/' => 'False [] range "ネ-[:digit:]" {#} m/ネ[ネ-[:digit:]{#}]ネ/',
                           => 'False [] range "ネ-[:digit:]" {#} m/ネ[ネ-[:digit:]{#}]ネ/',
    '/ネ[\d-\s]ネ/' => 'False [] range "\d-" {#} m/ネ[\d-{#}\s]ネ/',
                    => 'False [] range "\d-" {#} m/ネ[\d-{#}\s]ネ/',
    '/ネ[a\zb]ネ/' => 'Unrecognized escape \z in character class passed through {#} m/ネ[a\z{#}b]ネ/',
                   => 'Unrecognized escape \z in character class {#} m/ネ[a\z{#}b]ネ/',
);
# Tests involving a user-defined charnames translator are in pat_advanced.t

# In the following arrays of warnings, the value can be an array of things to
# expect.  If the empty string, it means no warning should be raised.


# Key-value pairs of code/error of code that should have non-fatal regexp
# warnings.  Most currently have \x{100} appended to them to force them to be
# upgraded to UTF-8, and the first pass restarted.  Previously this would
# cause some warnings to be output twice.  This tests that that behavior has
# been fixed.

my @warning = (
    'm/\b*\x{100}/' => '\b* matches null string many times {#} m/\b*{#}\x{100}/',
    '/\b{g}/a' => "Using /u for '\\b{g}' instead of /a {#} m/\\b{g}{#}/",
    '/\B{gcb}/a' => "Using /u for '\\B{gcb}' instead of /a {#} m/\\B{gcb}{#}/",
    'm/[:blank:]\x{100}/' => 'POSIX syntax [: :] belongs inside character classes {#} m/[:blank:]{#}\x{100}/',
    'm/[[:cntrl:]][:^ascii:]\x{100}/' =>  'POSIX syntax [: :] belongs inside character classes {#} m/[[:cntrl:]][:^ascii:]{#}\x{100}/',
    'm/[[:ascii]]\x{100}/' => "Assuming NOT a POSIX class since there is no terminating ':' {#} m/[[:ascii{#}]]\\x{100}/",
    'm/(?[[:word]])\x{100}/' => "Assuming NOT a POSIX class since there is no terminating ':' {#} m/(?[[:word{#}]])\\x{100}/",
    "m'\\y\\x{100}'"     => 'Unrecognized escape \y passed through {#} m/\y{#}\x{100}/',
    '/x{3,1}/'   => 'Quantifier {n,m} with n > m can\'t match {#} m/x{3,1}{#}/',
    '/\08/' => '\'\08\' resolved to \'\o{0}8\' {#} m/\08{#}/',
    '/\018/' => '\'\018\' resolved to \'\o{1}8\' {#} m/\018{#}/',
    '/(?=a)*/' => '(?=a)* matches null string many times {#} m/(?=a)*{#}/',
    'my $x = \'\m\'; qr/a$x/' => 'Unrecognized escape \m passed through {#} m/a\m{#}/',
    '/\q/' => 'Unrecognized escape \q passed through {#} m/\q{#}/',

    # These two tests do not include the marker, because regcomp.c no
    # longer knows where it goes by the time this warning is emitted.
    # See [perl #122680] regcomp warning gives wrong position of
    # problem.
    '/(?=a){1,3}\x{100}/' => 'Quantifier unexpected on zero-length expression in regex m/(?=a){1,3}\x{100}/',
    '/(a|b)(?=a){3}\x{100}/' => 'Quantifier unexpected on zero-length expression in regex m/(a|b)(?=a){3}\x{100}/',

    '/\_/' => "",
    '/[\006]/' => "",
    '/[:alpha:]\x{100}/' => 'POSIX syntax [: :] belongs inside character classes {#} m/[:alpha:]{#}\x{100}/',
    '/[:zog:]\x{100}/' => 'POSIX syntax [: :] belongs inside character classes (but this one isn\'t fully valid) {#} m/[:zog:]{#}\x{100}/',
    '/[.zog.]\x{100}/' => 'POSIX syntax [. .] belongs inside character classes (but this one isn\'t implemented) {#} m/[.zog.]{#}\x{100}/',
    '/[a-b]/' => "",
    '/(?c)\x{100}/' => 'Useless (?c) - use /gc modifier {#} m/(?c{#})\x{100}/',
    '/(?-c)\x{100}/' => 'Useless (?-c) - don\'t use /gc modifier {#} m/(?-c{#})\x{100}/',
    '/(?g)\x{100}/' => 'Useless (?g) - use /g modifier {#} m/(?g{#})\x{100}/',
    '/(?-g)\x{100}/' => 'Useless (?-g) - don\'t use /g modifier {#} m/(?-g{#})\x{100}/',
    '/(?o)\x{100}/' => 'Useless (?o) - use /o modifier {#} m/(?o{#})\x{100}/',
    '/(?-o)\x{100}/' => 'Useless (?-o) - don\'t use /o modifier {#} m/(?-o{#})\x{100}/',
    '/(?g-o)\x{100}/' => [ 'Useless (?g) - use /g modifier {#} m/(?g{#}-o)\x{100}/',
                    'Useless (?-o) - don\'t use /o modifier {#} m/(?g-o{#})\x{100}/',
                  ],
    '/(?g-c)\x{100}/' => [ 'Useless (?g) - use /g modifier {#} m/(?g{#}-c)\x{100}/',
                    'Useless (?-c) - don\'t use /gc modifier {#} m/(?g-c{#})\x{100}/',
                  ],
      # (?c) means (?g) error won't be thrown
     '/(?o-cg)\x{100}/' => [ 'Useless (?o) - use /o modifier {#} m/(?o{#}-cg)\x{100}/',
                      'Useless (?-c) - don\'t use /gc modifier {#} m/(?o-c{#}g)\x{100}/',
                    ],
    '/(?ogc)\x{100}/' => [ 'Useless (?o) - use /o modifier {#} m/(?o{#}gc)\x{100}/',
                    'Useless (?g) - use /g modifier {#} m/(?og{#}c)\x{100}/',
                    'Useless (?c) - use /gc modifier {#} m/(?ogc{#})\x{100}/',
                  ],
    '/a{1,1}?\x{100}/' => 'Useless use of greediness modifier \'?\' {#} m/a{1,1}?{#}\x{100}/',
    "/(?[ [ % - % ] ])/" => "",
    "/(?[ [ : - \\x$colon_hex ] ])\\x{100}/" => "\": - \\x$colon_hex \" is more clearly written simply as \":\" {#} m/(?[ [ : - \\x$colon_hex {#}] ])\\x{100}/",
    "/(?[ [ \\x$colon_hex - : ] ])\\x{100}/" => "\"\\x$colon_hex\ - : \" is more clearly written simply as \":\" {#} m/(?[ [ \\x$colon_hex - : {#}] ])\\x{100}/",
    "/(?[ [ \\t - \\x$tab_hex ] ])\\x{100}/" => "\"\\t - \\x$tab_hex \" is more clearly written simply as \"\\t\" {#} m/(?[ [ \\t - \\x$tab_hex {#}] ])\\x{100}/",
    "/(?[ [ \\x$tab_hex - \\t ] ])\\x{100}/" => "\"\\x$tab_hex\ - \\t \" is more clearly written simply as \"\\t\" {#} m/(?[ [ \\x$tab_hex - \\t {#}] ])\\x{100}/",
    "/(?[ [ $B_hex - C ] ])/" => "Ranges of ASCII printables should be some subset of \"0-9\", \"A-Z\", or \"a-z\" {#} m/(?[ [ $B_hex - C {#}] ])/",
    "/(?[ [ A - $B_hex ] ])/" => "Ranges of ASCII printables should be some subset of \"0-9\", \"A-Z\", or \"a-z\" {#} m/(?[ [ A - $B_hex {#}] ])/",
    "/(?[ [ $low_mixed_alpha - $high_mixed_alpha ] ])/" => "Ranges of ASCII printables should be some subset of \"0-9\", \"A-Z\", or \"a-z\" {#} m/(?[ [ $low_mixed_alpha - $high_mixed_alpha {#}] ])/",
    "/(?[ [ $low_mixed_digit - $high_mixed_digit ] ])/" => "Ranges of ASCII printables should be some subset of \"0-9\", \"A-Z\", or \"a-z\" {#} m/(?[ [ $low_mixed_digit - $high_mixed_digit {#}] ])/",
    "/[alnum]/" => "",
    "/[^alnum]/" => "",
    '/[:blank]\x{100}/' => 'POSIX syntax [: :] belongs inside character classes (but this one isn\'t fully valid) {#} m/[:blank{#}]\x{100}/',
    '/[[:digit]]\x{100}/' => 'Assuming NOT a POSIX class since there is no terminating \':\' {#} m/[[:digit{#}]]\x{100}/', # [perl # 8904]
    '/[[:digit:foo]\x{100}/' => 'Assuming NOT a POSIX class since there is no terminating \']\' {#} m/[[:digit:{#}foo]\x{100}/',
    '/[[:di#it:foo]\x{100}/x' => 'Assuming NOT a POSIX class since there is no terminating \']\' {#} m/[[:di#it:{#}foo]\x{100}/',
    '/[[:dgit]]\x{100}/' => 'Assuming NOT a POSIX class since there is no terminating \':\' {#} m/[[:dgit{#}]]\x{100}/',
    '/[[:dgit:foo]\x{100}/' => 'Assuming NOT a POSIX class since there is no terminating \']\' {#} m/[[:dgit:{#}foo]\x{100}/',
    '/[[:dgt]]\x{100}/' => "",      # Far enough away from a real class to not be recognized as one
    '/[[:dgt:foo]\x{100}/' => "",
    '/[[:DIGIT]]\x{100}/' => [ 'Assuming NOT a POSIX class since the name must be all lowercase letters {#} m/[[:DIGIT{#}]]\x{100}/',
                               'Assuming NOT a POSIX class since there is no terminating \':\' {#} m/[[:DIGIT{#}]]\x{100}/',
                           ],
    '/[[digit]\x{100}/' => [ 'Assuming NOT a POSIX class since there must be a starting \':\' {#} m/[[{#}digit]\x{100}/',
                             'Assuming NOT a POSIX class since there is no terminating \':\' {#} m/[[digit{#}]\x{100}/',
                           ],
    '/[[alpha]]\x{100}/' => [ 'Assuming NOT a POSIX class since there must be a starting \':\' {#} m/[[{#}alpha]]\x{100}/',
                              'Assuming NOT a POSIX class since there is no terminating \':\' {#} m/[[alpha{#}]]\x{100}/',
                           ],
    '/[[^word]\x{100}/' => [ 'Assuming NOT a POSIX class since the \'^\' must come after the colon {#} m/[[^{#}word]\x{100}/',
                              'Assuming NOT a POSIX class since there must be a starting \':\' {#} m/[[^{#}word]\x{100}/',
                              'Assuming NOT a POSIX class since there is no terminating \':\' {#} m/[[^word{#}]\x{100}/',
                            ],
    '/[[   ^   :   x d i g i t   :   ]   ]\x{100}/' => [ 'Assuming NOT a POSIX class since no blanks are allowed in one {#} m/[[   {#}^   :   x d i g i t   :   ]   ]\x{100}/',
                                               'Assuming NOT a POSIX class since the \'^\' must come after the colon {#} m/[[   ^{#}   :   x d i g i t   :   ]   ]\x{100}/',
                                               'Assuming NOT a POSIX class since no blanks are allowed in one {#} m/[[   ^   {#}:   x d i g i t   :   ]   ]\x{100}/',
                                               'Assuming NOT a POSIX class since no blanks are allowed in one {#} m/[[   ^   :   {#}x d i g i t   :   ]   ]\x{100}/',
                                               'Assuming NOT a POSIX class since no blanks are allowed in one {#} m/[[   ^   :   x d i g i t   :   ]{#}   ]\x{100}/',
                            ],
    '/[foo:lower:]]\x{100}/' => 'Assuming NOT a POSIX class since it doesn\'t start with a \'[\' {#} m/[foo{#}:lower:]]\x{100}/',
    '/[[;upper;]]\x{100}/' => [ 'Assuming NOT a POSIX class since a semi-colon was found instead of a colon {#} m/[[;{#}upper;]]\x{100}/',
                                'Assuming NOT a POSIX class since a semi-colon was found instead of a colon {#} m/[[;upper;]{#}]\x{100}/',
                              ],
    '/[foo;punct;]]\x{100}/' => [ 'Assuming NOT a POSIX class since it doesn\'t start with a \'[\' {#} m/[foo{#};punct;]]\x{100}/',
                                  'Assuming NOT a POSIX class since a semi-colon was found instead of a colon {#} m/[foo;{#}punct;]]\x{100}/',
                                  'Assuming NOT a POSIX class since a semi-colon was found instead of a colon {#} m/[foo;punct;]{#}]\x{100}/',
                                ],

); # See comments before this for why '\x{100}' is generally needed

# These need the character 'ネ' as a marker for mark_as_utf8()
my @warnings_utf8 = mark_as_utf8(
    'm/ネ\b*ネ/' => '\b* matches null string many times {#} m/ネ\b*{#}ネ/',
    '/(?=ネ)*/' => '(?=ネ)* matches null string many times {#} m/(?=ネ)*{#}/',
    'm/ネ[:foo:]ネ/' => 'POSIX syntax [: :] belongs inside character classes (but this one isn\'t fully valid) {#} m/ネ[:foo:]{#}ネ/',
    '/ネ(?c)ネ/' => 'Useless (?c) - use /gc modifier {#} m/ネ(?c{#})ネ/',
    '/utf8 ネ (?ogc) ネ/' => [
        'Useless (?o) - use /o modifier {#} m/utf8 ネ (?o{#}gc) ネ/',
        'Useless (?g) - use /g modifier {#} m/utf8 ネ (?og{#}c) ネ/',
        'Useless (?c) - use /gc modifier {#} m/utf8 ネ (?ogc{#}) ネ/',
    ],

);

push @warning, @warnings_utf8;

my @warning_only_under_strict = (
    '/[\N{U+00}-\x01]\x{100}/' => 'Both or neither range ends should be Unicode {#} m/[\N{U+00}-\x01{#}]\x{100}/',
    '/[\x00-\N{SOH}]\x{100}/' => 'Both or neither range ends should be Unicode {#} m/[\x00-\N{U+01}{#}]\x{100}/',
    '/[\N{DEL}-\o{377}]\x{100}/' => 'Both or neither range ends should be Unicode {#} m/[\N{U+7F}-\o{377}{#}]\x{100}/',
    '/[\o{0}-\N{U+01}]\x{100}/' => 'Both or neither range ends should be Unicode {#} m/[\o{0}-\N{U+01}{#}]\x{100}/',
    '/[\000-\N{U+01}]\x{100}/' => 'Both or neither range ends should be Unicode {#} m/[\000-\N{U+01}{#}]\x{100}/',
    '/[\N{DEL}-\377]\x{100}/' => 'Both or neither range ends should be Unicode {#} m/[\N{U+7F}-\377{#}]\x{100}/',
    '/[\N{U+00}-A]\x{100}/' => 'Ranges of ASCII printables should be some subset of "0-9", "A-Z", or "a-z" {#} m/[\N{U+00}-A{#}]\x{100}/',
    '/[a-\N{U+FF}]\x{100}/' => 'Ranges of ASCII printables should be some subset of "0-9", "A-Z", or "a-z" {#} m/[a-\N{U+FF}{#}]\x{100}/',
    '/[\N{U+00}-\a]\x{100}/' => "",
    '/[\a-\N{U+FF}]\x{100}/' => "",
    '/[\N{U+FF}-\x{100}]/' => 'Both or neither range ends should be Unicode {#} m/[\N{U+FF}-\x{100}{#}]/',
    '/[\N{U+100}-\x{101}]/' => "",
    "/[%-%]/" => "",
    "/[:-\\x$colon_hex]\\x{100}/" => "\":-\\x$colon_hex\" is more clearly written simply as \":\" {#} m/[:-\\x$colon_hex\{#}]\\x{100}/",
    "/[\\x$colon_hex-:]\\x{100}/" => "\"\\x$colon_hex-:\" is more clearly written simply as \":\" {#} m/[\\x$colon_hex\-:{#}]\\x{100}/",
    "/[\\t-\\x$tab_hex]\\x{100}/" => "\"\\t-\\x$tab_hex\" is more clearly written simply as \"\\t\" {#} m/[\\t-\\x$tab_hex\{#}]\\x{100}/",
    "/[\\x$tab_hex-\\t]\\x{100}/" => "\"\\x$tab_hex-\\t\" is more clearly written simply as \"\\t\" {#} m/[\\x$tab_hex\-\\t{#}]\\x{100}/",
    "/[$B_hex-C]/" => "Ranges of ASCII printables should be some subset of \"0-9\", \"A-Z\", or \"a-z\" {#} m/[$B_hex-C{#}]/",
    "/[A-$B_hex]/" => "Ranges of ASCII printables should be some subset of \"0-9\", \"A-Z\", or \"a-z\" {#} m/[A-$B_hex\{#}]/",
    "/[$low_mixed_alpha-$high_mixed_alpha]/" => "Ranges of ASCII printables should be some subset of \"0-9\", \"A-Z\", or \"a-z\" {#} m/[$low_mixed_alpha-$high_mixed_alpha\{#}]/",
    "/[$low_mixed_digit-$high_mixed_digit]/" => "Ranges of ASCII printables should be some subset of \"0-9\", \"A-Z\", or \"a-z\" {#} m/[$low_mixed_digit-$high_mixed_digit\{#}]/",
);

my @warning_utf8_only_under_strict = mark_as_utf8(
 '/ネ[᪉-᪐]/; #no latin1' => "Ranges of digits should be from the same group of 10 {#} m/ネ[᪉-᪐{#}]/",
 '/ネ(?[ [ ᪉ - ᪐ ] ])/; #no latin1' => "Ranges of digits should be from the same group of 10 {#} m/ネ(?[ [ ᪉ - ᪐ {#}] ])/",
 '/ネ[᧙-᧚]/; #no latin1' => "Ranges of digits should be from the same group of 10 {#} m/ネ[᧙-᧚{#}]/",
 '/ネ(?[ [ ᧙ - ᧚ ] ])/; #no latin1' => "Ranges of digits should be from the same group of 10 {#} m/ネ(?[ [ ᧙ - ᧚ {#}] ])/",
);

push @warning_only_under_strict, @warning_utf8_only_under_strict;

my @experimental_regex_sets = (
    '/(?[ \t ])/' => 'The regex_sets feature is experimental {#} m/(?[{#} \t ])/',
    'use utf8; /utf8 ネ (?[ [\tネ] ])/' => do { use utf8; 'The regex_sets feature is experimental {#} m/utf8 ネ (?[{#} [\tネ] ])/' },
    '/noutf8 ネ (?[ [\tネ] ])/' => 'The regex_sets feature is experimental {#} m/noutf8 ネ (?[{#} [\tネ] ])/',
);

my @deprecated = (
    '/\w{/' => 'Unescaped left brace in regex is deprecated, passed through {#} m/\w{{#}/',
    '/\q{/' => [
                 'Unrecognized escape \q{ passed through {#} m/\q{{#}/',
                 'Unescaped left brace in regex is deprecated, passed through {#} m/\q{{#}/'
               ],
    '/:{4,a}/' => 'Unescaped left brace in regex is deprecated, passed through {#} m/:{{#}4,a}/',
    '/abc/xix' => 'Having more than one /x regexp modifier is deprecated',
    '/(?xmsixp:abc)/' => 'Having more than one /x regexp modifier is deprecated',
    '/(?xmsixp)abc/' => 'Having more than one /x regexp modifier is deprecated',
    '/(?xxxx:abc)/' => 'Having more than one /x regexp modifier is deprecated',
);

for my $strict ("", "use re 'strict';") {

    # First time just use @death; but under strict we add the things that fail
    # there.  Doing it this way makes sure that 'strict' doesnt change the
    # things that are already fatal when not under strict.
    if ($strict) {
        for (my $i = 0; $i < @death_only_under_strict; $i += 3) {
            push @death, $death_only_under_strict[$i],    # The regex
                         $death_only_under_strict[$i+2];  # The fatal msg
        }
        for (my $i = 0; $i < @death_utf8_only_under_strict; $i += 3) {

            # Same with the utf8 versions
            push @death, mark_as_utf8($death_utf8_only_under_strict[$i],
                                      $death_utf8_only_under_strict[$i+2]);
        }
    }
    for (my $i = 0; $i < @death; $i += 2) {
        my $regex = $death[$i];
        my $expect = fixup_expect($death[$i+1]);
        no warnings 'experimental::regex_sets';
        no warnings 'experimental::re_strict';

        warning_is(sub {
                    my $eval_string = "$strict $regex";
                    $_ = "x";
                    eval $eval_string;
                    like($@, qr/\Q$expect/, $eval_string);
                }, undef, "... and died without any other warnings");
    }
}

for my $strict ("",  "no warnings 'experimental::re_strict'; use re 'strict';") {
    my @warning_tests = @warning;

    # Build the tests for @warning.  Use the strict/non-strict versions
    # appropriately.
    if ($strict) {
        push @warning_tests, @warning_only_under_strict;
    }
    else {
        for (my $i = 0; $i < @warning_only_under_strict; $i += 2) {
            if ($warning_only_under_strict[$i] =~ /\Q(?[/) {
                push @warning_tests, $warning_only_under_strict[$i],  # The regex
                                    $warning_only_under_strict[$i+1];
            }
            else {
                push @warning_tests, $warning_only_under_strict[$i],  # The regex
                                    "";    # No warning because not strict
            }
        }
        for (my $i = 0; $i < @death_only_under_strict; $i += 3) {
            push @warning_tests, $death_only_under_strict[$i],    # The regex
                                 $death_only_under_strict[$i+1];  # The warning
        }
        for (my $i = 0; $i < @death_utf8_only_under_strict; $i += 3) {
            push @warning_tests, mark_as_utf8($death_utf8_only_under_strict[$i],
                                        $death_utf8_only_under_strict[$i+1]);
        }
    }

    foreach my $ref (\@warning_tests, \@experimental_regex_sets, \@deprecated) {
        my $warning_type;
        my $turn_off_warnings = "";
        my $default_on;
        if ($ref == \@warning_tests) {
            $warning_type = 'regexp, digit';
            $turn_off_warnings = "no warnings 'experimental::regex_sets';";
            $default_on = $strict;
        }
        elsif ($ref == \@deprecated) {
            $warning_type = 'regexp, deprecated';
            $default_on = 1;
        }
        else {
            $warning_type = 'experimental::regex_sets';
            $default_on = 1;
        }
        for (my $i = 0; $i < @$ref; $i += 2) {
            my $regex = $ref->[$i];
            my @expect = fixup_expect($ref->[$i+1]);

            # A length-1 array with an empty warning means no warning gets
            # generated at all.
            undef @expect if @expect == 1 && $expect[0] eq "";

            {
                $_ = "x";
                #use feature 'unicode_eval';
                #print STDERR __LINE__, ": ", "eval '$strict no warnings; $regex'", "\n";
                eval "$strict no warnings; $regex";
            }
            if (is($@, "", "$strict $regex did not die")) {
                my @got = capture_warnings(sub {
                                        $_ = "x";
                                        eval "$strict $turn_off_warnings $regex" });
                my $count = @expect;
                if (! is(scalar @got, scalar @expect,
                            "... and gave expected number ($count) of warnings"))
                {
                    if (@got < @expect) {
                        $count = @got;
                        note "Expected warnings not gotten:\n\t" . join "\n\t",
                                                    @expect[$count .. $#expect];
                    }
                    else {
                        note "Unexpected warnings gotten:\n\t" . join("\n\t",
                                                         @got[$count .. $#got]);
                    }
                }
                foreach my $i (0 .. $count - 1) {
                    if (! like($got[$i], qr/\Q$expect[$i]/,
                                               "... and gave expected warning"))
                    {
                        chomp($got[$i]);
                        chomp($expect[$i]);
                        diag("GOT\n'$got[$i]'\nEXPECT\n'$expect[$i]'");
                    }
                    else {
                        ok (0 == capture_warnings(sub {
                            $_ = "x";
                            eval "$strict no warnings '$warning_type'; $regex;" }
                           ),
                           "... and turning off '$warning_type' warnings suppressed it");

                        # Test that whether the warning is on by default is
                        # correct.  This test relies on the fact that we
                        # are outside the scope of any ‘use warnings’.
                        local $^W;
                        my @warns = capture_warnings(sub { $_ = "x";
                                                        eval "$strict $regex" });
                        # Warning should be on as well if is testing
                        # '(?[...])' which turns on strict
                        if ($default_on || grep { $_ =~ /\Q(?[/ } @expect ) {
                           ok @warns > 0, "... and the warning is on by default";
                        }
                        else {
                         ok @warns == 0, "... and the warning is off by default";
                        }
                    }
                }
            }
        }
    }
}

done_testing();
