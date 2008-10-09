SHELL = /bin/sh
BASE = /usr/dx

# need arch set, e.g. by
# setenv DXARCH `dx -whicharch`
include $(BASE)/lib_$(DXARCH)/arch.mak

FILES_HapticDevice = userHapticDevice.$(OBJEXT) HapticDevice.$(OBJEXT)

BIN = $(BASE)/bin

#windows BIN = $(BASE)\bin

CFLAGS = -I./ -I$(BASE)/include $(DX_CFLAGS)

LDFLAGS = -L$(BASE)/lib_$(DXARCH)

LIBS = -lDX $(DX_GL_LINK_LIBS) $(DXEXECLINKLIBS)

OLIBS = -lDXlite -lm

BIN = $(BASE)/bin

# create the necessary executable
HapticDevice: $(FILES_HapticDevice) 
	$(SHARED_LINK) $(DXABI) $(DX_RTL_CFLAGS) $(LDFLAGS) -o HapticDevice userHapticDevice.$(OBJEXT) HapticDevice.$(OBJEXT) $(DX_RTL_LDFLAGS) $(SYSLIBS) $(DX_RTL_IMPORTS) $(DX_RTL_DXENTRY)

.c.o: ; cc -c $(DXABI) $(DX_RTL_CFLAGS) $(CFLAGS) $*.c 

.C.o: ; cc -c $(DXABI) $(DX_RTL_CFLAGS) $(CFLAGS) $*.C 

# a command to run the user module
run: HapticDevice 
	dx -edit -mdf HapticDevice.mdf &

# make the user files
userHapticDevice.c: HapticDevice.mdf
	$(BIN)/mdf2c -m HapticDevice.mdf > userHapticDevice.c
# kluge for when DXARCH isn't set
$(BASE)/lib_/arch.mak:
	(export DXARCH=`dx -whicharch` ; $(MAKE) -f HapticDevice.make )
	echo YOU NEED TO SET DXARCH via dx -whicharch
