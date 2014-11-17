#! perl

use Test::More 0.89;

local $SIG{__WARN__} = sub { fail("Got unexpected warning"); diag($_[0]) };

if ($] >= 5.010000) {
	is (eval <<'END', 1, 'lexical topic compiles') or diag $@;
	use experimental 'lexical_topic';
	my $_ = 1;
	is($_, 1, '$_ is 1');
END
}
else {
	fail("No experimental features available on perl $]");
}

if ($] >= 5.010001) {
	is (eval <<'END', 1, 'smartmatch compiles') or diag $@;
	use experimental 'smartmatch';
	sub bar { 1 };
	is(1 ~~ \&bar, 1, "is 1");
END
}

if ($] >= 5.018) {
	is (eval <<'END', 1, 'lexical subs compiles') or diag $@;
	use experimental 'lexical_subs';
	my sub foo { 1 };
	is(foo(), 1, "foo is 1");
	1;
END
}

done_testing;

