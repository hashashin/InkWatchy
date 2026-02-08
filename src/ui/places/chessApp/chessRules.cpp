#include "defines.h"

#if CHESS

#include <stdint.h>
#include <string.h>
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
static inline bool isBlack(uint8_t p){ return p>=BP && p<=BK; }

struct Position {
    uint8_t board[64];
    bool whiteToMove;
    uint8_t castling;   // 1 WK,2 WQ,4 BK,8 BQ
    uint8_t epSquare;   // 0..63 or 255
    uint16_t halfmove;
    uint16_t fullmove;
};

static inline int sqR(uint8_t s){ return s>>3; }
static inline int sqC(uint8_t s){ return s&7; }
static inline bool onBoard(int r,int c){ return (unsigned)r<8 && (unsigned)c<8; }
static inline uint8_t rcToSq(int r,int c){ return (uint8_t)(r*8+c); }

void chessInitStartPos(Position& p)
{
    memset(&p, 0, sizeof(p));
    static const uint8_t start[64] = {
        BR,BN,BB,BQ,BK,BB,BN,BR,
        BP,BP,BP,BP,BP,BP,BP,BP,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        WP,WP,WP,WP,WP,WP,WP,WP,
        WR,WN,WB,WQ,WK,WB,WN,WR
    };
    memcpy(p.board, start, 64);
    p.whiteToMove = true;
    p.castling = 1|2|4|8;
    p.epSquare = 255;
    p.halfmove = 0;
    p.fullmove = 1;
}

static uint8_t findKingSq(const Position& p, bool white)
{
    uint8_t k = white ? WK : BK;
    for (uint8_t i=0;i<64;i++) if (p.board[i]==k) return i;
    return 255;
}

static bool attacksSquare(const Position& p, uint8_t from, uint8_t pc, uint8_t target)
{
    int fr = sqR(from), fc = sqC(from);
    int tr = sqR(target), tc = sqC(target);
    int dr = tr - fr, dc = tc - fc;

    if (pc == WP) return (dr == -1 && (dc == -1 || dc == 1));
    if (pc == BP) return (dr == 1 && (dc == -1 || dc == 1));

    if (pc == WN || pc == BN){
        int adr = dr<0?-dr:dr, adc = dc<0?-dc:dc;
        return (adr==2 && adc==1) || (adr==1 && adc==2);
    }

    if (pc == WK || pc == BK){
        int adr = dr<0?-dr:dr, adc = dc<0?-dc:dc;
        return adr<=1 && adc<=1;
    }

    auto lineClear = [&](int stepR, int stepC)->bool{
        int r = fr + stepR, c = fc + stepC;
        while (onBoard(r,c)){
            uint8_t sq = rcToSq(r,c);
            if (sq == target) return true;
            if (p.board[sq] != EMPTY) return false;
            r += stepR; c += stepC;
        }
        return false;
    };

    if (pc==WB || pc==BB || pc==WQ || pc==BQ){
        if (dr==dc && dr!=0) return lineClear((dr>0)?1:-1, (dc>0)?1:-1);
        if (dr==-dc && dr!=0) return lineClear((dr>0)?1:-1, (dc>0)?-1:1);
    }
    if (pc==WR || pc==BR || pc==WQ || pc==BQ){
        if (dr==0 && dc!=0) return lineClear(0, (dc>0)?1:-1);
        if (dc==0 && dr!=0) return lineClear((dr>0)?1:-1, 0);
    }

    return false;
}

bool chessInCheck(const Position& p, bool whiteKing)
{
    uint8_t ksq = findKingSq(p, whiteKing);
    if (ksq == 255) return false;

    for (uint8_t sq=0;sq<64;sq++){
        uint8_t pc = p.board[sq];
        if (pc==EMPTY) continue;
        if (whiteKing && isBlack(pc)){
            if (attacksSquare(p, sq, pc, ksq)) return true;
        } else if (!whiteKing && isWhite(pc)){
            if (attacksSquare(p, sq, pc, ksq)) return true;
        }
    }
    return false;
}

