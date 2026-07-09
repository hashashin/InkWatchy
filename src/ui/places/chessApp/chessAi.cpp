#include "defines.h"

#if CHESS

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>

struct Move { uint8_t from,to,promo,flags; };
enum : uint8_t { MF_NONE=0, MF_EP=1<<0, MF_CASTLE=1<<1, MF_PROMO=1<<2 };

#ifdef BR
#undef BR
#endif
enum Piece : uint8_t {
    EMPTY=0, WP=1, WN=2, WB=3, WR=4, WQ=5, WK=6,
    BP=9, BN=10, BB=11, BR=12, BQ=13, BK=14
};

static inline bool isWhite(uint8_t p){ return p>=WP && p<=WK; }

struct Position {
    uint8_t board[64];
    bool whiteToMove;
    uint8_t castling;
    uint8_t epSquare;
    uint16_t halfmove;
    uint16_t fullmove;
};

// rules
int  chessGenLegalMoves(const Position& p, Move* out, int cap);
void chessMakeMove(Position& p, const Move& m, uint8_t& captured, uint8_t& oldCastling, uint8_t& oldEp, uint16_t& oldHalfmove);
void chessUnmakeMove(Position& p, const Move& m, uint8_t captured, uint8_t oldCastling, uint8_t oldEp, uint16_t oldHalfmove);
bool chessInCheck(const Position& p, bool whiteKing);

// ---------------- Eval ----------------
static inline int pieceValue(uint8_t pc){
    switch (pc){
        case WP: return 100; case WN: return 320; case WB: return 330; case WR: return 500; case WQ: return 900; case WK: return 0;
        case BP: return -100; case BN: return -320; case BB: return -330; case BR: return -500; case BQ: return -900; case BK: return 0;
        default: return 0;
    }
}

static inline int sqR(uint8_t s){ return s>>3; }
static inline int sqC(uint8_t s){ return s&7; }
static inline uint8_t mirrorSq(uint8_t s){ return (uint8_t)(((7 - sqR(s))<<3) | sqC(s)); }

static const int16_t PST_P[64] = {
     0,0,0,0,0,0,0,0,
    10,10,10,10,10,10,10,10,
     2,2,4,6,6,4,2,2,
     1,1,2,5,5,2,1,1,
     0,0,0,4,4,0,0,0,
     1,-1,-2,0,0,-2,-1,1,
     1,2,2,-4,-4,2,2,1,
     0,0,0,0,0,0,0,0
};
static const int16_t PST_N[64] = {
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,0,0,0,0,0,0,-10,
   -10,0,5,6,6,5,0,-10,
   -10,2,6,8,8,6,2,-10,
   -10,0,6,8,8,6,0,-10,
   -10,2,5,6,6,5,2,-10,
   -10,0,0,2,2,0,0,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};
static const int16_t PST_B[64] = {
   -10,-10,-10,-10,-10,-10,-10,-10,
   -10,0,0,0,0,0,0,-10,
   -10,0,4,6,6,4,0,-10,
   -10,2,4,6,6,4,2,-10,
   -10,0,6,6,6,6,0,-10,
   -10,6,6,6,6,6,6,-10,
   -10,2,0,0,0,0,2,-10,
   -10,-10,-10,-10,-10,-10,-10,-10
};
static const int16_t PST_R[64] = {
     0,0,2,4,4,2,0,0,
    -2,0,0,0,0,0,0,-2,
    -2,0,0,0,0,0,0,-2,
    -2,0,0,0,0,0,0,-2,
    -2,0,0,0,0,0,0,-2,
    -2,0,0,0,0,0,0,-2,
     4,6,6,6,6,6,6,4,
     0,0,0,2,2,0,0,0
};
static const int16_t PST_Q[64] = {
    -10,-10,-10,-5,-5,-10,-10,-10,
    -10,0,0,0,0,0,0,-10,
    -10,0,2,2,2,2,0,-10,
     -5,0,2,3,3,2,0,-5,
      0,0,2,3,3,2,0,-5,
    -10,2,2,2,2,2,0,-10,
    -10,0,2,0,0,0,0,-10,
    -10,-10,-10,-5,-5,-10,-10,-10
};

