#include <iostream>
#include <vector>
#include <string>

using namespace std;


// Function to print the Tic-Tac-Toe board
void printBoard(const vector<vector<char>>& board) {
    std::cout << std::endl;
    for (int i = 0; i < 3; i++){
        for (int j = 0; j < 3; j++){
            if (j == 2){
                if (board[i][j] == ' ')
                {
                    std::cout << "  " << std::endl;
                }
                if (board[i][j] == 'X')
                {
                    std::cout << "X " << std::endl;
                }
                if (board[i][j] == 'O')
                {
                    std::cout << "O " << std::endl;
                }
                if (i != 2)
                {
                    std::cout << "_________" << std::endl << std::endl;
                }
            }
            else {
                if (board[i][j] == ' ') std::cout << "  | ";
                if (board[i][j] == 'X') std::cout << "X | ";
                if (board[i][j] == 'O') std::cout << "O | ";
            }
        }
    }
    std::cout << std::endl;
}

// Function to check if a player has won
bool checkWin(const vector<vector<char>>& board, char player) {
    // Check rows and columns
    for (int i = 0; i < 3; ++i) {
        if ((board[i][0] == player && board[i][1] == player && board[i][2] == player) ||
            (board[0][i] == player && board[1][i] == player && board[2][i] == player)) {
            return true;
        }
    }

    // Check diagonals
    if ((board[0][0] == player && board[1][1] == player && board[2][2] == player) ||
        (board[0][2] == player && board[1][1] == player && board[2][0] == player)) {
        return true;
    }

    return false;
}

bool gameOver(const vector<vector<char>>& board) {
        // Check if the program has won
        if (checkWin(board, 'O')) {
            cout << "I win" << endl;
            return 1;
        }
        // Check if the player has won
        if (checkWin(board, 'X')) {
            cout << "I lost" << endl;
            return 1;
        }
        return 0;
}

bool badArgv(string strategy){
    for (size_t i = 1; i < 10; i++){
        if(!(strategy.find(static_cast<char>(i + '0'))<strategy.length())){
            return true;
        }
    }
    return false;
}

void doMove(vector<vector<char>>& board, char symbol,int move){
            // Update the board with the player's move
            int row = (move - 1) / 3;
            int col = (move - 1) % 3;
            board[row][col] = symbol;
}

int main(int argc, char* argv[]) {
    
    if (argc != 2) {
        std::cout << "Error: No strategy was provided." << std::endl;
        exit(1);
    }
    
    
    string strategy = argv[1];
    // Validate strategy
    if (strategy.size() != 9 || badArgv(strategy)) {
        std::cout << "Error: Strategy must have nine digits." << std::endl;
        exit(1);
    }

    vector<vector<char>> board(3, vector<char>(3, ' '));
    char programSymbol = 'O';
    char playerSymbol = 'X';
    // Main game loop
    for (size_t i = 9; i > 0; i--){
        printBoard(board);
        if (gameOver(board)) return 0;
        if (i%2){
            // Determine the next move based on the strategy
            int nextMove = 0;
            for (char digit : strategy) {
                int position = digit - '0';
                int row = (position - 1) / 3;
                int col = (position - 1) % 3;
                if (board[row][col] == ' ') {
                    nextMove = position;
                    break;
                }
            }
            doMove(board,programSymbol, nextMove);
            cout << nextMove << endl;
        }else{
            // Player's move
            int playerMove;
            cin >> playerMove;
            // Validate player's move
            if (playerMove < 1 || playerMove > 9 || board[(playerMove - 1) / 3][(playerMove - 1) % 3] != ' ') {
                std::cout << "Error: Illegal operation - The place is not available." << std::endl;
                exit(1);
            }
            // Update the board with the player's move
            doMove(board,playerSymbol, playerMove);
            }
    }
    if (gameOver(board)) return 0;
    cout << "DRAW" << endl;
    return 0;
}
