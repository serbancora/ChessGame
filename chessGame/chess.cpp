#include <iostream>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include <cmath>
#include <map>

using namespace std;
using namespace sf;

// Constants
const int SIZE = 8;            // Size of the chess board
const int TILE_SIZE = 64;      // Size of each tile
const int SIDEBAR_WIDTH = 150; // Width of the moves table sidebar
const int colLabelHeight = TILE_SIZE / 4;
const int rowLabelWidth = TILE_SIZE / 4;
const int WINDOW_WIDTH = SIZE * TILE_SIZE + rowLabelWidth + SIDEBAR_WIDTH;
const int WINDOW_HEIGHT = SIZE * TILE_SIZE + colLabelHeight;
const size_t MAX_VISIBLE_MOVES = 24;     // Maximum number of moves to display in the sidebar
bool isWhiteTurn = true;                 // True for white's turn, false for black's turn
vector<pair<Vector2i, Vector2i>> arrows; // To store the arrows drawn by the user
const float PI = 3.14159265358979323846;

enum GameState
{
    MENU,
    PLAYING,
    EXIT
};
GameState currentState = MENU;

void loadResources(map<string, Texture> &textures, map<string, Font> &fonts);
void drawMenu(RenderWindow &window, map<string, Texture> &textures, map<string, Font> &fonts);

class Piece // Represents a chess piece
{
public:
    string type;  // "P", "R", "N", "B", "Q", "K"
    bool isWhite; // true for white, false for black

    Piece(string type = " ", bool isWhite = false) : type(type), isWhite(isWhite) {}
    bool isEmpty() // Check if the piece is empty
    {
        return type == " ";
    }
};

class Square // Represents a square on the chess board
{
public:
    Piece piece;
    Square(Piece piece = Piece()) : piece(piece) {} // Initialize the square with a piece
};

class ChessBoard // Represents the chess board
{
private:
    bool hasWhiteKingMoved = false;
    bool hasBlackKingMoved = false;
    bool hasWhiteRookMoved[2] = {false, false}; // [0] for queenside, [1] for kingside
    bool hasBlackRookMoved[2] = {false, false}; // [0] for queenside, [1] for kingside

    vector<vector<Square>> board;
    bool didWEnPassant = false; // Check if white did en passant
    bool didBEnPassant = false; // Check if black did en passant

public:
    string lastMove = "";               // Store the last move
    string backupLastMove = "";         // Store the last move before simulation
    vector<vector<Square>> backupBoard; // Store the board before simulation

    ChessBoard() // Initialize the board
    {
        board.resize(SIZE, vector<Square>(SIZE, Square()));
        initializeBoard();
    }

    void resetBoard()
    {
        board.resize(SIZE, vector<Square>(SIZE, Square()));
        initializeBoard();
        hasWhiteKingMoved = false;
        hasBlackKingMoved = false;
        hasWhiteRookMoved[0] = false;
        hasWhiteRookMoved[1] = false;
        hasBlackRookMoved[0] = false;
        hasBlackRookMoved[1] = false;
        didWEnPassant = false;
        didBEnPassant = false;
        lastMove = "";
        backupLastMove = "";
    }

    void initializeBoard() // Set up the board with pieces
    {
        // place pawns
        for (int x = 0; x < SIZE; x++)
        {
            board[1][x].piece = {"P", false}; // black pawns
            board[6][x].piece = {"P", true};  // white pawns
        }

        // Place other pieces
        string order = "RNBQKBNR";
        for (int x = 0; x < SIZE; x++)
        {
            board[0][x].piece = Piece(string(1, order[x]), false); // Black pieces
            board[7][x].piece = Piece(string(1, order[x]), true);  // White pieces
        }
    }

    Square &getSquare(int y, int x) // Get the square at position (x, y)
    {
        return board[y][x];
    }

    bool isValidTile(int x, int y) // Check if the tile is within the board
    {
        return x >= 0 && x < SIZE && y >= 0 && y < SIZE;
    }

    bool isSameColor(Piece piece1, Piece piece2) // Check if two pieces are of the same color
    {
        if (piece2.isEmpty())
            return false;
        return (piece1.isWhite == piece2.isWhite);
    }

    string toAlgebraic(int x, int y) // Convert board coordinates to algebraic notation (e.g., (0, 0) -> "a8")
    {
        return string(1, char('a' + x)) + to_string(8 - y);
    }

    bool isKingInCheck(bool isWhite)
    {
        int kingX = -1, kingY = -1;

        // Find the king's position
        for (int y = 0; y < SIZE; y++)
        {
            for (int x = 0; x < SIZE; x++)
            {
                if (board[y][x].piece.isEmpty())
                    continue;
                Piece &piece = board[y][x].piece;
                if (piece.type == "K" && piece.isWhite == isWhite)
                {
                    kingX = x;
                    kingY = y;
                    break;
                }
            }
        }

        // Ensure the king's position is valid
        if (kingX == -1 || kingY == -1)
        {
            cerr << "Error: King not found on board. Invalid state.\n";
            return true; // Assume check if king is missing
        }

        string backupLastMove = lastMove; // Backup lastMove before simulation
        backupBoard = board;              // Backup the board before simulation
        bool isUnderCheck = false;

        // Check if any opponent piece can attack the king
        for (int y = 0; y < SIZE; y++)
        {
            for (int x = 0; x < SIZE; x++)
            {
                if (board[y][x].piece.isEmpty())
                    continue;
                Piece &piece = board[y][x].piece;
                if (!piece.isEmpty() && piece.isWhite != isWhite)
                {
                    bool tempCheck = false, tempCheckmate = false;
                    if (isValidMove(x, y, kingX, kingY, tempCheck, tempCheckmate, true, true))
                    {
                        isUnderCheck = true;
                        break;
                    }
                }
            }
        }

        // Restore state before returning
        lastMove = backupLastMove;
        board = backupBoard;
        return isUnderCheck;
    }

