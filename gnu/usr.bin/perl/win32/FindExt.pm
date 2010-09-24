package FindExt;

our $VERSION = '1.02';

use strict;
use warnings;

my $no = join('|',qw(GDBM_File ODBM_File NDBM_File DB_File
                     VMS VMS-DCLsym VMS-Stdio Sys-Syslog IPC-SysV I18N-Langinfo));
$no = qr/^(?:$no)$/i;

my %ext;
my %static;

sub set_static_extensions {
    # adjust results of scan_ext, and also save
    # statics in case scan_ext hasn't been called yet.
    # if '*' is passed then all XS extensions are static
    # (with possible exclusions)
    %static = ();
    my @list = @_;
    if ($_[0] eq '*') {
	my %excl = map {$_=>1} map {m/^!(.*)$/} @_[1 .. $#_];
	@list = grep {!exists $excl{$_}} keys %ext;
    }
    for (@list) {
        $static{$_} = 1;
        $ext{$_} = 'static' if $ext{$_} && $ext{$_} eq 'dynamic';
    }
}

sub scan_ext
{
    my $dir  = shift;
    find_ext("$dir/");
    extensions();
}

sub _ext_eq {
    my $key = shift;
    sub {
        sort grep $ext{$_} eq $key, keys %ext;
    }
}

*dynamic_ext = _ext_eq('dynamic');
*static_ext = _ext_eq('static');
*nonxs_ext = _ext_eq('nonxs');

sub _ext_ne {
    my $key = shift;
    sub {
        sort grep $ext{$_} ne $key, keys %ext;
    }
}

*extensions = _ext_ne('known');
# faithfully copy Configure in not including nonxs extensions for the nonce
*known_extensions = _ext_ne('nonxs');

sub is_static
{
 return $ext{$_[0]} eq 'static'
}

sub has_xs_or_c {
    my $dir = shift;
    opendir my $dh, $dir or die "opendir $dir: $!";
    while (defined (my $item = readdir $dh)) {
        return 1 if $item =~ /\.xs$/;
        return 1 if $item =~ /\.c$/;
    }
    return 0;
}

# Function to find available extensions, ignoring DynaLoader
sub find_ext
{
    my $ext_dir = shift;
    opendir my $dh, "$ext_dir";
    while (defined (my $item = readdir $dh)) {
        next if $item =~ /^\.\.?$/;
        next if $item eq "DynaLoader";
        next unless -d "$ext_dir$item";
        my $this_ext = $item;
        my $leaf = $item;

        $this_ext =~ s!-!/!g;
        $leaf =~ s/.*-//;

	# Temporary hack to cope with smokers that are not clearing directories:
        next if $ext{$this_ext};

        if (has_xs_or_c("$ext_dir$item")) {
            $ext{$this_ext} = $static{$this_ext} ? 'static' : 'dynamic';
        } else {
            $ext{$this_ext} = 'nonxs';
        }
        $ext{$this_ext} = 'known' if $ext{$this_ext} && $item =~ $no;
    }
}

1;
# Local variables:
# cperl-indent-level: 4
# indent-tabs-mode: nil
# End:
#
# ex: set ts=8 sts=4 sw=4 et:
