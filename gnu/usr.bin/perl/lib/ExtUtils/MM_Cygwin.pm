package ExtUtils::MM_Cygwin;

use strict;
use vars qw($VERSION @ISA);

use Config;
use File::Spec;

require ExtUtils::MM_Any;
require ExtUtils::MM_Unix;
@ISA = qw( ExtUtils::MM_Any ExtUtils::MM_Unix );

$VERSION = 1.04;

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

sub manifypods {
    my($self, %attribs) = @_;
    return "\nmanifypods : pure_all\n\t$self->{NOECHO}\$(NOOP)\n" unless
        %{$self->{MAN3PODS}} or %{$self->{MAN1PODS}};
    my($dist);
    my($pod2man_exe);
    if (defined $self->{PERL_SRC}) {
        $pod2man_exe = File::Spec->catfile($self->{PERL_SRC},'pod','pod2man');
    } else {
        $pod2man_exe = File::Spec->catfile($Config{scriptdirexp},'pod2man');
    }
    unless ($self->perl_script($pod2man_exe)) {
        # No pod2man but some MAN3PODS to be installed
        print <<END;

Warning: I could not locate your pod2man program. Please make sure,
         your pod2man program is in your PATH before you execute 'make'

END
        $pod2man_exe = "-S pod2man";
    }
    my(@m) = ();
    push @m,
qq[POD2MAN_EXE = $pod2man_exe\n],
qq[POD2MAN = \$(PERL) -we '%m=\@ARGV;for (keys %m){' \\\n],
q[-e 'next if -e $$m{$$_} && -M $$m{$$_} < -M $$_ && -M $$m{$$_} < -M "],
 $self->{MAKEFILE}, q[";' \\
-e 'print "Manifying $$m{$$_}\n"; $$m{$$_} =~ s/::/./g;' \\
-e 'system(qq[$(PERLRUN) $(POD2MAN_EXE) ].qq[$$_>$$m{$$_}])==0 or warn "Couldn\\047t install $$m{$$_}\n";' \\
-e 'chmod(oct($(PERM_RW))), $$m{$$_} or warn "chmod $(PERM_RW) $$m{$$_}: $$!\n";}'
];
    push @m, "\nmanifypods : pure_all ";
    push @m, join " \\\n\t", keys %{$self->{MAN1PODS}},
                             keys %{$self->{MAN3PODS}};

    push(@m,"\n");
    if (%{$self->{MAN1PODS}} || %{$self->{MAN3PODS}}) {
        grep { $self->{MAN1PODS}{$_} =~ s/::/./g } keys %{$self->{MAN1PODS}};
        grep { $self->{MAN3PODS}{$_} =~ s/::/./g } keys %{$self->{MAN3PODS}};
        push @m, "\t$self->{NOECHO}\$(POD2MAN) \\\n\t";
        push @m, join " \\\n\t", %{$self->{MAN1PODS}}, %{$self->{MAN3PODS}};
    }
    join('', @m);
}

sub perl_archive {
    if ($Config{useshrplib} eq 'true') {
        my $libperl = '$(PERL_INC)' .'/'. "$Config{libperl}";
        if( $] >= 5.007 ) {
            $libperl =~ s/a$/dll.a/;
        }
        return $libperl;
    } else {
        return '$(PERL_INC)' .'/'. ("$Config{libperl}" or "libperl.a");
    }
}

1;
__END__

=head1 NAME

ExtUtils::MM_Cygwin - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_Cygwin; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided there.

=over 4

=item canonpath

replaces backslashes with forward ones.  then acts as *nixish.

=item cflags

if configured for dynamic loading, triggers #define EXT in EXTERN.h

=item manifypods

replaces strings '::' with '.' in man page names

=item perl_archive

points to libperl.a

=back

=cut

