#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>

// Pseudotypes
#define BOOL int
#define FALSE 0
#define TRUE (!FALSE)

#define PACKED_SQUARE   int
#define PIECE_T         int
#define BOARDPTR_T(p)   PIECE_T (*p)[8][8]

// How the board will look in terminal window
// If you're not using a chess compatible font, comment out USE_CHESS_FONT
// If your terminal window doesn't do VT100 codes, comment out USE_ESCAPE
// Uncomment DONT_SCROLL if you want the board always drawn at top of window
// Set COLUMN_SPACER to be "" or " " based on whatever looks good for you.
#define USE_CHESS_FONT
#define USE_ESCAPE_CODES
#define USE_SPACERS

#ifdef USE_SPACERS
#define COLUMN_SPACER " "
#define COLUMN_SPACER_WHITE_LEFT  " "
#define COLUMN_SPACER_WHITE_RIGHT " "
#define COLUMN_SPACER_BLACK_LEFT  " "
#define COLUMN_SPACER_BLACK_RIGHT " "
#else
#define DONT_SCROLL
#define COLUMN_SPACER ""
#define COLUMN_SPACER_WHITE_LEFT  ""
#define COLUMN_SPACER_WHITE_RIGHT ""
#define COLUMN_SPACER_BLACK_LEFT  ""
#define COLUMN_SPACER_BLACK_RIGHT ""
#endif

// Debugging
//#define DEBUG_MATES
//#define DEBUG_EN_PASSANT
//#define DEBUG_CHECKS
//#define DEBUG_CASTLING
//#define DEBUG_PROMOTION
//#define DEBUG_EVALUATION

//
// Constants
//

// How many (half) moves to look ahead.
// Some debugging works best with this set to 0
// Setting it higher than 3 makes the game quite slow
#define LOOKAHEAD 3

// Additional flags on pieces OR destination squares
// to indicate that they're vulnerable to en passant,
// ineligible to castle, or promoting to a different piece.
#define CAN_EN_PASSANT  (1<<8)
#define IS_EN_PASSANT   (1<<9)
#define CANNOT_CASTLE   (1<<10)
#define IS_CASTLE       (1<<11)
#define IS_KNIGHT_PROMO (1<<12)
#define FLAGS_START     (1<<8)
#define NO_FLAGS(x)     ((x) % FLAGS_START)
#define FLAGS(x)        ((x) - NO_FLAGS(x))

// Color Bias
#define COLOR_WHITE     0
#define COLOR_BLACK     (1<<3)
#define IS_BLACK(x)     (NO_FLAGS(x) >= COLOR_BLACK)
#define ADVANCE_DIRECTION(color)    ((color) ? 1 : -1)
#define COLOR_NAME(color) ((color) ? "black" : "white")
#define COLOR_FLAG(color) ((color) ? COLOR_BLACK : COLOR_WHITE)

// Pieces
#define PIECE_PAWN      1
#define PIECE_KNIGHT    2
#define PIECE_BISHOP    3
#define PIECE_ROOK      4
#define PIECE_QUEEN     5
#define PIECE_KING      6
#define PIECE(x)        (NO_FLAGS(x) % COLOR_BLACK)

// Piece Values
#define VALUE_PAWN      1
#define VALUE_KNIGHT    3
#define VALUE_BISHOP    3
#define VALUE_ROOK      5
#define VALUE_QUEEN     9
#define VALUE_KING      1000

#define VALUE_STALEMATE 5000
#define VALUE_CHECKMATE 10000
#define VALUE_ILLEGAL   -32767

static const PIECE_T pieceValues[] = {
    0,              VALUE_PAWN,     VALUE_KNIGHT,   VALUE_BISHOP,   VALUE_ROOK,     VALUE_QUEEN,    VALUE_KING
};

// Colored Pieces
#define WHITE_PAWN      (COLOR_WHITE + PIECE_PAWN)
#define WHITE_KNIGHT    (COLOR_WHITE + PIECE_KNIGHT)
#define WHITE_BISHOP    (COLOR_WHITE + PIECE_BISHOP)
#define WHITE_ROOK      (COLOR_WHITE + PIECE_ROOK)
#define WHITE_QUEEN     (COLOR_WHITE + PIECE_QUEEN)
#define WHITE_KING      (COLOR_WHITE + PIECE_KING)

#define BLACK_PAWN      (COLOR_BLACK + PIECE_PAWN)
#define BLACK_KNIGHT    (COLOR_BLACK + PIECE_KNIGHT)
#define BLACK_BISHOP    (COLOR_BLACK + PIECE_BISHOP)
#define BLACK_ROOK      (COLOR_BLACK + PIECE_ROOK)
#define BLACK_QUEEN     (COLOR_BLACK + PIECE_QUEEN)
#define BLACK_KING      (COLOR_BLACK + PIECE_KING)

// Boards
#define MAX_POSSIBLE_MOVES 1024
#define EVAL_IN_CHECK   10000
#define EVAL_FORCE      9999
#define BARGS           BOARDPTR_T(board), int row, int col
#define BPARAMS         board, row, col

