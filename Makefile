_DIR   := .build
FF_VER := shared
_EXE   := untrunc
IS_RELEASE := 0
LIBUI_STATIC := 0

# make switching between ffmpeg versions easy
TARGET := $(firstword $(MAKECMDGOALS))
ifeq ($(TARGET), $(_EXE)-33)
	FF_VER := 3.3.9
	EXE := $(TARGET)
else ifeq ($(TARGET), $(_EXE)-34)
	FF_VER := 3.4.5
	EXE := $(TARGET)
else ifeq ($(TARGET), $(_EXE)-341)
	FF_VER := 3.4.1
	EXE := $(TARGET)
else ifeq ($(TARGET), $(_EXE)-41)
	FF_VER := 4.1
	EXE := $(TARGET)
endif

ifeq ($(OS),Windows_NT)
	_OS := Windows
else
	_OS := $(shell uname)
endif

FFDIR := ffmpeg-$(FF_VER)

ifeq ($(FF_VER), shared)
	LDFLAGS += -lavformat -lavcodec -lavutil
else
	CXXFLAGS += -I./$(FFDIR)
	LDFLAGS += -L$(FFDIR)/libavformat -lavformat
	LDFLAGS += -L$(FFDIR)/libavcodec -lavcodec
	LDFLAGS += -L$(FFDIR)/libavutil -lavutil
	#LDFLAGS += -L$(FFDIR)/libswscale/ -lswresample
	#LDFLAGS += -L$(FFDIR)/libavresample -lavresample
	#LDFLAGS += -lz -lbz2 -lX11 -lva -lva-drm -lva-x11 -llzma
	LDFLAGS += -lpthread -ldl

	ifeq ($(_OS), Darwin)
		LDFLAGS += -liconv
	endif
endif

CXXFLAGS += -std=c++11 -D_FILE_OFFSET_BITS=64

ifeq ($(IS_RELEASE), 1)
	CXXFLAGS += -O3
	LDFLAGS += -s
else
	CXXFLAGS += -g
endif

VER = $(shell test -d .git && command -v git >/dev/null && echo "v`git rev-list --count HEAD`-`git describe --always --dirty --abbrev=7`")
CPPFLAGS += -MMD -MP
CPPFLAGS += -DUNTR_VERSION=\"$(VER)\"

EXE ?= $(_EXE)
DIR := $(_DIR)_$(FF_VER)
SRC := $(wildcard src/*.cpp src/avc1/*.cpp src/hvc1/*.cpp)
OBJ := $(SRC:%.cpp=$(DIR)/%.o)
DEP := $(OBJ:.o=.d)

SRC_GUI := $(wildcard src/gui/*.cpp)
OBJ_GUI := $(SRC_GUI:%.cpp=$(DIR)/%.o)
DEP_GUI := $(OBJ_GUI:.o=.d)

ifeq ($(TARGET), $(_EXE)-gui)
	LDFLAGS += -lui -lpthread

	machine = $(shell $(CXX) -dumpmachine)
	ifneq (,$(findstring mingw,$(machine)))
		LDFLAGS += -Wl,--subsystem,windows
		ifeq ($(LIBUI_STATIC), 1)
			OBJ_GUI += $(DIR)/src/gui/win_resources.o
			LDFLAGS += -lpthread -luser32 -lkernel32 -lusp10 -lgdi32 -lcomctl32 -luxtheme -lmsimg32 -lcomdlg32 -ld2d1 -ldwrite -lole32 -loleaut32 -loleacc
		endif
	endif
endif

NPROC = $(shell command -v nproc >/dev/null && nproc || echo 1)
NJOBS = $(shell echo $$(( $(NPROC) / 3)) )
ifeq ($(NJOBS), 0)
	NJOBS = 1
endif

#$(info $$OBJ is [${OBJ}])
#$(info $$OBJ_GUI is [${OBJ_GUI}])
$(shell mkdir -p $(dir $(OBJ_GUI)) 2>/dev/null)
$(shell mkdir -p $(DIR)/src/avc1 $(DIR)/src/hvc1 2>/dev/null)

CURL := $(shell command -v curl 2>/dev/null)

.PHONY: all clean force


all: $(EXE)

$(FFDIR)/configure:
	@#read -p "Press [ENTER] if you agree to build ffmpeg-${FF_VER} now.. " input
	@echo "(info) downloading $(FFDIR) ..."
ifdef CURL
	curl -o /tmp/$(FFDIR).tar.xz https://www.ffmpeg.org/releases/$(FFDIR).tar.xz
else
	wget -q --show-progress -O /tmp/$(FFDIR).tar.xz https://www.ffmpeg.org/releases/$(FFDIR).tar.xz
endif
	tar xf /tmp/$(FFDIR).tar.xz
	mv $(FFDIR)/VERSION $(FFDIR)/VERSION.bak

$(FFDIR)/config.asm: | $(FFDIR)/configure
	@echo "(info) please wait ..."
	cd $(FFDIR); ./configure --disable-doc --disable-programs \
	--disable-everything --enable-decoders --disable-vdpau --enable-demuxers --enable-protocol=file \
	--disable-avdevice --disable-swresample --disable-swscale --disable-avfilter --disable-postproc \
	--disable-xlib --disable-vaapi --disable-zlib --disable-bzlib --disable-lzma \
	--disable-vda --disable-audiotoolbox --disable-videotoolbox

$(FFDIR)/libavcodec/libavcodec.a: | $(FFDIR)/config.asm
	cat $(FFDIR)/Makefile
	$(MAKE) -C $(FFDIR) -j$(NJOBS)

$(FFDIR):
ifneq ($(FF_VER), shared)
$(FFDIR): | $(FFDIR)/libavcodec/libavcodec.a
endif

print_info: | $(FFDIR)
	@echo untrunc: $(VER)
	@echo ffmpeg: $(FF_VER)
	@echo

$(EXE): print_info $(OBJ)
	$(CXX) $(filter-out $<,$^) $(LDFLAGS) -o $@

$(EXE)-gui: print_info $(filter-out $(DIR)/src/main.o, $(OBJ)) $(OBJ_GUI)
	$(CXX) $(filter-out $<,$^) $(LDFLAGS) -o $@

$(DIR)/%/win_resources.o: %/win_resources.rc
	windres.EXE $< $@

# rebuild src/common.o on new version/CPPFLAGS
$(DIR)/cpp_flags: force
	@echo '$(CPPFLAGS)' | cmp -s - $@ || echo '$(CPPFLAGS)' > $@
$(DIR)/src/common.o: $(DIR)/cpp_flags

$(DIR)/%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<

-include $(DEP)
-include $(DEP_GUI)

clean:
	$(RM) -r $(DIR)
	$(RM) $(EXE)
	$(RM) $(EXE)-gui

