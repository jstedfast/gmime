# Process this file with autoconf to produce a configure script.
# require autoconf 2.54
AC_PREREQ(2.54)

# Making releases:
#   gmime_micro_version += 1;
#   gmime_interface_age += 1;
#   gmime_binary_age += 1;
# if any functions have been added, set gmime_interface_age to 0.
# if backwards compatibility has been broken,
# set gmime_binary_age _and_ gmime_interface_age to 0.
#
m4_define([gmime_major_version], [3])
m4_define([gmime_minor_version], [2])
m4_define([gmime_micro_version], [6])
m4_define([gmime_interface_age], [0])
m4_define([gmime_binary_age],
          [m4_eval(100 * gmime_minor_version + gmime_micro_version)])
m4_define([gmime_version],
          [gmime_major_version.gmime_minor_version.gmime_micro_version])

# the API version is the major.minor since the last API breakage
m4_define([gmime_api_major], [3])
m4_define([gmime_api_minor], [0])
m4_define([gmime_api_version], [gmime_api_major.gmime_api_minor])

# libtool version related macros
m4_define([gmime_lt_release], [gmime_major_version.gmime_minor_version])
m4_define([gmime_lt_current],
          [m4_eval(100 * gmime_minor_version + gmime_micro_version - gmime_interface_age)])
m4_define([gmime_lt_revision], [gmime_interface_age])
m4_define([gmime_lt_age], [m4_eval(gmime_binary_age - gmime_interface_age)])
m4_define([gmime_lt_current_minus_age],
          [m4_eval(gmime_lt_current - gmime_lt_age)])

# if the minor version number is odd, then we want debugging.  Otherwise
# we only want minimal debugging support.
m4_define([gmime_debug_default],
          [m4_if(m4_eval(gmime_minor_version % 2), [1], [yes], [minimum])])


AC_INIT(gmime, [gmime_version],
        [https://github.com/jstedfast/gmime/issues])

AC_CONFIG_SRCDIR([README.md])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST

# Save this value here, since automake will set cflags later
cflags_set=${CFLAGS+set}

AM_INIT_AUTOMAKE([1.9 -Wno-portability no-define no-dist-gzip dist-xz tar-ustar])
AC_SUBST([ACLOCAL_AMFLAGS], ["-I m4 \${ACLOCAL_FLAGS}"])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])],
         [AC_SUBST([AM_DEFAULT_VERBOSITY], [1])])
dnl m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE


GMIME_MAJOR_VERSION=gmime_major_version
GMIME_MINOR_VERSION=gmime_minor_version
GMIME_MICRO_VERSION=gmime_micro_version
GMIME_INTERFACE_AGE=gmime_interface_age
GMIME_BINARY_AGE=gmime_binary_age
GMIME_API_VERSION=gmime_api_version
GMIME_VERSION=gmime_version

AC_SUBST(GMIME_MAJOR_VERSION)
AC_SUBST(GMIME_MINOR_VERSION)
AC_SUBST(GMIME_MICRO_VERSION)
AC_SUBST(GMIME_API_VERSION)
AC_SUBST(GMIME_VERSION)
AC_SUBST(GMIME_INTERFACE_AGE)
AC_SUBST(GMIME_BINARY_AGE)

AC_DEFINE(GMIME_VERSION, ["gmime_version"],
	  [Define to the GMime version])

# libtool versioning
LT_RELEASE=gmime_lt_release
LT_CURRENT=gmime_lt_current
LT_REVISION=gmime_lt_revision
LT_AGE=gmime_lt_age
LT_CURRENT_MINUS_AGE=gmime_lt_current_minus_age
AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)
AC_SUBST(LT_CURRENT_MINUS_AGE)

dnl Check for win32 platforms
AC_MSG_CHECKING([if building for Win32])
LIB_EXE_MACHINE_FLAG=X86
case "$host" in
  *-*-mingw*)
    platform_win32="yes"
    native_win32="yes"
    case "$host" in
    x86_64-*-*)
      LIB_EXE_MACHINE_FLAG=X64
      ;;
    esac
    ;;
  *)
    platform_win32="no"
    native_win32="no"
    ;;
esac

AC_MSG_RESULT([$platform_win32])

AC_SUBST(LIB_EXE_MACHINE_FLAG)

