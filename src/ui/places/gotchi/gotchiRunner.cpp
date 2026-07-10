#include "gotchiPlace.h"

#if GOTCHI

#include "emulator/bitmaps.h"
#include "emulator/hw.h"
#include "emulator/tamalib.h"

#define GOTCHI_TIMESTAMP_HZ 1000000
#define GOTCHI_DISPLAY_FRAMERATE 3
#define GOTCHI_TASK_PRIORITY 6
#define GOTCHI_YIELD_STEPS 128
#define GOTCHI_SAVE_MAGIC 0x474F5443UL
#define GOTCHI_SAVE_VERSION 1

#define GOTCHI_MATRIX_X 4
#define GOTCHI_MATRIX_Y 52
#define GOTCHI_PIXEL_STEP 6
#define GOTCHI_PIXEL_SIZE 5
#define GOTCHI_ICON_SCALE 3
#define GOTCHI_ICON_WIDTH 9
#define GOTCHI_ICON_HEIGHT 9
#define GOTCHI_ICON_DRAW_WIDTH (GOTCHI_ICON_WIDTH * GOTCHI_ICON_SCALE)
#define GOTCHI_ICON_DRAW_HEIGHT (GOTCHI_ICON_HEIGHT * GOTCHI_ICON_SCALE)
#define GOTCHI_ICON_X 12
#define GOTCHI_ICON_X_STEP 50
#define GOTCHI_TOP_ICONS_Y 2
#define GOTCHI_BOTTOM_ICONS_Y 171

struct GotchiSaveData
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    cpu_state_t cpu;
    uint8_t memory[MEMORY_SIZE];
};

GotchiButtons gotchiButtons;
std::mutex gotchiBuffMutex;
GFXcanvas1 *gotchiBuff = nullptr;

static std::atomic<bool> stopRequested{false};
static std::atomic<bool> taskStopped{true};
static TaskHandle_t gotchiHandle = nullptr;

static bool_t matrixBuffer[LCD_HEIGHT][LCD_WIDTH / 8] = {{0}};
static bool_t iconBuffer[ICON_NUM] = {0};

static bool consumeTick(std::atomic<uint16_t> &ticks)
{
    uint16_t value = ticks.load();
    while (value > 0)
    {
        if (ticks.compare_exchange_weak(value, value - 1))
        {
            return true;
        }
    }
    return false;
}

static bool saveState()
{
    GotchiSaveData saved = {};
    saved.magic = GOTCHI_SAVE_MAGIC;
    saved.version = GOTCHI_SAVE_VERSION;
    saved.size = sizeof(GotchiSaveData);
    cpu_get_state(&saved.cpu);
    if (saved.cpu.memory == nullptr)
    {
        return false;
    }

    memcpy(saved.memory, saved.cpu.memory, MEMORY_SIZE);
    saved.cpu.memory = nullptr;
    return fsSetBlob(GOTCHI_STATE_FILE, reinterpret_cast<uint8_t *>(&saved), sizeof(saved));
}

static bool restoreState()
{
    bufSize blob = fsGetBlob(GOTCHI_STATE_FILE);
    if (blob.buf == nullptr)
    {
        return false;
    }

    bool restored = false;
    if (blob.size == sizeof(GotchiSaveData))
    {
        GotchiSaveData saved;
        memcpy(&saved, blob.buf, sizeof(saved));
        if (saved.magic == GOTCHI_SAVE_MAGIC &&
            saved.version == GOTCHI_SAVE_VERSION &&
            saved.size == sizeof(GotchiSaveData))
        {
            cpu_state_t current;
            cpu_get_state(&current);
            memcpy(current.memory, saved.memory, MEMORY_SIZE);
            saved.cpu.memory = current.memory;
            cpu_set_state(&saved.cpu);
            cpu_sync_ref_timestamp();
            restored = true;
        }
    }

    free(blob.buf);
    if (!restored)
    {
        debugLog("Ignoring incompatible Gotchi save");
    }
    return restored;
}

static void halHalt()
{
}

static void halLog(log_level_t level, char *message, ...)
{
    (void)level;
    (void)message;
}

