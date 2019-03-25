#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "structs.h"

/* Function definitions */
int check_parameters(int argc, char** argv);
FILE* load_deck(char** argv);
Card* process_deck_file(FILE* deckFile, int* deckCardsCount);
int nonnegative_integer(char* arg);
void initiate_game(Card* market, Card* deck, int deckCardsCount, int* nextCard,
        int* gameInitiated, int** fds1, int playerCount, int tokens,
        int* availableTokens, int* cardsCount);
char* get_player_response(int*** fds, int** fds2, int playerNumber,
        int playerCount, int** fds1, int** fds3, Card* deck, Player* players,
        int* pids);
void send_message_to_players(char* message, int** fds1, int exception,
        int playerCount);
void initialise_players(Player* players, int playerCount);
int process_player_message(char* message, Card* market, int playerNumber,
        int playerCount, int* messageProcessed, int* availableTokens,
        Player* players, int* cardsCount, int** fds1, int* cardPurchased);
void prompt_player(int** fds1, int playerNumber);
void add_next_card_to_market(Card* deck, Card* market,
        int deckCardsCount, int* nextCard, int** fds1, int playerCount,
        int* cardsCount);
int process_game_round(int*** fds, int** fds1, int** fds2, int playerCount,
        int* cardsCount, Card* market, Card* deck, int* availableTokens,
        Player* players, int* nextCard, int deckCardsCount, int** fds3,
        int* pids);
int check_eog(Player* players, int playerCount, int* gameEnded,
        int pointThreshold, int cardsCount, int** fds1, int* pids);
void close_fds(int** fds1, int** fds2, int** fds3, int playerCount,
        int playerNumber);
void free_fds(int*** fds, int** fds1, int** fds2, int** fds3,
        int playerCount);
void terminate_players(int* pids, int playerCount, int** fds1,
        int printMessage);

volatile sig_atomic_t gotSignal = 0; // Indicate if SIGINT has been detected

/* Sets gotSignal to true if SIGINT detected. */
void sig_handler(int sig) {
    gotSignal = sig;
}

/* Sets up signal handling. */
void setup_signals(void) {
    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE
    struct sigaction sa = {
        .sa_handler = sig_handler
    };
    sigaction(SIGINT, &sa, 0);
}

/* Sets up the necessary pipes and returns the associated file descriptors. */
int*** setup_pipe(int playerCount, Card* deck) {
    int*** fds = malloc(sizeof(int**) * 3);
    fds[0] = malloc(sizeof(int*) * playerCount);
    fds[1] = malloc(sizeof(int*) * playerCount);
    fds[2] = malloc(sizeof(int*) * playerCount);
    int** fds1 = fds[0];
    int** fds2 = fds[1];
    int** fds3 = fds[2];
    for(int i = 0; i < playerCount; i++) {
        fds1[i] = malloc(sizeof(int) * 2);
        fds2[i] = malloc(sizeof(int) * 2);
        fds3[i] = malloc(sizeof(int) * 2);
    }
    for(int i = 0; i < playerCount; i++) {
        if (pipe(fds1[i])) {
            free(deck);
            free_fds(fds, fds1, fds2, fds3, playerCount);
            fprintf(stderr, "Bad start\n");
            exit(5);
        }
        if (pipe(fds2[i])) {
            free(deck);
            free_fds(fds, fds1, fds2, fds3, playerCount);
            fprintf(stderr, "Bad start\n");
            exit(5);
        }
        if (pipe(fds3[i])) {
            free(deck);
            free_fds(fds, fds1, fds2, fds3, playerCount);
            fprintf(stderr, "Bad start\n");
            exit(5);
        }
    }
    return fds;
}

/* Handles forking and child processes. Returns an array of ints containing
 * the process IDs of all child processes. */
