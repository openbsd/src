use warnings;
use strict;

use ExtUtils::MakeMaker;

WriteMakefile(
	NAME => "Carp",
	VERSION_FROM => "lib/Carp.pm",
	ABSTRACT_FROM => "lib/Carp.pm",
	PREREQ_PM => {
		"Exporter" => 0,
		"IPC::Open3" => "1.0103",
		"Test::More" => 0,
		"strict" => 0,
		"warnings" => 0,
	},
	LICENSE => "perl",
	INSTALLDIRS => "$]" < 5.011 ? "perl" : "site",
);

1;
