#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h> // for abs()

// Board dimensions and constants.
#define EMPTY_CELL '.'
#define BOARD_DIM 8
#define MAX_LEGAL_MOVES 256

#define SIDE_WHITE 0
#define SIDE_BLACK 1

// Global state for castling rights.
int whiteKingMoved = 0, whiteQRookMoved = 0, whiteKRookMoved = 0;
int blackKingMoved = 0, blackQRookMoved = 0, blackKRookMoved = 0;

// Global en passant target (if any). Valid only for one move.
int enPassantTargetRow = -1, enPassantTargetCol = -1;

// Structure representing a chess move.
typedef struct {
    int src_row, src_col;
    int dst_row, dst_col;
    char promoteTo; // Nonzero if a pawn promotes (always to Queen here).
} ChessMove;

// Global board. White pieces are uppercase; Black pieces are lowercase.
char chessBoard[BOARD_DIM][BOARD_DIM];

/*
 * initialize_board:
 * Sets up the board to the standard chess starting position.
 */
void initialize_board() {
    // Black's back rank (row 0) and pawns (row 1)
    chessBoard[0][0] = 'r'; chessBoard[0][1] = 'n'; chessBoard[0][2] = 'b'; chessBoard[0][3] = 'q';
    chessBoard[0][4] = 'k'; chessBoard[0][5] = 'b'; chessBoard[0][6] = 'n'; chessBoard[0][7] = 'r';
    for (int i = 0; i < BOARD_DIM; i++)
        chessBoard[1][i] = 'p';
    // Empty squares (rows 2-5)
    for (int r = 2; r < 6; r++) {
        for (int c = 0; c < BOARD_DIM; c++)
            chessBoard[r][c] = EMPTY_CELL;
    }
    // White's pawns (row 6) and back rank (row 7)
    for (int i = 0; i < BOARD_DIM; i++)
        chessBoard[6][i] = 'P';
    chessBoard[7][0] = 'R'; chessBoard[7][1] = 'N'; chessBoard[7][2] = 'B'; chessBoard[7][3] = 'Q';
    chessBoard[7][4] = 'K'; chessBoard[7][5] = 'B'; chessBoard[7][6] = 'N'; chessBoard[7][7] = 'R';
}

/*
 * display_board:
 * Prints the board along with file (a-h) and rank (1-8) labels.
 */
void display_board() {
    printf("  a b c d e f g h\n");
    for (int r = 0; r < BOARD_DIM; r++) {
        printf("%d ", 8 - r);
        for (int c = 0; c < BOARD_DIM; c++) {
            printf("%c ", chessBoard[r][c]);
        }
        printf("\n");
    }
}

/*
 * isPieceWhite / isPieceBlack:
 * Helper functions to determine if a piece symbol belongs to White or Black.
 */
int isPieceWhite(char symbol) {
    return (symbol >= 'A' && symbol <= 'Z');
}

int isPieceBlack(char symbol) {
    return (symbol >= 'a' && symbol <= 'z');
}

/*
 * isInsideBoard:
 * Returns true if the (row, col) coordinates are within board limits.
 */
int isInsideBoard(int row, int col) {
    return (row >= 0 && row < BOARD_DIM && col >= 0 && col < BOARD_DIM);
}

/*
 * clone_board:
 * Copies the board state from source into dest.
 */
void clone_board(char source[BOARD_DIM][BOARD_DIM], char dest[BOARD_DIM][BOARD_DIM]) {
    for (int r = 0; r < BOARD_DIM; r++)
        for (int c = 0; c < BOARD_DIM; c++)
            dest[r][c] = source[r][c];
}

/*
 * save_state & restore_state:
 * These functions save and restore the complete game state (board, castling rights, en passant target)
 * so that we can search moves without permanently affecting the current game.
 */
void save_state(char boardSave[BOARD_DIM][BOARD_DIM],
    int* saveWhiteKing, int* saveWhiteQRook, int* saveWhiteKRook,
    int* saveBlackKing, int* saveBlackQRook, int* saveBlackKRook,
    int* saveEnPassantRow, int* saveEnPassantCol) {
    clone_board(chessBoard, boardSave);
    *saveWhiteKing = whiteKingMoved;
    *saveWhiteQRook = whiteQRookMoved;
    *saveWhiteKRook = whiteKRookMoved;
    *saveBlackKing = blackKingMoved;
    *saveBlackQRook = blackQRookMoved;
    *saveBlackKRook = blackKRookMoved;
    *saveEnPassantRow = enPassantTargetRow;
    *saveEnPassantCol = enPassantTargetCol;
}