    bool isCheckmate(bool isWhite)
    {
        // Check if the king is in check
        if (!isKingInCheck(isWhite))
            return false;

        string backupLastMove = lastMove; // Backup lastMove before simulation
        backupBoard = board;              // Backup the board before simulation

        // Iterate through all squares to find pieces of the current player
        for (int y = 0; y < SIZE; y++)
        {
            for (int x = 0; x < SIZE; x++)
            {
                if (board[y][x].piece.isEmpty())
                    continue;
                Piece &piece = board[y][x].piece;
                if (!piece.isEmpty() && piece.isWhite == isWhite)
                {
                    // Check if this piece has any valid moves
                    for (int toY = 0; toY < SIZE; toY++)
                    {
                        for (int toX = 0; toX < SIZE; toX++)
                        {
                            bool tempCheck = false, tempCheckmate = false;
                            if (isValidMove(x, y, toX, toY, tempCheck, tempCheckmate, true))
                            {
                                lastMove = backupLastMove; // Restore lastMove
                                board = backupBoard;       // Restore the board
                                return false;              // The player has at least one valid move
                            }
                        }
                    }
                }
            }
        }
        // Restore state before returning checkmate status
        lastMove = backupLastMove; // Restore lastMove
        board = backupBoard;       // Restore the board
        return true;               // If no valid moves exist, it's a checkmate
    }

    bool isValidPawnMove(int fromX, int fromY, int toX, int toY, bool isWhite, string &lastMove, bool &didWEnPassant, bool &didBEnPassant, bool isDrawingMoves)
    {
        int direction = isWhite ? -1 : 1;
        int startRow = isWhite ? 6 : 1;

        // Single square move
        if (toX == fromX && toY == fromY + direction && board[toY][toX].piece.isEmpty())
        {
            if (!isDrawingMoves)
            {
                lastMove = toAlgebraic(toX, toY);
            }
            return true;
        }

        // Double square move from starting position
        if (toX == fromX && toY == fromY + 2 * direction && fromY == startRow &&
            board[toY][toX].piece.isEmpty() && board[fromY + direction][toX].piece.isEmpty())
        {
            if (!isDrawingMoves)
            {
                lastMove = toAlgebraic(toX, toY);
            }
            return true;
        }

        // Capture
        if (abs(toX - fromX) == 1 && toY == fromY + direction && !board[toY][toX].piece.isEmpty() &&
            !isSameColor(board[fromY][fromX].piece, board[toY][toX].piece))
        {
            if (!isDrawingMoves)
            {
                lastMove = string(1, char(fromX + 'a')) + "x" + toAlgebraic(toX, toY);
            }
            return true;
        }

        // En passant
        if (abs(toX - fromX) == 1 && toY == fromY + direction && board[toY][toX].piece.isEmpty())
        {
            string expectedLastMove = toAlgebraic(toX, fromY);
            if (lastMove == expectedLastMove && board[fromY][toX].piece.type == "P" && board[fromY][toX].piece.isWhite != isWhite && (isWhite ? expectedLastMove[1] == '5' : expectedLastMove[1] == '4'))
            {

                if (!isDrawingMoves)
                {
                    if (isWhite)
                        didWEnPassant = true;
                    else
                        didBEnPassant = true;
                    lastMove = string(1, char(fromX + 'a')) + "x" + toAlgebraic(toX, toY);
                    board[fromY][toX].piece = Piece(); // Capture the pawn
                }
                return true;
            }
        }

        return false;
    }

    bool isValidRookMove(int fromX, int fromY, int toX, int toY, bool isWhite, string &lastMove)
    {
        // Rook can only move horizontally or vertically
        if (fromX != toX && fromY != toY)
            return false;

        // Check if the path is clear
        if (!isPathClear(fromX, fromY, toX, toY))
            return false;

        // Check if the destination square contains a piece of the same color
        if (isSameColor(board[fromY][fromX].piece, board[toY][toX].piece))
            return false;

        // Detect ambiguity
        auto [requiresFile, requiresRank] = detectAmbiguity(fromX, fromY, toX, toY, 'R', isWhite);

        // Construct disambiguation
        string disambiguation = "";
        if (requiresFile && requiresRank)
        {
            // Include both file and rank when full ambiguity exists
            disambiguation = toAlgebraic(toX, toY);
        }
        else if (requiresFile)
        {
            // Include only the file when rank is not ambiguous
            disambiguation = string(1, char(fromX + 'a'));
        }
        else if (requiresRank)
        {
            // Include only the rank when file is not ambiguous
            disambiguation = to_string(8 - fromY);
        }

        // Determine capture notation
        string capture = board[toY][toX].piece.isEmpty() ? "" : "x";

        // Construct the move notation
        lastMove = "R" + disambiguation + capture + toAlgebraic(toX, toY);

        return true;
    }

    bool isValidKnightMove(int fromX, int fromY, int toX, int toY, bool isWhite, string &lastMove)
    {
        // Knight can only move in an L-shape
        if (abs(fromX - toX) * abs(fromY - toY) != 2)
            return false;

        // Check if the destination square contains a piece of the same color
        if (isSameColor(board[fromY][fromX].piece, board[toY][toX].piece))
            return false;

        // Detect ambiguity
        auto [requiresFile, requiresRank] = detectAmbiguity(fromX, fromY, toX, toY, 'N', isWhite);

        // Construct disambiguation
        string disambiguation = "";
        if (requiresFile && requiresRank)
        {
            // Include both file and rank if necessary
            disambiguation = toAlgebraic(toX, toY);
        }
        else if (requiresFile)
        {
            // Include only the file
            disambiguation = string(1, char(fromX + 'a'));
        }
        else if (requiresRank)
        {
            // Include only the rank
            disambiguation = to_string(8 - fromY);
        }

        // Determine capture notation
        string capture = board[toY][toX].piece.isEmpty() ? "" : "x";

        // Construct move notation
        lastMove = "N" + disambiguation + capture + toAlgebraic(toX, toY);

        return true;
    }

    bool isValidBishopMove(int fromX, int fromY, int toX, int toY, bool isWhite, string &lastMove)
    {
        // Bishop can only move diagonally
        if (abs(fromX - toX) != abs(fromY - toY))
            return false;

        // Check if the path is clear
        if (!isPathClear(fromX, fromY, toX, toY))
            return false;

        // Check if the destination square contains a piece of the same color
        if (isSameColor(board[fromY][fromX].piece, board[toY][toX].piece))
            return false;

        // Detect ambiguity
        auto [requiresFile, requiresRank] = detectAmbiguity(fromX, fromY, toX, toY, 'B', isWhite);

        // Construct disambiguation
        string disambiguation = "";
        if (requiresFile && requiresRank)
        {
            // Include both file and rank if necessary
            disambiguation = toAlgebraic(toX, toY);
        }
        else if (requiresFile)
        {
            // Include only the file
            disambiguation = string(1, char(fromX + 'a'));
        }
        else if (requiresRank)
        {
            // Include only the rank
            disambiguation = to_string(8 - fromY);
        }

        // Determine capture notation
        string capture = board[toY][toX].piece.isEmpty() ? "" : "x";

        // Construct move notation
        lastMove = "B" + disambiguation + capture + toAlgebraic(toX, toY);

        return true;
    }

