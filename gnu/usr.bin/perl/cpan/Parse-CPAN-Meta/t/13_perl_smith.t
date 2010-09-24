#!/usr/bin/perl

# Testing of common META.yml examples

BEGIN {
	if( $ENV{PERL_CORE} ) {
		chdir 't';
		@INC = ('../lib', 'lib');
	}
	else {
		unshift @INC, 't/lib/';
	}
}

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use File::Spec::Functions ':ALL';
use Parse::CPAN::Meta::Test;
use Test::More tests(1, 1);





#####################################################################
# Testing that Perl::Smith config files work

my $vanilla_file = catfile( test_data_directory(), 'vanilla.yml' );
my $vanilla      = load_ok( 'yanilla.yml', $vanilla_file, 1000 );

yaml_ok(
	$vanilla,
	[ {
		package_name    => 'VanillaPerl',
		package_version => 5,
		download_dir    => 'c:\temp\vp_sources',
		build_dir       => 'c:\temp\vp_build',
		image_dir       => 'c:\vanilla-perl',
		binary          => [
			{
				name    => 'dmake',
				url     => 'http://search.cpan.org/CPAN/authors/id/S/SH/SHAY/dmake-4.5-20060619-SHAY.zip',
				license => {
					'dmake/COPYING' => 'dmake/COPYING',
					'dmake/readme/license.txt' => 'dmake/license.txt',
				},
				install_to => {
					'dmake/dmake.exe' => 'dmake/bin/dmake.exe',
					'dmake/startup' => 'dmake/bin/startup',
				},
			},
			{
				name       => 'gcc-core',
				url        => 'http://umn.dl.sourceforge.net/mingw/gcc-core-3.4.5-20060117-1.tar.gz',
				license    => {
					'COPYING'     => 'gcc/COPYING',
					'COPYING.lib' => 'gcc/COPYING.lib',
				},
				install_to => 'mingw',
			},
			{
				name       => 'gcc-g++',
				url        => 'http://umn.dl.sourceforge.net/mingw/gcc-g++-3.4.5-20060117-1.tar.gz',
				license    => undef,
				install_to => 'mingw',
			},
			{
				name       => 'binutils',
				url        => 'http://umn.dl.sourceforge.net/mingw/binutils-2.16.91-20060119-1.tar.gz',
				license    => {
					'Copying'     => 'binutils/Copying',
					'Copying.lib' => 'binutils/Copying.lib',
				},
				install_to => 'mingw',
			},
			{
				name       => 'mingw-runtime',
				url        => 'http://umn.dl.sourceforge.net/mingw/mingw-runtime-3.10.tar.gz',
				license    => {
					'doc/mingw-runtime/Contributors' => 'mingw/Contributors',
					'doc/mingw-runtime/Disclaimer'   => 'mingw/Disclaimer',
				},
				install_to => 'mingw',
			},
			{
				name       => 'w32api',
				url        => 'http://umn.dl.sourceforge.net/mingw/w32api-3.6.tar.gz',
				license    => undef,
				install_to => 'mingw',
				extra      => {
					'extra\README.w32api' => 'licenses\win32api\README.w32api',
				},
			}
		],
		source => [
			{
				name       => 'perl',
				url        => 'http://mirrors.kernel.org/CPAN/src/perl-5.8.8.tar.gz',
				license    => {
					'perl-5.8.8/Readme'   => 'perl/Readme',
					'perl-5.8.8/Artistic' => 'perl/Artistic',
					'perl-5.8.8/Copying'  => 'perl/Copying',
				},
				unpack_to  => 'perl',
				install_to => 'perl',
				after      => {
					'extra\Config.pm' => 'lib\CPAN\Config.pm',
				},
			}
		],
		modules => [
			{
				name      => 'Win32::Job',
				unpack_to => {
					APIFile => 'Win32API-File',
				},
			},
			{
				name  => 'IO',
				force => 1,
			},
			{
				name => 'Compress::Zlib',
			},
			{
				name => 'IO::Zlib',
			},
			{
				name => 'Archive::Tar',
			},
			{
				name  => 'Net::FTP',
				extra => {
					'extra\libnet.cfg' => 'libnet.cfg',
				},
			},
		],
		extra => {
			'README' => 'README.txt',
			'LICENSE.txt' => 'LICENSE.txt',
			'Changes' => 'Release-Notes.txt',
			'extra\Config.pm' => 'perl\lib\CPAN\Config.pm',
			'extra\links\Perl-Documentation.url' => 'links\Perl Documentation.url',
			'extra\links\Perl-Homepage.url' => 'links\Perl Homepage.url',
			'extra\links\Perl-Mailing-Lists.url' => 'links\Perl Mailing Lists.url',
			'extra\links\Perlmonks-Community-Forum.url' => 'links\Perlmonks Community Forum.url',
			'extra\links\Search-CPAN-Modules.url' => 'links\Search CPAN Modules.url',
			'extra\links\Vanilla-Perl-Homepage.url' => 'links\Vanilla Perl Homepage.url',
		},
	} ],
	'vanilla.yml',
	nosyck     => 1,
	noyamlperl => 1,
);
