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

## We create gmimeConf.sh here and not from configure because we want
## to get the paths expanded correctly.  Macros like srcdir are given
## the value NONE in configure if the user doesn't specify them (this
## is an autoconf feature, not a bug).

DISTCLEANFILES = iconv-detect.h

EXTRA_DIST = 			\
	PORTING			\
	gmime-2.4.pc.in		\
	gmime.spec.in 		\
	iconv-detect.c		\
	zentimer.h 		\
	zenprofiler.h		\
	gtk-doc.make

BUILD_EXTRA_DIST = 		\
	gmime.spec

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gmime-2.4.pc

$(pkgconfig_DATA): config.status

$(libgmime_2_4_la_SOURCES): $(srcdir)/gmime

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
