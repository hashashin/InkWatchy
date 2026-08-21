#include "game2048.h"

#if GAME_2048
#include "rtcMem.h"

#define G2048_ROWS 4
#define G2048_COLS 4
#define G2048_CELL_SIZE 50
#define G2048_LINE_SIZE 2

#define G2048_FONT getFont("game2048/UbuntuMono25")
#define G2048_FONT_SMALL getFont("game2048/UbuntuMono12")

#define G2048_TILT_MOVE_DEG 25
#define G2048_TILT_RESET_DEG 12

#define G2048_TEXT_Y_1 95
#define G2048_TEXT_Y_2 130

#define G2048_DOT_RADIUS 2
#define G2048_DOT_HALO_RADIUS 4

enum g2048Dir
{
    G2048_LEFT = 0,
    G2048_RIGHT,
    G2048_UP,
    G2048_DOWN
};

enum g2048State
{
    G2048_PLAYING,
    G2048_WON,
    G2048_GAME_OVER
};

static uint16_t *g2048Grid = nullptr;
static g2048State g2048State = G2048_PLAYING;
static bool g2048TiltReady = true;

void g2048CellOfLine(int dir, int line, int pos, int *col, int *row)
{
    switch (dir)
    {
    case G2048_LEFT:
        *col = pos;
        *row = line;
        break;
    case G2048_RIGHT:
        *col = G2048_COLS - 1 - pos;
        *row = line;
        break;
    case G2048_UP:
        *col = line;
        *row = pos;
        break;
    default: // G2048_DOWN
        *col = line;
        *row = G2048_ROWS - 1 - pos;
        break;
    }
}

void g2048SlideLine(uint16_t line[G2048_COLS])
{
    uint16_t out[G2048_COLS] = {0};
    int w = 0;
    bool canMerge = true;
    for (int i = 0; i < G2048_COLS; i++)
    {
        if (line[i] == 0)
        {
            continue;
        }
        if (w > 0 && canMerge && out[w - 1] == line[i])
        {
            out[w - 1] = line[i] * 2;
            canMerge = false;
        }
        else
        {
            out[w++] = line[i];
            canMerge = true;
        }
    }
    for (int i = 0; i < G2048_COLS; i++)
    {
        line[i] = out[i];
    }
}

bool g2048Move(int dir)
{
    uint16_t before[G2048_ROWS * G2048_COLS];
    memcpy(before, g2048Grid, sizeof(before));

    for (int line = 0; line < G2048_ROWS; line++)
    {
        uint16_t cells[G2048_COLS];
        for (int i = 0; i < G2048_COLS; i++)
        {
            int col, row;
            g2048CellOfLine(dir, line, i, &col, &row);
            cells[i] = g2048Grid[row * G2048_COLS + col];
        }
        g2048SlideLine(cells);
        for (int i = 0; i < G2048_COLS; i++)
        {
            int col, row;
            g2048CellOfLine(dir, line, i, &col, &row);
            g2048Grid[row * G2048_COLS + col] = cells[i];
        }
    }

    return memcmp(before, g2048Grid, sizeof(before)) != 0;
}

bool g2048CanMove()
{
    for (int i = 0; i < G2048_ROWS * G2048_COLS; i++)
    {
        if (g2048Grid[i] == 0)
        {
            return true;
        }
        int col = i % G2048_COLS;
        int row = i / G2048_COLS;
        if (col < G2048_COLS - 1 && g2048Grid[i] == g2048Grid[i + 1])
        {
            return true;
        }
        if (row < G2048_ROWS - 1 && g2048Grid[i] == g2048Grid[i + G2048_COLS])
        {
            return true;
        }
    }
    return false;
}

void g2048SpawnTile()
{
    int empties[G2048_ROWS * G2048_COLS];
    int count = 0;
    for (int i = 0; i < G2048_ROWS * G2048_COLS; i++)
    {
        if (g2048Grid[i] == 0)
        {
            empties[count++] = i;
        }
    }
    if (count == 0)
    {
        return;
    }
    g2048Grid[empties[betterRandom(0, count)]] = (betterRandom(0, 10) < 9) ? 2 : 4;
}

void g2048DrawNumber(uint16_t value, int cellX, int cellY)
{
    setFont(value >= 100 ? G2048_FONT_SMALL : G2048_FONT);
    String s = String(value);
    int16_t x, y;
    uint16_t w, h;
    getTextBounds(s, &x, &y, &w, &h, 0, 0);
    dis->setCursor(cellX + (G2048_CELL_SIZE - w) / 2 - x, cellY + (G2048_CELL_SIZE - h) / 2 - y);
    dis->print(s);
}

