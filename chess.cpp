#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/multi_index.hpp>

/* *
 * The game table array 'piece_positions' contains the 
 * locations of each piece on the board, according to the following reference
 *
 * Board Location Reference
 *  1  2  3  4  5  6  7  8  - White ranks
 *  9 10 11 12 13 14 15 16
 * 17 18 19 20 21 22 23 24
 * 25 26 27 28 29 30 31 32
 * 33 34 35 36 37 38 39 40
 * 41 42 43 44 45 46 47 48
 * 49 50 51 52 53 54 55 56
 * 57 58 59 60 61 62 63 64  - Black ranks
 *
 * Location 0 means piece has been captured.
 *
 * player_pieces Index Reference
 *
 * White Pieces
 *  0    : King
 *  1    : Queen
 *  2,3  : Bishop
 *  4,5  : Knight
 *  6,7  : Rook
 *  8-15 : Pawn
 *
 * Black Pieces
 *  16		: King
 *  17		: Queen
 *  18,19	: Bishop
 *  20,21	: Knight
 *  22,23	: Rook
 *  24-31   : Pawn
 *
 *  When making calls to the 'move' action, piece_id will translate to one of these player_piece index numbers, and new_position
 *  will be one of the numbers on the board location reference.
 *
 *  Example 1) the starting space of the white king will be space 5, so player_pieces[0] == 5 at the start of the game.
 *
 *  Example 2) Black wants to move their left knight from its starting space to space 43.  They will call move as follows-
 *  cleos push action chess move '["black_player_account", "game_id_number", "20", "43", "0"]' -p black_player_account@active
 *
 * Castling - 
 *  the castle variable is used to track if the kings and rooks have been moved, according to the following masks-
 *   0x01 - white king cannot castle queen side
 *   0x02 - white king cannot castle king side
 *   0x04 - black king cannot castle queen side
 *   0x08 - black king cannot castle king side
 *
 * En Passant - 
 *  En Passant is a rule to do with moving pawns.  When one player moves a pawn two spaces forward, their opponent can -on their next move only- use one of their pawns to capture that
 *  piece even if it is directly to the left or right of it.  The en_passant variable will track the index of any pawn that was moved 2 spaces forward for one turn.
 *
 * Pawn Promotion -
 *  two variables are used to track pawn promotion.  The promoted_pawns variable is a 16 bit bitmask where each bit represents one of the pawns on the board.  If the bit is set, that means the pawn was promoted.
 *   white pawns - 0x01 << (pawn_index - 8)
 *   black pawns - 0x01 << (pawn_index - 16)
 *
 *  promoted_pawn_types is a 32 bit value, and uses the same relative positioning as pawn_promotion, but uses two bits to represent the type of piece a pawn has been promoted to
 *  These values are passed into the 'promotion_type' argument of the 'move' action when moving a pawn to the promotion rank.  The value will be ignored if a pawn is not being promoted, but must always be specified (use any value).
 *    0 = bishop : 1 = knight : 2 = rook : 3 = queen
 *
 * */

#define W_CAS_Q 0x01
#define W_CAS_K 0x02
#define B_CAS_Q 0x04
#define B_CAS_K 0x08

#define PROMOTED_BISHOP 0x00
#define PROMOTED_KNIGHT 0x01
#define PROMOTED_ROOK   0x02
#define PROMOTED_QUEEN  0x03

using namespace eosio;

class [[eosio::contract("chess")]] chess : public contract {

  /***************************************
   * Public Contract Action Specifications
   ***************************************/
	public:
		using contract::contract;

		chess(name receiver, name code, datastream<const char*> ds): contract(receiver, code, ds), game_index(receiver, code.value) {}

		[[eosio::action]]
		void newgame (
      name& player_w, 
      name& player_b
    ) {
      //only the contract account can set up new games
			require_auth(_self);

			//set up game state and initialize pieces to starting positions
			game_index.emplace(get_self(), [&]( auto& row ) {
				row.game_id = game_index.available_primary_key();
				row.player_w = player_w;
				row.player_b = player_b;
			});
		}

		[[eosio::action]]
		void concede (
      name& player,
      uint64_t& game_id
    ) {
      //player must provide credentials to concede
			require_auth(player);

      //find the specified game, check that the calling account is one of the players, and set the other player to the winner
			name winner;
			auto itr = game_index.find(game_id);
			if (itr != game_index.end()) {
				if (player == itr->player_b) {
					winner = itr->player_w;
				} else if (player == itr->player_w) {
					winner = itr->player_b;
				} else {
					print("You are not a player in this game");
					return;
				}

        //update the game record
				game_index.modify(itr, player, [&](auto& game_row) {
					game_row.winner = winner;
				});
			} else {
				print("Unable to find a game with ID ", game_id);
				return;
			}
		}

