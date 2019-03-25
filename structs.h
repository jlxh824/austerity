typedef struct {
    int costs[4];
    int points;
    char colour;
} Card;

typedef struct {
    char letter;
    int points;
    int discounts[4];
    int tokens[5];
} Player;
