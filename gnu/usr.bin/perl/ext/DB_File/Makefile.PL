use strict;
use warnings;

use ExtUtils::MakeMaker ;
use ExtUtils::Constant qw(WriteConstants);
use Config ;

# OS2 is a special case, so check for it now.
my $OS2 = "-DOS2" if $Config{'osname'} eq 'os2' ;

my $LIB = "-ldb" ;
# so is win32
$LIB = "-llibdb" if $^O eq 'MSWin32' ;

WriteMakefile(
	NAME 		=> 'DB_File',
	LIBS 		=> ["-L/usr/local/lib $LIB"],
        MAN3PODS        => {},         # Pods will be built by installman.
	#INC		=> '-I/usr/local/include',
	VERSION_FROM	=> 'DB_File.pm',
	OBJECT		=> 'version$(OBJ_EXT) DB_File$(OBJ_EXT)',
	XSPROTOARG	=> '-noprototypes',
	DEFINE		=> $OS2 || "",
	INC		=> ($^O eq "MacOS" ? "-i ::::db:include" : ""),
	'depend'	=> {'version$(OBJ_EXT)' => 'version.c'},
	'clean'		=> {FILES => 'constants.h constants.xs'},
	);

my @names = qw(
	BTREEMAGIC
	BTREEVERSION
	DB_LOCK
	DB_SHMEM
	DB_TXN
	HASHMAGIC
	HASHVERSION
	MAX_PAGE_NUMBER
	MAX_PAGE_OFFSET
	MAX_REC_NUMBER
	RET_ERROR
	RET_SPECIAL
	RET_SUCCESS
	R_CURSOR
	R_DUP
	R_FIRST
	R_FIXEDLEN
	R_IAFTER
	R_IBEFORE
	R_LAST
	R_NEXT
	R_NOKEY
	R_NOOVERWRITE
	R_PREV
	R_RECNOSYNC
	R_SETCURSOR
	R_SNAPSHOT
	__R_UNUSED
	);

    # Check the constants above all appear in @EXPORT in DB_File.pm
    my %names = map { $_, 1} @names;
    open F, "<DB_File.pm" or die "Cannot open DB_File.pm: $!\n";
    while (<F>)
    {
        last if /^\s*\@EXPORT\s+=\s+qw\(/ ;
    }

    while (<F>)
    {
        last if /^\s*\)/ ;
        /(\S+)/ ;
        delete $names{$1} if defined $1 ;
    }
    close F ;

    if ( keys %names )
    {
        my $missing = join ("\n\t", sort keys %names) ;
        die "The following names are missing from \@EXPORT in DB_File.pm\n" .
            "\t$missing\n" ;
    }
    

    WriteConstants( NAME => 'DB_File',
                    NAMES => \@names,
                    C_FILE  => 'constants.h',
                    XS_FILE  => 'constants.xs',
                  );
