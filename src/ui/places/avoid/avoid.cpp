#include "avoid.h"

#if AVOID
#include "rtcMem.h"

#define AVOID_TRUE_MAX_LINES 100     // Hard maximum amount of lines, the array is allocated for this
#define AVOID_START_LINES 20         // Amount of lines at the start of the game
#define AVOID_DOUBLE_SPAWN_CHANCE 60 // % chance that 2 lines spawn when one disappears
#define AVOID_LINE_LEN_MIN 10        // Min line length in px
#define AVOID_LINE_LEN_MAX 70        // Max line length in px
#define AVOID_LINE_SPEED_MIN 3       // Min line speed in px per loop
#define AVOID_LINE_SPEED_MAX 15      // Max line speed in px per loop
#define AVOID_PLAYER_SIZE 6          // Player square size in px
#define AVOID_ACC_DIVISOR 4.0f       // Divisor for accelerometer degrees, lower = faster
#define AVOID_GO_TEXT_Y_TOP 20       // Baseline of the first game over line when shown in the top part
#define AVOID_GO_TEXT_Y_BOTTOM 150   // Baseline of the first game over line when shown in the bottom part

struct avoidLine
{
    float x; // Start point of the line
    float y;
    float dx; // Normalized direction
    float dy;
    int len;
    int speed;
    bool entered; // Has the line been on the screen at least once
};

avoidLine *avoidLines = nullptr;
int avoidActiveLines = 0;
int avoidPlayerX = 0;
int avoidPlayerY = 0;
bool avoidGameOver = false;
int64_t avoidStartMs = 0;
int64_t avoidFinishMs = 0;

void avoidSpawnLine(avoidLine *l)
{
    l->len = betterRandom(AVOID_LINE_LEN_MIN, AVOID_LINE_LEN_MAX + 1);
    l->speed = betterRandom(AVOID_LINE_SPEED_MIN, AVOID_LINE_SPEED_MAX + 1);

    int edge = betterRandom(0, 4);
    float angleDeg;

    // The start point is pushed out by the line length, so the whole line spawns outside the screen
    if (edge == 0) // Top
    {
        l->x = betterRandom(0, dis->width());
        l->y = 0 - l->len;
        angleDeg = betterRandom(0, 180);
    }
    else if (edge == 1) // Bottom
    {
        l->x = betterRandom(0, dis->width());
        l->y = dis->height() + l->len;
        angleDeg = betterRandom(180, 360);
    }
    else if (edge == 2) // Left
    {
        l->x = 0 - l->len;
        l->y = betterRandom(0, dis->height());
        angleDeg = betterRandom(270, 450);
    }
    else // Right
    {
        l->x = dis->width() + l->len;
        l->y = betterRandom(0, dis->height());
        angleDeg = betterRandom(90, 270);
    }

    float rad = angleDeg * (PI / 180.0f);
    l->dx = cos(rad);
    l->dy = sin(rad);
    l->entered = false;
}

void avoidAddLine()
{
    if (avoidActiveLines >= AVOID_TRUE_MAX_LINES)
    {
        return;
    }
    avoidSpawnLine(&avoidLines[avoidActiveLines]);
    avoidActiveLines++;
}

bool avoidLineOffScreen(avoidLine *l)
{
    float ex = l->x + l->dx * l->len;
    float ey = l->y + l->dy * l->len;
    float minX = min(l->x, ex);
    float maxX = max(l->x, ex);
    float minY = min(l->y, ey);
    float maxY = max(l->y, ey);

    return (maxX < 0 || minX > dis->width() || maxY < 0 || minY > dis->height());
}

bool avoidLineHitsPlayer(avoidLine *l)
{
    float px = avoidPlayerX + AVOID_PLAYER_SIZE / 2.0f;
    float py = avoidPlayerY + AVOID_PLAYER_SIZE / 2.0f;

    float ax = l->x;
    float ay = l->y;
    float bx = l->x + l->dx * l->len;
    float by = l->y + l->dy * l->len;

    float abx = bx - ax;
    float aby = by - ay;
    float apx = px - ax;
    float apy = py - ay;

    float t = (apx * abx + apy * aby) / (abx * abx + aby * aby);
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;

    float cx = ax + t * abx;
    float cy = ay + t * aby;
    float ddx = px - cx;
    float ddy = py - cy;
    float dist = sqrt(ddx * ddx + ddy * ddy);

    return (dist < (AVOID_PLAYER_SIZE / 2.0f) + 1.0f);
}

