#include "defines.h"

#if CHESS

#include <stdint.h>
#include <string.h>

struct Move { uint8_t from,to,promo,flags; };

struct Position {
    uint8_t board[64];
    bool whiteToMove;
    uint8_t castling;
    uint8_t epSquare;
    uint16_t halfmove;
    uint16_t fullmove;
};

static inline int sqR(uint8_t s){ return s>>3; }
static inline int sqC(uint8_t s){ return s&7; }

// Fonts (fallback)
static const char* kPieceFontPath = "taychron/Mono13";
static const char* kHudFontPath   = "UbuntuMono10";

// HUD sizes
static const int kHeaderH = 14;
static const int kFooterH = 18;

// Cell draw padding so glyphs never bleed outside the square
static const int PAD = 2;

static int g_orgX = 6;
static int g_orgY = 18;
static int g_cell = 14;

static inline bool isBlackPiece(uint8_t pc){ return pc >= 9; }

// ---------- Last moves (ghost) ----------
static bool    g_aiMoveValid = false;
static uint8_t g_aiFrom = 0;
static uint8_t g_aiTo   = 0;

static bool    g_plMoveValid = false;
static uint8_t g_plFrom = 0;
static uint8_t g_plTo   = 0;

// ---------- Check state (blinking king border) ----------
static bool    g_whiteInCheck = false;
static bool    g_blackInCheck = false;
static uint8_t g_whiteKingSq  = 0;
static uint8_t g_blackKingSq  = 0;

void chessRenderSetLastMove(uint8_t fromSq, uint8_t toSq, bool valid) // AI move
{
    g_aiMoveValid = valid;
    g_aiFrom = fromSq;
    g_aiTo   = toSq;
}

void chessRenderSetLastPlayerMove(uint8_t fromSq, uint8_t toSq, bool valid)
{
    g_plMoveValid = valid;
    g_plFrom = fromSq;
    g_plTo   = toSq;
}

void chessRenderSetCheckState(uint8_t whiteKingSq, bool whiteCheck, uint8_t blackKingSq, bool blackCheck)
{
    g_whiteKingSq  = whiteKingSq;
    g_blackKingSq  = blackKingSq;
    g_whiteInCheck = whiteCheck;
    g_blackInCheck = blackCheck;
}

static inline void sqCenter(uint8_t sq, int& cx, int& cy)
{
    int r = sqR(sq), c = sqC(sq);
    int x = g_orgX + c * g_cell;
    int y = g_orgY + r * g_cell;
    cx = x + g_cell / 2;
    cy = y + g_cell / 2;
}

// Simple dashed line (player ghost = "fine grey" equivalent on 1-bit)
static void drawDashedLine(int x0,int y0,int x1,int y1, uint8_t on=2, uint8_t off=2)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0); // negative for classic Bresenham
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    uint8_t phase = 0;
    uint8_t run = on;
    bool draw = true;

    for (;;) {
        if (draw) dis->drawPixel(x0, y0, GxEPD_BLACK);

        if (x0 == x1 && y0 == y1) break;

        phase++;
        if (phase >= run) {
            phase = 0;
            draw = !draw;
            run = draw ? on : off;
        }

        int e2 = err << 1;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void drawGhostMoves()
{
    if (g_aiMoveValid) {
        int fx, fy, tx, ty;
        sqCenter(g_aiFrom, fx, fy);
        sqCenter(g_aiTo,   tx, ty);
        dis->drawLine(fx, fy, tx, ty, GxEPD_BLACK);
        dis->fillCircle(fx, fy, 1, GxEPD_BLACK);
        dis->fillCircle(tx, ty, 1, GxEPD_BLACK);
    }

    if (g_plMoveValid) {
        int fx, fy, tx, ty;
        sqCenter(g_plFrom, fx, fy);
        sqCenter(g_plTo,   tx, ty);
        drawDashedLine(fx, fy, tx, ty, 2, 2);
    }
}

static void computeLayout()
{
    int W = (int)dis->width();
    int H = (int)dis->height();

    int availH = H - kHeaderH - kFooterH;
    if (availH < 8 * 14) availH = 8 * 14;

    int cell = availH / 8;
    if (cell < 14) cell = 14;
    if (cell > 24) cell = 24;

    int boardPx = cell * 8;

    g_cell = cell;
    g_orgX = (W - boardPx) / 2;
    if (g_orgX < 0) g_orgX = 0;

    g_orgY = kHeaderH + (availH - boardPx) / 2;
    if (g_orgY < kHeaderH) g_orgY = kHeaderH;
}

static void setHudTextState()
{
    dis->setFont(getFont(kHudFontPath));
    dis->setTextSize(1);
    dis->setTextColor(GxEPD_BLACK, GxEPD_WHITE);
}

static void setPieceTextState()
{
    dis->setFont(getFont(kPieceFontPath));
    dis->setTextSize(1);
    dis->setTextColor(GxEPD_BLACK, GxEPD_WHITE);
}

static void drawHeader(const Position& p)
{
    dis->fillRect(0, 0, dis->width(), kHeaderH, GxEPD_WHITE);
    setHudTextState();
    dis->setCursor(0, 10);
    dis->print(p.whiteToMove ? "Turn: White" : "Turn: Black");
}

static void drawFooter(const char* msg)
{
    int H = (int)dis->height();
    dis->fillRect(0, H - kFooterH, dis->width(), kFooterH, GxEPD_WHITE);
    setHudTextState();
    dis->setCursor(0, H - 4);
    dis->print(msg);
}

static char pieceCharUpper(uint8_t pc)
{
    switch (pc){
        case 1:  return 'P';
        case 2:  return 'N';
        case 3:  return 'B';
        case 4:  return 'R';
        case 5:  return 'Q';
        case 6:  return 'K';
        case 9:  return 'P';
        case 10: return 'N';
        case 11: return 'B';
        case 12: return 'R';
        case 13: return 'Q';
        case 14: return 'K';
        default: return ' ';
    }
}

static bool isDestHighlighted(uint8_t sq, const Move* hl, int n)
{
    for (int i=0;i<n;i++) if (hl[i].to == sq) return true;
    return false;
}

static void drawPieceCenteredInBox(int x, int y, int wBox, int hBox, char ch)
{
    char s[2] = { ch, 0 };

    int16_t x1=0, y1=0;
    uint16_t w=0, h=0;
    dis->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);

    int cx = x + (wBox - (int)w) / 2 - (int)x1;
    int cy = y + (hBox - (int)h) / 2 - (int)y1;

    dis->setCursor(cx, cy);
    dis->print(s);
}

