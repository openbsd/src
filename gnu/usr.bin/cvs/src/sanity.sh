#!/bin/sh
# a quick sanity test for cvs.
#
# Copyright (C) 1992, 1993 Cygnus Support
#
# Original Author: K. Richard Pixley

# usage: sanity.sh [-r] @var{cvs-to-test}
# -r means to test remote instead of local cvs.

# See TODO list at end of file.

TESTDIR=/tmp/cvs-sanity

# "debugger"
#set -x

echo This test should produce no other output than this line, and "Ok."

# clean any old remnants
rm -rf ${TESTDIR}

if test x"$1" = x"-r"; then
  shift
  remote=yes
else
  remote=no
fi

testcvs=$1; shift

# Remaining arguments are the names of tests to run.
if test x"$*" = x; then
  tests="basic0 basic1 basic2 basic3 rtags death import new conflicts modules mflag errmsg1"
else
  tests="$*"
fi

# fixme: try things (what things? checkins?) without -m.
# Some of these tests are written to expect -Q.  But testing with
# -Q is kind of bogus, it is not the way users actually use CVS (usually).
# So new tests probably should invoke ${testcvs} directly, rather than ${CVS}.
CVS="${testcvs} -Q"

LOGFILE=`pwd`/check.log
if test -f check.log; then mv check.log check.plog; fi

mkdir ${TESTDIR}
cd ${TESTDIR}

# so far so good.  Let's try something harder.

# this should die
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ; then
  echo "FAIL: test 1" | tee -a ${LOGFILE}; exit 1
else
  echo "PASS: test 1" >>${LOGFILE}
fi

# this should still die
mkdir cvsroot
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ; then
  echo "FAIL: test 2" | tee -a ${LOGFILE}; exit 1
else
  echo "PASS: test 2" >>${LOGFILE}
fi

# this should still die
mkdir cvsroot/CVSROOT
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ; then
  echo "FAIL: test 3" | tee -a ${LOGFILE}; exit 1
else
  echo "PASS: test 3" >>${LOGFILE}
fi

# This one should work, although it should spit a warning.
mkdir tmp ; cd tmp
${CVS} -d `pwd`/../cvsroot co CVSROOT 2>> ${LOGFILE}
cd .. ; rm -rf tmp

# This one should succeed.  No warnings.
echo 'CVSROOT		-i mkmodules CVSROOT' > cvsroot/CVSROOT/modules
mkdir tmp ; cd tmp
if ${CVS} -d `pwd`/../cvsroot co CVSROOT ; then
  echo "PASS: test 4" >>${LOGFILE}
else
  echo "FAIL: test 4" | tee -a ${LOGFILE}; exit 1
fi

cd .. ; rm -rf tmp

# Try setting CVSROOT so we don't have to worry about it anymore.  (now that
# we've tested -d cvsroot.)
CVSROOT_FILENAME=`pwd`/cvsroot
CVSROOT=${CVSROOT_FILENAME} ; export CVSROOT
if test "x$remote" = xyes; then
  CVSROOT=`hostname`:${CVSROOT_FILENAME} ; export CVSROOT
  # Use rsh so we can test it without having to muck with inetd or anything 
  # like that.  Also needed to get CVS_SERVER to work.
  CVS_CLIENT_PORT=-1; export CVS_CLIENT_PORT
  CVS_SERVER=${testcvs}; export CVS_SERVER
fi

mkdir tmp ; cd tmp
if ${CVS} -d `pwd`/../cvsroot co CVSROOT ; then
  echo "PASS: test 5" >>${LOGFILE}
else
  echo "FAIL: test 5" | tee -a ${LOGFILE}; exit 1
fi

cd .. ; rm -rf tmp

# start keeping history
touch ${CVSROOT_FILENAME}/CVSROOT/history

### The big loop
for what in $tests; do
	case $what in
	basic0) # Now, let's build something.
#		mkdir first-dir
		# this doesn't yet work, though I think maybe it should.  xoxorich.
#		if ${CVS} add first-dir ; then
#			true
#		else
#			echo cvs does not yet add top level directories cleanly.
			mkdir ${CVSROOT_FILENAME}/first-dir