void g2048DrawBoard()
{
    dis->fillScreen(SCWhite);
    dis->setTextColor(SCBlack);
    dis->setTextWrap(false);

    for (int i = 1; i < G2048_ROWS; i++)
    {
        dis->fillRect(i * G2048_CELL_SIZE - 1, 0, G2048_LINE_SIZE, dis->height(), SCBlack);
        dis->fillRect(0, i * G2048_CELL_SIZE - 1, dis->width(), G2048_LINE_SIZE, SCBlack);
    }

    for (int row = 0; row < G2048_ROWS; row++)
    {
        for (int col = 0; col < G2048_COLS; col++)
        {
            uint16_t value = g2048Grid[row * G2048_COLS + col];
            if (value != 0)
            {
                g2048DrawNumber(value, col * G2048_CELL_SIZE, row * G2048_CELL_SIZE);
            }
        }
    }
}

void g2048DrawEndScreen(String line1, String line2)
{
    setFont(G2048_FONT_SMALL);
    writeTextCenterReplaceBack(line1, G2048_TEXT_Y_1);
    writeTextCenterReplaceBack(line2, G2048_TEXT_Y_2);
}

void g2048DrawDot()
{
    dis->fillCircle(dis->width() / 2, dis->height() / 2, G2048_DOT_HALO_RADIUS, SCWhite);
    dis->fillCircle(dis->width() / 2, dis->height() / 2, G2048_DOT_RADIUS, SCBlack);
}

void g2048AfterMove()
{
    g2048SpawnTile();
    g2048DrawBoard();

    for (int i = 0; i < G2048_ROWS * G2048_COLS; i++)
    {
        if (g2048Grid[i] == 2048)
        {
            g2048State = G2048_WON;
            g2048DrawEndScreen("You win!", "Tap to restart");
            return;
        }
    }

    if (g2048CanMove() == false)
    {
        g2048State = G2048_GAME_OVER;
        g2048DrawEndScreen("Game over", "Tap to restart");
    }
}

void initGame2048()
{
    if (g2048Grid == nullptr)
    {
        g2048Grid = (uint16_t *)malloc(G2048_ROWS * G2048_COLS * sizeof(uint16_t));
    }
    setFont(G2048_FONT);
    setTextSize(1);
    memset(g2048Grid, 0, G2048_ROWS * G2048_COLS * sizeof(uint16_t));
    g2048State = G2048_PLAYING;
    g2048TiltReady = true;
    g2048SpawnTile();
    g2048SpawnTile();
    g2048DrawBoard();
    g2048DrawDot();
    initAcc();
    disUp(true);
}

void loopGame2048()
{
    buttonState btn = useButton();

    if (g2048State != G2048_PLAYING)
    {
        if (btn != None)
        {
            initGame2048();
        }
        disUp();
        return;
    }

    if (btn == LongMenu)
    {
        initGame2048();
        return;
    }

    Accel acc;
    if (rM.SBMA.getAccel(&acc) == true)
    {
        float degX = getAxisDegrees(acc.x, acc.y, acc.z);
        float degY = getAxisDegrees(acc.y, acc.x, acc.z);

        if (g2048TiltReady == true)
        {
            int dir = -1;
            if (fabsf(degX) >= G2048_TILT_MOVE_DEG && fabsf(degX) >= fabsf(degY))
            {
                dir = (degX > 0) ? G2048_RIGHT : G2048_LEFT;
            }
            else if (fabsf(degY) >= G2048_TILT_MOVE_DEG)
            {
                dir = (degY > 0) ? G2048_DOWN : G2048_UP;
            }

            if (dir != -1)
            {
                g2048TiltReady = false;
                resetSleepDelay(SLEEP_EVERY_MS * 4);
                if (g2048Move(dir) == true)
                {
                    g2048AfterMove();
                    dUChange = true;
                }
            }
        }
        else if (fabsf(degX) < G2048_TILT_RESET_DEG && fabsf(degY) < G2048_TILT_RESET_DEG)
        {
            g2048TiltReady = true;
            g2048DrawDot();
            dUChange = true;
        }
    }

    disUp();
}

void exitGame2048()
{
    free(g2048Grid);
    g2048Grid = nullptr;
}

#endif