void restore_state(char boardSave[BOARD_DIM][BOARD_DIM],
    int saveWhiteKing, int saveWhiteQRook, int saveWhiteKRook,
    int saveBlackKing, int saveBlackQRook, int saveBlackKRook,
    int saveEnPassantRow, int saveEnPassantCol) {
    clone_board(boardSave, chessBoard);
    whiteKingMoved = saveWhiteKing;
    whiteQRookMoved = saveWhiteQRook;
    whiteKRookMoved = saveWhiteKRook;
    blackKingMoved = saveBlackKing;
    blackQRookMoved = saveBlackQRook;
    blackKRookMoved = saveBlackKRook;
    enPassantTargetRow = saveEnPassantRow;
    enPassantTargetCol = saveEnPassantCol;
}

/*
 * execute_move_on_board:
 * Applies a move to the given board state, updating castling rights, handling en passant,
 * and moving the rook when castling.
 */
void execute_move_on_board(char boardState[BOARD_DIM][BOARD_DIM], ChessMove move) {
    // Clear en passant target (it lasts only one move).
    enPassantTargetRow = -1;
    enPassantTargetCol = -1;

    char pieceSymbol = boardState[move.src_row][move.src_col];

    // --- Castling ---
    if (tolower(pieceSymbol) == 'k' && abs(move.dst_col - move.src_col) == 2) {
        boardState[move.src_row][move.src_col] = EMPTY_CELL;
        boardState[move.dst_row][move.dst_col] = pieceSymbol;
        // Kingside castling: move the rook from h-file.
        if (move.dst_col > move.src_col) {
            boardState[move.src_row][7] = EMPTY_CELL;
            boardState[move.src_row][move.dst_col - 1] = (pieceSymbol == 'K' ? 'R' : 'r');
        }
        else { // Queenside castling.
            boardState[move.src_row][0] = EMPTY_CELL;
            boardState[move.src_row][move.dst_col + 1] = (pieceSymbol == 'K' ? 'R' : 'r');
        }
        // Update king's moved flag.
        if (pieceSymbol == 'K')
            whiteKingMoved = 1;
        else
            blackKingMoved = 1;
        return;
    }

    // --- En Passant Capture ---
    if (tolower(pieceSymbol) == 'p' &&
        abs(move.dst_col - move.src_col) == 1 &&
        boardState[move.dst_row][move.dst_col] == EMPTY_CELL) {
        boardState[move.src_row][move.src_col] = EMPTY_CELL;
        boardState[move.dst_row][move.dst_col] = pieceSymbol;
        // Remove the pawn that just made a two-step move.
        if (pieceSymbol == 'P')
            boardState[move.dst_row + 1][move.dst_col] = EMPTY_CELL;
        else
            boardState[move.dst_row - 1][move.dst_col] = EMPTY_CELL;
        return;
    }

    // --- Normal Move ---
    boardState[move.src_row][move.src_col] = EMPTY_CELL;
    if (move.promoteTo)
        pieceSymbol = move.promoteTo;
    boardState[move.dst_row][move.dst_col] = pieceSymbol;

    // Update castling rights if a king or rook moves.
    if (tolower(pieceSymbol) == 'k') {
        if (pieceSymbol == 'K')
            whiteKingMoved = 1;
        else
            blackKingMoved = 1;
    }
    if (tolower(pieceSymbol) == 'r') {
        if (pieceSymbol == 'R') {
            if (move.src_row == 7 && move.src_col == 0)
                whiteQRookMoved = 1;
            if (move.src_row == 7 && move.src_col == 7)
                whiteKRookMoved = 1;
        }
        else {
            if (move.src_row == 0 && move.src_col == 0)
                blackQRookMoved = 1;
            if (move.src_row == 0 && move.src_col == 7)
                blackKRookMoved = 1;
        }
    }
    // Set en passant target if a pawn moves two squares forward.
    if (tolower(pieceSymbol) == 'p' && abs(move.dst_row - move.src_row) == 2) {
        enPassantTargetRow = (move.src_row + move.dst_row) / 2;
        enPassantTargetCol = move.src_col;
    }
}

/*
 * isCellAttacked:
 * Checks whether the square at (row, col) is attacked by any enemy piece.
 * It considers pawn, knight, sliding (rook, bishop, queen), and king moves.
 */
