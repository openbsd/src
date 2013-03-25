package t::lib::Test;

use strict;
use Exporter   ();
use File::Spec ();
use Test::More ();

use vars qw{@ISA @EXPORT};
BEGIN {
	@ISA    = qw{ Exporter };
	@EXPORT = qw{
		tests  yaml_ok  yaml_error slurp  load_ok
		test_data_directory
	};
}

# Do we have the authorative YAML to test against
eval {
	require YAML;

	# This doesn't currently work, but is documented to.
	# So if it ever turns up, use it.
	$YAML::UseVersion = 1;
};
my $HAVE_YAMLPM = !! (
	$YAML::VERSION
	and
	$YAML::VERSION >= 0.66
);
sub have_yamlpm { $HAVE_YAMLPM }

# Do we have YAML::Perl to test against?
eval {
	require YAML::Perl;
};
my $HAVE_YAMLPERL = !! (
	$YAML::Perl::VERSION
	and
	$YAML::Perl::VERSION >= 0.02
);
sub have_yamlperl { $HAVE_YAMLPERL }

# Do we have YAML::Syck to test against?
eval {
	require YAML::Syck;
};
my $HAVE_SYCK = !! (
	$YAML::Syck::VERSION
	and
	$YAML::Syck::VERSION >= 1.05
);
sub have_syck { $HAVE_SYCK }

# Do we have YAML::XS to test against?
eval {
	require YAML::XS;
};
my $HAVE_XS = !! (
	$YAML::XS::VERSION
	and
	$YAML::XS::VERSION >= 0.29
);
sub have_xs{ $HAVE_XS }

# 22 tests per call to yaml_ok
# 4  tests per call to load_ok
sub tests {
	return ( tests => count(@_) );
}

sub test_data_directory {
	return File::Spec->catdir( 't', 'data' );
}

sub count {
	my $yaml_ok = shift || 0;
	my $load_ok = shift || 0;
	my $single  = shift || 0;
	my $count   = $yaml_ok * 38 + $load_ok * 4 + $single;
	return $count;
}

