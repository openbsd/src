use strict;
use warnings;

use Config;

my $template;
{	# keep all the code in an external template to keep it easy to update
	local $/;
	open my $FROM, '<', '__Storable__.pm' or die $!;
	$template = <$FROM>;
	close $FROM or die $!;
}

sub CAN_FLOCK {
	return
		$Config{'d_flock'} ||
		$Config{'d_fcntl_can_lock'} ||
		$Config{'d_lockf'}
		? 1 : 0;
}

my $CAN_FLOCK = CAN_FLOCK();

# populate the sub and preserve it if used outside
$template =~ s{^sub CAN_FLOCK;.*$}{sub CAN_FLOCK { ${CAN_FLOCK} } # computed by Storable.pm.PL}m;
# alternatively we could remove the sub
#$template =~ s{^sub CAN_FLOCK;.*$}{}m;
# replace local function calls to hardcoded value
$template =~ s{&CAN_FLOCK}{${CAN_FLOCK}}g;

{
	open my $OUT, '>', 'Storable.pm' or die $!;
	print {$OUT} $template or die $!;
	close $OUT or die $!;
}