static void drawLegalDot(int x, int y, bool occupied)
{
    int cx = x + g_cell / 2;
    int cy = y + g_cell / 2;

    if (occupied){
        dis->drawCircle(cx, cy, 3, GxEPD_BLACK);   // capture ring
    } else {
        dis->fillCircle(cx, cy, 2, GxEPD_BLACK);   // empty move dot
    }
}

static void drawCheckBlinkBorder(int x, int y)
{
    bool on = ((millis() / 650) & 1) == 0;
    if (!on) return;

    dis->drawRect(x+0, y+0, g_cell,   g_cell,   GxEPD_BLACK);
    dis->drawRect(x+1, y+1, g_cell-2, g_cell-2, GxEPD_BLACK);
    dis->drawRect(x+2, y+2, g_cell-4, g_cell-4, GxEPD_BLACK);
}

// ---------------- ICONS (cached once) ----------------
// We must NOT call getImg 64x per frame; cache pointers once.
static bool g_iconsReady = false;
static ImageDef* g_pieceImg[15] = { nullptr };

static const char* iconNameFor(uint8_t pc)
{
    switch (pc){
        case 1:  return "chess/wp";
        case 2:  return "chess/wn";
        case 3:  return "chess/wb";
        case 4:  return "chess/wr";
        case 5:  return "chess/wq";
        case 6:  return "chess/wk";
        case 9:  return "chess/bp";
        case 10: return "chess/bn";
        case 11: return "chess/bb";
        case 12: return "chess/br";
        case 13: return "chess/bq";
        case 14: return "chess/bk";
        default: return nullptr;
    }
}

static void ensureIconsLoaded()
{
    if (g_iconsReady) return;
    g_iconsReady = true;

    // init all to empty
    for (int i=0;i<15;i++) g_pieceImg[i] = &emptyImgPack;

    // load only existing piece codes (12 total)
    const uint8_t pcs[] = { 1,2,3,4,5,6, 9,10,11,12,13,14 };
    for (unsigned i=0;i<sizeof(pcs); i++){
        uint8_t pc = pcs[i];
        const char* name = iconNameFor(pc);
        if (!name) continue;

        ImageDef* im = getImg(String(name));
        if (im && im->bitmap && im->bw > 0 && im->bh > 0) {
            g_pieceImg[pc] = im;
        }
    }
}

static bool drawPieceIconCentered(uint8_t pc, int x, int y)
{
    if (pc >= 15) return false;
    ImageDef* img = g_pieceImg[pc];
    if (!img || !img->bitmap || img->bw <= 0 || img->bh <= 0) return false;

    // For icons, use a smaller padding than text
    const int IPAD = 1; // <-- clave

    int xMin = x + IPAD;
    int yMin = y + IPAD;
    int wBox = g_cell - IPAD*2;
    int hBox = g_cell - IPAD*2;

    if (img->bw > wBox || img->bh > hBox) {
        if (g_cell < 18) dis->fillCircle(x + 2, y + 2, 1, GxEPD_BLACK);
        return false;
    }

    int dx = xMin + (wBox - img->bw) / 2;
    int dy = yMin + (hBox - img->bh) / 2;

    writeImageN(dx, dy, img, GxEPD_WHITE, GxEPD_BLACK);
    return true;
}