#		fi
#		rm -rf first-dir

		# check out an empty directory
		if ${CVS} co first-dir ; then
		  echo "PASS: test 6" >>${LOGFILE}
		else
		  echo "FAIL: test 6" | tee -a ${LOGFILE}; exit 1
		fi

		# update the empty directory
		if ${CVS} update first-dir ; then
		  echo "PASS: test 7" >>${LOGFILE}
		else
		  echo "FAIL: test 7" | tee -a ${LOGFILE}; exit 1
		fi

		# diff -u the empty directory
		if ${CVS} diff -u first-dir ; then
		  echo "PASS: test 8" >>${LOGFILE}
		else
		  echo "FAIL: test 8" | tee -a ${LOGFILE}; exit 1
		fi

		# diff -c the empty directory
		if ${CVS} diff -c first-dir ; then
		  echo "PASS: test 9" >>${LOGFILE}
		else
		  echo "FAIL: test 9" | tee -a ${LOGFILE}; exit 1
		fi

		# log the empty directory
		if ${CVS} log first-dir ; then
		  echo "PASS: test 10" >>${LOGFILE}
		else
		  echo "FAIL: test 10" | tee -a ${LOGFILE}; exit 1
		fi

		# status the empty directory
		if ${CVS} status first-dir ; then
		  echo "PASS: test 11" >>${LOGFILE}
		else
		  echo "FAIL: test 11" | tee -a ${LOGFILE}; exit 1
		fi

		# tag the empty directory
		if ${CVS} tag first first-dir  ; then
		  echo "PASS: test 12" >>${LOGFILE}
		else
		  echo "FAIL: test 12" | tee -a ${LOGFILE}; exit 1
		fi

		# rtag the empty directory
		if ${CVS} rtag empty first-dir  ; then
		  echo "PASS: test 13" >>${LOGFILE}
		else
		  echo "FAIL: test 13" | tee -a ${LOGFILE}; exit 1
		fi
		;;

	basic1) # first dive - add a files, first singly, then in a group.
		rm -rf ${CVSROOT_FILENAME}/first-dir
		rm -rf first-dir
		mkdir ${CVSROOT_FILENAME}/first-dir
		# check out an empty directory
		if ${CVS} co first-dir  ; then
		  echo "PASS: test 13a" >>${LOGFILE}
		else
		  echo "FAIL: test 13a" | tee -a ${LOGFILE}; exit 1
		fi

		cd first-dir
		files=first-file
		for i in a b ; do
			for j in ${files} ; do
				echo $j > $j
			done

			for do in add rm ; do
				for j in ${do} "commit -m test" ; do
					# ${do}
					if ${CVS} $j ${files}  >> ${LOGFILE} 2>&1; then
					  echo "PASS: test 14-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 14-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# update it.
					if [ "${do}" = "rm" -a "$j" != "commit -m test" ] || ${CVS} update ${files} ; then
					  echo "PASS: test 15-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 15-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# update all.
					if ${CVS} update  ; then
					  echo "PASS: test 16-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 16-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# status all.
					if ${CVS} status  >> ${LOGFILE}; then
					  echo "PASS: test 17-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 17-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

		# fixme: this one doesn't work yet for added files.
					# log all.
					if ${CVS} log  >> ${LOGFILE}; then
					  echo "PASS: test 18-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 18-${do}-$j" | tee -a ${LOGFILE}
					fi

					if test "x${do}-$j" = "xadd-add" || test "x${do}-$j" = "xrm-rm" ; then
					  true
					else
					  # diff -c all
					  if ${CVS} diff -c  >> ${LOGFILE} || [ $? = 1 ] ; then
					    echo "PASS: test 19-${do}-$j" >>${LOGFILE}
					  else
					    echo "FAIL: test 19-${do}-$j" | tee -a ${LOGFILE}
					  fi

					  # diff -u all
					  if ${CVS} diff -u  >> ${LOGFILE} || [ $? = 1 ] ; then
					    echo "PASS: test 20-${do}-$j" >>${LOGFILE}
					  else
					    echo "FAIL: test 20-${do}-$j" | tee -a ${LOGFILE}
					  fi
					fi

					cd ..
					# update all.
					if ${CVS} update  ; then
					  echo "PASS: test 21-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 21-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# log all.
		# fixme: doesn't work right for added files.
					if ${CVS} log first-dir  >> ${LOGFILE}; then
					  echo "PASS: test 22-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 22-${do}-$j" | tee -a ${LOGFILE}
					fi

					# status all.
					if ${CVS} status first-dir  >> ${LOGFILE}; then
					  echo "PASS: test 23-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 23-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# update all.
					if ${CVS} update first-dir  ; then
						true
					else
						echo '***' failed test 24-${do}-$j. ; exit 1
					fi

					if test "x${do}-$j" = "xadd-add" || test "x${do}-$j" = "xrm-rm" ; then
					  true
					else
					  # diff all
					  if ${CVS} diff -u  >> ${LOGFILE} || [ $? = 1 ] ; then
					    true
					  else
					    echo '***' failed test 25-${do}-$j. # FIXME; exit 1
					  fi

					  # diff all
					  if ${CVS} diff -u first-dir  >> ${LOGFILE} || [ $? = 1 ] ; then
					    true
					  else
					    echo '***' failed test 26-${do}-$j. # FIXME; exit 1
					  fi
					fi

					# update all.
					if ${CVS} co first-dir  ; then
						true
					else
						echo '***' failed test 27-${do}-$j. ; exit 1
					fi

					cd first-dir
				done # j
				rm -f ${files}
			done # do

			files="file2 file3 file4 file5"
		done
		if ${CVS} tag first-dive  ; then
			true
		else
			echo '***' failed test 28. ; exit 1
		fi
		cd ..
		;;

	basic2) # second dive - add bunch o' files in bunch o' added directories
		for i in first-dir dir1 dir2 dir3 dir4 ; do
			if [ ! -d $i ] ; then
				mkdir $i
				if ${CVS} add $i  >> ${LOGFILE}; then
					true
				else
					echo '***' failed test 29-$i. ; exit 1
				fi
			fi

			cd $i

			for j in file6 file7 file8 file9 file10 file11 file12 file13; do
				echo $j > $j
			done

			if ${CVS} add file6 file7 file8 file9 file10 file11 file12 file13  2>> ${LOGFILE}; then
				true
			else
				echo '***' failed test 30-$i-$j. ; exit 1
			fi
		done
		cd ../../../../..
		if ${CVS} update first-dir  ; then
			true
		else
			echo '***' failed test 31. ; exit 1
		fi

		# fixme: doesn't work right for added files.
		if ${CVS} log first-dir  >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 32. # ; exit 1
		fi

		if ${CVS} status first-dir  >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 33. ; exit 1
		fi