int* handle_fork(int*** fds, int** fds1, int** fds2, int** fds3, char** argv,
        int playerCount, Card* deck) {
    int* pids = malloc(sizeof(int) * playerCount);
    for(int i = 0; i < playerCount; i++) {
        int pid = -1;
        switch(pid = fork()) {
            case -1:
                free(pids);
                free(deck);
                free_fds(fds, fds1, fds2, fds3, playerCount);
                fprintf(stderr, "Bad start\n");
                exit(5);
            case 0:
                /* Child */
                if (dup2(fds1[i][0], 0) == -1) {
                    fprintf(stderr, "Bad start\n");
                    exit(5);
                }
                if (dup2(fds2[i][1], 1) == -1) {
                    fprintf(stderr, "Bad start\n");
                    exit(5);
                }
                close(2);
                close_fds(fds1, fds2, fds3, playerCount, i);
                fcntl(fds3[i][1], F_SETFD, FD_CLOEXEC);
                char execArg1[3];
                char execArg2[3];
                sprintf(execArg1, "%d", playerCount);
                sprintf(execArg2, "%d", i);
                execlp(argv[4 + i], argv[4 + i], execArg1, execArg2, NULL);
                write(fds3[i][1], "", 1);
                close(fds3[i][1]);
                free_fds(fds, fds1, fds2, fds3, playerCount);
                free(pids);
                free(deck);
                _exit(0);
            default:
                pids[i] = pid;
        }
    }
    return pids;
}

/* Close the necessary pipes after forking in parent. */
void parent_close_pipes(int** fds1, int** fds2, int** fds3, int playerCount) {
    for(int i = 0; i < playerCount; i++) {
        close(fds1[i][0]);
        close(fds2[i][1]);
        close(fds3[i][1]);
    }
}

/* Check whether all player processes have started successfully. */
void check_bad_start(int*** fds, int** fds1, int** fds2, int** fds3, int* pids,
        int playerCount, Card* deck) {
    for(int i = 0; i < playerCount; i++) {
        char temp;
        if (read(fds3[i][0], &temp, 1) != 0) {
            free(deck);
            fprintf(stderr, "Bad start\n");
            terminate_players(pids, playerCount, fds1, 0);
            free_fds(fds, fds1, fds2, fds3, playerCount);
            exit(5);
        }
    }
}

/* Checks the return value of check_parameters(). */
void check_check_parameters(int val) {
    if(val == 1) {
        fprintf(stderr, "Bad argument\n");
        exit(2);
    }
}

/* Checks the deck returned by process_deck_file(). */
void check_processed_deck(Card* deck, FILE* deckFile) {
    if(deck == NULL) {
        fprintf(stderr, "Invalid deck file contents\n");
        exit(4);
    }
}

/* The main function. */
int main(int argc, char** argv) {
    setup_signals();
    check_check_parameters(check_parameters(argc, argv));
    int deckCardsCount = 0;
    FILE* deckFile = load_deck(argv);
    Card* deck = process_deck_file(deckFile, &deckCardsCount);
    fclose(deckFile);
    check_processed_deck(deck, deckFile);
    int playerCount = argc - 4;
    int*** fds = setup_pipe(playerCount, deck);
    int** fds1 = fds[0];
    int** fds2 = fds[1];
    int** fds3 = fds[2];
    int* pids = handle_fork(fds, fds1, fds2, fds3, argv, playerCount, deck);
    /* Parent */
    if(gotSignal) {
        free(deck);
        terminate_players(pids, playerCount, fds1, 0);
        free_fds(fds, fds1, fds2, fds3, playerCount);
        fprintf(stderr, "SIGINT caught\n");
        exit(10);
    }
    parent_close_pipes(fds1, fds2, fds3, playerCount);
    check_bad_start(fds, fds1, fds2, fds3, pids, playerCount, deck);
    int tokens = atoi(argv[1]); // checking has already been done
    int pointThreshold = atoi(argv[2]); // checking has already been done
    /* Game status variables */
    int gameEnded = 0, gameInitiated = 0, cardsCount = 0, nextCard = 0;
    int availableTokens[4] = {0, 0, 0, 0};
    Card market[8];
    Player* players = malloc(sizeof(Player) * playerCount);
    initialise_players(players, playerCount);
    while(!gameEnded) {
        if(!gameInitiated) {
            initiate_game(market, deck, deckCardsCount, &nextCard,
                    &gameInitiated, fds1, playerCount, tokens,
                    availableTokens, &cardsCount);
        }
        process_game_round(fds, fds1, fds2, playerCount, &cardsCount, market,
                deck, availableTokens, players, &nextCard, deckCardsCount,
                fds3, pids);
        check_eog(players, playerCount, &gameEnded, pointThreshold,
                cardsCount, fds1, pids);
    }
    free_fds(fds, fds1, fds2, fds3, playerCount);
    free(players);
    free(deck);
    return 0;
}

