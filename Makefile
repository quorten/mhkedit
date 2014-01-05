X = .exe
O = .o
CC = gcc
CFLAGS = -c -g
LD = gcc
LDFLAGS = -mwindows
LD_LIBRARIES = -lcomctl32
OutDir = obj-dbg

all: $(OutDir) $(OutDir)/mhkedit$(X)

$(OutDir):
	-mkdir $(OutDir)

$(OutDir)/MhkEdit$(O): MhkEdit.c resource.h Panel.h
	$(CC) $(CFLAGS) -o $@ $<

$(OutDir)/Panel$(O): Panel.c Panel.h resource.h
	$(CC) $(CFLAGS) -o $@ $<

# $(OutDir)/HexEdit$(O): HexEdit.c HexEdit.h resource.h
# 	$(CC) $(CFLAGS) -o $@ $<

$(OutDir)/MhkEdit-rc$(O): MhkEdit.rc MhkEdit.ico about.dlg \
	rsrc_general.dlg rsrc_bitmap.dlg rsrc_sprite.dlg game_mode.dlg
	windres -Ocoff -o $@ $<

$(OutDir)/mhkedit$(X): $(OutDir)/MhkEdit$(O) $(OutDir)/Panel$(O) \
	$(OutDir)/MhkEdit-rc$(O)
	$(LD) $(LDFLAGS) -o $@ $^ $(LD_LIBRARIES)

clean:
#	rm -f -R $(OutDir)
	-echo y | del $(OutDir)
	-rd $(OutDir)