#define SQUARE_D4       pack(4, 3)
#define SQUARE_D5       pack(3, 3)
#define SQUARE_E4       pack(4, 4)
#define SQUARE_E5       pack(3, 4)

#define SQUARE_NONE    -1
#define EMPTY_SQUARE    0
// NOTE: with this initialization, array row "0" is chess rank 8; array column 0 is chess file a
static const PIECE_T initial_board[8][8] =
{
    BLACK_ROOK,     BLACK_KNIGHT,   BLACK_BISHOP,   BLACK_QUEEN,    BLACK_KING,     BLACK_BISHOP,   BLACK_KNIGHT,   BLACK_ROOK,
    BLACK_PAWN,     BLACK_PAWN,     BLACK_PAWN,     BLACK_PAWN,     BLACK_PAWN,     BLACK_PAWN,     BLACK_PAWN,     BLACK_PAWN,
    EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,
    EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,
    EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,
    EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,   EMPTY_SQUARE,
    WHITE_PAWN,     WHITE_PAWN,     WHITE_PAWN,     WHITE_PAWN,     WHITE_PAWN,     WHITE_PAWN,     WHITE_PAWN,     WHITE_PAWN,
    WHITE_ROOK,     WHITE_KNIGHT,   WHITE_BISHOP,   WHITE_QUEEN,    WHITE_KING,     WHITE_BISHOP,   WHITE_KNIGHT,   WHITE_ROOK
};

static PIECE_T chessBoard[8][8];

#define PS(sq) (PACKED_SQUARE)( (7 - ((#sq)[1] - '1')) * 8 + ((#sq)[0] - 'a') )
typedef struct { PACKED_SQUARE source; PACKED_SQUARE dest; int value; } MOVEINFO;

// These are trivial openings, consisting of white and black's first moves
// WARNING: The OSX command line compiler srand / rand is weak modulo 14 in tests
typedef struct { const MOVEINFO whiteMove; const MOVEINFO blackMove; const char* const openingName; } OPENING;
OPENING openings[] = {
	{{PS(e2), PS(e4), 1}, {PS(e7), PS(e5), 1}, "Ruy Lopez" },
	{{PS(e2), PS(e4), 1}, {PS(c7), PS(c5), 1}, "Sicilian" },
	{{PS(e2), PS(e4), 1}, {PS(e7), PS(e6), 1}, "French" },
	{{PS(e2), PS(e4), 1}, {PS(c7), PS(c6), 1}, "Caro-Kann" },
	{{PS(d2), PS(d4), 1}, {PS(d7), PS(d5), 1}, "Queen's Gambit" },
	{{PS(d2), PS(d4), 1}, {PS(g8), PS(f6), 1}, "Indian" },
	{{PS(c2), PS(c4), 1}, {PS(c7), PS(c5), 1}, "English" },
	{{PS(c2), PS(c4), 1}, {PS(g8), PS(f6), 1}, "Indian Defense" },
	{{PS(c2), PS(c4), 1}, {PS(e7), PS(e5), 1}, "Reverse Sicilian" },
	{{PS(g1), PS(f3), 1}, {PS(d7), PS(d5), 1}, "Queen's Pawn" },
	{{PS(g1), PS(f3), 1}, {PS(g8), PS(f6), 1}, "Indian Variant 1" },
	{{PS(g1), PS(f3), 1}, {PS(c7), PS(c5), 1}, "It's Complicated" },
	{{PS(g1), PS(f3), 1}, {PS(e7), PS(e6), 1}, "French Defense" },
	{{PS(g1), PS(f3), 1}, {PS(g7), PS(g6), 1}, "Fianchetto" },
	{{PS(e2), PS(e4), 1}, {PS(e7), PS(e5), 1}, "Ruy Lopez" }
};

static char* pieceLabels[]  = { " ", "P", "N", "B", "R", "Q", "K", " ",
                                ".", "p", "n", "b", "r", "q", "k", " " };

#ifdef USE_CHESS_FONT
static char* pieceSymbols[] = { " ", "♙", "♘", "♗", "♖", "♕", "♔", " ",
                                " ", "♟", "♞", "♝", "♜", "♛", "♚", " " };
#else
static char** pieceSymbols = &pieceLabels[0];
#endif

BOOL inCheck(BOOL isBlack, BOARDPTR_T(board));
BOOL inCheckKnownKingSquare(PACKED_SQUARE kingSquare, BOARDPTR_T(board));
void tryMove(BOARDPTR_T(newBoard), PACKED_SQUARE dest, BARGS);
static int ruleOf75 = 0;

//
// Basic square operations
//