    [[eosio::action]]
    void draw (
      name& player,
      uint64_t& game_id
    ) {
      //player must provide credentials to declare a draw
			require_auth(player);

      //find the specified game, check that the calling account is one of the players
			auto itr = game_index.find(game_id);
			if (itr != game_index.end()) {
        if (itr->player_w == player || itr->player_b == player) {
          game_index.modify(itr, player, [&](auto& game_row) {
            if (itr->draw_decl == ""_n) {
              game_row.draw_decl = player;
            } else if ((player == itr->player_b && itr->draw_decl == itr->player_w) || (player == itr->player_w && itr->draw_decl == itr->player_b)) {
              game_row.winner = get_self();
            }
          });
        } else {
          print("You are not a player in this game");
          return;
        }
      }
    }

		[[eosio::action]]
		void move (
      name& player, 
      uint64_t& game_id, 
      uint8_t& piece_id, 
      uint8_t& new_position, 
      uint8_t promotion_type
    ) {
      //player must provide authentication to make a move
			require_auth(player);

			//bounds check on position and piece IDs before iterating through game table
			if (new_position > 64 || new_position == 0) {
				print("Invalid position ID; must be a value between 1 - 64");
				return;
			}

			if (piece_id > 31) {
				print("Invalid piece ID; must be a value between 0 - 31");
				return;
			}

			//find game record
			auto itr = game_index.find(game_id);
			if (itr != game_index.end()) {

				//check that this game is still in progress
				if (itr->winner == get_self()) {
					print("This game has ended in a draw");
					return;
				} else if (itr->winner != ""_n) {
					print(itr->winner, " has already won this game");
					return;
				}

        //set up some variables needed to check the move's validity
        uint8_t captured_piece_index = 32;
        uint8_t castle = itr->castle;
        uint8_t en_passant_idx = itr->en_passant_idx;
        uint16_t promoted_pawns = itr->promoted_pawns;
        uint32_t promoted_pawn_types = itr->promoted_pawn_types;
        bool checkmate = false;

				//check that player is one of the players in this game
				if (player == itr->player_w) {
					//check that it's white player's turn
					if (itr->move_count % 2 != 0) {
						print("It is not your turn");
						return;
					}

					//check that the piece belongs to white player
					if (piece_id > 15) {
						print("Piece ", piece_id, " is not your piece");
						return;
					}

					//check that the move is valid, and get a reference to any captured piece index, and set variables for any special rules
					if (valid_move(piece_id, new_position, itr->piece_positions, castle, en_passant_idx, promoted_pawns, promoted_pawn_types, promotion_type, captured_piece_index, checkmate)) {

						game_index.modify(itr, player, [&](auto& game_row) {

              //update piece position
							game_row.piece_positions[piece_id] = new_position;

              //update move counter
							game_row.move_count = game_row.move_count + 1;

              //check if an enemy piece was captured and remove it;
							if (captured_piece_index < 32) {
								game_row.piece_positions[captured_piece_index] = 0;
							}

              //check if castling occurred (castle variable will be updated by the call to valid_move)
							if (itr->castle != castle) {
								game_row.castle = castle;

                //if the king is castling, move the appropriate rook as well
								if (piece_id == 0) {
									if (new_position == 2) {
										game_row.piece_positions[6] = 3;
									} else if (new_position == 6) {
										game_row.piece_positions[7] = 5;
									}
								}
							}

              //valid_move will update promoted_pawns and promoted_pawn_type if any pawns were promoted
              game_row.promoted_pawns = promoted_pawns;
              game_row.promoted_pawn_types = promoted_pawn_types;

              //valid_move will use en_passant_idx to specify any pawn that was moved two spaces forward, meaning it is eligible to be captured by the en passant rule on the next turn.
							game_row.en_passant_idx = en_passant_idx;

              if (checkmate) {
                game_row.winner = player;
              }
						});
					} else {
            print("Move invalid");
						return;
					}
				} else if (player == itr->player_b) {
					//check that it's black player's turn
					if (itr->move_count % 2 == 0) {
						print("It is not your turn");
						return;
					}

					//check that the piece belongs to black player
					if (piece_id < 16) {
						print("Piece ", piece_id, " is not your piece");
						return;
					}

					//check that the move is valid, and get a reference to any captured piece index, and set variables for any special rules
					if (valid_move(piece_id, new_position, itr->piece_positions, castle, en_passant_idx, promoted_pawns, promoted_pawn_types, promotion_type, captured_piece_index, checkmate)) {
						game_index.modify(itr, player, [&](auto& game_row) {

              //update piece position
							game_row.piece_positions[piece_id] = new_position;

              //update move counter
							game_row.move_count = game_row.move_count + 1;

              //check if an enemy piece was captured and remove it;
							if (captured_piece_index < 32) {
								game_row.piece_positions[captured_piece_index] = 0;
							}

              //check if castling occurred (castle variable will be updated by the call to valid_move)
							if (itr->castle != castle) {
								game_row.castle = castle;

                //if the king is castling, move the appropriate rook as well
								if (piece_id == 16) {
									if (new_position == 58) {
										game_row.piece_positions[22] = 59;
									}
									if (new_position == 62) {
										game_row.piece_positions[23] = 61;
									}
								}
							}

              //valid_move will update promoted_pawns and promoted_pawn_type if any pawns were promoted
              game_row.promoted_pawns = promoted_pawns;
              game_row.promoted_pawn_types = promoted_pawn_types;

              //valid_move will use en_passant_idx to specify any pawn that was moved two spaces forward, meaning it is eligible to be captured by the en passant rule on the next turn.
							game_row.en_passant_idx = en_passant_idx;

              if (checkmate) {
                game_row.winner = player;
              }
						});
					} else {
            print ("Move invalid");
						return;
					}
				} else {
					print("You are not a player in this game");
					return;
				}
			} else {
				print("Unable to find a game with ID ", game_id);
				return;
			}
    }