    bool isValidQueenMove(int fromX, int fromY, int toX, int toY, bool isWhite, string &lastMove)
    {
        // Queen can move horizontally, vertically, or diagonally
        if (fromX != toX && fromY != toY && abs(fromX - toX) != abs(fromY - toY))
            return false;

        // Check if the path is clear
        if (!isPathClear(fromX, fromY, toX, toY))
            return false;

        // Check if the destination square contains a piece of the same color
        if (isSameColor(board[fromY][fromX].piece, board[toY][toX].piece))
            return false;

        // Detect ambiguity
        auto [requiresFile, requiresRank] = detectAmbiguity(fromX, fromY, toX, toY, 'Q', isWhite);

        // Construct disambiguation
        string disambiguation = "";
        if (requiresFile && requiresRank)
        {
            // Include both file and rank if necessary
            disambiguation = toAlgebraic(toX, toY);
        }
        else if (requiresFile)
        {
            // Include only the file
            disambiguation = string(1, char(fromX + 'a'));
        }
        else if (requiresRank)
        {
            // Include only the rank
            disambiguation = to_string(8 - fromY);
        }

        // Determine capture notation
        string capture = board[toY][toX].piece.isEmpty() ? "" : "x";

        // Construct move notation
        lastMove = "Q" + disambiguation + capture + toAlgebraic(toX, toY);

        return true;
    }

    bool isValidKingMove(int fromX, int fromY, int toX, int toY, bool isWhite, bool &isCastling, int &rookFromX, int &rookToX)
    {
        int dx = abs(toX - fromX);
        int dy = abs(toY - fromY);

        // Normal king move
        if (dx <= 1 && dy <= 1)
        {
            if (!isSameColor(board[fromY][fromX].piece, board[toY][toX].piece))
            {
                // Simulate the move
                Piece capturedPiece = board[toY][toX].piece;
                board[toY][toX].piece = board[fromY][fromX].piece;
                board[fromY][fromX].piece = Piece();

                bool inCheck = isKingInCheck(isWhite);

                // Revert the move
                board[fromY][fromX].piece = board[toY][toX].piece;
                board[toY][toX].piece = capturedPiece;

                // Ensure the king does not move into a square under attack
                if (inCheck)
                    return false;

                // Update last move notation
                if (!board[toY][toX].piece.isEmpty())        // Capture
                    lastMove = "Kx" + toAlgebraic(toX, toY); // Example: Kxe5
                else
                    lastMove = "K" + toAlgebraic(toX, toY); // Example: Ke5

                return true;
            }
            return false;
        }

        // Castling logic (unchanged)
        if (dy == 0 && dx == 2)
        {
            bool isShortCastle = toX > fromX;
            rookFromX = isShortCastle ? 7 : 0; // Kingside or Queenside rook position
            rookToX = isShortCastle ? 5 : 3;   // Rook's new position during castling

            // Check if the king or the rook has moved
            if ((isWhite && hasWhiteKingMoved) || (!isWhite && hasBlackKingMoved))
                return false;

            if (isWhite)
            {
                if (isShortCastle && hasWhiteRookMoved[1])
                    return false;
                if (!isShortCastle && hasWhiteRookMoved[0])
                    return false;
            }
            else
            {
                if (isShortCastle && hasBlackRookMoved[1])
                    return false;
                if (!isShortCastle && hasBlackRookMoved[0])
                    return false;
            }

            // Check if path is clear
            int step = isShortCastle ? 1 : -1;
            for (int x = fromX + step; x != rookFromX; x += step)
            {
                if (!board[fromY][x].piece.isEmpty())
                    return false;
            }

            // Ensure the rook exists and matches the king's color
            Piece rook = board[fromY][rookFromX].piece;
            if (rook.type == "R" && rook.isWhite == isWhite)
            {
                isCastling = true;
                lastMove = isShortCastle ? "O-O" : "O-O-O"; // Castling notation
                return true;
            }
        }

        return false;
    }

    bool isValidMove(int fromX, int fromY, int toX, int toY)
    {
        bool putsOpponentInCheck = false;
        bool resultsInCheckmate = false;
        return isValidMove(fromX, fromY, toX, toY, putsOpponentInCheck, resultsInCheckmate);
    }

    bool isValidMove(int fromX, int fromY, int toX, int toY, bool isDrawingMoves)
    {
        bool putsOpponentInCheck = false;
        bool resultsInCheckmate = false;
        return isValidMove(fromX, fromY, toX, toY, putsOpponentInCheck, resultsInCheckmate, isDrawingMoves);
    }

    bool isValidMove(int fromX, int fromY, int toX, int toY, bool &putsOpponentInCheck, bool &resultsInCheckmate, bool inSimulation = false, bool skipKingSafetyCheck = false, bool isDrawingMoves = false)
    {
        Piece piece = board[fromY][fromX].piece;
        if (piece.isEmpty())
            return false;

        if (!isValidTile(fromX, fromY) || !isValidTile(toX, toY))
        {
            cerr << "Invalid move: Out of bounds (" << fromX << ", " << fromY << ") to (" << toX << ", " << toY << ")\n";
            return false;
        }

        backupLastMove = lastMove; // Backup lastMove before simulation
        backupBoard = board;       // Backup the board before simulation

        bool isCastling = false;
        int rookFromX = -1, rookToX = -1;

        // Validate the move based on the piece type
        bool valid = false;
        switch (piece.type[0])
        {
        case 'P':
            valid = isValidPawnMove(fromX, fromY, toX, toY, piece.isWhite, lastMove, didWEnPassant, didBEnPassant, isDrawingMoves);
            break;
        case 'R':
            valid = isValidRookMove(fromX, fromY, toX, toY, piece.isWhite, lastMove);
            break;
        case 'N':
            valid = isValidKnightMove(fromX, fromY, toX, toY, piece.isWhite, lastMove);
            break;
        case 'B':
            valid = isValidBishopMove(fromX, fromY, toX, toY, piece.isWhite, lastMove);
            break;
        case 'Q':
            valid = isValidQueenMove(fromX, fromY, toX, toY, piece.isWhite, lastMove);
            break;
        case 'K':
            valid = isValidKingMove(fromX, fromY, toX, toY, piece.isWhite, isCastling, rookFromX, rookToX);
            break;
        default:
            return false;
        }

        if (!valid)
        {
            lastMove = backupLastMove; // Restore lastMove if invalid
            backupBoard = board;       // Restore the board if invalid
            return false;
        }

        // Simulate the move
        Piece capturedPiece = board[toY][toX].piece;
        board[toY][toX].piece = piece;
        board[fromY][fromX].piece = Piece();

        // Skip king safety checks if explicitly told to
        bool kingInCheck = false;
        if (!skipKingSafetyCheck)
        {
            // Prevent the king from moving into check
            kingInCheck = isKingInCheck(piece.isWhite);
        }

        // Revert the move
        board[fromY][fromX].piece = piece;
        board[toY][toX].piece = capturedPiece;

        // Restore lastMove after simulation
        lastMove = backupLastMove;
        backupBoard = board; // Restore the board after simulation

        // Move is only valid if it does not leave the king in check
        return !kingInCheck;
    }