// Are the square coordinates valid?
BOOL isSquare(BARGS) { return (row >= 0 && row <= 7 && col >= 0 && col <= 7); }
// What's on the square (without flags, for now)
PIECE_T getSquare(BARGS) { return NO_FLAGS((*board)[row][col]); }
// What's on the square (with flags)
PIECE_T getSquareWithFlags(BARGS) { return (*board)[row][col]; }
// Is the square empty?
BOOL isEmpty(BARGS) { return (getSquare(BPARAMS) == EMPTY_SQUARE); }
// Is the square occupied by an enemy?
BOOL isEnemy(PIECE_T myPiece, BARGS) {
    PIECE_T target = getSquare(BPARAMS);
    return ((target != EMPTY_SQUARE) && (IS_BLACK(target) != IS_BLACK(myPiece)));
}
// Disregarding bounds, check, etc, is the square valid to move to?
BOOL isDest(PIECE_T myPiece, BARGS) { return isEmpty(BPARAMS) || isEnemy(myPiece, BPARAMS); }
// Considering bounds, is the square valid to move to?
BOOL isValid(PIECE_T myPiece, BARGS) { return isSquare(BPARAMS) && isDest(myPiece, BPARAMS); }
// Pack a row and column into an integer
PACKED_SQUARE pack(int row, int col) { return row * 8 + col; }
// Unpack an integer to a row or column
int unpackRow(PACKED_SQUARE square) { return NO_FLAGS(square) / 8; }
int unpackCol(PACKED_SQUARE square) { return NO_FLAGS(square) % 8; }

// What's on the square (without flags, for now)
PIECE_T getSquarePacked(BOARDPTR_T(board), PACKED_SQUARE source) { return getSquare(board, unpackRow(source), unpackCol(source)); }
// What's on the square (with flags)
PIECE_T getSquareWithFlagsPacked(BOARDPTR_T(board), PACKED_SQUARE source) { return getSquareWithFlags(board, unpackRow(source), unpackCol(source)); }

// NOTE: array row 0 is chess rank "8"; array column 0 is chess file "a"
// Packed King's square by color
PACKED_SQUARE kingsSquare(BOOL isBlack, BOARDPTR_T(board)) {
    PACKED_SQUARE kingSquare = SQUARE_NONE;
    int row, col;
    PIECE_T piece, ourKing;
    // Find our king
    ourKing = isBlack ? BLACK_KING : WHITE_KING;
    for (row = 0; row <= 7; row++) {
        for (col = 0; col <= 7; col++) {
            piece = getSquare(BPARAMS);
            if (piece == ourKing) {
                kingSquare = pack(row, col);
                break;
            }
        }
        if (kingSquare != SQUARE_NONE) break;
    }
    return kingSquare;
}
// Name of a square
void printSquareName(PACKED_SQUARE square) {
    int  rank = (7 - unpackRow(square)) + 1;
    char file = (char)(unpackCol(square)) + 'a';
    printf("%c%d", file, rank);
}

// Screen clear
void eraseBoard() {
#ifdef DONT_SCROLL
#ifdef USE_ESCAPE_CODES
    // VT100
    printf("\e[1;1H\e[2J");
#else
    // Linux, OSX, etc.
    system("clear");
#endif
#endif
}

// Move description
void printMove(PACKED_SQUARE source, PACKED_SQUARE dest, BOARDPTR_T(board)) {
    PIECE_T tryBoard[8][8];
    int sourceRow = unpackRow(source);
    int sourceCol = unpackCol(source);
    int destRow = unpackRow(dest);
    int destCol = unpackCol(dest);
    PIECE_T attacker = getSquare(board, sourceRow, sourceCol);
    PIECE_T target = getSquare(board, destRow, destCol);
    if (dest & IS_CASTLE) {
        if (unpackCol(dest) < 4) printf("O-O-O");
        else printf("O-O");
	}
    else {
        printf("%s", pieceLabels[PIECE(attacker)]);
        printSquareName(source);
        if (target != EMPTY_SQUARE) printf("x");
        if (dest & IS_EN_PASSANT) {
            // Report the destination of the attacker rather than the victim
            printSquareName(dest + ADVANCE_DIRECTION(IS_BLACK(attacker)) * 8);
            printf(" e.p.");
        } else {
            printSquareName(dest);
            // Test for pawn promotion
            tryMove(&tryBoard, dest, board, sourceRow, sourceCol);
            PIECE_T afterPiece = getSquare(&tryBoard, destRow, destCol);
            if (PIECE(attacker) == PIECE_PAWN && PIECE(afterPiece) != PIECE_PAWN) {
                printf("%s", pieceLabels[PIECE(afterPiece)]);
            }
        }
    }
    // Test for check
    tryMove(&tryBoard, dest, board, sourceRow, sourceCol);
    if (inCheck(!IS_BLACK(attacker), &tryBoard)) {
        //  Check!
        printf("+");
    }
}

//
// Basic piece operations
//

