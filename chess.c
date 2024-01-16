#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>

#define BOOL int
#define FALSE 0
#define TRUE (!FALSE)

// How the board will look in terminal window
// If you're not using a chess compatible font, comment out USE_CHESS_FONT
// If your terminal window doesn't do VT100 codes, comment out USE_ESCAPE
// Uncomment DONT_SCROLL if you want the board always drawn at top of window
// Set COLUMN_SPACER to be "" or " " based on whatever looks good for you.
#define USE_CHESS_FONT
#define USE_ESCAPE_CODES
//#define DONT_SCROLL
//#define COLUMN_SPACER ""
#define COLUMN_SPACER " "

// Debugging
//#define DEBUG_MATES
//#define DEBUG_EN_PASSANT

//
// Constants
//

// How many (half) moves to look ahead. At 4 things get quite slow.
#define LOOKAHEAD 3

// Additional flags on pieces to indicate that they're
// vulnerable to en passant, ineligible to castle, or
// promoting to a different piece. TODO: Implement castling
#define CAN_EN_PASSANT  (1<<8)
#define IS_EN_PASSANT	(1<<9)
#define CANNOT_CASTLE   (1<<10)
#define FLAGS_START     (1<<8)
#define NO_FLAGS(x)     ((x) % FLAGS_START)
#define FLAGS(x)		((x) - NO_FLAGS(x))

// Color Bias
#define COLOR_WHITE     0
#define COLOR_BLACK     (1<<3)
#define IS_BLACK(x)     ((NO_FLAGS(x) / COLOR_BLACK) == 1)
#define ADVANCE_DIRECTION(color)	((color) ? 1 : -1)

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
#define MAX_POSSIBLE_MOVES 1024
#define EVAL_IN_CHECK   10000
#define BOARDPTR_T(p)   int (*p)[8][8]
#define BARGS           BOARDPTR_T(board), int row, int col
#define BPARAMS         board, row, col

#define SQUARE_NONE    -1
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

static char* pieceLabels[]  = { " ", "P", "N", "B", "R", "Q", "K", " ",
                                ".", "p", "n", "b", "r", "q", "k", " " };

#ifdef USE_CHESS_FONT
static char* pieceSymbols[] = { " ", "♙", "♘", "♗", "♖", "♕", "♔", " ",
                                "☰", "♟", "♞", "♝", "♜", "♛", "♚", " " };
#else
static char** pieceSymbols = &pieceLabels[0];
#endif

int inCheck(int black, BOARDPTR_T(board));
void tryMove(BOARDPTR_T(newBoard), int dest, BARGS);
static int ruleOf75 = 0;

//
// Basic square operations
//

