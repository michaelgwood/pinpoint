EXTRA_DIST =

bin_PROGRAMS = pinpoint-ws

pinpoint_ws_SOURCES = \
		     pinpoint-ws.c \
		     http-server.c http-server.h \
		     dbus-client.c dbus-client.h \
		     $(NULL)

pinpoint_ws_CFLAGS = \
		    $(DEPS_CFLAGS) \
		    -DDATADIR=\"$(pkgdatadir)/webservice\"
		    $(NULL)

pinpoint_ws_LDADD = \
		   $(DEPS_LIBS) \
		   $(NULL)

pinpoint_wsdir = $(pkgdatadir)/webservice
dist_pinpoint_ws_DATA = \
			index.html \
			$(NULL)