int getValidMovesAsPiece(PACKED_SQUARE* dests, PIECE_T asPiece, BOOL noCheckTest, BARGS) {
    // Ignores what is actually on the square to simplify queen logic
    // WARNING: There are no flags in asPiece
    int numDests = 0;
    PIECE_T myPiece = asPiece;
    BOOL black = IS_BLACK(myPiece);
    int blackSign = ADVANCE_DIRECTION(black);
    int checkRow, checkCol;
    int deltaRow, deltaCol, delta;
    BOOL promotion;
    BOOL pawnCouldMoveForward = FALSE;
    switch (PIECE(myPiece)) {
        // TODO: Test for moves that are illegal due to check
        case    PIECE_PAWN:
            // Prevent en passant flag from persisting for more than one turn
            (*board)[row][col] &= ~CAN_EN_PASSANT;
            checkRow = row + blackSign;
            if (isSquare(board, checkRow, col) && isEmpty(board, checkRow, col)) {
                // Forward move
                dests[numDests++] = pack(checkRow, col);
                pawnCouldMoveForward = TRUE;
            }
            if (isSquare(board, checkRow, col+1) && isEnemy(myPiece, board, checkRow, col+1)) {
                // Capture toward A
                dests[numDests++] = pack(checkRow, col+1);
            }
            if (isSquare(board, checkRow, col-1) && isEnemy(myPiece, board, checkRow, col-1)) {
                // Capture toward H
                dests[numDests++] = pack(checkRow, col-1);
            }
            if (isSquare(board, checkRow, col+1) && isEnemy(myPiece, board, row, col+1) &&
                (getSquareWithFlags(board, row, col+1) & CAN_EN_PASSANT)) {
                // Capture en passant toward A; sideways capture with extra forward move
                dests[numDests++] = pack(row, col+1) | IS_EN_PASSANT;
            }
            if (isSquare(board, checkRow, col-1) && isEnemy(myPiece, board, row, col-1) &&
                (getSquareWithFlags(board, row, col-1) & CAN_EN_PASSANT)) {
                // Capture en passant toward H; sideways capture with extra forward move
                dests[numDests++] = pack(row, col-1) | IS_EN_PASSANT;
            }
            // Pawn's first move can be ahead two if it could have moved ahead one
            if ((row == (black ? 1 : 6)) && pawnCouldMoveForward) {
                checkRow += blackSign;
                if (isEmpty(board, checkRow, col)) {
                    // Forward move two; this makes the pawn vulnerable to en passant capture for ONE MOVE ONLY
                    // Temporarily store the flag on the DESTINATION rather than on the PIECE
                    dests[numDests++] = pack(checkRow, col) | CAN_EN_PASSANT;
                }
            }
            // Allow for pawn promotion to Queen OR Knight. The move already recorded is for queen
            promotion = (checkRow == 0) || (checkRow == 7);
            if (promotion) {
                PACKED_SQUARE knightPromo = dests[numDests-1] | IS_KNIGHT_PROMO;
                dests[numDests++] = knightPromo;
            }
            break;
        case    PIECE_KNIGHT:
            for (deltaRow = -2; deltaRow <= 2; deltaRow++) {
                for (deltaCol = -2; deltaCol <= 2; deltaCol++) {
                    if (abs(deltaRow) == abs(deltaCol) || deltaRow * deltaCol == 0) continue;
                    if (isValid(myPiece, board, row + deltaRow, col + deltaCol)) {
                        // Empty or capture
                        dests[numDests++] = pack(row + deltaRow, col + deltaCol);
                    }
                }
            }
            break;
        case    PIECE_BISHOP:
            for (deltaRow = -1; deltaRow <= 1; deltaRow += 2) {
                for (deltaCol = -1; deltaCol <= 1; deltaCol += 2) {
                    delta = 1;
                    while (1) {
                        checkRow = row + deltaRow * delta;
                        checkCol = col + deltaCol * delta;
                        if (!isValid(myPiece, board, checkRow, checkCol)) break;
                        // Empty or capture
                        dests[numDests++] = pack(checkRow, checkCol);
                        if (!isEmpty(board, checkRow, checkCol)) break;
                        ++delta;
                    }
                }
            }
            break;
        case    PIECE_ROOK:
            for (deltaRow = -1; deltaRow <= 1; deltaRow++) {
                for (deltaCol = -1; deltaCol <= 1; deltaCol++) {
                    if (abs(deltaRow) == abs(deltaCol)) continue;
                    delta = 1;
                    while (1) {
                        checkRow = row + deltaRow * delta;
                        checkCol = col + deltaCol * delta;
                        if (!isValid(myPiece, board, checkRow, checkCol)) break;
                        // Empty or capture; once moved, a rook cannot castle
                        dests[numDests++] = pack(checkRow, checkCol) | CANNOT_CASTLE;
                        if (!isEmpty(board, checkRow, checkCol)) break;
                        ++delta;
                    }
                }
            }
            break;
        case    PIECE_QUEEN:
            // Sum of possible bishop and rook moves
            numDests = getValidMovesAsPiece(dests, black ? BLACK_BISHOP : WHITE_BISHOP, noCheckTest, BPARAMS);
            numDests += getValidMovesAsPiece(dests + numDests, black ? BLACK_ROOK : WHITE_ROOK, noCheckTest, BPARAMS);
            break;
        case    PIECE_KING:
            // Conventional moves
            for (deltaRow = -1; deltaRow <= 1; deltaRow++) {
                for (deltaCol = -1; deltaCol <= 1; deltaCol++) {
                    if (deltaRow == 0 && deltaCol == 0) continue;
                    checkRow = row + deltaRow;
                    checkCol = col + deltaCol;
                    // Note: isValid() does not test for check
                    if (isValid(myPiece, board, checkRow, checkCol)) {
                        // Empty or capture; once moved, a king cannot castle
                        dests[numDests++] = pack(checkRow, checkCol) | CANNOT_CASTLE;
                    }
                }
            }
            // Castling
            // NOTE: noCheckTest prevents infinite recursion when evaluating legal moves and check
            PIECE_T king = getSquareWithFlags(board, row, col);
            if (!(FLAGS(king) & CANNOT_CASTLE) && (noCheckTest || !inCheck(black, board))) {
                for (int kingSide = 0; kingSide <= 1; kingSide++) {
                    int rookCol = kingSide * 7;
                    int moveSign = -1 + (2 * kingSide);
                    int checkCol = col + (2 * moveSign);
                    PIECE_T rook = getSquareWithFlags(board, row, rookCol);
                    if ((PIECE(rook) == PIECE_ROOK) && !(FLAGS(rook) & CANNOT_CASTLE) &&
                        isEmpty(board, row, col + moveSign) && isEmpty(board, row, checkCol) &&
                        (kingSide || isEmpty(board, row, checkCol + moveSign))) {
                        // Legal to castle on this side (unless moving through or into check)
                        // findBestMove will handle the case where we end up in check, but test the first square
                        BOOL isInCheck;
                        (*board)[row][col] = EMPTY_SQUARE;
                        (*board)[row][col + moveSign] = king;
                        {
                            isInCheck = noCheckTest || inCheck(black, board);
                        }
                        (*board)[row][col + moveSign] = EMPTY_SQUARE;
                        (*board)[row][col] = king;
                        if (!isInCheck) {
#ifdef DEBUG_CASTLING
                            printf("\n%s can try to castle %sside\n", COLOR_NAME(black), kingSide ? "king" : "queen");
#endif
                            dests[numDests++] = pack(row, checkCol) | (CANNOT_CASTLE | IS_CASTLE);
                        }
                    }
                }
            }
            break;
        default:
            break;
    }
    return numDests;
}

