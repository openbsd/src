#! /bin/sh
:
#
# Copyright (c) 1992, Brian Berliner
#
# You may distribute under the terms of the GNU General Public License as
# specified in the README file that comes with the CVS 1.4 kit.
#
# $CVSid: @(#)cvsinit.sh 1.1 94/10/22 $
#
# This script should be run once to help you setup your site for CVS.

# this line is edited by Makefile when creating cvsinit.inst
CVSLIB="xLIBDIRx"

CVS_VERSION="xVERSIONx"

# All purpose usage message, also suffices for --help and --version.
if test $# -gt 0; then
  echo "cvsinit version $CVS_VERSION"
  echo "usage: $0"
  echo "(set CVSROOT to the repository that you want to initialize)"
  exit 0
fi

# Make sure that the CVSROOT variable is set
if [ "x$CVSROOT" = x ]; then
    echo "The CVSROOT environment variable is not set."
    echo ""
    echo "You should choose a location for your source repository"
    echo "that can be shared by many developers.  It also helps to"
    echo "place the source repository on a file system that has"
    echo "plenty of free space."
    echo ""
    echo "Please enter the full path for your CVSROOT source repository:"
    read CVSROOT
    remind_cvsroot=yes
else
    remind_cvsroot=no
fi

# Now, create the $CVSROOT if it is not already there
if [ ! -d $CVSROOT ]; then
    echo "Creating $CVSROOT..."
    path=
    for comp in `echo $CVSROOT | sed -e 's,/, ,g'`; do
	path=$path/$comp
	if [ ! -d $path ]; then
	    mkdir $path
	fi
    done
else
    true
fi

# Next, check for $CVSROOT/CVSROOT
if [ ! -d $CVSROOT/CVSROOT ]; then
    if [ -d $CVSROOT/CVSROOT.adm ]; then
	echo "You have the old $CVSROOT/CVSROOT.adm directory."
	echo "I will rename it to $CVSROOT/CVSROOT for you..."
	mv $CVSROOT/CVSROOT.adm $CVSROOT/CVSROOT
    else
	echo "Creating the $CVSROOT/CVSROOT directory..."
	mkdir $CVSROOT/CVSROOT
    fi
else
    true
fi
if [ ! -d $CVSROOT/CVSROOT ]; then
    echo "Unable to create $CVSROOT/CVSROOT."
    echo "I give up."
    exit 1
fi

# Create the special *info files within $CVSROOT/CVSROOT

for info in modules loginfo commitinfo rcsinfo editinfo rcstemplate \
        checkoutlist; do
    if [ -f $CVSROOT/CVSROOT/${info},v ]; then
	if [ ! -f $CVSROOT/CVSROOT/$info ]; then
	    echo "Checking out $CVSROOT/CVSROOT/$info"
	    echo "  from $CVSROOT/CVSROOT/${info},v..."
	    (cd $CVSROOT/CVSROOT; co -q $info)
	fi
    else
	if [ -f $CVSROOT/CVSROOT/$info ]; then
	    echo "Checking in $CVSROOT/CVSROOT/${info},v"
	    echo "  from $CVSROOT/CVSROOT/$info..."
	else
	    echo "Creating a simple $CVSROOT/CVSROOT/$info file..."
	    case $info in
	      modules)
		sed -n -e '/END_REQUIRED_CONTENT/q' \
		    -e p $CVSLIB/examples/modules > $CVSROOT/CVSROOT/modules
		;;
	      loginfo)
		# try to find perl; use fancy log script if we can
		for perlpath in `echo $PATH | sed -e 's/:/ /g'` x; do
		    if [ -f $perlpath/perl ]; then
			echo "#!$perlpath/perl" > $CVSROOT/CVSROOT/log
			cat $CVSLIB/contrib/log >> $CVSROOT/CVSROOT/log
			chmod 755 $CVSROOT/CVSROOT/log
			cp $CVSLIB/examples/loginfo $CVSROOT/CVSROOT/loginfo
			break
		    fi
		done
		if [ $perlpath = x ]; then
		    # we did not find perl, so make a simple loginfo file
		    grep '^#' $CVSLIB/examples/loginfo \
			> $CVSROOT/CVSROOT/loginfo
		    cat >> $CVSROOT/CVSROOT/loginfo <<"END_HERE_DOC"
DEFAULT		(echo ""; echo $USER; date; cat) >> $CVSROOT/CVSROOT/commitlog
END_HERE_DOC
		fi
		;;
	      rcstemplate)
		cp $CVSLIB/examples/$info $CVSROOT/CVSROOT/$info
		;;
	      *)
		sed -e 's/^\([^#]\)/#\1/' $CVSLIB/examples/$info \
		    > $CVSROOT/CVSROOT/$info
		;;
	    esac
	fi
	(cd $CVSROOT/CVSROOT; ci -q -u -t/dev/null -m"initial checkin of $info" $info)
    fi
done

# check to see if there are any references to the old CVSROOT.adm directory
if grep CVSROOT.adm $CVSROOT/CVSROOT/modules >/dev/null 2>&1; then
    echo "Warning: your $CVSROOT/CVSROOT/modules file still"
    echo "	contains references to the old CVSROOT.adm directory"
    echo "	You should really change these to the new CVSROOT directory"
    echo ""
fi

# These files are generated from the contrib files.
# FIXME: Is it really wise to overwrite local changes like this?
# Shouldn't anything which is really supposed to be upgraded with new
# versions of CVS be in the CVS binary, not the repository?
# Shouldn't we at *least* version control the file so they can get
# back their editted version after we clobber it?
for contrib in commit_prep log_accum cln_hist; do
    echo "Copying the new version of '${contrib}'"
    echo "  to $CVSROOT/CVSROOT for you..."
    cp $CVSLIB/contrib/$contrib $CVSROOT/CVSROOT/$contrib
done

# XXX - also add a stub for the cvsignore file

# Turn on history logging by default
if [ ! -f $CVSROOT/CVSROOT/history ]; then
    echo "Enabling CVS history logging..."
    touch $CVSROOT/CVSROOT/history
fi

# finish up by running mkmodules
echo "All done!  Running 'mkmodules' as my final step..."
mkmodules $CVSROOT/CVSROOT

exit 0
