
use ExtUtils::MakeMaker;
use Carp;

my $mm_version = $ExtUtils::MakeMaker::VERSION || $ExtUtils::MakeMaker::Version;
if( $mm_version < 3.5 ) {
        croak("Sorry, but MakeMaker 3.5 or better is needed to build this package.");
}

&WriteMakefile(
	NAME         => 'Term::ReadKey',
	DISTNAME     => 'TermReadKey',
    LICENSE      => 'perl',
    META_MERGE => {
        no_index  => {
            file  => [qw(Configure.pm)],
            package  => [qw(Configure)],
        },
        'meta-spec' => { version => 2 },
        resources => {
            repository => {
                type => 'git',
                url  => 'https://github.com/jonathanstowe/TermReadKey.git',
                web  => 'https://github.com/jonathanstowe/TermReadKey',
            },
        },
    },
	VERSION_FROM => 'ReadKey.pm',
        XSPROTOARG   => '-noprototypes',
	PM => { "ReadKey.pm" => '$(INST_LIBDIR)/ReadKey.pm'},

	'dist' => { COMPRESS=>"gzip", SUFFIX=>"gz" },
  "test" => {
    "TESTS" => "t/*.t"
  }

# Uncomment these to allow testing of sgtty under Linux. Not needed normally.
#	INC => "-I/usr/include/bsd",
#	LIBS => "-lbsd"
);

sub MY::realclean {
	my $self = shift;
	$_ = $self->MM::realclean();
	s/\t/\trm -f cchars.h\n\t/;
	$_;
}

sub MY::top_targets {
	my $self = shift;
	$_ = $self->MM::top_targets();
	$_ .= "

sgtty cchars.h: genchars.pl
	\$(PERL) -I. -I\$(PERL_LIB) genchars.pl

distcc: genchars.pl
	\$(PERL) -I. -I\$(PERL_LIB) genchars.pl dist

ReadKey.c: cchars.h

";
	$_;

}

sub MY::test {
	my $self = shift;
	$_ = $self->MM::test();
	s#example/test.pl#-w example/test.pl#;
	$_;
}

sub MY::test_interactive
{
    return "Fooo";
}

sub MY::pure_site_install
{
    my $self = shift;
    my $new = $self->MM::test();

    $new .= "\n\t./register_module Your::Module";
    return $new;
}
