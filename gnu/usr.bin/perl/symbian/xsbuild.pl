#!/usr/bin/perl -w

use strict;

use Getopt::Long;
use File::Basename;
use Cwd;

unshift @INC, dirname $0 || '.';
do "sanity.pl" or die $@;

my $CoreBuild = -d "ext" && -f "perl.h" && -d "symbian" && -f "perl.c";

my $SymbianVersion;

if (exists $ENV{EPOCROOT}) {
    if ($ENV{EPOCROOT} =~ m!\\Symbian\\UIQ_21\\$!i) {
	$SymbianVersion = '7.0s'; # TODO: other UIQ versions
    } elsif ($ENV{EPOCROOT} =~ m!\\Symbian\\(.+?)\\!i) {
	$SymbianVersion = $1;
    }
}

$SymbianVersion = $ENV{XSBUILD_SYMBIAN_VERSION}
  if exists $ENV{XSBUILD_SYMBIAN_VERSION};

my $PerlVersion    = $ENV{XSBUILD_PERL_VERSION};
my $CSuffix        = '.c';
my $CPlusPlus;
my $Config;
my $Build;
my $Clean;
my $DistClean;
my $Sis;

sub usage {
    die <<__EOF__;
$0: Usage: $0 [--symbian=version] [--perl=version]
              [--extversion=x.y]
              [--csuffix=csuffix] [--cplusplus|--cpp]
              [--win=win] [--arm=arm]
              [--config|--build|--clean|--distclean|--sis] ext
__EOF__
}

my $CWD;
my $SDK;
my $VERSION;
my $R_V_SV;
my $PERLSDK;
my $WIN = 'wins';
my $ARM = 'thumb';
my $BUILDROOT = getcwd();

if ( !defined $PerlVersion && $0 =~ m:\\symbian\\perl\\(.+)\\bin\\xsbuild.pl:i )
{
    $PerlVersion = $1;
}

if ( !defined $SymbianVersion) {
    ($SymbianVersion) = ($ENV{PATH} =~ m!\\Symbian\\(.+?)\\!i);
}

my ($SYMBIAN_ROOT, $SYMBIAN_VERSION, $SDK_NAME, $SDK_VARIANT, $SDK_VERSION);

if ($CoreBuild) {
    do "sanity.pl" or die $@;
    my %VERSION = %{ do "version.pl" or die $@ };
    ($SYMBIAN_ROOT, $SYMBIAN_VERSION, $SDK_NAME, $SDK_VARIANT, $SDK_VERSION) =
      @{ do "sdk.pl" or die $@ };
    $VERSION = "$VERSION{REVISION}$VERSION{VERSION}$VERSION{SUBVERSION}";
    $R_V_SV  = "$VERSION{REVISION}.$VERSION{VERSION}.$VERSION{SUBVERSION}";
    $BUILDROOT    = do "cwd.pl" or die $@;
    $PerlVersion    = $R_V_SV;
}

my %CONF;

usage()
  unless GetOptions(
    'symbian=s'     => \$SymbianVersion,
    'perl=s'        => \$PerlVersion,
    'extversion=s'  => \$CONF{EXTVERSION},
    'csuffix=s'     => \$CSuffix,
    'cplusplus|cpp' => \$CPlusPlus,
    'win=s'         => \$WIN,
    'arm=s'         => \$ARM,
    'config'        => \$Config,
    'build'         => \$Build,
    'clean'         => \$Clean,
    'distclean'     => \$DistClean,
    'sis'           => \$Sis
  );

usage() unless @ARGV;

$CSuffix = '.cpp' if $CPlusPlus;
$Build = !( $Config || $Clean || $DistClean ) || $Sis unless defined $Build;

die "$0: Symbian version undefined\n" unless defined $SymbianVersion;

$SymbianVersion =~ s:/:\\:g;

#die "$0: Symbian version '$SymbianVersion' not found\n"
#  unless -d "\\Symbian\\$SymbianVersion";

die "$0: Perl version undefined\n" unless defined $PerlVersion;

$PERLSDK = "$SYMBIAN_ROOT\\Perl\\$PerlVersion";

die "$0: Perl version '$PerlVersion' not found\n"
  if !$CoreBuild && !-d $PERLSDK;

print "Configuring with Symbian $SymbianVersion and Perl $PerlVersion...\n";

$R_V_SV = $PerlVersion;

$VERSION = $PerlVersion unless defined $VERSION;

$VERSION =~ tr/.//d if defined $VERSION;