sub yaml_ok {
	my $string  = shift;
	my $object  = shift;
	my $name    = shift || 'unnamed';
	my %options = ( @_ );
	bless $object, 'CPAN::Meta::YAML';

	# If YAML itself is available, test with it
	SKIP: {
		unless ( $HAVE_YAMLPM ) {
			Test::More::skip( "Skipping YAML.pm, not available for testing", 7 );
		}
		if ( $options{noyamlpm} ) {
			Test::More::skip( "Skipping YAML.pm for known-broken feature", 7 );
		}

		# Test writing with YAML.pm
		my $yamlpm_out = eval { YAML::Dump( @$object ) };
		Test::More::is( $@, '', "$name: YAML.pm saves without error" );
		SKIP: {
			Test::More::skip( "Shortcutting after failure", 4 ) if $@;
			Test::More::ok(
				!!(defined $yamlpm_out and ! ref $yamlpm_out),
				"$name: YAML.pm serializes correctly",
			);
			my @yamlpm_round = eval { YAML::Load( $yamlpm_out ) };
			Test::More::is( $@, '', "$name: YAML.pm round-trips without error" );
			Test::More::skip( "Shortcutting after failure", 2 ) if $@;
			my $round = bless [ @yamlpm_round ], 'CPAN::Meta::YAML';
			Test::More::is_deeply( $round, $object, "$name: YAML.pm round-trips correctly" );               
		}

		# Test reading with YAML.pm
		my $yamlpm_copy = $string;
		my @yamlpm_in   = eval { YAML::Load( $yamlpm_copy ) };
		Test::More::is( $@, '', "$name: YAML.pm loads without error" );
		Test::More::is( $yamlpm_copy, $string, "$name: YAML.pm does not modify the input string" );
		SKIP: {
			Test::More::skip( "Shortcutting after failure", 1 ) if $@;
			Test::More::is_deeply( \@yamlpm_in, $object, "$name: YAML.pm parses correctly" );
		}
	}

	# If YAML::Syck itself is available, test with it
	SKIP: {
		unless ( $HAVE_SYCK ) {
			Test::More::skip( "Skipping YAML::Syck, not available for testing", 7 );
		}
		if ( $options{nosyck} ) {
			Test::More::skip( "Skipping YAML::Syck for known-broken feature", 7 );
		}
		unless ( @$object == 1 ) {
			Test::More::skip( "Skipping YAML::Syck for unsupported feature", 7 );
		}

		# Test writing with YAML::Syck
		my $syck_out = eval { YAML::Syck::Dump( @$object ) };
		Test::More::is( $@, '', "$name: YAML::Syck saves without error" );
		SKIP: {
			Test::More::skip( "Shortcutting after failure", 4 ) if $@;
			Test::More::ok(
				!!(defined $syck_out and ! ref $syck_out),
				"$name: YAML::Syck serializes correctly",
			);
			my @syck_round = eval { YAML::Syck::Load( $syck_out ) };
			Test::More::is( $@, '', "$name: YAML::Syck round-trips without error" );
			Test::More::skip( "Shortcutting after failure", 2 ) if $@;
			my $round = bless [ @syck_round ], 'CPAN::Meta::YAML';
			Test::More::is_deeply( $round, $object, "$name: YAML::Syck round-trips correctly" );            
		}

		# Test reading with YAML::Syck
		my $syck_copy = $string;
		my @syck_in   = eval { YAML::Syck::Load( $syck_copy ) };
		Test::More::is( $@, '', "$name: YAML::Syck loads without error" );
		Test::More::is( $syck_copy, $string, "$name: YAML::Syck does not modify the input string" );
		SKIP: {
			Test::More::skip( "Shortcutting after failure", 1 ) if $@;
			Test::More::is_deeply( \@syck_in, $object, "$name: YAML::Syck parses correctly" );
		}
	}

	# If YAML::XS itself is available, test with it
	SKIP: {
		unless ( $HAVE_XS ) {
			Test::More::skip( "Skipping YAML::XS, not available for testing", 7 );
		}
		if ( $options{noxs} ) {
			Test::More::skip( "Skipping YAML::XS for known-broken feature", 7 );
		}

		# Test writing with YAML::XS
		my $xs_out = eval { YAML::XS::Dump( @$object ) };
		Test::More::is( $@, '', "$name: YAML::XS saves without error" );
		SKIP: {
			Test::More::skip( "Shortcutting after failure", 4 ) if $@;
			Test::More::ok(
				!!(defined $xs_out and ! ref $xs_out),
				"$name: YAML::XS serializes correctly",
			);
			my @xs_round = eval { YAML::XS::Load( $xs_out ) };
			Test::More::is( $@, '', "$name: YAML::XS round-trips without error" );
			Test::More::skip( "Shortcutting after failure", 2 ) if $@;
			my $round = bless [ @xs_round ], 'CPAN::Meta::YAML';
			Test::More::is_deeply( $round, $object, "$name: YAML::XS round-trips correctly" );              
		}

		# Test reading with YAML::XS
		my $xs_copy = $string;
		my @xs_in   = eval { YAML::XS::Load( $xs_copy ) };
		Test::More::is( $@, '', "$name: YAML::XS loads without error" );
		Test::More::is( $xs_copy, $string, "$name: YAML::XS does not modify the input string" );
		SKIP: {
			Test::More::skip( "Shortcutting after failure", 1 ) if $@;
			Test::More::is_deeply( \@xs_in, $object, "$name: YAML::XS parses correctly" );
		}
	}

	# If YAML::Perl is available, test with it
	SKIP: {
		unless ( $HAVE_YAMLPERL ) {
			Test::More::skip( "Skipping YAML::Perl, not available for testing", 7 );
		}
		if ( $options{noyamlperl} ) {
			Test::More::skip( "Skipping YAML::Perl for known-broken feature", 7 );
		}

		# Test writing with YAML.pm
		my $yamlperl_out = eval { YAML::Perl::Dump( @$object ) };
		Test::More::is( $@, '', "$name: YAML::Perl saves without error" );
		SKIP: {
			Test::More::skip( "Shortcutting after failure", 4 ) if $@;
			Test::More::ok(
				!!(defined $yamlperl_out and ! ref $yamlperl_out),
				"$name: YAML::Perl serializes correctly",
			);
			my @yamlperl_round = eval { YAML::Perl::Load( $yamlperl_out ) };
			Test::More::is( $@, '', "$name: YAML::Perl round-trips without error" );
			Test::More::skip( "Shortcutting after failure", 2 ) if $@;
			my $round = bless [ @yamlperl_round ], 'CPAN::Meta::YAML';
			Test::More::is_deeply( $round, $object, "$name: YAML::Perl round-trips correctly" );            
		}

		# Test reading with YAML::Perl
		my $yamlperl_copy = $string;
		my @yamlperl_in   = eval { YAML::Perl::Load( $yamlperl_copy ) };
		Test::More::is( $@, '', "$name: YAML::Perl loads without error" );
		Test::More::is( $yamlperl_copy, $string, "$name: YAML::Perl does not modify the input string" );
		SKIP: {
			Test::More::skip( "Shortcutting after failure", 1 ) if $@;
			Test::More::is_deeply( \@yamlperl_in, $object, "$name: YAML::Perl parses correctly" );
		}
	}

	# Does the string parse to the structure
	my $yaml_copy = $string;
	my $yaml      = eval { CPAN::Meta::YAML->read_string( $yaml_copy ); };
	Test::More::is( $@, '', "$name: CPAN::Meta::YAML parses without error" );
	Test::More::is( $yaml_copy, $string, "$name: CPAN::Meta::YAML does not modify the input string" );
	SKIP: {
		Test::More::skip( "Shortcutting after failure", 2 ) if $@;
		Test::More::isa_ok( $yaml, 'CPAN::Meta::YAML' );
		Test::More::is_deeply( $yaml, $object, "$name: CPAN::Meta::YAML parses correctly" );
	}

	# Does the structure serialize to the string.
	# We can't test this by direct comparison, because any
	# whitespace or comments would be lost.
	# So instead we parse back in.
	my $output = eval { $object->write_string };
	Test::More::is( $@, '', "$name: CPAN::Meta::YAML serializes without error" );
	SKIP: {
		Test::More::skip( "Shortcutting after failure", 5 ) if $@;
		Test::More::ok(
			!!(defined $output and ! ref $output),
			"$name: CPAN::Meta::YAML serializes correctly",
		);
		my $roundtrip = eval { CPAN::Meta::YAML->read_string( $output ) };
		Test::More::is( $@, '', "$name: CPAN::Meta::YAML round-trips without error" );
		Test::More::skip( "Shortcutting after failure", 2 ) if $@;
		Test::More::isa_ok( $roundtrip, 'CPAN::Meta::YAML' );
		Test::More::is_deeply( $roundtrip, $object, "$name: CPAN::Meta::YAML round-trips correctly" );

		# Testing the serialization
		Test::More::skip( "Shortcutting perfect serialization tests", 1 ) unless $options{serializes};
		Test::More::is( $output, $string, 'Serializes ok' );
	}

	# Return true as a convenience
	return 1;
}

