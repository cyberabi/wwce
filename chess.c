#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#define BOOL int
#define FALSE 0
#define TRUE (!FALSE)

//
// Constants
//

// Additional flags on pieces to indicate that they're
// vulnerable to en passant, ineligible to castle, or
// promoting to a different piece. TODO: Implement
#define CAN_EN_PASSANT  100
#define CANNOT_CASTLE   1000
#define PROMOTING       10000
#define FLAGS_START     100
#define NO_FLAGS(x)     ((x) % FLAGS_START)

// Color Bias
#define COLOR_WHITE     0
#define COLOR_BLACK     10
#define IS_BLACK(x)     (NO_FLAGS(x)/COLOR_BLACK == 1)

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

static const int pieceValues[] = {
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
#define BOARDPTR_T(p)   int (*p)[8][8]
#define BARGS           BOARDPTR_T(board), int row, int col
#define BPARAMS         board, row, col

#define EMPTY_SQUARE    0
static const int initial_board[8][8] =
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

static int chessBoard[8][8];
typedef struct {
    int source;
    int dest;
    int value;
}   MOVEINFO;

static wchar_t* pieceSymbols = L" PNBRQK   .pnbrqk";
int inCheck(int black, BOARDPTR_T(board));
void tryMove(BOARDPTR_T(newBoard), int dest, BARGS);

//
// Basic square operations
//

// Are the square coordinates valid?
BOOL isSquare(BARGS) { return (row >= 0 && row <= 7 && col >= 0 && col <= 7); }
// What's on the square (without flags, for now)
BOOL getSquare(BARGS) { return NO_FLAGS((*board)[row][col]); }
// Is the square empty?
BOOL isEmpty(BARGS) { return (getSquare(BPARAMS) == EMPTY_SQUARE); }
// Is the square occupied by an enemy?
BOOL isEnemy(int myPiece, BARGS) {
    int target = getSquare(BPARAMS);
    return ((target != EMPTY_SQUARE) && (IS_BLACK(target) != IS_BLACK(myPiece)));
}
// Disregarding bounds, check, etc, is the square valid to move to?
BOOL isDest(int myPiece, BARGS) { return isEmpty(BPARAMS) || isEnemy(myPiece, BPARAMS); }
// Considering bounds, is the square valid to move to?
BOOL isValid(int myPiece, BARGS) { return isSquare(BPARAMS) && isDest(myPiece, BPARAMS); }
// Pack a row and column into an integer
int pack(int row, int col) { return row * 8 + col; }
// Unpack an integer to a row or column
int unpackRow(int square) { return square / 8; }
int unpackCol(int square) { return square % 8; }
// Name of a square
void printSquareName(int square) {
    int letter = (7 - unpackCol(square)) + 'A';
    printf("%c%d", letter, unpackRow(square)+1);
}
// Move description
void printMove(int source, int dest, BOARDPTR_T(board)) {
    int attacker = getSquare(board, unpackRow(source), unpackCol(source));
    int target = getSquare(board, unpackRow(dest), unpackCol(dest));
    printSquareName(source);
    printf("%c", (target == EMPTY_SQUARE) ? '-' : 'x');
    printSquareName(dest);
    {
        // Test for check
        int tryBoard[8][8];
        tryMove(&tryBoard, dest, board, unpackRow(source), unpackCol(source));
        BOOL attackerBlack = IS_BLACK(attacker);
        if (inCheck(!attackerBlack, &tryBoard)) {
            //  Check!
            printf(" ch");
        }
    }
}

//
// Basic piece operations
//

int getValidMovesAsPiece(int* dests, int asPiece, BARGS) {
    // Ignores what is actually on the square to simplify queen logic
    int numDests = 0;
    int myPiece = asPiece;
    BOOL black = IS_BLACK(myPiece);
    int blackSign = black ? 1 : -1;
    int checkRow, checkCol;
    int deltaRow, deltaCol, delta, piece;
    switch (PIECE(myPiece)) {
        // TODO: Test for moves that are illegal due to check
        case    PIECE_PAWN:
            // TODO: Implement rules for promotion and en passant
            checkRow = row + blackSign;
            if (isSquare(board, checkRow, col) && isEmpty(board, checkRow, col)) {
                // Forward move
                dests[numDests++] = pack(checkRow, col);
            }
            if (isSquare(board, checkRow, col+1) && isEnemy(myPiece, board, checkRow, col+1)) {
                // Capture toward A
                dests[numDests++] = pack(checkRow, col+1);
            }
            if (isSquare(board, checkRow, col-1) && isEnemy(myPiece, board, checkRow, col-1)) {
                // Capture toward H
                dests[numDests++] = pack(checkRow, col-1);
            }
            // Pawn's first move can be ahead two
            if (row == (black ? 1 : 6)) {
                checkRow += blackSign;
                if (isEmpty(board, checkRow, col)) {
                    // Forward move two
                    dests[numDests++] = pack(checkRow, col);
                }
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
                        // Empty or capture
                        dests[numDests++] = pack(checkRow, checkCol);
                        if (!isEmpty(board, checkRow, checkCol)) break;
                        ++delta;
                    }
                }
            }
            break;
        case    PIECE_QUEEN:
            // Sum of possible bishop and rook moves
            numDests = getValidMovesAsPiece(dests, black ? BLACK_BISHOP : WHITE_BISHOP, BPARAMS);
            numDests += getValidMovesAsPiece(dests + numDests, black ? BLACK_ROOK : WHITE_ROOK, BPARAMS);
            break;
        case    PIECE_KING:
            // TODO: Implement rules for castling (no prior moves, castling through check)
            for (deltaRow = -1; deltaRow <= 1; deltaRow++) {
                for (deltaCol = -1; deltaCol <= 1; deltaCol++) {
                    if (deltaRow == 0 && deltaCol == 0) continue;
                    checkRow = row + deltaRow;
                    checkCol = col + deltaCol;
                    if (isValid(myPiece, board, checkRow, checkCol)) {
                        // Empty or capture
                        dests[numDests++] = pack(checkRow, checkCol);
                    }
                }
            }
            break;
        default:
            break;
    }
    return numDests;
}

