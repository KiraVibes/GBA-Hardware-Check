#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include "gba.h"

typedef enum {
    LANG_DE = 0,
    LANG_EN,
    LANG_COUNT
} Language;

extern Language g_lang;

void lang_init(void);
void lang_toggle(void);
const char* lang_get_badge(void);

#endif /* LOCALIZATION_H */
