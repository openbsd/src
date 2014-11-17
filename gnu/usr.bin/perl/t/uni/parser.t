#!./perl

# Checks if the parser behaves correctly in edge cases
# (including weird syntax errors)

BEGIN {
    require './test.pl';
}

plan (tests => 52);

use utf8;
use open qw( :utf8 :std );

ok *tèst, "*main::tèst", "sanity check.";
ok $::{"tèst"}, "gets the right glob in the stash.";

my $glob_by_sub = sub { *ｍａｉｎ::method }->();

is *ｍａｉｎ::method, "*ｍａｉｎ::method", "glob stringy works";
is "" . *ｍａｉｎ::method, "*ｍａｉｎ::method", "glob stringify-through-concat works";
is $glob_by_sub, "*ｍａｉｎ::method", "glob stringy works";
is "" . $glob_by_sub, "*ｍａｉｎ::method", "";

sub gimme_glob {
    no strict 'refs';
    is *{$_[0]}, "*main::$_[0]";
    *{$_[0]};
}

is "" . gimme_glob("下郎"), "*main::下郎";
$a = *下郎;
is "" . $a, "*main::下郎";

*{gimme_glob("下郎")} = sub {};

{
    ok defined *{"下郎"}{CODE};
    ok !defined *{"\344\270\213\351\203\216"}{CODE};
}

$Lèon = 1;
is ${*Lèon{SCALAR}}, 1, "scalar define in the right glob,";
ok !${*{"L\303\250on"}{SCALAR}}, "..and nothing in the wrong one.";

my $a = "foo" . chr(190);
my $b = $a    . chr(256);
chop $b; # $b is $a with utf8 on

is $a, $b, '$a equals $b';

*$b = sub { 5 };

is eval { main->$a }, 5, q!$a can call $b's sub!;
ok !$@, "..and there's no error.";

my $c = $b;
utf8::encode($c);
ok $b ne $c, '$b unequal $c';
eval { main->$c };
ok $@, q!$c can't call $b's sub.!;

# Now define another sub under the downgraded name:
*$a = sub { 6 };
# Call it:
is eval { main->$a }, 6, "Adding a new sub to *a and calling it works,";
ok !$@, "..without errors.";
eval { main->$c };
ok $@, "but it's still unreachable through *c";

*$b = \10;
is ${*$a{SCALAR}}, 10;
is ${*$b{SCALAR}}, 10;
is ${*$c{SCALAR}}, undef;

opendir FÒÒ, ".";
closedir FÒÒ;
::ok($::{"FÒÒ"}, "Bareword generates the right glob.");
::ok(!$::{"F\303\222\303\222"});

sub участники { 1 }

ok $::{"участники"}, "non-const sub declarations generate the right glob";
ok *{$::{"участники"}}{CODE};
is *{$::{"участники"}}{CODE}->(), 1;

sub 原 () { 1 }

is grep({ $_ eq "\x{539f}"     } keys %::), 1, "Constant subs generate the right glob.";
is grep({ $_ eq "\345\216\237" } keys %::), 0;

#These should probably go elsewhere.
eval q{ sub wròng1 (_$); wròng1(1,2) };
like( $@, qr/Malformed prototype for main::wròng1/, 'Malformed prototype croak is clean.' );

eval q{ sub ча::ики ($__); ча::ики(1,2) };
like( $@, qr/Malformed prototype for ча::ики/ );

our $問 = 10;
is $問, 10, "our works";
is $main::問, 10, "...as does getting the same variable through the fully qualified name";
is ${"main::\345\225\217"}, undef, "..and using the encoded form doesn't";

{
    use charnames qw( :full );

    eval qq! my \$\x{30cb} \N{DROMEDARY CAMEL} !;
    $@ =~ s/eval \d+/eval 11/;
    is $@, 'Unrecognized character \x{1f42a}; marked by <-- HERE after  my $ニ <-- HERE near column 8 at (eval 11) line 1.
', "'Unrecognized character' croak is UTF-8 clean";

    eval "q\0foobar\0 \x{FFFF}+1";
    $@ =~ s/eval \d+/eval 11/;
    is(
        $@,
       "Unrecognized character \\x{ffff}; marked by <-- HERE after q\0foobar\0 <-- HERE near column 11 at (eval 11) line 1.\n",
       "...and nul-clean"
    );

    {
        use re 'eval';
        my $f = qq{(?{\$ネ+ 1; \x{1F42A} })};
        eval { "a" =~ /^a$f/ };
        my $e = $@;
        $e =~ s/eval \d+/eval 11/;
        is(
            $e,
            "Unrecognized character \\x{1f42a}; marked by <-- HERE after (?{\$ネ+ 1; <-- HERE near column 13 at (eval 11) line 1.\n",
            "Messages from a re-eval are UTF-8 clean"
        );

        $f = qq{(?{q\0foobar\0 \x{FFFF}+1 })};
        eval { "a" =~ /^a$f/ };
        my $e = $@;
        $e =~ s/eval \d+/eval 11/;
        is(
            $e,
            "Unrecognized character \\x{ffff}; marked by <-- HERE after q\x{0}foobar\x{0} <-- HERE near column 16 at (eval 11) line 1.\n",
           "...and nul-clean"
        );
    }
    
    {
        eval qq{\$ネ+ 1; \x{1F42A}};
        $@ =~ s/eval \d+/eval 11/;
        is(
            $@,
            "Unrecognized character \\x{1f42a}; marked by <-- HERE after \$ネ+ 1; <-- HERE near column 8 at (eval 11) line 1.\n",
            "Unrecognized character error doesn't cut off in the middle of characters"
        )
    }

}

{
    use feature 'state';
    for ( qw( my state our ) ) {
        local $@;
        eval "$_ Ｆｏｏ $x = 1;";
        like $@, qr/No such class Ｆｏｏ/u, "'No such class' warning for $_ is UTF-8 clean";
    }
}

{
    local $@;
    eval "our \$main::\x{30cb};";
    like $@, qr!No package name allowed for variable \$main::\x{30cb} in "our"!, "'No such package name allowed for variable' is UTF-8 clean";
}

{
    use feature 'state';
    local $@;
    for ( qw( my state ) ) {
        eval "$_ \$::\x{30cb};";
        like $@, qr!"$_" variable \$::\x{30cb} can't be in a package!, qq!'"$_" variable %s can't be in a package' is UTF-8 clean!;
    }
}

{
    local $@;
    eval qq!print \x{30cb}, "comma""!;
    like $@, qr/No comma allowed after filehandle/, "No comma allowed after filehandle triggers correctly for UTF-8 filehandles.";
}

# tests for "Bad name"
eval q{ Ｆｏｏ::$bar };
like( $@, qr/Bad name after Ｆｏｏ::/, 'Bad name after Ｆｏｏ::' );
eval q{ Ｆｏｏ''bar };
like( $@, qr/Bad name after Ｆｏｏ'/, 'Bad name after Ｆｏｏ\'' );

{
    no warnings 'utf8';
    my $malformed_to_be = "\x{c0}\x{a0}";   # Overlong sequence
    CORE::evalbytes "use charnames ':full'; use utf8; my \$x = \"\\N{abc$malformed_to_be}\"";
    like( $@, qr/Malformed UTF-8 character immediately after '\\N\{abc' at .* within string/, 'Malformed UTF-8 input to \N{}');
}
