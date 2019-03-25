#include "player.c"
int check_dowhat(char* message, Card* market, Player* players,
        int* cardsCount, int playerNumber, int* availableTokens,
        int* processedMessage);

/* The main function. */
int main(int argc, char** argv) {
    int playerCount = 0;    // Number of players in the game
    int playerNumber = 0;   // This player's player number
    check_parameters(argc, argv, &playerCount, &playerNumber, "shenzi");
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

/* Calculate the number of tokens needed to purchase a card by a player, taking
 * into account discounts. */
int calculate_tokens(Card card, Player player) {
    int tokens1 = card.costs[0] - player.discounts[0];
    int tokens2 = card.costs[1] - player.discounts[1];
    int tokens3 = card.costs[2] - player.discounts[2];
    int tokens4 = card.costs[3] - player.discounts[3];
    if(tokens1 < 0) {
        tokens1 = 0;
    }
    if(tokens2 < 0) {
        tokens2 = 0;
    }
    if(tokens3 < 0) {
        tokens3 = 0;
    }
    if(tokens4 < 0) {
        tokens4 = 0;
    }
    return tokens1 + tokens2 + tokens3 + tokens4;
}

/* Attempt to purchase a card according to this player's rule. */
int purchase_card(char* message, Card* market, Player* players,
        int* cardsCount, int playerNumber, int* processedMessage) {
    Player player = players[playerNumber];
    int canAfford[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for(int i = 0; i < *cardsCount; i++) {
        if(can_afford(market, i, player)) {
            canAfford[i] = 1;
        }
    }
    int affordableMaxPoint = -1;
    for(int i = 0; i < *cardsCount; i++) {
        if(canAfford[i] && market[i].points >= affordableMaxPoint) {
            affordableMaxPoint = market[i].points;
        }
    }
    int canAffordAndMaxPoints[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for(int i = 0; i < *cardsCount; i++) {
        if(canAfford[i] && market[i].points == affordableMaxPoint) {
            canAffordAndMaxPoints[i] = 1;
        }
    }
    int minTokens = 2147483647;
    for(int i = 0; i < *cardsCount; i++) {
        int numTokens = calculate_tokens(market[i], player);
        if(canAffordAndMaxPoints[i] && numTokens < minTokens) {
            minTokens = numTokens;
        }
    }
    int canAffordAndMaxPointsAndMinTokens[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for(int i = 0; i < *cardsCount; i++) {
        int numTokens = calculate_tokens(market[i], player);
        if(canAffordAndMaxPoints[i] && (numTokens == minTokens)) {
            canAffordAndMaxPointsAndMinTokens[i] = 1;
        }
    }
    for(int i = *cardsCount - 1; i >= 0; i--) {
        if(canAffordAndMaxPointsAndMinTokens[i]) {
            int* requiredTokens = get_required_tokens(market[i], player);
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

/* Attempt to take tokens according to this player's rule. */
int take_tokens(char* message, Card* market, Player* players,
        int* cardsCount, int playerNumber, int* availableTokens,
        int* processedMessage) {
    int tokensTaken[4] = {0, 0, 0, 0};
    int countTokensTaken = 0;
    int availableTokensCopy[4];
    for(int i = 0; i < 4; i++) {
        availableTokensCopy[i] = availableTokens[i];
    }
    if(get_total_tokens_count(availableTokensCopy) >= 3 &&
            less_than_two_empty_piles(availableTokensCopy)) {
        while(countTokensTaken != 3) {
            for(int i = 0; i < 4; i++) {
                if(availableTokensCopy[i] > 0) {
                    tokensTaken[i] += 1;
                    availableTokensCopy[i] -= 1;
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

/* Check if a "dowhat" message has been received from the hub and act
 * accordingly. */
int check_dowhat(char* message, Card* market, Player* players,
        int* cardsCount, int playerNumber, int* availableTokens,
        int* processedMessage) {
    if(strncmp(message, "dowhat\n", 7) == 0) {
        fprintf(stderr, "Received dowhat\n");
        // Attempt to purchase a card
        if(purchase_card(message, market, players, cardsCount, playerNumber,
                processedMessage) == 0) {
            return 0;
        }
        // Attempt to take tokens
        if(take_tokens(message, market, players, cardsCount, playerNumber,
                availableTokens, processedMessage) == 0) {
            return 0;
        }
        // Take wild
        printf("wild\n");
        fflush(stdout);
        *processedMessage = 1;
    }
    return 0;
}
