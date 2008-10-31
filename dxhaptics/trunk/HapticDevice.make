SHELL = /bin/sh
BASE = /usr/lib/dx

# need arch set, e.g. by
# setenv DXARCH `dx -whicharch`
include $(BASE)/lib_$(DXARCH)/arch.mak

FILES_HapticDevice = HapticDevice.$(OBJEXT)

BIN = $(BASE)/bin

#windows BIN = $(BASE)\bin

CFLAGS = -I./ -I/usr/include -I$(BASE)/include $(DX_CFLAGS)

LDFLAGS = -L$(BASE)/lib_$(DXARCH)

LIBS = -lHDU -lHD -lrt -lDX $(DX_GL_LINK_LIBS) $(DXEXECLINKLIBS)

OLIBS = -lDXlite -lm -lHDU -lHD -lrt

BIN = $(BASE)/bin

# create the necessary executable
HapticDevice: $(FILES_HapticDevice) outboard.$(OBJEXT)
	$(CC) $(DXABI) $(LDFLAGS) $(FILES_HapticDevice) outboard.$(OBJEXT) $(OLIBS) -o HapticDevice$(DOT_EXE_EXT)

# how to make the outboard main routine
outboard.$(OBJEXT): $(BASE)/lib/outboard.c
	$(CC) $(DXABI) $(CFLAGS) -DUSERMODULE=m_HapticDevice -c $(BASE)/lib/outboard.c


.c.o: ; cc -c $(DXABI) $(DX_RTL_CFLAGS) $(CFLAGS) $*.c 

.C.o: ; cc -c $(DXABI) $(DX_RTL_CFLAGS) $(CFLAGS) $*.C 

# a command to run the user module
run: HapticDevice 
	dx -edit -mdf HapticDevice.mdf &

# make the user files
userHapticDevice.c: HapticDevice.mdf
	$(BIN)/mdf2c HapticDevice.mdf > userHapticDevice.c
# kluge for when DXARCH isn't set
$(BASE)/lib_/arch.mak:
	(export DXARCH=`dx -whicharch` ; $(MAKE) -f HapticDevice.make )
	echo YOU NEED TO SET DXARCH via dx -whicharch
