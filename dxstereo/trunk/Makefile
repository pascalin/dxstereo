SHELL = /bin/sh
BASE = /usr/lib/dx
DXARCH = linux

include $(BASE)/lib_$(DXARCH)/arch.mak

FILES_dxstereo = dxstereo.$(OBJEXT)
FILES_dxexec = userdxstereo.$(OBJEXT) dxstereo.$(OBJEXT)

BIN = $(BASE)/bin

#windows BIN = $(BASE)\bin

CFLAGS = -I./ -I$(BASE)/include $(DX_CFLAGS)

LDFLAGS = -L$(BASE)/lib_$(DXARCH)

LIBS =  -lglut -lDX $(DX_GL_LINK_LIBS) $(DXEXECLINKLIBS)

OLIBS = -lDXlite -lm -lglut

BIN = $(BASE)/bin

# create the necessary executable
dxexec: $(FILES_dxexec) 
	$(CC) $(LDFLAGS) $(FILES_dxexec) $(LIBS) -o dxexec

# create the necessary executable (outboard)
dxstereo: $(FILES_dxstereo) outboard.$(OBJEXT)
	$(CC) $(DXABI) $(LDFLAGS) $(FILES_dxstereo) outboard.$(OBJEXT) $(OLIBS) -o dxstereo$(DOT_EXE_EXT)

# how to make the outboard main routine
outboard.$(OBJEXT): $(BASE)/lib/outboard.c
	$(CC) $(DXABI) $(CFLAGS) -DUSERMODULE=m_DXStereo -c $(BASE)/lib/outboard.c

.c.o: ; cc -c $(DXABI) $(DX_RTL_CFLAGS) $(CFLAGS) $*.c 

.C.o: ; cc -c $(DXABI) $(DX_RTL_CFLAGS) $(CFLAGS) $*.C 

# make the user files
userdxstereo.c: dxstereo.mdf
	$(BIN)/mdf2c dxstereo.mdf > userdxstereo.c
# kluge for when DXARCH isn't set
$(BASE)/lib_/arch.mak:
	(export DXARCH=`dx -whicharch` ; $(MAKE) )
	echo YOU NEED TO SET DXARCH via dx -whicharch

# a command to run the user module
run: dxexec
	dx -edit -exec ./dxexec -mdf dxstereo.mdf &

run_outboard: dxstereo
	dx -edit -mdf dxstereo_out.mdf

clean:
	rm -f dxexec dxstereo userdxstereo.c *.o
