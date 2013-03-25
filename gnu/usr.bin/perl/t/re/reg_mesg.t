#!./perl -w

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	require './test.pl';
	eval 'require Config'; # assume defaults if this fails
}

use strict;

##
## If the markers used are changed (search for "MARKER1" in regcomp.c),
## update only these two regexs, and leave the {#} in the @death/@warning
## arrays below. The {#} is a meta-marker -- it marks where the marker should
## go.
##
sub fixup_expect {
    my $expect = shift;
    $expect =~ s/{\#}/<-- HERE/;
    $expect =~ s/{\#}/ <-- HERE /;
    $expect .= " at ";
    return $expect;
}

my $inf_m1 = ($Config::Config{reg_infty} || 32767) - 1;
my $inf_p1 = $inf_m1 + 2;

##
## Key-value pairs of code/error of code that should have fatal errors.
##
my @death =
(
 '/[[=foo=]]/' => 'POSIX syntax [= =] is reserved for future extensions in regex; marked by {#} in m/[[=foo=]{#}]/',

 '/(?<= .*)/' =>  'Variable length lookbehind not implemented in regex m/(?<= .*)/',

 '/(?<= x{1000})/' => 'Lookbehind longer than 255 not implemented in regex m/(?<= x{1000})/',

 '/(?@)/' => 'Sequence (?@...) not implemented in regex; marked by {#} in m/(?@{#})/',

 '/(?{ 1/' => 'Sequence (?{...}) not terminated or not {}-balanced in regex; marked by {#} in m/(?{{#} 1/',

 '/(?(1x))/' => 'Switch condition not recognized in regex; marked by {#} in m/(?(1x{#}))/',

 '/(?(1)x|y|z)/' => 'Switch (?(condition)... contains too many branches in regex; marked by {#} in m/(?(1)x|y|{#}z)/',

 '/(?(x)y|x)/' => 'Unknown switch condition (?(x) in regex; marked by {#} in m/(?({#}x)y|x)/',

 '/(?/' => 'Sequence (? incomplete in regex; marked by {#} in m/(?{#}/',

 '/(?;x/' => 'Sequence (?;...) not recognized in regex; marked by {#} in m/(?;{#}x/',
 '/(?<;x/' => 'Sequence (?<;...) not recognized in regex; marked by {#} in m/(?<;{#}x/',

 '/(?\ix/' => 'Sequence (?\...) not recognized in regex; marked by {#} in m/(?\{#}ix/',
 '/(?\mx/' => 'Sequence (?\...) not recognized in regex; marked by {#} in m/(?\{#}mx/',
 '/(?\:x/' => 'Sequence (?\...) not recognized in regex; marked by {#} in m/(?\{#}:x/',
 '/(?\=x/' => 'Sequence (?\...) not recognized in regex; marked by {#} in m/(?\{#}=x/',
 '/(?\!x/' => 'Sequence (?\...) not recognized in regex; marked by {#} in m/(?\{#}!x/',
 '/(?\<=x/' => 'Sequence (?\...) not recognized in regex; marked by {#} in m/(?\{#}<=x/',
 '/(?\<!x/' => 'Sequence (?\...) not recognized in regex; marked by {#} in m/(?\{#}<!x/',
 '/(?\>x/' => 'Sequence (?\...) not recognized in regex; marked by {#} in m/(?\{#}>x/',
 '/(?^-i:foo)/' => 'Sequence (?^-...) not recognized in regex; marked by {#} in m/(?^-{#}i:foo)/',
 '/(?^-i)foo/' => 'Sequence (?^-...) not recognized in regex; marked by {#} in m/(?^-{#}i)foo/',
 '/(?^d:foo)/' => 'Sequence (?^d...) not recognized in regex; marked by {#} in m/(?^d{#}:foo)/',
 '/(?^d)foo/' => 'Sequence (?^d...) not recognized in regex; marked by {#} in m/(?^d{#})foo/',
 '/(?^lu:foo)/' => 'Regexp modifiers "l" and "u" are mutually exclusive in regex; marked by {#} in m/(?^lu{#}:foo)/',
 '/(?^lu)foo/' => 'Regexp modifiers "l" and "u" are mutually exclusive in regex; marked by {#} in m/(?^lu{#})foo/',
'/(?da:foo)/' => 'Regexp modifiers "d" and "a" are mutually exclusive in regex; marked by {#} in m/(?da{#}:foo)/',
'/(?lil:foo)/' => 'Regexp modifier "l" may not appear twice in regex; marked by {#} in m/(?lil{#}:foo)/',
'/(?aaia:foo)/' => 'Regexp modifier "a" may appear a maximum of twice in regex; marked by {#} in m/(?aaia{#}:foo)/',
'/(?i-l:foo)/' => 'Regexp modifier "l" may not appear after the "-" in regex; marked by {#} in m/(?i-l{#}:foo)/',

 '/((x)/' => 'Unmatched ( in regex; marked by {#} in m/({#}(x)/',

 "/x{$inf_p1}/" => "Quantifier in {,} bigger than $inf_m1 in regex; marked by {#} in m/x{{#}$inf_p1}/",

 '/x{3,1}/' => 'Can\'t do {n,m} with n > m in regex; marked by {#} in m/x{3,1}{#}/',

 '/x**/' => 'Nested quantifiers in regex; marked by {#} in m/x**{#}/',

 '/x[/' => 'Unmatched [ in regex; marked by {#} in m/x[{#}/',

 '/*/', => 'Quantifier follows nothing in regex; marked by {#} in m/*{#}/',

 '/\p{x/' => 'Missing right brace on \p{} in regex; marked by {#} in m/\p{{#}x/',

 '/[\p{x]/' => 'Missing right brace on \p{} in regex; marked by {#} in m/[\p{{#}x]/',

 '/(x)\2/' => 'Reference to nonexistent group in regex; marked by {#} in m/(x)\2{#}/',

 'my $m = "\\\"; $m =~ $m', => 'Trailing \ in regex m/\/',

 '/\x{1/' => 'Missing right brace on \x{} in regex; marked by {#} in m/\x{{#}1/',

 '/[\x{X]/' => 'Missing right brace on \x{} in regex; marked by {#} in m/[\x{{#}X]/',

 '/[[:barf:]]/' => 'POSIX class [:barf:] unknown in regex; marked by {#} in m/[[:barf:]{#}]/',

 '/[[=barf=]]/' => 'POSIX syntax [= =] is reserved for future extensions in regex; marked by {#} in m/[[=barf=]{#}]/',

 '/[[.barf.]]/' => 'POSIX syntax [. .] is reserved for future extensions in regex; marked by {#} in m/[[.barf.]{#}]/',
  
 '/[z-a]/' => 'Invalid [] range "z-a" in regex; marked by {#} in m/[z-a{#}]/',

 '/\p/' => 'Empty \p{} in regex; marked by {#} in m/\p{#}/',

 '/\P{}/' => 'Empty \P{} in regex; marked by {#} in m/\P{{#}}/',
);

##
## Key-value pairs of code/error of code that should have non-fatal warnings.
##
my @warning = (
    'm/\b*/' => '\b* matches null string many times in regex; marked by {#} in m/\b*{#}/',

    'm/[:blank:]/' => 'POSIX syntax [: :] belongs inside character classes in regex; marked by {#} in m/[:blank:]{#}/',

    "m'[\\y]'"     => 'Unrecognized escape \y in character class passed through in regex; marked by {#} in m/[\y{#}]/',

    'm/[a-\d]/' => 'False [] range "a-\d" in regex; marked by {#} in m/[a-\d{#}]/',
    'm/[\w-x]/' => 'False [] range "\w-" in regex; marked by {#} in m/[\w-{#}x]/',
    'm/[a-\pM]/' => 'False [] range "a-\pM" in regex; marked by {#} in m/[a-\pM{#}]/',
    'm/[\pM-x]/' => 'False [] range "\pM-" in regex; marked by {#} in m/[\pM-{#}x]/',
    "m'\\y'"     => 'Unrecognized escape \y passed through in regex; marked by {#} in m/\y{#}/',
);

while (my ($regex, $expect) = splice @death, 0, 2) {
    my $expect = fixup_expect($expect);
    # skip the utf8 test on EBCDIC since they do not die
    next if $::IS_EBCDIC && $regex =~ /utf8/;

    warning_is(sub {
		   $_ = "x";
		   eval $regex;
		   like($@, qr/\Q$expect/);
	       }, undef, "$regex died without any other warnings");
}

while (my ($regex, $expect) = splice @warning, 0, 2) {
    my $expect = fixup_expect($expect);
    warning_like(sub {
		     $_ = "x";
		     eval $regex;
		     is($@, '', "$regex did not die");
		 }, qr/\Q$expect/);
}

done_testing();