    pair<bool, bool> detectAmbiguity(int fromX, int fromY, int toX, int toY, char type, bool isWhite)
    {
        bool requiresFile = false, requiresRank = false;
        int ambiguityCount = 0; // Count how many pieces can move to (toX, toY)

        for (int y = 0; y < SIZE; y++)
        {
            for (int x = 0; x < SIZE; x++)
            {
                if (x == fromX && y == fromY)
                    continue; // Skip the current piece

                if (board[y][x].piece.isEmpty())
                    continue;

                Piece &p = board[y][x].piece;

                // Check if it's the same type and color
                if (p.type[0] == type && p.isWhite == isWhite)
                {
                    // Validate if this piece can move to the target
                    bool isValidMove = false;
                    if (type == 'R') // Rook
                        isValidMove = ((x == toX || y == toY) && isPathClear(x, y, toX, toY));
                    else if (type == 'B') // Bishop
                        isValidMove = (abs(x - toX) == abs(y - toY)) && isPathClear(x, y, toX, toY);
                    else if (type == 'Q') // Queen
                        isValidMove = ((x == toX || y == toY || abs(x - toX) == abs(y - toY)) && isPathClear(x, y, toX, toY));
                    else if (type == 'N') // Knight
                        isValidMove = (abs(x - toX) == 2 && abs(y - toY) == 1) || (abs(x - toX) == 1 && abs(y - toY) == 2);

                    if (isValidMove)
                    {
                        ambiguityCount++;
                        // Check file conflict
                        if (x == fromX)
                        {
                            requiresRank = true;
                        }
                        // Check rank conflict
                        if (y == fromY)
                        {
                            requiresFile = true;
                        }

                        // If this piece conflicts in both dimensions, both disambiguators are needed
                        if (x != fromX && y != fromY)
                        {
                            requiresFile = true;
                            requiresRank = true;
                        }
                    }
                }
            }
        }
        if (ambiguityCount <= 0)
        {
            requiresFile = false;
            requiresRank = false;
        }
        if (ambiguityCount == 1 && requiresRank && requiresFile)
        {
            // Prioritize file disambiguation over rank
            requiresRank = false;
        }
        return {requiresFile, requiresRank};
    }

    bool isPathClear(int fromX, int fromY, int toX, int toY)
    {
        int dx = (toX > fromX) - (toX < fromX); // Step direction in X
        int dy = (toY > fromY) - (toY < fromY); // Step direction in Y

        for (int x = fromX + dx, y = fromY + dy; x != toX || y != toY; x += dx, y += dy)
        {
            if (!board[y][x].piece.isEmpty())
                return false;
        }
        return true;
    }

    void movePiece(int fromX, int fromY, int toX, int toY, bool isCastling = false, int rookFromX = -1, int rookToX = -1)
    {
        // En passant Handling
        if (didWEnPassant || didBEnPassant)
        {
            int enPassantY = didWEnPassant ? 4 : 3;
            board[enPassantY][toX].piece = Piece(); // Capture the pawn
        }

        // Castling Handling
        else if (isCastling)
        {
            board[fromY][rookToX].piece = board[fromY][rookFromX].piece; // Move rook
            board[fromY][rookFromX].piece = Piece();                     // Clear rook square
        }

        // Move the piece
        board[toY][toX].piece = board[fromY][fromX].piece;
        board[fromY][fromX].piece = Piece();

        didWEnPassant = false;
        didBEnPassant = false;

        // Update castling flags
        if (board[toY][toX].piece.type == "K")
        {
            if (board[toY][toX].piece.isWhite)
                hasWhiteKingMoved = true;
            else
                hasBlackKingMoved = true;
        }
        else if (board[toY][toX].piece.type == "R")
        {
            if (fromX == 0)
                (fromY == 7 ? hasWhiteRookMoved[0] : hasBlackRookMoved[0]) = true;
            else if (fromX == 7)
                (fromY == 7 ? hasWhiteRookMoved[1] : hasBlackRookMoved[1]) = true;
        }

        // Promotion
        if (board[toY][toX].piece.type == "P" && (toY == 0 || toY == SIZE - 1))
        {
            board[toY][toX].piece = Piece("Q", board[toY][toX].piece.isWhite);
            lastMove += "=Q"; // Add promotion notation
        }

        // Check and Checkmate Detection
        if (isKingInCheck(!board[toY][toX].piece.isWhite))
        {
            if (isCheckmate(!board[toY][toX].piece.isWhite))
            {
                lastMove += "#"; // Checkmate notation
                cout << "Game over: Checkmate! No more moves allowed!" << endl;
            }
            else
            {
                lastMove += "+"; // Check notation
            }
        }
        arrows.clear(); // Clear the arrows after the move
    }
};

class Game // Represents the game of chess
{
private:
    ChessBoard chessBoard;                      // The game board
    int selectedTileX = -1, selectedTileY = -1; // Track the tile where the drag started
    Sprite draggedPieceSprite;                  // Sprite for the piece being dragged
    bool isDragging = false;                    // Track if dragging is active
    Vector2f dragOffset;                        // Offset for smooth dragging
    vector<string> moveHistory;                 // Store the history of the moves
    int moveCounter = 1;                        // Count how many moves have been played
    vector<pair<int, int>> validMoves;          // Store the valid moves for the selected piece
    Font font;                                  // Store the font of the labels
    const int labelFontSize = 10;               // Font Size for cols/rows labels
    const int sideBarFontSize = 16;             // Font Size for sidebar
    map<string, Texture> *textures;             // Pointer to the textures map
    Vector2i arrowStart;                        // To store the starting point
    Vector2i arrowEnd;                          // To store the ending point
    bool isDrawingArrow = false;                // To track if the user is drawing an arrow

public:
    // Game constructor
    Game(map<string, Texture> &texturesRef) : textures(&texturesRef)
    {
        resetGame();
    }

