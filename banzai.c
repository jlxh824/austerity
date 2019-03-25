#include "player.c"
int check_dowhat(char* message, Card* market, Player* players,
        int* cardsCount, int playerNumber, int* availableTokens,
        int* processedMessage);

/* The main function. */
int main(int argc, char** argv) {
    int playerCount = 0;    // Number of players in the game
    int playerNumber = 0;   // This player's player number
    check_parameters(argc, argv, &playerCount, &playerNumber, "banzai");
    Player* players = malloc(sizeof(Player) * playerCount);
    setup_players(players, playerCount);
    int processedTokens = 0;
    int tokensCount = -1;   // Number of tokens to start the game with
    int cardsCount = 0; // Number of cards available for purchase on the market
    int gameEnded = 0;
    Card market[8];
    int availableTokens[4] = {0, 0, 0, 0}; // Tokens available to take by types
    char message[70];
    fgets(message, 70, stdin);
    check_tokens(message, &tokensCount, availableTokens, &processedTokens,
            market, players, cardsCount, playerCount);
    if(!processedTokens) {
        free(players);
        fprintf(stderr, "Communication Error\n");
        exit(6);
    }
    while(!gameEnded) {
        if(fgets(message, 70, stdin) == NULL) {
            free(players);
            fprintf(stderr, "Communication Error\n");
            exit(6);
        }
        int processedMessage = 0;
        check_eog(message, players, playerCount, &processedMessage,
                &gameEnded);
        check_wild(message, market, players, playerCount, cardsCount,
                &processedMessage);
        check_took(message, market, players, playerCount, cardsCount,
                &processedMessage, availableTokens);
        check_newcard(message, market, &cardsCount, players, playerCount,
                &processedMessage);
        check_purchased(message, market, players, playerCount, &cardsCount,
                &processedMessage, availableTokens);
        check_dowhat(message, market, players, &cardsCount, playerNumber,
                availableTokens, &processedMessage);
        if(!processedMessage) {
            free(players);
            fprintf(stderr, "Communication Error\n");
            exit(6);
        }
    }
    free(players);
    return 0;
}

/* Count the number of tokens a player has. */
int count_tokens(Player* players, int playerNumber) {
    int count = 0;
    for(int i = 0; i < 5; i++) {
        count += players[playerNumber].tokens[i];
    }
    return count;
}

/* Get the total cost of a card. */
int total_cost(Card card) {
    int count = 0;
    for(int i = 0; i < 4; i++) {
        count += card.costs[i];
    }
    return count;
}

/* Get the number of wild tokens a player needs to purchase a card. */
int wild_tokens_needed(Player player, Card card) {
    int wildNeeded = 0;
    for(int i = 0; i < 4; i++) {
        int cost = card.costs[i];
        int token = player.tokens[i];
        int discount = player.discounts[i];
        if(cost - token - discount > 0) {
            wildNeeded += cost - token - discount;
        } else {
            continue;
        }
    }
    return wildNeeded;
}

/* Attempt to take tokens according to this player's rule. */
int take_tokens(Player* players, int playerNumber, int* availableTokens,
        int* processedMessage) {
    int tokensCount = count_tokens(players, playerNumber);
    int tokensTaken[4] = {0, 0, 0, 0};
    int countTokensTaken = 0;
    int availableTokensCopy[4];
    for(int i = 0; i < 4; i++) {
        availableTokensCopy[i] = availableTokens[i];
    }
    if(tokensCount < 3 && get_total_tokens_count(availableTokensCopy) >= 3
            && less_than_two_empty_piles(availableTokensCopy)) {
        while(countTokensTaken != 3) {
            for(int i = 0; i < 4; i++) {
                int j = 0;
                switch(i) {
                    case 0:
                        j = 2;
                        break;
                    case 1:
                        j = 1;
                        break;
                    case 2:
                        j = 0;
                        break;
                    case 3:
                        j = 3;
                        break;
                }
                if(availableTokensCopy[j] > 0) {
                    tokensTaken[j] += 1;
                    availableTokensCopy[j] -= 1;
                    countTokensTaken += 1;
                }
                if(countTokensTaken == 3) {
                    break;
                }
            }
        }
        fflush(stdout);
        printf("take%d,%d,%d,%d\n", tokensTaken[0], tokensTaken[1],
                tokensTaken[2], tokensTaken[3]);
        fflush(stdout);
        *processedMessage = 1;
        return 0;
    }
    return 1;
}

/* Attempt to purchase a card according to this player's rule. */
int purchase_card(Card* market, Player* players, int* cardsCount,
        int playerNumber, int* processedMessage) {
    Player player = players[playerNumber];
    int canAffordAndNonZero[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for(int i = 0; i < *cardsCount; i++) {
        if(can_afford(market, i, player) && market[i].points > 0) {
            canAffordAndNonZero[i] = 1;
        }
    }
    int affordableMaxCost = -1;
    for(int i = 0; i < *cardsCount; i++) {
        if(canAffordAndNonZero[i] &&
                total_cost(market[i]) >= affordableMaxCost) {
            affordableMaxCost = total_cost(market[i]);
        }
    }
    int canAffordAndNonZeroAndMaxCosts[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for(int i = 0; i < *cardsCount; i++) {
        if(canAffordAndNonZero[i] &&
                total_cost(market[i]) == affordableMaxCost) {
            canAffordAndNonZeroAndMaxCosts[i] = 1;
        }
    }
    int maxWildTokens = -1;
    for(int i = 0; i < *cardsCount; i++) {
        int wildNeeded = wild_tokens_needed(player, market[i]);
        if(canAffordAndNonZeroAndMaxCosts[i] && wildNeeded > maxWildTokens) {
            maxWildTokens = wildNeeded;
        }
    }
    int canAffordNonZeroAndMaxCostsAndMaxWild[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for(int i = 0; i < *cardsCount; i++) {
        int wildNeeded = wild_tokens_needed(player, market[i]);
        if(canAffordAndNonZeroAndMaxCosts[i] && wildNeeded == maxWildTokens) {
            canAffordNonZeroAndMaxCostsAndMaxWild[i] = 1;
        }
    }
    for(int i = 0; i < *cardsCount; i++) {
        if(canAffordNonZeroAndMaxCostsAndMaxWild[i]) {
            int* requiredTokens = get_required_tokens(market[i], player);
            fflush(stdout);
            printf("purchase%d:%d,%d,%d,%d,%d\n", i, requiredTokens[0],
                    requiredTokens[1], requiredTokens[2], requiredTokens[3],
                    requiredTokens[4]);
            fflush(stdout);
            free(requiredTokens);
            *processedMessage = 1;
            return 0;
        }
    }
    return 1;
}

/* Check if a "dowhat" message has been received from the hub and act
 * accordingly. */
int check_dowhat(char* message, Card* market, Player* players,
        int* cardsCount, int playerNumber, int* availableTokens,
        int* processedMessage) {
    if(strncmp(message, "dowhat\n", 7) == 0) {
        /* Attempt to take tokens */
        if(take_tokens(players, playerNumber, availableTokens,
                processedMessage) == 0) {
            return 0;
        }
        /* Attempt to buy a card */
        if(purchase_card(market, players, cardsCount, playerNumber,
                processedMessage) == 0) {
            return 0;
        }
        /* Take a wild */
        printf("wild\n");
        fflush(stdout);
        *processedMessage = 1;
    }
    return 0;
}
