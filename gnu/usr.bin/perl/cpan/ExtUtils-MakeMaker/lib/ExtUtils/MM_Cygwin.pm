package ExtUtils::MM_Cygwin;

use strict;

use ExtUtils::MakeMaker::Config;
use File::Spec;

require ExtUtils::MM_Unix;
require ExtUtils::MM_Win32;
our @ISA = qw( ExtUtils::MM_Unix );

our $VERSION = '6.98';


=head1 NAME

ExtUtils::MM_Cygwin - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_Cygwin; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided there.

=over 4

=item os_flavor

We're Unix and Cygwin.

=cut

sub os_flavor {
    return('Unix', 'Cygwin');
}

=item cflags

if configured for dynamic loading, triggers #define EXT in EXTERN.h

=cut

sub cflags {
    my($self,$libperl)=@_;
    return $self->{CFLAGS} if $self->{CFLAGS};
    return '' unless $self->needs_linking();

    my $base = $self->SUPER::cflags($libperl);
    foreach (split /\n/, $base) {
        /^(\S*)\s*=\s*(\S*)$/ and $self->{$1} = $2;
    };
    $self->{CCFLAGS} .= " -DUSEIMPORTLIB" if ($Config{useshrplib} eq 'true');

    return $self->{CFLAGS} = qq{
CCFLAGS = $self->{CCFLAGS}
OPTIMIZE = $self->{OPTIMIZE}
PERLTYPE = $self->{PERLTYPE}
};

}


=item replace_manpage_separator

replaces strings '::' with '.' in MAN*POD man page names

=cut

sub replace_manpage_separator {
    my($self, $man) = @_;
    $man =~ s{/+}{.}g;
    return $man;
}

=item init_linker

points to libperl.a

=cut

sub init_linker {
    my $self = shift;

    if ($Config{useshrplib} eq 'true') {
        my $libperl = '$(PERL_INC)' .'/'. "$Config{libperl}";
        if( $] >= 5.006002 ) {
            $libperl =~ s/a$/dll.a/;
        }
        $self->{PERL_ARCHIVE} = $libperl;
    } else {
        $self->{PERL_ARCHIVE} =
          '$(PERL_INC)' .'/'. ("$Config{libperl}" or "libperl.a");
    }

    $self->{PERL_ARCHIVE_AFTER} ||= '';
    $self->{EXPORT_LIST}  ||= '';
}

=item maybe_command

Determine whether a file is native to Cygwin by checking whether it
resides inside the Cygwin installation (using Windows paths). If so,
use C<ExtUtils::MM_Unix> to determine if it may be a command.
Otherwise use the tests from C<ExtUtils::MM_Win32>.

=cut

sub maybe_command {
    my ($self, $file) = @_;

    my $cygpath = Cygwin::posix_to_win_path('/', 1);
    my $filepath = Cygwin::posix_to_win_path($file, 1);

    return (substr($filepath,0,length($cygpath)) eq $cygpath)
    ? $self->SUPER::maybe_command($file) # Unix
    : ExtUtils::MM_Win32->maybe_command($file); # Win32
}

=item dynamic_lib

Use the default to produce the *.dll's.
But for new archdir dll's use the same rebase address if the old exists.

=cut

sub dynamic_lib {
    my($self, %attribs) = @_;
    my $s = ExtUtils::MM_Unix::dynamic_lib($self, %attribs);
    my $ori = "$self->{INSTALLARCHLIB}/auto/$self->{FULLEXT}/$self->{BASEEXT}.$self->{DLEXT}";
    if (-e $ori) {
        my $imagebase = `/bin/objdump -p $ori | /bin/grep ImageBase | /bin/cut -c12-`;
        chomp $imagebase;
        if ($imagebase gt "40000000") {
            my $LDDLFLAGS = $self->{LDDLFLAGS};
            $LDDLFLAGS =~ s/-Wl,--enable-auto-image-base/-Wl,--image-base=0x$imagebase/;
            $s =~ s/ \$\(LDDLFLAGS\) / $LDDLFLAGS /m;
        }
    }
    $s;
}

=item all_target

Build man pages, too

=cut

sub all_target {
    ExtUtils::MM_Unix::all_target(shift);
}

=back

=cut

1;