$ENV{SDK}   = $SYMBIAN_ROOT;    # For the Errno extension
$ENV{CROSS} = 1;                # For the Encode extension (unbuilt now)

my $UARM = 'urel';
my $UREL = "$SYMBIAN_ROOT\\epoc32\\release\\-ARM-\\$UARM";
my $SRCDBG;
if (exists $ENV{UREL}) {
    $UREL = $ENV{UREL}; # from sdk.pl
    $UREL =~ s/-ARM-/$ARM/;
    $UARM = $ENV{UARM}; # from sdk.pl
    $SRCDBG = $UARM eq 'udeb' ? "SRCDBG" : "";
}

my %EXTCFG;

sub write_bld_inf {
    my ($base) = @_;
    print "\tbld.inf\n";
    open( BLD_INF, ">bld.inf" ) or die "$0: bld.inf: $!\n";
    print BLD_INF <<__EOF__;
PRJ_MMPFILES
$base.mmp
PRJ_PLATFORMS
$WIN $ARM
__EOF__
    close(BLD_INF);
}

sub system_echo {
    my $cmd = shift;
    print "xsbuild: ", $cmd, "\n";
    return system($cmd);
}

sub run_PL {
    my ( $PL, $dir, $file ) = @_;
    if ( defined $file ) {
        print "\t(Running $dir\\$PL to create $file)\n";
        unlink($file);
    }
    else {
        print "\t(Running $dir\\$PL)\n";
    }
    my $cmd;
    $ENV{PERL_CORE} = 1 if $CoreBuild;
    system_echo("perl -I$BUILDROOT\\lib -I$BUILDROOT\\xlib\\symbian -I$BUILDROOT\\t\\lib $PL") == 0
      or warn "$0: $PL failed.\n";
    if ( defined $file ) { -s $file or die "$0: No $file created.\n" }
}

sub read_old_multi {
    my ( $conf, $k ) = @_;
    push @{ $conf->{$k} }, split( ' ', $1 ) if /^$k\s(.+)$/;
}

sub uniquefy_filenames {
    my $b = [];
    my %c = ();
    for my $i (@{$_[0]}) {
        $i =~ s!/!\\!g;
        $i = lc $i if $i =~ m!\\!;
        $i =~ s!^c:!!;
        push @$b, $i unless $c{$i}++;
    }
    return $b;
}

sub read_mmp {
    my ( $conf, $mmp ) = @_;
    if ( -r $mmp && open( MMP, "<$mmp" ) ) {
        print "\tReading $mmp...\n";
        while (<MMP>) {
            chomp;
            $conf->{TARGET}     = $1 if /^TARGET\s+(.+)$/;
            $conf->{TARGETPATH} = $1 if /^TARGETPATH\s+(.+)$/;
            $conf->{EXTVERSION} = $1 if /^EXTVERSION\s+(.+)$/;
            read_old_multi( $conf, "SOURCE" );
            read_old_multi( $conf, "SOURCEPATH" );
            read_old_multi( $conf, "USERINCLUDE" );
            read_old_multi( $conf, "SYSTEMINCLUDE" );
            read_old_multi( $conf, "LIBRARY" );
            read_old_multi( $conf, "MACRO" );
        }
        close(MMP);
    }
}

