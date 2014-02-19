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
m4_define([gmime_major_version], [2])
m4_define([gmime_minor_version], [6])
m4_define([gmime_micro_version], [20])
m4_define([gmime_interface_age], [0])
m4_define([gmime_binary_age],
          [m4_eval(100 * gmime_minor_version + gmime_micro_version)])
m4_define([gmime_version],
          [gmime_major_version.gmime_minor_version.gmime_micro_version])

# the API version is the major.minor since the last API breakage
m4_define([gmime_api_major], [2])
m4_define([gmime_api_minor], [6])
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
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gmime])

AC_CONFIG_SRCDIR([README])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST

# Save this value here, since automake will set cflags later
cflags_set=${CFLAGS+set}

AM_INIT_AUTOMAKE([1.9 dist-bzip2 tar-ustar no-dist-gzip foreign -Wno-portability])
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
AC_CHECK_HEADERS(regex.h)
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

dnl Check for some time functions
AC_CHECK_FUNCS(strftime localtime gmtime_r gmtime_s)

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

dnl Enable stricter MIME parsing rules
AC_ARG_ENABLE(strict-parser,
              AC_HELP_STRING([--enable-strict-parser],
	      [enable stricter MIME parser rules [[default=no]]]),,
	      [enable_strict_parser="no"])
if test "x$enable_strict_parser" = "xyes"; then
  AC_DEFINE(STRICT_PARSER, 1, [Define if GMime should enable stricter parsing rules.])
fi

dnl Disable cryptography support
AC_ARG_ENABLE(cryptography,
              AC_HELP_STRING([--enable-cryptography],
	      [enable cryptography support [[default=yes]]]),,
	      [enable_cryptography="yes"])
AM_CONDITIONAL(ENABLE_CRYPTOGRAPHY, test "x$enable_cryptography" = "xyes")
if test "x$enable_cryptography" = "xyes"; then
  AC_DEFINE(ENABLE_CRYPTOGRAPHY, 1, [Define if cryptography in GMime is enabled.])
fi


dnl We need at *least* glib 2.16.0 for GIO and 2.18.0 for g_set_error_literal
AM_PATH_GLIB_2_0(2.18.0, ,
		 AC_MSG_ERROR(Cannot find GLIB: Is pkg-config in your path?),
		 gobject gmodule gthread gio)

dnl *****************************
dnl *** Checks for zlib       ***
dnl *****************************
AC_CHECK_HEADERS(zlib.h)
AC_CHECK_LIB(z, inflate, ZLIB="-lz")

dnl Check for libiconv
AM_ICONV()

CFLAGS_save="$CFLAGS"
CFLAGS="$CFLAGS -I$srcdir"
LIBS_save="$LIBS"
LIBS="$LIBS $LIBICONV"

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

dnl Timezone checks
AC_CACHE_CHECK(for tm_gmtoff in struct tm, ac_cv_struct_tm_gmtoff,
	AC_TRY_COMPILE([
		#include <time.h>
		], [
		struct tm tm;
		tm.tm_gmtoff = 1;
		return tm.tm_gmtoff;
		], ac_cv_struct_tm_gmtoff="yes", ac_cv_struct_tm_gmtoff="no"))
if test "$ac_cv_struct_tm_gmtoff" = "yes"; then
	AC_DEFINE(HAVE_TM_GMTOFF, 1, [Define if struct tm has a tm_gmtoff member])
else
	AC_CACHE_CHECK(for timezone variable, ac_cv_var_timezone,
		AC_TRY_COMPILE([
			#include <time.h>
		], [
			timezone = 1;
		], ac_cv_var_timezone="yes", ac_cv_var_timezone="no"))
	if test "$ac_cv_var_timezone" = "yes"; then
		AC_DEFINE(HAVE_TIMEZONE, 1, [Define if libc defines a timezone variable])
		AC_CACHE_CHECK(for altzone variable, ac_cv_var_altzone,
			AC_TRY_COMPILE([
				#include <time.h>
			], [
				altzone = 1;
			], ac_cv_var_altzone="yes", ac_cv_var_altzone="no"))
		if test "$ac_cv_var_altzone" = "yes"; then
			AC_DEFINE(HAVE_ALTZONE, 1, [Define if libc defines an altzone variable])
		fi
	else
		AC_CACHE_CHECK(for _timezone variable, ac_cv_var__timezone,
			AC_TRY_COMPILE([
				#include <time.h>
			], [
				_timezone = 1;
			], ac_cv_var__timezone="yes", ac_cv_var__timezone="no"))
		if test "$ac_cv_var__timezone" = "yes"; then
			AC_DEFINE(HAVE__TIMEZONE, 1, [Define if libc defines a _timezone variable])
		else
			AC_ERROR(unable to find a way to determine timezone)
		fi
	fi
fi

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

dnl ******************************************
dnl *** Checks for GpgME needed for S/MIME ***
dnl ******************************************
AC_ARG_ENABLE([smime], 
	      AC_HELP_STRING([--enable-smime],
	      [enable S/MIME support [[default=no]]]),,
	      [enable_smime="no"])

if test "x$enable_cryptography" != "xyes"; then
   enable_smime="no"
fi

m4_ifdef([AM_PATH_GPGME_PTHREAD], [
AS_IF([test "x$enable_smime" = "xyes"], [
   AM_PATH_GPGME_PTHREAD(1.1.6, enable_smime="yes (via GpgME)", enable_smime="no")
   if test "x$enable_smime" != "xno"; then
      AC_DEFINE(ENABLE_SMIME, 1, [Define if GMime should enable experimental S/MIME support.])
   fi
])
])

