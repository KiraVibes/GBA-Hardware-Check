#include "../include/localization.h"

Language g_lang = LANG_DE;

void lang_init(void) {
    g_lang = LANG_DE;
}

void lang_toggle(void) {
    g_lang = (g_lang == LANG_DE) ? LANG_EN : LANG_DE;
}

const char* lang_get_badge(void) {
    return (g_lang == LANG_DE) ? "[DE]" : "[EN]";
}
