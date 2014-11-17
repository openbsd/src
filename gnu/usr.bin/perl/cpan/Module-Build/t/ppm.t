#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest;
use Config;

blib_load('Module::Build');
blib_load('Module::Build::ConfigData');
my $PPM_support = Module::Build::ConfigData->feature('PPM_support');
my $manpage_support = Module::Build::ConfigData->feature('manpage_support');
my $HTML_support = Module::Build::ConfigData->feature('HTML_support');

my $tmp;

{
  my ($have_c_compiler, $tmp_exec) = check_compiler();
  if ( ! $have_c_compiler ) {
    plan skip_all => 'No compiler found';
  } elsif ( ! $PPM_support ) {
    plan skip_all => 'PPM support modules not installed';
  } elsif ( !$Config{usedl} ) {
    plan skip_all => 'Perl not compiled for dynamic loading';
  } elsif ( ! $HTML_support )  {
    plan skip_all => "HTML support not installed";
  } elsif ( ! eval {require Archive::Tar} ) {
    plan skip_all => "Archive::Tar not installed to read archives.";
  } elsif ( ! eval {IO::Zlib->VERSION(1.01)} ) {
    plan skip_all => "IO::Zlib 1.01 required to read compressed archives.";
  } elsif ( $^O eq 'VMS' ) {
    plan skip_all => "Needs porting work on VMS";
  } else {
    plan tests => 12;
  }
  require Cwd;
  $tmp = MBTest->tmpdir( $tmp_exec ? () : (DIR => Cwd::cwd) );
}


use DistGen;
my $dist = DistGen->new( dir => $tmp, xs => 1 );
$dist->add_file( 'hello', <<'---' );
#!perl -w
print "Hello, World!\n";
__END__

=pod

=head1 NAME

hello

=head1 DESCRIPTION

Says "Hello"

=cut
---
$dist->change_build_pl
({
  module_name => $dist->name,
  license     => 'perl',
  scripts     => [ 'hello' ],
});
$dist->regen;

$dist->chdir_in;

use File::Spec::Functions qw(catdir);

my @installstyle = qw(lib perl5);
my $mb = Module::Build->new_from_context(
  verbose => 0,
  quiet   => 1,

  installdirs => 'site',
  config => {
    manpage_reset(), html_reset(),
    ( $manpage_support ?
      ( installsiteman1dir  => catdir($tmp, 'site', 'man', 'man1'),
        installsiteman3dir  => catdir($tmp, 'site', 'man', 'man3') ) : () ),
    ( $HTML_support ?
      ( installsitehtml1dir => catdir($tmp, 'site', 'html'),
        installsitehtml3dir => catdir($tmp, 'site', 'html') ) : () ),
  },
  html_links => 0,
);



$mb->dispatch('ppd', args => {codebase => '/path/to/codebase-xs'});

(my $dist_filename = $dist->name) =~ s/::/-/g;
my $ppd = slurp($dist_filename . '.ppd');

my $perl_version = Module::Build::PPMMaker->_ppd_version($mb->perl_version);
my $varchname = Module::Build::PPMMaker->_varchname($mb->config);

# This test is quite a hack since with XML you don't really want to
# do a strict string comparison, but absent an XML parser it's the
# best we can do.
is $ppd, <<"---";
<SOFTPKG NAME="$dist_filename" VERSION="0.01">
    <ABSTRACT>Perl extension for blah blah blah</ABSTRACT>
    <AUTHOR>A. U. Thor, a.u.thor\@a.galaxy.far.far.away</AUTHOR>
    <IMPLEMENTATION>
        <ARCHITECTURE NAME="$varchname" />
        <CODEBASE HREF="/path/to/codebase-xs" />
    </IMPLEMENTATION>
</SOFTPKG>
---



$mb->dispatch('ppmdist');
is $@, '';

my $tar = Archive::Tar->new;

my $tarfile = $mb->ppm_name . '.tar.gz';
$tar->read( $tarfile, 1 );

my $files = { map { $_ => 1 } $tar->list_files };

my $fname = 'Simple';
$fname = DynaLoader::mod2fname([$fname]) if defined &DynaLoader::mod2fname;
exists_ok($files, "blib/arch/auto/Simple/$fname." . $mb->config('dlext'));
exists_ok($files, 'blib/lib/Simple.pm');
exists_ok($files, 'blib/script/hello');

SKIP: {
  skip( "manpage_support not enabled.", 2 ) unless $manpage_support;

  exists_ok($files, 'blib/man3/Simple.' . $mb->config('man3ext'));
  exists_ok($files, 'blib/man1/hello.' . $mb->config('man1ext'));
}

SKIP: {
  skip( "HTML_support not enabled.", 2 ) unless $HTML_support;

  exists_ok($files, 'blib/html/site/lib/Simple.html');
  exists_ok($files, 'blib/html/bin/hello.html');
}

$tar->clear;
undef( $tar );

$mb->dispatch('realclean');
$dist->clean;


SKIP: {
  skip( "HTML_support not enabled.", 3 ) unless $HTML_support;

  # Make sure html documents are generated for the ppm distro even when
  # they would not be built during a normal build.
  $mb = Module::Build->new_from_context(
    verbose => 0,
    quiet   => 1,

    installdirs => 'site',
    config => {
      html_reset(),
      installsiteman1dir  => catdir($tmp, 'site', 'man', 'man1'),
      installsiteman3dir  => catdir($tmp, 'site', 'man', 'man3'),
    },
    html_links => 0,
  );

  $mb->dispatch('ppmdist');
  is $@, '';

  $tar = Archive::Tar->new;
  $tar->read( $tarfile, 1 );

  $files = {map { $_ => 1 } $tar->list_files};

  exists_ok($files, 'blib/html/site/lib/Simple.html');
  exists_ok($files, 'blib/html/bin/hello.html');

  $tar->clear;

  $mb->dispatch('realclean');
  $dist->clean;
}


########################################

sub exists_ok {
  my $files = shift;
  my $file  = shift;
  local $Test::Builder::Level = $Test::Builder::Level + 1;
  ok exists( $files->{$file} ) && $files->{$file}, $file;
}

# A hash of all Config.pm settings related to installing
# manpages with values set to an empty string.
sub manpage_reset {
  return (
    installman1dir => '',
    installman3dir => '',
    installsiteman1dir => '',
    installsiteman3dir => '',
    installvendorman1dir => '',
    installvendorman3dir => '',
  );
}

# A hash of all Config.pm settings related to installing
# html documents with values set to an empty string.
sub html_reset {
  return (
    installhtmldir => '',
    installhtml1dir => '',
    installhtml3dir => '',
    installsitehtml1dir => '',
    installsitehtml3dir => '',
    installvendorhtml1dir => '',
    installvendorhtml3dir => '',
  );
}