int getValidMoves(int* dests, BARGS) {
    // Uses the contents of the specified square
    return getValidMovesAsPiece(dests, getSquare(BPARAMS), BPARAMS);
}

int getAllMoves(MOVEINFO* moveInfo, BOOL black, BOARDPTR_T(board)) {
    // Find all moves the specified color can make
    // Passed array should be 1024 in size for 16 max
    // pieces x 64 max moves each
    int moves[64], numMoves, numInfos = 0, row, col, move, piece;
    for (row = 0; row <= 7; row++) {
        for (col = 0; col <= 7; col++) {
            piece = getSquare(BPARAMS);
            if (piece == EMPTY_SQUARE) continue;
            if (IS_BLACK(piece) != black) continue;
            numMoves = getValidMoves(moves, BPARAMS);
            for (move = 0; move < numMoves; move++) {
                moveInfo[numInfos + move].source = pack(row, col);
                moveInfo[numInfos + move].dest = moves[move];
                moveInfo[numInfos + move].value = -10000;
            }
            numInfos += numMoves;
        }
    }
    return numInfos;
}

BOOL inCheck(int black, BOARDPTR_T(board)) {
    // Is the specified color in check?
    int kingSquare = -1, row, col, piece, move, ourKing;
    // Find our king
    ourKing = black ? BLACK_KING : WHITE_KING;
    for (row = 0; row <= 7; row++) {
        for (col = 0; col <= 7; col++) {
            piece = getSquare(BPARAMS);
            if (piece == ourKing) {
                kingSquare = pack(row, col);
                break;
            }
        }
        if (kingSquare != -1) break;
    }
    // Find all valid enemy moves
    MOVEINFO moveInfo[1024];
    int numInfos = getAllMoves(moveInfo, !black, board);
    // See if any enemy move targets the king's square
    for (move = 0; move < numInfos; move++) {
        if (moveInfo[move].dest == kingSquare) {
            //printf("Found check from ");printSquareName(moveInfo[move].source);printf("\n");
            return TRUE;
        }
    }
    return FALSE;
}

