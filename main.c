#include "../../backend/Rss.h"

#define ONESRC
#include "../../backend/Rss.c"
#undef  ONESRC

#include <stdio.h>
#include <string.h>
#include <kolibrisys.h>
#include <kos32sys1.h>
#include <box_lib.h>

#define FONT_6x9_CP866    0
#define FONT_8x16_CP866   0x10000000
#define FONT_8x16_UTF16LE 0x20000000
#define FONT_8x16_UTF8    0x30000000

#define BT_NORMAL  0
#define BT_DEL     0x80000000
#define BT_HIDE    0x40000000
#define BT_NOFRAME 0x20000000

#define ID_BTN_ADD_SOURCE 43

struct kolibri_system_colors sys_color_table;
Rss *rss;
EditBox newRssLink;
char textBuffer[9999];

unsigned long (stdcall *InputBox)(void* Buffer, char* Caption, char* Prompt, char* Default,
                                  unsigned long Flags, unsigned long BufferSize, void* RedrawProc);

size_t utf8len(char *utf8) {
    size_t len = 0;
    
    for (size_t idx = 0; utf8[idx]; idx++) {
        char c = utf8[idx];

        if ((c & 0b11111000) == 0b11110000 ||
            (c & 0b11110000) == 0b11100000 ||
            (c & 0b11100000) == 0b11000000 ||
            (c & 0b10000000) == 0b00000000) {
            len++;
        }
    }
    return len;
}

void draw_window() {
    begin_draw();
    sys_create_window(10, 40, 320, 240, 0, sys_color_table.work_area, 0x13);
    draw_text_sys("BsRSS (фронт-энд Колибри)", 4, 4, 0, 0x90000000 | FONT_8x16_UTF8);
    define_button((5 << 16) + 310, (36 << 16) + 13, ID_BTN_ADD_SOURCE, 0xeeeeee);
    draw_text_sys("Добавить RSS канал", 5, 36, 0, 0x90000000 | FONT_8x16_UTF8 | sys_color_table.work_text);
    draw_text_sys(textBuffer, 5, 64, 0, 0x90000000 | FONT_8x16_UTF8 | sys_color_table.work_text);
    end_draw();
}

int main(int argc, char **argv) {
    int errorCode = 0;

    debug("\nGETTING STARTED...\n\n\n");
    {
        void *exp = NULL;

        debug(FILE_LINE " Loading Inputbox.obj\n");
        if (!(exp = _ksys_cofflib_load("/sys/lib/Inputbox.obj")))
            { return -1; }
        debug(FILE_LINE " Loading Inputbox::InputBox\n");
        if (!(InputBox = _ksys_cofflib_getproc(exp, "InputBox")))
            { return -1; }
    }
    debug(FILE_LINE ": InputBox loaded\n");
    get_system_colors(&sys_color_table);
    debug(FILE_LINE ": System colors getted\n");
    if ((errorCode = sysInit()))
        { debug("#%d in %s", errorCode, FILE_LINE); return errorCode; }
    if (!(rss = malloc(rssSize())))
        { debug("Out of memory in " FILE_LINE); return RSS_ERROR_OUT_OF_MEMORY; }
    if ((errorCode = rssInit(rss)))
        { debug("#%d in %s", errorCode, FILE_LINE); return errorCode; }
    debug("rssInit done!\n");
    //if ((errorCode = rssAddSource(rss, "http://kuzmolovo.ru/News/news.xml", NULL))) // "http://ariom.ru/blog/txt/export.xml", NULL))) // "http://feeds.bbci.co.uk/news/rss.xml", NULL)))
    //    { debug("#%d in %s", errorCode, FILE_LINE); return errorCode; }
    //debug("rssAddSource done!\n");
    for (;;) {
        switch (wait_for_event(10)) {
        case KOLIBRI_EVENT_BUTTON:
            switch (get_os_button()) {
                case ID_BTN_ADD_SOURCE:
                    {
                        char buffer[256];
                        Source *source = NULL;
                        Item *item = NULL;

                        if ((errorCode = InputBox(buffer, "TO IMPLEMENT: New source", "Введите ссылку",
                            "http://feeds.reuters.com/Reuters/domesticNews", 0, 256, NULL)))
                            { debug("Input error $%d", errorCode); return errorCode; }
                        if ((errorCode = rssAddSource(rss, buffer, &source)))
                            { debug("%d in %s", errorCode, FILE_LINE); return errorCode; }
                        if ((errorCode = rssSourceGetReady(source)))
                            { debug("%d in %s", errorCode, FILE_LINE); return errorCode; }
                        if (!(errorCode = rssSourceGetNextItem(source, &item))) {
                            strcat(textBuffer, item->title);
                            strcat(textBuffer, "\n\n");
                            strcat(textBuffer, item->description);
                            strcat(textBuffer, "\n\n\n");
                            sysFree(item);
                        }
                        if (!(errorCode = rssSourceGetNextItem(source, &item))) {
                            strcat(textBuffer, item->title);
                            strcat(textBuffer, "\n\n");
                            strcat(textBuffer, item->description);
                            strcat(textBuffer, "\n\n\n");
                            sysFree(item);
                        }
                        if (errorCode && errorCode != RSS_ERROR_END_OF_ITEMS)
                            { debug("Error #%d", errorCode); }
                        rssSourceRelax(source);
                    }
                    break;
            }
            break;
        case KOLIBRI_EVENT_REDRAW:
            draw_window();
            break;
        }
    }
    rssFree(rss);
    free(rss);
    free(newRssLink.text);
    return 0;
}