#		if ${CVS} diff -u first-dir   >> ${LOGFILE} || [ $? = 1 ] ; then
#			true
#		else
#			echo '***' failed test 34. # ; exit 1
#		fi

		if ${CVS} ci -m "second dive" first-dir  >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 35. ; exit 1
		fi

		if ${CVS} tag second-dive first-dir  ; then
			true
		else
			echo '***' failed test 36. ; exit 1
		fi
		;;

	basic3) # third dive - in bunch o' directories, add bunch o' files, delete some, change some.
		for i in first-dir dir1 dir2 dir3 dir4 ; do
			cd $i

			# modify some files
			for j in file6 file8 file10 file12 ; do
				echo $j >> $j
			done

			# delete some files
			rm file7 file9 file11 file13

			if ${CVS} rm file7 file9 file11 file13  2>> ${LOGFILE}; then
				true
			else
				echo '***' failed test 37-$i. ; exit 1
			fi

			# and add some new ones
			for j in file14 file15 file16 file17 ; do
				echo $j > $j
			done

			if ${CVS} add file14 file15 file16 file17  2>> ${LOGFILE}; then
				true
			else
				echo '***' failed test 38-$i. ; exit 1
			fi
		done
		cd ../../../../..
		if ${CVS} update first-dir  ; then
			true
		else
			echo '***' failed test 39. ; exit 1
		fi

		# fixme: doesn't work right for added files
		if ${CVS} log first-dir  >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 40. # ; exit 1
		fi

		if ${CVS} status first-dir  >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 41. ; exit 1
		fi

#		if ${CVS} diff -u first-dir  >> ${LOGFILE} || [ $? = 1 ] ; then
#			true
#		else
#			echo '***' failed test 42. # ; exit 1
#		fi

		if ${CVS} ci -m "third dive" first-dir  >>${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 43. ; exit 1
		fi

		if ${CVS} tag third-dive first-dir  ; then
			true
		else
			echo '***' failed test 44. ; exit 1
		fi

		# Hmm...  fixme.
#		if ${CVS} release first-dir  ; then
#			true
#		else
#			echo '***' failed test 45. # ; exit 1
#		fi

		# end of third dive
		rm -rf first-dir
		;;

	rtags) # now try some rtags
		# rtag HEADS
		if ${CVS} rtag rtagged-by-head first-dir  ; then
			true
		else
			echo '***' failed test 46. ; exit 1
		fi

		# tag by tag
		if ${CVS} rtag -r rtagged-by-head rtagged-by-tag first-dir  ; then
			true
		else
			echo '***' failed test 47. ; exit 1
		fi

		# tag by revision
		if ${CVS} rtag -r1.1 rtagged-by-revision first-dir  ; then
			true
		else
			echo '***' failed test 48. ; exit 1
		fi

		# rdiff by revision
		if ${CVS} rdiff -r1.1 -rrtagged-by-head first-dir  >> ${LOGFILE} || [ $? = 1 ] ; then
			true
		else
			echo '***' failed test 49. ; exit 1
		fi

		# now export by rtagged-by-head and rtagged-by-tag and compare.
		rm -rf first-dir
		if ${CVS} export -r rtagged-by-head first-dir  ; then
			true
		else
			echo '***' failed test 50. ; exit 1
		fi

		mv first-dir 1dir
		if ${CVS} export -r rtagged-by-tag first-dir  ; then
			true
		else
			echo '***' failed test 51. ; exit 1
		fi

		if diff -c -r 1dir first-dir ; then
			true
		else
			echo '***' failed test 52. ; exit 1
		fi
		rm -rf 1dir first-dir

		# For some reason, this command has stopped working and hence much of this sequence is currently off.
		# export by revision vs checkout by rtagged-by-revision and compare.