    void resetGame()
    {
        chessBoard = ChessBoard();
        selectedTileX = -1;
        selectedTileY = -1;
        validMoves.clear();

        isWhiteTurn = true;
        isDragging = false;
        moveHistory.clear();
        moveCounter = 1;
        chessBoard.resetBoard();
        arrows.clear();
    }
    bool isGameOver()
    {
        return chessBoard.lastMove.find("#") != string::npos; // Check for checkmate symbol in the last move
    }

    void handleRMB(RenderWindow &window, Event &event)
    {
        static Vector2i clickStartTile; // To store the tile where the mouse was pressed
        bool isShortClick = false;      // Flag for short-click detection

        // Right mouse button pressed
        if (event.type == Event::MouseButtonPressed && event.mouseButton.button == Mouse::Right)
        {
            isDrawingArrow = true;

            // Store the mouse start position (snapped to the center of the tile)
            int tileX = event.mouseButton.x / TILE_SIZE;
            int tileY = event.mouseButton.y / TILE_SIZE;
            int centerX = tileX * TILE_SIZE + TILE_SIZE / 2;
            int centerY = tileY * TILE_SIZE + TILE_SIZE / 2;

            arrowStart = Vector2i(centerX, centerY);
            arrowEnd = arrowStart; // Initialize endpoint

            clickStartTile = Vector2i(tileX, tileY); // Save the start tile
        }

        // Update arrow endpoint while moving the mouse
        if (event.type == Event::MouseMoved && isDrawingArrow)
        {
            arrowEnd = Mouse::getPosition(window); // Update dynamically
        }

        // Right mouse button released
        if (event.type == Event::MouseButtonReleased && event.mouseButton.button == Mouse::Right)
        {
            // Convert mouse release position to tile indices
            int tileX = event.mouseButton.x / TILE_SIZE;
            int tileY = event.mouseButton.y / TILE_SIZE;

            // Check if the start and end tiles are the same (short click)
            if (clickStartTile.x == tileX && clickStartTile.y == tileY)
            {
                isShortClick = true; // It's a short click
            }

            // Clear arrows if it's a short click
            if (isShortClick)
            {
                arrows.clear(); // Remove all arrows
            }
            else
            {
                // Snap end position to the center of the end tile
                int centerX = tileX * TILE_SIZE + TILE_SIZE / 2;
                int centerY = tileY * TILE_SIZE + TILE_SIZE / 2;

                // Only add the arrow if start and end positions are different
                if (arrowStart.x != centerX || arrowStart.y != centerY)
                {
                    arrows.push_back({arrowStart, Vector2i(centerX, centerY)});
                }
            }

            // Stop drawing the arrow
            isDrawingArrow = false;
        }
    }

    void handleLMB(RenderWindow &window, map<string, Texture> &textures)
    {
        static bool isMousePressed = false;
        Vector2i mousePosition = Mouse::getPosition(window);

        // Button dimensions (same as in draw function)
        float buttonWidth = SIDEBAR_WIDTH - 10;
        float buttonHeight = 40;
        float buttonX = SIZE * TILE_SIZE + rowLabelWidth + 5;
        float buttonY = WINDOW_HEIGHT - buttonHeight - 10;

        // Allow "Return to Main Menu" button to work at all times
        if (Mouse::isButtonPressed(Mouse::Left))
        {
            if (!isMousePressed)
            {
                isMousePressed = true;

                // Check for the button press
                if (mousePosition.x > buttonX && mousePosition.x < buttonX + buttonWidth &&
                    mousePosition.y > buttonY && mousePosition.y < buttonY + buttonHeight)
                {
                    currentState = MENU;
                    return; // Exit early to avoid processing game input
                }

                // Regular game interaction (only if not game over)
                if (!isGameOver())
                {
                    onMousePress(mousePosition, textures);
                }
            }
            else
            {
                if (!isGameOver())
                {
                    onMouseDrag(mousePosition);
                }
            }
        }
        else
        {
            if (isMousePressed)
            {
                isMousePressed = false;

                if (!isGameOver())
                {
                    onMouseRelease(mousePosition);
                }
            }
        }
    }

    void onMousePress(Vector2i mousePosition, map<string, Texture> &textures)
    {
        int tileX = mousePosition.x / TILE_SIZE;
        int tileY = mousePosition.y / TILE_SIZE;

        if (chessBoard.isValidTile(tileX, tileY))
        {
            Piece &piece = chessBoard.getSquare(tileY, tileX).piece;
            validMoves.clear();

            if (!piece.isEmpty() && piece.isWhite == isWhiteTurn)
            {
                selectedTileX = tileX;
                selectedTileY = tileY;

                // Start dragging
                isDragging = true;

                // Ensure texture exists
                string textureKey = (piece.isWhite ? "W" : "B") + piece.type;
                if (textures.find(textureKey) == textures.end())
                {
                    cerr << "Error: Missing texture for piece type " << piece.type << endl;
                    isDragging = false;
                    return;
                }
                else
                {
                    draggedPieceSprite.setTexture(textures.at(textureKey), true);
                    draggedPieceSprite.setScale(0.5f, 0.5f); // Scale appropriately
                    dragOffset = Vector2f(mousePosition.x - tileX * TILE_SIZE, mousePosition.y - tileY * TILE_SIZE);
                    draggedPieceSprite.setPosition(
                        mousePosition.x - dragOffset.x,
                        mousePosition.y - dragOffset.y);
                }
                // Find valid moves for the selected piece
                for (int y = 0; y < SIZE; y++)
                {
                    for (int x = 0; x < SIZE; x++)
                    {
                        bool putsOpponentInCheck = false;
                        bool putsInCheckmate = false;
                        if (chessBoard.isValidMove(tileX, tileY, x, y, putsOpponentInCheck, putsInCheckmate, false, false, true))
                        {
                            if(!putsOpponentInCheck && !putsInCheckmate) // Do not allow moves that put the opponent in check or checkmate
                                validMoves.push_back({x, y});
                        }
                    }
                }
            }
        }
    }

    void onMouseDrag(Vector2i mousePosition)
    {
        if (isDragging)
        {
            // Always center the piece below the cursor
            draggedPieceSprite.setPosition(
                mousePosition.x - dragOffset.x,
                mousePosition.y - dragOffset.y);
        }
    }