void avoidDraw()
{
    dis->fillScreen(SCWhite);

    for (int i = 0; i < avoidActiveLines; i++)
    {
        avoidLine *l = &avoidLines[i];
        dis->drawLine((int)l->x, (int)l->y, (int)(l->x + l->dx * l->len), (int)(l->y + l->dy * l->len), SCBlack);
    }

    dis->fillRect(avoidPlayerX, avoidPlayerY, AVOID_PLAYER_SIZE, AVOID_PLAYER_SIZE, SCBlack);
}

void avoidDrawGameOver()
{
    setFont(&FreeSansBold9pt7b);
    setTextSize(1);

    uint32_t sec = (avoidFinishMs - avoidStartMs) / 1000;
    String timeStr = addZero(String(sec / 60), 2) + ":" + addZero(String(sec % 60), 2);

    // If the cube is in the upper half, show the text in the lower half and vice versa
    int16_t y = (avoidPlayerY < dis->height() / 2) ? AVOID_GO_TEXT_Y_BOTTOM : AVOID_GO_TEXT_Y_TOP;

    writeTextCenterReplaceBack("Game over", y);
    writeTextCenterReplaceBack("Time: " + timeStr, y + 18);
    writeTextCenterReplaceBack("Lines: " + String(avoidActiveLines), y + 36);
}

void initAvoid()
{
    if (avoidLines == nullptr)
    {
        avoidLines = (avoidLine *)malloc(AVOID_TRUE_MAX_LINES * sizeof(avoidLine));
    }

    avoidPlayerX = (dis->width() - AVOID_PLAYER_SIZE) / 2;
    avoidPlayerY = (dis->height() - AVOID_PLAYER_SIZE) / 2;
    avoidGameOver = false;
    avoidActiveLines = 0;
    avoidStartMs = millisBetter();
    avoidFinishMs = 0;

    for (int i = 0; i < AVOID_START_LINES; i++)
    {
        avoidAddLine();
    }

    initAcc();
    avoidDraw();
    disUp(true);
}

void loopAvoid()
{
    buttonState btn = useButton();

    if (avoidGameOver == true)
    {
        if (btn != None)
        {
            initAvoid();
        }
        disUp();
        return;
    }

    {
        Accel acc;
        if (rM.SBMA.getAccel(&acc))
        {
            float degX = getAxisDegrees(acc.x, acc.y, acc.z);
            float degY = getAxisDegrees(acc.y, acc.x, acc.z);

            avoidPlayerX += (int)(degX / AVOID_ACC_DIVISOR);
            avoidPlayerY += (int)(degY / AVOID_ACC_DIVISOR);

            if (avoidPlayerX < 0)
                avoidPlayerX = 0;
            if (avoidPlayerY < 0)
                avoidPlayerY = 0;
            if (avoidPlayerX > dis->width() - AVOID_PLAYER_SIZE)
                avoidPlayerX = dis->width() - AVOID_PLAYER_SIZE;
            if (avoidPlayerY > dis->height() - AVOID_PLAYER_SIZE)
                avoidPlayerY = dis->height() - AVOID_PLAYER_SIZE;
        }

        for (int i = 0; i < avoidActiveLines; i++)
        {
            avoidLine *l = &avoidLines[i];
            l->x += l->dx * l->speed;
            l->y += l->dy * l->speed;

            if (avoidLineOffScreen(l) == true)
            {
                if (l->entered == false)
                {
                    // The line went out without ever entering the screen, just replace it
                    avoidSpawnLine(l);
                    continue;
                }

                // Remove the line by swapping it with the last one
                avoidActiveLines--;
                avoidLines[i] = avoidLines[avoidActiveLines];

                // 60% chance that 2 lines appear instead of 1
                if (betterRandom(0, 100) < AVOID_DOUBLE_SPAWN_CHANCE)
                {
                    avoidAddLine();
                    avoidAddLine();
                }
                else
                {
                    avoidAddLine();
                }

                // The swapped-in line hasn't been processed yet this frame
                i--;
                continue;
            }

            l->entered = true;

            if (avoidLineHitsPlayer(l) == true)
            {
                avoidGameOver = true;
                avoidFinishMs = millisBetter();
                avoidDraw(); // Draw the final frame with the collision
                avoidDrawGameOver();
                disUp(true);
                resetSleepDelay();
                return;
            }
        }

        avoidDraw();
    }

    dUChange = true;
    resetSleepDelay();
    disUp();
}

void exitAvoid()
{
    if (avoidLines != nullptr)
    {
        free(avoidLines);
        avoidLines = nullptr;
    }
}

#endif