dnl ****************************
dnl *** Enable Mono bindings ***
dnl ****************************
AC_ARG_ENABLE([mono], 
	      AC_HELP_STRING([--enable-mono],
	      [enable Mono bindings [[default=auto]]]),,
	      [enable_mono="auto"])
if test "x$enable_mono" != "xno"; then
	AC_PATH_PROG(CSC, mcs, no)
	# if we found mcs, make sure it's mono and not something else named
	# mcs, such as Tru64's /usr/bin/mcs, for manipulating object file
	# comment sections of eCOFF object files.
	if test "x$CSC" != "xno"; then
		AC_MSG_CHECKING([whether $CSC is GNU Mono])
		$CSC --version > /dev/null 2>&1
		if test $? -ne 0; then
			CSC=no
			AC_MSG_RESULT([no])
		else
			AC_MSG_RESULT([yes])
		fi
	fi
else
	CSC="no"
fi

if test "x$CSC" = "xno"; then
	dnl error out if the user has explicitly requested mono bindings
	if test "x$enable_mono" = "xyes"; then
		AC_MSG_ERROR([Could not find mcs])
	fi
	
	enable_mono="no"
	AM_CONDITIONAL(ENABLE_MONO, false)
else
	enable_mono="yes"
	AM_CONDITIONAL(ENABLE_MONO, true)
fi

if test "x$enable_mono" = "xyes"; then
	AC_PATH_PROG(GACUTIL, gacutil, no)
	if test "x$GACUTIL" = "xno"; then
		AC_MSG_ERROR([Your mono installation doesn't expose gacutil])
	fi
	
	AC_SUBST(CSC)
	AC_SUBST(GACUTIL)
	
	PKG_CHECK_MODULES(GLIB_SHARP, glib-sharp-2.0 >= 2.4.0)
	AC_SUBST(GLIB_SHARP_LIBS)
	
	PKG_CHECK_MODULES(GAPI_TOOLS, gapi-2.0)
	AC_PATH_PROG(GAPI_CODEGEN, gapi2-codegen, no)
	if test "x$GAPI_CODEGEN" = "xno"; then
        	AC_MSG_ERROR([You need to install gtk-sharp2-gapi])
	fi
	
	AC_PATH_PROG(GAPI_FIXUP, gapi2-fixup, no)
	if test "x$GAPI_FIXUP" = "xno"; then
        	AC_MSG_ERROR([You need to install gtk-sharp2-gapi])
	fi
	
	AC_PATH_PROG(GAPI_PARSER, gapi2-parser, no)
	if test "x$GAPI_PARSER" = "xno"; then
        	AC_MSG_ERROR([You need to install gtk-sharp2-gapi])
	fi
	
	dnl The version should be along the lines of
	dnl "<major>.<minor>.0.<interface count>", where major and minor are
	dnl the current major and minor gmime versions, and interface count is
	dnl incremented any time the mono API changes, which should really
	dnl only happen within development series.
	MONO_INTERFACE_VERSION=0
	API_VERSION="$GMIME_MAJOR_VERSION.$GMIME_MINOR_VERSION.0.$MONO_INTERFACE_VERSION"
	AC_SUBST(API_VERSION)
fi

AC_ARG_WITH([gacdir], AC_HELP_STRING([--with-gacdir], [Specify the Global Assembly Cache root directory]),
            [[with_gacdir=$withval]],
            [[with_gacdir=\${prefix}/lib]])
gacdir=$with_gacdir
AC_SUBST(gacdir)

dnl Check for GObject introspection and Vala binding generator
GOBJECT_INTROSPECTION_CHECK([1.30.0])
VAPIGEN_CHECK

dnl Extra libs
EXTRA_LIBS="$ZLIB"
if test "x$LIBSOCKET" != "x"; then
	EXTRA_LIBS="$EXTRA_LIBS $LIBSOCKET"
fi
if test "x$LIBNSL" != "x"; then
	EXTRA_LIBS="$EXTRA_LIBS $LIBNSL"
fi
if test "x$LIBICONV" != "x"; then
	EXTRA_LIBS="$EXTRA_LIBS $LIBICONV"
fi
if test "x$GPGME_PTHREAD_LIBS" != "x"; then
	EXTRA_LIBS="$EXTRA_LIBS $GPGME_PTHREAD_LIBS"
fi

CFLAGS="$CFLAGS -fno-strict-aliasing"
LIBS="$LIBS $EXTRA_LIBS"

AC_SUBST(CFLAGS)
AC_SUBST(LIBS)

GMIME_CFLAGS="$LFS_CFLAGS $GPGME_PTHREAD_CFLAGS"
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
docs/Makefile
docs/reference/Makefile
docs/tutorial/Makefile
examples/Makefile
util/Makefile
gmime/Makefile
gmime/gmime-version.h
mono/Makefile
mono/AssemblyInfo.cs
mono/gmime-sharp.dll.config
mono/gmime-sharp.pc
tests/Makefile
tools/Makefile
gmime.spec
gmime.pc
)

echo "

Configuration:

  Source code location: ${srcdir}
  Install prefix:       ${prefix}
  Compiler:             ${CC}

  Profiling enabled:    ${enable_profiling}

  Large file support:   ${enable_largefile}
  Console warnings:     ${enable_warnings}
  PGP/MIME support:     ${enable_cryptography}
  S/MIME support:       ${enable_smime}
  Strict parser:        ${enable_strict_parser}

  Mono bindings:        ${enable_mono}
  Vala bindings:        ${enable_vala}
"