int getValidMoves(PACKED_SQUARE* dests, BOOL noCheckTest, BARGS) {
    // Uses the contents of the specified square
    // Note: does not test whether our king is in check after these moves
    return getValidMovesAsPiece(dests, getSquare(BPARAMS), noCheckTest, BPARAMS);
}

int getAllMoves(MOVEINFO* moveInfo, BOOL isBlack, BOOL noCheckTest, BOARDPTR_T(board)) {
    // Find all moves the specified color can make
    // Passed array should be 1024 in size for 16 max
    // pieces x 64 max moves each
    // Note: does not test whether our king is in check after these moves
    PACKED_SQUARE moves[64];
    int numMoves, numInfos = 0, row, col, move;
    PIECE_T piece;
    for (row = 0; row <= 7; row++) {
        for (col = 0; col <= 7; col++) {
            piece = getSquareWithFlags(BPARAMS);
            if (piece == EMPTY_SQUARE) continue;
            if (IS_BLACK(piece) != isBlack) continue;
            (*board)[row][col] = piece & ~CAN_EN_PASSANT;       // Flag is no longer valid on our turn
            numMoves = getValidMoves(moves, noCheckTest, BPARAMS);
            for (move = 0; move < numMoves; move++) {
                moveInfo[numInfos + move].source = pack(row, col);
                moveInfo[numInfos + move].dest = moves[move];   // Can include flags
                moveInfo[numInfos + move].value = -EVAL_IN_CHECK;
            }
            numInfos += numMoves;
        }
    }
    return numInfos;
}

int countSquareAttackersOfColor(BOOL isBlack, PACKED_SQUARE targetSquare, BOARDPTR_T(board)) {
    // How many pieces of the specified color are attacking the specified square.
    // Used for center control, and for check detection
    int moveCount = 0;
    int move;
    // Find all valid moves for this color (no lookahead)
    MOVEINFO moveInfo[MAX_POSSIBLE_MOVES];
    int numInfos = getAllMoves(moveInfo, isBlack, TRUE, board);
    // See if any move targets the specified square
    for (move = 0; move < numInfos; move++) {
        if (NO_FLAGS(moveInfo[move].dest) == targetSquare) {
            ++moveCount;
#if defined(DEBUG_MATES) || defined(DEBUG_CHECKS) || defined(DEBUG_EVALUATION)
            printf("Found potental %s attack ", COLOR_NAME(isBlack)); printMove(moveInfo[move].source, targetSquare, board);printf("\n");
#endif
        }
    }
    return moveCount;
}

BOOL inCheckKnownKingSquare(PACKED_SQUARE kingSquare, BOARDPTR_T(board)) {
    // Is the specified square, containing a king, in check?
    BOOL attackerIsBlack = !IS_BLACK(getSquare(board, unpackRow(kingSquare), unpackCol(kingSquare)));
    return (countSquareAttackersOfColor(attackerIsBlack, kingSquare, board) > 0);
}

BOOL inCheck(BOOL isBlack, BOARDPTR_T(board)) {
    // Is the specified color in check?
    return inCheckKnownKingSquare(kingsSquare(isBlack, board), board);
}