sub yaml_error {
	my $string = shift;
	my $like   = shift;
	my $yaml   = CPAN::Meta::YAML->read_string( $string );
	Test::More::is( $yaml, undef, '->read_string returns undef' );
	Test::More::ok( CPAN::Meta::YAML->errstr =~ /$like/, "Got expected error" );
	# NOTE: like() gives better diagnostics (but requires 5.005)
	# Test::More::like( $@, qr/$_[0]/, "CPAN::Meta::YAML throws expected error" );
}

sub slurp {
	my $file = shift;
	local $/ = undef;
	open( FILE, " $file" ) or die "open($file) failed: $!";
	binmode( FILE, $_[0] ) if @_ > 0 && $] > 5.006;
	# binmode(FILE); # disable perl's BOM interpretation
	my $source = <FILE>;
	close( FILE ) or die "close($file) failed: $!";
	$source;
}

sub load_ok {
	my $name = shift;
	my $file = shift;
	my $size = shift;
	Test::More::ok( -f $file, "Found $name" );
	Test::More::ok( -r $file, "Can read $name" );
	my $content = slurp( $file );
	Test::More::ok( (defined $content and ! ref $content), "Loaded $name" );
	Test::More::ok( ($size < length $content), "Content of $name larger than $size bytes" );
	return $content;
}

1;