int isCellAttacked(char boardState[BOARD_DIM][BOARD_DIM], int row, int col, int attackerSide) {
    // Pawn attacks.
    if (attackerSide == SIDE_WHITE) {
        int pawnRow = row + 1;
        if (isInsideBoard(pawnRow, col - 1) && boardState[pawnRow][col - 1] == 'P') return 1;
        if (isInsideBoard(pawnRow, col + 1) && boardState[pawnRow][col + 1] == 'P') return 1;
    }
    else {
        int pawnRow = row - 1;
        if (isInsideBoard(pawnRow, col - 1) && boardState[pawnRow][col - 1] == 'p') return 1;
        if (isInsideBoard(pawnRow, col + 1) && boardState[pawnRow][col + 1] == 'p') return 1;
    }
    // Knight moves.
    int knightOffsets[8][2] = { {-2,-1}, {-2,1}, {-1,-2}, {-1,2},
                                {1,-2}, {1,2}, {2,-1}, {2,1} };
    for (int i = 0; i < 8; i++) {
        int newRow = row + knightOffsets[i][0];
        int newCol = col + knightOffsets[i][1];
        if (isInsideBoard(newRow, newCol)) {
            char piece = boardState[newRow][newCol];
            if (attackerSide == SIDE_WHITE && piece == 'N') return 1;
            if (attackerSide == SIDE_BLACK && piece == 'n') return 1;
        }
    }
    // Rook/Queen linear moves.
    int linearDirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    for (int d = 0; d < 4; d++) {
        int dRow = linearDirs[d][0], dCol = linearDirs[d][1];
        int newRow = row + dRow, newCol = col + dCol;
        while (isInsideBoard(newRow, newCol)) {
            char piece = boardState[newRow][newCol];
            if (piece != EMPTY_CELL) {
                if (attackerSide == SIDE_WHITE) {
                    if (piece == 'R' || piece == 'Q')
                        return 1;
                }
                else {
                    if (piece == 'r' || piece == 'q')
                        return 1;
                }
                break;
            }
            newRow += dRow;
            newCol += dCol;
        }
    }
    // Bishop/Queen diagonal moves.
    int diagDirs[4][2] = { {1,1}, {1,-1}, {-1,1}, {-1,-1} };
    for (int d = 0; d < 4; d++) {
        int dRow = diagDirs[d][0], dCol = diagDirs[d][1];
        int newRow = row + dRow, newCol = col + dCol;
        while (isInsideBoard(newRow, newCol)) {
            char piece = boardState[newRow][newCol];
            if (piece != EMPTY_CELL) {
                if (attackerSide == SIDE_WHITE) {
                    if (piece == 'B' || piece == 'Q')
                        return 1;
                }
                else {
                    if (piece == 'b' || piece == 'q')
                        return 1;
                }
                break;
            }
            newRow += dRow;
            newCol += dCol;
        }
    }
    // King adjacent attack.
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue;
            int newRow = row + dr, newCol = col + dc;
            if (isInsideBoard(newRow, newCol)) {
                char piece = boardState[newRow][newCol];
                if (attackerSide == SIDE_WHITE && piece == 'K') return 1;
                if (attackerSide == SIDE_BLACK && piece == 'k') return 1;
            }
        }
    }
    return 0;
}

/*
 * isKingInCheck:
 * Determines if the king for the given side is in check.
 */
int isKingInCheck(char boardState[BOARD_DIM][BOARD_DIM], int side) {
    char kingSymbol = (side == SIDE_WHITE) ? 'K' : 'k';
    int kingRow = -1, kingCol = -1;
    for (int r = 0; r < BOARD_DIM; r++) {
        for (int c = 0; c < BOARD_DIM; c++) {
            if (boardState[r][c] == kingSymbol) {
                kingRow = r;
                kingCol = c;
                break;
            }
        }
        if (kingRow != -1) break;
    }
    if (kingRow == -1) return 1; // Missing king => consider it in check.
    int opponent = (side == SIDE_WHITE) ? SIDE_BLACK : SIDE_WHITE;
    return isCellAttacked(boardState, kingRow, kingCol, opponent);
}

/*
 * generateLegalMoves:
 * Generates all legal moves for the current side. It includes normal moves, pawn moves
 * (with double moves, en passant, and promotions), as well as castling moves.
 */
