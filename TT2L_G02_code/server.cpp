#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <pthread.h>
#include <cstdio>
#include <cctype>
#include <ctime>
#include <csignal> 
#include <cerrno>

using namespace std;

#define BOARD_SIZE 36 
#define MAX_PLAYERS 5
#define SHM_NAME "/game_memory" 
#define FIFO_NAME "/tmp/game_pipe"

// Buffer settings
#define MAX_LOGS 50    
#define LOG_LEN 128    

#define PHASE_LOBBY 0
#define PHASE_SYMBOL_SELECT 1
#define PHASE_GAMEPLAY 2

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
    
    // Synch primitives
    pthread_mutex_t game_mutex;
    pthread_mutex_t log_mutex; // Process-shared mutex
    
    // Circular buffer for logger
    char log_queue[MAX_LOGS][LOG_LEN]; 
    int log_head;  
    int log_tail;  
    int log_count; 
} GameState;

GameState* global_shm_ptr = NULL;

// Producer: Adds message to buffer
void write_log(const char* message) {
    if (!global_shm_ptr) return;

    pthread_mutex_lock(&global_shm_ptr->log_mutex);
    
    if (global_shm_ptr->log_count < MAX_LOGS) {
        time_t now = time(NULL);
        char timeBuf[64];
        strftime(timeBuf, 64, "%Y-%m-%d %H:%M:%S", localtime(&now));
        
        char formatted[LOG_LEN];
        snprintf(formatted, LOG_LEN, "[%s] %s", timeBuf, message);
        
        strncpy(global_shm_ptr->log_queue[global_shm_ptr->log_tail], formatted, LOG_LEN - 1);
        global_shm_ptr->log_queue[global_shm_ptr->log_tail][LOG_LEN - 1] = '\0';
        
        global_shm_ptr->log_tail = (global_shm_ptr->log_tail + 1) % MAX_LOGS;
        global_shm_ptr->log_count++;
    }
    
    pthread_mutex_unlock(&global_shm_ptr->log_mutex);
}

// Consumer: Reads from buffer and writes to file
void* logger_thread(void* arg) { 
    GameState* game = (GameState*)arg;
    write_log("System: Logger Thread Started.");
    cout << "[SYSTEM] Logger Thread initialized." << endl; 

    while(true) {
        char buffer[LOG_LEN];
        int has_data = 0;

        pthread_mutex_lock(&game->log_mutex);
        if (game->log_count > 0) {
            strncpy(buffer, game->log_queue[game->log_head], LOG_LEN);
            game->log_head = (game->log_head + 1) % MAX_LOGS;
            game->log_count--;
            has_data = 1;
        }
        pthread_mutex_unlock(&game->log_mutex);

        if (has_data) {
            FILE* logFile = fopen("game.log", "a");
            if (logFile) {
                fprintf(logFile, "%s\n", buffer);
                fclose(logFile);
            }
        } else {
            usleep(50000); 
        }
    }
    return NULL; 
}

void load_scores(GameState* game) {
    cout << "[SYSTEM] Reading 'scores.txt' for persistence..." << endl; 
    FILE* scoresFile = fopen("scores.txt", "r");
    if (scoresFile) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (fscanf(scoresFile, "%d", &game->player_scores[i]) != 1) game->player_scores[i] = 0;
        }
        fclose(scoresFile);
        write_log("Persistence: Scores loaded.");
    } 
    else {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            game->player_scores[i] = 0;
        }
        write_log("Persistence: No scores found. Created new.");
    }
}

void save_scores(GameState* game) {
    cout << "[SYSTEM] Writing final scores to 'scores.txt'..." << endl; 
    FILE* scoresFile = fopen("scores.txt", "w");
    if (scoresFile) {
        for (int i = 0; i < MAX_PLAYERS; i++){
            fprintf(scoresFile, "%d\n", game->player_scores[i]);
        }
        fclose(scoresFile);
        write_log("Persistence: Scores saved.");
    }
}

void log_join(int player_idx, int pid) {
    char buffer[128];
    sprintf(buffer, "Connection: Player %d joined the game (PID: %d)", player_idx + 1, pid);
    write_log(buffer);
}

