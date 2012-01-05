## Process this file with automake to produce Makefile.in

SUBDIRS = .

INCLUDES = -I$(top_srcdir) $(GMIME_CFLAGS) $(GLIB_CFLAGS)

bin_PROGRAMS = gmime-uuencode gmime-uudecode

DEPS = 						\
	$(top_builddir)/util/libutil.la		\
	$(top_builddir)/gmime/libgmime-2.4.la

LDADDS = 					\
	$(top_builddir)/util/libutil.la		\
	$(top_builddir)/gmime/libgmime-2.4.la 	\
	$(GLIB_LIBS)

if SYSTEM_GETOPT
GETOPT_SOURCES =
else
GETOPT_SOURCES = getopt.c getopt.h getopt1.c
endif

gmime_uuencode_SOURCES = $(GETOPT_SOURCES) uuencode.c
gmime_uuencode_LDFLAGS = 
gmime_uuencode_DEPENDENCIES = $(DEPS)
gmime_uuencode_LDADD = $(LDADDS)

gmime_uudecode_SOURCES = $(GETOPT_SOURCES) uudecode.c
gmime_uudecode_LDFLAGS = 
gmime_uudecode_DEPENDENCIES = $(DEPS)
gmime_uudecode_LDADD = $(LDADDS)