/* Close the pipe ends that need to be closed in child. */
void close_fds(int** fds1, int** fds2, int** fds3, int playerCount,
        int playerNumber) {
    for(int i = 0; i < playerCount; i++) {
        close(fds1[i][1]);
        close(fds1[i][0]);
        close(fds2[i][0]);
        close(fds2[i][1]);
        close(fds3[i][0]);
        if(i != playerNumber) {
            close(fds3[i][1]);
        }
    }
}

/* Check (invoked after every game round) whether the game has ended and act
accordingly. */
int check_eog(Player* players, int playerCount, int* gameEnded,
        int pointThreshold, int cardsCount, int** fds1, int* pids) {
    int maxPoints = players[0].points;
    for(int i = 0; i < playerCount; i++) {
        if(players[i].points > maxPoints) {
            maxPoints = players[i].points;
        }
    }
    if(maxPoints < pointThreshold && cardsCount != 0) {
        return 1;
    }
    char winnersMessage[100];
    int len = 0;
    len += sprintf(winnersMessage + len, "Winner(s)");
    int firstWinnerProcessed = 0;
    for(int i = 0; i < playerCount; i++) {
        if(players[i].points == maxPoints) {
            if(firstWinnerProcessed) {
                len += sprintf(winnersMessage + len, ",%c", players[i].letter);
            } else {
                len += sprintf(winnersMessage + len, " %c", players[i].letter);
                firstWinnerProcessed = 1;
            }
        }
    }
    printf("%s\n", winnersMessage);
    terminate_players(pids, playerCount, fds1, 1);
    *gameEnded = 1;
    return 0;
}

/* Coordinates the operation of each game round. */
int process_game_round(int*** fds, int** fds1, int** fds2, int playerCount,
        int* cardsCount, Card* market, Card* deck, int* availableTokens,
        Player* players, int* nextCard, int deckCardsCount, int** fds3,
        int* pids) {
    for(int i = 0; i < playerCount; i++) {
        int alreadyReceivedInvalidResponse = 0;
        for(int j = 0; j < 2; j++) {
            if(gotSignal) {
                terminate_players(pids, playerCount, fds1, 0);
                free_fds(fds, fds1, fds2, fds3, playerCount);
                free(players);
                free(deck);
                fprintf(stderr, "SIGINT caught\n");
                exit(10);
            }
            prompt_player(fds1, i);
            char* message = get_player_response(fds, fds2, i, playerCount,
                    fds1, fds3, deck, players, pids);
            int messageProcessed = 0;
            int cardPurchased = 0;
            process_player_message(message, market, i, playerCount,
                    &messageProcessed, availableTokens, players, cardsCount,
                    fds1, &cardPurchased);
            if(messageProcessed) {
                free(message);
                if(cardPurchased && (*nextCard < deckCardsCount)) {
                    add_next_card_to_market(deck, market, deckCardsCount,
                            nextCard, fds1, playerCount, cardsCount);
                }
                break;
            } else if(!alreadyReceivedInvalidResponse) {
                alreadyReceivedInvalidResponse = 1;
                free(message);
                continue;
            } else {
                fprintf(stderr, "Protocol error by client\n");
                terminate_players(pids, playerCount, fds1, 1);
                free_fds(fds, fds1, fds2, fds3, playerCount);
                free(players);
                free(deck);
                free(message);
                exit(7);
            }
        }
        if(*cardsCount == 0) {
            break;
        }
    }
    return 0;
}

/* Add the next card in the deck to the market, printing out a message and
notifying all players. */
void add_next_card_to_market(Card* deck, Card* market,
        int deckCardsCount, int* nextCard, int** fds1, int playerCount,
        int* cardsCount) {
    market[7] = deck[*nextCard];
    printf("New card = Bonus %c, worth %d, costs %d,%d,%d,%d\n",
            deck[*nextCard].colour, deck[*nextCard].points,
            deck[*nextCard].costs[0], deck[*nextCard].costs[1],
            deck[*nextCard].costs[2], deck[*nextCard].costs[3]);
    char buffer[70];
    sprintf(buffer, "newcard%c:%d:%d,%d,%d,%d\n", deck[*nextCard].colour,
            deck[*nextCard].points, deck[*nextCard].costs[0],
            deck[*nextCard].costs[1], deck[*nextCard].costs[2],
            deck[*nextCard].costs[3]);
    send_message_to_players(buffer, fds1, -1, playerCount);
    (*nextCard)++;
    (*cardsCount)++;
}