#		if ${CVS} export -r1.1 first-dir  ; then
#			true
#		else
#			echo '***' failed test 53. # ; exit 1
#		fi
		# note sidestep below
		#mv first-dir 1dir

		if ${CVS} co -rrtagged-by-revision first-dir  ; then
			true
		else
			echo '***' failed test 54. ; exit 1
		fi
		# fixme: this is here temporarily to sidestep test 53.
		ln -s first-dir 1dir

		# directory copies are done in an oblique way in order to avoid a bug in sun's tmp filesystem.
		mkdir first-dir.cpy ; (cd first-dir ; tar cf - * | (cd ../first-dir.cpy ; tar xf -))

		if diff --exclude=CVS -c -r 1dir first-dir ; then
			true
		else
			echo '***' failed test 55. ; exit 1
		fi

		# interrupt, while we've got a clean 1.1 here, let's import it into another tree.
		cd 1dir
		if ${CVS} import -m "first-import" second-dir first-immigration immigration1 immigration1_0  ; then
			true
		else
			echo '***' failed test 56. ; exit 1
		fi
		cd ..

		if ${CVS} export -r HEAD second-dir  ; then
			true
		else
			echo '***' failed test 57. ; exit 1
		fi

		if diff --exclude=CVS -c -r first-dir second-dir ; then
			true
		else
			echo '***' failed test 58. ; exit 1
		fi

		rm -rf 1dir first-dir
		mkdir first-dir
		(cd first-dir.cpy ; tar cf - * | (cd ../first-dir ; tar xf -))

		# update the top, cancelling sticky tags, retag, update other copy, compare.
		cd first-dir
		if ${CVS} update -A -l *file*  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 59. ; exit 1
		fi

		# If we don't delete the tag first, cvs won't retag it.
		# This would appear to be a feature.
		if ${CVS} tag -l -d rtagged-by-revision  ; then
			true
		else
			echo '***' failed test 60a. ; exit 1
		fi
		if ${CVS} tag -l rtagged-by-revision  ; then
			true
		else
			echo '***' failed test 60b. ; exit 1
		fi

		cd .. ; mv first-dir 1dir
		mv first-dir.cpy first-dir ; cd first-dir
		if ${CVS} diff -u  >> ${LOGFILE} || [ $? = 1 ] ; then
			true
		else
			echo '***' failed test 61. ; exit 1
		fi

		if ${CVS} update  ; then
			true
		else
			echo '***' failed test 62. ; exit 1
		fi

		cd ..