AM_CONDITIONAL(PLATFORM_WIN32, [test "x$platform_win32" = "xyes"])
AM_CONDITIONAL(OS_WIN32, test ["$native_win32" = "xyes"])
AM_CONDITIONAL(OS_WIN32_X64, [test "$LIB_EXE_MACHINE_FLAG" = "X64"])

if test "$glib_native_win32" = "yes"; then
  AC_CHECK_TOOL(WINDRES, windres, no)
  if test "$WINDRES" = no; then
    AC_MSG_ERROR([*** Could not find an implementation of windres in your PATH.])
  fi
  AC_CHECK_TOOL(NM, nm, no)
  if test "$NM" = no; then
    AC_MSG_ERROR([*** Could not find an implementation of nm in your PATH.])
  fi
  AC_CHECK_TOOL(RANLIB, ranlib, :)
  AC_CHECK_PROG(ms_librarian, [lib.exe], [yes], [no])
fi
AM_CONDITIONAL(MS_LIB_AVAILABLE, [test x$ms_librarian = xyes])

dnl Checks for programs.
AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_C_INLINE
dnl AC_HEADER_STDC
AC_STDC_HEADERS
dnl AC_ARG_PROGRAM
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PATH_PROG(RM, rm, /bin/rm)
AC_PATH_PROG(MV, mv, /bin/mv)
AC_PATH_PROG(TAR, tar, /bin/tar)

AC_EXEEXT
AC_PROG_LIBTOOL
dnl when using libtool 2.x create libtool early, because it's used in configure
m4_ifdef([LT_OUTPUT], [LT_OUTPUT])
DOLT

dnl Checks for header files.
AC_CHECK_HEADERS(sys/mman.h)
AC_CHECK_HEADERS(sys/param.h)
AC_CHECK_HEADERS(winsock2.h)
AC_CHECK_HEADERS(string.h)
AC_CHECK_HEADERS(stdlib.h)
AC_CHECK_HEADERS(netdb.h)
AC_CHECK_HEADERS(time.h)
AC_CHECK_HEADERS(poll.h)

AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_MODE_T

