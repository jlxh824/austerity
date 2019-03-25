#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include "structs.h"

void check_parameters(int argc, char** argv, int* playerCount, int*
        playerNumber, char* player);
void setup_players(Player* players, int playerCount);
void check_wild(char* message, Card* market, Player* players, int playerCount,
        int cardsCount, int* processedMessage);
int check_took(char* message, Card* market, Player* players, int playerCount,
        int cardsCount, int* processedMessage, int* availableTokens);
void check_eog(char* message, Player* players, int playerCount,
        int* processedMessage, int* gameEnded);
int check_newcard(char* message, Card* market, int* cardsCount, Player*
        players, int playerCount, int* processedMessage);
int check_purchased(char* message, Card* market, Player* players,
        int playerCount, int* cardsCount, int* processedMessage,
        int* availableTokens);
int check_tokens(char* message, int* tokensCount, int* availableTokens,
        int* processedTokens, Card* market, Player* players,
        int cardsCount, int playerCount);
void display_cards(Card* market, int cardsCount);
void display_players(Player* players, int playerCount);

/* Takes a string message and checks if it is a nonngative integer. Return 1
if it is, 0 if not. */
int message_is_nonnegative_integer(char* message) {
    for(int i = 0; i < strlen(message); i++) {
        if(!isdigit(message[i]) && !(message[i] == '\n')) {
            return 0;
        }
    }
    return 1;
}