  /***************************************
   * Private Helper Functions
   ***************************************/
  private:

    /* *
     * valid_move
     *  checks the following-
     *  - is this piece alive?
     *  - is the path to the new position valid and unblocked?
     *  - does this move leave player's king unchecked?
     *  - was any piece captured? - if so, update captured_piece_index
     *  - are we castling? - if so, update castle
     *  - was a pawn moved two spaces from it's start? - if so, update en_passant_idx, if not, reset en_passant_idx
     *  - was a pawn promoted? - if so, update promoted_pawns and promoted_pawn_index
     *  - does this move lead to checkmate?
     *  - TODO: does this move lead to stalemate / draw?
     * */		
		bool valid_move (
			uint8_t piece_id, 
			uint8_t new_position, 
			const std::vector<uint8_t>& piece_positions, 
			uint8_t& castle,
			uint8_t& en_passant_idx, 
			uint16_t& promoted_pawns, 
			uint32_t& promoted_pawn_types, 
      uint8_t promotion_type,
			uint8_t& captured_piece_index,
      bool& checkmate
		) {

      //get the current position of piece_id from the array
			uint8_t current_position = piece_positions[piece_id];
      bool is_whites_move = piece_id < 16;

      //reset en_passant_idx
      en_passant_idx = 32;

      //make sure this piece is still uncaptured
			if (current_position == 0) {
				return false;
			}

      //make sure the new position is different than the current position
			if (current_position == new_position) {
				return false;
			}

      switch ( piece_id ) {
				case 0 : //white king
          if (!valid_king_move(current_position, new_position, castle, piece_positions, is_whites_move, captured_piece_index)) {
            //check castling special case
            if (
              (((castle & W_CAS_K) == 0) && current_position == 4 && new_position == 2) ||
              (((castle & W_CAS_Q) == 0) && current_position == 4 && new_position == 6)
            ) {
              //check that the path is unblocked
              uint8_t rook_index = is_whites_move ? (new_position == 2 ? 6 : 7) : (new_position == 58 ? 22 : 23);
              uint8_t rook_pos = piece_positions[rook_index];

              for (uint8_t index = 0; index < 32; ++index) {
                if (blocked(current_position, new_position, piece_positions[index]) || blocked(rook_pos, new_position, piece_positions[index])) {
                  return false;
                }
              }

              //king cannot castle out of check
              if (in_check(is_whites_move, piece_positions, promoted_pawns, promoted_pawn_types)) {
                return false;
              }

              //king cannot castle through check
              std::vector<uint8_t> new_piece_positions (piece_positions);
              new_piece_positions[is_whites_move ? 0 : 16] = ((current_position + new_position) / 2);
              if (in_check(is_whites_move, new_piece_positions, promoted_pawns, promoted_pawn_types)) {
                return false;
              }
            } else {
              return false;
            }
          }
          //king move was valid, so disable castling
          castle = castle | W_CAS_K | W_CAS_Q;
					break;
				case 1 : //white queen
          if (!valid_queen_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index)) {
            return false;
          }
					break;
				case 2 ... 3 : //white bishop
          if (!valid_bishop_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index)) {
            return false;
          }
					break;
				case 4 ... 5 : //white knight
          if (!valid_knight_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index)) {
            return false;
          }
					break;
				case 6 : //white rook, king side
          if (!valid_rook_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index)) {
            return false;
          } else {
            castle = castle | W_CAS_K;
          }
        case 7 : //white rook, queen side
          if (!valid_rook_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index)) {
            return false;
          } else {
            castle = castle | W_CAS_Q;
          }
					break;
				case 8 ... 15 : //white pawn
          if (!valid_pawn_move(piece_id, new_position, piece_positions, is_whites_move, captured_piece_index, promoted_pawns, promoted_pawn_types, en_passant_idx)) {
            return false;
          } else if (new_position > 56) {
            //pawn has reached promotion rank
            promote_pawn(piece_id, promoted_pawns, promoted_pawn_types, promotion_type);
          }
          break;
				case 16 : //black king
          if (!valid_king_move(current_position, new_position, castle, piece_positions, is_whites_move, captured_piece_index)) {
            //check castling special case
            if (
              (((castle & B_CAS_K) == 0) && current_position == 60 && new_position == 58) ||
              (((castle & B_CAS_Q) == 0) && current_position == 60 && new_position == 62)
            ) {
              //check that the path is unblocked
              uint8_t rook_index = is_whites_move ? (new_position == 2 ? 6 : 7) : (new_position == 58 ? 22 : 23);
              uint8_t rook_pos = piece_positions[rook_index];
              for (uint8_t index = 0; index < 32; ++index) {
                if (blocked(current_position, new_position, piece_positions[index]) || blocked(rook_pos, new_position, piece_positions[index])) {
                  return false;
                }
              }

              //king cannot castle out of check
              if (in_check(is_whites_move, piece_positions, promoted_pawns, promoted_pawn_types)) {
                return false;
              }

              //king cannot castle through check
              std::vector<uint8_t> new_piece_positions (piece_positions);
              new_piece_positions[is_whites_move ? 0 : 16] = ((current_position + new_position) / 2);
              if (in_check(is_whites_move, new_piece_positions, promoted_pawns, promoted_pawn_types)) {
                return false;
              }
            } else {
              return false;
            }
          }
          //king move was valid, so disable castling
          castle = castle | B_CAS_K | B_CAS_Q;
          break;
				case 17 : //black queen
          if (!valid_queen_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index)) {
            return false;
          }
          break;
				case 18 ... 19 : //black bishop
          if (!valid_bishop_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index)) {
            return false;
          }
          break;
				case 20 ... 21 : //black knight
          if (!valid_knight_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index)) {
            return false;
          }
					break;
				case 22 : //black rook, king side
          if (!valid_rook_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index)) {
            return false;
          } else {
            castle = castle | B_CAS_K;
          }
					break;
        case 23 : //black rook, queen side
          if (!valid_rook_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index)) {
            return false;
          } else {
            castle = castle | B_CAS_Q;
          }
          break;
				case 24 ... 31 : //black pawn
          if (!valid_pawn_move(piece_id, new_position, piece_positions, is_whites_move, captured_piece_index, promoted_pawns, promoted_pawn_types, en_passant_idx)) {
            return false;
          } else if (new_position < 9) {
            //pawn has reached promotion rank
            promote_pawn(piece_id, promoted_pawns, promoted_pawn_types, promotion_type);
          }
					break;
      }

      //create a new position vector to examine the new board state
      std::vector<uint8_t> new_piece_positions(piece_positions); 
      new_piece_positions[piece_id] = new_position;
      if (captured_piece_index < 32) {
        new_piece_positions[captured_piece_index] = 0;
      }

      //can't make a move that leaves our king in check
      if (in_check(is_whites_move, new_piece_positions, promoted_pawns, promoted_pawn_types)) {
        return false;
      }

      //figure out if the enemy king is in checkmate
      checkmate = in_checkmate(!is_whites_move, new_piece_positions, promoted_pawns, promoted_pawn_types);

      return true;
    }

    /* *
     * returns true if the position specified is checked by the opposing color.
     *  @param is_white_piece - specifies the color of the current player ex) is_white_piece == true means to check if any black pieces are checking position
     *  @param piece_positions - vector of all piece positions
     *  @param promoted_pawns - bit vector specifying which pawns are promoted
     *  @param promoted_pawn_type - specifies what type of piece a pawn has been promoted to
     * */
    bool in_check (
      bool is_whites_move,
      const std::vector<uint8_t>& piece_positions,
			uint16_t promoted_pawns,
			uint32_t promoted_pawn_types
    ) {
      //find out which enemy pieces are threatening the king
      uint8_t throwaway = 0; //validity functions return the index of any enemy piece in the target 'position', but we don't care about that here so we use this throwaway variable as a placeholder
      uint8_t position = is_whites_move ? piece_positions[0] : piece_positions[16]; //position of the king we are checking

      //check enemy king - no possibility of another piece being in between, so we can just check if the enemy king is next to this space
      uint8_t enemy_king_pos = is_whites_move ? piece_positions[16] : piece_positions[0];
      if (valid_king_move(enemy_king_pos, position, 0xFF, piece_positions, is_whites_move, throwaway)) {
        return true;
      }

      uint8_t enemy_queen_pos = is_whites_move ? piece_positions[17] : piece_positions[1];
      if (valid_queen_move(enemy_queen_pos, position, piece_positions, !is_whites_move, throwaway)) {
        return true;
      }

      uint8_t enemy_bishop1_pos = is_whites_move ? piece_positions[18] : piece_positions[2];
      if (valid_bishop_move(enemy_bishop1_pos, position, piece_positions, !is_whites_move, throwaway)) {
        return true;
      }

      uint8_t enemy_bishop2_pos = is_whites_move ? piece_positions[19] : piece_positions[3];
      if (valid_bishop_move(enemy_bishop2_pos, position, piece_positions, !is_whites_move, throwaway)) {
        return true;
      }

      uint8_t enemy_knight1_pos = is_whites_move ? piece_positions[20] : piece_positions[4];
      if (valid_knight_move(enemy_knight1_pos, position, piece_positions, !is_whites_move, throwaway)) {
        return true;
      }

      uint8_t enemy_knight2_pos = is_whites_move ? piece_positions[21] : piece_positions[5];
      if (valid_knight_move(enemy_knight2_pos, position, piece_positions, !is_whites_move, throwaway)) {
        return true;
      }

      uint8_t enemy_rook1_pos = is_whites_move ? piece_positions[22] : piece_positions[6];
      if (valid_rook_move(enemy_rook1_pos, position, piece_positions, !is_whites_move, throwaway)) {
        return true;
      }

      uint8_t enemy_rook2_pos = is_whites_move ? piece_positions[23] : piece_positions[7];
      if (valid_rook_move(enemy_rook2_pos, position, piece_positions, !is_whites_move, throwaway)) {
        return true;
      }

      uint8_t enemy_pawn_index_offset = is_whites_move ? 24 : 8;
      for (uint8_t index = 0; index < 8; ++index) {
        uint8_t enemy_pawn_index = index + enemy_pawn_index_offset;
        uint8_t ep_invalid = 32; //can't check a space using en passant, so pass in the invalid index to the pawn validation function
        if (valid_pawn_move(enemy_pawn_index, position, piece_positions, !is_whites_move, throwaway, promoted_pawns, promoted_pawn_types, ep_invalid)) {
          return true;
        }
      }

      return false;
    }

    bool in_checkmate (
      bool check_white,
      const std::vector<uint8_t> piece_positions,
			uint16_t promoted_pawns,
			uint32_t promoted_pawn_types
    ) {
      uint8_t king_pos = check_white ? piece_positions[0] : piece_positions[16];
      //TODO: use 'throwaway' to update new_piece_positions if a piece is captured
      uint8_t throwaway = 32;
      std::vector<uint8_t> new_piece_positions (piece_positions);

      //first, check if the king can move 1 space in any direction
      new_piece_positions[king_pos] = piece_positions[king_pos] + 1;
      if (valid_king_move(piece_positions[king_pos], new_piece_positions[king_pos], 0xFF, piece_positions, check_white, throwaway)) {
        if (!in_check(check_white, new_piece_positions, promoted_pawns, promoted_pawn_types)) {
          return false;
        }
      }

      new_piece_positions[king_pos] = piece_positions[king_pos] - 1;
      if (valid_king_move(piece_positions[king_pos], new_piece_positions[king_pos], 0xFF, piece_positions, check_white, throwaway)) {
        if (!in_check(check_white, new_piece_positions, promoted_pawns, promoted_pawn_types)) {
          return false;
        }
      }

      for (int offset = -9; offset < -6; ++offset) {
        uint8_t king_pos = check_white ? 0 : 16;
        new_piece_positions[king_pos] = piece_positions[king_pos] + offset;
        if (valid_king_move(piece_positions[king_pos], new_piece_positions[king_pos], 0xFF, piece_positions, check_white, throwaway)) {
          if (!in_check(check_white, new_piece_positions, promoted_pawns, promoted_pawn_types)) {
            return false;
          }
        }
      }

      for (int offset = 7; offset < 10; ++offset) {
        uint8_t king_pos = check_white ? 0 : 16;
        new_piece_positions[king_pos] = piece_positions[king_pos] + offset;
        if (valid_king_move(piece_positions[king_pos], new_piece_positions[king_pos], 0xFF, piece_positions, check_white, throwaway)) {
          if (!in_check(check_white, new_piece_positions, promoted_pawns, promoted_pawn_types)) {
            return false;
          }
        }
      }

      //TODO: look for any pieces that threaten the king's current position.  If any exist, see if they are capturable in one move, or if they can be blocked by a non-king move.
      //if all valid king moves are checked, see if the current position is checked
      if (!in_check(check_white, piece_positions, promoted_pawns, promoted_pawn_types)) {
        return false;
      } else {

      }

      return true;
    }

    bool valid_king_move (
      uint8_t current_position,
      uint8_t new_position,
      uint8_t castle,
      const std::vector<uint8_t>& piece_positions,
      bool is_whites_move,
      uint8_t& captured_piece_index
    ) {
      int diff = new_position - current_position;
      int abs_diff = std::abs(diff);

      //check that the king has only moved 1 space in any direction
			if (abs_diff != 1 && abs_diff != 7 && abs_diff != 8 && abs_diff != 9) {
				return false;
			}

      //check that the king has not moved off the edge
			if ( ((diff == -7 || diff == 1 || diff == 9) && current_position % 8 == 0) ||
				((diff == -9 || diff == -8 || diff == -7) && current_position < 9) ||
				((diff == -9 || diff == -1 || diff == 7) && current_position % 8 == 1) ||
				((diff == 7 || diff == 8 || diff == 9) && current_position > 56) )
			{
				return false;
			}

      //search through other pieces to see if one of them has been captured, or if a friendly piece is blocking this move
      for (int index = 0; index < 32; ++index) {
        if (piece_positions[index] == new_position) {
          if (is_enemy_piece(is_whites_move, index)) {
            captured_piece_index = index;
          } else {
            return false;
          }
        }
      }

			return true;
    }

    bool valid_queen_move (
      uint8_t current_position,
      uint8_t new_position,
      const std::vector<uint8_t>& piece_positions,
      bool is_whites_move,
      uint8_t& captured_piece_index
    ) {

      //current position zero means this piece has already been captured
      if (current_position == 0) {
        return false;
      }

      //check that the move is on a row, column, or diagonal
      if (
        !same_row(current_position, new_position) &&
        !same_col(current_position, new_position) &&
        !same_ne_diag(current_position, new_position) &&
        !same_nw_diag(current_position, new_position) &&
        !same_se_diag(current_position, new_position) &&
        !same_sw_diag(current_position, new_position)
      ) {
        return false;
      }  

      //check that none of the other uncaptured pieces on the board are blocking this move, and figure out if this move captures another piece
      for (uint8_t index = 0; index < 32; ++index) {
        if (piece_positions[index] == new_position) {
          if (is_enemy_piece(is_whites_move, index)) {
            captured_piece_index = index;
          } else {
            return false;
          }
        } else if (piece_positions[index] != current_position && piece_positions[index] != 0) {
          if (blocked(current_position, new_position, piece_positions[index])) {
            return false;
          }
        }
      }

      return true;
    }

    bool valid_bishop_move (
      uint8_t current_position,
      uint8_t new_position,
      const std::vector<uint8_t>& piece_positions,
      bool is_whites_move,
      uint8_t& captured_piece_index
    ) {

      //current position zero means this piece has already been captured
      if (current_position == 0) {
        return false;
      }

      //check move is on a diagonal from current position
      if (
        !same_ne_diag(current_position, new_position) &&
        !same_nw_diag(current_position, new_position) &&
        !same_se_diag(current_position, new_position) &&
        !same_sw_diag(current_position, new_position)
      ) {
        return false;
      }

      //check for other pieces blocking this move, and find any captured pieces
      for (uint8_t index = 0; index < 32; ++index) {
        if (piece_positions[index] == new_position) {
          if (is_enemy_piece(is_whites_move, index)) {
            captured_piece_index = index;
          } else {
            return false;
          }
        } else if (piece_positions[index] != current_position && piece_positions[index] != 0) {
          if (blocked(current_position, new_position, piece_positions[index])) {
            return false;
          }
        }
      }

      return true;
    }

    bool valid_knight_move (
      uint8_t current_position,
      uint8_t new_position,
      const std::vector<uint8_t>& piece_positions,
      bool is_whites_move,
      uint8_t& captured_piece_index
    ) {
			int diff = new_position - current_position;
			int abs_diff = std::abs(diff);

      //current position zero means this piece has already been captured
      if (current_position == 0) {
        return false;
      }

      //check that the move is valid
			if (abs_diff != 6 && abs_diff != 10 && abs_diff != 15 && abs_diff != 17) {
				return false;
			}

      //check that the move does not send the knight off the edge
			if (((diff == 6  || diff == -10) && ((current_position - 1) % 8) < 2) ||
				((diff == 15 || diff == -17) && ((current_position - 1) % 8) < 1) ||
				((diff == 17 || diff == -15) && ((current_position - 1) % 8) > 6) ||
				((diff == 10 || diff ==  -6) && ((current_position - 1) % 8) > 5) ||
				((diff == -17 || diff == -15) && (current_position < 17)) ||
				((diff == -10 || diff == -6) && (current_position < 9)) ||
				((diff == 6 || diff == 10) && (current_position > 56)) ||
				((diff == 15 || diff == 17) && (current_position > 48)) )
			{
				return false;
			}
      
      for (int index = 0; index < 32; ++index) {
        if (piece_positions[index] == new_position) {
          if (is_enemy_piece(is_whites_move, index)) {
            captured_piece_index = index;
          } else {
            return false;
          }
        }
      }

      return true;
    }

    bool valid_rook_move (
      uint8_t current_position,
      uint8_t new_position,
      const std::vector<uint8_t>& piece_positions,
      bool is_whites_move,
      uint8_t& captured_piece_index
    ) {

      //current position zero means this piece has already been captured
      if (current_position == 0) {
        return false;
      }

      //check move is on a row or column from current position
      if (
        !same_row(current_position, new_position) &&
        !same_col(current_position, new_position)
      ) {
        return false;
      }

      for (uint8_t index = 0; index < 32; ++index) {
        if (piece_positions[index] == new_position) {
          if (is_enemy_piece(is_whites_move, index)) {
            captured_piece_index = index;
          } else {
            return false;
          }
        } else {
          if (blocked(current_position, new_position, piece_positions[index])) {
            return false;
          }
        }
      }

      return true;
    }

    bool valid_pawn_move (
      uint8_t pawn_index, //NOTICE: this needs an index instead of a position
      uint8_t new_position,
      const std::vector<uint8_t>& piece_positions,
      bool is_whites_move,
      uint8_t& captured_piece_index,
      uint16_t promoted_pawns,
      uint32_t promoted_pawn_types,
      uint8_t& en_passant_idx
    ) {

      uint8_t current_position = piece_positions[pawn_index];

      //current position zero means this piece has already been captured
      if (current_position == 0) {
        return false;
      }

      //first, check if this pawn has been promoted
      uint8_t promoted_pawn_type = 0;
      if (is_pawn_promoted(pawn_index, promoted_pawns, promoted_pawn_types, promoted_pawn_type)) {
        //promoted pawn
        if (promoted_pawn_type == PROMOTED_BISHOP) {
          return valid_bishop_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index);
        } else if (promoted_pawn_type == PROMOTED_KNIGHT) {
          return valid_knight_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index);
        } else if (promoted_pawn_type == PROMOTED_ROOK) {
          return valid_rook_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index);
        } else if (promoted_pawn_type == PROMOTED_QUEEN) {
          return valid_queen_move(current_position, new_position, piece_positions, is_whites_move, captured_piece_index);
        } else {
          return false;
        }
      } else {
        //normal pawn
        int diff = new_position - current_position;
        if (is_whites_move) {
          //white pawns can only move down
          if (diff == 8 || diff == 16) {
            //straight moves must be unblocked
            for (int index = 0; index < 32; ++index) {
              if (piece_positions[index] == new_position || piece_positions[index] == current_position + 8) {
                return false;
              }
            }
            en_passant_idx = diff == 16 ? pawn_index : 32;
          } else if (diff == 7) {
            //diagonal moves must capture
            for (int index = 0; index < 32; ++index) {
              if (is_enemy_piece(is_whites_move, index)) {
                //check for enemy piece in target position, or in the en passant position
                if (piece_positions[index] == new_position || (index == en_passant_idx && piece_positions[index] == (current_position - 1))) {
                  captured_piece_index = index;
                  break;
                }
              } else {
                if (piece_positions[index] == new_position) {
                  return false;
                }
              }
            }
          } else if (diff == 9) {
            //diagonal moves must capture
            for (int index = 0; index < 32; ++index) {
              if (is_enemy_piece(is_whites_move, index)) {
                //check for enemy piece in target position, or in the en passant position
                if (piece_positions[index] == new_position || (index == en_passant_idx && piece_positions[index] == (current_position + 1))) {
                  captured_piece_index = index;
                  break;
                }
              } else {
                if (piece_positions[index] == new_position) {
                  return false;
                }
              }
            }
          } else {
            return false;
          }
        } else {
          //black pawns can only move up
          if (diff == -8 || diff == -16) {
            //straight moves must be unblocked
            for (int index = 0; index < 32; ++index) {
              if (piece_positions[index] == new_position || piece_positions[index] == current_position - 8) {
                return false;
              }
            }
            en_passant_idx = diff == -16 ? pawn_index : 32;
          } else if (diff == -7) {
            //diagonal moves must capture
            for (int index = 0; index < 32; ++index) {
              if (is_enemy_piece(is_whites_move, index)) {
                //check for enemy piece in target position, or in the en passant position
                if (piece_positions[index] == new_position || (index == en_passant_idx && piece_positions[index] == (current_position + 1))) {
                  captured_piece_index = index;
                  break;
                }
              } else {
                if (piece_positions[index] == new_position) {
                  return false;
                }
              }
            }
          } else if (diff == -9) {
            //diagonal moves must capture
            for (int index = 0; index < 32; ++index) {
              if (is_enemy_piece(is_whites_move, index)) {
                //check for enemy piece in target position, or in the en passant position
                if (piece_positions[index] == new_position || (index == en_passant_idx && piece_positions[index] == (current_position - 1))) {
                  captured_piece_index = index;
                  break;
                }
              } else {
                if (piece_positions[index] == new_position) {
                  return false;
                }
              }
            }
          } else {
            return false;
          }
        }
      }

      return true;
    }

    /* *
     * is_pawn_promoted
     *  convenience function for checking pawn promotion
     *  returns true if the pawn in pawn_index is alive, and has been promoted.  If so, returns the piece type in the promoted_pawn_type variable
     * */
    bool is_pawn_promoted (
      uint8_t pawn_index,
      uint16_t promoted_pawns,
      uint32_t promoted_pawn_types,
      uint8_t& promoted_pawn_type
    ) {
      if (pawn_index > 31) {
        return false;
      }

      uint8_t offset = 0;
      if (pawn_index < 16) {
        offset = pawn_index - 8;
      } else {
        offset = pawn_index - 16;
      }

      bool promoted = (promoted_pawns & (0x01 << offset)) > 0;

      if (promoted) {
        promoted_pawn_type = (promoted_pawn_types & (0x03 << (offset * 2))) >> (offset * 2);
      }

      return promoted;
    }

    /* *
     * promote_pawn
     *  convenience function for promoting a pawn
     *  updates promoted_pawns and promoted_pawn_types
     * */
    void promote_pawn (
      uint8_t pawn_index,
      uint16_t& promoted_pawns,
      uint32_t& promoted_pawn_types,
      uint8_t promoted_pawn_type
    ) {
      if (pawn_index > 31) {
        return;
      }
      
      uint8_t offset = 0;
      if (pawn_index < 16) {
        offset = pawn_index - 8;
      } else {
        offset = pawn_index - 16;
      }

      promoted_pawns = (promoted_pawns | (0x01 << offset));
      promoted_pawn_types = (promoted_pawn_types | ((promoted_pawn_type & 0x03) << (offset * 2)));
    }

    /* *
     * same_row
     *  returns true if the two positions are valid and on the same row
     * */
    bool same_row (
      uint8_t position1, 
      uint8_t position2
    ) {
      if (position1 < 65 && position2 < 65) {
        if (((position1 - 1) / 8) == ((position2 - 1) / 8)) {
          return true;
        }
      }
      return false;
    }

    /* *
     * same_col
     *  returns true if the two positions are valid and on the same column
     * */
    bool same_col(
      uint8_t position1, 
      uint8_t position2
    ) {
      if (position1 < 65 && position2 < 65) {
        if ((position1 % 8) == (position2 % 8)) {
          return true;
        }
      }
      return false;
    }

    /* *
     * same_nw_diag
     *  returns true if the two positions are valid, and position 2 is on position 1's northwest diagonal
     * */
    bool same_nw_diag (
      uint8_t position1, 
      uint8_t position2
    ) {
      if (position1 < 65 && position2 < 65) {
        uint8_t diff = std::abs(position2 - position1);
        uint8_t col1 = (position1 - 1) % 8;
        uint8_t col2 = (position2 - 1) % 8;
        if ((position1 > position2) && (col1 > col2) && (diff % 9 == 0)) {
          return true;
        }
      }
      return false;
    }

    /* *
     * same_ne_diag
     *  returns true if the two positions are valid, and position 2 is on position 1's northeast diagonal
     * */
    bool same_ne_diag (
      uint8_t position1, 
      uint8_t position2
    ) {
      if (position1 < 65 && position2 < 65) {
        uint8_t diff = std::abs(position2 - position1);
        uint8_t col1 = (position1 - 1) % 8;
        uint8_t col2 = (position2 - 1) % 8;

        if ((position1 > position2) && (col1 < col2) && (diff % 7 == 0)) {
          return true;
        }
      }
      return false;
    }

    /* *
     * same_sw_diag
     *  returns true if the two positions are valid, and position 2 is on position 1's southwest diagonal
     * */
    bool same_sw_diag (
      uint8_t position1, 
      uint8_t position2
    ) {
      if (position1 < 65 && position2 < 65) {
        uint8_t diff = std::abs(position2 - position1);
        uint8_t col1 = (position1 - 1) % 8;
        uint8_t col2 = (position2 - 1) % 8;

        if ((position1 < position2) && (col1 > col2) && (diff % 7 == 0)) {
          return true;
        }
      }
      return false;
    }

    /* *
     * same_se_diag
     *  returns true if the two positions are valid, and position 2 is on position 1's southeast diagonal
     * */
    bool same_se_diag (
      uint8_t position1, 
      uint8_t position2
    ) {
      if (position1 < 65 && position2 < 65) {
        uint8_t diff = std::abs(position2 - position1);
        uint8_t col1 = (position1 - 1) % 8;
        uint8_t col2 = (position2 - 1) % 8;

        if ((position1 < position2) && (col1 < col2) && (diff % 9 == 0)) {
          return true;
        }
      }
      return false;
    }

    /* *
     * blocked
     *  returns true if the test_position is between current_position and new_position on a row, column, or diagonal
     * */
    bool blocked (
      uint8_t current_position,
      uint8_t new_position,
      uint8_t test_position
    ) {
      if ((current_position < 65) && (new_position < 65) && (test_position < 65)) {
        if (
          (same_row(current_position, new_position) && same_row(current_position, test_position)) ||
          (same_col(current_position, new_position) && same_col(current_position, test_position)) ||
          (same_ne_diag(current_position, new_position) && same_ne_diag(current_position, test_position)) ||
          (same_nw_diag(current_position, new_position) && same_nw_diag(current_position, test_position)) ||
          (same_se_diag(current_position, new_position) && same_se_diag(current_position, test_position)) ||
          (same_sw_diag(current_position, new_position) && same_sw_diag(current_position, test_position))
        ) {
          if ((current_position < new_position) && (current_position < test_position) && (test_position < new_position)) {
            return true;
          }
          if ((current_position > new_position) && (current_position > test_position) && (test_position > new_position)) {
            return true;
          }
        }
      }
      return false;
    }

    bool is_enemy_piece (
      bool is_whites_move,
      uint8_t piece_index
    ) {
      if (is_whites_move && piece_index > 15) { return true; }
      else if (!is_whites_move && piece_index < 16) { return true; }
      return false;
    }

		struct [[eosio::table]] game {
			uint64_t game_id;
			name player_b;
			name player_w;
			name winner = ""_n;
      name draw_decl = ""_n;
			uint32_t move_count = 0;
			uint8_t castle = 0;
			uint8_t en_passant_idx = 32;
			uint16_t promoted_pawns = 0;
			uint32_t promoted_pawn_types = 0;
			std::vector<uint8_t> piece_positions {4, 5, 3, 6, 2, 7, 1, 8, 9, 10, 11, 12, 13, 14, 15, 16, 60, 61, 59, 62, 58, 63, 57, 64, 49, 50, 51, 52, 53, 54, 55, 56};

			auto primary_key() const { return game_id; }
		};

		typedef eosio::multi_index<"games"_n, game> games;

		games game_index;
};

EOSIO_DISPATCH( chess, (newgame) (move) (concede) (draw) )
