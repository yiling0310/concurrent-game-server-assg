#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <pthread.h>
#include <cstdio>
#include <cctype>
#include <ctime>

using namespace std;

// === MUST MATCH SERVER CONSTANTS ===
#define BOARD_SIZE 36 
#define MAX_PLAYERS 5
#define SHM_NAME "/game_memory" 
#define FIFO_NAME "/tmp/game_pipe"

#define PHASE_LOBBY 0
#define PHASE_SYMBOL_SELECT 1
#define PHASE_GAMEPLAY 2

// === MUST MATCH SERVER STRUCT ===
typedef struct {
    char board[BOARD_SIZE];
    char symbols[MAX_PLAYERS];
    int player_pids[MAX_PLAYERS];
    int player_scores[MAX_PLAYERS];
    int pending_moves[MAX_PLAYERS];
    int last_move_result[MAX_PLAYERS];
    int connected_player;     
    int required_players;    
    int current_player_index;
    int game_phase; 
    int game_over;
    int winner;
    time_t turn_start_time;
    char global_message[256]; 
    pthread_mutex_t game_mutex;
} GameState;

// Function to show the Reference Grid (Numbers only)
void display_reference_grid(int p) {    
    if(p == 3){
        cout << "\n=====GRID LABELS=====\n";
        cout << "     |     |     |     \n";
        cout << "  1  |  2  |  3  |  4  \n";
        cout << "_____|_____|_____|_____\n";
        cout << "     |     |     |     \n";
        cout << "  5  |  6  |  7  |  8  \n";
        cout << "_____|_____|_____|_____\n";
        cout << "     |     |     |     \n";
        cout << "  9  | 10  | 11  | 12  \n";
        cout << "_____|_____|_____|_____\n";
        cout << "     |     |     |     \n";
        cout << "  13 | 14  | 15  | 16  \n";
        cout << "     |     |     |     \n";
    }

    else if(p == 4){
        cout << "\n========GRID LABELS========\n";
        cout << "     |     |     |     |     \n";
        cout << "  1  |  2  |  3  |  4  |  5  \n";
        cout << "_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     \n";
        cout << "  6  |  7  |  8  |  9  | 10  \n";
        cout << "_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     \n";
        cout << " 11  | 12  | 13  | 14  | 15  \n";
        cout << "_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     \n";
        cout << " 16  | 17  | 18  | 19  | 20  \n";
        cout << "_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     \n";
        cout << " 21  | 22  | 23  | 24  | 25  \n";
        cout << "     |     |     |     |     \n";
    }

    else if(p == 5){
        cout << "\n===========GRID LABELS===========\n";
        cout << "     |     |     |     |     |     \n";
        cout << "  1  |  2  |  3  |  4  |  5  |  6  \n";
        cout << "_____|_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     |     \n";
        cout << "  7  |  8  |  9  | 10  | 11  | 12  \n";
        cout << "_____|_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     |     \n";
        cout << " 13  | 14  | 15  | 16  | 17  | 18  \n";
        cout << "_____|_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     |     \n";
        cout << " 19  | 20  | 21  | 22  | 23  | 24  \n";
        cout << "_____|_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     |     \n";
        cout << " 25  | 26  | 27  | 28  | 29  | 30  \n";
        cout << "_____|_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     |     \n";
        cout << " 31  | 32  | 33  | 34  | 35  | 36  \n";
        cout << "     |     |     |     |     |     \n";
    }
}

#define B(x) (state->board[x] ? state->board[x] : (char)('0' + 0)) 

