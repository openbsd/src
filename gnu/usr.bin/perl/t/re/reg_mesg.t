#!./perl -w

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
}

my $debug = 1;

##
## If the markers used are changed (search for "MARKER1" in regcomp.c),
## update only these two variables, and leave the {#} in the @death/@warning
## arrays below. The {#} is a meta-marker -- it marks where the marker should
## go.

my $marker1 = "<-- HERE";
my $marker2 = " <-- HERE ";

##
## Key-value pairs of code/error of code that should have fatal errors.
##

eval 'use Config';         # assume defaults if fail
our %Config;
my $inf_m1 = ($Config{reg_infty} || 32767) - 1;
my $inf_p1 = $inf_m1 + 2;
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
@warning = (
    'm/\b*/' => '\b* matches null string many times in regex; marked by {#} in m/\b*{#}/',

    'm/[:blank:]/' => 'POSIX syntax [: :] belongs inside character classes in regex; marked by {#} in m/[:blank:]{#}/',

    "m'[\\y]'"     => 'Unrecognized escape \y in character class passed through in regex; marked by {#} in m/[\y{#}]/',

    'm/[a-\d]/' => 'False [] range "a-\d" in regex; marked by {#} in m/[a-\d{#}]/',
    'm/[\w-x]/' => 'False [] range "\w-" in regex; marked by {#} in m/[\w-{#}x]/',
    'm/[a-\pM]/' => 'False [] range "a-\pM" in regex; marked by {#} in m/[a-\pM{#}]/',
    'm/[\pM-x]/' => 'False [] range "\pM-" in regex; marked by {#} in m/[\pM-{#}x]/',
    "m'\\y'"     => 'Unrecognized escape \y passed through in regex; marked by {#} in m/\y{#}/',
);

my $total = (@death + @warning)/2;

# utf8 is a noop on EBCDIC platforms, it is not fatal
my $Is_EBCDIC = (ord('A') == 193);
if ($Is_EBCDIC) {
    my @utf8_death = grep(/utf8/, @death); 
    $total = $total - @utf8_death;
}

print "1..$total\n";

my $count = 0;

while (@death)
{
    my $regex = shift @death;
    my $result = shift @death;
    # skip the utf8 test on EBCDIC since they do not die
    next if ($Is_EBCDIC && $regex =~ /utf8/);
    $count++;

    $_ = "x";
    eval $regex;
    if (not $@) {
	print "# oops, $regex didn't die\nnot ok $count\n";
	next;
    }
    chomp $@;
    $result =~ s/{\#}/$marker1/;
    $result =~ s/{\#}/$marker2/;
    $result .= " at ";
    if ($@ !~ /^\Q$result/) {
	print "# For $regex, expected:\n#  $result\n# Got:\n#  $@\n#\nnot ";
    }
    print "ok $count - $regex\n";
}


our $warning;
$SIG{__WARN__} = sub { $warning = shift };

while (@warning)
{
    $count++;
    my $regex = shift @warning;
    my $result = shift @warning;

    undef $warning;
    $_ = "x";
    eval $regex;

    if ($@)
    {
	print "# oops, $regex died with:\n#\t$@#\nnot ok $count\n";
	next;
    }

    if (not $warning)
    {
	print "# oops, $regex didn't generate a warning\nnot ok $count\n";
	next;
    }
    $result =~ s/{\#}/$marker1/;
    $result =~ s/{\#}/$marker2/;
    $result .= " at ";
    if ($warning !~ /^\Q$result/)
    {
	print <<"EOM";
# For $regex, expected:
#   $result
# Got:
#   $warning
#
not ok $count
EOM
	next;
    }
    print "ok $count - $regex\n";
}



