
$(shell mkdir -p bin/release)
$(shell mkdir -p bin/o/win32)

FLAGS := -DGBI_PREFIX=F3DEX2 -DVFILE_VISIBILITY=static -DNDEBUG -Wno-unused-function -Wno-unused-variable -Wall -lm -flto -Os -s -flto -Igfxasm/src -Iwowlib -DWOW_OVERLOAD_FILE src/*.c gfxasm/src/*.c -Isrc
WinGcc := i686-w64-mingw32.static-gcc
WindRes := i686-w64-mingw32.static-windres

default:
	@echo Possible targets:
	@echo make lux-gui
	@echo make lux-cli
	@echo make w32-gui
	@echo make w32-cli

lux-gui:
	@gcc -o bin/release/z64convert-linux-gui -DZ64CONVERT_GUI $(FLAGS) `wowlib/deps/wow_gui_x11.sh`

lux-cli:
	@gcc -o bin/release/z64convert-linux $(FLAGS)

w32-gui:
	@$(WindRes) src/icon.rc -o bin/o/win32/icon.o
	@$(WinGcc) -o bin/release/z64convert.exe $(FLAGS) \
	-municode -DZ64CONVERT_GUI \
	-mwindows -lgdi32 -luser32 -lkernel32 -lm \
	bin/o/win32/icon.o \
	-Wno-switch -Wno-format -Wno-unused-but-set-variable

w32-cli:
	@$(WinGcc) -o bin/release/z64convert-cli.exe $(FLAGS) \
	-municode -mconsole