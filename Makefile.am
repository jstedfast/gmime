## Process this file with automake to produce Makefile.in

SUBDIRS = util gmime src 

if ENABLE_MONO
SUBDIRS += mono
endif

SUBDIRS += tests docs

if !PLATFORM_WIN32
SUBDIRS += examples
endif

SUBDIRS += .

bin_SCRIPTS = gmime-config

## We create gmimeConf.sh here and not from configure because we want
## to get the paths expanded correctly.  Macros like srcdir are given
## the value NONE in configure if the user doesn't specify them (this
## is an autoconf feature, not a bug).

CLEANFILES = gmimeConf.sh
DISTCLEANFILES = iconv-detect.h

confexecdir = $(libdir)
confexec_DATA = gmimeConf.sh

EXTRA_DIST = 			\
	PORTING			\
	gmimeConf.sh.in 	\
	gmime-2.0.pc.in		\
	gmime.spec.in 		\
	iconv-detect.c		\
	zentimer.h 		\
	zenprofiler.h		\
	gtk-doc.make

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

dist-hook: gmime.spec
	-cp gmime.spec $(distdir)
	(cd $(srcdir) ; tar -cf -) | (cd $(distdir); tar xf -)

# This is a version of the automake-1.7 distcheck rule modified to
# pass --enable-gtk-doc to ./configure and to not chmod distdir to
# 0444 so that the gtk-docs can actually build.
#
mydistcheck: dist
	-rm -rf $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) zxf $(distdir).tar.gz
	mkdir $(distdir)/=build
	mkdir $(distdir)/=inst
	dc_install_base=`cd $(distdir)/=inst && pwd`; \
	cd $(distdir)/=build \
	  && ../configure --srcdir=.. --prefix=$$dc_install_base --enable-gtk-doc \
	  && $(MAKE) $(AM_MAKEFLAGS) \
	  && $(MAKE) $(AM_MAKEFLAGS) dvi \
	  && $(MAKE) $(AM_MAKEFLAGS) check \
	  && $(MAKE) $(AM_MAKEFLAGS) install \
	  && $(MAKE) $(AM_MAKEFLAGS) installcheck \
	  && $(MAKE) $(AM_MAKEFLAGS) uninstall \
	  && $(MAKE) $(AM_MAKEFLAGS) dist
	-rm -rf $(distdir)
	@banner="$(distdir).tar.gz is ready for distribution"; \
	dashes=`echo "$$banner" | sed s/./=/g`; \
	echo "$$dashes"; \
	echo "$$banner"; \
	echo "$$dashes"