int evaluatePosition(BOOL isBlack, BOARDPTR_T(board)) {
    // Simple evaluator: what are the pieces on the board worth
    // This drives beginner play resembling "capture the biggest piece you can"
    int value = 0, row, col;
    PIECE_T piece, pieceType;
    for (row = 0; row <= 7; row++) {
        for (col = 0; col <= 7; col++) {
            piece = getSquare(BPARAMS);
            if (piece == EMPTY_SQUARE) continue;
            pieceType = PIECE(piece);
            value += ((isBlack == IS_BLACK(piece)) ? 1 : -1) * pieceValues[pieceType];
        }
    }

    // Check how many of the four center squares d4, d5, e4, e5 we control
    // NOTE: Weight this carefully against value of material
    int controlCount = 0;
    if (countSquareAttackersOfColor(isBlack, SQUARE_D4, board) > 0) ++controlCount;
    if (countSquareAttackersOfColor(isBlack, SQUARE_D5, board) > 0) ++controlCount;
    if (countSquareAttackersOfColor(isBlack, SQUARE_E4, board) > 0) ++controlCount;
    if (countSquareAttackersOfColor(isBlack, SQUARE_E5, board) > 0) ++controlCount;
    value += controlCount / 2;

    return value;
}

void copyBoard(BOARDPTR_T(board), BOARDPTR_T(tryBoard))
{
    // Copy try board to active board
    memcpy(board, tryBoard, sizeof(*tryBoard));
}

void tryMove(BOARDPTR_T(newBoard), PACKED_SQUARE dest, BARGS) {
    // Tries moving the piece at row, col to dest
    // Copy active board to try board
    copyBoard(newBoard, board);
    // Apply move on try board. Note that this will also remove
    // captured pieces from the board, but does not track them.
    PIECE_T piece = getSquare(BPARAMS);
    BOOL black = IS_BLACK(piece);
    int destCol = unpackCol(dest);
    int destRow = unpackRow(dest);
    int oldPiece = getSquare(newBoard, destRow, destCol);
    // Simplified pawn promotion to a queen or a knight
    // This leaves out some edge cases where a rook or bishop is preferable to a queen to avoid stalemate
    if (piece == WHITE_PAWN && destRow == 0) piece = FLAGS(dest & IS_KNIGHT_PROMO) ? WHITE_KNIGHT : WHITE_QUEEN;
    if (piece == BLACK_PAWN && destRow == 7) piece = FLAGS(dest & IS_KNIGHT_PROMO) ? BLACK_KNIGHT : BLACK_QUEEN;
    (*newBoard)[row][col] = EMPTY_SQUARE;
    if (FLAGS(dest) & IS_EN_PASSANT) {
        // en passant capture removes the piece with CAN_EN_PASSANT from the board
        int blackSign = ADVANCE_DIRECTION(black);
        (*newBoard)[destRow][destCol] = EMPTY_SQUARE;
        (*newBoard)[destRow + blackSign][destCol] = piece;
#ifdef DEBUG_EN_PASSANT
        printf("Pawn at "); printSquareName(pack(row, col));
        printf(" trying en passant capture at "); printSquareName(dest); printf("\n");
#endif
    } else {
        // If there's a CAN_EN_PASSANT flag or a CANNOT_CASTLE flag on the DESTINATION, copy to PIECE
        (*newBoard)[destRow][destCol] = piece | (FLAGS(dest) & (CAN_EN_PASSANT | CANNOT_CASTLE));
#ifdef DEBUG_EN_PASSANT
        if (FLAGS(dest) & CAN_EN_PASSANT) {
            printf("Pawn moving to "); printSquareName(dest); printf(" is vulnerable to en passant.\n");
        }
#endif
        if (FLAGS(dest) & IS_CASTLE) {
            // Castling moves two pieces. Only the king move is packed, so we also have to move the rook.
            BOOL queenSide = (destCol < 4);
#ifdef DEBUG_CASTLING
            printf("Trying to castle to "); printSquareName(dest); printf("\n");
#endif
            if (queenSide) {
                // Queenside
                (*newBoard)[destRow][3] = (*newBoard)[destRow][0] | CANNOT_CASTLE;
                (*newBoard)[destRow][0] = EMPTY_SQUARE;
            } else {
                // Kingside
                (*newBoard)[destRow][5] = (*newBoard)[destRow][7] | CANNOT_CASTLE;
                (*newBoard)[destRow][7] = EMPTY_SQUARE;
            }
        }
    }
#ifdef DEBUG_CHECKS
    if (oldPiece == BLACK_KING || oldPiece == WHITE_KING) {
        printf("\nFATAL: Captured king at ");printSquareName(dest);printf("\n");
        exit(0);    // For breakpoint
    }
#endif
}

int evalSort(const void* p1, const void* p2) {
    const MOVEINFO* m1 = (MOVEINFO*)p1;
    const MOVEINFO* m2 = (MOVEINFO*)p2;
    return (m2->value - m1->value);
}

