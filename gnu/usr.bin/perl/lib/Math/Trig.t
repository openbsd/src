#!./perl 

#
# Regression tests for the Math::Trig package
#
# The tests here are quite modest as the Math::Complex tests exercise
# these interfaces quite vigorously.
# 
# -- Jarkko Hietaniemi, April 1997

BEGIN {
    if ($ENV{PERL_CORE}) {
	chdir 't' if -d 't';
	@INC = '../lib';
    }
}

BEGIN {
    eval { require Test::More };
    if ($@) {
	# We are willing to lose testing in e.g. 5.00504.
	print "1..0 # No Test::More, skipping\n";
	exit(0);
    } else {
	import Test::More;
    }
}

plan(tests => 69);

use Math::Trig 1.03;

my $pip2 = pi / 2;

use strict;

use vars qw($x $y $z);

my $eps = 1e-11;

if ($^O eq 'unicos') { # See lib/Math/Complex.pm and t/lib/complex.t.
    $eps = 1e-10;
}

sub near ($$;$) {
    my $e = defined $_[2] ? $_[2] : $eps;
    my $d = $_[1] ? abs($_[0]/$_[1] - 1) : abs($_[0]);
    print "# near? $_[0] $_[1] : $d : $e\n";
    $_[1] ? ($d < $e) : abs($_[0]) < $e;
}

$x = 0.9;
ok(near(tan($x), sin($x) / cos($x)));

ok(near(sinh(2), 3.62686040784702));

ok(near(acsch(0.1), 2.99822295029797));

$x = asin(2);
is(ref $x, 'Math::Complex');

# avoid using Math::Complex here
$x =~ /^([^-]+)(-[^i]+)i$/;
($y, $z) = ($1, $2);
ok(near($y,  1.5707963267949));
ok(near($z, -1.31695789692482));

ok(near(deg2rad(90), pi/2));

ok(near(rad2deg(pi), 180));

use Math::Trig ':radial';

{
    my ($r,$t,$z) = cartesian_to_cylindrical(1,1,1);

    ok(near($r, sqrt(2)));
    ok(near($t, deg2rad(45)));
    ok(near($z, 1));

    ($x,$y,$z) = cylindrical_to_cartesian($r, $t, $z);

    ok(near($x, 1));
    ok(near($y, 1));
    ok(near($z, 1));

    ($r,$t,$z) = cartesian_to_cylindrical(1,1,0);

    ok(near($r, sqrt(2)));
    ok(near($t, deg2rad(45)));
    ok(near($z, 0));

    ($x,$y,$z) = cylindrical_to_cartesian($r, $t, $z);

    ok(near($x, 1));
    ok(near($y, 1));
    ok(near($z, 0));
}

{
    my ($r,$t,$f) = cartesian_to_spherical(1,1,1);

    ok(near($r, sqrt(3)));
    ok(near($t, deg2rad(45)));
    ok(near($f, atan2(sqrt(2), 1)));

    ($x,$y,$z) = spherical_to_cartesian($r, $t, $f);

    ok(near($x, 1));
    ok(near($y, 1));
    ok(near($z, 1));
       
    ($r,$t,$f) = cartesian_to_spherical(1,1,0);

    ok(near($r, sqrt(2)));
    ok(near($t, deg2rad(45)));
    ok(near($f, deg2rad(90)));

    ($x,$y,$z) = spherical_to_cartesian($r, $t, $f);

    ok(near($x, 1));
    ok(near($y, 1));
    ok(near($z, 0));
}

{
    my ($r,$t,$z) = cylindrical_to_spherical(spherical_to_cylindrical(1,1,1));

    ok(near($r, 1));
    ok(near($t, 1));
    ok(near($z, 1));

    ($r,$t,$z) = spherical_to_cylindrical(cylindrical_to_spherical(1,1,1));

    ok(near($r, 1));
    ok(near($t, 1));
    ok(near($z, 1));
}

{
    use Math::Trig 'great_circle_distance';

    ok(near(great_circle_distance(0, 0, 0, pi/2), pi/2));

    ok(near(great_circle_distance(0, 0, pi, pi), pi));

    # London to Tokyo.
    my @L = (deg2rad(-0.5), deg2rad(90 - 51.3));
    my @T = (deg2rad(139.8),deg2rad(90 - 35.7));

    my $km = great_circle_distance(@L, @T, 6378);

    ok(near($km, 9605.26637021388));
}

