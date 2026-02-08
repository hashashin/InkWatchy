#include "chessApp.h"

#if CHESS

#include <string.h>

// Xtensa toolchain defines BR as a special register macro (specreg.h)
// It conflicts with our Black Rook name.
#ifdef BR
#undef BR
#endif

extern "C" unsigned long millis(void);

// ============================================================
// Minimal chess types shared across modules
// ============================================================
struct Move {
    uint8_t from;
    uint8_t to;
    uint8_t promo;   // 0 none else piece code
    uint8_t flags;   // MF_*
};

enum : uint8_t {
    MF_NONE   = 0,
    MF_EP     = 1 << 0,
    MF_CASTLE = 1 << 1,
    MF_PROMO  = 1 << 2
};

enum Piece : uint8_t {
    EMPTY=0,
    WP=1, WN=2, WB=3, WR=4, WQ=5, WK=6,
    BP=9, BN=10, BB=11, BR=12, BQ=13, BK=14
};

static inline bool isWhite(uint8_t p){ return p>=WP && p<=WK; }

struct Position {
    uint8_t board[64];
    bool whiteToMove;
    uint8_t castling;   // 1 WK,2 WQ,4 BK,8 BQ
    uint8_t epSquare;   // 0..63 or 255
    uint16_t halfmove;
    uint16_t fullmove;
};

// from chessRules.cpp
void chessInitStartPos(Position& p);
int  chessGenLegalMoves(const Position& p, Move* out, int cap);
void chessMakeMove(Position& p, const Move& m, uint8_t& captured, uint8_t& oldCastling, uint8_t& oldEp, uint16_t& oldHalfmove);
bool chessIsGameOver(const Position& p, bool& isMate, bool& isStalemate);
bool chessInCheck(const Position& p, bool whiteKing);

// from chessAi.cpp
Move chessAiBestMove(Position& p, int maxDepth, int nodeBudget);

// from chessRender.cpp
void chessRenderFull(const Position& p, uint8_t cursor, int8_t selected, const Move* highlights, int hlCount, const char* msg);
void chessRenderDirty(const Position& p, uint64_t dirtyMask,
                      uint8_t cursorOld, uint8_t cursorNew,
                      int8_t selectedOld, int8_t selectedNew,
                      const Move* hlOld, int hlOldCount,
                      const Move* hlNew, int hlNewCount,
                      const char* msg, bool msgDirty, bool hudDirty);

void chessRenderSetLastMove(uint8_t fromSq, uint8_t toSq, bool valid);          // AI
void chessRenderSetLastPlayerMove(uint8_t fromSq, uint8_t toSq, bool valid);    // Player
void chessRenderSetCheckState(uint8_t wKingSq, bool wCheck, uint8_t bKingSq, bool bCheck);

// ============================================================
// App state
// ============================================================
static Position g_pos{};
static uint8_t g_cursor = 0;         // 0..63
static int8_t  g_selected = -1;      // -1 none else square
static Move    g_hl[64];
static int     g_hlCount = 0;

static char    g_msg[48] = "MENU: select/move  BACK: cancel";
static bool    g_msgDirty = true;
static bool    g_hudDirty = true;

static uint64_t g_dirtySquares = 0;
static bool g_needFull = true;

// check blinking scheduler (low frequency)
static uint32_t g_nextCheckBlinkMs = 0;

// helpers
static inline uint8_t rcToSq(int r,int c){ return (uint8_t)(r*8+c); }
static inline int sqR(uint8_t s){ return s>>3; }
static inline int sqC(uint8_t s){ return s&7; }
static inline void markDirty(uint8_t sq){ g_dirtySquares |= (1ULL << sq); }
static inline void markAllDirty(){ g_dirtySquares = ~0ULL; }

static uint8_t findPieceSq(uint8_t pieceCode)
{
    for (uint8_t i=0;i<64;i++){
        if (g_pos.board[i] == pieceCode) return i;
    }
    return 0;
}

static void updateCheckState()
{
    bool wCheck = chessInCheck(g_pos, true);
    bool bCheck = chessInCheck(g_pos, false);
    uint8_t wK = findPieceSq(WK);
    uint8_t bK = findPieceSq(BK);
    chessRenderSetCheckState(wK, wCheck, bK, bCheck);
}

static void setMsg(const char* s){
    strncpy(g_msg, s, sizeof(g_msg)-1);
    g_msg[sizeof(g_msg)-1] = 0;
    g_msgDirty = true;
}