// Are the square coordinates valid?
BOOL isSquare(BARGS) { return (row >= 0 && row <= 7 && col >= 0 && col <= 7); }
// What's on the square (without flags, for now)
BOOL getSquare(BARGS) { return NO_FLAGS((*board)[row][col]); }
// What's on the square (with flags)
BOOL getSquareWithFlags(BARGS) { return (*board)[row][col]; }
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
int unpackRow(int square) { return NO_FLAGS(square) / 8; }
int unpackCol(int square) { return NO_FLAGS(square) % 8; }
// Name of a square
void printSquareName(int square) {
    int letter = (7 - unpackCol(square)) + 'a';
    printf("%c%d", letter, (7-unpackRow(square))+1);
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
void printMove(int source, int dest, BOARDPTR_T(board)) {
    int attacker = getSquare(board, unpackRow(source), unpackCol(source));
    int target = getSquare(board, unpackRow(dest), unpackCol(dest));
    printf("%s", pieceLabels[PIECE(attacker)]);
    printSquareName(source);
    if (target != EMPTY_SQUARE) printf("x");
	if (dest & IS_EN_PASSANT) {
		// Report the destination of the attacker rather than the victim
		printSquareName(dest + ADVANCE_DIRECTION(IS_BLACK(attacker)) * 8);
		printf("e.p.");
    } else {
		printSquareName(dest);
	}
    {
        // Test for pawn promotion
        int tryBoard[8][8], afterPiece;
        tryMove(&tryBoard, dest, board, unpackRow(source), unpackCol(source));
        afterPiece = getSquare(&tryBoard, unpackRow(dest), unpackCol(dest));
        if (PIECE(attacker) == PIECE_PAWN && PIECE(afterPiece) != PIECE_PAWN) {
            printf("%s", pieceLabels[PIECE(afterPiece)]);
        }
        // Test for check
        BOOL attackerBlack = IS_BLACK(attacker);
        if (inCheck(!attackerBlack, &tryBoard)) {
            //  Check!
            printf("+");
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
    int blackSign = ADVANCE_DIRECTION(black);
    int checkRow, checkCol;
    int deltaRow, deltaCol, delta, piece;
    switch (PIECE(myPiece)) {
        // TODO: Test for moves that are illegal due to check
        case    PIECE_PAWN:
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
            // Pawn's first move can be ahead two
            if (row == (black ? 1 : 6)) {
                checkRow += blackSign;
                if (isEmpty(board, checkRow, col)) {
                    // Forward move two
                    dests[numDests++] = pack(checkRow, col) | CAN_EN_PASSANT;
                }
            }
            // TODO: Update to allow for pawn promotion to Queen OR Knight
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
            piece = getSquareWithFlags(BPARAMS);
            if (piece == EMPTY_SQUARE) continue;
            if (IS_BLACK(piece) != black) continue;
			(*board)[row][col] = piece & ~CAN_EN_PASSANT;	// Flag is no longer valid on our turn
            numMoves = getValidMoves(moves, BPARAMS);
            for (move = 0; move < numMoves; move++) {
                moveInfo[numInfos + move].source = pack(row, col);
                moveInfo[numInfos + move].dest = moves[move];	// Can include flags
                moveInfo[numInfos + move].value = -EVAL_IN_CHECK;
            }
            numInfos += numMoves;
        }
    }
    return numInfos;
}

BOOL inCheck(int black, BOARDPTR_T(board)) {
    // Is the specified color in check?
    int kingSquare = SQUARE_NONE, row, col, piece, move, ourKing;
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
        if (kingSquare != SQUARE_NONE) break;
    }
    // Find all valid enemy moves
    MOVEINFO moveInfo[MAX_POSSIBLE_MOVES];
    int numInfos = getAllMoves(moveInfo, !black, board);
    // See if any enemy move targets the king's square
    for (move = 0; move < numInfos; move++) {
        if (moveInfo[move].dest == kingSquare) {
#ifdef DEBUG_MATES
            printf("\nFound check from ");printSquareName(moveInfo[move].source);printf("\n");
#endif
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

void copyBoard(BOARDPTR_T(board), BOARDPTR_T(tryBoard))
{
    // Copy try board to active board
    memcpy(board, tryBoard, sizeof(*tryBoard));
}

void tryMove(BOARDPTR_T(newBoard), int dest, BARGS) {
    // Tries moving the piece at row, col to dest
    // Copy active board to try board
    copyBoard(newBoard, board);
    // Apply move on try board. Note that this will also remove
    // captured pieces from the board, but does not track them.
    int piece = getSquare(BPARAMS);
    BOOL black = IS_BLACK(piece);
    int destCol = unpackCol(dest);
    int destRow = unpackRow(dest);
    // Trivial pawn promotion - always to a queen
    // TODO: Update to allow for pawn promotion to Queen OR Knight
    if (piece == WHITE_PAWN && destRow == 0) piece = WHITE_QUEEN;
    if (piece == BLACK_PAWN && destRow == 7) piece = BLACK_QUEEN;
    (*newBoard)[row][col] = EMPTY_SQUARE;
	if (FLAGS(dest) & IS_EN_PASSANT) {
    	int blackSign = ADVANCE_DIRECTION(black);
    	(*newBoard)[destRow][destCol] = EMPTY_SQUARE;
    	(*newBoard)[destRow+blackSign][destCol] = piece;
#ifdef DEBUG_EN_PASSANT
		printf("Pawn at "); printSquareName(pack(row, col));
		printf(" trying en passant capture at "); printSquareName(dest); printf("\n");
#endif
	} else {
    	(*newBoard)[destRow][destCol] = piece | (FLAGS(dest) & CAN_EN_PASSANT);
	}
#ifdef DEBUG_EN_PASSANT
	if (FLAGS(dest) & CAN_EN_PASSANT) {
		printf("Pawn moving to "); printSquareName(dest); printf(" is vulnerable to en passant.\n");
	}
#endif
}

int evalSort(const void* p1, const void* p2) {
    const MOVEINFO* m1 = (MOVEINFO*)p1;
    const MOVEINFO* m2 = (MOVEINFO*)p2;
    return (m2->value - m1->value);
}

void findBestMove(BOOL black, int* from, int* to, BOARDPTR_T(board), int lookAhead) {
    // Returns coordinates of piece in *from, and its destination in *to.
    // To look ahead, creates a temporary board and finds the best move
    // of the opposing side, for each possible move.
    int numInfos, tryBoard[8][8], move = 0, validMoves = 0;
    MOVEINFO moveInfo[MAX_POSSIBLE_MOVES];
    // Get all the moves
    numInfos = getAllMoves(moveInfo, black, board);
    // Evaluate each move (moves that leave us in check remain -EVAL_IN_CHECK)
    for (move = 0; move < numInfos; move++)
    {
        BOOL laBlack = black;
        int laTurn;
        int source = moveInfo[move].source;
        int dest = moveInfo[move].dest;
#ifdef DEBUG_MATES
        printf("Trying "); printMove(source, dest, board); printf("\n");
#endif
        copyBoard(&tryBoard, board);
        for (laTurn = lookAhead; laTurn >= 0; laTurn--) {
            int row = unpackRow(source);
            int col = unpackCol(source);
            tryMove(&tryBoard, dest, &tryBoard, row, col);
            if (inCheck(laBlack, &tryBoard)) break;
            // This move is OK
            if (laTurn > 0) {
                // Simple look-ahead - recurse as other side and make best move
#ifdef DEBUG_MATES
                printf("Looking ahead [%d]\n", lookAhead - laTurn + 1);
#endif
                laBlack = !laBlack;
                findBestMove(laBlack, &source, &dest, &tryBoard, laTurn-1);
                if (source == SQUARE_NONE) break;    // No possible move
            }
        }
        // tryBoard will have the future state of the board after all lookahead
        // If opponent is in check, that's good. If we're in check, then the
        // contemplated move is still valid unless it happened before lookahead
        if (laTurn == lookAhead && inCheck(black, &tryBoard)) continue;
        moveInfo[move].value = evaluatePosition(black, &tryBoard);
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
        printf("No valid moves.\n");
#endif
        *from = *to = SQUARE_NONE;
    }
}

void printSpacedChar(char c) {
    printf("%s%c%s", COLUMN_SPACER, c, COLUMN_SPACER);
}

void printSpaced(char* text) {
    char c;
    while ('\0' != (c = *text++)) printSpacedChar(c);
}

void printBoard(BOARDPTR_T(board)) {
    int row, col, piece, pieceType, pieceColor, squareBlack = FALSE;
    printf("  "); printSpaced("hgfedcba");
    printf("\n  "); printSpaced("--------");
    for (row = 0; row <= 7; row++) {
        printf("\n%d|", (7 - row) + 1);
        for (col = 0; col <= 7; col++) {
            piece = getSquare(BPARAMS);
            if (piece == EMPTY_SQUARE)
                piece += squareBlack ? COLOR_BLACK : COLOR_WHITE;
#ifdef USE_ESCAPE_CODES
            if (IS_BLACK(piece)) {
                printf("%s\e[0;30m%s\e[0m%s", COLUMN_SPACER, pieceSymbols[piece], COLUMN_SPACER);
            } else {
                printf("%s\e[0;90m%s\e[0m%s", COLUMN_SPACER, pieceSymbols[piece], COLUMN_SPACER);
            }
#else
            printSpaced(pieceSymbols[piece]);
#endif
            squareBlack = !squareBlack;
        }
        squareBlack = !squareBlack;
    }
    printf("\n");
}

int main() {
    int from = 0, to = 0;
    // Set up the initial board
    srand(time(0));
    memcpy(&chessBoard, &initial_board, sizeof(chessBoard));
    eraseBoard();
    printf("\n\n"); printBoard(&chessBoard);
    // Play until no moves.
    int black = FALSE, move = 1;
    while (from != SQUARE_NONE) {
        findBestMove(black, &from, &to, &chessBoard, LOOKAHEAD);
        if (from != SQUARE_NONE) {
            eraseBoard();
            // Make the best move
            int attacker = getSquare(&chessBoard, unpackRow(from), unpackCol(from));
            int target = getSquare(&chessBoard, unpackRow(to), unpackCol(to));
            printf("\n%3d. ", move);
            if (black) printf("          ");
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
            black = !black;
            if (!black) ++move;
        }
    }
    if (from == SQUARE_NONE) {
        printf(inCheck(black, &chessBoard) ? "Checkmate.\n" : "Stalemate.\n");
    }
    printf("Game over.\n");
    return 0;
}
