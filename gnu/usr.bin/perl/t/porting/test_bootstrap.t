#!/perl -w
use strict;

# See "Writing a test" in perlhack.pod for the instructions about the order that
# testing directories run, and which constructions should be avoided in the
# early tests.

# This regression tests ensures that the rules aren't accidentally overlooked.

BEGIN {
    chdir 't';
    require './test.pl';
}

plan('no_plan');

open my $fh, '<', '../MANIFEST' or die "Can't open MANIFEST: $!";

# Three tests in t/comp need to use require or use to get their job done:
my %exceptions = (hints => "require './test.pl'",
		  parser => 'use DieDieDie',
		  proto => 'use strict',
		 );
		  
while (my $file = <$fh>) {
    next unless $file =~ s!^t/!!;
    chomp $file;
    $file =~ s/\s+.*//;
    next unless $file =~ m!\.t$!;

    local $/;
    open my $t, '<', $file or die "Can't open $file: $!";
    # avoid PERL_UNICODE causing us to read non-UTF-8 files as UTF-8
    binmode $t;
    my $contents = <$t>;
    # Make sure that we don't match ourselves
    unlike($contents, qr/use\s+Test::More/, "$file doesn't use Test::\QMore");
    next unless $file =~ m!^base/! or $file =~ m!^comp!;

    # Remove only the excepted constructions for the specific files.
    if ($file =~ m!comp/(.*)\.t! && $exceptions{$1}) {
	my $allowed = $exceptions{$1};
	$contents =~ s/\Q$allowed//gs;
    }

    # All uses of use are allowed in t/comp/use.t
    unlike($contents, qr/^\s*use\s+/m, "$file doesn't use use")
	unless $file eq 'comp/use.t';
    # All uses of require are allowed in t/comp/require.t
    unlike($contents, qr/^\s*require\s+/m, "$file doesn't use require")
	unless $file eq 'comp/require.t'
}

# There are regression tests using test.pl that don't want PL_sawampersand
# set.  Or at least that was the case until PL_sawampersand was disabled
# and replaced with copy-on-write.

# We still allow PL_sawampersand to be enabled with
# -Accflags=-DPERL_SAWAMPERSAND, or with -DPERL_NO_COW, so its still worth
# checking.
# There's no portable, reliable way to check whether PL_sawampersand is
# set, so instead we just "grep $`|$&|$' test.pl"

{
    my $file = '';
    my $fh;
    if (ok(open(my $fh, '<', 'test.pl'), "opened test.pl")) {
	$file = do { local $/; <$fh> };
	$file //= '';
    }
    else {
	diag("error: $!");
    }
    ok(length($file) > 0, "read test.pl successfully");
    ok($file !~ /\$&/, 'Nothing in test.pl mentioned $&');
    ok($file !~ /\$`/, 'Nothing in test.pl mentioned $`');
    ok($file !~ /\$'/, 'Nothing in test.pl mentioned $\'');
}
