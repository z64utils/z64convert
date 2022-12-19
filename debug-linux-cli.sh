mkdir -p bin/

gcc -o bin/z64convert-debug -DGBI_PREFIX=F3DEX2 -DVFILE_VISIBILITY=static -DNDEBUG -Wno-unused-function -Wno-unused-variable -Wall -lm -Og -g -Igfxasm/src -Iwowlib -DWOW_OVERLOAD_FILE src/*.c gfxasm/src/*.c -Isrc -lm 