static void pushMove(Move* out, int& n, int cap, uint8_t from, uint8_t to, uint8_t flags=0, uint8_t promo=0)
{
    if (n>=cap) return;
    out[n++] = Move{from,to,promo,flags};
}

static void genPseudoForSquare(const Position& p, uint8_t from, Move* out, int& n, int cap)
{
    uint8_t pc = p.board[from];
    if (pc==EMPTY) return;

    bool white = isWhite(pc);
    if (p.whiteToMove != white) return;

    int r = sqR(from), c = sqC(from);

    // Pawn
    if (pc==WP || pc==BP){
        int dir = (pc==WP) ? -1 : 1;
        int startRank = (pc==WP) ? 6 : 1;
        int promoRank = (pc==WP) ? 0 : 7;

        int r1 = r + dir;
        if (onBoard(r1,c)){
            uint8_t to = rcToSq(r1,c);
            if (p.board[to]==EMPTY){
                if (r1 == promoRank) pushMove(out,n,cap,from,to, MF_PROMO, 0);
                else pushMove(out,n,cap,from,to);

                if (r == startRank){
                    int r2 = r + 2*dir;
                    uint8_t to2 = rcToSq(r2,c);
                    if (p.board[to2]==EMPTY) pushMove(out,n,cap,from,to2);
                }
            }
        }

        for (int dc=-1; dc<=1; dc+=2){
            int cc = c + dc;
            int rr = r + dir;
            if (!onBoard(rr,cc)) continue;
            uint8_t to = rcToSq(rr,cc);
            uint8_t tgt = p.board[to];

            if (tgt!=EMPTY && (white?isBlack(tgt):isWhite(tgt))){
                if (rr==promoRank) pushMove(out,n,cap,from,to, MF_PROMO, 0);
                else pushMove(out,n,cap,from,to);
            }
            if (p.epSquare != 255 && to == p.epSquare){
                pushMove(out,n,cap,from,to, MF_EP);
            }
        }
        return;
    }

    // Knight
    if (pc==WN || pc==BN){
        static const int kD[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
        for (auto& d : kD){
            int rr=r+d[0], cc=c+d[1];
            if (!onBoard(rr,cc)) continue;
            uint8_t to = rcToSq(rr,cc);
            uint8_t t = p.board[to];
            if (t==EMPTY || (white?isBlack(t):isWhite(t))) pushMove(out,n,cap,from,to);
        }
        return;
    }

    // King + castling pseudo (path-in-check validated later)
    if (pc==WK || pc==BK){
        for (int dr=-1;dr<=1;dr++){
            for (int dc=-1;dc<=1;dc++){
                if (dr==0 && dc==0) continue;
                int rr=r+dr, cc=c+dc;
                if (!onBoard(rr,cc)) continue;
                uint8_t to = rcToSq(rr,cc);
                uint8_t t = p.board[to];
                if (t==EMPTY || (white?isBlack(t):isWhite(t))) pushMove(out,n,cap,from,to);
            }
        }

        if (white && from==rcToSq(7,4)){
            if ((p.castling & 1) && p.board[rcToSq(7,5)]==EMPTY && p.board[rcToSq(7,6)]==EMPTY){
                pushMove(out,n,cap,from,rcToSq(7,6),MF_CASTLE);
            }
            if ((p.castling & 2) && p.board[rcToSq(7,3)]==EMPTY && p.board[rcToSq(7,2)]==EMPTY && p.board[rcToSq(7,1)]==EMPTY){
                pushMove(out,n,cap,from,rcToSq(7,2),MF_CASTLE);
            }
        }
        if (!white && from==rcToSq(0,4)){
            if ((p.castling & 4) && p.board[rcToSq(0,5)]==EMPTY && p.board[rcToSq(0,6)]==EMPTY){
                pushMove(out,n,cap,from,rcToSq(0,6),MF_CASTLE);
            }
            if ((p.castling & 8) && p.board[rcToSq(0,3)]==EMPTY && p.board[rcToSq(0,2)]==EMPTY && p.board[rcToSq(0,1)]==EMPTY){
                pushMove(out,n,cap,from,rcToSq(0,2),MF_CASTLE);
            }
        }
        return;
    }

    auto slide = [&](int sdr,int sdc){
        int rr=r+sdr, cc=c+sdc;
        while (onBoard(rr,cc)){
            uint8_t to=rcToSq(rr,cc);
            uint8_t t=p.board[to];
            if (t==EMPTY){
                pushMove(out,n,cap,from,to);
            } else {
                if (white?isBlack(t):isWhite(t)) pushMove(out,n,cap,from,to);
                break;
            }
            rr+=sdr; cc+=sdc;
        }
    };

    if (pc==WB || pc==BB){ slide(-1,-1); slide(-1,1); slide(1,-1); slide(1,1); return; }
    if (pc==WR || pc==BR){ slide(-1,0); slide(1,0); slide(0,-1); slide(0,1); return; }
    if (pc==WQ || pc==BQ){
        slide(-1,-1); slide(-1,1); slide(1,-1); slide(1,1);
        slide(-1,0);  slide(1,0);  slide(0,-1); slide(0,1);
        return;
    }
}

void chessMakeMove(Position& p, const Move& m, uint8_t& captured, uint8_t& oldCastling, uint8_t& oldEp, uint16_t& oldHalfmove)
{
    captured = p.board[m.to];
    oldCastling = p.castling;
    oldEp = p.epSquare;
    oldHalfmove = p.halfmove;

    uint8_t pc = p.board[m.from];
    p.epSquare = 255;

    if (pc==WP || pc==BP || captured!=EMPTY) p.halfmove = 0;
    else p.halfmove++;

    p.board[m.to] = pc;
    p.board[m.from] = EMPTY;

    if (m.flags & MF_EP){
        int dir = (pc==WP) ? 1 : -1;
        uint8_t capSq = (uint8_t)(m.to + dir*8);
        captured = p.board[capSq];
        p.board[capSq] = EMPTY;
    }

    if (pc==WP && (int)m.to == (int)m.from - 16) p.epSquare = (uint8_t)(m.from - 8);
    if (pc==BP && (int)m.to == (int)m.from + 16) p.epSquare = (uint8_t)(m.from + 8);

    if (m.flags & MF_PROMO){
        p.board[m.to] = isWhite(pc) ? WQ : BQ;
        if (m.promo) p.board[m.to] = m.promo;
    }

    if (m.flags & MF_CASTLE){
        if (pc==WK && m.from==rcToSq(7,4) && m.to==rcToSq(7,6)){
            p.board[rcToSq(7,5)] = WR; p.board[rcToSq(7,7)] = EMPTY;
        } else if (pc==WK && m.to==rcToSq(7,2)){
            p.board[rcToSq(7,3)] = WR; p.board[rcToSq(7,0)] = EMPTY;
        }
        if (pc==BK && m.from==rcToSq(0,4) && m.to==rcToSq(0,6)){
            p.board[rcToSq(0,5)] = BR; p.board[rcToSq(0,7)] = EMPTY;
        } else if (pc==BK && m.to==rcToSq(0,2)){
            p.board[rcToSq(0,3)] = BR; p.board[rcToSq(0,0)] = EMPTY;
        }
    }

    auto clearCastlingIf = [&](uint8_t sq, uint8_t mask){
        if (m.from==sq || m.to==sq) p.castling &= ~mask;
    };
    clearCastlingIf(rcToSq(7,4), 1|2);
    clearCastlingIf(rcToSq(7,7), 1);
    clearCastlingIf(rcToSq(7,0), 2);
    clearCastlingIf(rcToSq(0,4), 4|8);
    clearCastlingIf(rcToSq(0,7), 4);
    clearCastlingIf(rcToSq(0,0), 8);

    p.whiteToMove = !p.whiteToMove;
    if (p.whiteToMove) p.fullmove++;
}

void chessUnmakeMove(Position& p, const Move& m, uint8_t captured, uint8_t oldCastling, uint8_t oldEp, uint16_t oldHalfmove)
{
    if (p.whiteToMove) p.fullmove--;
    p.whiteToMove = !p.whiteToMove;

    p.castling = oldCastling;
    p.epSquare = oldEp;
    p.halfmove = oldHalfmove;

    uint8_t pc = p.board[m.to];

    if (m.flags & MF_CASTLE){
        if (pc==WK && m.to==rcToSq(7,6)){
            p.board[rcToSq(7,7)] = WR; p.board[rcToSq(7,5)] = EMPTY;
        } else if (pc==WK && m.to==rcToSq(7,2)){
            p.board[rcToSq(7,0)] = WR; p.board[rcToSq(7,3)] = EMPTY;
        }
        if (pc==BK && m.to==rcToSq(0,6)){
            p.board[rcToSq(0,7)] = BR; p.board[rcToSq(0,5)] = EMPTY;
        } else if (pc==BK && m.to==rcToSq(0,2)){
            p.board[rcToSq(0,0)] = BR; p.board[rcToSq(0,3)] = EMPTY;
        }
    }

    p.board[m.from] = p.board[m.to];
    if (m.flags & MF_PROMO){
        p.board[m.from] = p.whiteToMove ? WP : BP;
    }

    p.board[m.to] = captured;

    if (m.flags & MF_EP){
        p.board[m.to] = EMPTY;
        int dir = (p.whiteToMove) ? 1 : -1;
        uint8_t capSq = (uint8_t)(m.to + dir*8);
        p.board[capSq] = (p.whiteToMove) ? BP : WP;
    }
}

static bool isLegalAfterMake(Position& p, const Move& m)
{
    uint8_t captured=0, oc=0, oe=0; uint16_t oh=0;
    chessMakeMove(p,m,captured,oc,oe,oh);
    bool ok = !chessInCheck(p, !p.whiteToMove);
    chessUnmakeMove(p,m,captured,oc,oe,oh);
    return ok;
}

static bool isCastlePathOk(const Position& pIn, const Move& m)
{
    if (!(m.flags & MF_CASTLE)) return true;

    Position t = pIn;
    bool sideWhite = t.whiteToMove;

    if (chessInCheck(t, sideWhite)) return false;

    uint8_t kFrom = m.from;
    uint8_t kMid = (m.to > m.from) ? (uint8_t)(m.from + 1) : (uint8_t)(m.from - 1);

    Move step{ kFrom, kMid, 0, 0 };
    uint8_t cap=0, oc=0, oe=0; uint16_t oh=0;
    chessMakeMove(t, step, cap, oc, oe, oh);
    bool midOk = !chessInCheck(t, sideWhite);
    chessUnmakeMove(t, step, cap, oc, oe, oh);
    if (!midOk) return false;

    return true;
}

static bool chessIsLegalMoveInternal(const Position& pIn, const Move& m)
{
    Position p = pIn;
    if (!isLegalAfterMake(p, m)) return false;
    if (!isCastlePathOk(pIn, m)) return false;
    return true;
}

int chessGenLegalMoves(const Position& p, Move* out, int cap)
{
    int n = 0;

    // genera pseudo por casilla y filtra legal al vuelo
    for (uint8_t sq = 0; sq < 64; sq++) {
        Move pseudo[32];   // <- MUCHO más pequeño (máx movimientos de una pieza)
        int pn = 0;

        genPseudoForSquare(p, sq, pseudo, pn, 32);

        for (int i = 0; i < pn && n < cap; i++) {
            if (chessIsLegalMoveInternal(p, pseudo[i])) {
                out[n++] = pseudo[i];
            }
        }
    }
    return n;
}

bool chessIsGameOver(const Position& p, bool& isMate, bool& isStalemate)
{
    Move m[64];
    int n = chessGenLegalMoves(p, m, 64);
    if (n > 0){ isMate=false; isStalemate=false; return false; }

    bool inCheck = chessInCheck(p, p.whiteToMove);
    isMate = inCheck;
    isStalemate = !inCheck;
    return true;
}

#endif