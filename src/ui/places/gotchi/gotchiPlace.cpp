#include "gotchiPlace.h"

#if GOTCHI

#include "emulator/cpu.h"

#define GOTCHI_BUFFER_SIZE 5000
#define GOTCHI_FONT_TITLE getFont("dogicapixel4")
#define GOTCHI_FONT_BODY getFont("UbuntuMono10")

static bool gotchiReady = false;
static bool gotchiHelpVisible = false;

static void drawHelp()
{
    dis->fillScreen(SCWhite);
    dis->setTextColor(SCBlack);
    dis->setTextWrap(false);
    dis->drawRect(4, 4, 192, 192, SCBlack);

    dis->setFont(GOTCHI_FONT_TITLE);
    dis->setCursor(14, 20);
    dis->print(GOTCHI_HELP_TITLE);
    dis->drawFastHLine(14, 29, 172, SCBlack);

    dis->setFont(GOTCHI_FONT_BODY);
    dis->setCursor(24, 61);
    dis->print(GOTCHI_HELP_DOWN);
    dis->setCursor(24, 88);
    dis->print(GOTCHI_HELP_MENU);
    dis->setCursor(24, 115);
    dis->print(GOTCHI_HELP_UP);
    dis->setCursor(24, 142);
    dis->print(GOTCHI_HELP_BACK);

    dis->setFont(GOTCHI_FONT_TITLE);
    dis->setCursor(14, 184);
    dis->print(GOTCHI_HELP_CLOSE);
    dUChange = true;
}

static void copyGotchiFrame()
{
    std::lock_guard<std::mutex> lock(gotchiBuffMutex);
    if (gotchiBuff == nullptr)
    {
        return;
    }

    uint8_t *source = gotchiBuff->getBuffer();
    if (memcmp(source, dis->_buffer, GOTCHI_BUFFER_SIZE) != 0)
    {
        memcpy(dis->_buffer, source, GOTCHI_BUFFER_SIZE);
        dUChange = true;
    }
}

static void drawMissingRom()
{
    dis->fillScreen(SCWhite);
    dis->setTextColor(SCBlack);
    dis->setTextWrap(false);

    dis->setFont(GOTCHI_FONT_TITLE);
    dis->setCursor(8, 15);
    dis->print(GOTCHI_TITLE);
    dis->drawFastHLine(8, 23, 184, SCBlack);

    dis->setFont(GOTCHI_FONT_BODY);
    dis->setCursor(12, 61);
    dis->print(GOTCHI_ROM_MISSING);
    dis->setCursor(12, 88);
    dis->print(GOTCHI_ROM_COPY);
    dis->setCursor(12, 112);
    dis->print("/other/");
    dis->setCursor(12, 132);
    dis->print(GOTCHI_ROM_FILE);

    dis->setFont(GOTCHI_FONT_TITLE);
    dis->setCursor(8, 194);
    dis->print(GOTCHI_BACK_HINT);
    dUChange = true;
}

void initGotchi()
{
    gotchiReady = false;
    gotchiHelpVisible = false;
    gotchiButtons.left.store(0);
    gotchiButtons.right.store(0);
    gotchiButtons.middle.store(0);

    bufSize rom = fsGetBlob(GOTCHI_ROM_FILE, "/other/");
    if (rom.size != GOTCHI_ROM_SIZE)
    {
        debugLog("Gotchi ROM missing or invalid, size: " + String(rom.size));
        free(rom.buf);
        romData = nullptr;
        drawMissingRom();
        return;
    }

    romData = rom.buf;
    gotchiReady = startGotchiTask();
    if (!gotchiReady)
    {
        free(romData);
        romData = nullptr;
        drawMissingRom();
    }
}

void loopGotchi()
{
    if (!gotchiReady)
    {
        useButton();
        disUp();
        return;
    }

    buttonState button = useButton();
    if (button == LongMenu)
    {
        gotchiHelpVisible = !gotchiHelpVisible;
        if (gotchiHelpVisible)
        {
            drawHelp();
        }
        else
        {
            copyGotchiFrame();
        }
        resetSleepDelay();
        disUp();
        return;
    }

    if (gotchiHelpVisible)
    {
        resetSleepDelay();
        disUp();
        return;
    }

    copyGotchiFrame();

    switch (button)
    {
    case Up:
        gotchiButtons.right.fetch_add(GOTCHI_BUTTON_TICKS);
        break;
    case Down:
        gotchiButtons.left.fetch_add(GOTCHI_BUTTON_TICKS);
        break;
    case Menu:
        gotchiButtons.middle.fetch_add(GOTCHI_BUTTON_TICKS);
        break;
    default:
        break;
    }

    resetSleepDelay();
    disUp();
}

void exitGotchi()
{
    if (gotchiReady)
    {
        endGotchiTask();
    }
    gotchiReady = false;
    gotchiHelpVisible = false;
    free(romData);
    romData = nullptr;
}

#endif
