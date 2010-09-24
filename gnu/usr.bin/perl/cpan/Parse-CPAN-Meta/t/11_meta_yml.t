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
use Test::More tests(8, 3);





#####################################################################
# Testing YAML::Tiny's own META.yml file

yaml_ok(
	<<'END_YAML',
abstract: Read/Write YAML files with as little code as possible
author: 'Adam Kennedy <cpan@ali.as>'
build_requires:
  File::Spec: 0.80
  Test::More: 0.47
distribution_type: module
generated_by: Module::Install version 0.63
license: perl
name: YAML-Tiny
no_index:
  directory:
    - inc
    - t
requires:
  perl: 5.005
version: 0.03
END_YAML
	[ {
		abstract          => 'Read/Write YAML files with as little code as possible',
		author            => 'Adam Kennedy <cpan@ali.as>',
		build_requires    => {
			'File::Spec' => '0.80',
			'Test::More' => '0.47',
		},
		distribution_type => 'module',
		generated_by      => 'Module::Install version 0.63',
		license           => 'perl',
		name              => 'YAML-Tiny',
		no_index          => {
			directory    => [ qw{inc t} ],
		},
		requires          => {
			perl         => '5.005',
		},
		version           => '0.03',
	} ],
	'YAML::Tiny',
);






#####################################################################
# Testing a META.yml from a commercial project that crashed

yaml_ok(
	<<'END_YAML',
# http://module-build.sourceforge.net/META-spec.html
#XXXXXXX This is a prototype!!!  It will change in the future!!! XXXXX#
name:         ITS-SIN-FIDS-Content-XML
version:      0.01
version_from: lib/ITS/SIN/FIDS/Content/XML.pm
installdirs:  site
requires:
    Test::More:                    0.45
    XML::Simple:                   2

distribution_type: module
generated_by: ExtUtils::MakeMaker version 6.30
END_YAML
	[ {
		name              => 'ITS-SIN-FIDS-Content-XML',
		version           => "0.01",
		version_from      => 'lib/ITS/SIN/FIDS/Content/XML.pm',
		installdirs       => 'site',
		requires          => {
			'Test::More'  => 0.45,
			'XML::Simple' => 2,
			},
		distribution_type => 'module',
		generated_by      => 'ExtUtils::MakeMaker version 6.30',
	} ],
	'YAML::Tiny',
);






#####################################################################
# Testing various failing META.yml files from CPAN

yaml_ok(
	<<'END_YAML',
---
abstract: Mii in Nintendo Wii data parser and builder
author: Toru Yamaguchi <zigorou@cpan.org>
distribution_type: module
generated_by: Module::Install version 0.65
license: perl
meta-spec:
  url: http://module-build.sourceforge.net/META-spec-v1.3.html
  version: 1.3
name: Games-Nintendo-Wii-Mii
no_index:
  directory:
    - inc
    - t
requires:
  Carp: 1.03
  Class::Accessor::Fast: 0.3
  File::Slurp: 9999.12
  IO::File: 1.1
  Readonly: 0
  Tie::IxHash: 1.21
  URI: 1.35
  XML::LibXML: 1.62
version: 0.02
END_YAML
	[ {
		abstract => 'Mii in Nintendo Wii data parser and builder',
		author   => 'Toru Yamaguchi <zigorou@cpan.org>',
		distribution_type => 'module',
		generated_by => 'Module::Install version 0.65',
		license => 'perl',
		'meta-spec' => {
			url => 'http://module-build.sourceforge.net/META-spec-v1.3.html',
			version => '1.3',
		},
		name => 'Games-Nintendo-Wii-Mii',
		no_index => {
			directory => [ qw{ inc t } ],
		},
		requires => {
			'Carp' => '1.03',
			'Class::Accessor::Fast' => '0.3',
			'File::Slurp' => '9999.12',
			'IO::File'    => '1.1',
			'Readonly'    => '0',
			'Tie::IxHash' => '1.21',
			'URI'         => '1.35',
			'XML::LibXML' => '1.62',
		},
		version => '0.02',
	} ],
	'Games-Nintendo-Wii-Mii',
);

yaml_ok(
	<<'END_YAML',
# http://module-build.sourceforge.net/META-spec.html
#XXXXXXX This is a prototype!!!  It will change in the future!!! XXXXX#
name:         Acme-Time-Baby
version:      2.106
version_from: Baby.pm
installdirs:  site
requires:
    warnings:

distribution_type: module
generated_by: ExtUtils::MakeMaker version 6.17
END_YAML
	[ {
		name => 'Acme-Time-Baby',
		version => '2.106',
		version_from => 'Baby.pm',
		installdirs => 'site',
		requires => {
			warnings => undef,
		},
		distribution_type => 'module',
		generated_by => 'ExtUtils::MakeMaker version 6.17',
	} ],
	'Acme-Time-Baby',
	noyamlperl => 1,
);





#####################################################################
# File with a YAML header