static timestamp_t halGetTimestamp()
{
    return millisBetter() * 1000ULL;
}

static void halSleepUntil(timestamp_t timestamp)
{
    timestamp_t now = halGetTimestamp();
    if (timestamp > now)
    {
        uint64_t remaining = timestamp - now;
        delayMicroseconds(static_cast<uint32_t>(remaining));
    }
}

static void drawScaledIcon(uint8_t icon, int16_t targetX, int16_t targetY)
{
    const uint8_t *bitmap = gotchiBitmaps + icon * 18;
    gotchiBuff->fillRect(targetX, targetY, GOTCHI_ICON_DRAW_WIDTH, GOTCHI_ICON_DRAW_HEIGHT, GxEPD_WHITE);
    for (uint8_t y = 0; y < GOTCHI_ICON_HEIGHT; y++)
    {
        const uint16_t row = pgm_read_byte(bitmap + y * 2) |
                             (pgm_read_byte(bitmap + y * 2 + 1) << 8);
        for (uint8_t x = 0; x < GOTCHI_ICON_WIDTH; x++)
        {
            if (row & (1U << x))
            {
                gotchiBuff->fillRect(targetX + x * GOTCHI_ICON_SCALE,
                                    targetY + y * GOTCHI_ICON_SCALE,
                                    GOTCHI_ICON_SCALE,
                                    GOTCHI_ICON_SCALE,
                                    GxEPD_BLACK);
            }
        }
    }
}

static void drawIconMarker(int16_t centerX, int16_t y, bool pointsDown, bool active)
{
    gotchiBuff->fillRect(centerX - 6, y, 12, 6, GxEPD_WHITE);
    if (!active)
    {
        return;
    }

    for (uint8_t row = 0; row < 5; row++)
    {
        const uint8_t width = pointsDown ? 10 - row * 2 : 2 + row * 2;
        gotchiBuff->drawFastHLine(centerX - width / 2, y + row, width, GxEPD_BLACK);
    }
}

static void drawMatrixRow(uint8_t sourceY)
{
    const int16_t y = GOTCHI_MATRIX_Y + sourceY * GOTCHI_PIXEL_STEP;
    for (uint8_t x = 0; x < LCD_WIDTH; x++)
    {
        uint8_t mask = 0x80 >> (x % 8);
        uint16_t color = (matrixBuffer[sourceY][x / 8] & mask) ? GxEPD_BLACK : GxEPD_WHITE;
        gotchiBuff->fillRect(GOTCHI_MATRIX_X + x * GOTCHI_PIXEL_STEP,
                            y,
                            GOTCHI_PIXEL_SIZE,
                            GOTCHI_PIXEL_SIZE,
                            color);
    }
}

static void drawIcons()
{
    for (uint8_t i = 0; i < ICON_NUM; i++)
    {
        const uint8_t column = i % 4;
        const int16_t x = GOTCHI_ICON_X + column * GOTCHI_ICON_X_STEP;
        if (i < 4)
        {
            drawScaledIcon(i, x, GOTCHI_TOP_ICONS_Y);
            drawIconMarker(x + GOTCHI_ICON_DRAW_WIDTH / 2, 32, true, iconBuffer[i]);
        }
        else
        {
            drawIconMarker(x + GOTCHI_ICON_DRAW_WIDTH / 2, 161, false, iconBuffer[i]);
            drawScaledIcon(i, x, GOTCHI_BOTTOM_ICONS_Y);
        }
    }
}

static void displayTama()
{
    std::lock_guard<std::mutex> lock(gotchiBuffMutex);
    if (gotchiBuff == nullptr)
    {
        return;
    }

    for (uint8_t y = 0; y < LCD_HEIGHT; y++)
    {
        drawMatrixRow(y);
    }
    drawIcons();
}

static void halUpdateScreen()
{
    displayTama();
}

static void halSetLcdMatrix(u8_t x, u8_t y, bool_t value)
{
    uint8_t mask = 0x80 >> (x % 8);
    if (value)
    {
        matrixBuffer[y][x / 8] |= mask;
    }
    else
    {
        matrixBuffer[y][x / 8] &= ~mask;
    }
}