int generateLegalMoves(int side, ChessMove movesList[]) {
    int moveCount = 0;
    for (int r = 0; r < BOARD_DIM; r++) {
        for (int c = 0; c < BOARD_DIM; c++) {
            char currentPiece = chessBoard[r][c];
            if (currentPiece == EMPTY_CELL) continue;
            int pieceSide = isPieceWhite(currentPiece) ? SIDE_WHITE : SIDE_BLACK;
            if (pieceSide != side) continue;
            char lowerPiece = tolower(currentPiece);
            if (lowerPiece == 'p') {
                int direction = (side == SIDE_WHITE) ? -1 : 1;
                int startRow = (side == SIDE_WHITE) ? 6 : 1;
                int promotionRow = (side == SIDE_WHITE) ? 0 : 7;
                int nextRow = r + direction;
                // Single square forward.
                if (isInsideBoard(nextRow, c) && chessBoard[nextRow][c] == EMPTY_CELL) {
                    ChessMove mv;
                    mv.src_row = r; mv.src_col = c;
                    mv.dst_row = nextRow; mv.dst_col = c;
                    mv.promoteTo = (nextRow == promotionRow) ? ((side == SIDE_WHITE) ? 'Q' : 'q') : 0;
                    char boardCopy[BOARD_DIM][BOARD_DIM];
                    clone_board(chessBoard, boardCopy);
                    execute_move_on_board(boardCopy, mv);
                    if (!isKingInCheck(boardCopy, side))
                        movesList[moveCount++] = mv;
                    // Two-square move.
                    if (r == startRow && chessBoard[r + direction][c] == EMPTY_CELL &&
                        isInsideBoard(r + 2 * direction, c) && chessBoard[r + 2 * direction][c] == EMPTY_CELL) {
                        mv.dst_row = r + 2 * direction;
                        mv.promoteTo = 0;
                        clone_board(chessBoard, boardCopy);
                        execute_move_on_board(boardCopy, mv);
                        if (!isKingInCheck(boardCopy, side))
                            movesList[moveCount++] = mv;
                    }
                }
                // Pawn captures.
                for (int dc = -1; dc <= 1; dc += 2) {
                    int captureCol = c + dc;
                    if (isInsideBoard(nextRow, captureCol) && chessBoard[nextRow][captureCol] != EMPTY_CELL) {
                        char target = chessBoard[nextRow][captureCol];
                        if ((side == SIDE_WHITE && isPieceBlack(target)) ||
                            (side == SIDE_BLACK && isPieceWhite(target))) {
                            ChessMove mv;
                            mv.src_row = r; mv.src_col = c;
                            mv.dst_row = nextRow; mv.dst_col = captureCol;
                            mv.promoteTo = (nextRow == promotionRow) ? ((side == SIDE_WHITE) ? 'Q' : 'q') : 0;
                            char boardCopy[BOARD_DIM][BOARD_DIM];
                            clone_board(chessBoard, boardCopy);
                            execute_move_on_board(boardCopy, mv);
                            if (!isKingInCheck(boardCopy, side))
                                movesList[moveCount++] = mv;
                        }
                    }
                }
                // En passant capture.
                if (enPassantTargetRow != -1 && enPassantTargetCol != -1) {
                    for (int dc = -1; dc <= 1; dc += 2) {
                        if (c + dc == enPassantTargetCol && nextRow == enPassantTargetRow) {
                            ChessMove mv;
                            mv.src_row = r; mv.src_col = c;
                            mv.dst_row = nextRow; mv.dst_col = c + dc;
                            mv.promoteTo = (nextRow == promotionRow) ? ((side == SIDE_WHITE) ? 'Q' : 'q') : 0;
                            char boardCopy[BOARD_DIM][BOARD_DIM];
                            clone_board(chessBoard, boardCopy);
                            execute_move_on_board(boardCopy, mv);
                            if (!isKingInCheck(boardCopy, side))
                                movesList[moveCount++] = mv;
                        }
                    }
                }
            }
            else if (lowerPiece == 'n') {
                int knightJumps[8][2] = { {-2,-1}, {-2,1}, {-1,-2}, {-1,2},
                                          {1,-2}, {1,2}, {2,-1}, {2,1} };
                for (int i = 0; i < 8; i++) {
                    int newRow = r + knightJumps[i][0];
                    int newCol = c + knightJumps[i][1];
                    if (!isInsideBoard(newRow, newCol)) continue;
                    char target = chessBoard[newRow][newCol];
                    if (target == EMPTY_CELL ||
                        (side == SIDE_WHITE && isPieceBlack(target)) ||
                        (side == SIDE_BLACK && isPieceWhite(target))) {
                        ChessMove mv;
                        mv.src_row = r; mv.src_col = c;
                        mv.dst_row = newRow; mv.dst_col = newCol;
                        mv.promoteTo = 0;
                        char boardCopy[BOARD_DIM][BOARD_DIM];
                        clone_board(chessBoard, boardCopy);
                        execute_move_on_board(boardCopy, mv);
                        if (!isKingInCheck(boardCopy, side))
                            movesList[moveCount++] = mv;
                    }
                }
            }
            else if (lowerPiece == 'b' || lowerPiece == 'r' || lowerPiece == 'q') {
                int directions[8][2];
                int numDirs = 0;
                if (lowerPiece == 'b') {
                    int bishopDirs[4][2] = { {1,1}, {1,-1}, {-1,1}, {-1,-1} };
                    for (int i = 0; i < 4; i++) {
                        directions[numDirs][0] = bishopDirs[i][0];
                        directions[numDirs][1] = bishopDirs[i][1];
                        numDirs++;
                    }
                }
                else if (lowerPiece == 'r') {
                    int rookDirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
                    for (int i = 0; i < 4; i++) {
                        directions[numDirs][0] = rookDirs[i][0];
                        directions[numDirs][1] = rookDirs[i][1];
                        numDirs++;
                    }
                }
                else if (lowerPiece == 'q') {
                    int queenDirs[8][2] = { {1,0}, {-1,0}, {0,1}, {0,-1},
                                            {1,1}, {1,-1}, {-1,1}, {-1,-1} };
                    for (int i = 0; i < 8; i++) {
                        directions[numDirs][0] = queenDirs[i][0];
                        directions[numDirs][1] = queenDirs[i][1];
                        numDirs++;
                    }
                }
                for (int d = 0; d < numDirs; d++) {
                    int dRow = directions[d][0], dCol = directions[d][1];
                    int newRow = r + dRow, newCol = c + dCol;
                    while (isInsideBoard(newRow, newCol)) {
                        char target = chessBoard[newRow][newCol];
                        if (target == EMPTY_CELL) {
                            ChessMove mv;
                            mv.src_row = r; mv.src_col = c;
                            mv.dst_row = newRow; mv.dst_col = newCol;
                            mv.promoteTo = 0;
                            char boardCopy[BOARD_DIM][BOARD_DIM];
                            clone_board(chessBoard, boardCopy);
                            execute_move_on_board(boardCopy, mv);
                            if (!isKingInCheck(boardCopy, side))
                                movesList[moveCount++] = mv;
                        }
                        else {
                            if ((side == SIDE_WHITE && isPieceBlack(target)) ||
                                (side == SIDE_BLACK && isPieceWhite(target))) {
                                ChessMove mv;
                                mv.src_row = r; mv.src_col = c;
                                mv.dst_row = newRow; mv.dst_col = newCol;
                                mv.promoteTo = 0;
                                char boardCopy[BOARD_DIM][BOARD_DIM];
                                clone_board(chessBoard, boardCopy);
                                execute_move_on_board(boardCopy, mv);
                                if (!isKingInCheck(boardCopy, side))
                                    movesList[moveCount++] = mv;
                            }
                            break;
                        }
                        newRow += dRow;
                        newCol += dCol;
                    }
                }
            }
            else if (lowerPiece == 'k') {
                // King moves (one square in any direction).
                for (int dr = -1; dr <= 1; dr++) {
                    for (int dc = -1; dc <= 1; dc++) {
                        if (dr == 0 && dc == 0) continue;
                        int newRow = r + dr, newCol = c + dc;
                        if (!isInsideBoard(newRow, newCol)) continue;
                        char target = chessBoard[newRow][newCol];
                        if (target == EMPTY_CELL ||
                            (side == SIDE_WHITE && isPieceBlack(target)) ||
                            (side == SIDE_BLACK && isPieceWhite(target))) {
                            ChessMove mv;
                            mv.src_row = r; mv.src_col = c;
                            mv.dst_row = newRow; mv.dst_col = newCol;
                            mv.promoteTo = 0;
                            char boardCopy[BOARD_DIM][BOARD_DIM];
                            clone_board(chessBoard, boardCopy);
                            execute_move_on_board(boardCopy, mv);
                            if (!isKingInCheck(boardCopy, side))
                                movesList[moveCount++] = mv;
                        }
                    }
                }
                // --- Castling Moves ---
                if (side == SIDE_WHITE && r == 7 && c == 4 && !whiteKingMoved) {
                    // White kingside castling.
                    if (!whiteKRookMoved && chessBoard[7][7] == 'R' &&
                        chessBoard[7][5] == EMPTY_CELL && chessBoard[7][6] == EMPTY_CELL &&
                        !isCellAttacked(chessBoard, 7, 4, SIDE_BLACK) &&
                        !isCellAttacked(chessBoard, 7, 5, SIDE_BLACK) &&
                        !isCellAttacked(chessBoard, 7, 6, SIDE_BLACK)) {
                        ChessMove mv;
                        mv.src_row = 7; mv.src_col = 4;
                        mv.dst_row = 7; mv.dst_col = 6;
                        mv.promoteTo = 0;
                        char boardCopy[BOARD_DIM][BOARD_DIM];
                        clone_board(chessBoard, boardCopy);
                        execute_move_on_board(boardCopy, mv);
                        if (!isKingInCheck(boardCopy, SIDE_WHITE))
                            movesList[moveCount++] = mv;
                    }
                    // White queenside castling.
                    if (!whiteQRookMoved && chessBoard[7][0] == 'R' &&
                        chessBoard[7][1] == EMPTY_CELL && chessBoard[7][2] == EMPTY_CELL && chessBoard[7][3] == EMPTY_CELL &&
                        !isCellAttacked(chessBoard, 7, 4, SIDE_BLACK) &&
                        !isCellAttacked(chessBoard, 7, 3, SIDE_BLACK) &&
                        !isCellAttacked(chessBoard, 7, 2, SIDE_BLACK)) {
                        ChessMove mv;
                        mv.src_row = 7; mv.src_col = 4;
                        mv.dst_row = 7; mv.dst_col = 2;
                        mv.promoteTo = 0;
                        char boardCopy[BOARD_DIM][BOARD_DIM];
                        clone_board(chessBoard, boardCopy);
                        execute_move_on_board(boardCopy, mv);
                        if (!isKingInCheck(boardCopy, SIDE_WHITE))
                            movesList[moveCount++] = mv;
                    }
                }
                else if (side == SIDE_BLACK && r == 0 && c == 4 && !blackKingMoved) {
                    // Black kingside castling.
                    if (!blackKRookMoved && chessBoard[0][7] == 'r' &&
                        chessBoard[0][5] == EMPTY_CELL && chessBoard[0][6] == EMPTY_CELL &&
                        !isCellAttacked(chessBoard, 0, 4, SIDE_WHITE) &&
                        !isCellAttacked(chessBoard, 0, 5, SIDE_WHITE) &&
                        !isCellAttacked(chessBoard, 0, 6, SIDE_WHITE)) {
                        ChessMove mv;
                        mv.src_row = 0; mv.src_col = 4;
                        mv.dst_row = 0; mv.dst_col = 6;
                        mv.promoteTo = 0;
                        char boardCopy[BOARD_DIM][BOARD_DIM];
                        clone_board(chessBoard, boardCopy);
                        execute_move_on_board(boardCopy, mv);
                        if (!isKingInCheck(boardCopy, SIDE_BLACK))
                            movesList[moveCount++] = mv;
                    }
                    // Black queenside castling.
                    if (!blackQRookMoved && chessBoard[0][0] == 'r' &&
                        chessBoard[0][1] == EMPTY_CELL && chessBoard[0][2] == EMPTY_CELL && chessBoard[0][3] == EMPTY_CELL &&
                        !isCellAttacked(chessBoard, 0, 4, SIDE_WHITE) &&
                        !isCellAttacked(chessBoard, 0, 3, SIDE_WHITE) &&
                        !isCellAttacked(chessBoard, 0, 2, SIDE_WHITE)) {
                        ChessMove mv;
                        mv.src_row = 0; mv.src_col = 4;
                        mv.dst_row = 0; mv.dst_col = 2;
                        mv.promoteTo = 0;
                        char boardCopy[BOARD_DIM][BOARD_DIM];
                        clone_board(chessBoard, boardCopy);
                        execute_move_on_board(boardCopy, mv);
                        if (!isKingInCheck(boardCopy, SIDE_BLACK))
                            movesList[moveCount++] = mv;
                    }
                }
            }
        }
    }
    return moveCount;
}