void display_board_grid(GameState *state, int p) {
    cout << "\n======= GAME BOARD ========="<< endl;
    cout << "Players: ";
    for(int i=0; i<state->required_players; i++) {
        char s = state->symbols[i];
        cout << "P" << (i+1) << ":[" << (s ? s : '?') << "] ";
    }
    cout << endl;

    auto P = [&](int idx) {
        if (state->board[idx] != 0) {
            cout << "  " << state->board[idx] << "  ";
        } else {
            cout << "     "; // Empty space
        }
    };


    if (p == 3) { 
        cout << "     |     |     |     \n";
        P(0); cout << "|"; P(1); cout << "|"; P(2); cout << "|"; P(3); cout << "\n";
        cout << "_____|_____|_____|_____\n";
        cout << "     |     |     |     \n";
        P(4); cout << "|"; P(5); cout << "|"; P(6); cout << "|"; P(7); cout << "\n";
        cout << "_____|_____|_____|_____\n";
        cout << "     |     |     |     \n";
        P(8); cout << "|"; P(9); cout << "|"; P(10); cout << "|"; P(11); cout << "\n";
        cout << "_____|_____|_____|_____\n";
        cout << "     |     |     |     \n";
        P(12); cout << "|"; P(13); cout << "|"; P(14); cout << "|"; P(15); cout << "\n";
        cout << "     |     |     |     \n";
    }
    else if (p == 4) { 
        cout << "     |     |     |     |     \n";
        P(0); cout << "|"; P(1); cout << "|"; P(2); cout << "|"; P(3); cout << "|"; P(4); cout << "\n";
        cout << "_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     \n";
        P(5); cout << "|"; P(6); cout << "|"; P(7); cout << "|"; P(8); cout << "|"; P(9); cout << "\n";
        cout << "_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     \n";
        P(10); cout << "|"; P(11); cout << "|"; P(12); cout << "|"; P(13); cout << "|"; P(14); cout << "\n";
        cout << "_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     \n";
        P(15); cout << "|"; P(16); cout << "|"; P(17); cout << "|"; P(18); cout << "|"; P(19); cout << "\n";
        cout << "_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     \n";
        P(20); cout << "|"; P(21); cout << "|"; P(22); cout << "|"; P(23); cout << "|"; P(24); cout << "\n";
        cout << "     |     |     |     |     \n";
    }
    else if (p == 5) { 
        cout << "     |     |     |     |     |     \n";
        P(0); cout << "|"; P(1); cout << "|"; P(2); cout << "|"; P(3); cout << "|"; P(4); cout << "|"; P(5); cout << "\n";
        cout << "_____|_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     |     \n";
        P(6); cout << "|"; P(7); cout << "|"; P(8); cout << "|"; P(9); cout << "|"; P(10); cout << "|"; P(11); cout << "\n";
        cout << "_____|_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     |     \n";
        P(12); cout << "|"; P(13); cout << "|"; P(14); cout << "|"; P(15); cout << "|"; P(16); cout << "|"; P(17); cout << "\n";
        cout << "_____|_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     |     \n";
        P(18); cout << "|"; P(19); cout << "|"; P(20); cout << "|"; P(21); cout << "|"; P(22); cout << "|"; P(23); cout << "\n";
        cout << "_____|_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     |     \n";
        P(24); cout << "|"; P(25); cout << "|"; P(26); cout << "|"; P(27); cout << "|"; P(28); cout << "|"; P(29); cout << "\n";
        cout << "_____|_____|_____|_____|_____|_____\n";
        cout << "     |     |     |     |     |     \n";
        P(30); cout << "|"; P(31); cout << "|"; P(32); cout << "|"; P(33); cout << "|"; P(34); cout << "|"; P(35); cout << "\n";
        cout << "     |     |     |     |     |     \n";
    }
}