static inline int pstScore(uint8_t pc, uint8_t sq){
    uint8_t s = sq;
    bool w = isWhite(pc);
    if (!w) s = mirrorSq(sq);

    switch (pc){
        case WP: return  PST_P[s];
        case WN: return  PST_N[s];
        case WB: return  PST_B[s];
        case WR: return  PST_R[s];
        case WQ: return  PST_Q[s];
        case BP: return -PST_P[s];
        case BN: return -PST_N[s];
        case BB: return -PST_B[s];
        case BR: return -PST_R[s];
        case BQ: return -PST_Q[s];
        default: return 0;
    }
}

static int evaluate(const Position& p)
{
    int score = 0;
    for (uint8_t sq=0;sq<64;sq++){
        uint8_t pc = p.board[sq];
        if (!pc) continue;
        score += pieceValue(pc);
        score += pstScore(pc, sq);
    }
    return score;
}

static inline int pieceClass(uint8_t pc){
    switch (pc){
        case WP: case BP: return 1;
        case WN: case BN: return 3;
        case WB: case BB: return 3;
        case WR: case BR: return 5;
        case WQ: case BQ: return 9;
        default: return 0;
    }
}
static inline int mvvLva(uint8_t victim, uint8_t attacker){
    return pieceClass(victim)*10 - pieceClass(attacker);
}

// ---------------- Search state (NO STACK BUFFERS) ----------------
static int g_nodes = 0;
static int g_nodeBudget = 0;
static int g_ply = 0;

// Per-ply move buffers, kept off the stack to avoid blowups. Allocated on the
// heap only while the AI is actually thinking (see chessAiBestMove), so they
// cost 0 RAM when the chess app is not open / not searching.
static Move (*g_movesBuf)[256] = nullptr;

static inline void searchYield()
{
    // Avoid WDT during longer searches
    if ((g_nodes & 2047) == 0) {
        delay(0);
    }
}

static int quiescence(Position& p, int alpha, int beta)
{
    if (++g_nodes >= g_nodeBudget) return evaluate(p);
    searchYield();

    int stand = evaluate(p);
    if (stand >= beta) return beta;
    if (stand > alpha) alpha = stand;

    Move* moves = g_movesBuf[g_ply];
    int n = chessGenLegalMoves(p, moves, 256);

    for (int i=0;i<n;i++){
        uint8_t victim = p.board[moves[i].to];
        bool isCap = victim != EMPTY;
        bool isPromo = (moves[i].flags & MF_PROMO) != 0;
        if (!isCap && !isPromo) continue;

        uint8_t cap=0, oc=0, oe=0; uint16_t oh=0;
        chessMakeMove(p, moves[i], cap, oc, oe, oh);

        // next ply
        g_ply = std::min(g_ply + 1, 7);
        int score = -quiescence(p, -beta, -alpha);
        g_ply = std::max(g_ply - 1, 0);

        chessUnmakeMove(p, moves[i], cap, oc, oe, oh);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;

        if (g_nodes >= g_nodeBudget) break;
    }

    return alpha;
}

static bool squareIsAttackedAfterMove(Position& p, const Move& m)
{
    // after making m, check if opponent can capture on m.to
    Move reply[256];
    int n = chessGenLegalMoves(p, reply, 256);
    for (int i=0;i<n;i++){
        if (reply[i].to == m.to) return true;
    }
    return false;
}