/* Send "dowhat" to a specified player only. */
void prompt_player(int** fds1, int playerNumber) {
    write(fds1[playerNumber][1], "dowhat\n", 7);
}

/* Initialise the players in the "players" array to initial values. */
void initialise_players(Player* players, int playerCount) {
    for(int i = 0; i < playerCount; i++) {
        players[i].letter = (char)(65 + i);
        players[i].points = 0;
        for(int j = 0; j < 4; j++) {
            players[i].discounts[j] = 0;
        }
        for(int j = 0; j < 5; j++) {
            players[i].tokens[j] = 0;
        }
    }
}

/* Takes a message from a player to a hub and process it if it is a "wild"
 * message. */
int process_player_message_wild(char* message, int playerNumber,
        int playerCount, Player* players, int* messageProcessed,
        int** fds1) {
    if(strcmp(message, "wild\n") == 0) {
        players[playerNumber].tokens[4] += 1;
        *messageProcessed = 1;
        char wildMessage[10];
        // Notify all players
        sprintf(wildMessage, "wild%c\n", 65 + playerNumber);
        send_message_to_players(wildMessage, fds1, -1, playerCount);
        printf("Player %c took a wild\n", 65 + playerNumber);
    }
    return 0;
}

/* Used in process_player_message_purchase(). Apply a discount on the players
 * who purchased a card, depending on the card's colour. */
void check_purchased_card_colour(Card purchasedCard, Player* players,
        int playerNumber) {
    switch(purchasedCard.colour) {
        case 'P':
            players[playerNumber].discounts[0] += 1;
            break;
        case 'B':
            players[playerNumber].discounts[1] += 1;
            break;
        case 'Y':
            players[playerNumber].discounts[2] += 1;
            break;
        case 'R':
            players[playerNumber].discounts[3] += 1;
            break;
    }
}

/* Used in process_player_message_purchase(). Sends a message to all players
 * after a player has successfully bought a card. Also prints a message to
 * stdout. */
void send_purchased_message(int** fds1, int* tokens, int playerNumber,
        int playerCount, int cardNumber) {
    char purchaseMessage[70];
    sprintf(purchaseMessage, "purchased%c:%d:%d,%d,%d,%d,%d\n",
            65 + playerNumber, cardNumber, tokens[0], tokens[1], tokens[2],
            tokens[3], tokens[4]);
    send_message_to_players(purchaseMessage, fds1, -1, playerCount);
    printf("Player %c purchased %d using %d,%d,%d,%d,%d\n",
            65 + playerNumber, cardNumber, tokens[0], tokens[1], tokens[2],
            tokens[3], tokens[4]);
}

/* Check if a player can afford a certain card on the market. Return 1 if
can afford; 0 if can't. */
int can_afford(Card* market, int cardNumber, Player player) {
    int* costs = market[cardNumber].costs;
    int* tokens = player.tokens;
    int* discounts = player.discounts;
    int wildCount = tokens[4];
    int canAfford = 1;
    for(int i = 0; i < 4; i++) {
        if(tokens[i] < costs[i]) {
            if(tokens[i] + wildCount + discounts[i] >= costs[i]) {
                wildCount -= (costs[i] - tokens[i] - discounts[i]);
            } else {
                canAfford = 0;
                break;
            }
        }
    }
    return canAfford;
}

/* Updates the game state variables after a card purchase by a player. */
void update_game_state_after_purchase(Player* players, int playerNumber,
        Card purchasedCard, int* availableTokens, Card* market,
        int* cardsCount, int cardNumber, int* tokens) {
    players[playerNumber].points += purchasedCard.points;
    check_purchased_card_colour(purchasedCard, players, playerNumber);
    for(int i = 0; i < 5; i++) {
        players[playerNumber].tokens[i] -= tokens[i];
        availableTokens[i] += tokens[i];
    }
    for(int i = cardNumber + 1; i < 8; i++) {
        market[i - 1] = market[i];
    }
    (*cardsCount)--;
}

/* Takes a message from a player to a hub and process it if it is a "purchase"
 * message. */