    void onMouseRelease(Vector2i mousePosition)
    {
        int tileX = mousePosition.x / TILE_SIZE;
        int tileY = mousePosition.y / TILE_SIZE;

        // Clear valid moves highlight
        validMoves.clear();

        // If dragging is active
        if (isDragging)
        {
            // Ensure release is on a valid tile
            if (!chessBoard.isValidTile(tileX, tileY))
            {
                cout << "Invalid tile release at (" << tileX << ", " << tileY << ")\n";
                resetDraggingState();
                return;
            }

            // Get the piece being dragged
            Piece &fromPiece = chessBoard.getSquare(selectedTileY, selectedTileX).piece;

            // Handle king-specific castling logic
            bool isCastling = false;
            int rookFromX = -1, rookToX = -1;

            if (fromPiece.type == "K")
            {
                if (chessBoard.isValidKingMove(selectedTileX, selectedTileY, tileX, tileY, fromPiece.isWhite, isCastling, rookFromX, rookToX))
                {
                    chessBoard.movePiece(selectedTileX, selectedTileY, tileX, tileY, isCastling, rookFromX, rookToX);
                    finalizeMove();
                    return;
                }
            }

            // General move validation
            if (chessBoard.isValidMove(selectedTileX, selectedTileY, tileX, tileY))
            {
                chessBoard.movePiece(selectedTileX, selectedTileY, tileX, tileY);
                finalizeMove();
            }
            else
            {
                cout << "Invalid move attempt from (" << selectedTileX << ", " << selectedTileY
                     << ") to (" << tileX << ", " << tileY << ")\n";
            }
        }

        // Reset dragging state in all cases
        resetDraggingState();
    }

    void finalizeMove()
    {
        updateMoveHistory();
        isWhiteTurn = !isWhiteTurn; // Switch turn
        resetDraggingState();
    }

    void resetDraggingState()
    {
        isDragging = false;
        // draggedPieceSprite.setTexture(Texture()); // Clear the texture
    }

    void updateMoveHistory()
    {
        if (moveHistory.size() >= MAX_VISIBLE_MOVES)
        {
            moveHistory.erase(moveHistory.begin()); // Remove oldest move if the list is full
        }

        // Handle castling separately
        if (chessBoard.lastMove == "O-O" || chessBoard.lastMove == "O-O-O")
        {
            if (isWhiteTurn)
            {
                moveHistory.push_back(to_string(moveCounter) + ". " + chessBoard.lastMove);
                moveCounter++;
            }
            else
            {
                moveHistory.back() += " " + chessBoard.lastMove;
            }
            return;
        }

        // Add regular move notation
        if (isWhiteTurn)
        {
            moveHistory.push_back(to_string(moveCounter) + ". " + chessBoard.lastMove);
            moveCounter++;
        }
        else
        {
            moveHistory.back() += " " + chessBoard.lastMove;
        }
    }

