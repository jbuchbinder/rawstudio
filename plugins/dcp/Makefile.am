plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS = -O4 -Wall -ffast-math -ffinite-math-only \
	-funsafe-math-optimizations -funroll-loops

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" -I../../librawstudio/ @PACKAGE_CFLAGS@

lib_LTLIBRARIES = dcp.la

libdir = $(datadir)/rawstudio/plugins/

dcp_la_LIBADD = @PACKAGE_LIBS@ dcp-sse2.lo dcp-sse4.lo
dcp_la_LDFLAGS = -module -avoid-version

dcp.lo: dcp.c
	$(LTCOMPILE) -DEXIT_CODE=0 -c dcp.c

if CAN_COMPILE_SSE4_1
SSE4_FLAG=-msse4.1
else
SSE4_FLAG=
endif

if CAN_COMPILE_SSE2
SSE2_FLAG=-msse2
else
SSE2_FLAG=
endif

dcp-sse2.lo: dcp-sse2.c
	$(LTCOMPILE) $(SSE2_FLAG) -DEXIT_CODE=1 -c dcp-sse2.c

dcp-sse4.lo: dcp-sse4.c
	$(LTCOMPILE) $(SSE4_FLAG) -DEXIT_CODE=1 -c dcp-sse4.c
