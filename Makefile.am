## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = m4 build util gmime src 

if ENABLE_MONO
SUBDIRS += mono
endif

SUBDIRS += tests docs

if !PLATFORM_WIN32
SUBDIRS += examples
endif

SUBDIRS += tools .

DISTCLEANFILES = iconv-detect.h gmime-$(GMIME_API_VERSION).pc

EXTRA_DIST = 				\
	PORTING				\
	gmime.pc.in			\
	gmime.spec.in 			\
	iconv-detect.c			\
	zentimer.h 			\
	gtk-doc.make

BUILD_EXTRA_DIST = 			\
	gmime.spec

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gmime-$(GMIME_API_VERSION).pc

$(pkgconfig_DATA): config.status

dist-hook: $(BUILD_EXTRA_DIST)
	files='$(BUILD_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  rm -f $(distdir)/$$f && cp $$d/$$f $(distdir) || exit 1; done

gmime-$(GMIME_API_VERSION).pc: gmime.pc
	-cp gmime.pc gmime-$(GMIME_API_VERSION).pc

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
