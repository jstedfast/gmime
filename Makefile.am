## Process this file with automake to produce Makefile.in

SUBDIRS = gmime src tests doc .

bin_SCRIPTS = gmime-config

dist-hook: gmime.spec
	-cp gmime.spec $(distdir)
	(cd $(srcdir) ; tar -cf -) | (cd $(distdir); tar xf -)

## We create gmimeConf.sh here and not from configure because we want
## to get the paths expanded correctly.  Macros like srcdir are given
## the value NONE in configure if the user doesn't specify them (this
## is an autoconf feature, not a bug).

CLEANFILES = gmimeConf.sh iconv-detect.h

confexecdir = $(libdir)
confexec_DATA = gmimeConf.sh

EXTRA_DIST = 			\
	PORTING			\
	gmimeConf.sh.in 	\
	gmime-2.0.pc.in		\
	gmime.spec.in 		\
	iconv-detect.c		\
	zentimer.h 		\
	zenprofiler.h

BUILD_EXTRA_DIST = 		\
	gmime.spec

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gmime-2.0.pc

$(pkgconfig_DATA): config.status

gmimeConf.sh: gmimeConf.sh.in Makefile
## Use sed and then mv to avoid problems if the user interrupts.
	sed -e 's?\@GMIME_LIBDIR\@?$(GMIME_LIBDIR)?g' \
	    -e 's?\@GMIME_INCLUDEDIR\@?$(GMIME_INCLUDEDIR)?g' \
	    -e 's?\@GMIME_LIBS\@?$(GMIME_LIBS)?g' \
	    -e 's?\@VERSION\@?$(VERSION)?g' \
	      < $(srcdir)/gmimeConf.sh.in > gmimeConf.tmp \
	  && mv gmimeConf.tmp gmimeConf.sh

$(libgmime_2_0_la_SOURCES): $(srcdir)/gmime