void findBestMove(BOOL isBlack, PACKED_SQUARE* from, PACKED_SQUARE* to, BOARDPTR_T(board), int lookAhead) {
    // Returns coordinates of piece in *from, and its destination in *to.
    // To look ahead, creates a temporary board and finds the best move
    // of the opposing side, for each possible move.
    int numInfos, move = 0, validMoves = 0;
    PIECE_T tryBoard[8][8];
    MOVEINFO moveInfo[MAX_POSSIBLE_MOVES];
    // Get all the moves
    numInfos = getAllMoves(moveInfo, isBlack, FALSE, board);
    for (move = 0; move < numInfos; move++) {
        // Evaluate each move (moves that leave us in check remain -EVAL_IN_CHECK)
        PACKED_SQUARE source = moveInfo[move].source;
        PACKED_SQUARE dest = moveInfo[move].dest;
        int row = unpackRow(source);
        int col = unpackCol(source);
        // Make the move our temporary board
#ifdef DEBUG_MATES
        printf("Trying "); printMove(source, dest, board); printf("\n");
#endif
        copyBoard(&tryBoard, board);
        tryMove(&tryBoard, dest, &tryBoard, row, col);
        if (inCheck(isBlack, &tryBoard)) {
            // Discard moves that leave us in check
#ifdef DEBUG_CHECKS
            printf("Move "); printMove(source, dest, board); printf(" would leave %s in check\n", COLOR_NAME(isBlack));
#endif
            moveInfo[move].value = VALUE_ILLEGAL;
            continue;
        }
        // Look ahead some number of moves. To simplify, we check only the best known move
        // for each player. laBlack indicates whether the result is for us or our opponent.
        // NOTE: If lookAhead is zero, no recursion; this is an important part of the algorithm.
        PACKED_SQUARE laSource = SQUARE_NONE;
        PACKED_SQUARE laDest = SQUARE_NONE;
        BOOL laBlack = isBlack;
        int laTurn;
        for (laTurn = lookAhead; laTurn > 0; laTurn--) {
#ifdef DEBUG_MATES
            printf("Looking ahead [%d] as %s\n", laTurn, COLOR_NAME(!laBlack));
#endif
            laBlack = !laBlack;
            findBestMove(laBlack, &laSource, &laDest, &tryBoard, laTurn - 1);
            if (laSource == SQUARE_NONE) break;    // No possible move (stalemate or checkmate)
            // Put the best available move on the board
            tryMove(&tryBoard, laDest, &tryBoard, unpackRow(laSource), unpackCol(laSource));
        }
        // tryBoard will have the future state of the board after all lookahead
        // If opponent is in check, that's good. Mate is even better. Stalemate
        // Is bad. If we're in check (and this isn't before recursion), then the
        // contemplated move is still valid but has a bad expected outcome.
        moveInfo[move].value = 0;
        BOOL lastIsInCheck = inCheck(laBlack, &tryBoard);
        if (laSource == SQUARE_NONE) {
            // No moves; stalemate or checkmate
            if (lastIsInCheck) {
                // Checkmate. If it's the opponent, this is a good path; for us, bad
#ifdef DEBUG_MATES
                printf("Found checkmate of %s\n", COLOR_NAME(laBlack));
#endif
                moveInfo[move].value = (laBlack != isBlack) ? VALUE_CHECKMATE : -VALUE_CHECKMATE;
            } else {
                // Stalemate. If it's the opponent, this is a bad path
#ifdef DEBUG_MATES
                printf("Found stalemate of %s\n", COLOR_NAME(laBlack));
#endif
                if (laBlack != isBlack) moveInfo[move].value = -VALUE_STALEMATE;
            }
        } else {
            // If we're in check after all is said and done, that's a negative
            // If they're in check after all is said and done, that's a positive
        }
        moveInfo[move].value += evaluatePosition(laBlack, &tryBoard) * ((laBlack == isBlack) ? 1 : -1);
#ifdef DEBUG_CASTLING
        // Test code to always castle when we can
        if (FLAGS(dest) & IS_CASTLE) {
            printf("Forcing castling\n");
            moveInfo[move].value = EVAL_FORCE;
        }
#endif
#ifdef DEBUG_PROMOTION
        // Test code to always promote to knight when we can
        if (FLAGS(dest) & IS_KNIGHT_PROMO) {
            printf("Forcing promotion to knight\n");
            moveInfo[move].value = EVAL_FORCE;
        }
#endif
        ++validMoves;
    }
    if (validMoves > 0) {
        // Sort the moves for highest result evaluation
        qsort(moveInfo, numInfos, sizeof(MOVEINFO), evalSort);
#ifdef DEBUG_MATES
        for (move = 0; move < numInfos; move++) {
            printf("Trying ");
            printMove(moveInfo[move].source, moveInfo[move].dest, board);
            printf(" Value: %d\n", moveInfo[move].value);
        }
#endif
        // Find the range of moves with the highest result
        int topValue = moveInfo[0].value;
        if (topValue > VALUE_ILLEGAL) {
            int topCount = 0;
            while ((topCount < numInfos) && (moveInfo[topCount].value == topValue)) ++topCount;
            // Select randomly from the highest results
            move = rand() % topCount;
#ifdef DEBUG_MATES
            printf("Chose #%d of %d best moves.\n", move, topCount);
#endif
            *from = moveInfo[move].source;
            *to = moveInfo[move].dest;
        } else {
#ifdef DEBUG_MATES
            printf("No legal moves.\n");
#endif
            *from = *to = SQUARE_NONE;
        }
    } else {
#ifdef DEBUG_MATES
        printf("No valid moves.\n");
#endif
        *from = *to = SQUARE_NONE;
    }
}