# Haven't investigated why this is failing.
#		if diff --exclude=CVS -c -r 1dir first-dir ; then
#			true
#		else
#			echo '***' failed test 63. # ; exit 1
#		fi
		rm -rf 1dir first-dir

		if ${CVS} his -e -a  >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 64. ; exit 1
		fi
		;;

	death) # next dive.  test death support.
		rm -rf ${CVSROOT_FILENAME}/first-dir
		mkdir  ${CVSROOT_FILENAME}/first-dir
		if ${CVS} co first-dir  ; then
			true
		else
			echo '***' failed test 65 ; exit 1
		fi

		cd first-dir

		# add a file.
		touch file1
		if ${CVS} add file1  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 66 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 67 ; exit 1
		fi

		# remove
		rm file1
		if ${CVS} rm file1  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 68 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE} ; then
			true
		else
			echo '***' failed test 69 ; exit 1
		fi

		# add again and create second file
		touch file1 file2
		if ${CVS} add file1 file2  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 70 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 71 ; exit 1
		fi

		# log
		if ${CVS} log file1  >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 72 ; exit 1
		fi


		# branch1
		if ${CVS} tag -b branch1  ; then
			true
		else
			echo '***' failed test 73 ; exit 1
		fi

		# and move to the branch.
		if ${CVS} update -r branch1  ; then
			true
		else
			echo '***' failed test 74 ; exit 1
		fi

		# add a file in the branch
		echo line1 from branch1 >> file3
		if ${CVS} add file3  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 75 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 76 ; exit 1
		fi

		# remove
		rm file3
		if ${CVS} rm file3  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 77 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE} ; then
			true
		else
			echo '***' failed test 78 ; exit 1
		fi

		# add again
		echo line1 from branch1 >> file3
		if ${CVS} add file3  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 79 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 80 ; exit 1
		fi

		# change the first file
		echo line2 from branch1 >> file1

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 81 ; exit 1
		fi

		# remove the second
		rm file2
		if ${CVS} rm file2  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 82 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE}; then
			true
		else
			echo '***' failed test 83 ; exit 1
		fi

		# back to the trunk.
		if ${CVS} update -A  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 84 ; exit 1
		fi

		if [ -f file3 ] ; then
			echo '***' failed test 85 ; exit 1
		else
			true
		fi

		# join
		if ${CVS} update -j branch1  >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 86 ; exit 1
		fi

		if [ -f file3 ] ; then
			true
		else
			echo '***' failed test 87 ; exit 1
		fi

		# update
		if ${CVS} update  ; then
			true
		else
			echo '***' failed test 88 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 89 ; exit 1
		fi

		# remove first file.
		rm file1
		if ${CVS} rm file1  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 90 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE}; then
			true
		else
			echo '***' failed test 91 ; exit 1
		fi

		if [ -f file1 ] ; then
			echo '***' failed test 92 ; exit 1
		else
			true
		fi

		# back to branch1
		if ${CVS} update -r branch1  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 93 ; exit 1
		fi

		if [ -f file1 ] ; then
			true
		else
			echo '***' failed test 94 ; exit 1
		fi

		# and join
		if ${CVS} update -j HEAD  >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 95 ; exit 1
		fi

		cd .. ; rm -rf first-dir ${CVSROOT_FILENAME}/first-dir
		;;

	import) # test death after import
		# import
		mkdir import-dir ; cd import-dir

		for i in 1 2 3 4 ; do
			echo imported file"$i" > imported-file"$i"
		done

		if ${CVS} import -m first-import first-dir vendor-branch junk-1_0  ; then
			true
		else
			echo '***' failed test 96 ; exit 1
		fi
		cd ..

		# co
		if ${CVS} co first-dir  ; then
			true
		else
			echo '***' failed test 97 ; exit 1
		fi

		cd first-dir
		for i in 1 2 3 4 ; do
			if [ -f imported-file"$i" ] ; then
				true
			else
				echo '***' failed test 98-$i ; exit 1
			fi
		done

		# remove
		rm imported-file1
		if ${CVS} rm imported-file1  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 99 ; exit 1
		fi

		# change
		# this sleep is significant.  Otherwise, on some machines, things happen so
		# fast that the file mod times do not differ.
		sleep 1
		echo local-change >> imported-file2

		# commit
		if ${CVS} ci -m local-changes  >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 100 ; exit 1
		fi

		# log
		if ${CVS} log imported-file1 | grep '1.1.1.2 (dead)'  ; then
			echo '***' failed test 101 ; exit 1
		else
			true
		fi

		# update into the vendor branch.
		if ${CVS} update -rvendor-branch  ; then
			true
		else
			echo '***' failed test 102 ; exit 1
		fi

		# remove file4 on the vendor branch
		rm imported-file4

		if ${CVS} rm imported-file4  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 103 ; exit 1
		fi

		# commit
		if ${CVS} ci -m vendor-removed imported-file4 >>${LOGFILE}; then
			true
		else
			echo '***' failed test 104 ; exit 1
		fi

		# update to main line
		if ${CVS} update -A  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 105 ; exit 1
		fi

		# second import - file4 deliberately unchanged
		cd ../import-dir
		for i in 1 2 3 ; do
			echo rev 2 of file $i >> imported-file"$i"
		done

		if ${CVS} import -m second-import first-dir vendor-branch junk-2_0  ; then
			true
		else
			echo '***' failed test 106 ; exit 1
		fi
		cd ..

		# co
		if ${CVS} co first-dir  ; then
			true
		else
			echo '***' failed test 107 ; exit 1
		fi

		cd first-dir

		if [ -f imported-file1 ] ; then
			echo '***' failed test 108 ; exit 1
		else
			true
		fi

		for i in 2 3 ; do
			if [ -f imported-file"$i" ] ; then
				true
			else
				echo '***' failed test 109-$i ; exit 1
			fi
		done

		# check vendor branch for file4
		if ${CVS} update -rvendor-branch  ; then
			true
		else
			echo '***' failed test 110 ; exit 1
		fi

		if [ -f imported-file4 ] ; then
			true
		else
			echo '***' failed test 111 ; exit 1
		fi

		# update to main line
		if ${CVS} update -A  2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 112 ; exit 1
		fi

		cd ..

		if ${CVS} co -jjunk-1_0 -jjunk-2_0 first-dir  >>${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 113 ; exit 1
		fi

		cd first-dir

		if [ -f imported-file1 ] ; then
			echo '***' failed test 114 ; exit 1
		else
			true
		fi

		for i in 2 3 ; do
			if [ -f imported-file"$i" ] ; then
				true
			else
				echo '***' failed test 115-$i ; exit 1
			fi
		done

		if cat imported-file2 | grep '===='  >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 116 ; exit 1
		fi
		cd .. ; rm -rf first-dir ${CVSROOT_FILENAME}/first-dir
		;;

	new) # look for stray "no longer pertinent" messages.
		rm -rf first-dir ${CVSROOT_FILENAME}/first-dir
		mkdir ${CVSROOT_FILENAME}/first-dir

		if ${CVS} co first-dir  ; then
			true
		else
			echo '***' failed test 117 ; exit 1
		fi

		cd first-dir
		touch a

		if ${CVS} add a  2>>${LOGFILE}; then
			true
		else
			echo '***' failed test 118 ; exit 1
		fi

		if ${CVS} ci -m added  >>${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 119 ; exit 1
		fi

		rm a

		if ${CVS} rm a  2>>${LOGFILE}; then
			true
		else
			echo '***' failed test 120 ; exit 1
		fi

		if ${CVS} ci -m removed >>${LOGFILE} ; then
			true
		else
			echo '***' failed test 121 ; exit 1
		fi

		if ${CVS} update -A  2>&1 | grep longer ; then
			echo '***' failed test 122 ; exit 1
		else
			true
		fi

		if ${CVS} update -rHEAD 2>&1 | grep longer ; then
			echo '***' failed test 123 ; exit 1
		else
			true
		fi

		cd .. ; rm -rf first-dir ; rm -rf ${CVSROOT_FILENAME}/first-dir
		;;

	conflicts)
		rm -rf first-dir ${CVSROOT_FILENAME}/first-dir
		mkdir ${CVSROOT_FILENAME}/first-dir

		mkdir 1
		cd 1

		if ${CVS} co first-dir ; then
			echo 'PASS: test 124' >>${LOGFILE}
		else
			echo 'FAIL: test 124' | tee -a ${LOGFILE}
		fi

		cd first-dir
		touch a

		if ${CVS} add a 2>>${LOGFILE} ; then
			echo 'PASS: test 125' >>${LOGFILE}
		else
			echo 'FAIL: test 125' | tee -a ${LOGFILE}
		fi

		if ${CVS} ci -m added >>${LOGFILE} 2>&1; then
			echo 'PASS: test 126' >>${LOGFILE}
		else
			echo 'FAIL: test 126' | tee -a ${LOGFILE}
		fi

		cd ../..
		mkdir 2
		cd 2

		if ${CVS} co first-dir ; then
			echo 'PASS: test 127' >>${LOGFILE}
		else
			echo 'FAIL: test 127' | tee -a ${LOGFILE}
		fi
		cd first-dir
		if test -f a; then
			echo 'PASS: test 127a' >>${LOGFILE}
		else
			echo 'FAIL: test 127a' | tee -a ${LOGFILE}
		fi

		cd ../../1/first-dir
		echo add a line >>a
		if ${CVS} ci -m changed >>${LOGFILE} 2>&1; then
			echo 'PASS: test 128' >>${LOGFILE}
		else
			echo 'FAIL: test 128' | tee -a ${LOGFILE}
		fi

		cd ../../2/first-dir
		echo add a conflicting line >>a
		if ${CVS} ci -m changed >>${LOGFILE} 2>&1; then
			echo 'FAIL: test 129' | tee -a ${LOGFILE}
		else
			# Should be printing `out of date check failed'.
			echo 'PASS: test 129' >>${LOGFILE}
		fi

		if ${CVS} update 2>>${LOGFILE}; then
			# We should get a conflict, but that doesn't affect
			# exit status
			echo 'PASS: test 130' >>${LOGFILE}
		else
			echo 'FAIL: test 130' | tee -a ${LOGFILE}
		fi

		# Try to check in the file with the conflict markers in it.
		if ${CVS} ci -m try 2>>${LOGFILE}; then
			echo 'FAIL: test 131' | tee -a ${LOGFILE}
		else
			# Should tell us to resolve conflict first
			echo 'PASS: test 131' >>${LOGFILE}
		fi

		echo lame attempt at resolving it >>a
		# Try to check in the file with the conflict markers in it.
		if ${CVS} ci -m try >>${LOGFILE} 2>&1; then
			echo 'FAIL: test 132' | tee -a ${LOGFILE}
		else
			# Should tell us to resolve conflict first
			echo 'PASS: test 132' >>${LOGFILE}
		fi

		echo resolve conflict >a
		if ${CVS} ci -m resolved >>${LOGFILE} 2>&1; then
			echo 'PASS: test 133' >>${LOGFILE}
		else
			echo 'FAIL: test 133' | tee -a ${LOGFILE}
		fi

		# Now test that we can add a file in one working directory
		# and have an update in another get it.
		cd ../../1/first-dir
		echo abc >abc
		if ${testcvs} add abc >>${LOGFILE} 2>&1; then
			echo 'PASS: test 134' >>${LOGFILE}
		else
			echo 'FAIL: test 134' | tee -a ${LOGFILE}
		fi
		if ${testcvs} ci -m 'add abc' abc >>${LOGFILE} 2>&1; then
			echo 'PASS: test 135' >>${LOGFILE}
		else
			echo 'FAIL: test 135' | tee -a ${LOGFILE}
		fi
		cd ../../2
		if ${testcvs} -q update >>${LOGFILE}; then
			echo 'PASS: test 136' >>${LOGFILE}
		else
			echo 'FAIL: test 136' | tee -a ${LOGFILE}
		fi
		if test -f first-dir/abc; then
			echo 'PASS: test 137' >>${LOGFILE}
		else
			echo 'FAIL: test 137' | tee -a ${LOGFILE}
		fi

		# Now test something similar, but in which the parent directory
		# (not the directory in question) has the Entries.Static flag
		# set.
		cd ../1/first-dir
		mkdir subdir
		if ${testcvs} add subdir >>${LOGFILE}; then
			echo 'PASS: test 138' >>${LOGFILE}
		else
			echo 'FAIL: test 138' | tee -a ${LOGFILE}
		fi
		cd ../..
		mkdir 3
		cd 3
		if ${testcvs} -q co first-dir/abc first-dir/subdir \
		    >>${LOGFILE}; then
		  echo 'PASS: test 139' >>${LOGFILE}
		else
		  echo 'FAIL: test 139' | tee -a ${LOGFILE}
		fi
		cd ../1/first-dir/subdir
		echo sss >sss
		if ${testcvs} add sss >>${LOGFILE} 2>&1; then
		  echo 'PASS: test 140' >>${LOGFILE}
		else
		  echo 'FAIL: test 140' | tee -a ${LOGFILE}
		fi
		if ${testcvs} ci -m adding sss >>${LOGFILE} 2>&1; then
		  echo 'PASS: test 140' >>${LOGFILE}
		else
		  echo 'FAIL: test 140' | tee -a ${LOGFILE}
		fi
		cd ../../../3/first-dir
		if ${testcvs} -q update >>${LOGFILE}; then
		  echo 'PASS: test 141' >>${LOGFILE}
		else
		  echo 'FAIL: test 141' | tee -a ${LOGFILE}
		fi
		if test -f subdir/sss; then
		  echo 'PASS: test 142' >>${LOGFILE}
		else
		  echo 'FAIL: test 142' | tee -a ${LOGFILE}
		fi

		cd ../.. 
		rm -rf 1 2 3 ; rm -rf ${CVSROOT_FILENAME}/first-dir
		;;
	modules)
	  # The following line stolen from cvsinit.sh.  FIXME: create our
	  # repository via cvsinit.sh; that way we test it too.
	  (cd ${CVSROOT_FILENAME}/CVSROOT; ci -q -u -t/dev/null \
	    -m'initial checkin of modules' modules)

	  rm -rf first-dir ${CVSROOT_FILENAME}/first-dir
	  mkdir ${CVSROOT_FILENAME}/first-dir

	  mkdir 1
	  cd 1

	  if ${testcvs} -q co first-dir; then
	    echo 'PASS: test 143' >>${LOGFILE}
	  else
	    echo 'FAIL: test 143' | tee -a ${LOGFILE}
	  fi

	  cd first-dir
	  mkdir subdir
	  ${testcvs} add subdir >>${LOGFILE}
	  cd subdir

	  touch a

	  if ${testcvs} add a 2>>${LOGFILE} ; then
	    echo 'PASS: test 144' >>${LOGFILE}
	  else
	    echo 'FAIL: test 144' | tee -a ${LOGFILE}
	  fi

	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 145' >>${LOGFILE}
	  else
	    echo 'FAIL: test 145' | tee -a ${LOGFILE}
	  fi

	  cd ..
	  if ${testcvs} -q co CVSROOT >>${LOGFILE}; then
	    echo 'PASS: test 146' >>${LOGFILE}
	  else
	    echo 'FAIL: test 146' | tee -a ${LOGFILE}
	  fi

	  # Here we test that CVS can deal with CVSROOT (whose repository
	  # is at top level) in the same directory as subdir (whose repository
	  # is a subdirectory of first-dir).  TODO: Might want to check that
	  # files can actually get updated in this state.
	  if ${testcvs} -q update; then
	    echo 'PASS: test 147' >>${LOGFILE}
	  else
	    echo 'FAIL: test 147' | tee -a ${LOGFILE}
	  fi

	  echo realmodule first-dir/subdir a >>CVSROOT/modules
	  echo aliasmodule -a first-dir/subdir/a >>CVSROOT/modules
	  if ${testcvs} ci -m 'add modules' CVSROOT/modules \
	      >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 148' >>${LOGFILE}
	  else
	    echo 'FAIL: test 148' | tee -a ${LOGFILE}
	  fi
	  cd ..
	  if ${testcvs} co realmodule >>${LOGFILE}; then
	    echo 'PASS: test 149' >>${LOGFILE}
	  else
	    echo 'FAIL: test 149' | tee -a ${LOGFILE}
	  fi
	  if test -d realmodule && test -f realmodule/a; then
	    echo 'PASS: test 150' >>${LOGFILE}
	  else
	    echo 'FAIL: test 150' | tee -a ${LOGFILE}
	  fi
	  if ${testcvs} co aliasmodule >>${LOGFILE}; then
	    echo 'PASS: test 151' >>${LOGFILE}
	  else
	    echo 'FAIL: test 151' | tee -a ${LOGFILE}
	  fi
	  if test -d aliasmodule; then
	    echo 'FAIL: test 152' | tee -a ${LOGFILE}
	  else
	    echo 'PASS: test 152' >>${LOGFILE}
	  fi
	  echo abc >>first-dir/subdir/a
	  if (${testcvs} -q co aliasmodule | tee test153.tmp) \
	      >>${LOGFILE}; then
	    echo 'PASS: test 153' >>${LOGFILE}
	  else
	    echo 'FAIL: test 153' | tee -a ${LOGFILE}
	  fi
	  echo 'M first-dir/subdir/a' >ans153.tmp
	  if cmp test153.tmp ans153.tmp; then
	    echo 'PASS: test 154' >>${LOGFILE}
	  else
	    echo 'FAIL: test 154' | tee -a ${LOGFILE}
	  fi
	  if ${testcvs} -q co realmodule; then
	    echo 'PASS: test 155' >>${LOGFILE}
	  else
	    echo 'FAIL: test 155' | tee -a ${LOGFILE}
	  fi
	  cd ..
	  rm -rf 1 ; rm -rf ${CVSROOT_FILENAME}/first-dir
	  ;;
	mflag)
	  for message in '' ' ' '	
           ' '    	  	test' ; do
	    # Set up
	    mkdir a-dir; cd a-dir
	    # Test handling of -m during import
	    echo testa >>test
	    if ${testcvs} import -m "$message" a-dir A A1 >>${LOGFILE} 2>&1;then
	      echo 'PASS: test 156' >>${LOGFILE}
	    else
	      echo 'FAIL: test 156' | tee -a ${LOGFILE}
	    fi
	    # Must import twice since the first time uses inline code that
	    # avoids RCS call.
	    echo testb >>test
	    if ${testcvs} import -m "$message" a-dir A A2 >>${LOGFILE} 2>&1;then
	      echo 'PASS: test 157' >>${LOGFILE}
	    else
	      echo 'FAIL: test 157' | tee -a ${LOGFILE}
	    fi
	    # Test handling of -m during ci
	    cd ..; rm -rf a-dir;
	    if ${testcvs} co a-dir >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 158' >>${LOGFILE}
	    else
	      echo 'FAIL: test 158' | tee -a ${LOGFILE}
	    fi
	    cd a-dir
	    echo testc >>test
	    if ${testcvs} ci -m "$message" >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 159' >>${LOGFILE}
	    else
	      echo 'FAIL: test 159' | tee -a ${LOGFILE}
	    fi
	    # Test handling of -m during rm/ci
	    rm test;
	    if ${testcvs} rm test >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 160' >>${LOGFILE}
	    else
	      echo 'FAIL: test 160' | tee -a ${LOGFILE}
	    fi
	    if ${testcvs} ci -m "$message" >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 161' >>${LOGFILE}
	    else
	      echo 'FAIL: test 161' | tee -a ${LOGFILE}
	    fi
	    # Clean up
	    cd ..; rm -rf a-dir ${CVSROOT_FILENAME}/a-dir
	  done
	  ;;
	errmsg1)
	  mkdir ${CVSROOT_FILENAME}/1dir
	  mkdir 1
	  cd 1
	  if ${testcvs} -q co 1dir; then
	    echo 'PASS: test 162' >>${LOGFILE}
	  else
	    echo 'FAIL: test 162' | tee -a ${LOGFILE}
	  fi
	  cd 1dir
	  touch foo
	  if ${testcvs} add foo 2>>${LOGFILE}; then
	    echo 'PASS: test 163' >>${LOGFILE}
	  else
	    echo 'FAIL: test 163' | tee -a ${LOGFILE}
	  fi
	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 164' >>${LOGFILE}
	  else
	    echo 'FAIL: test 164' | tee -a ${LOGFILE}
	  fi
	  cd ../..
	  mkdir 2
	  cd 2
	  if ${testcvs} -q co 1dir >>${LOGFILE}; then
	    echo 'PASS: test 165' >>${LOGFILE}
	  else
	    echo 'FAIL: test 165' | tee -a ${LOGFILE}
	  fi
	  chmod a-w 1dir
	  cd ../1/1dir
	  rm foo; 
	  if ${testcvs} rm foo >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 166' >>${LOGFILE}
	  else
	    echo 'FAIL: test 166' | tee -a ${LOGFILE}
	  fi
	  if ${testcvs} ci -m removed >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 167' >>${LOGFILE}
	  else
	    echo 'FAIL: test 167' | tee -a ${LOGFILE}
	  fi
	  cd ../../2/1dir
	  ${testcvs} -q update 2>../tst167.err
	  cat <<EOF >../tst167.ans