int process_player_message_purchase(char* message, Card* market,
        int playerNumber, int playerCount, int* messageProcessed,
        Player* players, int* cardsCount, int** fds1, int* cardPurchased,
        int* availableTokens) {
    if(strncmp(message, "purchase", 8) == 0) {
        char* colonSep[2];
        for(int i = 0; i < 2; i++) {
            char* temp1;
            if((temp1 = strsep(&message, ":")) != NULL) {
                colonSep[i] = temp1;
            } else {
                return 1;
            }
        }
        if(!isdigit(colonSep[0][8]) || strlen(colonSep[0]) != 9) {
            return 1;
        }
        int cardNumber = colonSep[0][8] - '0';
        int tokens[5];
        for(int i = 0; i < 5; i++) {
            char* temp2;
            if((temp2 = strsep(&colonSep[1], ",")) != NULL) {
                for(int i = 0; i < strlen(temp2); i++) {
                    if(!isdigit(temp2[i]) && temp2[i] != '\n') {
                        return 1;
                    }
                }
                tokens[i] = atoi(temp2);
            } else {
                return 1;
            }
        }
        if(cardNumber >= *cardsCount) {
            return 1;
        }
        if(!can_afford(market, cardNumber, players[playerNumber])) {
            return 1;
        }
        Card purchasedCard = market[cardNumber];
        update_game_state_after_purchase(players, playerNumber, purchasedCard,
                availableTokens, market, cardsCount, cardNumber, tokens);
        send_purchased_message(fds1, tokens, playerNumber, playerCount,
                cardNumber);
        *messageProcessed = 1;
        *cardPurchased = 1;
    }
    return 0;
}

/* Takes an array of ints of length 4 and returns the sum of the elements. */
int count_tokens_taken(int* tokensTaken) {
    int count = 0;
    for(int i = 0; i < 4; i++) {
        count += tokensTaken[i];
    }
    return count;
}

/* Used in process_player_message_take(). Sends a message to all players
 * after a player has successfully taken tokens. Also prints a message to
 * stdout. */
void send_take_message(int** fds1, int* tokensTaken, int playerNumber,
        int playerCount) {
    char takeMessage[70];
    sprintf(takeMessage, "took%c:%d,%d,%d,%d\n", 65 + playerNumber,
            tokensTaken[0], tokensTaken[1], tokensTaken[2],
            tokensTaken[3]);
    send_message_to_players(takeMessage, fds1, -1, playerCount);
    printf("Player %c drew %d,%d,%d,%d\n", 65 + playerNumber,
            tokensTaken[0], tokensTaken[1], tokensTaken[2],
            tokensTaken[3]);
}

/* Takes a message from a player to a hub and process it if it is a "take"
 * message. */
int process_player_message_take(char* message, int playerNumber,
        int playerCount, int* messageProcessed, int* availableTokens,
        Player* players, int** fds1) {
    if(strncmp(message, "take", 4) == 0) {
        char* commaSep[4];
        for(int i = 0; i < 4; i++) {
            char* temp1;
            if((temp1 = strsep(&message, ",")) != NULL) {
                commaSep[i] = temp1;
            } else {
                return 1;
            }
        }
        for(int i = 4; i < strlen(commaSep[0]); i++) {
            if(!isdigit(commaSep[0][i])) {
                return 1;
            }
        }
        for(int i = 1; i < 4; i++) {
            for(int j = 0; j < strlen(commaSep[i]); j++) {
                if(!isdigit(commaSep[i][j]) && commaSep[i][j] != '\n') {
                    return 1;
                }
            }
        }
        int tokensTaken[4];
        tokensTaken[0] = atoi(commaSep[0] + 4);
        for(int i = 1; i < 4; i++) {
            tokensTaken[i] = atoi(commaSep[i]);
        }
        if(count_tokens_taken(tokensTaken) != 3) {
            return 1;
        }
        for(int i = 0; i < 4; i++) {
            if(tokensTaken[i] >= 0 && tokensTaken[i] <= availableTokens[i]) {
                availableTokens[i] -= tokensTaken[i];
                players[playerNumber].tokens[i] += tokensTaken[i];
            } else {
                return 1;
            }
        }
        send_take_message(fds1, tokensTaken, playerNumber, playerCount);
        *messageProcessed = 1;
    }
    return 0;
}