/*
 * output_move:
 * Converts a ChessMove to standard coordinate notation (e.g., "e2e4") and prints it.
 * Promotions append "=Q" (or "=q").
 */
void output_move(ChessMove move) {
    char srcFile = 'a' + move.src_col;
    char srcRank = '8' - move.src_row;
    char dstFile = 'a' + move.dst_col;
    char dstRank = '8' - move.dst_row;
    printf("%c%c%c%c", srcFile, srcRank, dstFile, dstRank);
    if (move.promoteTo)
        printf("=%c", move.promoteTo);
}

/*
 * interpret_move:
 * Parses a move string (e.g., "e2e4" or "e7e8=Q") into a ChessMove structure.
 * It also performs basic validation.
 */
int interpret_move(char* input, ChessMove* move, int side) {
    if (strlen(input) < 4) return 0;
    move->src_col = input[0] - 'a';
    move->src_row = '8' - input[1];
    move->dst_col = input[2] - 'a';
    move->dst_row = '8' - input[3];
    move->promoteTo = 0;
    if (strlen(input) >= 6 && input[4] == '=')
        move->promoteTo = input[5];
    if (!isInsideBoard(move->src_row, move->src_col) || !isInsideBoard(move->dst_row, move->dst_col))
        return 0;
    char piece = chessBoard[move->src_row][move->src_col];
    if (piece == EMPTY_CELL) return 0;
    if (side == SIDE_WHITE && !isPieceWhite(piece)) return 0;
    if (side == SIDE_BLACK && !isPieceBlack(piece)) return 0;
    return 1;
}

