#
# $Id: Makefile.PL,v 2.8 2009/07/08 13:34:15 dankogai Exp $
#
use 5.007003;
use strict;
use warnings;
use ExtUtils::MakeMaker;
use File::Spec;

# Just for sure :)
my %ARGV = map { my @r = split /=/,$_; defined $r[1] or $r[1]=1; @r } @ARGV;
$ARGV{DEBUG} and warn "$_ => $ARGV{$_}\n" for keys  %ARGV;
$ENV{PERL_CORE} ||= $ARGV{PERL_CORE} if $ARGV{PERL_CORE};

my %tables = 
    (
     def_t => ['ascii.ucm',
           '8859-1.ucm',
           'null.ucm',
           'ctrl.ucm',
           ]
     );

my @exe_files = qw(bin/enc2xs
           bin/piconv
           );
my @more_exe_files = qw(
            unidump
            );
my @pmlibdirs = qw(lib Encode);

$ARGV{MORE_SCRIPTS} and push @exe_files, @more_exe_files;
$ARGV{INSTALL_UCM}   and push @pmlibdirs, "ucm";

WriteMakefile(
    NAME         => "Encode",
    EXE_FILES    => \@exe_files,
    VERSION_FROM => 'Encode.pm',
    OBJECT       => '$(O_FILES)',
    'dist'       => {
        COMPRESS     => 'gzip -9f',
        SUFFIX       => 'gz',
        DIST_DEFAULT => 'all tardist',
    },
    INC         => '-I' . File::Spec->catfile( '.', 'Encode' ),
    PMLIBDIRS   => \@pmlibdirs,
    INSTALLDIRS => 'perl',
);

package MY;


sub post_initialize
{
    my ($self) = @_;
    my %o;
    # Find existing O_FILES
    foreach my $f (@{$self->{'O_FILES'}})
    {
    $o{$f} = 1;
    }
    my $x = $self->{'OBJ_EXT'};
    # Add the table O_FILES
    foreach my $e (keys %tables)
    {
    $o{$e.$x} = 1;
    }
    # Trick case-blind filesystems.
    delete $o{'encode'.$x};
    $o{'Encode'.$x} = 1;
    # Reset the variable
    $self->{'O_FILES'} = [sort keys %o];
    my @files;
    foreach my $table (keys %tables)
    {
    foreach my $ext (qw($(OBJ_EXT) .c .h .exh .fnm))
    {
    push (@files,$table.$ext);
    }
    $self->{SOURCE} .= " $table.c"
    if $^O eq 'MacOS' && $self->{SOURCE} !~ /\b$table\.c\b/;
}
$self->{'clean'}{'FILES'} .= join(' ',@files);
return '';
}

sub postamble
{
    my $self = shift;
    my $dir  = $self->catdir($self->curdir,'ucm');
    my $str  = "# Encode\$(OBJ_EXT) does not depend on .c files directly\n";
    $str  .= "# (except Encode.c), but on .h and .exh files written by enc2xs\n";
    $str  .= $^O eq 'MacOS' ? 'Encode.c.{$(MACPERL_BUILD_EXT_STATIC)}.o :' : 'Encode$(OBJ_EXT) :';
    $str  .= ' Encode.c';
    foreach my $table (keys %tables)
    {
    $str .= " $table.c";
    }
    $str .= "\n\n";
    foreach my $table (keys %tables)
    {
    my $numlines = 1;
    my $lengthsofar = length($str);
    my $continuator = '';
    my $enc2xs = $self->catfile('bin', 'enc2xs');
    $str .= "$table.c : $enc2xs Makefile.PL";
    foreach my $file (@{$tables{$table}})
    {
        $str .= $continuator.' '.$self->catfile($dir,$file);
        if ( length($str)-$lengthsofar > 128*$numlines )
        {
        $continuator .= " \\\n\t";
        $numlines++;
        } else {
        $continuator = '';
        }
    }
    my $plib   = $self->{PERL_CORE} ? '"-I$(PERL_LIB)"' : '';
    $plib .= " -MCross=$::Cross::platform" if defined $::Cross::platform;
    my $ucopts = '-"Q" -"O"';
    $str .=
        qq{\n\t\$(PERL) $plib $enc2xs $ucopts -o \$\@ -f $table.fnm\n\n};
    open (FILELIST, ">$table.fnm")
        || die "Could not open $table.fnm: $!";
    foreach my $file (@{$tables{$table}})
    {
        print FILELIST $self->catfile($dir,$file) . "\n";
    }
    close(FILELIST);
    }
    return $str;
}
