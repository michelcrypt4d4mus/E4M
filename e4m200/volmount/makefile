#
# THIS MAKEFILE WORKS WITH MICROSOFT NMAKE ONLY
#

BASENAME = volmount
	
RESOURCES = $(BASENAME).res

all: $(BASENAME).exe

OBJECTS = $(BASENAME).obj cmdline.obj  dlgcode.obj combo.obj endian.obj \
	crypto.obj sdvol.obj password.obj pkcs5.obj volumes1.obj \
	random.obj crc.obj

CFLAGS = $(CFLAGS_G) /DVOLMOUNT /YXe4mdefs.h

!include ..\common.ms

$(BASENAME).res : $(BASENAME).rc ../common/dlgs.rc

$(BASENAME).exe : $(OBJECTS) $(RESOURCES)  crypto.lib 
	$(LINK_G) $(OBJECTS) $(RESOURCES) crypto.lib  /subsystem:windows

