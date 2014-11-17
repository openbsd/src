#!./perl -w

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	require './test.pl';
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
    return if $expect_ref eq "";

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
## for the utf8 tests: Append 'use utf8' to the pattern, and mark the strings
## to check against as UTF-8
##
## This also creates a second variant of the tests to check if the
## latin1 error messages are working correctly.
my $l1   = "\x{ef}";
my $utf8 = "\x{30cd}";
utf8::encode($utf8);

sub mark_as_utf8 {
    my @ret;
    while ( my ($pat, $msg) = splice(@_, 0, 2) ) {
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

 '/(?(1)x|y|z)/' => 'Switch (?(condition)... contains too many branches {#} m/(?(1)x|y|{#}z)/',

 '/(?(x)y|x)/' => 'Unknown switch condition (?(...)) {#} m/(?(x{#})y|x)/',

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
'/a\b{cde/' => 'Use "\b\{" instead of "\b{" {#} m/a\{#}b{cde/',
'/a\B{cde/' => 'Use "\B\{" instead of "\B{" {#} m/a\{#}B{cde/',

 '/((x)/' => 'Unmatched ( {#} m/({#}(x)/',

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

 '/\p/' => 'Empty \p{} {#} m/\p{#}/',

 '/\P{}/' => 'Empty \P{} {#} m/\P{{#}}/',
 '/(?[[[:word]]])/' => "Unmatched ':' in POSIX class {#} m/(?[[[:word{#}]]])/",
 '/(?[[:word]])/' => "Unmatched ':' in POSIX class {#} m/(?[[:word{#}]])/",
 '/(?[[[:digit: ])/' => "Unmatched '[' in POSIX class {#} m/(?[[[:digit:{#} ])/",
 '/(?[[:digit: ])/' => "Unmatched '[' in POSIX class {#} m/(?[[:digit:{#} ])/",
 '/(?[[[::]]])/' => "POSIX class [::] unknown {#} m/(?[[[::]{#}]])/",
 '/(?[[[:w:]]])/' => "POSIX class [:w:] unknown {#} m/(?[[[:w:]{#}]])/",
 '/(?[[:w:]])/' => "POSIX class [:w:] unknown {#} m/(?[[:w:]{#}])/",
 '/(?[a])/' =>  'Unexpected character {#} m/(?[a{#}])/',
 '/(?[\t])/l' => '(?[...]) not valid in locale {#} m/(?[{#}\t])/',
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
 '/(?[ \p{foo} ])/' => 'Property \'foo\' is unknown {#} m/(?[ \p{foo}{#} ])/',
 '/(?[ \p{ foo = bar } ])/' => 'Property \'foo = bar\' is unknown {#} m/(?[ \p{ foo = bar }{#} ])/',
 '/(?[ \8 ])/' => 'Unrecognized escape \8 in character class {#} m/(?[ \8{#} ])/',
 '/(?[ \t ]/' => 'Syntax error in (?[...]) in regex m/(?[ \t ]/',
 '/(?[ [ \t ]/' => 'Syntax error in (?[...]) in regex m/(?[ [ \t ]/',
 '/(?[ \t ] ]/' => 'Syntax error in (?[...]) in regex m/(?[ \t ] ]/',
 '/(?[ [ ] ]/' => 'Syntax error in (?[...]) in regex m/(?[ [ ] ]/',
 '/(?[ \t + \e # This was supposed to be a comment ])/' => 'Syntax error in (?[...]) in regex m/(?[ \t + \e # This was supposed to be a comment ])/',
 '/(?[ ])/' => 'Incomplete expression within \'(?[ ])\' {#} m/(?[ {#}])/',
 'm/(?[[a-\d]])/' => 'False [] range "a-\d" {#} m/(?[[a-\d{#}]])/',
 'm/(?[[\w-x]])/' => 'False [] range "\w-" {#} m/(?[[\w-{#}x]])/',
 'm/(?[[a-\pM]])/' => 'False [] range "a-\pM" {#} m/(?[[a-\pM{#}]])/',
 'm/(?[[\pM-x]])/' => 'False [] range "\pM-" {#} m/(?[[\pM-{#}x]])/',
 'm/(?[[\N{LATIN CAPITAL LETTER A WITH MACRON AND GRAVE}]])/' => '\N{} in character class restricted to one character {#} m/(?[[\N{U+100.300{#}}]])/',
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
);

my @death_utf8 = mark_as_utf8(
 '/ネ[[=ネ=]]ネ/' => 'POSIX syntax [= =] is reserved for future extensions {#} m/ネ[[=ネ=]{#}]ネ/',
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
 '/ネ[[:ネ:]]ネ/' => 'POSIX class [:ネ:] unknown {#} m/ネ[[:ネ:]{#}]ネ/',

 '/ネ[[=ネ=]]ネ/' => 'POSIX syntax [= =] is reserved for future extensions {#} m/ネ[[=ネ=]{#}]ネ/',

 '/ネ[[.ネ.]]ネ/' => 'POSIX syntax [. .] is reserved for future extensions {#} m/ネ[[.ネ.]{#}]ネ/',

 '/[ネ-a]ネ/' => 'Invalid [] range "ネ-a" {#} m/[ネ-a{#}]ネ/',

 '/ネ\p{}ネ/' => 'Empty \p{} {#} m/ネ\p{{#}}ネ/',

 '/ネ(?[[[:ネ]]])ネ/' => "Unmatched ':' in POSIX class {#} m/ネ(?[[[:ネ{#}]]])ネ/",
 '/ネ(?[[[:ネ: ])ネ/' => "Unmatched '[' in POSIX class {#} m/ネ(?[[[:ネ:{#} ])ネ/",
 '/ネ(?[[[::]]])ネ/' => "POSIX class [::] unknown {#} m/ネ(?[[[::]{#}]])ネ/",
 '/ネ(?[[[:ネ:]]])ネ/' => "POSIX class [:ネ:] unknown {#} m/ネ(?[[[:ネ:]{#}]])ネ/",
 '/ネ(?[[:ネ:]])ネ/' => "POSIX class [:ネ:] unknown {#} m/ネ(?[[:ネ:]{#}])ネ/",
 '/ネ(?[ネ])ネ/' =>  'Unexpected character {#} m/ネ(?[ネ{#}])ネ/',
 '/ネ(?[ネ])/l' => '(?[...]) not valid in locale {#} m/ネ(?[{#}ネ])/',
 '/ネ(?[ + [ネ] ])/' => 'Unexpected binary operator \'+\' with no preceding operand {#} m/ネ(?[ +{#} [ネ] ])/',
 '/ネ(?[ \cK - ( + [ネ] ) ])/' => 'Unexpected binary operator \'+\' with no preceding operand {#} m/ネ(?[ \cK - ( +{#} [ネ] ) ])/',
 '/ネ(?[ \cK ( [ネ] ) ])/' => 'Unexpected \'(\' with no preceding operator {#} m/ネ(?[ \cK ({#} [ネ] ) ])/',
 '/ネ(?[ \cK [ネ] ])ネ/' => 'Operand with no preceding operator {#} m/ネ(?[ \cK [ネ{#}] ])ネ/',
 '/ネ(?[ \0004 ])ネ/' => 'Need exactly 3 octal digits {#} m/ネ(?[ \0004 {#}])ネ/',
 '/(?[ \o{ネ} ])ネ/' => 'Non-octal character {#} m/(?[ \o{ネ{#}} ])ネ/',
 '/ネ(?[ \o{} ])ネ/' => 'Number with no digits {#} m/ネ(?[ \o{}{#} ])ネ/',
 '/(?[ \x{ネ} ])ネ/' => 'Non-hex character {#} m/(?[ \x{ネ{#}} ])ネ/',
 '/(?[ \p{ネ} ])/' => 'Property \'ネ\' is unknown {#} m/(?[ \p{ネ}{#} ])/',
 '/(?[ \p{ ネ = bar } ])/' => 'Property \'ネ = bar\' is unknown {#} m/(?[ \p{ ネ = bar }{#} ])/',
 '/ネ(?[ \t ]/' => 'Syntax error in (?[...]) in regex m/ネ(?[ \t ]/',
 '/(?[ \t + \e # ネ This was supposed to be a comment ])/' => 'Syntax error in (?[...]) in regex m/(?[ \t + \e # ネ This was supposed to be a comment ])/',
 'm/(*ネ)ネ/' => q<Unknown verb pattern 'ネ' {#} m/(*ネ){#}ネ/>,
 '/\cネ/' => "Character following \"\\c\" must be printable ASCII",
);
push @death, @death_utf8;

# Tests involving a user-defined charnames translator are in pat_advanced.t

# In the following arrays of warnings, the value can be an array of things to
# expect.  If the empty string, it means no warning should be raised.

##
## Key-value pairs of code/error of code that should have non-fatal regexp warnings.
##
my @warning = (
    'm/\b*/' => '\b* matches null string many times {#} m/\b*{#}/',
    'm/[:blank:]/' => 'POSIX syntax [: :] belongs inside character classes {#} m/[:blank:]{#}/',

    "m'[\\y]'"     => 'Unrecognized escape \y in character class passed through {#} m/[\y{#}]/',

    'm/[a-\d]/' => 'False [] range "a-\d" {#} m/[a-\d{#}]/',
    'm/[\w-x]/' => 'False [] range "\w-" {#} m/[\w-{#}x]/',
    'm/[a-\pM]/' => 'False [] range "a-\pM" {#} m/[a-\pM{#}]/',
    'm/[\pM-x]/' => 'False [] range "\pM-" {#} m/[\pM-{#}x]/',
    "m'\\y'"     => 'Unrecognized escape \y passed through {#} m/\y{#}/',
    '/x{3,1}/'   => 'Quantifier {n,m} with n > m can\'t match {#} m/x{3,1}{#}/',
    '/\08/' => '\'\08\' resolved to \'\o{0}8\' {#} m/\08{#}/',
    '/\018/' => '\'\018\' resolved to \'\o{1}8\' {#} m/\018{#}/',
    '/[\08]/' => '\'\08\' resolved to \'\o{0}8\' {#} m/[\08{#}]/',
    '/[\018]/' => '\'\018\' resolved to \'\o{1}8\' {#} m/[\018{#}]/',
    '/(?=a)*/' => '(?=a)* matches null string many times {#} m/(?=a)*{#}/',
    'my $x = \'\m\'; qr/a$x/' => 'Unrecognized escape \m passed through {#} m/a\m{#}/',
    '/\q/' => 'Unrecognized escape \q passed through {#} m/\q{#}/',
    '/\q{/' => 'Unrecognized escape \q{ passed through {#} m/\q{{#}/',
    '/(?=a){1,3}/' => 'Quantifier unexpected on zero-length expression {#} m/(?=a){1,3}{#}/',
    '/(a|b)(?=a){3}/' => 'Quantifier unexpected on zero-length expression {#} m/(a|b)(?=a){3}{#}/',
    '/\_/' => "",
    '/[\_\0]/' => "",
    '/[\07]/' => "",
    '/[\006]/' => "",
    '/[\0005]/' => "",
    '/[\8\9]/' => ['Unrecognized escape \8 in character class passed through {#} m/[\8{#}\9]/',
                   'Unrecognized escape \9 in character class passed through {#} m/[\8\9{#}]/',
                  ],
    '/[:alpha:]/' => 'POSIX syntax [: :] belongs inside character classes {#} m/[:alpha:]{#}/',
    '/[:zog:]/' => 'POSIX syntax [: :] belongs inside character classes {#} m/[:zog:]{#}/',
    '/[.zog.]/' => 'POSIX syntax [. .] belongs inside character classes {#} m/[.zog.]{#}/',
    '/[a-b]/' => "",
    '/[a-\d]/' => 'False [] range "a-\d" {#} m/[a-\d{#}]/',
    '/[\d-b]/' => 'False [] range "\d-" {#} m/[\d-{#}b]/',
    '/[\s-\d]/' => 'False [] range "\s-" {#} m/[\s-{#}\d]/',
    '/[\d-\s]/' => 'False [] range "\d-" {#} m/[\d-{#}\s]/',
    '/[a-[:digit:]]/' => 'False [] range "a-[:digit:]" {#} m/[a-[:digit:]{#}]/',
    '/[[:digit:]-b]/' => 'False [] range "[:digit:]-" {#} m/[[:digit:]-{#}b]/',
    '/[[:alpha:]-[:digit:]]/' => 'False [] range "[:alpha:]-" {#} m/[[:alpha:]-{#}[:digit:]]/',
    '/[[:digit:]-[:alpha:]]/' => 'False [] range "[:digit:]-" {#} m/[[:digit:]-{#}[:alpha:]]/',
    '/[a\zb]/' => 'Unrecognized escape \z in character class passed through {#} m/[a\z{#}b]/',
    '/(?c)/' => 'Useless (?c) - use /gc modifier {#} m/(?c{#})/',
    '/(?-c)/' => 'Useless (?-c) - don\'t use /gc modifier {#} m/(?-c{#})/',
    '/(?g)/' => 'Useless (?g) - use /g modifier {#} m/(?g{#})/',
    '/(?-g)/' => 'Useless (?-g) - don\'t use /g modifier {#} m/(?-g{#})/',
    '/(?o)/' => 'Useless (?o) - use /o modifier {#} m/(?o{#})/',
    '/(?-o)/' => 'Useless (?-o) - don\'t use /o modifier {#} m/(?-o{#})/',
    '/(?g-o)/' => [ 'Useless (?g) - use /g modifier {#} m/(?g{#}-o)/',
                    'Useless (?-o) - don\'t use /o modifier {#} m/(?g-o{#})/',
                  ],
    '/(?g-c)/' => [ 'Useless (?g) - use /g modifier {#} m/(?g{#}-c)/',
                    'Useless (?-c) - don\'t use /gc modifier {#} m/(?g-c{#})/',
                  ],
      # (?c) means (?g) error won't be thrown
     '/(?o-cg)/' => [ 'Useless (?o) - use /o modifier {#} m/(?o{#}-cg)/',
                      'Useless (?-c) - don\'t use /gc modifier {#} m/(?o-c{#}g)/',
                    ],
    '/(?ogc)/' => [ 'Useless (?o) - use /o modifier {#} m/(?o{#}gc)/',
                    'Useless (?g) - use /g modifier {#} m/(?og{#}c)/',
                    'Useless (?c) - use /gc modifier {#} m/(?ogc{#})/',
                  ],
    '/a{1,1}?/' => 'Useless use of greediness modifier \'?\' {#} m/a{1,1}?{#}/',
    '/b{3}  +/x' => 'Useless use of greediness modifier \'+\' {#} m/b{3}  +{#}/',
);

my @warnings_utf8 = mark_as_utf8(
    'm/ネ\b*ネ/' => '\b* matches null string many times {#} m/ネ\b*{#}ネ/',
    '/(?=ネ)*/' => '(?=ネ)* matches null string many times {#} m/(?=ネ)*{#}/',
    'm/ネ[:foo:]ネ/' => 'POSIX syntax [: :] belongs inside character classes {#} m/ネ[:foo:]{#}ネ/',
    "m'ネ[\\y]ネ'" => 'Unrecognized escape \y in character class passed through {#} m/ネ[\y{#}]ネ/',
    'm/ネ[ネ-\d]ネ/' => 'False [] range "ネ-\d" {#} m/ネ[ネ-\d{#}]ネ/',
    'm/ネ[\w-ネ]ネ/' => 'False [] range "\w-" {#} m/ネ[\w-{#}ネ]ネ/',
    'm/ネ[ネ-\pM]ネ/' => 'False [] range "ネ-\pM" {#} m/ネ[ネ-\pM{#}]ネ/',
    '/ネ[ネ-[:digit:]]ネ/' => 'False [] range "ネ-[:digit:]" {#} m/ネ[ネ-[:digit:]{#}]ネ/',
    '/ネ[\d-\s]ネ/' => 'False [] range "\d-" {#} m/ネ[\d-{#}\s]ネ/',
    '/ネ[a\zb]ネ/' => 'Unrecognized escape \z in character class passed through {#} m/ネ[a\z{#}b]ネ/',
    '/ネ(?c)ネ/' => 'Useless (?c) - use /gc modifier {#} m/ネ(?c{#})ネ/',    
    '/utf8 ネ (?ogc) ネ/' => [
        'Useless (?o) - use /o modifier {#} m/utf8 ネ (?o{#}gc) ネ/',
        'Useless (?g) - use /g modifier {#} m/utf8 ネ (?og{#}c) ネ/',
        'Useless (?c) - use /gc modifier {#} m/utf8 ネ (?ogc{#}) ネ/',
    ],

);

push @warning, @warnings_utf8;

my @experimental_regex_sets = (
    '/(?[ \t ])/' => 'The regex_sets feature is experimental {#} m/(?[{#} \t ])/',
    'use utf8; /utf8 ネ (?[ [\tネ] ])/' => do { use utf8; 'The regex_sets feature is experimental {#} m/utf8 ネ (?[{#} [\tネ] ])/' },
    '/noutf8 ネ (?[ [\tネ] ])/' => 'The regex_sets feature is experimental {#} m/noutf8 ネ (?[{#} [\tネ] ])/',
);

my @deprecated = (
    "/(?x)latin1\\\x{85}\x{85}\\\x{85}/" => 'Escape literal pattern white space under /x {#} ' . "m/(?x)latin1\\\x{85}\x{85}{#}\\\x{85}/",
    'use utf8; /(?x)utf8\\/' => 'Escape literal pattern white space under /x {#} ' . "m/(?x)utf8\\\N{NEXT LINE}\N{NEXT LINE}{#}\\\N{NEXT LINE}/",
    '/((?# This is a comment in the middle of a token)?:foo)/' => 'In \'(?...)\', splitting the initial \'(?\' is deprecated {#} m/((?# This is a comment in the middle of a token)?{#}:foo)/',
    '/((?# This is a comment in the middle of a token)*FAIL)/' => 'In \'(*VERB...)\', splitting the initial \'(*\' is deprecated {#} m/((?# This is a comment in the middle of a token)*{#}FAIL)/',
);

while (my ($regex, $expect) = splice @death, 0, 2) {
    my $expect = fixup_expect($expect);
    no warnings 'experimental::regex_sets';
    # skip the utf8 test on EBCDIC since they do not die
    next if $::IS_EBCDIC && $regex =~ /utf8/;

    warning_is(sub {
		   $_ = "x";
		   eval $regex;
		   like($@, qr/\Q$expect/, $regex);
	       }, undef, "... and died without any other warnings");
}

foreach my $ref (\@warning, \@experimental_regex_sets, \@deprecated) {
    my $warning_type = ($ref == \@warning)
                       ? 'regexp'
                       : ($ref == \@deprecated)
                         ? 'regexp, deprecated'
                         : 'experimental::regex_sets';
    while (my ($regex, $expect) = splice @$ref, 0, 2) {
        my @expect = fixup_expect($expect);
        {
            $_ = "x";
            no warnings;
            eval $regex;
        }
        if (is($@, "", "$regex did not die")) {
            my @got = capture_warnings(sub {
                                    $_ = "x";
                                    eval $regex });
            my $count = @expect;
            if (! is(scalar @got, scalar @expect, "... and gave expected number ($count) of warnings")) {
                if (@got < @expect) {
                    $count = @got;
                    note "Expected warnings not gotten:\n\t" . join "\n\t", @expect[$count .. $#expect];
                }
                else {
                    note "Unexpected warnings gotten:\n\t" . join("\n\t", @got[$count .. $#got]);
                }
            }
            foreach my $i (0 .. $count - 1) {
                if (! like($got[$i], qr/\Q$expect[$i]/, "... and gave expected warning")) {
                    chomp($got[$i]);
                    chomp($expect[$i]);
                    diag("GOT\n'$got[$i]'\nEXPECT\n'$expect[$i]'");
                }
                else {
                    ok (0 == capture_warnings(sub {
                                    $_ = "x";
                                    eval "no warnings '$warning_type'; $regex;" }
                                ),
                    "... and turning off '$warning_type' warnings suppressed it");
                    # Test that whether the warning is on by default is
                    # correct.  Experimental and deprecated warnings are;
                    # others are not.  This test relies on the fact that we
                    # are outside the scope of any ‘use warnings’.
                    local $^W;
                    my $on = 'on' x ($warning_type ne 'regexp');
                    ok !!$on ==
                        capture_warnings(sub { $_ = "x"; eval $regex }),
                      "... and the warning is " . ($on||'off')
                       . " by default";
                }
            }
        }
    }
}

done_testing();