cvs server: warning: foo is not (any longer) pertinent
cvs update: unable to remove ./foo: Permission denied
EOF
	  if cmp ../tst167.ans ../tst167.err >/dev/null ||
	  ( echo 'cvs [update aborted]: cannot rename file foo to CVS/,,foo: Permission denied' | cmp - ../tst167.err >/dev/null )
	  then
	    echo 'PASS: test 168' >>${LOGFILE}
	  else
	    echo 'FAIL: test 168' | tee -a ${LOGFILE}
	  fi

	  cd ..
	  chmod u+w 1dir
	  cd ..
	  rm -rf 1 2 ${CVSROOT_FILENAME}/1dir
	  ;;

	*) echo $what is not the name of a test -- ignored ;;
	esac
done

echo Ok.

# TODO:
# * Test `cvs admin'.
# * Test `cvs update -d foo' (where foo does not exist).
# * Test `cvs update foo bar' (where foo and bar are both from the same
#   repository).  Suppose one is a branch--make sure that both directories
#   get updated with the respective correct thing.
# * Zero length files (check in, check out).
# * `cvs update ../foo'.  Also ../../foo ./../foo foo/../../bar /foo/bar
#   foo/.././../bar foo/../bar etc.
# * Test all flags in modules file.
#   Test that ciprog gets run both on checkin in that directory, or a
#     higher-level checkin which recurses into it.
# * Test that $ followed by "Header" followed by $ gets expanded on checkin.
# * Test operations on a directory that contains other directories but has 
#   no files of its own.
# * -t global option
# * cvs rm followed by cvs add or vice versa (with no checkin in between).
# * cvs rm twice (should be a nice error message).
# * -P option to checkout--(a) refrains from checking out new empty dirs,
#   (b) prunes empty dirs already there.
# * Test that cvs -d `hostname`:/tmp/cvs-sanity/non/existent co foo
#   gives an appropriate error (e.g. 
#     Cannot access /tmp/cvs-sanity/non-existent/CVSROOT
#     No such file or directory).
# End of TODO list.

# Remove the test directory, but first change out of it.
cd /tmp
rm -rf ${TESTDIR}

# end of sanity.sh
