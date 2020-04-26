#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>

typedef struct {
    char *image;
    int imageSize;
    char *errorMessage;
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

static char *imageFile;
static char outputFolder[4096];

// system-dependent
static void mkdir(const char *name);
// misc
static void mkdir_p(const char *_name); // create folder creating its parents
static uint16_t get16(void *_from, int index); // get uint16_t from array at offset
static uint32_t get32(void *_from, int index); // get uint32_t from array at offset
// fat12
static int fat12__getItemNameSize(void *_folderEntry);
static void fat12__getItemName(void *_folderEntry, void *_name);
static int fat12__getNextClaster(Fat12 *this, int currentClaster);
static int fat12__getFile(Fat12 *this, void *_buffer, int size, int claster);
static int fat12__getOffsetByClaster(Fat12 *this, int claster);
static int fat12__handleFolder(Fat12 *this, int claster);
static int fat12__handleRootFolder(Fat12 *this);
static int fat12__open(Fat12 *this, const char *img);
static int fat12__error(Fat12 *this, char *errorMessage);

static void mkdir(const char *name) {
    struct {
        int fn;
        int fuck[4];
        char b;
        const char *path __attribute__((packed));
    } info;
    memset(&info, 0, sizeof(info));
    info.fn = 9;
    info.b = 0;
    info.path = name;
    asm volatile ("int $0x40"::"a"(70), "b"(&info));
}

static void mkdir_p(const char *_name) {
    char *name = calloc(strlen(_name) + 1, 1);

    strcpy(name, _name);
    char *ptr = name;
    while (ptr) {
        if (ptr != name) { *ptr = '/'; }
        ptr = strchr(ptr + 1, '/');
        if (ptr) { *ptr = 0; }
        mkdir(name);
    }
}

static uint32_t get32(void *_from, int index) {
    uint8_t *from = _from;
    return from[index] |
        (from[index + 1] << 8) |
        (from[index + 2] << 16) |
        (from[index + 3] << 24);
}

static uint16_t get16(void *_from, int index) {
    uint8_t *from = _from;

    return from[index] | (from[index + 1] << 8);
}

static int fat12__getNextClaster(Fat12 *this, int currentClaster) {
    int nextClasterOffset = this->firstFat + currentClaster + (currentClaster >> 1);

    if (currentClaster % 2 == 0) {
        return get16(this->image, nextClasterOffset) & 0xfff;
    } else {
        return get16(this->image, nextClasterOffset) >> 4;
    }
}

static int fat12__getFile(Fat12 *this, void *_buffer, int size, int claster) {
    int offset = 0;
    char *buffer = _buffer;

    while (claster < 0xff7) {
        int toCopy = this->bytesPerSector * this->sectorsPerClaster;
        void *clasterPtr = &this->image[fat12__getOffsetByClaster(this, claster)];

        claster = fat12__getNextClaster(this, claster);
        // if next claster is END OF FILE claster, copy only rest of file
        if (claster >= 0xff7) { toCopy = size % toCopy; }
        memcpy(&buffer[offset], clasterPtr, toCopy);
        offset += toCopy;
    }
    return 1;
}

static int fat12__getOffsetByClaster(Fat12 *this, int claster) {
    return this->dataRegion + (claster - 2) 
        * this->bytesPerSector * this->sectorsPerClaster;
}

static int fat12__getItemNameSize(void *_folderEntry) {
    uint8_t *folderEntry = _folderEntry;

    // Long File Name entry, not a file itself
    if ((folderEntry[11] & 0x0f) == 0x0f) { return 0; }
    // file with long name
    if ((folderEntry[11 - 32] & 0x0f) == 0x0f) { return 1024; }
    // regular FAT12 file
    return 13; // "NAME8888" '.' "EXT" '\x0'
}

static void fat12__getItemName(void *_folderEntry, void *_name) {
    uint8_t *folderEntry = _folderEntry;
    uint8_t *name = _name;

    if ((folderEntry[11 - 32] & 0x0f) != 0x0f) {
        int length = 8;

        memset(name, 0, 13);
        memcpy(name, folderEntry, 8);
        while (name[length - 1] == ' ') { length--; }
        if (folderEntry[9] != ' ') {
            name[length++] = '.';
            memcpy(&name[length], &folderEntry[8], 3);
            length += 3;
        }
        while (name[length - 1] == ' ') { length--; }
        name[length] = '\0';
    } else {
        // previous folder entries hold long name in format:
        // 0 sequence nmber (in turn back to first Long File Name entry, from 1)
        // 1 - 10 file name next characters in utf-16
        // 11 file attributes (0x0f - LFN entry)
        // 12 reserved
        // 13 checksum
        // 14 - 25 file name next characters
        // 26 - 27 reserved
        // 28 - 31 file name next characters
        // in these entries name placed in sequential order
        // but first characters are located in first previous entry
        // next characters - in next previous etc.
        // if current entry is orificated by 0x40 - the entry is last (cinains last characters)
        // unneed places for characters in the last entry are filled by 0xff
        int length = 0;

        for (int i = 1; i < 255 / 13; i++) {
            //! TODO: Add unicode support
            name[length++] = folderEntry[i * -32 + 1];
            name[length++] = folderEntry[i * -32 + 3];
            name[length++] = folderEntry[i * -32 + 5];
            name[length++] = folderEntry[i * -32 + 7];
            name[length++] = folderEntry[i * -32 + 9];
            name[length++] = folderEntry[i * -32 + 14];
            name[length++] = folderEntry[i * -32 + 16];
            name[length++] = folderEntry[i * -32 + 18];
            name[length++] = folderEntry[i * -32 + 20];
            name[length++] = folderEntry[i * -32 + 22];
            name[length++] = folderEntry[i * -32 + 24];
            name[length++] = folderEntry[i * -32 + 28];
            name[length++] = folderEntry[i * -32 + 30];
            if (folderEntry[i * -32] & 0x40) {
                while (name[length - 1] == 0xff) { name[--length] = 0; }
                name[length++] = 0;
                return;
            }
        }
    }
}

static int fat12__handleFolderEntry(Fat12 *this, int folderEntryOffset) {
    int nameSize = 0;
    char *name = NULL;

    if (!this->image[folderEntryOffset]) { return 1; }
    nameSize = fat12__getItemNameSize(&this->image[folderEntryOffset]);
    if (nameSize != 0) {
        name = malloc(nameSize);
        fat12__getItemName(&this->image[folderEntryOffset], name);
        if ((this->image[folderEntryOffset + 11] & 0x10)) {
            // handle folder only if it isn't current folder or parent one
            if (memcmp(&this->image[folderEntryOffset], ".          ", 11) &&
                memcmp(&this->image[folderEntryOffset], "..         ", 11)) {
                size_t oldStringEnd = strlen(outputFolder);

                strcat(outputFolder, "/");
                strcat(outputFolder, name);
                if (!fat12__handleFolder(this, get16(this->image, folderEntryOffset + 26))) {
                    free(name);
                    return 0;
                }
                outputFolder[oldStringEnd] = '\0';
            }
        } else {
            void *buffer = NULL;
            int size = get32(this->image, folderEntryOffset + 28);
            int cluster = get16(this->image, folderEntryOffset + 26);

            buffer = malloc(size);
            if (!fat12__getFile(this, buffer, size, cluster)) {
                free(buffer);
                return 0;
            }
            {
                size_t oldStringEnd = strlen(outputFolder);
                FILE *fp = NULL;

                mkdir_p(outputFolder);
                strcat(outputFolder, "/");
                strcat(outputFolder, name);
                con_printf("Extracting %s\n", outputFolder);
                fp = fopen(outputFolder, "wb");
                if (!fp) { perror(NULL); }
                fwrite(buffer, 1, size, fp);
                fclose(fp);
                outputFolder[oldStringEnd] = '\0';
            }
            free(buffer);
        }
        free(name);
    }
    return 1;
}

static int fat12__handleFolder(Fat12 *this, int claster) {
    for (; claster < 0xff7; claster = fat12__getNextClaster(this, claster)) {
        int offset = fat12__getOffsetByClaster(this, claster);

        for (int i = 0; i < (this->bytesPerSector * this->sectorsPerClaster / 32); i++) {
            if (!fat12__handleFolderEntry(this, offset + 32 * i)) {
                return 0;
            }
        }
    }
    return 1;
}

static int fat12__handleRootFolder(Fat12 *this) {
    for (int i = 0; i < this->maxRootEntries; i++) {
        if (!fat12__handleFolderEntry(this, this->rootDirectory + 32 * i)) {
            return 0;
        }
    }
    return 1;
}

static int fat12__open(Fat12 *this, const char *img) {
    FILE *fp = NULL;

    if (!(fp = fopen(img, "rb"))) { 
        return fat12__error(this, "Can't open imput file"); 
    }
    fseek(fp, 0, SEEK_END);
    this->imageSize = ftell(fp);
    rewind(fp);
    if (!(this->image = malloc(this->imageSize))) { 
        return fat12__error(this, "Can't allocate memory for image"); 
    }
    fread(this->image, 1, this->imageSize, fp);
    fclose(fp);
    this->bytesPerSector = *(uint16_t *)((uintptr_t)this->image + 11);
    this->sectorsPerClaster = *(uint8_t *)((uintptr_t)this->image + 0x0d);
    this->reservedSectorCount = *(uint16_t *)((uintptr_t)this->image + 0x0e);
    this->numberOfFats = *(uint8_t *)((uintptr_t)this->image + 0x10);
    this->maxRootEntries = *(uint16_t *)((uintptr_t)this->image + 0x11);
    this->totalSectors = *(uint16_t *)((uintptr_t)this->image + 0x13);
    if (!this->totalSectors) { 
        this->totalSectors = *(uint32_t *)((uintptr_t)this->image + 0x20); 
    }
    this->sectorsPerFat = *(uint16_t *)((uintptr_t)this->image + 0x16);
    this->firstFat = (0 + this->reservedSectorCount) * this->bytesPerSector;
    this->rootDirectory = this->firstFat + this->numberOfFats 
        * this->sectorsPerFat * this->bytesPerSector;
    this->dataRegion = this->rootDirectory + this->maxRootEntries * 32;
    con_printf("Bytes per sector: %d\n",      this->bytesPerSector);
    con_printf("Sectors per claster: %d\n",   this->sectorsPerClaster);
    con_printf("Reserver sector count: %d\n", this->reservedSectorCount);
    con_printf("Number of FATs: %d\n",        this->numberOfFats);
    con_printf("Max root entries: %d\n",      this->maxRootEntries);
    con_printf("Total sectors: %d\n",         this->totalSectors);
    con_printf("Sectors per FAT: %d\n",       this->sectorsPerFat);
    con_printf("First FAT: %d\n",             this->firstFat);
    con_printf("Root directory: %d\n",        this->rootDirectory);
    con_printf("Data region: %d\n",           this->dataRegion);
    return 1;
}

static int fat12__error(Fat12 *this, char *errorMessage) {
    this->errorMessage = errorMessage;
    return 0;
}

static int handleError(Fat12 *fat12) {
    printf("Error in Fat12: %s\n", fat12->errorMessage);
    return -1;
}




int main(int argc, char **argv) {
    Fat12 *fat12 = malloc(sizeof(Fat12));

    if (con_init_console_dll()) return -1;
    con_set_title("UnImg - kolibri.img file unpacker");

    if (argc < 2) { 
        con_write_asciiz("Usage: unimg \"/path/to/kolibri.img\" \"/optional/extract/path\""); 
        con_exit(0);
        return -1;
    }
    
    imageFile = argv[1];

    if (argc >= 3) strcpy(outputFolder, argv[2]);
    else strcpy(outputFolder, "/TMP0/1/KOLIBRI.IMG");

    if (!fat12__open(fat12, imageFile)) { 
        return handleError(fat12); 
    }

    if (!fat12__handleRootFolder(fat12)) {
        return handleError(fat12);
    }

    free(fat12);
    con_write_asciiz("\nDONE!\n\n");
    con_exit(0);
}