static void recomputeHighlights()
{
    g_hlCount = 0;
    if (g_selected < 0) return;

    Move moves[256];
    int n = chessGenLegalMoves(g_pos, moves, 256);
    for (int i=0;i<n && g_hlCount<64;i++){
        if (moves[i].from == (uint8_t)g_selected) g_hl[g_hlCount++] = moves[i];
    }
}

static bool isHighlightedDest(uint8_t sq, Move& outMove)
{
    for (int i=0;i<g_hlCount;i++){
        if (g_hl[i].to == sq) { outMove = g_hl[i]; return true; }
    }
    return false;
}

static void requestFullRedraw()
{
    g_needFull = true;
    markAllDirty();
    g_msgDirty = true;
    g_hudDirty = true;
}

static void resetGame()
{
    chessInitStartPos(g_pos);
    g_cursor = rcToSq(7,4);
    g_selected = -1;
    g_hlCount = 0;
    setMsg("New game. You are White.");
    g_hudDirty = true;

    chessRenderSetLastMove(0, 0, false);
    chessRenderSetLastPlayerMove(0, 0, false);
    updateCheckState();

    requestFullRedraw();
}

static void markSpecialMoveDirty(const Move& m, uint8_t movedPc)
{
    // En passant: captured pawn is not on "to"
    if (m.flags & MF_EP) {
        int dir = (movedPc == WP) ? 1 : -1;
        uint8_t capSq = (uint8_t)(m.to + dir * 8);
        markDirty(capSq);
    }

    // Castling: rook also moves
    if (m.flags & MF_CASTLE) {
        if (movedPc == WK) {
            if (m.to == rcToSq(7,6)) { // O-O
                markDirty(rcToSq(7,7));
                markDirty(rcToSq(7,5));
            } else if (m.to == rcToSq(7,2)) { // O-O-O
                markDirty(rcToSq(7,0));
                markDirty(rcToSq(7,3));
            }
        } else if (movedPc == BK) {
            if (m.to == rcToSq(0,6)) {
                markDirty(rcToSq(0,7));
                markDirty(rcToSq(0,5));
            } else if (m.to == rcToSq(0,2)) {
                markDirty(rcToSq(0,0));
                markDirty(rcToSq(0,3));
            }
        }
    }
}

static void applyPlayerMenuPress()
{
    uint8_t sq = g_cursor;
    uint8_t p = g_pos.board[sq];

    if (g_selected < 0){
        if (!g_pos.whiteToMove){
            setMsg("Wait: engine turn.");
            return;
        }
        if (p == EMPTY || !isWhite(p)){
            setMsg("Select a white piece.");
            return;
        }

        g_selected = (int8_t)sq;
        recomputeHighlights();
        g_hudDirty = true;
        markDirty((uint8_t)g_selected);
        setMsg("Choose destination.");
        return;
    }

    Move m{};
    if (!isHighlightedDest(sq, m)){
        if (p != EMPTY && isWhite(p)){
            int8_t oldSel = g_selected;

            g_selected = (int8_t)sq;
            recomputeHighlights();

            markDirty((uint8_t)oldSel);
            markDirty((uint8_t)g_selected);
            g_hudDirty = true;
            setMsg("Re-selected. Choose destination.");
        } else {
            setMsg("Illegal.");
        }
        return;
    }

    // Promotion: always queen
    if (m.flags & MF_PROMO){
        m.promo = WQ;
    }

    uint8_t movedPc = g_pos.board[m.from];

    uint8_t captured=0, oldCast=0, oldEp=0; uint16_t oldHalf=0;
    chessMakeMove(g_pos, m, captured, oldCast, oldEp, oldHalf);

    // remember player last move (ghost dashed)
    chessRenderSetLastPlayerMove(m.from, m.to, true);

    markDirty(m.from);
    markDirty(m.to);
    markSpecialMoveDirty(m, movedPc);

    g_selected = -1;
    g_hlCount = 0;
    g_hudDirty = true;

    updateCheckState();

    bool mate=false, stalemate=false;
    if (chessIsGameOver(g_pos, mate, stalemate)){
        if (mate) setMsg("Checkmate! You win.");
        else setMsg("Draw.");
        requestFullRedraw();
        return;
    }

    setMsg("Thinking...");
    requestFullRedraw();
}