/* Takes a message from a player to a hub and process it. */
int process_player_message(char* message, Card* market, int playerNumber,
        int playerCount, int* messageProcessed, int* availableTokens,
        Player* players, int* cardsCount, int** fds1, int* cardPurchased) {
    process_player_message_wild(message, playerNumber, playerCount, players,
            messageProcessed, fds1);
    process_player_message_take(message, playerNumber, playerCount,
            messageProcessed, availableTokens, players, fds1);
    process_player_message_purchase(message, market, playerNumber,
            playerCount, messageProcessed, players, cardsCount, fds1,
            cardPurchased, availableTokens);
    return 0;
}

/* Listens from pipes a player's output to the hub and return that message. */
char* get_player_response(int*** fds, int** fds2, int playerNumber,
        int playerCount, int** fds1, int** fds3, Card* deck, Player* players,
        int* pids) {
    char* messageReceived = malloc(sizeof(char) * 70);
    for(int i = 0; i < 70; i++) {
        messageReceived[i] = '\0';
    }
    if(read(fds2[playerNumber][0], messageReceived, 70) <= 0) {
        free(players);
        free(deck);
        free(messageReceived);
        printf("Game ended due to disconnect\n");
        fprintf(stderr, "Client disconnected\n");
        terminate_players(pids, playerCount, fds1, 1);
        free_fds(fds, fds1, fds2, fds3, playerCount);
        exit(6);
    }
    return messageReceived;
}

/* Send a message to all players except the one specified by the
 * 'exception' parameter. */
void send_message_to_players(char* message, int** fds1, int exception,
        int playerCount) {
    for(int i = 0; i < playerCount; i++) {
        if(i != exception) {
            write(fds1[i][1], message, strlen(message));
        }
    }
}

/* Initialise the game state variables, including sending tokens message to
 * players. */
void initiate_game(Card* market, Card* deck, int deckCardsCount, int* nextCard,
        int* gameInitiated, int** fds1, int playerCount, int tokens,
        int* availableTokens, int* cardsCount) {
    int cardsToAdd = (deckCardsCount <= 8) ? deckCardsCount : 8;
    /* Send tokens messages to players */
    char tokensMessage[70];
    sprintf(tokensMessage, "tokens%d\n", tokens);
    send_message_to_players(tokensMessage, fds1, -1, playerCount);
    for(int i = 0; i < 4; i++) {
        availableTokens[i] = tokens;
    }
    /* Print and send new card messages */
    for(int i = 0; i < cardsToAdd; i++) {
        market[i] = deck[i];
        printf("New card = Bonus %c, worth %d, costs %d,%d,%d,%d\n",
                deck[i].colour, deck[i].points, deck[i].costs[0],
                deck[i].costs[1], deck[i].costs[2], deck[i].costs[3]);
        char buffer[70];
        sprintf(buffer, "newcard%c:%d:%d,%d,%d,%d\n", deck[i].colour,
                deck[i].points, deck[i].costs[0], deck[i].costs[1],
                deck[i].costs[2], deck[i].costs[3]);
        send_message_to_players(buffer, fds1, -1, playerCount);
        (*nextCard)++;
        (*cardsCount)++;
    }
    *gameInitiated = 1;
}

/* Returns 0 if all arguments are correctly formatted, 1 otherwise. */
int check_parameters(int argc, char** argv) {
    if(argc < 6 || argc > 30) {
        fprintf(stderr, "Usage: austerity tokens points deck player player "
                "[player ...]\n");
        exit(1);
    }
    /* Check argv[1] */
    for(int i = 0; i < strlen(argv[1]); i++) {
        if(!isdigit(argv[1][i])) {
            return 1;
        }
    }
    if(atoi(argv[1]) < 0) {
        return 1;
    }
    /* Check argv[2] */
    for(int i = 0; i < strlen(argv[2]); i++) {
        if(!isdigit(argv[2][i])) {
            return 1;
        }
    }
    if(atoi(argv[2]) < 0) {
        return 1;
    }
    return 0;
}

/* Gets a FILE* to file path of the deck file as specified by the user and
 * returns it. */
FILE* load_deck(char** argv) {
    char* deckPath = argv[3];
    FILE* deckFile;
    if((deckFile = fopen(deckPath, "r")) == NULL) {
        fprintf(stderr, "Cannot access deck file\n");
        exit(3);
    }
    return deckFile;
}

/* Returns 1 if c is 'P', 'B', 'Y' or 'R' and 0 otherwise. */
int char_is_pbyr(char c) {
    if(c == 'P' || c == 'B' || c == 'Y' || c == 'R') {
        return 1;
    }
    return 0;
}