static void drawSquare(uint8_t sq, uint8_t pc, bool cursor, bool selected, bool highlight)
{
    int r = sqR(sq), c = sqC(sq);
    int x = g_orgX + c * g_cell;
    int y = g_orgY + r * g_cell;

    dis->fillRect(x, y, g_cell, g_cell, GxEPD_WHITE);
    dis->drawRect(x, y, g_cell, g_cell, GxEPD_BLACK);

    if (highlight){
        bool occupied = (pc != 0);
        drawLegalDot(x, y, occupied);
    }

    if (selected){
        dis->drawRect(x+1, y+1, g_cell-2, g_cell-2, GxEPD_BLACK);
        dis->drawRect(x+2, y+2, g_cell-4, g_cell-4, GxEPD_BLACK);
    }

    if (cursor){
        dis->drawRect(x, y, g_cell, g_cell, GxEPD_BLACK);
        dis->drawRect(x+1, y+1, g_cell-2, g_cell-2, GxEPD_BLACK);
    }

    if ((g_whiteInCheck && sq == g_whiteKingSq) || (g_blackInCheck && sq == g_blackKingSq)){
        drawCheckBlinkBorder(x, y);
    }

    if (pc != 0){
        // Prefer icons if available
        ensureIconsLoaded();
        if (drawPieceIconCentered(pc, x, y)){
            return;
        }

        // Fallback: letters + underline blacks
        char ch = pieceCharUpper(pc);
        if (ch != ' '){
            setPieceTextState();

            int ix = x + PAD;
            int iy = y + PAD;
            int iw = g_cell - PAD*2;
            int ih = g_cell - PAD*2;

            dis->setTextColor(GxEPD_BLACK, GxEPD_WHITE);
            drawPieceCenteredInBox(ix, iy, iw, ih, ch);

            if (isBlackPiece(pc)){
                int barY = y + g_cell - 3;
                int barX1 = x + 3;
                int barX2 = x + g_cell - 3;
                dis->drawLine(barX1, barY,   barX2, barY,   GxEPD_BLACK);
                dis->drawLine(barX1, barY-1, barX2, barY-1, GxEPD_BLACK);
            }
        }
    }
}

void chessRenderFull(const Position& p, uint8_t cursor, int8_t selected, const Move* highlights, int hlCount, const char* msg)
{
    computeLayout();

    dis->fillScreen(GxEPD_WHITE);
    drawHeader(p);

    for (uint8_t sq=0;sq<64;sq++){
        bool hl = isDestHighlighted(sq, highlights, hlCount);
        drawSquare(sq, p.board[sq], sq==cursor, (selected>=0 && sq==(uint8_t)selected), hl);
    }

    drawGhostMoves();
    drawFooter(msg);
}

void chessRenderDirty(const Position& p, uint64_t dirtyMask,
                      uint8_t cursorOld, uint8_t cursorNew,
                      int8_t selectedOld, int8_t selectedNew,
                      const Move* hlOld, int hlOldCount,
                      const Move* hlNew, int hlNewCount,
                      const char* msg, bool msgDirty, bool hudDirty)
{
    computeLayout();

    if (hudDirty){
        drawHeader(p);
    }

    uint64_t mask = dirtyMask;
    mask |= (1ULL << cursorOld) | (1ULL << cursorNew);
    if (selectedOld >= 0) mask |= (1ULL << (uint8_t)selectedOld);
    if (selectedNew >= 0) mask |= (1ULL << (uint8_t)selectedNew);
    for (int i=0;i<hlOldCount;i++) mask |= (1ULL << hlOld[i].to);
    for (int i=0;i<hlNewCount;i++) mask |= (1ULL << hlNew[i].to);

    if (g_whiteInCheck) mask |= (1ULL << g_whiteKingSq);
    if (g_blackInCheck) mask |= (1ULL << g_blackKingSq);

    for (uint8_t sq=0;sq<64;sq++){
        if ((mask >> sq) & 1ULL){
            bool hl = isDestHighlighted(sq, hlNew, hlNewCount);
            drawSquare(sq, p.board[sq], sq==cursorNew, (selectedNew>=0 && sq==(uint8_t)selectedNew), hl);
        }
    }

    drawGhostMoves();

    if (msgDirty){
        drawFooter(msg);
    }
}

#endif