sub write_mmp {
    my ( $ext, $base, $userinclude, @src ) = @_;

    my $extdash = $ext; $extdash =~ s!\\!-!g;

    print "\t$base.mmp\n";
    $CONF{TARGET}        = "perl$VERSION-$extdash.dll";
    $CONF{TARGETPATH}    = "\\System\\Libs\\Perl\\$R_V_SV";
    $CONF{SOURCE}        = [@src];
    $CONF{SOURCEPATH}    = [ $CWD, $BUILDROOT ];
    $CONF{USERINCLUDE}   = [ $CWD, $BUILDROOT ];
    $CONF{SYSTEMINCLUDE} = ["$PERLSDK\\include"] unless $CoreBuild;
    $CONF{SYSTEMINCLUDE} = [ $BUILDROOT, "$BUILDROOT\\symbian" ] if $CoreBuild;
    $CONF{LIBRARY}       = [];
    $CONF{MACRO}         = [];
    read_mmp( \%CONF, "_init.mmp" );
    read_mmp( \%CONF, "$base.mmp" );

    if ($base eq 'Zlib') {
	push @{$CONF{USERINCLUDE}}, "$CWD\\zlib-src";
    }

    for my $ui ( @{$userinclude} ) {
        $ui =~ s!/!\\!g;
        if ( $ui =~ m!^(?:[CD]:)?\\! ) {
            push @{ $CONF{USERINCLUDE} }, $ui;
        }
        else {
            push @{ $CONF{USERINCLUDE} }, "$BUILDROOT\\$ui";
        }
    }
    push @{ $CONF{SYSTEMINCLUDE} }, "\\epoc32\\include";
    push @{ $CONF{SYSTEMINCLUDE} }, "\\epoc32\\include\\libc";
    push @{ $CONF{LIBRARY} },       "euser.lib";
    push @{ $CONF{LIBRARY} },       "estlib.lib";
    push @{ $CONF{LIBRARY} },       "perl$VERSION.lib";
    push @{ $CONF{MACRO} },         "SYMBIAN" unless $CoreBuild;
    push @{ $CONF{MACRO} },         "PERL_EXT" if $CoreBuild;
    push @{ $CONF{MACRO} },         "MULTIPLICITY";
    push @{ $CONF{MACRO} },         "PERL_IMPLICIT_CONTEXT";
    push @{ $CONF{MACRO} },         "PERL_GLOBAL_STRUCT";
    push @{ $CONF{MACRO} },         "PERL_GLOBAL_STRUCT_PRIVATE";

    if ($SDK_VARIANT eq 'S60') {
      push @{ $CONF{MACRO} }, '__SERIES60__'
	unless grep { $_ eq '__SERIES60__' } @{ $CONF{MACRO} };
    }
    if ($SDK_VARIANT eq 'S80') {
      push @{ $CONF{MACRO} }, '__SERIES80__'
	unless grep { $_ eq '__SERIES80__' } @{ $CONF{MACRO} };
    }
    if ($SDK_VARIANT eq 'S90') {
      push @{ $CONF{MACRO} }, '__SERIES90__'
	unless grep { $_ eq '__SERIES90__' } @{ $CONF{MACRO} };
    }
    if ($SDK_VARIANT eq 'UIQ') {
      push @{ $CONF{MACRO} }, '__UIQ__'
	unless grep { $_ eq '__UIQ__' } @{ $CONF{MACRO} };
    }

    for my $u (qw(SOURCE SOURCEPATH SYSTEMINCLUDE USERINCLUDE LIBRARY MACRO)) {
        $CONF{$u} = uniquefy_filenames( $CONF{$u} );
    }
    open( BASE_MMP, ">$base.mmp" ) or die "$0: $base.mmp: $!\n";

    print BASE_MMP <<__EOF__;
TARGET		$CONF{TARGET}
TARGETTYPE	dll
TARGETPATH	$CONF{TARGETPATH}
SOURCE		@{$CONF{SOURCE}}
$SRCDBG
__EOF__
    for my $u (qw(SOURCEPATH SYSTEMINCLUDE USERINCLUDE)) {
        for my $v ( @{ $CONF{$u} } ) {
            print BASE_MMP "$u\t$v\n";
        }
    }
    # OPTION does not work in MMPs for pre-2.0 SDKs?
    print BASE_MMP <<__EOF__;
LIBRARY		@{$CONF{LIBRARY}}
MACRO		@{$CONF{MACRO}}
// OPTION	MSVC /P // Uncomment for creating .i (cpp'ed .cpp)
// OPTION	GCC -E  // Uncomment for creating .i (cpp'ed .cpp)
__EOF__
#    if (-f "$base.rss") {
#        print BASE_MMP "RESOURCE\t$base.rss\n";
#    }
    close(BASE_MMP);

}

