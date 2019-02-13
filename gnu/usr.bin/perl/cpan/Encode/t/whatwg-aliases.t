# This test checks aliases support based on the list in the
# WHATWG Encoding Living Standard
#
# https://encoding.spec.whatwg.org/
#
# The input of this test is the file whatwg-aliases.json downloaded from
# https://encoding.spec.whatwg.org/encodings.json
#
# To run:
#   AUTHOR_TESTING=1 prove -l t/whatwg-aliases.t


use Test::More
    ($ENV{AUTHOR_TESTING} || $ENV{RELEASE_TESTING})
    ? 'no_plan'
    : (skip_all => 'For maintainers only');
use Encode 'find_encoding';
use JSON::PP 'decode_json';
use File::Spec;
use FindBin;

my $encodings = decode_json(do {
    # https://encoding.spec.whatwg.org/encodings.json
    open my $f, '<', File::Spec->catdir($FindBin::Bin, 'whatwg-aliases.json');
    local $/;
    <$f>
});

my %IGNORE = map { $_ => '' } qw(
    replacement
    utf8
);

my %TODO = (
    'ISO-8859-8-I'   => 'Not supported',
    'gb18030'        => 'Not supported',
    '866'            => 'Not supported',
    'x-user-defined' => 'Not supported',
    # ...
);

for my $section (@$encodings) {
    for my $enc (@{$section->{encodings}}) {

	my $name = $enc->{name};

	next if exists $IGNORE{$name};

	local $TODO = $TODO{$name} if exists $TODO{$name};

	my $encoding = find_encoding($name);
	isa_ok($encoding, 'Encode::Encoding', $name);

	for my $label (@{$enc->{labels}}) {
	    local $TODO = $TODO{$label} if exists $TODO{$label};

	    my $e = find_encoding($label);
	    if (isa_ok($e, 'Encode::Encoding', $label)) {
		next if exists $IGNORE{$label};
		is($e->name, $encoding->name, "$label ->name is $name")
	    }
	}
    }
}

done_testing;