/*
 * evaluate_board:
 * A simple evaluation function based solely on material count.
 * Piece values: Pawn=100, Knight=320, Bishop=330, Rook=500, Queen=900, King=20000.
 */
int evaluate_board() {
    int score = 0;
    for (int r = 0; r < BOARD_DIM; r++) {
        for (int c = 0; c < BOARD_DIM; c++) {
            char piece = chessBoard[r][c];
            if (piece == EMPTY_CELL) continue;
            int pieceValue = 0;
            switch (tolower(piece)) {
            case 'p': pieceValue = 100; break;
            case 'n': pieceValue = 320; break;
            case 'b': pieceValue = 330; break;
            case 'r': pieceValue = 500; break;
            case 'q': pieceValue = 900; break;
            case 'k': pieceValue = 20000; break;
            }
            if (isPieceWhite(piece))
                score += pieceValue;
            else
                score -= pieceValue;
        }
    }
    return score;
}

/*
 * minimax:
 * A simple minimax search with alpha-beta pruning.
 * It recursively evaluates positions to a specified depth and returns an evaluation score.
 */
int minimax(int depth, int side, int alpha, int beta) {
    if (depth == 0) return evaluate_board();

    ChessMove movesList[MAX_LEGAL_MOVES];
    int numMoves = generateLegalMoves(side, movesList);
    if (numMoves == 0) {
        // No moves: checkmate if king is in check, stalemate otherwise.
        if (isKingInCheck(chessBoard, side))
            return -20000;
        else
            return 0;
    }

    int bestScore = -1000000;
    char boardSave[BOARD_DIM][BOARD_DIM];
    int saveWhiteKing, saveWhiteQRook, saveWhiteKRook, saveBlackKing, saveBlackQRook, saveBlackKRook, saveEnPassantRow, saveEnPassantCol;

    for (int i = 0; i < numMoves; i++) {
        save_state(boardSave, &saveWhiteKing, &saveWhiteQRook, &saveWhiteKRook,
            &saveBlackKing, &saveBlackQRook, &saveBlackKRook,
            &saveEnPassantRow, &saveEnPassantCol);
        execute_move_on_board(chessBoard, movesList[i]);
        int score = -minimax(depth - 1, (side == SIDE_WHITE) ? SIDE_BLACK : SIDE_WHITE, -beta, -alpha);
        restore_state(boardSave, saveWhiteKing, saveWhiteQRook, saveWhiteKRook,
            saveBlackKing, saveBlackQRook, saveBlackKRook,
            saveEnPassantRow, saveEnPassantCol);
        if (score > bestScore)
            bestScore = score;
        if (bestScore > alpha)
            alpha = bestScore;
        if (alpha >= beta)
            break;
    }
    return bestScore;
}

