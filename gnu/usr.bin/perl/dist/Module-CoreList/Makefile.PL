use ExtUtils::MakeMaker;

push @extra, 'INSTALLDIRS' => 'perl' if $] >= 5.008009 and $] < 5.012;


WriteMakefile
(
    'NAME' => 'Module::CoreList',
    'VERSION_FROM' => 'lib/Module/CoreList.pm',
    'ABSTRACT_FROM' => 'lib/Module/CoreList.pod',
    'PREREQ_PM' => {
	'Test::More' => '0',
    },
    'EXE_FILES' => [ _scripts() ],
    'PL_FILES' => {},
    LICENSE => 'perl',
    @extra,
)
;

sub _scripts {
 my $scripts = 'corelist';
 if ( $] >= 5.008009 and !$ENV{PERL_CORE} ) {
   require Config;
   my $version = sprintf("%vd",$^V);
   if ( $Config::Config{versiononly} and
      $Config::Config{startperl} =~ /\Q$version\E$/ ) {
      require File::Copy;
      File::Copy::copy( 'corelist', "corelist$version" );
      $scripts = "corelist$version";
    }
 }
 return $scripts;
}
