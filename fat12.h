#pragma once

enum {
    FAT12_ERROR_SUCCESS,
    FAT12_ERROR_CAN_NOT_OPEN_IMAGE_FILE,
    FAT12_ERROR_CAN_NOT_ALLOCATE_MEMORY_FOR_IMAGE,
};

typedef struct {
    char *image;
    int imageSize;
    int errorCode;
    int bytesPerSector;
    int sectorsPerClaster;
    int reservedSectorCount;
    int numberOfFats;
    int maxRootEntries;
    int totalSectors;
    int sectorsPerFat;
    int firstFat;
    int rootDirectory;
    int dataRegion;
} Fat12;
