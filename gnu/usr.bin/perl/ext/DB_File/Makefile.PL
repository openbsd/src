use ExtUtils::MakeMaker 5.16 ;
use Config ;

# OS2 is a special case, so check for it now.
my $OS2 = "-DOS2" if $Config{'osname'} eq 'os2' ;

WriteMakefile(
	NAME 		=> 'DB_File',
	LIBS 		=> ["-L/usr/local/lib -ldb"],
        MAN3PODS        => ' ',         # Pods will be built by installman.
	#INC		=> '-I/usr/local/include',
	VERSION_FROM	=> 'DB_File.pm',
	XSPROTOARG	=> '-noprototypes',
	DEFINE		=> "$OS2",
	);

