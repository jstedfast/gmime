## Process this file with automake to produce Makefile.in

SUBDIRS = .

INCLUDES = -I$(top_srcdir) $(GLIB_CFLAGS)

bin_PROGRAMS = uuencode uudecode

DEPS = 						\
	$(top_builddir)/util/libutil.la		\
	$(top_builddir)/gmime/libgmime-2.0.la

LDADDS = 					\
	$(top_builddir)/util/libutil.la		\
	$(top_builddir)/gmime/libgmime-2.0.la 	\
	$(GLIB_LIBS)

uuencode_SOURCES = getopt.c getopt.h getopt1.c uuencode.c
uuencode_LDFLAGS = 
uuencode_DEPENDENCIES = $(DEPS)
uuencode_LDADD = $(LDADDS)

uudecode_SOURCES = getopt.c getopt.h getopt1.c uudecode.c
uudecode_LDFLAGS = 
uudecode_DEPENDENCIES = $(DEPS)
uudecode_LDADD = $(LDADDS)