int main(){
    cout << "--- Client Started ---" << endl;
    int my_pid = getpid(); 

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if(shm_fd == -1) { 
        perror("Server not found (shm_open failed)"); 
        return 1; 
    }

    GameState* game = (GameState*)mmap(NULL, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (game == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    bool i_am_host = false;

    while(true) {
        pthread_mutex_lock(&game->game_mutex);

        if (game->game_over == 0 && game->required_players > 0 && game->connected_player >= game->required_players) {
            cout << " ERROR: THE GAME IS FULL!" << endl;
            pthread_mutex_unlock(&game->game_mutex);
            return 0;
        }

        if (game->game_over == 1) {
            game->game_over = 2; 
            i_am_host = true;
            pthread_mutex_unlock(&game->game_mutex);
            break; 
        }

        if (game->game_over == 2) {
            pthread_mutex_unlock(&game->game_mutex);
            cout << " ... Waiting for the new Host to setup the game ... " << endl;
            sleep(1); 
            continue; 
        }

        if (game->connected_player == 0) {
            i_am_host = true;
        }

        pthread_mutex_unlock(&game->game_mutex);
        break; // Proceed
    }

    int p_count = 0;
    if (i_am_host) {
        cout << ">>> You are Player 1 (HOST) <<<" << endl;
        do {
            cout << "Enter Number of Players (3-5): ";
            cin >> p_count;
        } while (p_count < 3 || p_count > 5);
    } else {
        cout << ">>> Connecting... <<<" << endl;
    }

    int pipe_fd = open(FIFO_NAME, O_WRONLY); 
    if (pipe_fd == -1) {
        perror("Failed to open pipe");
        return 1;
    }

    char msg[50];
    if (i_am_host) {
        sprintf(msg, "INIT %d %d", p_count, my_pid);
    } else {
        sprintf(msg, "JOIN %d", my_pid);
    }
    write(pipe_fd, msg, sizeof(msg));
    close(pipe_fd);
    
    cout << "Waiting for ID ..." << endl;
    int player_id = -1;
    while(player_id == -1) {
        pthread_mutex_lock(&game->game_mutex);
        for(int i=0; i<MAX_PLAYERS; i++) {
            if(game->player_pids[i] == my_pid) {
                player_id = i;
                break;
            }
        }
        pthread_mutex_unlock(&game->game_mutex);
        usleep(100000);
    }

    // 5. Main Loop
    while(true) {
        system("clear");
        int phase = game->game_phase;
        int current_turn = game->current_player_index;
        char symbol_choose = game->symbols[player_id];
        
        cout << "Player " << (player_id + 1);
        if (symbol_choose) cout << " (" << symbol_choose << ")";
        else cout << " (No Symbol)";
        cout << endl;
        
        cout << "Server Msg: " << game->global_message << endl;
        cout << "----------------------------------------" << endl;

        // LOBBY
        if (phase == PHASE_LOBBY) {
            cout << "        LOBBY WAITING ROOM        " << endl;
            cout << "Joined: " << game->connected_player << " / " << game->required_players << endl;
            sleep(1);
            continue;
        }

        // SYMBOL SELECTION
        if (phase == PHASE_SYMBOL_SELECT) {
            cout << "      PHASE: SYMBOL SELECTION     " << endl;
            for(int i=0; i<game->required_players; i++) {
                char s = game->symbols[i];
                cout << "Player " << (i+1) << ": " << (s ? s : '?') << endl;
            }

            if (current_turn == player_id) {
                cout << "\n>>> YOUR TURN TO CHOOSE SYMBOL! <<<" << endl;
                cout << "Enter a letter (A-Z): ";
                char in_sym;
                cin >> in_sym;
                in_sym = toupper(in_sym);
                
                game->pending_moves[player_id] = (int)in_sym;
                
                cout << "Validating..." << endl;
                sleep(1);
                
                if (game->last_move_result[player_id] == -1) {
                    game->last_move_result[player_id] = 0;
                    sleep(2);
                }
            } else {
                cout << "\nWaiting for Player " << (current_turn + 1) << " to choose..." << endl;
                sleep(1);
            }
            continue;
        }

        if (phase == PHASE_GAMEPLAY) {
            
            display_reference_grid(game->required_players);

            display_board_grid(game, game->required_players);

            if (game->game_over) {
                 cout << "\nGAME OVER!" << endl;
                 if(game->winner == player_id) cout << "YOU WIN!!!" << endl;
                 else if(game->winner == -1) cout << "IT IS A DRAW!" << endl;
                 else cout << "Player " << (game->winner+1) << " wins." << endl;
                 cout << "Run ./client to play again" << endl;
                 break;
            }

            if (current_turn == player_id) {
                    cout << "\n>>> YOUR TURN! Enter grid number: " << flush;
                    
                    fd_set readfds;
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    struct timeval tv;
                    tv.tv_sec = 2; // Check every 2 seconds
                    tv.tv_usec = 0;

                    int retval = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);

                    if (retval > 0) {
                        int move; 
                        cin >> move;

                        if (cin.fail()) {
                            cin.clear();
                            cin.ignore(1000, '\n');
                            cout << "Invalid Input! Numbers only." << endl;
                            sleep(1);
                            continue;
                        }
                        game->pending_moves[player_id] = move;
                        sleep(1);
                        if (game->last_move_result[player_id] == -1) {
                            cout << "INVALID MOVE!" << endl;
                            game->last_move_result[player_id] = 0;
                            sleep(2);
                        }
                    }
                } else {
                    char turn_sym = game->symbols[current_turn];
                    cout << "\nWaiting for Player " << (current_turn + 1) << " (" << turn_sym << ")..." << endl;
                    sleep(2);
                }
        }
    }
    return 0;
}