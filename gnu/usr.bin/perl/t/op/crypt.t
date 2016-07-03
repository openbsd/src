#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
}

BEGIN {
    use Config;

    require "test.pl";

    if( !$Config{d_crypt} ) {
        skip_all("crypt unimplemented");
    }
    else {
        plan(tests => 4);
    }
}

# Can't assume too much about the string returned by crypt(),
# and about how many bytes of the encrypted (really, hashed)
# string matter.
#
# HISTORICALLY the results started with the first two bytes of the salt,
# followed by 11 bytes from the set [./0-9A-Za-z], and only the first
# eight characters mattered, but those are probably no more safe
# bets, given alternative encryption/hashing schemes like MD5,
# C2 (or higher) security schemes, and non-UNIX platforms.

my $alg = '$2b$12$12345678901234567890';   # Use Blowfish
SKIP: {
	skip ("VOS crypt ignores salt.", 1) if ($^O eq 'vos');
	ok(substr(crypt("ab", $alg . "cd"), 2) ne substr(crypt("ab", $alg . "ce"), 2), "salt makes a difference");
}

$a = "a\xFF\x{100}";

eval {$b = crypt($a, $alg . "cd")};
like($@, qr/Wide character in crypt/, "wide characters ungood");

chop $a; # throw away the wide character

eval {$b = crypt($a, $alg . "cd")};
is($@, '',                   "downgrade to eight bit characters");
is($b, crypt("a\xFF", $alg . "cd"), "downgrade results agree");

