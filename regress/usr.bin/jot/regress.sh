# $OpenBSD: regress.sh,v 1.8 2018/06/25 06:03:48 tb Exp $
# $FreeBSD: head/usr.bin/jot/tests/regress.sh 208728 2010-06-02 07:47:29Z brian $

JOT=${JOT-jot}
echo "# JOT is ${JOT}"
echo "# SRCDIR is ${SRCDIR}"

REGRESSION_START(`pwd`)

# A: test all 16 possibilities for reps, begin, end, s with omissions
REGRESSION_TEST([[hhhh]], [[${JOT} 50 20 120 2]])
REGRESSION_TEST([[hhhd]], [[${JOT} 50 20 120 -]])
REGRESSION_TEST([[hhdh]], [[${JOT} 50 20 - 2]])
REGRESSION_TEST([[hhdd]], [[${JOT} 50 20 - -]])
REGRESSION_TEST([[hdhh]], [[${JOT} 50 - 120 2]])
REGRESSION_TEST([[hdhd]], [[${JOT} 50 - 120 -]])
REGRESSION_TEST([[hddh]], [[${JOT} 50 - - 2]])
REGRESSION_TEST([[hddd]], [[${JOT} 50 - - -]])
REGRESSION_TEST([[dhhh]], [[${JOT} - 20 120 2]])
REGRESSION_TEST([[dhhd]], [[${JOT} - 20 120 -]])
REGRESSION_TEST([[dhdh]], [[${JOT} - 20 - 2]])
REGRESSION_TEST([[dhdd]], [[${JOT} - 20 - -]])
REGRESSION_TEST([[ddhh]], [[${JOT} - - 120 2]])
REGRESSION_TEST([[ddhd]], [[${JOT} - - 120 -]])
REGRESSION_TEST([[dddh]], [[${JOT} - - - 2]])
REGRESSION_TEST([[dddd]], [[${JOT} - - - -]])

# B: same as A, but different numbers, only 12 because of 4 duplicates
REGRESSION_TEST([[hhhh2]], [[${JOT} 30 20 160 2]])
REGRESSION_TEST([[hhhd2]], [[${JOT} 30 20 160 -]])
REGRESSION_TEST([[hhdh2]], [[${JOT} 30 20 - 2]])
REGRESSION_TEST([[hhdd2]], [[${JOT} 30 20 - -]])
REGRESSION_TEST([[hdhh2]], [[${JOT} 30 - 160 2]])
REGRESSION_TEST([[hdhd2]], [[${JOT} 30 - 160 -]])
REGRESSION_TEST([[hddh2]], [[${JOT} 30 - - 2]])
REGRESSION_TEST([[hddd2]], [[${JOT} 30 - - -]])
REGRESSION_TEST([[dhhh2]], [[${JOT} - 20 160 2]])
REGRESSION_TEST([[dhhd2]], [[${JOT} - 20 160 -]])
REGRESSION_TEST([[ddhh2]], [[${JOT} - - 160 2]])
REGRESSION_TEST([[ddhd2]], [[${JOT} - - 160 -]])

# C: reverse roles of begin and end in A.
REGRESSION_TEST([[hhhh3]], [[${JOT} 50 120 20 -2]])
REGRESSION_TEST([[hhhd3]], [[${JOT} 50 120 20 -]])
REGRESSION_TEST([[hhdh3]], [[${JOT} 50 120 - -2]])
REGRESSION_TEST([[hhdd3]], [[${JOT} 50 120 - -]])
REGRESSION_TEST([[hdhh3]], [[${JOT} 50 - 20 -2]])
REGRESSION_TEST([[hdhd3]], [[${JOT} 50 - 20 -]])
REGRESSION_TEST([[dhhh3]], [[${JOT} - 120 20 -2]])
REGRESSION_TEST([[dhhd3]], [[${JOT} - 120 20 -]])
REGRESSION_TEST([[dhdh3]], [[${JOT} - 120 - -2]])
REGRESSION_TEST([[dhdd3]], [[${JOT} - 120 - -]])
REGRESSION_TEST([[ddhh3]], [[${JOT} - - 20 -2]])
REGRESSION_TEST([[ddhd3]], [[${JOT} - - 20 -]])

# D: reverse roles of begin and end in B.
REGRESSION_TEST([[hhhh4]], [[${JOT} 30 160 20 -2]])
REGRESSION_TEST([[hhhd4]], [[${JOT} 30 160 20 -]])
REGRESSION_TEST([[hhdh4]], [[${JOT} 30 160 - -2]])
REGRESSION_TEST([[hhdd4]], [[${JOT} 30 160 - -]])
REGRESSION_TEST([[hdhh4]], [[${JOT} 30 - 20 -2]])
REGRESSION_TEST([[hdhd4]], [[${JOT} 30 - 20 -]])
REGRESSION_TEST([[dhhh4]], [[${JOT} - 160 20 -2]])
REGRESSION_TEST([[dhhd4]], [[${JOT} - 160 20 -]])

# E: what happens if begin and end are equal?
REGRESSION_TEST([[dbbd]], [[${JOT} - 10 10 -]])
REGRESSION_TEST([[hbbd]], [[${JOT} 15 10 10 -]])
REGRESSION_TEST([[dbbh]], [[${JOT} - 10 10 2]])
REGRESSION_TEST([[hbbh]], [[${JOT} 15 10 10 2]])

