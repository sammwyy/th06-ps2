# Touhou 6 (EoSD) PS2 port. Requires the ps2dev toolchain.
# Output: build/th06.elf

PS2DEV ?= /usr/local/ps2dev
PS2SDK ?= $(PS2DEV)/ps2sdk
GSKIT ?= $(PS2DEV)/gsKit
PORTS ?= $(PS2SDK)/ports

EE_PREFIX = $(PS2DEV)/ee/bin/mips64r5900el-ps2-elf-
EE_CXX = $(EE_PREFIX)g++

BUILD = build
OBJDIR = $(BUILD)/obj
TARGET = $(BUILD)/th06.elf
AUDSRV_IRX = $(BUILD)/audsrv.irx

# make iso bundles the elf, SYSTEM.CNF and audsrv.irx into a bootable disc image.
# Drop game data (PBG3 archives, bgm/, ...) into iso_root/ to include it on the disc.
ISO = $(BUILD)/th06.iso
ISO_STAGING = $(BUILD)/iso
ISO_ROOT = iso_root
MKISOFS ?= mkisofs

SRCS = \
	src/AnmManager.cpp \
	src/AsciiManager.cpp \
	src/BombData.cpp \
	src/BulletData.cpp \
	src/BulletManager.cpp \
	src/Chain.cpp \
	src/Controller.cpp \
	src/EclManager.cpp \
	src/EffectManager.cpp \
	src/Ending.cpp \
	src/EnemyEclInstr.cpp \
	src/EnemyManager.cpp \
	src/FileManager.cpp \
	src/FileSystem.cpp \
	src/GameErrorContext.cpp \
	src/GameManager.cpp \
	src/GameWindow.cpp \
	src/Gui.cpp \
	src/ItemManager.cpp \
	src/main.cpp \
	src/MainMenu.cpp \
	src/MidiOutput.cpp \
	src/MusicRoom.cpp \
	src/Player.cpp \
	src/Ps2Pad.cpp \
	src/ReplayManager.cpp \
	src/ResultScreen.cpp \
	src/Rng.cpp \
	src/ScreenEffect.cpp \
	src/SoundPlayer.cpp \
	src/StartScreen.cpp \
	src/Stage.cpp \
	src/Supervisor.cpp \
	src/TextHelper.cpp \
	src/utils.cpp \
	src/MemAlloc.cpp \
	src/ZunMath.cpp \
	src/ZunTimer.cpp \
	src/graphics/GsKitGfx.cpp \
	src/midi/MidiDefault.cpp \
	src/pbg3/FileAbstraction.cpp \
	src/pbg3/IPbg3Parser.cpp \
	src/pbg3/Pbg3Archive.cpp \
	src/pbg3/Pbg3Parser.cpp \
	src/thirdparty/sjis_converter.cpp

OBJS = $(SRCS:%.cpp=$(OBJDIR)/%.o)

INCS = -Isrc \
	-I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include \
	-I$(GSKIT)/include \
	-I$(PORTS)/include -I$(PORTS)/include/SDL2 \
	-I$(PORTS)/include/freetype2

# DEBUG enables the in-game DebugPrint logging (file loads, renderer init, ...)
CXXFLAGS = -D_EE -DDEBUG -G0 -O2 -std=c++20 -Wall -gdwarf-2 -gz -MMD -MP $(INCS)

LDFLAGS = -T$(PS2SDK)/ee/startup/linkfile \
	-L$(PS2SDK)/ee/lib -L$(GSKIT)/lib -L$(PORTS)/lib \
	-Wl,-zmax-page-size=128

LIBS = -lSDL2main -lSDL2_image -lSDL2_ttf -lSDL2 \
	-lfreetype -ljpeg -lpng -lz \
	-lgskit_toolkit -lgskit -ldmakit \
	-lpatches -lps2_drivers -laudsrv -lpadx -lmc -lm

all: $(TARGET) $(AUDSRV_IRX)

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(EE_CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD)
	$(EE_CXX) $(LDFLAGS) -O2 -o $@ $(OBJS) $(LIBS)

$(AUDSRV_IRX):
	@mkdir -p $(BUILD)
	cp $(PS2SDK)/iop/irx/audsrv.irx $@

iso: $(ISO)

$(ISO): $(TARGET) $(AUDSRV_IRX)
	@rm -rf $(ISO_STAGING)
	@mkdir -p $(ISO_STAGING)
	cp $(TARGET) $(ISO_STAGING)/TH06.ELF
	cp $(AUDSRV_IRX) $(ISO_STAGING)/AUDSRV.IRX
	printf 'BOOT2 = cdrom0:\\TH06.ELF;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n' > $(ISO_STAGING)/SYSTEM.CNF
	@if [ -d $(ISO_ROOT) ]; then cp -r $(ISO_ROOT)/. $(ISO_STAGING)/; fi
	$(MKISOFS) -quiet -l -o $@ $(ISO_STAGING)
	@echo "ISO written to $@"

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

.PHONY: all clean iso