void log_turn(int player_idx) {
    char buffer[128];
    sprintf(buffer, "Turn Change: It is now Player %d's turn", player_idx + 1);
    write_log(buffer);
}

void log_move(int player_idx, int move_idx, char symbol, int total_players){
    int grid_cols = (total_players < 3) ? 4 : (total_players + 1);
    if (grid_cols > 6) grid_cols = 6;
    
    char buffer[256];
    sprintf(buffer, "Player %d (%c) placed symbol on Grid %d ", 
            player_idx + 1, symbol, move_idx + 1);
    write_log(buffer);
}

void log_symbol(int player_idx, char symbol) {
    char buffer[128];
    sprintf(buffer, "Player %d selected symbol '%c'", player_idx + 1, symbol);
    write_log(buffer);
}

void log_winner(int player_idx){
    char buffer[128];
    sprintf(buffer, "RESULT: GAME OVER. The Winner is Player %d!", player_idx + 1);
    write_log(buffer);
}

void log_draw(){
    write_log("RESULT: GAME OVER. It is a Draw.");
}

void init_game(GameState *game){
    cout << "[SYSTEM] Initializing Game State in Shared Memory..." << endl; 
    for (int i=0; i<BOARD_SIZE; i++) game->board[i] = 0;
    for (int i=0; i<MAX_PLAYERS; i++) {
        game->pending_moves[i] = 0;
        game->last_move_result[i] = 0;
        game->symbols[i] = 0; 
    }
    
    game->log_head = 0;
    game->log_tail = 0;
    game->log_count = 0;

    game->connected_player = 0;
    game->current_player_index = 0;
    game->game_over = 0;
    game->winner = -1;
    game->game_phase = PHASE_LOBBY;
    game->turn_start_time = time(NULL);
    sprintf(game->global_message, "Waiting for players to join...");
    write_log("Game Initialized.");
}

int check_winner(GameState* game) {
    int p = game->required_players;
    int grid_cols = (p < 3) ? 4 : (p + 1);
    
    for(int r=0; r<grid_cols; r++) {
        int idx = r*grid_cols;
        if(game->board[idx] == 0) continue;
        bool win = true;
        for(int c=1; c<grid_cols; c++) if(game->board[idx+c] != game->board[idx]) win = false;
        if(win) return 1;
    }
    for(int c=0; c<grid_cols; c++) {
        if(game->board[c] == 0) continue;
        bool win = true;
        for(int r=1; r<grid_cols; r++) if(game->board[c + r*grid_cols] != game->board[c]) win = false;
        if(win) return 1;
    }
    if(game->board[0] != 0) {
        bool win = true;
        for(int i=1; i<grid_cols; i++) if(game->board[i*(grid_cols+1)] != game->board[0]) win = false;
        if(win) return 1;
    }
    if(game->board[grid_cols-1] != 0) {
        bool win = true;
        for(int i=1; i<grid_cols; i++) if(game->board[(i+1)*(grid_cols-1)] != game->board[grid_cols-1]) win = false;
        if(win) return 1;
    }
    return 0;
}

int check_draw(GameState* game) {
    int p = game->required_players;
    int grid_cols = (p < 3) ? 4 : (p + 1);
    int total = grid_cols * grid_cols;
    for(int i=0; i<total; i++) if(game->board[i] == 0) return 0;
    return 1;
}

void* schedule_thread(void* arg) {
    cout << "[SYSTEM] Scheduler Thread initialized." << endl; 
    GameState* state = (GameState*)arg;
    write_log("Scheduler Started.");
    while(true) {
        sleep(1);

        pthread_mutex_lock(&state->game_mutex);

        if (state->game_phase == PHASE_GAMEPLAY && !state->game_over) {
            
            double elapsed = difftime(time(NULL), state->turn_start_time);

            if (elapsed >= 20.0) {
                int current = state->current_player_index;
                state->current_player_index = (current + 1) % state->required_players;
                
                state->turn_start_time = time(NULL);
                sprintf(state->global_message, "Time Up! Skipped Player %d.", current + 1);
                char logBuf[100];
                sprintf(logBuf, "[Scheduler] Forced skip for Player %d", current+1);
                cout << "[Scheduler] Forced skip for Player " << (current+1) << endl;
            }
        }
        pthread_mutex_unlock(&state->game_mutex);
    }
    return NULL;
}

