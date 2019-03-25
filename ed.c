#include "player.c"
int check_dowhat(char* message, Card* market, Player* players,
        int* cardsCount, int playerNumber, int* availableTokens,
        int* processedMessage, int playerCount);

/* The main function. */
int main(int argc, char** argv) {
    int playerCount = 0;    // number of players in the game
    int playerNumber = 0;   // this player's player number
    check_parameters(argc, argv, &playerCount, &playerNumber, "ed");
    Player* players = malloc(sizeof(Player) * playerCount);
    setup_players(players, playerCount);
    int processedTokens = 0;
    int tokensCount = -1;   // number of tokens to start the game with
    int cardsCount = 0; // number of cards available for purchase on the market
    int gameEnded = 0;
    Card market[8];
    int availableTokens[4] = {0, 0, 0, 0}; // available tokens of each type
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
                availableTokens, &processedMessage, playerCount);
        if(!processedMessage) {
            free(players);
            fprintf(stderr, "Communication Error\n");
            exit(6);
        }
    }
    free(players);
    return 0;
}

/* Checks if any other player can afford a specified card in the market.
 * Returns 1 if yes, 0 if no. */
int others_can_afford(Card* market, int cardNumber, Player* players,
        int playerNumber, int playerCount, int cardsCount) {
    for(int i = 0; i < playerCount; i++) {
        if(i != playerNumber) {
            if(can_afford(market, cardNumber, players[i])) {
                return 1;
            }
        }
    }
    return 0;
}

/* Populates an array of ints that for each card, tells whether any player
 * could afford that card. */
void populate_others_can_afford(Card* market, Player* players, int* cardsCount,
        int playerNumber, int playerCount, int* othersCanAfford) {
    for(int i = 0; i < *cardsCount; i++) {
        if(others_can_afford(market, i, players, playerNumber, playerCount,
                *cardsCount)) {
            othersCanAfford[i] = 1;
        }
    }
}

/* Finds and returns the point of card that has the maximum point that any
 * other player can afford. */
int get_max_points_others_can_afford(int* cardsCount, int* othersCanAfford,
        Card* market) {
    int maxPointsOthersCanAfford = -1;
    for(int i = 0; i < *cardsCount; i++) {
        if(othersCanAfford[i] && market[i].points > maxPointsOthersCanAfford) {
            maxPointsOthersCanAfford = market[i].points;
        }
    }
    return maxPointsOthersCanAfford;
}

/* Finds and returns the player number of the next player that can afford the
 * card that is worth the most points and is affordable for any player. */
int get_next_player(Player* players, Card* market, int* cardsCount,
        int playerNumber, int playerCount, int* othersCanAffordAndMaxPoints) {
    int nextPlayer = -1;
    for(int i = playerNumber + playerCount - 1; i >= playerNumber + 1; i--) {
        for(int j = 0; j < *cardsCount; j++) {
            if(othersCanAffordAndMaxPoints[j] && can_afford(market, j,
                    players[i % playerCount])) {
                nextPlayer = i % playerCount;
                break;
            }
        }
    }
    return nextPlayer;
}