static inline int scoreMove(const Position& p, const Move& m)
{
    uint8_t attacker = p.board[m.from];
    uint8_t victim   = p.board[m.to];

    int s = 0;

    if (victim){
        s += 1000 + mvvLva(victim, attacker);

        // crude anti-blunder: if we capture something cheap with something expensive
        // and the landing square is immediately capturable, heavily penalize.
        int attV = pieceClass(attacker);
        int vicV = pieceClass(victim);
        if (attV > vicV){
            Position tmp = p;
            uint8_t cap=0, oc=0, oe=0; uint16_t oh=0;
            chessMakeMove(tmp, m, cap, oc, oe, oh);

            if (squareIsAttackedAfterMove(tmp, m)){
                s -= (attV - vicV) * 250;  // strong penalty (tune)
                // extra penalty for queen hanging
                if (attV >= 9) s -= 1200;
            }
            // no need to unmake because tmp is a copy
        }
    }

    if (m.flags & MF_PROMO)  s += 900;
    if (m.flags & MF_CASTLE) s += 30;

    return s;
}


static int alphabeta(Position& p, int depth, int alpha, int beta)
{
    if (++g_nodes >= g_nodeBudget) return evaluate(p);
    searchYield();

    Move* moves = g_movesBuf[g_ply];
    int n = chessGenLegalMoves(p, moves, 256);

    if (n == 0){
        bool inCheck = chessInCheck(p, p.whiteToMove);
        if (inCheck) return -100000 + (7 - depth);
        return 0;
    }

    if (depth <= 0){
        return quiescence(p, alpha, beta);
    }

    // selection sort for ordering (cheap and stable)
    for (int i=0;i<n;i++){
        int best=i, bestS=scoreMove(p,moves[i]);
        for (int j=i+1;j<n;j++){
            int sj=scoreMove(p,moves[j]);
            if (sj>bestS){ best=j; bestS=sj; }
        }
        if (best!=i) std::swap(moves[i], moves[best]);
    }

    for (int i=0;i<n;i++){
        uint8_t cap=0, oc=0, oe=0; uint16_t oh=0;
        chessMakeMove(p, moves[i], cap, oc, oe, oh);

        g_ply = std::min(g_ply + 1, 7);
        int score = -alphabeta(p, depth-1, -beta, -alpha);
        g_ply = std::max(g_ply - 1, 0);

        chessUnmakeMove(p, moves[i], cap, oc, oe, oh);

        if (score > alpha) alpha = score;
        if (alpha >= beta) break;
        if (g_nodes >= g_nodeBudget) break;
    }
    return alpha;
}

Move chessAiBestMove(Position& p, int maxDepth, int nodeBudget)
{
    // Work on a copy so search can NEVER corrupt the real game state.
    Position pos = p;

    g_nodes = 0;
    g_nodeBudget = nodeBudget;
    g_ply = 0;

    Move root[256];
    int n = chessGenLegalMoves(pos, root, 256);
    if (n == 0) return Move{0,0,0,0};

    // root ordering
    for (int i=0;i<n;i++){
        int best=i, bestS=scoreMove(pos, root[i]);
        for (int j=i+1;j<n;j++){
            int sj=scoreMove(pos, root[j]);
            if (sj>bestS){ best=j; bestS=sj; }
        }
        if (best!=i) std::swap(root[i], root[best]);
    }

    Move bestMove = root[0];

    // Allocate the per-ply search buffers only for the duration of this search
    // (freed before returning). If the allocation fails, fall back to the best
    // move found by root ordering instead of crashing.
    g_movesBuf = (Move (*)[256])malloc(8 * 256 * sizeof(Move));
    if (g_movesBuf == nullptr) {
        return bestMove;
    }

    for (int depth=1; depth<=maxDepth; depth++){
        int bestScore = -1000000;
        Move localBest = bestMove;

        for (int i=0;i<n;i++){
            uint8_t cap=0, oc=0, oe=0; uint16_t oh=0;
            chessMakeMove(pos, root[i], cap, oc, oe, oh);

            g_ply = 1;
            int score = -alphabeta(pos, depth-1, -1000000, 1000000);
            g_ply = 0;

            chessUnmakeMove(pos, root[i], cap, oc, oe, oh);

            if (score > bestScore){
                bestScore = score;
                localBest = root[i];
            }
            if (g_nodes >= g_nodeBudget) break;
        }

        bestMove = localBest;
        if (g_nodes >= g_nodeBudget) break;
    }

    free(g_movesBuf);
    g_movesBuf = nullptr;
    return bestMove;
}

#endif