    void draw(RenderWindow &window, map<string, Texture> &textures, map<string, Font> &fonts)
    {
        // Draw board
        for (int y = 0; y < SIZE; y++)
        {
            for (int x = 0; x < SIZE; x++)
            {
                // Draw the tiles
                RectangleShape square(Vector2f(TILE_SIZE, TILE_SIZE));
                square.setPosition(x * TILE_SIZE, y * TILE_SIZE);
                square.setTexture((x + y) % 2 == 0 ? &textures["WS1"] : &textures["BS1"]);
                window.draw(square);

                // Draw pieces, but skip the dragged piece
                Piece &piece = chessBoard.getSquare(y, x).piece;
                if (!(isDragging && selectedTileX == x && selectedTileY == y) && !piece.isEmpty())
                {
                    Sprite sprite;
                    string textureKey = (piece.isWhite ? "W" : "B") + piece.type;
                    sprite.setTexture(textures[textureKey]);
                    sprite.setPosition(x * TILE_SIZE, y * TILE_SIZE);
                    sprite.setScale(0.5f, 0.5f);
                    window.draw(sprite);
                }

                // Highlight king in check or checkmate
                if (!piece.isEmpty() && piece.type == "K" &&
                    ((piece.isWhite && chessBoard.isKingInCheck(true)) ||
                     (!piece.isWhite && chessBoard.isKingInCheck(false))))
                {
                    CircleShape outline(TILE_SIZE / 2.5f);          // Circle size matches piece size
                    outline.setFillColor(Color::Transparent);       // No fill
                    outline.setOutlineColor(Color(255, 0, 0, 128)); // Semi-transparent red
                    outline.setOutlineThickness(4.0f);              // Thickness of the outline
                    outline.setPosition(
                        x * TILE_SIZE + TILE_SIZE / 2.0f - outline.getRadius(),
                        y * TILE_SIZE + TILE_SIZE / 2.0f - outline.getRadius());
                    window.draw(outline);
                }
            }
        }

        // Draw row and column labels
        for (int i = 0; i < SIZE; i++)
        {
            // Draw column letters (a-h)
            Text colText;
            colText.setFont(fonts["arial"]);
            colText.setString(string(1, char('a' + i)));
            colText.setCharacterSize(labelFontSize);
            colText.setFillColor(Color::White);
            colText.setPosition(i * TILE_SIZE + TILE_SIZE / 2 - labelFontSize / 2, SIZE * TILE_SIZE + colLabelHeight / 2 - labelFontSize / 2); // Below board
            window.draw(colText);

            // Draw row numbers (1-8)
            Text rowText;
            rowText.setFont(fonts["arial"]);
            rowText.setString(to_string(SIZE - i));
            rowText.setCharacterSize(labelFontSize);
            rowText.setFillColor(Color::White);
            rowText.setPosition(SIZE * TILE_SIZE + rowLabelWidth / 2 - labelFontSize / 2, i * TILE_SIZE + TILE_SIZE / 2 - labelFontSize / 2); // Right of board
            window.draw(rowText);
        }

        // Draw move history sidebar
        RectangleShape sidebar(Vector2f(SIDEBAR_WIDTH, SIZE * TILE_SIZE + TILE_SIZE / 4));
        sidebar.setFillColor(Color(220, 220, 220)); // Light gray background
        sidebar.setPosition(SIZE * TILE_SIZE + rowLabelWidth, 0);
        window.draw(sidebar);

        // Draw sidebar title
        Text sidebarTitle;
        sidebarTitle.setFont(fonts["arial"]);
        sidebarTitle.setString("Move History");
        sidebarTitle.setCharacterSize(sideBarFontSize);
        sidebarTitle.setFillColor(Color::Black);
        sidebarTitle.setPosition(SIZE * TILE_SIZE + rowLabelWidth + SIDEBAR_WIDTH / 2 - 45, 10);
        window.draw(sidebarTitle);

        // Draw sidebar border
        RectangleShape sidebarBorder(Vector2f(SIDEBAR_WIDTH, 2));
        sidebarBorder.setFillColor(Color::Black);
        sidebarBorder.setPosition(SIZE * TILE_SIZE + rowLabelWidth, 30);
        window.draw(sidebarBorder);

        // Draw move history
        for (int i = 0; i < moveHistory.size(); i++)
        {
            Text moveText;
            moveText.setFont(fonts["arial"]);
            moveText.setString(moveHistory[i]);
            moveText.setCharacterSize(sideBarFontSize);
            moveText.setFillColor(Color::Black);
            moveText.setPosition(SIZE * TILE_SIZE + rowLabelWidth + 10, i * (sideBarFontSize + 2) + 40);
            window.draw(moveText);
        }

        // Button dimensions
        float buttonWidth = SIDEBAR_WIDTH - 20;
        float buttonHeight = 40;
        float buttonX = SIZE * TILE_SIZE + rowLabelWidth + 10;
        float buttonY = WINDOW_HEIGHT - buttonHeight - 10; // Position at the bottom

        // Draw the button
        RectangleShape menuButton(Vector2f(buttonWidth, buttonHeight));
        menuButton.setFillColor(Color(200, 200, 200)); // Light gray
        menuButton.setOutlineColor(Color::Black);
        menuButton.setOutlineThickness(2.0f);
        menuButton.setPosition(buttonX, buttonY);

        // Draw button text (smaller size and centered)
        Text menuText;
        menuText.setFont(fonts["arial"]);
        menuText.setString("Return to Main Menu");
        menuText.setCharacterSize(12); // Smaller font size
        menuText.setFillColor(Color::Black);

        // Center text within the button
        FloatRect textRect = menuText.getLocalBounds();
        menuText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        menuText.setPosition(buttonX + buttonWidth / 2.0f, buttonY + buttonHeight / 2.0f);

        window.draw(menuButton);
        window.draw(menuText);

        // Draw valid moves highlights
        for (auto &move : validMoves)
        {
            CircleShape highlight(TILE_SIZE / 16.0f);
            highlight.setFillColor(Color(0, 255, 0, 128));
            highlight.setPosition(move.first * TILE_SIZE + TILE_SIZE / 2.0f - highlight.getRadius(),
                                  move.second * TILE_SIZE + TILE_SIZE / 2.0f - highlight.getRadius());
            window.draw(highlight);
        }

        // Draw dragged piece on top
        if (isDragging)
        {
            window.draw(draggedPieceSprite);
        }

        // Draw arrows
        drawArrows(window);

        // Highlight game-over state
        if (isGameOver())
        {
            RectangleShape overlay(Vector2f(TILE_SIZE * SIZE + rowLabelWidth, WINDOW_HEIGHT));
            overlay.setFillColor(Color(0, 0, 0, 150)); // Semi-transparent black overlay
            window.draw(overlay);

            Text gameOverText;
            gameOverText.setFont(fonts["arial"]);
            gameOverText.setString("Game Over");
            gameOverText.setCharacterSize(32);
            gameOverText.setFillColor(Color::White);
            gameOverText.setPosition(TILE_SIZE * SIZE / 2 - 80, TILE_SIZE * SIZE / 2 - 20);
            window.draw(gameOverText);
        }
    }
    void drawArrows(RenderWindow &window)
    {
        // Draw all finalized arrows
        for (auto &arrow : arrows)
        {
            Vector2f start(arrow.first.x, arrow.first.y);
            Vector2f end(arrow.second.x, arrow.second.y);

            // Draw the arrow with a thick shaft and bold arrowhead
            drawPrettyArrow(window, start, end, Color::Green);
        }

        // Draw the temporary arrow while right-clicking
        if (isDrawingArrow && arrowStart != arrowEnd)
        {
            Vector2f start(arrowStart.x, arrowStart.y);
            Vector2f end(arrowEnd.x, arrowEnd.y);

            // Draw the temporary arrow
            drawPrettyArrow(window, start, end, Color::Green);
        }
    }

    void drawPrettyArrow(RenderWindow &window, Vector2f start, Vector2f end, Color color)
    {
        // Adjust the transparency of the color (e.g., 128 for 50% opacity)
        color.a = 128;

        // Calculate direction, length, and angle
        Vector2f direction = end - start;
        float length = sqrt(direction.x * direction.x + direction.y * direction.y);
        float angle = atan2(direction.y, direction.x) * 180 / PI;

        // 1. Draw the arrow shaft (centered rectangle)
        RectangleShape shaft(Vector2f(length - 20, 8)); // Shaft: length minus head, thickness = 8
        shaft.setFillColor(color);

        // Set origin to center-left of the rectangle
        shaft.setOrigin(0, shaft.getSize().y / 2);

        // Position and rotate the shaft
        shaft.setPosition(start);
        shaft.setRotation(angle);
        window.draw(shaft);

        // 2. Draw the arrowhead (triangle)
        ConvexShape arrowhead;
        arrowhead.setPointCount(3);
        arrowhead.setPoint(0, Vector2f(0, 0));     // Tip of the arrowhead
        arrowhead.setPoint(1, Vector2f(-20, 10));  // Bottom left corner
        arrowhead.setPoint(2, Vector2f(-20, -10)); // Bottom right corner
        arrowhead.setFillColor(color);

        // Position and rotate the arrowhead at the endpoint
        arrowhead.setPosition(end);
        arrowhead.setRotation(angle);
        window.draw(arrowhead);
    }
};