/* Attempt to purchase a card according to this player's rule. */
int purchase_card(char* message, Card* market, Player* players,
        int* cardsCount, int playerNumber, int* processedMessage, int
        playerCount, int* cardIdentified, int* oldestEligibleCard) {
    Player player = players[playerNumber];
    int othersCanAfford[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    populate_others_can_afford(market, players, cardsCount, playerNumber,
            playerCount, othersCanAfford);
    int maxPointsOthersCanAfford = get_max_points_others_can_afford(cardsCount,
            othersCanAfford, market);
    int othersCanAffordAndMaxPoints[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for(int i = 0; i < *cardsCount; i++) {
        if(othersCanAfford[i] &&
                market[i].points == maxPointsOthersCanAfford) {
            othersCanAffordAndMaxPoints[i] = 1;
        }
    }
    int nextPlayer = get_next_player(players, market, cardsCount, playerNumber,
            playerCount, othersCanAffordAndMaxPoints);
    *cardIdentified = 0;
    int nextPlayerCanAffordAndMaxPoints[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for(int i = 0; i < *cardsCount; i++) {
        if(othersCanAffordAndMaxPoints[i] && can_afford(market, i,
                players[nextPlayer])) {
            nextPlayerCanAffordAndMaxPoints[i] = 1;
            *cardIdentified = 1;
        }
    }
    *oldestEligibleCard = -1;
    if(*cardIdentified) {
        for(int i = 0; i < *cardsCount; i++) {
            if(nextPlayerCanAffordAndMaxPoints[i]) {
                *oldestEligibleCard = i;
                break;
            }
        }
        if(can_afford(market, *oldestEligibleCard, player)) {
            int* requiredTokens = get_required_tokens(
                    market[*oldestEligibleCard], player);
            fflush(stdout);
            printf("purchase%d:%d,%d,%d,%d,%d\n", *oldestEligibleCard,
                    requiredTokens[0], requiredTokens[1], requiredTokens[2],
                    requiredTokens[3], requiredTokens[4]);
            fflush(stdout);
            free(requiredTokens);
            *processedMessage = 1;
            return 0;
        }
    }
    return 1;
}

/* Check which tokens to take when a card has been identified. */
void card_identified_take_tokens(Card* market, int cardIdentified,
        int oldestEligibleCard, int* countTokensTaken, int* tokensTaken,
        int* yellowTaken, int* redTaken, int* brownTaken, int* purpleTaken,
        Player player, int* availableTokensCopy) {
    if(cardIdentified) {
        Card card = market[oldestEligibleCard];
        // yellow
        if(*countTokensTaken < 3 && (player.discounts[2] + player.tokens[2]
                < card.costs[2]) && availableTokensCopy[2] >= 1) {
            tokensTaken[2]++;
            (*countTokensTaken)++;
            *yellowTaken = 1;
        }
        // red
        if(*countTokensTaken < 3 && (player.discounts[3] + player.tokens[3]
                < card.costs[3]) && availableTokensCopy[3] >= 1) {
            tokensTaken[3]++;
            (*countTokensTaken)++;
            *redTaken = 1;
        }
        // brown
        if(*countTokensTaken < 3 && (player.discounts[1] + player.tokens[1] <
                card.costs[1]) && availableTokensCopy[1] >= 1) {
            tokensTaken[1]++;
            (*countTokensTaken)++;
            *brownTaken = 1;
        }
        // purple
        if(*countTokensTaken < 3 && (player.discounts[0] + player.tokens[0] <
                card.costs[0]) && availableTokensCopy[0] >= 1) {
            tokensTaken[0]++;
            (*countTokensTaken)++;
            *purpleTaken = 1;
        }
    }
}

/* Check which tokens to take regardless of whether a card has been identified
 * or not. */
void any_take_tokens(Card* market, int* countTokensTaken, int* tokensTaken,
        int* yellowTaken, int* redTaken, int* brownTaken, int* purpleTaken,
        int* availableTokensCopy) {
    // yellow
    if(*countTokensTaken < 3 && availableTokensCopy[2] >= 1 &&
            !(*yellowTaken)) {
        tokensTaken[2]++;
        (*countTokensTaken)++;
        *yellowTaken = 1;
    }
    // red
    if(*countTokensTaken < 3 && availableTokensCopy[3] >= 1 &&
            !(*redTaken)) {
        tokensTaken[3]++;
        (*countTokensTaken)++;
        *redTaken = 1;
    }
    // brown
    if(*countTokensTaken < 3 && availableTokensCopy[1] >= 1 &&
            !(*brownTaken)) {
        tokensTaken[1]++;
        (*countTokensTaken)++;
        *brownTaken = 1;
    }
    // purple
    if(*countTokensTaken < 3 && availableTokensCopy[0] >= 1 &&
            !(*purpleTaken)) {
        tokensTaken[0]++;
        (*countTokensTaken)++;
        *purpleTaken = 1;
    }
}

/* Attempt to take tokens according to this player's rule. */
int take_tokens(char* message, Card* market, Player* players,
        int* cardsCount, int playerNumber, int* availableTokens,
        int* processedMessage, int cardIdentified, int oldestEligibleCard) {
    Player player = players[playerNumber];
    int availableTokensCopy[4];
    for(int i = 0; i < 4; i++) {
        availableTokensCopy[i] = availableTokens[i];
    }
    if(get_total_tokens_count(availableTokensCopy) >= 3 &&
            less_than_two_empty_piles(availableTokensCopy)) {
        int tokensTaken[4] = {0, 0, 0, 0};
        int countTokensTaken = 0;
        int yellowTaken = 0;
        int redTaken = 0;
        int brownTaken = 0;
        int purpleTaken = 0;
        card_identified_take_tokens(market, cardIdentified,
                oldestEligibleCard, &countTokensTaken, tokensTaken,
                &yellowTaken, &redTaken, &brownTaken, &purpleTaken, player,
                availableTokensCopy);
        any_take_tokens(market, &countTokensTaken, tokensTaken, &yellowTaken,
                &redTaken, &brownTaken, &purpleTaken, availableTokensCopy);
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
        int* processedMessage, int playerCount) {
    if(strncmp(message, "dowhat\n", 7) == 0) {
        /* Attempt to buy a card */
        int cardIdentified = 0;
        // card number of the oldest card eligible to purchase
        int oldestEligibleCard = -1;
        if(purchase_card(message, market, players, cardsCount, playerNumber,
                processedMessage, playerCount, &cardIdentified,
                &oldestEligibleCard) == 0) {
            return 0;
        }
        /* Attempt to take tokens */
        if(take_tokens(message, market, players, cardsCount, playerNumber,
                availableTokens, processedMessage, cardIdentified,
                oldestEligibleCard) == 0) {
            return 0;
        }
        /* Take wild */
        printf("wild\n");
        fflush(stdout);
        *processedMessage = 1;
    }
    return 0;
}
