# austerity
A simulation of a card based multiplayer command line game, programmed in C99. Comes with three types of players to test with. 

## Compile Instruction
`make` with the provided makefile. 

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
- `player1`, `player2`, etc. are paths to the player executables. A minimum of 2 and a maximum of 26 players must be specified. 

For example, 
```
./austerity 7 15 cards ./shenzi ./shenzi ./shenzi
```
would start a game having 7 tokens in each non-wild pile, requiring 15 points to trigger the end of game, using the cards deck file, and with 3 players (each running `./shenzi`). 

## The Game
TBD