void handleMenuInput(RenderWindow &window, Event &event, Game *game)
{
    // Button dimensions (same as in drawMenu)
    float buttonWidth = 300.0f;                 // Button width
    float buttonHeight = 80.0f;                 // Button height
    float buttonSpacing = 30.0f;                // Space between buttons
    float circleCenterX = WINDOW_WIDTH / 2.0f;  // Center of the circle (X-axis)
    float circleCenterY = WINDOW_HEIGHT / 2.0f; // Center of the circle (Y-axis)

    // Button positions
    float playButtonX = circleCenterX - buttonWidth / 2.0f;
    float playButtonY = circleCenterY - buttonHeight - buttonSpacing / 2.0f;

    float exitButtonX = circleCenterX - buttonWidth / 2.0f;
    float exitButtonY = circleCenterY + buttonSpacing / 2.0f;

    if (event.type == Event::MouseButtonPressed && event.mouseButton.button == Mouse::Left)
    {
        Vector2i mousePosition = Mouse::getPosition(window);

        // Check "Play" button bounds
        if (mousePosition.x > playButtonX && mousePosition.x < playButtonX + buttonWidth &&
            mousePosition.y > playButtonY && mousePosition.y < playButtonY + buttonHeight)
        {
            currentState = PLAYING;

            // Reset gameplay state
            isWhiteTurn = true;
            game->resetGame(); // Reset the game state
        }

        // Check "Exit" button bounds
        if (mousePosition.x > exitButtonX && mousePosition.x < exitButtonX + buttonWidth &&
            mousePosition.y > exitButtonY && mousePosition.y < exitButtonY + buttonHeight)
        {
            currentState = EXIT;
        }
    }
}

void loadResources(map<string, Texture> &textures, map<string, Font> &fonts)
{
    vector<pair<string, string>> texturesToLoad = {
        {"BR", "Textures/BR.png"},
        {"BN", "Textures/BN.png"},
        {"BB", "Textures/BB.png"},
        {"BQ", "Textures/BQ.png"},
        {"BK", "Textures/BK.png"},
        {"BP", "Textures/BP.png"},
        {"WR", "Textures/WR.png"},
        {"WN", "Textures/WN.png"},
        {"WB", "Textures/WB.png"},
        {"WQ", "Textures/WQ.png"},
        {"WK", "Textures/WK.png"},
        {"WP", "Textures/WP.png"},
        {"WS1", "Textures/WS1.png"},
        {"BS1", "Textures/BS1.png"},
        {"menuBackground", "Textures/menuBackground.png"}};

    for (auto &[key, path] : texturesToLoad)
    {
        Texture texture;
        if (!texture.loadFromFile(path))
        {
            cerr << "Error: Failed to load " << path << endl;
            exit(1);
        }
        textures[key] = move(texture);
    }

    if (!fonts["arial"].loadFromFile("Fonts/arial.ttf"))
    {
        cerr << "Failed to load font" << endl;
        exit(1);
    }
}

void drawMenu(RenderWindow &window, map<string, Texture> &textures, map<string, Font> &fonts)
{
    window.clear();

    // Draw background
    Sprite background;
    background.setTexture(textures["menuBackground"]);
    background.setScale(
        static_cast<float>(WINDOW_WIDTH) / background.getTexture()->getSize().x,
        static_cast<float>(WINDOW_HEIGHT) / background.getTexture()->getSize().y);
    window.draw(background);

    // Define button sizes and positions
    float buttonWidth = 300.0f;                 // Button width
    float buttonHeight = 80.0f;                 // Button height
    float buttonSpacing = 30.0f;                // Space between buttons
    float circleCenterX = WINDOW_WIDTH / 2.0f;  // Center of the circle (X-axis)
    float circleCenterY = WINDOW_HEIGHT / 2.0f; // Center of the circle (Y-axis)

    // Play Button Position
    float playButtonY = circleCenterY - buttonHeight - buttonSpacing / 2.0f;

    // Exit Button Position
    float exitButtonY = circleCenterY + buttonSpacing / 2.0f;

    // Button Colors
    Color buttonFillColor(240, 240, 240);    // Light gray
    Color buttonOutlineColor(100, 100, 100); // Darker gray for outline
    Color textColor(50, 50, 50);             // Dark text

    // Load font
    Font &font = fonts["arial"];

    // Draw "Play" button
    RectangleShape playButton(Vector2f(buttonWidth, buttonHeight));
    playButton.setFillColor(buttonFillColor);
    playButton.setOutlineColor(buttonOutlineColor);
    playButton.setOutlineThickness(4.0f);
    playButton.setPosition(circleCenterX - buttonWidth / 2.0f, playButtonY);
    window.draw(playButton);

    // Draw "Play" button text
    Text playText;
    playText.setFont(font);
    playText.setString("1. Play");
    playText.setCharacterSize(40);
    playText.setFillColor(textColor);
    playText.setStyle(Text::Bold);
    playText.setPosition(circleCenterX - buttonWidth / 4.0f, playButtonY + buttonHeight / 4.0f);
    window.draw(playText);

    // Draw "Exit" button
    RectangleShape exitButton(Vector2f(buttonWidth, buttonHeight));
    exitButton.setFillColor(buttonFillColor);
    exitButton.setOutlineColor(buttonOutlineColor);
    exitButton.setOutlineThickness(4.0f);
    exitButton.setPosition(circleCenterX - buttonWidth / 2.0f, exitButtonY);
    window.draw(exitButton);

    // Draw "Exit" button text
    Text exitText;
    exitText.setFont(font);
    exitText.setString("2. Exit");
    exitText.setCharacterSize(40);
    exitText.setFillColor(textColor);
    exitText.setStyle(Text::Bold);
    exitText.setPosition(circleCenterX - buttonWidth / 4.0f, exitButtonY + buttonHeight / 4.0f);
    window.draw(exitText);

    window.display();
}

int main()
{
    // Create window with the required size
    RenderWindow window(VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Chess Game");

    // Load resources (textures and fonts)
    map<string, Texture> textures;
    map<string, Font> fonts;
    loadResources(textures, fonts);

    // Initialize the game object and pass the textures map
    Game *game = new Game(textures); // Global game instance

    // Main game loop
    while (window.isOpen())
    {
        Event event;

        // Poll events
        while (window.pollEvent(event))
        {
            // Handle window close event
            if (event.type == Event::Closed)
            {
                window.close();
            }

            // Handle different game states
            if (currentState == MENU)
            {
                handleMenuInput(window, event, game); // Handle menu interactions
            }
            else if (currentState == PLAYING)
            {
                game->handleLMB(window, textures); // Handle gameplay interactions
                game->handleRMB(window, event);    // Right mouse button for arrows
            }
        }

        // Rendering based on the current state
        if (currentState == MENU)
        {
            // Render the main menu
            drawMenu(window, textures, fonts);
        }
        else if (currentState == PLAYING)
        {
            // Render the game
            window.clear();
            game->draw(window, textures, fonts);
            window.display();
        }
        else if (currentState == EXIT)
        {
            window.close(); // Exit the game
        }
    }

    return 0;
}