void handle_sigchld(int sig) { while(waitpid(-1, NULL, WNOHANG) > 0); }
void handle_sigint(int sig) {
    if (global_shm_ptr) {
        cout << "\n[SYSTEM] SIGINT Received. Cleaning up..." << endl;
        printf("\nSaving scores and shutting down...\n");
        pthread_mutex_lock(&global_shm_ptr->game_mutex);
        save_scores(global_shm_ptr);
        pthread_mutex_unlock(&global_shm_ptr->game_mutex);

        cout << "[SYSTEM] Unlinking Shared Memory and Named Pipes..." << endl;
        shm_unlink(SHM_NAME);
        unlink(FIFO_NAME);
    }
    exit(0);
}

int main(){
    shm_unlink(SHM_NAME); 
    unlink(FIFO_NAME); 

    int shm_fd;
    GameState* shm_ptr = nullptr;

    shm_fd = shm_open(SHM_NAME, O_CREAT|O_RDWR, 0666);
    if(shm_fd == -1){
        perror("shm_open failed");
        return 1;
    }

    if(ftruncate(shm_fd, sizeof(GameState)) == -1){
        perror("ftruncate failed");
        shm_unlink(SHM_NAME);
        return 1;
    };

    cout << "[SYSTEM] Shared Memory Created: " << SHM_NAME << endl; 

    shm_ptr = (GameState*)mmap(NULL, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(shm_ptr == MAP_FAILED){
        perror("mmap failed");
        shm_unlink(SHM_NAME);
        return 1;
    }

    cout << "[SYSTEM] Memory Mapped at address: " << shm_ptr << endl; 

    global_shm_ptr = shm_ptr;

    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
    
    pthread_mutex_init(&shm_ptr->game_mutex, &mutexAttr);
    pthread_mutex_init(&shm_ptr->log_mutex, &mutexAttr);

    cout << "[SYSTEM] Process-Shared Mutexes Initialized." << endl;
    
    init_game(shm_ptr);
    load_scores(shm_ptr);

    if (mkfifo(FIFO_NAME, 0666) == -1 && errno != EEXIST){
        perror("FIFO");
        shm_unlink(SHM_NAME);
        return 1;
    }

    cout << "[SYSTEM] Named Pipe (FIFO) Created: " << FIFO_NAME << endl; 
  
    pthread_t t1, t2;
    pthread_create(&t1, NULL, logger_thread, shm_ptr);
    pthread_create(&t2, NULL, schedule_thread, shm_ptr);
  
    signal(SIGINT, handle_sigint);
    signal(SIGCHLD, handle_sigchld);

    cout << " SERVER RUNNING. Waiting for clients..." << endl;

    while(true) {
        int pipe_fd;
        pipe_fd = open(FIFO_NAME, O_RDONLY);

        if (pipe_fd == -1) continue;

        char buffer[100];
        read(pipe_fd, buffer, sizeof(buffer));
        close(pipe_fd);

        char cmd[10]; 
        int p_count = 0; 
        int client_pid = 0;
        sscanf(buffer, "%s", cmd);

        if (strcmp(cmd, "INIT") == 0){
            sscanf(buffer, "INIT %d %d", &p_count, &client_pid);
        }
        else{
            sscanf(buffer, "JOIN %d", &client_pid);
        }

        pid_t pid = fork();

        if(pid < 0){
            perror("fork failure");
            continue;
        }

        if (pid == 0) {
            // Child process
            pthread_mutex_lock(&shm_ptr->game_mutex);

            if (strcmp(cmd, "INIT") == 0) {
                shm_ptr->required_players = p_count;
                init_game(shm_ptr); 
            }

            if (shm_ptr->required_players > 0 && shm_ptr->connected_player >= shm_ptr->required_players) {
                pthread_mutex_unlock(&shm_ptr->game_mutex);
                exit(0);
            }

            int player_id = shm_ptr->connected_player;
            shm_ptr->player_pids[player_id] = client_pid;

            shm_ptr->connected_player++;

            if (shm_ptr->connected_player == shm_ptr->required_players) {
                shm_ptr->game_phase = PHASE_SYMBOL_SELECT;
                shm_ptr->current_player_index = 0; 
                sprintf(shm_ptr->global_message, "Players are full. Choosing symbols...");
                write_log("[Game] Phase changed to SYMBOL_SELECT.");
            }
            pthread_mutex_unlock(&shm_ptr->game_mutex);

            cout << "Process for Player " << (player_id + 1) << " started." << endl;

            while (!shm_ptr->game_over) {
                if (shm_ptr->pending_moves[player_id] != 0) {
                    pthread_mutex_lock(&shm_ptr->game_mutex);

                    if (shm_ptr->current_player_index == player_id) {

                        if (shm_ptr->game_phase == PHASE_SYMBOL_SELECT) {
                            char symbol_choose = (char)shm_ptr->pending_moves[player_id];
                            bool taken = false;
                            for(int i=0; i<shm_ptr->required_players; i++){ 
                                if(shm_ptr->symbols[i] == symbol_choose) taken = true;
                            }

                            if (!taken && isalpha(symbol_choose)) {
                                shm_ptr->symbols[player_id] = symbol_choose;
                                shm_ptr->last_move_result[player_id] = 1;
                                log_symbol(player_id, symbol_choose);
                                shm_ptr->current_player_index++;

                                if (shm_ptr->current_player_index >= shm_ptr->required_players) {
                                    shm_ptr->game_phase = PHASE_GAMEPLAY;
                                    shm_ptr->current_player_index = 0;
                                    shm_ptr->turn_start_time = time(NULL);
                                    sprintf(shm_ptr->global_message, "Game Started! Player 1's Turn.");
                                    write_log("[Game] Phase changed to GAMEPLAY.");
                                } 
                                else sprintf(
                                    shm_ptr->global_message, "Player %d chose %c. Next...", player_id+1, symbol_choose
                                );
                            } else shm_ptr->last_move_result[player_id] = -1;
                        } 

                        else if (shm_ptr->game_phase == PHASE_GAMEPLAY) {
                            int idx = shm_ptr->pending_moves[player_id] - 1;

                            if (idx >= 0 && idx < BOARD_SIZE && shm_ptr->board[idx] == 0) {
                                shm_ptr->board[idx] = shm_ptr->symbols[player_id];
                                shm_ptr->last_move_result[player_id] = 1;
                                log_move(player_id, idx, shm_ptr->symbols[player_id], shm_ptr->required_players);

                                if (check_winner(shm_ptr)) {
                                    shm_ptr->game_over = 1;
                                    shm_ptr->winner = player_id;
                                    shm_ptr->player_scores[player_id]++;
                                    save_scores(shm_ptr); 
                                    log_winner(player_id);
                                    sprintf(shm_ptr->global_message, "Player %d Wins!", player_id + 1);                                    
                                    cout << "Player " << player_id + 1 << " has WON the game!" << endl;
                                    cout << " If you want shut down and save scores, PRESS Ctrl+C" << endl;
                                } 
                                else if (check_draw(shm_ptr)) {
                                    shm_ptr->game_over = 1;
                                    shm_ptr->winner = -1;
                                    log_draw();
                                    sprintf(shm_ptr->global_message, "Draw!");
                                    cout << "The game ended in a DRAW." << endl;
                                    cout << " If you want to shut down and save scores, PRESS Ctrl+C" << endl;
                                } 
                                else {
                                    shm_ptr->current_player_index = (shm_ptr->current_player_index + 1) % shm_ptr->required_players;
                                    shm_ptr->turn_start_time = time(NULL);
                                    sprintf(shm_ptr->global_message, "Next Turn: Player %d", shm_ptr->current_player_index+1);
                                    log_turn(shm_ptr->current_player_index);                             
                                }
                            } else shm_ptr->last_move_result[player_id] = -1; 
                        }
                    }
                    shm_ptr->pending_moves[player_id] = 0; 
                    pthread_mutex_unlock(&shm_ptr->game_mutex);
                }
                usleep(100000);
            }
            exit(0);
        }
    }
    return 0;
}