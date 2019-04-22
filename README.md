### EOS Chess Contract

This project is a way to for two EOS account holders to play a game of chess, using the EOS blockchain as the moderator.

#### Setup

[eosio install instructions](https://developers.eos.io/eosio-home/docs/setting-up-your-environment)

[CDT (contract development toolkit) installation](https://developers.eos.io/eosio-home/docs/installing-the-contract-development-toolkit)

[Create dev wallet](https://developers.eos.io/eosio-home/docs/wallets)

NOTE: you will want to store the auto-generated dev wallet password in a place that's easily accessible.  

Build the contract-
```
eosio-cpp -o chess.wasm chess.cpp --abigen
```

In `setup.sh`, replace the public keys in the account creation commands with the public key you generated with the `cleos wallet create_key` command in the wallets section.
Then run `./nodeos_start` and `./setup.sh`.  (if setup still gives you errors, be sure your wallet is still unlocked as described in the 'Create dev wallet' section)

The setup script creates some test accounts, including one called 'chess'.  It then sets the contract to the chess account, allowing you to call contract actions against that account, as in the examples in the 'Actions' section below.


#### Startup
```
eosio-cpp -o chess.wasm chess.cpp --abigen
./nodeos_start
cleos wallet unlock
./setup.sh
```

this sequence of commands should build the chess contract, get a local test node running, and set the chess contract on the local node to the 'chess' account

#### Games Table
The data structure (multi index) storing the games is the `struct game` class in `chess.cpp`, and is named `games` on the blockchain.  Once a new game has been created, you can view the records in this table with the following command -
```
cleos get table chess chess games
```

The main concept here is that there is a `piece_positions` array, where each index represents a specific piece on the board, and the value represents a board position as follows -

```
Board Positions
64 63 62 61 60 59 58 57   - Black Ranks
56 55 54 53 52 51 50 49
48 47 46 45 44 43 42 41
40 39 38 37 36 35 34 33
32 31 30 29 28 27 26 25
24 23 22 21 20 19 18 17
16 15 14 13 12 11 10  9
 8  7  6  5  4  3  2  1   - White Ranks
```

```
Piece Indexes
White
0     : King
1     : Queen
2-3   : Bishop
4-5   : Knight
6-7   : Rook
8-15  : Pawn

Black
16    : King
17    : Queen
18-19 : Bishop
20-21 : Knight
22-23 : Rook
24-31 : Pawn
```

#### Actions
newgame - Set up a new game.  Parameters are the white player and the black player account names, respsectively.
```
cleos push action chess newgame '["alice", "bob"]' -p chess@active
```

concede - Used by a player to concede a game.  In the following example, alice uses the concede action to concede the game with gameid 0.  Obviously, this only works if she's actually a player in that game.
```
cleos push action chess concede '["alice", "0"]' -p alice@active
```

draw - Used by both players to declare a draw.  BOTH players must send this command in order for the game to be declared a draw.
```
cleos push action chess draw '["alice", "0"]' -p alice@active
cleos push action chess draw '["bob", "0"]' -p bob@active
```

move - Used by one of the players to specify a move.  Parameters are, in order:
- player account name
- game id
- piece id - see 'Piece Indexes' above
- new position - see 'Board Positions' above
- promotion type - used when promoting pawns, ignored otherwise.  Values are: 0=bishop, 1=knight, 2=rook, 3=queen
```
cleos push action chess move '["alice", "0", "12", "29", "0"]' -p alice@active
```

#### Testing
to facilitate testing, I've included two python scripts.

`test_games/parse_pgn.py` - this is used to convert a PGN file (a common way of [annotating a chess game](https://en.wikipedia.org/wiki/Portable_Game_Notation)) into a list of move actions.  will take in a pgn file, and create a second file called `{filename}.sh`, which can be run after the contract is set up (contans a list of cleos commands)

`test_games/gen_fenurl.py` - this script will require the python [requests](http://docs.python-requests.org/en/master/) package to be installed, as it interfaces with the eos RPC API to grab the current game state.  It takes a game ID as an argument, and returns a URL to [lichess](https://lichess.org/editor), a website that provides a visualization of a [FEN String](https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation)




