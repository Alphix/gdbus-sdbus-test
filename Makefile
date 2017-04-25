#
# Copyright (C) 2017 David Härdeman <david@hardeman.nu>
#

#
# Generic settings
#
CC              = gcc
GENERIC_CFLAGS	= -g -Wall -Werror -D_FILE_OFFSET_BITS=64 \
		  -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1 -flto
EXTRA_CFLAGS	=
GENERIC_LDFLAGS	=
EXTRA_LDFLAGS	=

SDBUS_PACKAGES	= libsystemd
SDBUS_CFLAGS	= ${GENERIC_CFLAGS} ${EXTRA_CFLAGS} $(shell pkg-config --cflags ${SDBUS_PACKAGES})
SDBUS_LDFLAGS	= ${GENERIC_LDFLAGS} ${EXTRA_LDFLAGS} $(shell pkg-config --libs ${SDBUS_PACKAGES})
SDBUS_COMPILE	= $(CC) $(SDBUS_CFLAGS)
SDBUS_LINK	= $(CC) $(SDBUS_CFLAGS) $(SDBUS_LDFLAGS)
SDBUS_C_OBJECTS	= sdbus-client.o
SDBUS_S_OBJECTS	= sdbus-server.o

GDBUS_PACKAGES	= gtk+-3.0 gmodule-2.0
GDBUS_CFLAGS	= ${GENERIC_CFLAGS} ${EXTRA_CFLAGS} $(shell pkg-config --cflags ${GDBUS_PACKAGES})
GDBUS_LDFLAGS	= ${GENERIC_LDFLAGS} ${EXTRA_LDFLAGS} $(shell pkg-config --libs ${GDBUS_PACKAGES})
GDBUS_COMPILE	= $(CC) $(GDBUS_CFLAGS)
GDBUS_LINK	= $(CC) $(GDBUS_CFLAGS) $(GDBUS_LDFLAGS)
GDBUS_C_OBJECTS	= gdbus-client.o generated.o
GDBUS_S_OBJECTS	= gdbus-server.o generated.o

.SUFFIXES:

#
# Targets
#

all: gdbus-client gdbus-server sdbus-client sdbus-server
.DEFAULT: all

generated.c generated.h: TestCase.xml
	gdbus-codegen --interface-prefix org.gnome --c-generate-object-manager --generate-c-code generated $<

generated.o: generated.c generated.h
	$(GDBUS_COMPILE) -o $@ -c $<

gdbus-%.o: gdbus-%.c generated.o
	$(GDBUS_COMPILE) -o $@ -c $<

gdbus-client: $(GDBUS_C_OBJECTS)
	$(GDBUS_LINK) -o $@ $^

gdbus-server: $(GDBUS_S_OBJECTS)
	$(GDBUS_LINK) -o $@ $^

sdbus-%.o: sdbus-%.c sdbus.h
	$(SDBUS_COMPILE) -o $@ -c $<

sdbus-client: $(SDBUS_C_OBJECTS)
	$(SDBUS_LINK) -o $@ $^

sdbus-server: $(SDBUS_S_OBJECTS)
	$(SDBUS_LINK) -o $@ $^

clean:
	- rm -f generated.[ch] *.o gdbus-client gdbus-server sdbus-client sdbus-server

.PHONY: clean all