int evaluatePosition(int black, BOARDPTR_T(board)) {
    // Simple evaluator: what are the pieces on the board worth
    // This drives beginner play resambling "capture the biggest piece you can"
    int value = 0, row, col, piece, pieceType, pieceColor, move;
    // Find our king
    for (row = 0; row <= 7; row++) {
        for (col = 0; col <= 7; col++) {
            piece = getSquare(BPARAMS);
            if (piece == EMPTY_SQUARE) continue;
            pieceType = PIECE(piece);
            value += ((black == IS_BLACK(piece)) ? 1 : -1) * pieceValues[pieceType];
        }
    }
    return value;
}

void tryMove(BOARDPTR_T(newBoard), int dest, BARGS) {
    // Tries moving the piece at row, col to dest
    // Copy active board to try board
    memcpy(newBoard, board, sizeof(*newBoard));
    // Apply move on try board. Note that this will also remove
    // captured pieces from the board, but does not track them.
    int piece = getSquare(BPARAMS);
    (*newBoard)[row][col] = EMPTY_SQUARE;
    (*newBoard)[unpackRow(dest)][unpackCol(dest)] = piece;
}

void commitMove(BOARDPTR_T(board), BOARDPTR_T(tryBoard))
{
    // Copy try board to active board
    memcpy(board, tryBoard, sizeof(*tryBoard));
}

int evalSort(const void* p1, const void* p2) {
    const MOVEINFO* m1 = (MOVEINFO*)p1;
    const MOVEINFO* m2 = (MOVEINFO*)p2;
    return (m2->value - m1->value);
}

void findBestMove(BOOL black, int* from, int* to, BOARDPTR_T(board)) {
    int numInfos, tryBoard[8][8], move = 0, validMoves = 0;
    MOVEINFO moveInfo[1024];
    // Get all the moves
    numInfos = getAllMoves(moveInfo, black, board);
    // Evaluate each move (moves that leave us in check remain -10000)
    for (move=0; move < numInfos; move++)
    {
        int row = unpackRow(moveInfo[move].source);
        int col = unpackCol(moveInfo[move].source);
        tryMove(&tryBoard, moveInfo[move].dest, BPARAMS);
        if (inCheck(black, &tryBoard)) continue;
        moveInfo[move].value = evaluatePosition(black, &tryBoard);
        ++validMoves;
    }
    if (validMoves > 0) {
        // Sort the moves for highest result evaluation
        qsort(moveInfo, numInfos, sizeof(MOVEINFO), evalSort);
        //for (move = 0; move < numInfos; move++) {
        //    printf("Trying ");
        //    printMove(moveInfo[move].source, moveInfo[move].dest, board);
        //    printf(" Value: %d\n", moveInfo[move].value);
        //}
        // Find the range of moves with the highest result
        int topValue = moveInfo[0].value;
        int topCount = 0;
        while ((topCount < numInfos) && (moveInfo[topCount].value == topValue)) ++topCount;
        // Select randomly from the highest results
        move = rand() % topCount;
        //printf("Chose #%d of %d best moves.\n", move, topCount);
        *from = moveInfo[move].source;
        *to = moveInfo[move].dest;
    } else {
        printf("No valid moves.\n");
        *from = *to = -1;
    }
}

void printBoard(BOARDPTR_T(board)) {
    int row, col, piece, pieceType, pieceColor, squareBlack = FALSE;
    printf("   H  G  F  E  D  C  B  A ");
    for (row = 0; row <= 7; row++) {
        printf("\n%d ", row + 1);
        for (col = 0; col <= 7; col++) {
            piece = getSquare(BPARAMS);
            if (piece == EMPTY_SQUARE)
                piece += squareBlack ? COLOR_BLACK : COLOR_WHITE;
            printf(" %lc ", pieceSymbols[piece]);
            squareBlack = !squareBlack;
        }
    }
    printf("\n");
}

int main() {
    int from = 0, to = 0;
    // Set up the initial board
    memcpy(&chessBoard, &initial_board, sizeof(chessBoard));
    printBoard(&chessBoard);
    // Play until no moves.
    int black = FALSE;
    while (from != -1) {
        findBestMove(black, &from, &to, &chessBoard);
        if (from != -1) {
            // Make the best move
            if (black) printf("      ");
            printMove(from, to, &chessBoard); printf("\n");
            tryMove(&chessBoard, to, &chessBoard, unpackRow(from), unpackCol(from));
            printBoard(&chessBoard);
            // Switch sides
            black = !black;
        }
    }
    printf("Game over.\n");
    return 0;
}