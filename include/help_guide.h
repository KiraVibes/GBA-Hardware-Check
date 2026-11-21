#ifndef HELP_GUIDE_H
#define HELP_GUIDE_H

#include "gba.h"

enum HelpTopic {
    HELP_TOPIC_HEALTH = 0,
    HELP_TOPIC_SYSTEM,
    HELP_TOPIC_GAMEPAD,
    HELP_TOPIC_DISPLAY,
    HELP_TOPIC_AUDIO,
    HELP_TOPIC_BENCHMARK,
    HELP_TOPIC_STRESS,
    HELP_TOPIC_BATTERY,
    HELP_TOPIC_LINK,
    HELP_TOPIC_EZFLASH,
    HELP_TOPIC_COUNT
};

/* Display comprehensive, interactive hardware guide */
void run_help_guide(enum HelpTopic initial_topic);

#endif /* HELP_GUIDE_H */
