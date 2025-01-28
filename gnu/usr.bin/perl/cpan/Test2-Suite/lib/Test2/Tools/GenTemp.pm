package Test2::Tools::GenTemp;

use strict;
use warnings;

our $VERSION = '0.000162';

use File::Temp qw/tempdir/;
use File::Spec;

our @EXPORT = qw{gen_temp};
use base 'Exporter';

sub gen_temp {
    my %args = @_;

    my $tempdir_args = delete $args{'-tempdir'} || [CLEANUP => 1, TMPDIR => 1];

    my $tmp = tempdir(@$tempdir_args);

    gen_dir($tmp, \%args);

    return $tmp;
}

sub gen_dir {
    my ($dir, $content) = @_;

    for my $path (keys %$content) {
        my $fq = File::Spec->catfile($dir, $path);
        my $inside = $content->{$path};

        if (ref $inside) {
            # Subdirectory
            mkdir($fq) or die "Could not make dir '$fq': $!";
            gen_dir($fq, $inside);
        }
        else {
            open(my $fh, '>', $fq) or die "Could not open file '$fq' for writing: $!";
            print $fh $inside;
            close($fh);
        }
    }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::GenTemp - Tool for generating a populated temp directory.

=head1 DESCRIPTION

This exports a tool that helps you make a temporary directory, nested
directories and text files within.

=head1 SYNOPSIS

    use Test2::Tools::GenTemp qw/gen_temp/;

    my $dir = gen_temp(
        a_file => "Contents of a_file",
        a_dir  => {
            'a_file' => 'Contents of a_dir/afile',
            a_nested_dir => { ... },
        },
        ...
    );

    done_testing;

=head1 EXPORTS

All subs are exported by default.

=over 4

=item gen_temp(file => 'content', subdir => [ sub_dir_file => 'content', ...], ...)

=item gen_temp(-tempdir => \@TEMPDIR_ARGS, file => 'content', subdir => [ sub_dir_file => 'content', ...], ...)

This will generate a new temporary directory with all the files and subdirs you
specify, recursively. The initial temp directory is created using
C<File::Temp::tempdir()>, you may pass arguments to tempdir using the
C<< -tempdir => [...] >> argument.

=back

=head1 SOURCE

The source code repository for Test2-Suite can be found at
F<https://github.com/Test-More/Test2-Suite/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2018 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
