import requests
import sys
import json

url = "http://localhost:8888/v1/chain/get_table_rows"
base_chessurl = "https://lichess.org/editor"

headers = {
  'accept': "application/json",
  'content-type': "application/json"
}

gameid = '0'
if len(sys.argv) > 1:
  gameid = sys.argv[1]

payload = '{"code":"chess", "table":"games", "scope":"chess", "json":"true", "index":"primary", "limit":"1", "lower_bound":"' + gameid + '"}'

response = requests.post(url, headers=headers, data=payload)

game = json.loads(response.text)["rows"][0]

board = [['x' for x in range(8)] for y in range(8)]
positions = game["piece_positions"]

def add_to_board(board, piece, pos) :
  if (pos > 0) :
    row = 7 - ((pos - 1) // 8)
    col = 7 - ((pos - 1) % 8)
    board[row][col] = piece
  return board

#white pieces
board = add_to_board(board, 'K', positions[0])
board = add_to_board(board, 'Q', positions[1])
for bidx in range(2, 4) :
  board = add_to_board(board, 'B', positions[bidx])
for nidx in range(4, 6) :
  board = add_to_board(board, 'N', positions[nidx])
for ridx in range(6, 8) :
  board = add_to_board(board, 'R', positions[ridx])
for pidx in range(8, 16) :
  board = add_to_board(board, 'P', positions[pidx])

#black pieces
board = add_to_board(board, 'k', positions[16])
board = add_to_board(board, 'q', positions[17])
for bidx in range(18, 20) :
  board = add_to_board(board, 'b', positions[bidx])
for nidx in range(20, 22) :
  board = add_to_board(board, 'n', positions[nidx])
for ridx in range(22, 24) :
  board = add_to_board(board, 'r', positions[ridx])
for pidx in range(24, 32) :
  board = add_to_board(board, 'p', positions[pidx])

chessurl = base_chessurl
for row in board :
  spaces = 0
  chessurl += '/'
  for col in row :
    if col == 'x' :
      spaces += 1
    elif spaces > 0 :
      chessurl += str(spaces)
      chessurl += col
      spaces = 0
    else :
      chessurl += col
  if spaces > 0 :
    chessurl += str(spaces)
chessurl += '_'

if int(game["move_count"]) % 2 == 0 :
  chessurl += 'w_'
else :
  chessurl += 'b_'

castle = int(game["castle"])
if castle == 0x0F :
  chessurl += '-'
else :
  if castle & 0x02 == 0:
    chessurl += 'K'
  if castle & 0x01 == 0:
    chessurl += 'Q'
  if castle & 0x08 == 0:
    chessurl += 'k'
  if castle & 0x04 == 0:
    chessurl += 'q'

chessurl += "_"

epidx = int(game["en_passant_idx"])
if epidx < 32 :
  eppos = positions[epidx]
  chessurl += chr(7 - ((eppos - 1) % 8) + ord('a'))
  chessurl += ((eppos - 1) // 8) + 1
else :
  chessurl += '-'

print(chessurl)

