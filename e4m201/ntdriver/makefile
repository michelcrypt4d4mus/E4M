#
# THIS MAKEFILE WORKS WITH MICROSOFT NMAKE ONLY
#

# change this to point to your DDK
DDK = d:\ddk

BASENAME = e4mnt4

all: $(BASENAME).sys

OBJECTS = ntdriver.obj ntvol.obj ntrawdv.obj ntfiledv.obj \
	volumes1.obj crypto.obj endian.obj crc.obj pkcs5.obj cache.obj

#add this for asm listings:- /FAcs 

!IFNDEF RELEASE

# Original debug build options from build.exe was
# DFPO=0
DEFINES_D = -D_X86_=1 -Di386=1 -DSTD_CALL -DCONDITION_HANDLING=1 \
-DNT_UP=1 -DNT_INST=0 -DWIN32=100 -D_NT1X_=100 \
-DWINNT=1 -D_WIN32_WINNT=0x0400 -DWIN32_LEAN_AND_MEAN=1 -DDBG=1 -DDEBUG -DDEVL=1 \
-DFPO=0 -D_DLL=1 -D_IDWBUILD -DRDRDBG -DSRVDBG -DDBG_MESSAGES=1 \
-D_UNICODE -DLITTLE_ENDIAN

CFLAGS_D = $(DEFINES_D) /Zel /Zp8 /Gy -cbstring /Gz /QIfdiv- /QIf /Gi- /Gm- /GX- \
/GR- /GF -FI$(DDK)\inc\warning.h -Z7 /Od /Oi /Oy- /W3 /WX

LFLAGS_D = -MERGE:_PAGE=PAGE -MERGE:_TEXT=.text -SECTION:INIT,d \
-INCREMENTAL:NO -FORCE:MULTIPLE -RELEASE -FULLBUILD \
-IGNORE:4001,4037,4039,4065,4070,4078,4087,4089,4096 -NODEFAULTLIB \
-debug:notmapped,FULL -debugtype:both -version:4.00 -osversion:4.00 \
-PDB:NONE -MERGE:.rdata=.text -MAP:$(BASENAME).map -optidata -driver -align:0x20 \
-subsystem:native,4.00 -base:0x10000 -entry:DriverEntry@8

!ELSE

DEFINES_D = -D_X86_=1 -Di386=1 -DSTD_CALL -DCONDITION_HANDLING=1 \
-DNT_UP=1 -DNO_DISK_ACCESS -DNT_INST=0 -DWIN32=100 -D_NT1X_=100 \
-DWINNT=1 -D_WIN32_WINNT=0x0400 -DWIN32_LEAN_AND_MEAN=1 -DDEVL=1 \
-DFPO=1 -DNDEBUG -D_DLL=1 -D_IDWBUILD -D_UNICODE \
-DLITTLE_ENDIAN

# /W warning level /Gz calling convention is __stdcall /QIfdiv- disable pentium bug fix
# /QIf emit FPO records for every function /Gi- disable incremental compile
# /Gm- disable min. rebuild /GX- disable exception handling /GR- disable RTTI
# /GF eliminate duplicate strings /Oxs /Oy Frame-Pointer omission

CFLAGS_D = $(DEFINES_D) /Zel /Zp8 /Gy -cbstring /Gz /QIfdiv- /QIf /Gi- /Gm- /GX- \
/GR- /GF /Oxs /Oy -FI$(DDK)\inc\warning.h /W3 /WX /FAcs 

LFLAGS_D = -MERGE:_PAGE=PAGE -MERGE:_TEXT=.text -SECTION:INIT,d \
-INCREMENTAL:NO -FORCE:MULTIPLE -RELEASE -FULLBUILD \
-IGNORE:4001,4037,4039,4065,4070,4078,4087,4089,4096 -NODEFAULTLIB -version:4.00 \
-osversion:4.00 -PDB:NONE -MERGE:.rdata=.text -MAP:$(BASENAME).map -optidata -driver \
-align:0x20 -subsystem:native,4.00 -base:0x10000 -entry:DriverEntry@8

!ENDIF

LINK_D = link $(LFLAGS_D) /nologo /out:$(BASENAME).sys -machine:IX86 \
   $(DDK)\lib\i386\free\ntoskrnl.lib \
   $(DDK)\lib\i386\free\hal.lib

CFLAGS = $(CFLAGS_D) /DNT4_DRIVER /I$(DDK)\inc /YXe4mdefs.h

!include ..\common.ms

$(BASENAME).sys : $(OBJECTS) crypto.lib
	$(LINK_D) $(OBJECTS) crypto.lib

install:
	copy $(BASENAME).sys c:\nt4\system32\drivers\$(BASENAME).sys

uninstall:
	del c:\nt4\system32\drivers\$(BASENAME).sys

verify:
	fc /b c:\nt4\system32\drivers\$(BASENAME).sys $(BASENAME).sys

