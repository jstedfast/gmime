API = gmime-api.xml
RAW_API = gmime-api.raw
METADATA = GMime.metadata
ASSEMBLY_NAME = gmime-sharp
references = $(GLIB_SHARP_LIBS)
SOURCES_XML = sources.xml

PACKAGE_SHARP = $(PACKAGE)-sharp-$(GMIME_API_VERSION)
TARGET = $(ASSEMBLY)
ASSEMBLY = $(ASSEMBLY_NAME).dll
noinst_DATA = $(API) $(ASSEMBLY) $(ASSEMBLY).config
CLEANFILES = $(ASSEMBLY) generated-stamp generated/*.cs $(API)
DISTCLEANFILES = $(ASSEMBLY).config AssemblyInfo.cs gmime-sharp-$(GMIME_API_VERSION).pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(PACKAGE_SHARP).pc

sources = 					\
	StreamWrapper.cs			\
	HeaderEnumerator.cs			\
	Header.cs

build_sources =  				\
	$(srcdir)/StreamWrapper.cs		\
	$(srcdir)/HeaderEnumerator.cs		\
	$(srcdir)/Header.cs			\
	AssemblyInfo.cs

customs = 					\
	CertificateCollection.custom		\
	DataWrapper.custom			\
	Global.custom				\
	HeaderList.custom			\
	InternetAddress.custom			\
	InternetAddressList.custom		\
	Message.custom				\
	Multipart.custom			\
	Object.custom				\
	SignatureCollection.custom		\
	Stream.custom

build_customs = 				\
	$(srcdir)/CertificateCollection.custom	\
	$(srcdir)/DataWrapper.custom		\
	$(srcdir)/Global.custom			\
	$(srcdir)/HeaderList.custom		\
	$(srcdir)/InternetAddress.custom	\
	$(srcdir)/InternetAddressList.custom	\
	$(srcdir)/Message.custom		\
	$(srcdir)/Multipart.custom		\
	$(srcdir)/Object.custom			\
	$(srcdir)/SignatureCollection.custom	\
	$(srcdir)/Stream.custom

EXTRA_DIST =					\
	$(ASSEMBLY).config.in			\
	$(ASSEMBLY_NAME).snk			\
	AssemblyInfo.cs.in			\
	$(METADATA)				\
	$(RAW_API)				\
	$(customs)				\
	$(sources)				\
	$(SOURCES_XML)				\
	gmime-sharp.pc.in

$(PACKAGE_SHARP).pc: gmime-sharp.pc
	-cp gmime-sharp.pc $(PACKAGE_SHARP).pc

$(API): $(srcdir)/$(RAW_API) $(srcdir)/$(METADATA)
	cp $(srcdir)/$(RAW_API) $(API)
	chmod u+w $(API)
	$(GAPI_FIXUP) --api=$(API) --metadata=$(srcdir)/$(METADATA)

generated-stamp: $(API) $(build_customs)
	rm -f generated/* &&	\
	$(GAPI_CODEGEN) --generate $(API) --outdir=generated --customdir=$(srcdir) --assembly-name=$(ASSEMBLY_NAME) \
	&& touch generated-stamp

$(ASSEMBLY): $(build_sources) generated-stamp $(srcdir)/$(ASSEMBLY_NAME).snk
	$(CSC) -unsafe /out:$(ASSEMBLY) /target:library $(references) \
	-keyfile:$(srcdir)/$(ASSEMBLY_NAME).snk $(build_sources) generated/*.cs
	sn -q -R $(ASSEMBLY) $(srcdir)/$(ASSEMBLY_NAME).snk

install-data-hook:
	@if test -n '$(TARGET)'; then									\
	  if test -n '$(DESTDIR)'; then									\
	    echo "$(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /root $(DESTDIR)$(libdir)";	\
	    $(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /root $(DESTDIR)$(libdir) || exit 1;	\
	  else												\
	    echo "$(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /gacdir $(libdir)";		\
	    $(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /gacdir $(libdir) || exit 1;		\
	  fi;												\
	fi

uninstall-local:
	@if test -n '$(TARGET)'; then									\
	  if test -n '$(DESTDIR)'; then									\
	    echo "$(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /root $(DESTDIR)$(libdir)";	\
	    $(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /root $(DESTDIR)$(libdir) || exit 1;  \
	  else												\
	    echo "$(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /gacdir $(libdir)";		\
	    $(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /gacdir $(libdir) || exit 1;	\
	  fi;												\
	fi

parse:
	$(GAPI_PARSER) $(SOURCES_XML)