sub write_makefile {
    my ( $base, $build ) = @_;

    print "\tMakefile\n";

    my $windef1 = "$SYMBIAN_ROOT\\Epoc32\\Build$CWD\\$base\\$WIN\\$base.def";
    my $windef2 = "..\\BWINS\\${base}u.def";
    my $armdef1 = "$SYMBIAN_ROOT\\Epoc32\\Build$CWD\\$base\\$ARM\\$base.def";
    my $armdef2 = "..\\BMARM\\${base}u.def";

    my $wrap = $SYMBIAN_ROOT && defined $SDK_VARIANT eq 'S60' && $SDK_VERSION eq '1.2' && $SYMBIAN_ROOT !~ /_CW$/;
    my $ABLD = $wrap ? 'perl b.pl' : 'abld';

    open( MAKEFILE, ">Makefile" ) or die "$0: Makefile: $!\n";
    print MAKEFILE <<__EOF__;
WIN = $WIN
ARM = $ARM
ABLD = $ABLD

all:	build freeze

sis:	build_arm freeze_arm

build:	abld.bat build_win build_arm

abld.bat:
	bldmake bldfiles

build_win: abld.bat
	bldmake bldfiles
	\$(ABLD) build \$(WIN) udeb

build_arm: abld.bat
	bldmake bldfiles
	\$(ABLD) build \$(ARM) $UARM

win:	build_win freeze_win

arm:	build_arm freeze_arm

freeze:	freeze_win freeze_arm

freeze_win:
	bldmake bldfiles
	\$(ABLD) freeze \$(WIN) $base

freeze_arm:
	bldmake bldfiles
	\$(ABLD) freeze \$(ARM) $base

defrost:	defrost_win defrost_arm

defrost_win:
	-del /f $windef1
	-del /f $windef2

defrost_arm:
	-del /f $armdef1
	-del /f $armdef2

clean:	clean_win clean_arm

clean_win:
	\$(ABLD) clean \$(WIN)

clean_arm:
	\$(ABLD) clean \$(ARM)

realclean:	clean realclean_win realclean_arm
	-del /f _init.c b.pl
	-del /f $base.c $base.mmp

realclean_win:
	\$(ABLD) reallyclean \$(WIN)

realclean_arm:
	\$(ABLD) reallyclean \$(ARM)

distclean:	defrost realclean
	-rmdir ..\\BWINS ..\\BMARM
	-del /f const-c.inc const-xs.inc
	-del /f Makefile abld.bat bld.inf
__EOF__
    close(MAKEFILE);
    if ($wrap) {
	if(open(B,">b.pl")) {
	    print B <<'__EOF__';
# abld.pl wrapper.

# nmake doesn't like MFLAGS and MAKEFLAGS being set to -w and w.
delete $ENV{MFLAGS};
delete $ENV{MAKEFLAGS};

print "abld @ARGV\n";
system_echo("abld @ARGV");
__EOF__
            close(B);
	} else {
	    warn "$0: failed to create b.pl: $!\n";
	}
    }
}

sub update_dir {
    print "[chdir from ", getcwd(), " to ";
    chdir(shift) or return;
    update_cwd();
    print getcwd(), "]\n";
}

sub patch_config {
    # Problem: the Config.pm we have in $BUILDROOT\\lib carries the
    # version number of the Perl we are building, while the Perl
    # we are running might have some other version.  Solution:
    # temporarily replace the Config.pm with a patched version.
    #
    # Reverse patch will be done with this special script
    my $config_restore_script = "$BUILDROOT\\lib\\symbian_config_restore.pl";
    # make sure the patch script was not left from previous run
    unlink $config_restore_script;
    return unless $CoreBuild;
    my $V = sprintf "%vd", $^V;
    # create reverse patch script
    if (open(RSCRIPT, ">$config_restore_script")) {
        print RSCRIPT <<__EOF__;
#!perl -pi.bak
s:\\Q$V:$R_V_SV:
__EOF__
        close RSCRIPT;
    } else {
        die "$0: Cannot create $config_restore_script: $!";
    }
    # patch the config
    unlink("$BUILDROOT\\lib\\Config.pm.bak");
    print "(patching $BUILDROOT\\lib\\Config.pm)\n";
    system_echo("perl -pi.bak -e \"s:\\Q$R_V_SV:$V:\" $BUILDROOT\\lib\\Config.pm");
}

sub restore_config {
    my $config_restore_script = "$BUILDROOT\\lib\\symbian_config_restore.pl";
    # this function should always return True
    # because it's commonly used in error handling blocks as
    #   &restore_config and die
    return 1 unless -f $config_restore_script;
    unlink("$BUILDROOT\\lib\\Config.pm.bak");
    print "(restoring $BUILDROOT\\lib\\Config.pm)\n";
    system_echo("perl -pi.bak $config_restore_script $BUILDROOT\\lib\\Config.pm");
    unlink "$BUILDROOT\\lib\\Config.pm.bak", $config_restore_script;
    # above command should always return 2 already,
    # but i want to be absolutely sure that return value is True
    return 1;
}

