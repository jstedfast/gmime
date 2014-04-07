#!/bin/sh
# Run this to generate all the initial makefiles, etc.

test -n "$srcdir" || srcdir=`dirname $0`
test -n "$srcdir" || srcdir=.

ORIGDIR=`pwd`
cd $srcdir
PROJECT=GMime
TEST_TYPE=-f
FILE=configure.ac

DIE=0

have_libtool=false
if libtoolize --version < /dev/null > /dev/null 2>&1 ; then
	libtool_version=`libtoolize --version | sed '/^$/d;s/([^)]*)//g;s/^[^0-9]*//;s/[- ].*//g;q'`
	case $libtool_version in
	    1.4*|1.5*|2.2*|2.4*)
		have_libtool=true
		;;
	esac
fi
if $have_libtool ; then : ; else
	echo
	echo "You must have libtool 1.4 installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at http://ftp.gnu.org/gnu/libtool/"
	DIE=1
fi

(gtkdocize --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have gtk-doc installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnome.org/pub/GNOME/sources/gtk-doc/"
	DIE=1
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at http://ftp.gnu.org/gnu/autoconf/"
	DIE=1
}

for automake_version in 1.8 1.9 1.10 1.11 1.12 1.13 1.14; do
    if automake-${automake_version} --version < /dev/null > /dev/null 2>&1 ; then
        AUTOMAKE=automake-${automake_version}
        ACLOCAL=aclocal-${automake_version}
        break
    fi
done

if test -z "${AUTOMAKE}"; then
	echo
	echo "You must have automake >= 1.8.x installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at http://ftp.gnu.org/gnu/automake/"
	DIE=1
fi

if test "$DIE" -eq 1; then
	exit 1
fi

test $TEST_TYPE $FILE || {
	echo "You must run this script in the top-level $PROJECT directory"
	exit 1
}

if test -z "$AUTOGEN_SUBDIR_MODE"; then
        if test -z "$*"; then
                echo "I am going to run ./configure with no arguments - if you wish "
                echo "to pass any to it, please specify them on the $0 command line."
        fi
fi

$ACLOCAL -I m4/ $ACLOCAL_FLAGS || exit $?

libtoolize --force || exit $?
gtkdocize || exit $?

autoheader || exit $?

$AUTOMAKE --add-missing || exit $?
autoconf || exit $?
cd $ORIGDIR || exit $?

if test -z "$AUTOGEN_SUBDIR_MODE" && test -z "NOCONFIGURE"; then
        $srcdir/configure --enable-maintainer-mode $AUTOGEN_CONFIGURE_ARGS "$@" || exit $?

        echo 
        echo "Now type 'make' to compile $PROJECT."
fi