/* Counts the number of lines in the deck file pointed to by the parameter
 * FILE* */
int count_lines_in_deck(FILE* deckFile) {
    int lineCount = 0;
    while(1) {
        char temp[100];
        if(fgets(temp, 100, deckFile) != NULL) {
            lineCount++;
        } else {
            break;
        }
    }
    rewind(deckFile);
    return lineCount;
}

/* Given a FILE* to the deck file, convert the saved deck to an array of Card
 * structs and returns it. If the file isn't a valid deck file, return NULL. */
Card* process_deck_file(FILE* deckFile, int* deckCardsCount) {
    int lineCount = count_lines_in_deck(deckFile);
    if(lineCount == 0) {
        return NULL;
    }
    Card* deck = malloc(sizeof(Card) * lineCount);
    for(int i = 0; i < lineCount; i++) {
        Card currentCard;
        char currentLine[100];
        char* lineStart = currentLine;
        fgets(currentLine, 100, deckFile);
        char* colonSep[3];
        for(int i = 0; i < 3; i++) {
            char* temp1;
            if((temp1 = strsep(&lineStart, ":")) != NULL) {
                colonSep[i] = temp1;
            } else {
                free(deck);
                return NULL;
            }
        }
        if(strlen(colonSep[0]) != 1 || !char_is_pbyr(colonSep[0][0])) {
            free(deck);
            return NULL;
        }
        if(!nonnegative_integer(colonSep[1])) {
            free(deck);
            return NULL;
        }
        currentCard.colour = colonSep[0][0];
        currentCard.points = atoi(colonSep[1]);
        for(int i = 0; i < 4; i++) {
            char* temp2;
            if(((temp2 = strsep(&colonSep[2], ",")) != NULL) &&
                    nonnegative_integer(temp2)) {
                currentCard.costs[i] = atoi(temp2);
            } else {
                free(deck);
                return NULL;
            }
        }
        deck[i] = currentCard;
    }
    *deckCardsCount = lineCount;
    return deck;
}

/* Takes in a string and returns 1 if it is a nonnegative integer or 0
otherwise. */
int nonnegative_integer(char* arg) {
    int digitCount = 0;
    for(int i = 0; i < strlen(arg); i++) {
        if(!(isdigit(arg[i]) || arg[i] == '\n')) {
            return 0;
        }
        if(isdigit(arg[i])) {
            digitCount++;
        }
    }
    if(digitCount == 0) {
        return 0;
    }
    return 1;
}

/* Free the pipe file descriptors. */
void free_fds(int*** fds, int** fds1, int** fds2, int** fds3,
        int playerCount) {
    for(int i = 0; i < playerCount; i++) {
        free(fds1[i]);
        free(fds2[i]);
        free(fds3[i]);
    }
    free(fds1);
    free(fds2);
    free(fds3);
    free(fds);
}

/* Converts a string to upper case. */
char* toUpper(const char* str) {
    char* str1 = malloc(sizeof(char) * 10);
    for(int i = 0; i < strlen(str); i++) {
        str1[i] = toupper(str[i]);
    }
    return str1;
}

/* Sends EOG to all players and terminate them if necessary. Also if necssary,
print exit status information about the players. */
void terminate_players(int* pids, int playerCount, int** fds1, int
        printMessage) {
    send_message_to_players("eog\n", fds1, -1, playerCount);
    int* status = malloc(sizeof(int) * playerCount);
    // Sleep for 2 seconds to wait for termination of players on their own
    sleep(2);
    for(int i = 0; i < playerCount; i++) {
        if(waitpid(pids[i], &status[i], WNOHANG) == 0) {
            kill(pids[i], SIGKILL);
            status[i] = -1;
            waitpid(pids[i], &status[i], 0);
        }
    }
    if(printMessage) {
        for(int i = 0; i < playerCount; i++) {
            if(WIFSIGNALED(status[i])) {
                char* signalName =
                        toUpper(sys_signame[WTERMSIG(status[i])]);
                fprintf(stderr,
                        "Player %c shutdown after receiving signal SIG%s\n",
                        65 + i, signalName);
                free(signalName);
            } else if(WIFEXITED(status[i]) && WEXITSTATUS(status[i]) != 0) {
                fprintf(stderr, "Player %c ended with status %d\n", 65 + i,
                        WEXITSTATUS(status[i]));
            }
        }
    }
    free(status);
    free(pids);

}