yaml_ok(
	<<'END_YAML',
--- #YAML:1.0
name:     Data-Swap
version:  0.05
license:  perl
distribution_type: module
requires:
   perl:  5.6.0
dynamic_config: 0
END_YAML
	[ {
		name => 'Data-Swap',
		version => '0.05',
		license => 'perl',
		distribution_type => 'module',
		requires => {
			perl => '5.6.0',
		},
		dynamic_config => '0',
	} ],
	'Data-Swap',
	nosyck => 1,
);





#####################################################################
# Various files that fail for unknown reasons

SCOPE: {
	my $content = load_ok(
		'Template-Provider-Unicode-Japanese.yml',
		catfile( test_data_directory(), 'Template-Provider-Unicode-Japanese.yml' ),
		100
	);
	yaml_ok(
		$content,
		[ {
			abstract => 'Decode all templates by Unicode::Japanese',
			author   => 'Hironori Yoshida C<< <yoshida@cpan.org> >>',
			distribution_type => 'module',
			generated_by => 'Module::Install version 0.65',
			license => 'perl',
			'meta-spec' => {
				url => 'http://module-build.sourceforge.net/META-spec-v1.3.html',
				version => '1.3',
			},
			name => 'Template-Provider-Unicode-Japanese',
			no_index => {
				directory => [ qw{ inc t } ],
			},
			requires => {
				'Template::Config' => 0,
				'Unicode::Japanese' => 0,
				perl => '5.6.0',
				version => '0',
			},
			version => '1.2.1',
		} ],
		'Template-Provider-Unicode-Japanese',
		noyamlperl => 1,
	);
}

SCOPE: {
	my $content = load_ok(
		'HTML-WebDAO.yml',
		catfile( test_data_directory(), 'HTML-WebDAO.yml' ),
		100
	);
	yaml_ok(
		$content,
		[ {
			abstract => 'Perl extension for create complex web application',
			author   => [
				'Zahatski Aliaksandr, E<lt>zagap@users.sourceforge.netE<gt>',
			],
			license  => 'perl',
			name     => 'HTML-WebDAO',
			version  => '0.04',
		} ],
		'HTML-WebDAO',
		nosyck => 1,
	);
}

SCOPE: {
	my $content = load_ok(
		'Spreadsheet-Read.yml',
		catfile( test_data_directory(), 'Spreadsheet-Read.yml' ),
		100
	);
	yaml_ok(
		$content,
		[ {
			'resources' => {
				'license' => 'http://dev.perl.org/licenses/'
			},
			'meta-spec' => {
				'version' => '1.4',
				'url' => 'http://module-build.sourceforge.net/META-spec-v1.4.html'
			},
			'distribution_type' => 'module',
			'generated_by' => 'Author',
			'version' => 'VERSION',
			'name' => 'Read',
			'author' => [
				'H.Merijn Brand <h.m.brand@xs4all.nl>'
			],
			'license' => 'perl',
			'build_requires' => {
				'Test::More' => '0',
				'Test::Harness' => '0',
				'perl' => '5.006'
			},
			'provides' => {
				'Spreadsheet::Read' => {
					'version' => 'VERSION',
					'file' => 'Read.pm'
				}
			},
			'optional_features' => [
				{
					'opt_csv' => {
						'requires' => {
							'Text::CSV_XS' => '0.23'
						},
						'recommends' => {
							'Text::CSV_PP' => '1.10',
							'Text::CSV_XS' => '0.58',
							'Text::CSV' => '1.10'
						},
						'description' => 'Provides parsing of CSV streams'
					}
				},
				{
					'opt_excel' => {
						'requires' => {
							'Spreadsheet::ParseExcel' => '0.26',
							'Spreadsheet::ParseExcel::FmtDefault' => '0'
						},
						'recommends' => {
							'Spreadsheet::ParseExcel' => '0.42'
						},
						'description' => 'Provides parsing of Microsoft Excel files'
					}
				},
				{
					'opt_excelx' => {
						'requires' => {
							'Spreadsheet::XLSX' => '0.07'
						},
						'description' => 'Provides parsing of Microsoft Excel 2007 files'
					}
				},
				{
					'opt_oo' => {
						'requires' => {
							'Spreadsheet::ReadSXC' => '0.2'
						},
						'description' => 'Provides parsing of OpenOffice spreadsheets'
					}
				},
				{
					'opt_tools' => {
						'recommends' => {
							'Tk::TableMatrix::Spreadsheet' => '0',
							'Tk::NoteBook' => '0',
							'Tk' => '0'
						},
						'description' => 'Spreadsheet tools'
					}
				}
			],
			'requires' => {
				'perl' => '5.006',
				'Data::Dumper' => '0',
				'Exporter' => '0',
				'Carp' => '0'
			},
			'recommends' => {
				'perl' => '5.008005',
				'IO::Scalar' => '0',
				'File::Temp' => '0.14'
			},
			'abstract' => 'Meta-Wrapper for reading spreadsheet data'
		} ],
		'Spreadsheet-Read',
		noyamlpm   => 1,
		noyamlperl => 1,
	);
}
