package BaseIncDoubleExtender;

BEGIN { ::ok( $INC[-1] ne '.', 'no trailing dot in @INC during module load from base' ) }

use lib 't/lib/blahdeblah';

push @INC, 't/lib/on-end';

1;
