mkdir -p bin/release

gcc -o bin/release/z64convert-linux-gui -DZ64CONVERT_GUI `./common.sh` `wowlib/deps/wow_gui_x11.sh`

