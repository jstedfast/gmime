## Process this file with automake to produce Makefile.in

SUBDIRS = . tests doc

INCLUDES = -I@srcdir@ $(VERSION_FLAGS) $(GLIB_CFLAGS) -DG_LOG_DOMAIN=\"gmime\"

VERSION_FLAGS = -DGMIME_VERSION=\"$(GMIME_VERSION)\"

noinst_PROGRAMS = 	\
	gen-table 	\
	test-best	\
	test-mime 	\
	test-parser 	\
	test-mbox 	\
	test-streams 	\
	test-html 	\
	test-iconv 	\
	test-partial	\
	test-pgp	\
	test-pgpmime

bin_SCRIPTS = gmime-config

lib_LTLIBRARIES = libgmime-2.0.la

libgmime_2_0_la_LDFLAGS = -version-info $(GMIME_VERSION_INFO)

libgmime_2_0_la_SOURCES = 		\
	memchunk.c			\
	gmime.c				\
	gmime-charset.c			\
	gmime-cipher-context.c		\
	gmime-content-type.c		\
	gmime-data-wrapper.c		\
	gmime-disposition.c		\
	gmime-exception.c		\
	gmime-filter.c			\
	gmime-filter-basic.c		\
	gmime-filter-best.c		\
	gmime-filter-charset.c		\
	gmime-filter-crlf.c		\
	gmime-filter-from.c		\
	gmime-filter-html.c		\
	gmime-filter-strip.c		\
	gmime-filter-yenc.c		\
	gmime-gpg-context.c		\
	gmime-header.c			\
	gmime-iconv.c			\
	gmime-iconv-utils.c		\
	gmime-message.c			\
	gmime-message-part.c		\
	gmime-message-partial.c		\
	gmime-multipart.c		\
	gmime-multipart-encrypted.c	\
	gmime-multipart-signed.c	\
	gmime-object.c			\
	gmime-param.c			\
	gmime-parser.c			\
	gmime-part.c			\
	gmime-session.c			\
	gmime-stream.c			\
	gmime-stream-buffer.c		\
	gmime-stream-cat.c		\
	gmime-stream-file.c		\
	gmime-stream-filter.c		\
	gmime-stream-fs.c		\
	gmime-stream-mem.c		\
	gmime-stream-mmap.c		\
	gmime-stream-null.c		\
	gmime-utils.c			\
	internet-address.c		\
	md5-utils.c

gmimeincludedir = $(includedir)/gmime-2.0/gmime
gmimeinclude_HEADERS = 			\
	gmime.h				\
	gmime-charset.h			\
	gmime-cipher-context.h		\
	gmime-content-type.h		\
	gmime-data-wrapper.h		\
	gmime-disposition.h		\
	gmime-exception.h		\
	gmime-exception-list.def	\
	gmime-filter.h			\
	gmime-filter-basic.h		\
	gmime-filter-best.h		\
	gmime-filter-charset.h		\
	gmime-filter-crlf.h		\
	gmime-filter-from.h		\
	gmime-filter-html.h		\
	gmime-filter-strip.h		\
	gmime-filter-yenc.h		\
	gmime-gpg-context.h		\
	gmime-header.h			\
	gmime-iconv.h			\
	gmime-iconv-utils.h		\
	gmime-message.h			\
	gmime-message-part.h		\
	gmime-message-partial.h		\
	gmime-multipart.h		\
	gmime-multipart-encrypted.h	\
	gmime-multipart-signed.h	\
	gmime-object.h			\
	gmime-param.h			\
	gmime-parser.h			\
	gmime-part.h			\
	gmime-session.h			\
	gmime-stream.h			\
	gmime-stream-buffer.h		\
	gmime-stream-cat.h		\
	gmime-stream-file.h		\
	gmime-stream-filter.h		\
	gmime-stream-fs.h		\
	gmime-stream-mem.h		\
	gmime-stream-mmap.h		\
	gmime-stream-null.h		\
	gmime-type-utils.h		\
	gmime-utils.h			\
	internet-address.h

noinst_HEADERS = 			\
	memchunk.h			\
	gmime-table-private.h		\
	gmime-charset-map-private.h	\
	md5-utils.h

DEPS = $(top_builddir)/libgmime-2.0.la
LDADDS = $(top_builddir)/libgmime-2.0.la $(GLIB_LIBS)

gen_table_SOURCES = gen-table.c
gen_table_LDFLAGS = 
gen_table_DEPENDENCIES = 
gen_table_LDADD = 

check-local: tests

testall : tests

tests: MIMEtests PARSERtests STREAMtests FILTERtests ICONVtests PGPtests

MIMEtests : test-mime
	@echo "##"
	@echo "## MIME regression tests"
	@echo "##"

PARSERtests : test-parser test-mbox test-partial
	@echo "##"
	@echo "## MIME Parser regression tests"
	@echo "##"

STREAMtests : test-streams
	@echo "##"
	@echo "## Stream regression tests"
	@echo "##"

FILTERtests : test-html
	@echo "##"
	@echo "## Filter regression tests"
	@echo "##"

ICONVtests : test-iconv
	@echo "##"
	@echo "## iconv cache regression tests"
	@echo "##"

PGPtests : test-pgp test-pgpmime
	@echo "##"
	@echo "## PGP tests"
	@echo "##"

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

$(srcdir)/libgmime:
	-$(RM) $(srcdir)/libgmime
	ln -s $(srcdir)/. $(srcdir)/libgmime

$(libgmime_2_0_la_SOURCES): $(srcdir)/libgmime
