# KolibriOS Image Unpacker
## Summary
Extracts files from FAT12 KolibriOS image to specified folder.

## How to use
```unimg path/to/img output/folder```

## How to build
```tcc fat12.c -lck -o unimg.kex```

## Toolchain
Default toolchain for TCC on Kolibri, got from KolibriISO/develop/tcc
