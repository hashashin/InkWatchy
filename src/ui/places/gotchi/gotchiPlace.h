#pragma once

#include "defines.h"

#ifndef GOTCHI
#define GOTCHI 0
#endif

#if GOTCHI

#include <atomic>

#ifndef GOTCHI_ROM_FILE
#define GOTCHI_ROM_FILE "tama.b"
#endif

#ifndef GOTCHI_STATE_FILE
#define GOTCHI_STATE_FILE "gotchi_state_v1"
#endif

#ifndef GOTCHI_TASK_STACK_SIZE
#define GOTCHI_TASK_STACK_SIZE 6144
#endif

#ifndef GOTCHI_MOTOR
#define GOTCHI_MOTOR 0
#endif

#ifndef GOTCHI_MOTOR_MS
#define GOTCHI_MOTOR_MS 30
#endif

#ifndef GOTCHI_MOTOR_DELAY_MS
#define GOTCHI_MOTOR_DELAY_MS 250
#endif

#define GOTCHI_ROM_SIZE 9216
#define GOTCHI_BUTTON_TICKS 200

struct GotchiButtons
{
    std::atomic<uint16_t> left{0};
    std::atomic<uint16_t> right{0};
    std::atomic<uint16_t> middle{0};
};

extern GotchiButtons gotchiButtons;
extern std::mutex gotchiBuffMutex;
extern GFXcanvas1 *gotchiBuff;

bool startGotchiTask();
void endGotchiTask();

void initGotchi();
void loopGotchi();
void exitGotchi();

#endif