{
    my $R2D = 57.295779513082320876798154814169;

    sub frac { $_[0] - int($_[0]) }

    my $lotta_radians = deg2rad(1E+20, 1);
    ok(near($lotta_radians,  1E+20/$R2D));

    my $negat_degrees = rad2deg(-1E20, 1);
    ok(near($negat_degrees, -1E+20*$R2D));

    my $posit_degrees = rad2deg(-10000, 1);
    ok(near($posit_degrees, -10000*$R2D));
}

{
    use Math::Trig 'great_circle_direction';

    ok(near(great_circle_direction(0, 0, 0, pi/2), pi));

# Retired test: Relies on atan2(0, 0), which is not portable.
#	ok(near(great_circle_direction(0, 0, pi, pi), -pi()/2));

    my @London  = (deg2rad(  -0.167), deg2rad(90 - 51.3));
    my @Tokyo   = (deg2rad( 139.5),   deg2rad(90 - 35.7));
    my @Berlin  = (deg2rad ( 13.417), deg2rad(90 - 52.533));
    my @Paris   = (deg2rad (  2.333), deg2rad(90 - 48.867));

    ok(near(rad2deg(great_circle_direction(@London, @Tokyo)),
	    31.791945393073));

    ok(near(rad2deg(great_circle_direction(@Tokyo, @London)),
	    336.069766430326));

    ok(near(rad2deg(great_circle_direction(@Berlin, @Paris)),
	    246.800348034667));
    
    ok(near(rad2deg(great_circle_direction(@Paris, @Berlin)),
	    58.2079877553156));

    use Math::Trig 'great_circle_bearing';

    ok(near(rad2deg(great_circle_bearing(@Paris, @Berlin)),
	    58.2079877553156));

    use Math::Trig 'great_circle_waypoint';
    use Math::Trig 'great_circle_midpoint';

    my ($lon, $lat);

    ($lon, $lat) = great_circle_waypoint(@London, @Tokyo, 0.0);

    ok(near($lon, $London[0]));

    ok(near($lat, $London[1]));

    ($lon, $lat) = great_circle_waypoint(@London, @Tokyo, 1.0);

    ok(near($lon, $Tokyo[0]));

    ok(near($lat, $Tokyo[1]));

    ($lon, $lat) = great_circle_waypoint(@London, @Tokyo, 0.5);

    ok(near($lon, 1.55609593577679)); # 89.16 E

    ok(near($lat, 0.36783532946162)); # 68.93 N

    ($lon, $lat) = great_circle_midpoint(@London, @Tokyo);

    ok(near($lon, 1.55609593577679)); # 89.16 E

    ok(near($lat, 0.367835329461615)); # 68.93 N

    ($lon, $lat) = great_circle_waypoint(@London, @Tokyo, 0.25);

    ok(near($lon, 0.516073562850837)); # 29.57 E

    ok(near($lat, 0.400231313403387)); # 67.07 N

    ($lon, $lat) = great_circle_waypoint(@London, @Tokyo, 0.75);

    ok(near($lon, 2.17494903805952)); # 124.62 E

    ok(near($lat, 0.617809294053591)); # 54.60 N

    use Math::Trig 'great_circle_destination';

    my $dir1 = great_circle_direction(@London, @Tokyo);
    my $dst1 = great_circle_distance(@London,  @Tokyo);

    ($lon, $lat) = great_circle_destination(@London, $dir1, $dst1);

    ok(near($lon, $Tokyo[0]));

    ok(near($lat, $pip2 - $Tokyo[1]));

    my $dir2 = great_circle_direction(@Tokyo, @London);
    my $dst2 = great_circle_distance(@Tokyo,  @London);

    ($lon, $lat) = great_circle_destination(@Tokyo, $dir2, $dst2);

    ok(near($lon, $London[0]));

    ok(near($lat, $pip2 - $London[1]));

    my $dir3 = (great_circle_destination(@London, $dir1, $dst1))[2];

    ok(near($dir3, 2.69379263839118)); # about 154.343 deg

    my $dir4 = (great_circle_destination(@Tokyo,  $dir2, $dst2))[2];

    ok(near($dir4, 3.6993902625701)); # about 211.959 deg

    ok(near($dst1, $dst2));
}

# eof