/*
 * choose_best_move:
 * Iterates through all legal moves and uses minimax to pick the best move.
 * This is our AI decision function, set to search a given depth.
 */
ChessMove choose_best_move(int side, int depth) {
    ChessMove movesList[MAX_LEGAL_MOVES];
    int numMoves = generateLegalMoves(side, movesList);
    ChessMove bestMove = movesList[0];
    int bestScore = -1000000;
    char boardSave[BOARD_DIM][BOARD_DIM];
    int saveWhiteKing, saveWhiteQRook, saveWhiteKRook, saveBlackKing, saveBlackQRook, saveBlackKRook, saveEnPassantRow, saveEnPassantCol;

    for (int i = 0; i < numMoves; i++) {
        save_state(boardSave, &saveWhiteKing, &saveWhiteQRook, &saveWhiteKRook,
            &saveBlackKing, &saveBlackQRook, &saveBlackKRook,
            &saveEnPassantRow, &saveEnPassantCol);
        execute_move_on_board(chessBoard, movesList[i]);
        int score = -minimax(depth - 1, (side == SIDE_WHITE) ? SIDE_BLACK : SIDE_WHITE, -1000000, 1000000);
        restore_state(boardSave, saveWhiteKing, saveWhiteQRook, saveWhiteKRook,
            saveBlackKing, saveBlackQRook, saveBlackKRook,
            saveEnPassantRow, saveEnPassantCol);
        if (score > bestScore) {
            bestScore = score;
            bestMove = movesList[i];
        }
    }
    return bestMove;
}