void printSpacedChar(char c) {
    printf("%s%c%s", COLUMN_SPACER, c, COLUMN_SPACER);
}

void printBlackSpacedChar(char c) {
    printf("%s%c%s", COLUMN_SPACER_BLACK_LEFT, c, COLUMN_SPACER_BLACK_RIGHT);
}

void printWhiteSpacedChar(char c) {
    printf("%s%c%s", COLUMN_SPACER_WHITE_LEFT, c, COLUMN_SPACER_WHITE_RIGHT);
}

void printSpaced(char* text) {
    char c;
    while ('\0' != (c = *text++)) printSpacedChar(c);
}

void printBlackSpaced(char* text) {
    char c;
    while ('\0' != (c = *text++)) printBlackSpacedChar(c);
}

void printWhiteSpaced(char* text) {
    char c;
    while ('\0' != (c = *text++)) printWhiteSpacedChar(c);
}

void printBoard(BOARDPTR_T(board)) {
    int row, col, piece, pieceType, pieceColor;
    for (row = 0; row <= 7; row++) {
        printf("\n%d|", (7 - row) + 1);
        for (col = 0; col <= 7; col++) {
            BOOL isBlackSquare = (col & 1) ^ (row & 1);
            piece = getSquare(BPARAMS);
            if (piece == EMPTY_SQUARE)
                piece +=  isBlackSquare ? COLOR_BLACK : COLOR_WHITE;
#ifdef USE_ESCAPE_CODES
            char* columnSpacerLeft = isBlackSquare ? COLUMN_SPACER_BLACK_LEFT : COLUMN_SPACER_WHITE_LEFT;
            char* columnSpacerRight = isBlackSquare ? COLUMN_SPACER_BLACK_RIGHT : COLUMN_SPACER_WHITE_RIGHT;
            if (isBlackSquare) {
                printf("\e[42m%s%s%s\e[0m", columnSpacerLeft, pieceSymbols[piece], columnSpacerRight);
            } else {
                printf("\e[47m%s%s%s\e[0m", columnSpacerLeft, pieceSymbols[piece], columnSpacerRight);
            }
#else
            if (isBlackSquare)
                printBlackSpaced(pieceSymbols[piece]);
            else
                printWhiteSpaced(pieceSymbols[piece]);
#endif
        }
    }
    printf("\n  "); printSpaced("--------");
    printf("\n  "); printSpaced("abcdefgh");
    printf("\n");
}

int main() {
    PACKED_SQUARE from = 0, to = 0;
	// Choose an opening. These are just the first move for white and for black
	int opening = rand() % (sizeof(openings) / sizeof(OPENING));
	printf("Chose the '%s' opening...\n", openings[opening].openingName);
    // Set up the initial board
    srand(time(0));
    memcpy(&chessBoard, &initial_board, sizeof(chessBoard));
    eraseBoard();
    printf("\n\n"); printBoard(&chessBoard);
    // Play until no moves.
    BOOL isBlack = FALSE;
    int move = 1;
	// Play the game (including the opening)
    while (from != SQUARE_NONE) {
		if (move == 1 && opening >= 0) {
			from = isBlack ? openings[opening].blackMove.source : openings[opening].whiteMove.source;
			to = isBlack ? openings[opening].blackMove.dest : openings[opening].whiteMove.dest;
		} else {
        	findBestMove(isBlack, &from, &to, &chessBoard, LOOKAHEAD);
		}
        if (from != SQUARE_NONE) {
            eraseBoard();
            // Make the best move
            int attacker = getSquare(&chessBoard, unpackRow(from), unpackCol(from));
            int target = getSquare(&chessBoard, unpackRow(to), unpackCol(to));
            printf("\n%3d. ", move);
            if (isBlack) printf("          ");
            printMove(from, to, &chessBoard); printf("\n");
            tryMove(&chessBoard, to, &chessBoard, unpackRow(from), unpackCol(from));
            printBoard(&chessBoard);
            // Draw detection
            if (target != EMPTY_SQUARE || PIECE(attacker) == PIECE_PAWN) {
                // Reset 75 move draw
                ruleOf75 = 0;
            } else {
                ++ruleOf75;
                if (ruleOf75 == 150) {
                    // If checkmate, game over as checkmate
                    // Otherwise, game over as draw. For now,
                    // no need to distinguish.
                    printf("Invoking 75 move rule.\n");
                    break;
                }
            }
            // Switch sides
            isBlack = !isBlack;
            if (!isBlack) ++move;
        }
    }
    if (from == SQUARE_NONE) {
        printf(inCheck(isBlack, &chessBoard) ? "Checkmate.\n" : "Stalemate.\n");
    }
    printf("Game over.\n");
    return 0;
}