static void doEngineTurnIfNeeded()
{
    if (g_pos.whiteToMove) return;

    const int maxDepth = 3;
    const int nodeBudget = 20000;

    Move best = chessAiBestMove(g_pos, maxDepth, nodeBudget);

    if (best.from == best.to){
        bool mate=false, stalemate=false;
        chessIsGameOver(g_pos, mate, stalemate);
        if (mate) setMsg("Checkmate! You lose.");
        else setMsg("Draw.");
        requestFullRedraw();
        return;
    }

    uint8_t movedPc = g_pos.board[best.from];

    uint8_t captured=0, oldCast=0, oldEp=0; uint16_t oldHalf=0;
    chessMakeMove(g_pos, best, captured, oldCast, oldEp, oldHalf);

    // remember AI last move (ghost solid)
    chessRenderSetLastMove(best.from, best.to, true);

    markDirty(best.from);
    markDirty(best.to);
    markSpecialMoveDirty(best, movedPc);

    g_hudDirty = true;

    updateCheckState();

    bool mate=false, stalemate=false;
    if (chessIsGameOver(g_pos, mate, stalemate)){
        if (mate) setMsg("Checkmate! You lose.");
        else setMsg("Draw.");
    } else {
        setMsg("Your move.");
    }

    requestFullRedraw();
}

static void moveCursor(int dr, int dc)
{
    int r = sqR(g_cursor);
    int c = sqC(g_cursor);
    int nr = (r + dr + 8) & 7;
    int nc = (c + dc + 8) & 7;
    uint8_t old = g_cursor;
    g_cursor = rcToSq(nr,nc);
    markDirty(old);
    markDirty(g_cursor);
}

void initChessApp()
{
    resetGame();
    useAllButtons();
}

void exitChessApp()
{
    // no persistence by design
}

void loopChessApp()
{
    buttonState b = useButton();

    // IMPORTANT: only reset sleep delay on input
    resetSleepDelay(SLEEP_EVERY_MS);

    uint8_t oldCursor = g_cursor;
    int8_t oldSel = g_selected;

    Move hlOld[64]; int hlOldCount = g_hlCount;
    if (hlOldCount) memcpy(hlOld, g_hl, sizeof(Move)*hlOldCount);

    bool wantEngine = false;

    if (b != buttonState::None && b != buttonState::Unknown){

        // Vertical
        if (b == buttonState::Up)   moveCursor(-1, 0);
        if (b == buttonState::Down) moveCursor(+1, 0);

        // Horizontal (long press)
        if (b == buttonState::LongUp)   moveCursor(0, -1);
        if (b == buttonState::LongDown) moveCursor(0, +1);

        if (b == buttonState::Menu){
            applyPlayerMenuPress();
            wantEngine = (!g_pos.whiteToMove);
        }

        if (b == buttonState::Back){
            if (g_selected >= 0){
                int8_t s = g_selected;
                g_selected = -1;
                g_hlCount = 0;
                markDirty((uint8_t)s);
                g_hudDirty = true;
                setMsg("Cancelled.");
                requestFullRedraw();
            } else {
                generalSwitch(gamesMenu);
                return;
            }
        }

        // Long MENU = new game
        if (b == buttonState::LongMenu){
            resetGame();
            return;
        }
    }

    if (wantEngine){
        doEngineTurnIfNeeded();
    }

    // Blink scheduling: if any side is in check, repaint just that king square occasionally
    // to make the border blink without spamming updates.
    {
        bool wCheck = chessInCheck(g_pos, true);
        bool bCheck = chessInCheck(g_pos, false);

        if (wCheck || bCheck){
            uint32_t now = (uint32_t)millis();
            if (now >= g_nextCheckBlinkMs){
                g_nextCheckBlinkMs = now + 900; // low frequency blink
                uint8_t wK = findPieceSq(WK);
                uint8_t bK = findPieceSq(BK);
                if (wCheck) markDirty(wK);
                if (bCheck) markDirty(bK);
                // ensure renderer knows current check state
                chessRenderSetCheckState(wK, wCheck, bK, bCheck);
            }
        } else {
            g_nextCheckBlinkMs = 0;
        }
    }

    if (g_needFull){
        chessRenderFull(g_pos, g_cursor, g_selected, g_hl, g_hlCount, g_msg);
        g_needFull = false;
        g_dirtySquares = 0;
        g_msgDirty = false;
        g_hudDirty = false;

        dUChange = true;
        disUp(true, true, true);
        return;
    }

    if (g_dirtySquares || g_msgDirty || g_hudDirty || oldCursor != g_cursor || oldSel != g_selected || hlOldCount != g_hlCount){
        chessRenderDirty(g_pos, g_dirtySquares, oldCursor, g_cursor, oldSel, g_selected,
                         hlOld, hlOldCount, g_hl, g_hlCount, g_msg, g_msgDirty, g_hudDirty);

        g_dirtySquares = 0;
        g_msgDirty = false;
        g_hudDirty = false;

        dUChange = true;
        disUp(true, true, true);
    }
}

#endif
