use ExtUtils::MakeMaker;

WriteMakefile( NAME => "OpenBSD", INSTALLDIRS => 'perl', EXE_FILES =>
	[ qw( pkg_add pkg_info pkg_delete pkg_create pkg pkg_merge pkg_mklocatedb) ], VERSION => `uname -r`, INSTALLSCRIPT => '/usr/sbin' );
