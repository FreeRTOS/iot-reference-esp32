# C2DS-esp32-efuse-utility
Set the eFuse value to identify device type

Note
----
The `main/efuse_table.c/.h` files were generated using the `efuse_table_gen.py` utility in the `$ESP_IDF/component/efuse` directory.
Ex:
```bash
╭─tennis@tennismbp2021 ~/esp/esp-idf/components/efuse  ‹v5.3.2› 
╰─➤  python ./efuse_table_gen.py /Users/tennis/src/C2DS-esp32-efuse-utility/main/efuse_table.csv
Max number of bits in BLK 256
Parsing efuse CSV input file /Users/tennis/src/C2DS-esp32-efuse-utility/main/efuse_table.csv ...
Verifying efuse table...
Creating efuse *.h file /Users/tennis/src/C2DS-esp32-efuse-utility/main/include/efuse_table.h ...
Creating efuse *.c file /Users/tennis/src/C2DS-esp32-efuse-utility/main/efuse_table.c ...
```