sub xsconfig {
    my ( $ext, $dir ) = @_;
    print "Configuring for $ext, directory '$dir'...\n";
    my $extu = $CoreBuild ? "$BUILDROOT\\lib\\ExtUtils" : "$PERLSDK\\lib\\ExtUtils";
    update_dir($dir) or die "$0: chdir '$dir': $!\n";
    &patch_config;

    my $build  = dirname($ext);
    my $base   = basename($ext);
    my $basexs = "$base.xs";
    my $basepm = "$base.pm";
    my $basec  = "$base$CSuffix";
    my $extdir = ".";
    if ( $dir =~ m:^ext\\(.+): ) {
        $extdir = $1;
    }
    elsif ( $dir ne "." ) {
        $extdir = $dir;
    }
    my $extdirdir  = dirname($extdir);
    my $targetroot = "\\System\\Libs\\Perl\\$R_V_SV";
    write_bld_inf($base) if -f $basexs;

    my %src;
    $src{$basec}++;

    $extdirdir = $extdirdir eq "." ? "" : "$extdirdir\\";

    my $extdash = $ext; $extdash =~ s!\\!-!g;

    my %lst;
    $lst{"$UREL\\perl$VERSION-$extdash.dll"} =
      "$targetroot\\$ARM-symbian\\$base.dll"
      if -f $basexs;
    $lst{"$dir\\$base.pm"} = "$targetroot\\$extdirdir$base.pm"
      if -f $basepm && $base ne 'XSLoader';

    my %incdir;
    my $ran_PL;
    if ( -d 'lib' ) {
        use File::Find;
        my @found;
        find( sub { push @found, $File::Find::name if -f $_ }, 'lib' );
        for my $found (@found) {
	    next if $found =~ /\.bak$/i; # Zlib
            my ($short) = ( $found =~ m/^lib.(.+)/ );
            $short =~ s!/!\\!g;
            $found =~ s!/!\\!g;
            $lst{"$dir\\$found"} = "$targetroot\\$short";
        }
    }
    if ( my @pm = glob("*.pm */*.pm") ) {
        for my $pm (@pm) {
            next if $pm =~ m:^t/:;
            $pm =~ s:/:\\:g;
            $lst{"$dir\\$pm"} = "$targetroot\\$extdirdir$pm";
        }
    }
    if ( my @c = glob("*.c *.cpp */*.c */*.cpp") ) {
	map { s:^zlib-src/:: } @c if $ext eq 'ext\Compress\Raw\Zlib';
        for my $c (@c) {
            $c =~ s:/:\\:g;
            $src{$c}++;
        }
    }
    if ( my @h = glob("*.h */*.h") ) {
        map { s:^zlib-src/:: } @h if $ext eq 'ext\Compress\Raw\Zlib';
        for my $h (@h) {
            $h =~ s:/:\\:g;
            $h = dirname($h);
            $incdir{"$dir\\$h"}++ unless $h eq ".";
        }
    }
    if ( exists $EXTCFG{$ext} ) {
        for my $cfg ( @{ $EXTCFG{$ext} } ) {
            if ( $cfg =~ /^([-+])?(.+\.(c|cpp|h))$/ ) {
                my $o = defined $1 ? $1 : '+';
                my $f = $2;
                $f =~ s:/:\\:g;
                for my $f ( glob($f) ) {
                    if ( $o eq '+' ) {
                        warn "$0: no source file $dir\\$f\n" unless -f $f;
                        $src{$f}++ unless $cfg =~ /\.h$/;
                        if ( $f =~ m:^(.+)\\[^\\]+$: ) {
                            $incdir{$1}++;
                        }
                    }
                    elsif ( $o eq '-' ) {
                        delete $src{$f};
                    }
                }
            }
            if ( $cfg =~ /^([-+])?(.+\.(pm|pl|inc))$/ ) {
                my $o = defined $1 ? $1 : '+';
                my $f = $2;
                $f =~ s:/:\\:g;
                for my $f ( glob($f) ) {
                    if ( $o eq '+' ) {
                        warn "$0: no Perl file $dir\\$f\n" unless -f $f;
                        $lst{"$dir\\$f"} = "$targetroot\\$extdir\\$f";
                    }
                    elsif ( $o eq '-' ) {
                        delete $lst{"$dir\\$f"};
                    }
                }
            }
            if ( $cfg eq 'CONST' && !$ran_PL++ ) {
                run_PL( "Makefile.PL", $dir, "const-xs.inc" );
            }
        }
    }
    unless ( $ran_PL++ ) {
        run_PL( "Makefile.PL", $dir ) if -f "Makefile.PL";
    }
    if ( $dir eq "ext\\Errno" ) {
        run_PL( "Errno_pm.PL", $dir, "Errno.pm" );
        $lst{"$dir\\Errno.pm"} = "$targetroot\\Errno.pm";
    }
    elsif ( $dir eq "ext\\Devel\\PPPort" ) {
        run_PL( "ppport_h.PL", $dir, "ppport.h" );
    }
    elsif ( $dir eq "ext\\DynaLoader" ) {
        run_PL( "XSLoader_pm.PL", $dir, "XSLoader.pm" );
        $lst{"ext\\DynaLoader\\XSLoader.pm"} = "$targetroot\\XSLoader.pm";
    }
    elsif ( $dir eq "ext\\Encode" ) {
        system_echo("perl bin\\enc2xs -Q -O -o def_t.c -f def_t.fnm") == 0
          or &restore_config and die "$0: running enc2xs failed: $!\n";
    }

    my @lst = sort keys %lst;

    read_mmp( \%CONF, "_init.mmp" );
    read_mmp( \%CONF, "$base.mmp" );

    if ( -f $basexs ) {
        my %MM;    # MakeMaker results
        my @MM = qw(VERSION XS_VERSION);
        if ( -f "Makefile" ) {
            print "\tReading MakeMaker Makefile...\n";
            if ( open( MAKEFILE, "Makefile" ) ) {
                while (<MAKEFILE>) {
                    for my $m (@MM) {
                        if (m!^$m = (.+)!) {
                            $MM{$m} = $1;
                            print "\t$m = $1\n";
                        }
                    }
                }
                close(MAKEFILE);
            }
            else {
                warn "$0: Makefile: $!";
            }
            print "\tDeleting MakeMaker Makefile.\n";
            unlink("Makefile");
        }

        unlink($basec);
        print "\t$basec\n";
        if ( defined $CONF{EXTVERSION} ) {
            my $EXTVERSION = $CONF{EXTVERSION};
            print "\tUsing $EXTVERSION for version...\n";
            $MM{VERSION} = $MM{XS_VERSION} = $EXTVERSION;
        }
        (&restore_config and die "$0: VERSION or XS_VERSION undefined\n")
          unless defined $MM{VERSION} && defined $MM{XS_VERSION};
        if ( open( BASE_C, ">$basec" ) ) {
            print BASE_C <<__EOF__;
#ifndef VERSION
#define VERSION "$MM{VERSION}"
#endif
#ifndef XS_VERSION
#define XS_VERSION "$MM{XS_VERSION}"
#endif
__EOF__
            close(BASE_C);
        }
        else {
            warn "$0: $basec: $!";
        }
        unless (
            system_echo(
"perl -I$BUILDROOT\\lib -I$PERLSDK\\lib $extu\\xsubpp -csuffix .cpp -typemap $extu\\typemap -noprototypes $basexs >> $basec"
            ) == 0
            && -s $basec
          )
        {
            &restore_config;
            die "$0: perl xsubpp failed: $!\n";
        }

        print "\t_init.c\n";
        open( _INIT_C, ">_init.c" )
            or &restore_config and die "$!: _init.c: $!\n";
        print _INIT_C <<__EOF__;
    #include "EXTERN.h"
    #include "perl.h"
    EXPORT_C void _init(void *handle) {
    }
__EOF__
        close(_INIT_C);

        my @src = ( "_init.c", sort keys %src );

        if ( $base eq "Encode" ) {    # Currently unused.
            for my $submf ( glob("*/Makefile") ) {
                my $d = dirname($submf);
                print "Configuring Encode::$d...\n";
                if ( open( SUBMF, $submf ) ) {
                    if ( update_dir($d) ) {
                        my @subsrc;
                        while (<SUBMF>) {
                            next if 1 .. /postamble/;
                            if (m!^(\w+_t)\.c : !) {
                                system_echo(
                                    "perl ..\\bin\\enc2xs -Q -o $1.c -f $1.fnm")
                                  == 0
                                  or warn "$0: enc2xs: $!\n";
                                push @subsrc, "$1.c";
                            }
                        }
                        close(SUBMF);
                        unlink($submf);
                        my $subbase = $d;
                        $subbase =~ s!/!::!g;
                        write_mmp( $ext, $subbase, ["..\\Encode"], "$subbase.c",
                            @subsrc );
                        write_makefile( $subbase, $build );
                        write_bld_inf($subbase);

                        unless (
                            system_echo(
"perl -I$BUILDROOT\\lib ..\\$extu\\xsubpp -csuffix .cpp -typemap ..\\$extu\\typemap -noprototypes $subbase.xs > $subbase.c"
                            ) == 0
                            && -s "$subbase.c"
                          )
                        {
                            &restore_config;
                            die "$0: perl xsubpp failed: $!\n";
                        }
                        update_dir("..");
                    }
                    else {
                        warn "$0: chdir $d: $!\n";
                    }
                }
                else {
                    warn "$0: $submf: $!";
                }
            }
            print "Configuring Encode...\n";
        }

        write_mmp( $ext, $base, [ keys %incdir ], @src );
        write_makefile( $base, $build );
    }
    &restore_config;

    my $lstname = $ext;
    $lstname =~ s:^ext\\::;
    $lstname =~ s:\\:-:g;
    print "\t$lstname.lst\n";
    my $lstout =
      $CoreBuild ? "$BUILDROOT/symbian/$lstname.lst" : "$BUILDROOT/$lstname.lst";
    if ( open( my $lst, ">$lstout" ) ) {
        for my $f (@lst) { print $lst qq["$f"-"!:$lst{$f}"\n] }
        close($lst);
    }
    else {
        die "$0: $lstout: $!\n";
    }
    update_dir($BUILDROOT);
}