/* Initialise the array of players. */
void setup_players(Player* players, int playerCount) {
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

/* Check the parameters. */
void check_parameters(int argc, char** argv, int* playerCount,
        int* playerNumber, char* player) {
    if(argc != 3) {
        fprintf(stderr, "Usage: %s pcount myid\n", player);
        exit(1);
    } else if(strlen(argv[1]) > 2) {
        fprintf(stderr, "Invalid player count\n");
        exit(2);
    } else if(strlen(argv[2]) > 2) {
        fprintf(stderr, "Invalid player ID\n");
        exit(3);
    }
    for(int i = 0; i < strlen(argv[1]); i++) {
        if(isdigit(argv[1][i])) {
            int add = 0;
            if(strlen(argv[1]) == 1) {
                add = (argv[1][i] - '0');
            } else {
                switch(i) {
                    case 0:
                        add = (argv[1][i] - '0') * 10;
                        break;
                    case 1:
                        add = argv[1][i] - '0';
                        break;
                }
            }
            *playerCount += add;
        } else {
            fprintf(stderr, "Invalid player count\n");
            exit(2);
        }
    }
    if(*playerCount > 26 || *playerCount < 2) {
        fprintf(stderr, "Invalid player count\n");
        exit(2);
    }
    for(int i = 0; i < strlen(argv[2]); i++) {
        if(!isdigit(argv[2][i])) {
            fprintf(stderr, "Invalid player ID\n");
            exit(3);
        }
    }
    *playerNumber = atoi(argv[2]);
    if(*playerNumber >= *playerCount) {
        fprintf(stderr, "Invalid player ID\n");
        exit(3);
    }
}

/* Check if a "wild?" message has been received from the hub and act
accordingly. */
void check_wild(char* message, Card* market, Player* players, int playerCount,
        int cardsCount, int* processedMessage) {
    if(!(*processedMessage) && strncmp(message, "wild", 4) == 0) {
        int playerNumber = (int)message[4] - 65;
        players[playerNumber].tokens[4] += 1;
        display_cards(market, cardsCount);
        display_players(players, playerCount);
        *processedMessage = 1;
    }
}

/* Check if a "took*" message has been received from the hub and act
accordingly. Return 1 if error occured in processing message, 0 otherwise. */
int check_took(char* message, Card* market, Player* players, int playerCount,
        int cardsCount, int* processedMessage, int* availableTokens) {
    if(!(*processedMessage) && strncmp(message, "took", 4) == 0) {
        int playerNumber = (int)message[4] - 65;
        char* colonSep[2];
        for(int i = 0; i < 2; i++) {
            char* temp1;
            if((temp1 = strsep(&message, ":")) != NULL) {
                colonSep[i] = temp1;
            } else {
                return 1;
            }
        }

        for(int i = 0; i < 4; i++) {
            char* temp2;
            if((temp2 = strsep(&colonSep[1], ",")) != NULL) {
                if(!message_is_nonnegative_integer(temp2)) {
                    return 1;
                }
                players[playerNumber].tokens[i] += atoi(temp2);
                availableTokens[i] -= atoi(temp2);
            } else {
                return 1;
            }
        }

        display_cards(market, cardsCount);
        display_players(players, playerCount);
        *processedMessage = 1;
    }
    return 0;
}

/* Check if an "eog" message has been received from the hub and act
accordingly. */
void check_eog(char* message, Player* players, int playerCount, int*
        processedMessage, int* gameEnded) {
    if(!(*processedMessage) && strncmp(message, "eog", 3) == 0) {
        int maxPoints = players[0].points;
        for(int i = 0; i < playerCount; i++) {
            if(players[i].points > maxPoints) {
                maxPoints = players[i].points;
            }
        }
        char winnersMessage[100];
        int len = 0;
        len += sprintf(winnersMessage + len, "Game over. Winners are");
        int firstWinnerProcessed = 0;
        for(int i = 0; i < playerCount; i++) {
            if(players[i].points == maxPoints) {
                if(firstWinnerProcessed) {
                    len += sprintf(winnersMessage + len, ",%c",
                            players[i].letter);
                } else {
                    len += sprintf(winnersMessage + len, " %c",
                            players[i].letter);
                    firstWinnerProcessed = 1;
                }
            }
        }
        fprintf(stderr, "%s\n", winnersMessage);
        *processedMessage = 1;
        *gameEnded = 1;
    }
}

/* Check if a "newcard*" message has been received from the hub and act
accordingly. Return 1 if error occured in processing message, 0 otherwise. */
int check_newcard(char* message, Card* market, int* cardsCount,
        Player* players, int playerCount, int* processedMessage) {
    if(!(*processedMessage) && strncmp(message, "newcard", 7) == 0
            && *cardsCount < 8) {
        Card temp;
        char* colonSep[3];
        for(int i = 0; i < 3; i++) {
            char* temp2;
            if((temp2 = strsep(&message, ":")) != NULL) {
                colonSep[i] = temp2;
            } else {
                return 1;
            }
        }
        temp.colour = colonSep[0][7];
        if(!message_is_nonnegative_integer(colonSep[1])) {
            return 1;
        }
        temp.points = atoi(colonSep[1]);
        for(int i = 0; i < 4; i++) {
            char* temp3;
            if((temp3 = strsep(&colonSep[2], ",")) != NULL) {
                if(!message_is_nonnegative_integer(temp3)) {
                    return 1;
                }
                temp.costs[i] = atoi(temp3);
            } else {
                return 1;
            }
        }
        market[*cardsCount] = temp;
        *cardsCount += 1;
        display_cards(market, *cardsCount);
        display_players(players, playerCount);
        *processedMessage = 1;
    }
    return 0;
}

/* Used in check_purchased(). Apply a discount on the players who purchased a
 * card, depending on the card's colour. */
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

/* Check if a "purchased*" message has been received from the hub and act
accordingly. Return 1 if error occured in processing message, 0 otherwise. */
int check_purchased(char* message, Card* market, Player* players,
        int playerCount, int* cardsCount, int* processedMessage,
        int* availableTokens) {
    if(!(*processedMessage) && strncmp(message, "purchased", 9) == 0
            && *cardsCount >= 1) {
        char* colonSep[3];
        for(int i = 0; i < 3; i++) {
            char* temp1;
            if((temp1 = strsep(&message, ":")) != NULL) {
                colonSep[i] = temp1;
            } else {
                return 1;
            }
        }
        if(!message_is_nonnegative_integer(colonSep[1])) {
            return 1;
        }
        int playerNumber = (int)colonSep[0][9] - 65;
        int cardNumber = atoi(colonSep[1]);
        int tokens[5];
        for(int i = 0; i < 5; i++) {
            char* temp2;
            if((temp2 = strsep(&colonSep[2], ",")) != NULL) {
                if(!message_is_nonnegative_integer(temp2)) {
                    return 1;
                }
                tokens[i] = atoi(temp2);
            } else {
                return 1;
            }
        }
        Card purchasedCard = market[cardNumber];
        players[playerNumber].points += purchasedCard.points;
        check_purchased_card_colour(purchasedCard, players, playerNumber);
        for(int i = 0; i < 5; i++) {
            players[playerNumber].tokens[i] -= tokens[i];
        }
        for(int i = 0; i < 4; i++) {
            availableTokens[i] += tokens[i];
        }
        for(int i = cardNumber + 1; i < 8; i++) {
            market[i - 1] = market[i];
        }
        *cardsCount -= 1;
        display_cards(market, *cardsCount);
        display_players(players, playerCount);
        *processedMessage = 1;
    }
    return 0;
}

/* Check if a "tokens*" message has been received from the hub and act
accordingly. */
int check_tokens(char* message, int* tokensCount, int* availableTokens,
        int* processedTokens, Card* market, Player* players, int cardsCount,
        int playerCount) {
    if(strncmp(message, "tokens", 6) == 0) {
        if(!message_is_nonnegative_integer(message + 6)) {
            return 1;
        }
        if(!((*tokensCount = atoi(message + 6)) >= 0)) {
            return 1;
        }
        for(int i = 0; i < 4; i++) {
            availableTokens[i] = *tokensCount;
        }
        *processedTokens = 1;
        display_cards(market, cardsCount);
        display_players(players, playerCount);
    }
    return 0;
}

/* Check if the player can afford a certain card on the market. Return 1 if
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

/* Return the required tokens to buy a certain card as an array. It is assumed
that the player can afford the card (which would have been checked in another
function). */
int* get_required_tokens(Card card, Player player) {
    int* costs = card.costs;
    int* totalTokens = player.tokens;
    int wildCount = totalTokens[4];

    int costsWithDiscounts[4];
    for(int i = 0; i < 4; i++) {
        int difference = costs[i] - player.discounts[i];
        if(difference < 0) {
            costsWithDiscounts[i] = 0;
        } else {
            costsWithDiscounts[i] = costs[i] - player.discounts[i];
        }
    }

    int* requiredTokens = malloc(5 * sizeof(int));
    for(int i = 0; i < 5; i++) {
        requiredTokens[i] = 0;
    }
    
    for(int i = 0; i < 4; i++) {
        if(costsWithDiscounts[i] == 0) {
            continue;
        }
        if(totalTokens[i] >= costsWithDiscounts[i]) {
            totalTokens[i] -= costsWithDiscounts[i];
            requiredTokens[i] += costsWithDiscounts[i];
        } else if(totalTokens[i] + wildCount >= costsWithDiscounts[i]) {
            requiredTokens[i] += totalTokens[i];
            int wildRequired = costsWithDiscounts[i] - totalTokens[i];
            requiredTokens[4] += wildRequired;
            wildCount -= wildRequired;
        }
    }
    return requiredTokens;
}

/* Return the total number of tokens available to take. */
int get_total_tokens_count(int* availableTokens) {
    int count = 0;
    for(int i = 0; i < 4; i++) {
        count += availableTokens[i];
    }
    return count;
}

/* Return 1 if there are less than two empty piles of tokens to take from, 0
otherwise. */
int less_than_two_empty_piles(int* availableTokens) {
    int emptyPilesCounter = 0;
    for(int i = 0; i < 4; i++) {
        if(availableTokens[i] == 0) {
            emptyPilesCounter++;
        }
    }
    if(emptyPilesCounter >= 2) {
        return 0;
    }
    return 1;
}

/* Display the information of cards available to purchase to stdout. */
void display_cards(Card* market, int cardsCount) {
    for(int i = 0; i < cardsCount; i++) {
        Card temp = market[i];
        fprintf(stderr, "Card %d:%c/%d/%d,%d,%d,%d\n", i, temp.colour,
                temp.points, temp.costs[0], temp.costs[1], temp.costs[2],
                temp.costs[3]);
    }
}

/* Display the information of players to stdout. */
void display_players(Player* players, int playerCount) {
    for(int i = 0; i < playerCount; i++) {
        Player temp = players[i];
        fprintf(stderr,
                "Player %c:%d:Discounts=%d,%d,%d,%d:Tokens=%d,%d,%d,%d,%d\n",
                temp.letter, temp.points, temp.discounts[0], temp.discounts[1],
                temp.discounts[2], temp.discounts[3], temp.tokens[0],
                temp.tokens[1], temp.tokens[2], temp.tokens[3],
                temp.tokens[4]);
    }
}