static void halSetLcdIcon(u8_t icon, bool_t value)
{
    if (icon < ICON_NUM)
    {
        iconBuffer[icon] = value;
    }
}

static void halSetFrequency(u32_t frequency)
{
    (void)frequency;
}

static void halPlayFrequency(bool_t enabled)
{
#if GOTCHI_MOTOR
    static uint64_t lastBuzz = 0;
    uint64_t now = millisBetter();
    if (enabled && now - lastBuzz >= GOTCHI_MOTOR_DELAY_MS)
    {
        lastBuzz = now;
        vibrateMotor(GOTCHI_MOTOR_MS);
    }
#else
    (void)enabled;
#endif
}

static int halHandler()
{
    hw_set_button(BTN_MIDDLE, consumeTick(gotchiButtons.middle) ? BTN_STATE_PRESSED : BTN_STATE_RELEASED);
    hw_set_button(BTN_RIGHT, consumeTick(gotchiButtons.right) ? BTN_STATE_PRESSED : BTN_STATE_RELEASED);
    hw_set_button(BTN_LEFT, consumeTick(gotchiButtons.left) ? BTN_STATE_PRESSED : BTN_STATE_RELEASED);
    return 0;
}

static hal_t hal = {
    halHalt,
    halLog,
    halSleepUntil,
    halGetTimestamp,
    halUpdateScreen,
    halSetLcdMatrix,
    halSetLcdIcon,
    halSetFrequency,
    halPlayFrequency,
    halHandler,
};

static void drawShell()
{
    gotchiBuff->fillScreen(GxEPD_WHITE);
}

static void gotchiRun(void *parameter)
{
    (void)parameter;
    tamalib_register_hal(&hal);
    tamalib_set_framerate(GOTCHI_DISPLAY_FRAMERATE);
    tamalib_init(GOTCHI_TIMESTAMP_HZ);
    restoreState();
    displayTama();

    uint16_t steps = 0;
    while (!stopRequested.load())
    {
        tamalib_mainloop_step_by_step();
        if (++steps >= GOTCHI_YIELD_STEPS)
        {
            steps = 0;
            vTaskDelay(1);
        }
    }

    saveState();
    taskStopped.store(true);
    gotchiHandle = nullptr;
    vTaskDelete(nullptr);
}

bool startGotchiTask()
{
    debugLog("Starting Gotchi");
    stopRequested.store(false);
    taskStopped.store(false);
    memset(matrixBuffer, 0, sizeof(matrixBuffer));
    memset(iconBuffer, 0, sizeof(iconBuffer));

    {
        std::lock_guard<std::mutex> lock(gotchiBuffMutex);
        gotchiBuff = new GFXcanvas1(200, 200, true);
        if (gotchiBuff == nullptr || gotchiBuff->getBuffer() == nullptr)
        {
            delete gotchiBuff;
            gotchiBuff = nullptr;
            taskStopped.store(true);
            return false;
        }
        drawShell();
    }

    BaseType_t result = xTaskCreate(gotchiRun,
                                    "gotchiTask",
                                    GOTCHI_TASK_STACK_SIZE,
                                    nullptr,
                                    GOTCHI_TASK_PRIORITY,
                                    &gotchiHandle);
    if (result != pdPASS)
    {
        std::lock_guard<std::mutex> lock(gotchiBuffMutex);
        delete gotchiBuff;
        gotchiBuff = nullptr;
        gotchiHandle = nullptr;
        taskStopped.store(true);
        return false;
    }
    return true;
}

void endGotchiTask()
{
    stopRequested.store(true);
    uint32_t waitedMs = 0;
    while (!taskStopped.load() && waitedMs < 2000)
    {
        delayTask(5);
        waitedMs += 5;
    }

    if (!taskStopped.load() && gotchiHandle != nullptr)
    {
        debugLog("Gotchi task did not stop cleanly");
        vTaskDelete(gotchiHandle);
        gotchiHandle = nullptr;
        taskStopped.store(true);
    }

    std::lock_guard<std::mutex> lock(gotchiBuffMutex);
    delete gotchiBuff;
    gotchiBuff = nullptr;
}

#endif