/*
 * main:
 * The main game loop. The human (White) inputs moves in coordinate notation,
 * and the AI (Black) now uses a minimax search to choose its move.
 *
 * For castling, enter:
 *   - Kingside as "e1g1" (for White) or "e8g8" (for Black)
 *   - Queenside as "e1c1" or "e8c8"
 */
int main() {
    initialize_board();
    srand(time(NULL));

    // Reset global state.
    whiteKingMoved = whiteQRookMoved = whiteKRookMoved = 0;
    blackKingMoved = blackQRookMoved = blackKRookMoved = 0;
    enPassantTargetRow = -1;
    enPassantTargetCol = -1;

    int activeSide = SIDE_WHITE;
    int searchDepth = 3;  // Adjust search depth for AI (depth 3 gives beginner-intermediate strength)

    while (1) {
        display_board();
        ChessMove legalMoves[MAX_LEGAL_MOVES];
        int numLegal = generateLegalMoves(activeSide, legalMoves);
        if (numLegal == 0) {
            if (isKingInCheck(chessBoard, activeSide))
                printf("%s is checkmated. %s wins!\n",
                    (activeSide == SIDE_WHITE ? "White" : "Black"),
                    (activeSide == SIDE_WHITE ? "Black" : "White"));
            else
                printf("Stalemate!\n");
            break;
        }
        if (activeSide == SIDE_WHITE) {
            // Human move.
            printf("Enter your move (e.g., e2e4): ");
            char inputStr[10];
            fgets(inputStr, sizeof(inputStr), stdin);
            inputStr[strcspn(inputStr, "\n")] = 0;
            ChessMove playerMove;
            if (!interpret_move(inputStr, &playerMove, SIDE_WHITE)) {
                printf("Invalid move format.\n");
                continue;
            }
            int valid = 0;
            for (int i = 0; i < numLegal; i++) {
                if (legalMoves[i].src_row == playerMove.src_row &&
                    legalMoves[i].src_col == playerMove.src_col &&
                    legalMoves[i].dst_row == playerMove.dst_row &&
                    legalMoves[i].dst_col == playerMove.dst_col &&
                    legalMoves[i].promoteTo == playerMove.promoteTo) {
                    valid = 1;
                    break;
                }
            }
            if (!valid) {
                printf("Illegal move. Try again.\n");
                continue;
            }
            execute_move_on_board(chessBoard, playerMove);
        }
        else {
            // AI move.
            ChessMove aiMove = choose_best_move(SIDE_BLACK, searchDepth);
            printf("AI plays: ");
            output_move(aiMove);
            printf("\n");
            execute_move_on_board(chessBoard, aiMove);
        }
        activeSide = (activeSide == SIDE_WHITE) ? SIDE_BLACK : SIDE_WHITE;
    }
    return 0;
}
