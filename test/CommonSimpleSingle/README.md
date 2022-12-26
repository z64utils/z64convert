# CommonSimpleSingle

This folder contains a single objex2 export describing three files. It is intended to test `z64convert`'s ability to divide a single input file into multiple output files while optimizing shared assets into one common data bank.

A successful test is characterized by:

- Output consisting of two room files and one scene file.

- Both room files should reference texture data that exists only in the scene file.

- The scene file should contain collision data.

The output room and scene files are not game-ready, as they do not contain Zelda64-compliant headers. They are instead intended to be loaded into `z64scene`.

A big thank you to [@Zeldaboy14](https://github.com/Zeldaboy14) for preparing this export on such short notice!
