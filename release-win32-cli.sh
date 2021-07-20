mkdir -p bin/release

PATH=$PATH:~/c/mxe/usr/bin

i686-w64-mingw32.static-gcc -o bin/release/z64convert-cli.exe `./common.sh` \
	-municode -mconsole

