## Process this file with automake to produce Makefile.in

SUBDIRS = .

gmimeincludedir = $(includedir)/gmime-2.0/gmime

INCLUDES = -I$(top_srcdir) 		\
	$(VERSION_FLAGS) 		\
	-DG_LOG_DOMAIN=\"gmime\"	\
	-DG_DISABLE_DEPRECATED		\
	$(GLIB_CFLAGS)

VERSION_FLAGS = -DGMIME_VERSION=\"$(GMIME_VERSION)\"

noinst_PROGRAMS = gen-table

lib_LTLIBRARIES = libgmime-2.0.la

libgmime_2_0_la_SOURCES = 		\
	gmime.c				\
	gmime-charset.c			\
	gmime-cipher-context.c		\
	gmime-content-type.c		\
	gmime-data-wrapper.c		\
	gmime-disposition.c		\
	gmime-filter.c			\
	gmime-filter-basic.c		\
	gmime-filter-best.c		\
	gmime-filter-charset.c		\
	gmime-filter-crlf.c		\
	gmime-filter-enriched.c		\
	gmime-filter-from.c		\
	gmime-filter-html.c		\
	gmime-filter-md5.c		\
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
	cache.c				\
	gtrie.c				\
	list.c				\
	md5-utils.c			\
	memchunk.c			\
	url-scanner.c

gmimeinclude_HEADERS = 			\
	gmime.h				\
	gmime-charset.h			\
	gmime-cipher-context.h		\
	gmime-content-type.h		\
	gmime-data-wrapper.h		\
	gmime-disposition.h		\
	gmime-error.h			\
	gmime-filter.h			\
	gmime-filter-basic.h		\
	gmime-filter-best.h		\
	gmime-filter-charset.h		\
	gmime-filter-crlf.h		\
	gmime-filter-enriched.h		\
	gmime-filter-from.h		\
	gmime-filter-html.h		\
	gmime-filter-md5.h		\
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
	internet-address.h		\
	md5-utils.h

noinst_HEADERS = 			\
	cache.h				\
	gtrie.h				\
	list.h				\
	memchunk.h			\
	url-scanner.h			\
	gmime-table-private.h		\
	gmime-charset-map-private.h

libgmime_2_0_la_LDFLAGS = -version-info $(GMIME_VERSION_INFO)

DEPS = $(top_builddir)/libgmime-2.0.la
LDADDS = $(top_builddir)/libgmime-2.0.la $(GLIB_LIBS)

gen_table_SOURCES = gen-table.c
gen_table_LDFLAGS = 
gen_table_DEPENDENCIES = 
gen_table_LDADD = 