# F: random output
# rand1 and rand2 test coverage (10000 is way too big: 200 should be enough)
REGRESSION_TEST([[rand1]], [[${JOT} -r 10000 0 9 | sort -u]])
REGRESSION_TEST([[rand2]], [[${JOT} -r 10000 9 0 | sort -u]])
# same thing again, but divide by 10
REGRESSION_TEST([[rand1p1]], [[${JOT} -p 1 -r 10000 0 0.9 | sort -u]])
REGRESSION_TEST([[rand2p1]], [[${JOT} -p 1 -r 10000 0.9 0 | sort -u]])
# rdhhh and rhdhh test if begin and ender are set to the default with jot -r
REGRESSION_TEST([[rdhhh]], [[${JOT} -r 100 - 10 2 2>/dev/null | sort -n | head -1]])
REGRESSION_TEST([[rhdhh]], [[${JOT} -r 100 90 - 2 2>/dev/null | sort -n | tail -1]])
# test variant of old manpage example, as it exercises the 'use_unif = 0' path
REGRESSION_TEST([[nonunif]], [[${JOT} -p0 -r 10000 0.5 9.5 | sort -u]])

# G: Examples from the FreeBSD manual
REGRESSION_TEST([[n21]], [[${JOT} 21 -1 1.00]])
REGRESSION_TEST([[ascii]], [[${JOT} -c 128 0]])
REGRESSION_TEST([[xaa]], [[${JOT} -w xa%c 26 a]])
REGRESSION_TEST([[yes]], [[${JOT} -b yes 10]])
REGRESSION_TEST([[ed]], [[${JOT} -w %ds/old/new/ 30 2 - 5]])
REGRESSION_TEST([[stutter]], [[${JOT} - 9 0 -.5]])
REGRESSION_TEST([[stutter2]], [[${JOT} -w %d - 9.5 0 -.5]])
REGRESSION_TEST([[block]], [[${JOT} -b x 512]])
REGRESSION_TEST([[tabs]], [[${JOT} -s, - 10 132 4]])
REGRESSION_TEST([[grep]], [[${JOT} -s "" -b . 80]])

# H: various format strings
REGRESSION_TEST([[wf]], [[${JOT} -w "a%.1fb" 10]])
REGRESSION_TEST([[we]], [[${JOT} -w "a%eb" 10]])
REGRESSION_TEST([[wwe]], [[${JOT} -w "a%-15eb" 10]])
REGRESSION_TEST([[wg]], [[${JOT} -w "a%20gb" 10]])
REGRESSION_TEST([[wc]], [[${JOT} -w "a%cb" 10 33 43]])
REGRESSION_TEST([[wgd]], [[${JOT} -w "a%gb" 10 .2]])
REGRESSION_TEST([[wu]], [[${JOT} -w "a%ub" 10]])
REGRESSION_TEST([[wU]], [[${JOT} -w "a%Ub" 10]])
REGRESSION_TEST([[wlu]], [[${JOT} -w "a%lub" 10]])
REGRESSION_TEST([[wo]], [[${JOT} -w "a%ob" 10]])
REGRESSION_TEST([[wO]], [[${JOT} -w "a%Ob" 10]])
REGRESSION_TEST([[wlo]], [[${JOT} -w "a%lob" 10]])
REGRESSION_TEST([[wx]], [[${JOT} -w "a%xb" 10]])
REGRESSION_TEST([[wX1]], [[${JOT} -w "a%Xb" 10]])
REGRESSION_TEST([[wXl]], [[${JOT} -w "a%Xb" 10 2147483648]])
REGRESSION_TEST([[wdl]], [[${JOT} -w "a%db" 10 2147483648 2>/dev/null]])
REGRESSION_TEST([[wxn]], [[${JOT} -w "a%xb" 10 -5 2>/dev/null]])
REGRESSION_TEST([[wdn]], [[${JOT} -w "a%db" 10 -5]])
REGRESSION_TEST([[wDn]], [[${JOT} -w "a%Db" 10 -5]])
REGRESSION_TEST([[wldn]], [[${JOT} -w "a%ldb" 10 -5]])
REGRESSION_TEST([[wp1]], [[${JOT} -w "%%%d%%%%" 10]])
REGRESSION_TEST([[wp2]], [[${JOT} -w "%d%%d%%" 10]])
REGRESSION_TEST([[wp3]], [[${JOT} -w "a%%A%%%d%%B%%b" 10]])
REGRESSION_TEST([[wp4]], [[${JOT} -w "%%d%d%%d%%" 10]])
REGRESSION_TEST([[wp5]], [[${JOT} -w ftp://www.example.com/pub/uploaded%%20files/disk%03d.iso 10]])
REGRESSION_TEST([[wp6]], [[${JOT} -w "%d%" 10]])
REGRESSION_TEST([[x]], [[${JOT} -w "%X" -s , 100 1 200]])

# I: Examples from the OpenBSD manual
REGRESSION_TEST([[man1]], [[${JOT} 6 1 10 0.5]])
REGRESSION_TEST([[man2]], [[${JOT} -p 1 6 1 10 0.5]])
REGRESSION_TEST([[man3]], [[${JOT} -p 0 6 .9999999999 10 0.5]])
REGRESSION_TEST([[man4]], [[${JOT} -w %d 6 1 10 0.5]])
REGRESSION_TEST([[man5]], [[${JOT} 21 -1 1.00]])
REGRESSION_TEST([[man6]], [[${JOT} -w xa%c 26 a]])
REGRESSION_TEST([[man7]], [[${JOT} -w %ds/old/new/ 30 2 - 5]])
REGRESSION_TEST([[man8]], [[${JOT} -b x 512]])
REGRESSION_TEST([[man9]], [[${JOT} -s, - 10 132 4]])
REGRESSION_TEST([[man10]], [[${JOT} -s "" -b. 80]])

# J: Misc tests
REGRESSION_TEST([[nb1]], [[{ ${JOT} -n -b1 1 && echo; }]])
REGRESSION_TEST([[noargs]], [[${JOT}]])

REGRESSION_END()
