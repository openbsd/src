#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib/');
    }
    else {
        unshift @INC, 't/lib/';
    }
}
chdir 't';

BEGIN {
    $Testfile = 'testfile.foo';
}

BEGIN {
    1 while unlink $Testfile, 'newfile';
    # forcibly remove ecmddir/temp2, but don't import mkpath
    use File::Path ();
    File::Path::rmtree( 'ecmddir' );
}

BEGIN {
    use Test::More tests => 26;
    use File::Spec;
}

BEGIN {
    # bad neighbor, but test_f() uses exit()
        *CORE::GLOBAL::exit = '';   # quiet 'only once' warning.
    *CORE::GLOBAL::exit = sub { return @_ };
    use_ok( 'ExtUtils::Command' );
}

{
    # concatenate this file with itself
    # be extra careful the regex doesn't match itself
    use TieOut;
    my $out = tie *STDOUT, 'TieOut';
    my $self = $0;
    unless (-f $self) {
        my ($vol, $dirs, $file) = File::Spec->splitpath($self);
        my @dirs = File::Spec->splitdir($dirs);
        unshift(@dirs, File::Spec->updir);
        $dirs = File::Spec->catdir(@dirs);
        $self = File::Spec->catpath($vol, $dirs, $file);
    }
    @ARGV = ($self, $self);

    cat();
    is( scalar( $$out =~ s/use_ok\( 'ExtUtils::Command'//g), 2, 
        'concatenation worked' );

    # the truth value here is reversed -- Perl true is C false
    @ARGV = ( $Testfile );
    ok( test_f(), 'testing non-existent file' );

    @ARGV = ( $Testfile );
    cmp_ok( ! test_f(), '==', (-f $Testfile), 'testing non-existent file' );

    # these are destructive, have to keep setting @ARGV
    @ARGV = ( $Testfile );
    touch();

    @ARGV = ( $Testfile );
    ok( test_f(), 'now creating that file' );

    @ARGV = ( $Testfile );
    ok( -e $ARGV[0], 'created!' );

    my ($now) = time;
    utime ($now, $now, $ARGV[0]);
    sleep 2;

    # Just checking modify time stamp, access time stamp is set
    # to the beginning of the day in Win95.
    # There's a small chance of a 1 second flutter here.
    my $stamp = (stat($ARGV[0]))[9];
    cmp_ok( abs($now - $stamp), '<=', 1, 'checking modify time stamp' ) ||
      diag "mtime == $stamp, should be $now";

    @ARGV = qw(newfile);
    touch();

    my $new_stamp = (stat('newfile'))[9];
    cmp_ok( abs($new_stamp - $stamp), '>=', 2,  'newer file created' );

    @ARGV = ('newfile', $Testfile);
    eqtime();

    $stamp = (stat($Testfile))[9];
    cmp_ok( abs($new_stamp - $stamp), '<=', 1, 'eqtime' );

    # eqtime use to clear the contents of the file being equalized!
    open(FILE, ">>$Testfile") || die $!;
    print FILE "Foo";
    close FILE;

    @ARGV = ('newfile', $Testfile);
    eqtime();
    ok( -s $Testfile, "eqtime doesn't clear the file being equalized" );

    SKIP: {
        if ($^O eq 'amigaos' || $^O eq 'os2' || $^O eq 'MSWin32' ||
            $^O eq 'NetWare' || $^O eq 'dos' || $^O eq 'cygwin'  ||
            $^O eq 'MacOS'
           ) {
            skip( "different file permission semantics on $^O", 3);
        }

        # change a file to execute-only
        @ARGV = ( '0100', $Testfile );
        ExtUtils::Command::chmod();

        is( ((stat($Testfile))[2] & 07777) & 0700,
            0100, 'change a file to execute-only' );

        # change a file to read-only
        @ARGV = ( '0400', $Testfile );
        ExtUtils::Command::chmod();

        is( ((stat($Testfile))[2] & 07777) & 0700,
            ($^O eq 'vos' ? 0500 : 0400), 'change a file to read-only' );

        # change a file to write-only
        @ARGV = ( '0200', $Testfile );
        ExtUtils::Command::chmod();

        is( ((stat($Testfile))[2] & 07777) & 0700,
            ($^O eq 'vos' ? 0700 : 0200), 'change a file to write-only' );
    }

    # change a file to read-write
    @ARGV = ( '0600', $Testfile );
    ExtUtils::Command::chmod();

    is( ((stat($Testfile))[2] & 07777) & 0700,
        ($^O eq 'vos' ? 0700 : 0600), 'change a file to read-write' );

    # mkpath
    @ARGV = ( File::Spec->join( 'ecmddir', 'temp2' ) );
    ok( ! -e $ARGV[0], 'temp directory not there yet' );

    mkpath();
    ok( -e $ARGV[0], 'temp directory created' );

    # copy a file to a nested subdirectory
    unshift @ARGV, $Testfile;
    cp();

    ok( -e File::Spec->join( 'ecmddir', 'temp2', $Testfile ), 'copied okay' );

    # cp should croak if destination isn't directory (not a great warning)
    @ARGV = ( $Testfile ) x 3;
    eval { cp() };

    like( $@, qr/Too many arguments/, 'cp croaks on error' );

    # move a file to a subdirectory
    @ARGV = ( $Testfile, 'ecmddir' );
    mv();

    ok( ! -e $Testfile, 'moved file away' );
    ok( -e File::Spec->join( 'ecmddir', $Testfile ), 'file in new location' );

    # mv should also croak with the same wacky warning
    @ARGV = ( $Testfile ) x 3;

    eval { mv() };
    like( $@, qr/Too many arguments/, 'mv croaks on error' );

    # Test expand_wildcards()
    {
        my $file = $Testfile;
        @ARGV = ();
        chdir 'ecmddir';

        # % means 'match one character' on VMS.  Everything else is ?
        my $match_char = $^O eq 'VMS' ? '%' : '?';
        ($ARGV[0] = $file) =~ s/.\z/$match_char/;

        # this should find the file
        ExtUtils::Command::expand_wildcards();

        is_deeply( \@ARGV, [$file], 'expanded wildcard ? successfully' );

        # try it with the asterisk now
        ($ARGV[0] = $file) =~ s/.{3}\z/\*/;
        ExtUtils::Command::expand_wildcards();

        is_deeply( \@ARGV, [$file], 'expanded wildcard * successfully' );

        chdir File::Spec->updir;
    }

    # remove some files
    my @files = @ARGV = ( File::Spec->catfile( 'ecmddir', $Testfile ),
    File::Spec->catfile( 'ecmddir', 'temp2', $Testfile ) );
    rm_f();

    ok( ! -e $_, "removed $_ successfully" ) for (@ARGV);

    # rm_f dir
    @ARGV = my $dir = File::Spec->catfile( 'ecmddir' );
    rm_rf();
    ok( ! -e $dir, "removed $dir successfully" );
}

END {
    1 while unlink $Testfile, 'newfile';
    File::Path::rmtree( 'ecmddir' );
}