dnl Check for nfds_t - used by poll()
AC_MSG_CHECKING(for nfds_t)
AC_TRY_COMPILE([
	#include <sys/types.h>
	#include <poll.h>
	], [
	nfds_t nfds = 1;
	return nfds;
	]
,
	AC_MSG_RESULT(yes)
,
	AC_DEFINE(nfds_t, unsigned long int, [Define to `unsigned long int' if <poll.h> does not define.])
	AC_MSG_RESULT(no)
)

dnl Check for working mmap
AC_FUNC_MMAP
AC_CHECK_FUNCS(munmap msync)

dnl Check for select() and poll()
AC_CHECK_FUNCS(select poll)

dnl ************************************
dnl Checks for gtk-doc and docbook-tools
dnl ************************************

GTK_DOC_CHECK([1.8])

AC_PATH_PROGS([DB2HTML], [db2html docbook2html])
AM_CONDITIONAL(HAVE_DOCBOOK, [test -n "$DB2HTML"])

dnl NOTE: We need to use a separate automake conditional for this
dnl       to make this work with the tarballs.
AM_CONDITIONAL(ENABLE_GTK_DOC, test "x$enable_gtk_doc" = "xyes")

dnl Check for profiling options
AC_ARG_ENABLE([profiling],
              AC_HELP_STRING([--enable-profiling],
	      [enable profiling compile flags [[default=no]]]),,
              [enable_profiling="no"])
if test "x$enable_profiling" = "xyes"; then
# CFLAGS="-O0 -g -pg -fprofile-arcs -ftest-coverage"
  CFLAGS="-O0 -g -pg"
fi

dnl Enable warning spewage on the console
AC_ARG_ENABLE([warnings],
              AC_HELP_STRING([--enable-warnings],
	      [enable g_warning output when invalid MIME is encountered [[default=no]]]),,
	      [enable_warnings="no"])
if test "x$enable_warnings" = "xyes"; then
  AC_DEFINE(ENABLE_WARNINGS, 1, [Define if GMime should enable warning output.])
fi

dnl ***********************
dnl *** Tests for iconv ***
dnl ***********************
dnl
dnl We do this before the gettext checks, to avoid distortion

dnl On Windows we use a native implementation

AS_IF([ test x"$glib_native_win32" = xyes], [
  with_libiconv=native
], [
  AC_ARG_WITH(libiconv,
	      [AS_HELP_STRING([--with-libiconv=@<:@no/gnu/native@:>@],
			      [use the libiconv library])],,
	      [with_libiconv=maybe])

  found_iconv=no
  case $with_libiconv in
    maybe)
      # Check in the C library first
      AC_CHECK_FUNC(iconv_open, [with_libiconv=no; found_iconv=yes])
      # Check if we have GNU libiconv
      if test $found_iconv = "no"; then
	AC_CHECK_LIB(iconv, libiconv_open, [with_libiconv=gnu; found_iconv=yes])
      fi
      # Check if we have a iconv in -liconv, possibly from vendor
      if test $found_iconv = "no"; then
	AC_CHECK_LIB(iconv, iconv_open, [with_libiconv=native; found_iconv=yes])
      fi
      ;;
    no)
      AC_CHECK_FUNC(iconv_open, [with_libiconv=no; found_iconv=yes])
      ;;
    gnu|yes)
      AC_CHECK_LIB(iconv, libiconv_open, [with_libiconv=gnu; found_iconv=yes])
      ;;
    native)
      AC_CHECK_LIB(iconv, iconv_open, [with_libiconv=native; found_iconv=yes])
      ;;
  esac

  if test "x$found_iconv" = "xno" ; then
     AC_MSG_ERROR([*** No iconv() implementation found in C library or libiconv])
  fi
])

dnl
dnl Now we are done with gettext checks, figure out ICONV_LIBS
dnl

AS_IF([test x"$glib_native_win32" != xyes], [
  if test x$with_libiconv != xno ; then
    case " $INTLLIBS " in
    *[[\ \	]]-liconv[[\ \	]]*) ;;
    *) ICONV_LIBS="-liconv" ;;
    esac
  fi
])
AC_SUBST(ICONV_LIBS)

case $with_libiconv in
  gnu)
    AC_DEFINE(USE_LIBICONV_GNU, 1, [Using GNU libiconv])
    ;;
  native)
    AC_DEFINE(USE_LIBICONV_NATIVE, 1, [Using a native implementation of iconv in a separate library])
    ;;
esac

dnl
dnl zlib support
dnl
PKG_CHECK_MODULES([ZLIB], [zlib], [found_zlib=yes], [found_zlib=no])
AS_IF([test "x$found_zlib" = "xno"], [
  AC_CHECK_LIB(z, inflate, [AC_CHECK_HEADER(zlib.h, found_zlib=yes)])
  if test "x$found_zlib" = "xno" ; then
    AC_MSG_ERROR([*** Working zlib library and headers not found ***])
  fi
  ZLIB_LIBS='-lz'
  AC_SUBST(ZLIB_LIBS)
])

dnl We need at *least* glib 2.16.0 for GIO, 2.18.0 for g_set_error_literal, and 2.32.0 for g_mutex_init
AM_PATH_GLIB_2_0(2.32.0, ,
		 AC_MSG_ERROR(Cannot find GLIB: Is pkg-config in your path?),
		 gobject gmodule gthread gio)

CFLAGS_save="$CFLAGS"
CFLAGS="$CFLAGS -I$srcdir"
LIBS_save="$LIBS"
LIBS="$LIBS $ICONV_LIBS"

AC_CACHE_CHECK([preferred charset formats for system iconv], [ac_cv_have_iconv_detect_h], [
	AC_RUN_IFELSE([AC_LANG_SOURCE([
		#define CONFIGURE_IN
		#include "iconv-detect.c"
		])],
	[ac_cv_have_iconv_detect_h=yes], [ac_cv_have_iconv_detect_h=no],
	[
		AC_MSG_RESULT([cannot run test program while cross compiling])
		AC_MSG_ERROR([Copy target system's iconv-detect.h to . and configure with ac_cv_have_iconv_detect_h=yes])
	])
])
if test "x$ac_cv_have_iconv_detect_h" = xyes; then
	AC_MSG_RESULT([found])
	AC_DEFINE(HAVE_ICONV_DETECT_H, 1, [Define to 1 to use auto-detected iconv-friendly charset names.])
else
	AC_MSG_RESULT([not found
	*** The iconv-detect program was unable to determine the
	*** preferred charset formats recognized by your system
	*** iconv library. It is suggested that you install a
	*** working iconv library such as the one found at
	*** ftp://ftp.gnu.org/pub/gnu/libiconv
	*** 
	*** Default charset formats will be used.
	])
fi

CFLAGS="$CFLAGS_save"
LIBS="$LIBS_save"

dnl Check for GNU getopt
AC_CHECK_HEADER(getopt.h)
AC_MSG_CHECKING(for GNU getopt_long)
AC_TRY_LINK([
	#include <string.h>
	#include <unistd.h>
	#define _GNU_SOURCE
	#include <getopt.h>
	], [
	return getopt_long (0, NULL, NULL, NULL, NULL);
	]
,
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_GETOPT_H, 1, [Define to 1 if you have the <getopt.h> header with the GNU `getopt_long' function.])
	have_getopt="yes"
,
	AC_MSG_RESULT([no])
	have_getopt="no"
)
AM_CONDITIONAL(SYSTEM_GETOPT, test "x$have_getopt" = "xyes")

dnl Check for fsync (native win32 doesn't have this)
AC_MSG_CHECKING(for fsync)
AC_TRY_LINK([
	#include <unistd.h>
	], [
	fsync(0);
	]
,
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_FSYNC, 1, [Define to 1 if you have the `fsync' function.])
,
	AC_MSG_RESULT(no)
)

dnl Check for MAXHOSTNAMELEN
AC_MSG_CHECKING(for MAXHOSTNAMELEN)
AC_TRY_COMPILE([
	#include <sys/param.h>
	], [
	return MAXHOSTNAMELEN;
	]
,
	AC_MSG_RESULT(yes)
,
	AC_DEFINE(MAXHOSTNAMELEN, 64, [Define with a value if your <sys/param.h> does not define MAXHOSTNAMELEN])
	AC_MSG_RESULT(no; defined as 64)
)

dnl Check for domainname member in struct utsname from uname(2)
AC_MSG_CHECKING(for domainname in struct utsname)
AC_TRY_COMPILE([
	#define _GNU_SOURCE
	#include <sys/utsname.h>
	], [
	struct utsname unam;
		
	unam.domainname[0] = '\0';

	return unam.domainname[0];
	],
	AC_MSG_RESULT(yes)
   	AC_DEFINE(HAVE_UTSNAME_DOMAINNAME, 1, [Define if struct utsname has a domainname member])
,
	AC_MSG_RESULT(no)
)

dnl *************************************
dnl *** Checks for large file support ***
dnl *************************************
AC_ARG_ENABLE([largefile],
	      AC_HELP_STRING([--enable-largefile],
	      [enable support for large files [[default=yes]]]),,
	      [enable_largefile="yes"])
if test "x$enable_largefile" != "xno"; then
    AC_SYS_LARGEFILE
    AC_CACHE_CHECK([for _LARGEFILE64_SOURCE value needed for large files], ac_cv_largefile64_source,
    [
	AC_TRY_COMPILE([
		#include <sys/types.h>
		#include <sys/stat.h>
		#include <fcntl.h>
	],[
		return open ("__o_largefile", O_CREAT | O_RDWR | O_LARGEFILE, 0644);
	],[
		ac_cv_largefile64_source="no"
	],[
		AC_TRY_COMPILE([
			#define _LARGEFILE64_SOURCE
			#include <sys/types.h>
			#include <sys/stat.h>
			#include <fcntl.h>
		],[
			return open ("__o_largefile", O_CREAT | O_RDWR | O_LARGEFILE, 0644);
		],[
			ac_cv_largefile64_source="yes"
		],[
			ac_cv_largefile64_source="unknown"
		])
	])
    ])
    
    enable_largefile="no"
    
    if test "x$ac_cv_largefile64_source" = "xyes"; then
        LFS_CFLAGS="-D_LARGEFILE64_SOURCE"
        enable_largefile="yes"
    elif test "x$ac_cv_largefile64_source" = "xunknown"; then
        AC_DEFINE(O_LARGEFILE, 0, [Define to 0 if your system does not have the O_LARGEFILE flag])
    fi
    
    if test -n "$ac_cv_sys_large_files" -a "x$ac_cv_sys_large_files" != "xno"; then
        LFS_CFLAGS="$LFS_CFLAGS -D_LARGE_FILES"
        enable_largefile="yes"
    fi
    
    if test "x$ac_cv_sys_file_offset_bits" != "xno"; then
        LFS_CFLAGS="$LFS_CFLAGS -D_FILE_OFFSET_BITS=$ac_cv_sys_file_offset_bits"
        enable_largefile="yes"
    fi
else
    LFS_CFLAGS=""
fi
AM_CONDITIONAL(ENABLE_LARGEFILE, test "x$enable_largefile" = "xyes")

dnl Check type sizes
CFLAGS_save="$CFLAGS"
CFLAGS="$CFLAGS $LFS_CFLAGS"
AC_CHECK_SIZEOF(ssize_t)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(off_t)
AC_CHECK_SIZEOF(time_t)
CFLAGS="$CFLAGS_save"

dnl Check for some network functions
AC_CHECK_FUNCS(gethostname getdomainname getaddrinfo)

dnl ****************************
dnl *** Checks for libsocket ***
dnl ****************************
LIBSOCKET=""
AC_CHECK_LIB(socket, getaddrinfo, LIBSOCKET="-lsocket")

dnl *************************
dnl *** Checks for libnsl ***
dnl *************************
LIBNSL=""
AC_CHECK_LIB(nsl, getaddrinfo, LIBNSL="-lnsl")

dnl **************************************************
dnl *** Checks for GpgME needed for PGP and S/MIME ***
dnl **************************************************
AC_ARG_ENABLE([crypto], 
	      AC_HELP_STRING([--enable-crypto],
	      [enable PGP and S/MIME support [[default=yes]]]),,
	      [enable_crypto="yes"])

m4_ifdef([AM_PATH_GPGME_PTHREAD], [
AS_IF([test "x$enable_crypto" = "xyes"], [
   AM_PATH_GPGME_PTHREAD(1.6.0, enable_crypto="yes (via GpgME)", enable_crypto="no")
   if test "x$enable_crypto" != "xno"; then
      AC_DEFINE(ENABLE_CRYPTO, 1, [Define if GMime should enable GpgME PGP and S/MIME support.])
   fi
])
])
AM_CONDITIONAL(ENABLE_CRYPTO, test "x$enable_crypto" != "xno")

dnl ******************************
dnl *** Checks for GNU Libidn2 ***
dnl ******************************
AC_ARG_WITH(libidn, AC_HELP_STRING([--with-libidn=[DIR]],
                                [Support IDN (needs GNU Libidn)]),
  libidn=$withval, libidn=yes)
if test "$libidn" != "no" ; then
  PKG_CHECK_MODULES(LIBIDN, libidn2 >= 2.0.0, [libidn=yes], [libidn=no])
  if test "$libidn" != "yes" ; then
    libidn=no
    AC_MSG_WARN([Libidn not found])
  else
    libidn=yes
    AC_DEFINE(LIBIDN, 1, [Define to 1 if GNU Libidn should be used.])
  fi
fi
AC_MSG_CHECKING([if Libidn should be used])
AC_MSG_RESULT($libidn)

dnl Check for GObject introspection and Vala binding generator
GOBJECT_INTROSPECTION_CHECK([1.30.0])
VAPIGEN_CHECK

dnl ************************************
dnl *** Enable lcov coverage reports ***
dnl ************************************

AC_ARG_ENABLE(coverage,
  AS_HELP_STRING([--enable-coverage],
		 [enable coverage testing with gcov]),
  [enable_coverage=$enableval], [enable_coverage=no])

AS_IF([ test "x$enable_coverage" = "xyes"], [
  dnl we need gcc:
  if test "$GCC" != "yes"; then
    AC_MSG_ERROR([GCC is required for --enable-coverage])
  fi

  dnl Check if ccache is being used
  AC_CHECK_PROG(SHTOOL, shtool, shtool)
  case `$SHTOOL path $CC` in
    *ccache*[)] gcc_ccache=yes;;
    *[)] gcc_ccache=no;;
  esac

  if test "$gcc_ccache" = "yes" && (test -z "$CCACHE_DISABLE" || test "$CCACHE_DISABLE" != "1"); then
    AC_MSG_ERROR([ccache must be disabled when --enable-coverage option is used. You can disable ccache by setting environment variable CCACHE_DISABLE=1.])
  fi

  ltp_version_list="1.6 1.7 1.8 1.9 1.10 1.11 1.12 1.13"
  AC_CHECK_PROG(LTP, lcov, lcov)
  AC_CHECK_PROG(LTP_GENHTML, genhtml, genhtml)

  AS_IF([ test "$LTP" ], [
    AC_CACHE_CHECK([for ltp version], glib_cv_ltp_version, [
      glib_cv_ltp_version=invalid
      ltp_version=`$LTP -v 2>/dev/null | $SED -e 's/^.* //'`
      for ltp_check_version in $ltp_version_list; do
        if test "$ltp_version" = "$ltp_check_version"; then
          glib_cv_ltp_version="$ltp_check_version (ok)"
        fi
      done
    ])
  ], [
    ltp_msg="To enable code coverage reporting you must have one of the following LTP versions installed: $ltp_version_list"
    AC_MSG_ERROR([$ltp_msg])
  ])

  case $glib_cv_ltp_version in
    ""|invalid[)]
      ltp_msg="You must have one of the following versions of LTP: $ltp_version_list (found: $ltp_version)."
      AC_MSG_ERROR([$ltp_msg])
      LTP="exit 0;"
      ;;
  esac

  if test -z "$LTP_GENHTML"; then
    AC_MSG_ERROR([Could not find genhtml from the LTP package])
  fi

  dnl Remove all optimization flags from CFLAGS
  changequote({,})
  CFLAGS=`echo "$CFLAGS" | $SED -e 's/-O[0-9]*//g'`
  changequote([,])

  dnl Add the special gcc flags
  CFLAGS="$CFLAGS -O0 -fprofile-arcs -ftest-coverage"
  LDFLAGS="$LDFLAGS -lgcov"
])

dnl Extra libs
EXTRA_LIBS="$ZLIB_LIBS"
if test "x$LIBSOCKET" != "x"; then
	EXTRA_LIBS="$EXTRA_LIBS $LIBSOCKET"
fi
if test "x$LIBNSL" != "x"; then
	EXTRA_LIBS="$EXTRA_LIBS $LIBNSL"
fi
if test "x$ICONV_LIBS" != "x"; then
	EXTRA_LIBS="$EXTRA_LIBS $ICONV_LIBS"
fi
if test "x$GPGME_PTHREAD_LIBS" != "x"; then
	EXTRA_LIBS="$EXTRA_LIBS $GPGME_PTHREAD_LIBS"
fi
if test "x$LIBIDN_LIBS" != "x"; then
   	EXTRA_LIBS="$EXTRA_LIBS $LIBIDN_LIBS"
fi

CFLAGS="$CFLAGS -fno-strict-aliasing"
# enable more warnings when building from Git (assume we use gcc/clang)
# add -Werror to make them fatal (so they get fixed, actually...)
if test -d "${srcdir}/.git"; then
	CFLAGS="$CFLAGS -Wall -Wshadow -Wunreachable-code -Wformat=2"
fi
LIBS="$LIBS $EXTRA_LIBS"

AC_SUBST(CFLAGS)
AC_SUBST(LIBS)

GMIME_CFLAGS="$LFS_CFLAGS $GPGME_PTHREAD_CFLAGS $LIBIDN_CFLAGS"
GMIME_LIBDIR="-L${libdir}"
GMIME_INCLUDEDIR="-I${includedir}/gmime-$GMIME_API_VERSION"
GMIME_LIBS_PRIVATE="$EXTRA_LIBS"
GMIME_LIBS="-lgmime-$GMIME_API_VERSION"

AC_SUBST(GMIME_LIBDIR)
AC_SUBST(GMIME_INCLUDEDIR)
AC_SUBST(GMIME_LIBS_PRIVATE)
AC_SUBST(GMIME_CFLAGS)
AC_SUBST(GMIME_LIBS)

AC_OUTPUT(
Makefile
m4/Makefile
build/Makefile
build/vs2008/Makefile
build/vs2008/config-win32.h
build/vs2010/Makefile
build/vs2010/config-win32.h
build/vs2017/Makefile
docs/Makefile
docs/reference/Makefile
docs/tutorial/Makefile
examples/Makefile
util/Makefile
gmime/Makefile
gmime/gmime-version.h
tests/Makefile
tools/Makefile
gmime.spec
gmime.pc
)

echo "

Configuration:

  Source code location:  ${srcdir}
  Install prefix:        ${prefix}
  Compiler:              ${CC}

  Documentation:         ${enable_gtk_doc}

  Profiling enabled:     ${enable_profiling}
  Coverage enabled:      ${enable_coverage}

  Large file support:    ${enable_largefile}
  Console warnings:      ${enable_warnings}
  PGP/MIME support:      ${enable_crypto}
  S/MIME support:        ${enable_crypto}
  libidn support:        ${libidn}

  GObject introspection: ${enable_introspection}
  Vala bindings:         ${enable_vala}
"
