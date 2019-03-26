# austerity
A simulation of a card based multiplayer command line game, programmed in C99. Comes with three types of players to test with. 

## Compile Instruction
`make` with the provided makefile. Probably wouldn't work if compiled in a non-POSIX environment. 

## Usage Instruction
Note: See the section below this one for an explanation of how the game works. 

The game consists of the hub (`austerity`) and three types of players (`banzai`, `ed` and `shenzi`). To start the game, execute 
```
./austerity tokens points deckfile player1 player2 [player3 ...]
```
where
- `tokens` is the number of tokens in each of the (non-wild) piles and must be a nonnegative integer. 
- `points` is the number of points required to trigger the end of the game and must be a nonnegative integer.
- `deckfile` is the path to the deckfile to use.
- `player1`, `player2`, etc. are paths to the player executables. A minimum of 2 and a maximum of 26 players must be specified. A sample deck file called `sample_cards` is provided. 

For example, 
```
./austerity 7 15 sample_cards ./shenzi ./shenzi ./shenzi
```
would start a game having 7 tokens in each non-wild pile, requiring 15 points to trigger the end of game, using the cards deck file, and with 3 players (each running `./shenzi`). 

## The Game
The game consists of two or more players, a deck of cards and five piles of tokens. The token types are: Purple (P), Brown (B), Yellow (Y), Red (R) and Wild. There are a limited number of tokens of each colour, while there are an unlimited number of tokens in the Wild pile.

To begin the game, 8 cards are drawn from the top of the deck (if there are less than 8 cards in the deck, then that number will be chosen) and placed face up. If ever a face up card is bought by a player, a new card will be drawn from the deck (additional cards will not be drawn if the deck is empty). All players begin with 0 tokens and 0 cards. 

### Cards
Each card has the following features:
- Cost to purchase (number of each colour is a non-negative integer), e.g.: `1P+2B+0Y+0R`
- Number of points the card is worth (a non-negative integer), e.g.: `2`
- Colour, used for discounts on all future card purchases, e.g.: `R`

Players will be granted a single token discount of the card colour for each card they own. For example, a player with three cards, one R and two B, will now have a discount of 1R and 2B. The discount will apply to all future card purchases.

### Player turns
On their turn, each player takes **one** of the following actions:
1. Select a card to purchase, by paying the token cost (applying any card discounts). Non-wild tokens are used in preference to wilds. Tokens used in a purchase are returned to their corresponding pile (including wild tokens).
2. Take 3 tokens, each of a different colour. If two or more of the coloured piles are empty, this action will not be possible.
3. Take a single wild token. Wild tokens can be used to make up for missing tokens of any colour when purchasing cards.

### End of game
The game continues until a player reaches the specified number of points required for a win. Once a player reaches this number of points, the round continues until all players have had their turn (this means all players will have had the same number of turns at the end of the game). It is possible for multiple players to reach the target number of points by the end of the game. The winner(s) of the game are the players with the highest number of points. 

If the end of the deck is reached before players have reached the points required for a win, the game will continue while there are still cards available for purchase. No new cards will be turned over, as there are none to draw from. Remaining face up cards will continue to be available for purchase. Once all face up cards have been purchased, the game will end. The winner(s) in this scenario are the players with the highest number of points.

### Deck file
The hub will use the deck file to load the available deck of cards for a game. Each card will be on a new line within the file. There must be at least one card in the deck file for it to be valid.
The structure for a card is: `D:V:P,B,Y,R` where
- `D` is the colour of discount this card gives.
- `V` is the number of points this card is worth.
- `P`, `B`, `Y` and `R` are the price of this card in the colour of tokens (can be zero). 

An example deck file might look like:
```
B:1:1,0,0,0
Y:0:0,3,0,2
```
