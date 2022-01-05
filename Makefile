FLAGS := -DGBI_PREFIX=F3DEX2 -DVFILE_VISIBILITY=static -DNDEBUG -Wno-unused-function -Wno-unused-variable -Wall -Os -s -Igfxasm/src -Iwowlib -DWOW_OVERLOAD_FILE -Isrc
MFLAGS :=  -lm -flto
WinGcc := i686-w64-mingw32.static-gcc
WindRes := i686-w64-mingw32.static-windres

SourceC := $(shell find src/* -type f -name '*.c') $(shell find gfxasm/src/* -type f -name '*.c')
BuildGuiWinO := $(foreach f,$(SourceC:.c=.o),bin/gui/win32/$f)
BuildCliWinO := $(foreach f,$(SourceC:.c=.o),bin/cli/win32/$f)

BuildGuiLinO := $(foreach f,$(SourceC:.c=.o),bin/gui/linux/$f)
BuildCliLinO := $(foreach f,$(SourceC:.c=.o),bin/cli/linux/$f)

.PHONY: wingui wincli clean

$(shell mkdir -p bin/release)
$(shell mkdir -p bin/ $(foreach dir, \
	$(dir $(BuildGuiWinO)) \
	$(dir $(BuildCliWinO)) \
	$(dir $(BuildGuiLinO)) \
	$(dir $(BuildCliLinO)) \
	,$(dir)))

default:
	@echo Possible targets:
	@echo make lingui
	@echo make lincli
	@echo make wingui
	@echo make wincli

clean:
	@rm -f -r bin/**
	@rm -f z64convert-cl*
	@rm -f z64convert-gu*

# lin-gui:
# 	@gcc -o bin/release/z64convert-linux-gui -DZ64CONVERT_GUI $(FLAGS) `wowlib/deps/wow_gui_x11.sh`

# lin-cli:
# 	@gcc -o bin/release/z64convert-linux $(FLAGS)

# w32-gui:
# 	@$(WindRes) src/icon.rc -o bin/o/win32/icon.o
# 	@$(WinGcc) -o bin/release/z64convert.exe $(FLAGS) \
# 	-municode -DZ64CONVERT_GUI \
# 	-mwindows -lgdi32 -luser32 -lkernel32 -lm \
# 	bin/o/win32/icon.o \
# 	-Wno-switch -Wno-format -Wno-unused-but-set-variable

#
# Linux GUI
#

lingui: $(BuildGuiLinO) z64convert-gui

bin/gui/linux/%.o: %.c
	@echo [$<]
	@$(gcc) -c -o $@ $< $(FLAGS) \
 	-DZ64CONVERT_GUI \
 	-Wno-switch -Wno-format -Wno-unused-but-set-variable

z64convert-gui: $(BuildGuiLinO)
	@echo [$@]
	@$(gcc) -o $@ $^ $(FLAGS) $(MFLAGS) \
 	-DZ64CONVERT_GUI \
 	-Wno-switch -Wno-format -Wno-unused-but-set-variable \
	-no-pie -lX11 -lm -lpthread

#
# Linux CLI
#

lincli: $(BuildCliLinO) z64convert-cli

bin/cli/linux/%.o: %.c
	@echo [$<]
	@gcc -c -o $@ $< $(FLAGS) \
 	-Wno-switch -Wno-format -Wno-unused-but-set-variable

z64convert-cli: $(BuildCliLinO)
	@echo [$@]
	@gcc -o $@ $^ $(FLAGS) $(MFLAGS) \
 	-Wno-switch -Wno-format -Wno-unused-but-set-variable

#
# Windows32 GUI
#

wingui: $(BuildGuiWinO) bin/gui/win32/src/icon.ro z64convert-gui.exe

bin/gui/win32/%.o: %.c
	@echo [$<]
	@$(WinGcc) -c -o $@ $< $(FLAGS) \
 	-municode -DZ64CONVERT_GUI -mwindows \
 	-Wno-switch -Wno-format -Wno-unused-but-set-variable

bin/gui/win32/src/icon.ro: src/icon.rc
	@echo [$<]
	@$(WindRes) -o $@ $^

z64convert-gui.exe: $(BuildGuiWinO) bin/gui/win32/src/icon.ro
	@echo [$@]
	@$(WinGcc) -o $@ $^ $(FLAGS) $(MFLAGS) \
 	-municode -DZ64CONVERT_GUI -mwindows \
 	-lgdi32 -luser32 -lkernel32 -lm \
 	-Wno-switch -Wno-format -Wno-unused-but-set-variable

#
# Windows32 CLI
#

wincli: $(BuildCliWinO) z64convert-cli.exe

bin/cli/win32/%.o: %.c
	@echo [$<]
	@$(WinGcc) -c -o $@ $< $(FLAGS) \
 	-municode -mconsole \
 	-Wno-switch -Wno-format -Wno-unused-but-set-variable

z64convert-cli.exe: $(BuildCliWinO)
	@echo [z64convert-gui.exe]
	@$(WinGcc) -o $@ $^ $(FLAGS) $(MFLAGS) \
 	-municode -mconsole \
 	-Wno-switch -Wno-format -Wno-unused-but-set-variable
