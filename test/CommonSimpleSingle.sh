mkdir -p test/bin/CommonSimpleSingle/

valgrind --leak-check=full bin/z64convert-debug --in test/CommonSimpleSingle/complete_room.objex --out test/bin/CommonSimpleSingle/output --print-palettes --header -
