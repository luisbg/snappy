NULL =

public_headers = \
		utils.h \
		user_interface.h \
		gst_engine.h

c_sources = \
	utils.c \
	user_interface.c \
	gst_engine.c \
	dogme.c

CLEANFILES =

bin_PROGRAMS = dogme

dogme_SOURCES = $(c_sources)
dogme_CFLAGS = $(CLUTTER_CFLAGS) $(GSTREAMER_0_10_CFLAGS) $(CLUTTER_GST_CFLAGS) -DDOGME_DATA_DIR="\"$(pkgdatadir)\""
dogme_LDADD = $(CLUTTER_LIBS) $(GSTREAMER_0_10_LIBS) $(CLUTTER_GST_LIBS)

noinst_HEADERS = $(public_headers)