sub update_cwd {
    $CWD = getcwd();
    $CWD =~ s!^[A-Z]:!!i;
    $CWD =~ s!/!\\!g;
}

if (grep /^(Compress::Raw::Zlib|Cwd|Data::Dumper|Digest::SHA|Sys::Syslog|Time::HiRes)$/, @ARGV) {
    &patch_config;
    system_echo("perl -I$BUILDROOT\\lib -I$PERLSDK\\lib $BUILDROOT\\mkppport");
    &restore_config;
}

for my $ext (@ARGV) {

    $ext =~ s!::!\\!g;
    my $extdash = $ext =~ /ext\\/ ? $ext : "ext\\$ext"; $extdash =~ s!\\!-!g;
    $ext =~ s!/!\\!g;

    my $cfg;

    $cfg = $2 if $ext =~ s/(.+?),(.+)/$1/;

    my $dir;

    unless ( -e $ext ) {
        if ( $ext =~ /\.xs$/ && !-f $ext ) {
            if ( -f "ext\\$ext" ) {
                $ext = "ext\\$ext";
                $dir = dirname($ext);
            }
        }
        elsif ( !-d $ext ) {
            if ( -d "ext\\$ext" ) {
                $ext = "ext\\$ext";
                $dir = $ext;
            }
        }
        $dir = "." unless defined $dir;
    }
    else {
        if ( $ext =~ /\.xs$/ && -f $ext ) {
            $ext = dirname($ext);
            $dir = $ext;
        }
        elsif ( -d $ext ) {
            $dir = $ext;
        }
    }

    if ( $ext eq "XSLoader" ) {
        $ext = "ext\\XSLoader";
    }
    if ( $ext eq "ext\\XSLoader" ) {
        $dir = "ext\\DynaLoader";
    }

    $EXTCFG{$ext} = [ split( /,/, $cfg ) ] if defined $cfg;

    die "$0: no lib\\Config.pm\n"
      if $CoreBuild && $Build && !-f "lib\\Config.pm";

    if ($CoreBuild) {
        open( my $cfg, "symbian/install.cfg" )
          or die "$0: symbian/install.cfg: $!\n";
        my $extdir = $dir;
        $extdir =~ s:^ext\\::;
        while (<$cfg>) {
            next unless /^ext\s+(.+)/;
            chomp;
            my $ext = $1;
            my @ext = split( ' ', $ext );
            $EXTCFG{"ext\\$ext[0]"} = [@ext];
        }
        close($cfg);
    }

    if ( $Config || $Build ) {
        xsconfig( $ext, $dir ) or die "$0: xsconfig '$ext' failed\n";
        next if $Config;
    }

    if ($dir eq ".") {
	warn "$0: No directory for $ext, skipping...\n";
	next;
    }

    my $chdir = $ext eq "ext\\XSLoader" ? "ext\\DynaLoader" : $dir;
    die "$0: no directory '$chdir'\n" unless -d $chdir;
    update_dir($chdir) or die "$0: chdir '$chdir' failed: $!\n";

    my %CONF;

    my @ext   = split( /\\/, $ext );
    my $base  = $ext[-1];

    if ( $Clean || $DistClean ) {
        print "Cleaning $ext...\n";
        unlink("bld.inf");
        unlink("$base.mmp");
        unlink("_init.c");
        unlink("const-c.inc");
        unlink("const-xs.inc");
        rmdir("..\\bmarm");
    }

    if ( $Build && $ext ne "ext\\XSLoader" && $ext ne "ext\\Errno" ) {

     # We compile the extension three (3) times.
     # (1) Only the _init.c to get _init() as the ordinal 1 function in the DLL.
     # (2) With the rest and the _init.c to get ordinals for the rest.
     # (3) With an updated _init.c that carries the symbols from step (2).

        system_echo("make clean");
        system_echo("make defrost") == 0 or warn "$0: make defrost failed\n";

        my @TARGET;

        push @TARGET, 'sis' if $Sis;

        # Compile #1.
        # Hide all but the _init.c.
        print "\n*** $ext - Compile 1 of 3.\n\n";
	print "(patching $base.mmp)\n";
        system(
"perl -pi.bak -e \"s:^SOURCE\\s+_init.c:SOURCE\\t_init.c // :\" $base.mmp"
        );
	system_echo("bldmake bldfiles");
        system_echo("make @TARGET") == 0 or die "$0: make #1 failed\n";

        # Compile #2.
        # Reveal the rest again.
        print "\n*** $ext - Compile 2 of 3.\n\n";
	print "(patching $base.mmp)\n";
        system(
"perl -pi.bak -e \"s:^SOURCE\\t_init.c // :SOURCE\\t_init.c :\" $base.mmp"
        );
        system_echo("make @TARGET") == 0 or die "$0: make #2 failed\n";
        unlink("$base.mmp.bak");

        open( _INIT_C, ">_init.c" ) or die "$0: _init.c: $!\n";
        print _INIT_C <<'__EOF__';
#include "EXTERN.h"
#include "perl.h"

/* This is a different but matching definition from in dl_symbian.xs. */
typedef struct {
    void*	handle;
    int		error;
    HV*		symbols;
} PerlSymbianLibHandle;

EXPORT_C void _init(void* handle) {
__EOF__

        my %symbol;
	my $def;
	my $basef;
        for my $f ("$SYMBIAN_ROOT\\Epoc32\\Build$CWD\\$base\\WINS\\perl$VERSION-$extdash.def",
		   "..\\BMARM\\perl$VERSION-${extdash}u.def") {
	    print "\t($f - ";
	    if ( open( $def, $f ) ) {
		print "OK)\n";
	        $basef = $f;
		last;
	    } else {
		print "no)\n";
	    }
	}
	unless (defined $basef) {
	    die "$0: failed to find .def for $base\n";
	}
        while (<$def>) {
            next while 1 .. /^EXPORTS/;
            if (/^\s*(\w+) \@ (\d+) /) {
                $symbol{$1} = $2;
            }
        }
        close($def);
 
        my @symbol = sort keys %symbol;
        if (@symbol) {
            print _INIT_C <<'__EOF__';
    dTHX;
    PerlSymbianLibHandle* h = (PerlSymbianLibHandle*)handle;
    if (!h->symbols)
        h->symbols = newHV();
    if (h->symbols) {
__EOF__
            for my $sym (@symbol) {
                my $len = length($sym);
                print _INIT_C <<__EOF__;
        hv_store(h->symbols, "$sym", $len, newSViv($symbol{$sym}), 0);
__EOF__
            }
        }
        else {
            die "$0: $basef: no exports found\n";
        }

        print _INIT_C <<'__EOF__';
    }
}
__EOF__
        close(_INIT_C);

        # Compile #3.  This is for real.
        print "\n*** $ext - Compile 3 of 3.\n\n";
        system_echo("make @TARGET") == 0 or die "$0: make #3 failed\n";

    }
    elsif ( $Clean || $DistClean ) {
        if ( $ext eq "ext\\Errno" ) {
            unlink( "Errno.pm", "Makefile" );
        }
        else {
            if ( -f "Makefile" ) {
                if ($Clean) {
                    system_echo("make clean") == 0 or die "$0: make clean failed\n";
                }
                elsif ($DistClean) {
                    system_echo("make distclean") == 0
                      or die "$0: make distclean failed\n";
                }
            }
	    if ( $ext eq "ext\\Compress\\Raw\\Zlib" ) {
		my @bak;
		find( sub { push @bak, $File::Find::name if /\.bak$/ }, "." );
		unlink(@bak) if @bak;
		my @src;
		find( sub { push @src, $_ if -f $_ }, "zlib-src" );
		unlink(@src) if @src;
		unlink("constants.xs");
	    }
            if ( $ext eq "ext\\Devel\\PPPort" ) {
                unlink("ppport.h");
            }
        }
	my @D = glob("../BMARM/*.def ../BWINS/*.def");
	unlink(@D) if @D;
        my @B = glob("ext/BWINS ext/BMARM ext/*/BWINS ext/*/BMARM Makefile");
        rmdir(@B) if @B;
    }

    update_dir($BUILDROOT);

}    # for my $ext

exit(0);

