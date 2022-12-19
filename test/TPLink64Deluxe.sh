mkdir -p test/bin/TPLink64Deluxe/

valgrind --leak-check=full bin/z64convert-debug --in test/TPLink64Deluxe/TPLink64Deluxe.objex --out test/bin/TPLink64Deluxe/TPLink64Deluxe.zobj --playas
