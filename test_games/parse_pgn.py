import sys

def same_row (position1, position2) :
  if position1 < 65 and position2 < 65 :
    if (((position1 - 1) // 8) == ((position2 - 1) // 8)) :
      return True
  return False


def same_col(position1, position2) :
  if position1 < 65 and position2 < 65 :
    if ((position1 % 8) == (position2 % 8)) :
      return True
  return False


def same_nw_diag(position1, position2) :
  if position1 < 65 and position2 < 65 :
    diff = abs(position2 - position1)
    col1 = (position1 - 1) % 8
    col2 = (position2 - 1) % 8
    if ((position1 > position2) and (col1 > col2) and (diff % 9 == 0)) :
      return True
  return False


def same_ne_diag (position1, position2) :
  if position1 < 65 and position2 < 65 :
    diff = abs(position2 - position1)
    col1 = (position1 - 1) % 8
    col2 = (position2 - 1) % 8
    if ((position1 > position2) and (col1 < col2) and (diff % 7 == 0)) :
      return True
  return False


def same_sw_diag (position1, position2) :
  if position1 < 65 and position2 < 65 :
    diff = abs(position2 - position1)
    col1 = (position1 - 1) % 8
    col2 = (position2 - 1) % 8
    if ((position1 < position2) and (col1 > col2) and (diff % 7 == 0)) :
      return True
  return False


def same_se_diag (position1, position2) :
  if position1 < 65 and position2 < 65 :
    diff = abs(position2 - position1)
    col1 = (position1 - 1) % 8
    col2 = (position2 - 1) % 8
    if ((position1 < position2) and (col1 < col2) and (diff % 9 == 0)) :
      return True
  return False


def blocked (position1, position2, testposition) :
  if same_row(position1, position2) :
    if ((testposition < position1 and testposition > position2) or
        (testposition > position1 and testposition < position2)) :
      return True;
  if same_col(position1, position2) and same_col(position1, testposition) :
    if ((testposition < position1 and testposition > position2) or
        (testposition > position1 and testposition < position2)) :
      return True;
  if same_ne_diag(position1, position2) and same_ne_diag(position1, testposition) :
    if testposition > position2 :
      return True;
  if same_nw_diag(position1, position2) and same_nw_diag(position1, testposition) :
    if testposition > position2 :
      return True;
  if same_sw_diag(position1, position2) and same_sw_diag(position1, testposition) :
    if testposition < position2 :
      return True;
  if same_se_diag(position1, position2) and same_se_diag(position1, testposition) :
    if testposition < position2 :
      return True;
  return False;


def get_rook(pos, boardstate, whites_move, ambiguous) :
  rp1 = boardstate[22]
  rp1index = 22
  if whites_move :
    rp1 = boardstate[6]
    rp1index = 6
  if rp1 == 0 :
    return rp1index + 1
  if ambiguous != "" :
    if ambiguous.isdigit() :
      num = (ord(ambiguous) - ord('0')) * 8
      if rp1 >= num and rp1 <= (num + 8) :
        return rp1index
      else :
        return rp1index + 1
    else :
      char = 8 - (ord(ambiguous) - ord('a'))
      if (rp1 - 1) % 8 == char :
        return rp1index
      else :
        return rp1index + 1
  if ((same_row(pos, rp1) == True) or
     (same_col(pos, rp1) == True)) :
    block = False;
    for i in range(32) :
      if blocked(rp1, pos, boardstate[i]) :
        block = True;
    if not block :
      return rp1index
    else :
      return rp1index + 1
  else :
    return rp1index + 1


def get_bishop(pos, boardstate, whites_move, ambiguous) :
  bp1 = boardstate[18]
  bp1index = 18
  if whites_move :
    bp1 = boardstate[2]
    bp1index = 2
  if bp1 == 0 :
    return bp1index + 1
  if ambiguous != "" :
    if ambiguous.isdigit() :
      num = (ord(ambiguous) - ord('0')) * 8
      if bp1 >= num and bp1 <= (num + 8) :
        return bp1index
      else :
        return bp1index + 1
    else :
      char = 8 - (ord(ambiguous) - ord('a'))
      if (bp1 - 1) % 8 == char :
        return bp1index
      else :
        return bp1index + 1
  if ((same_ne_diag(pos, bp1) == True) or
     (same_nw_diag(pos, bp1) == True) or
     (same_se_diag(pos, bp1) == True) or
     (same_sw_diag(pos, bp1) == True)) :
    block = False;
    for i in range(32) :
      if blocked(bp1, pos, boardstate[i]) :
        block = True;
    if not block :
      return bp1index
    else :
      return bp1index + 1
  else :
    return bp1index + 1


def get_knight(pos, boardstate, whites_move, ambiguous) :
  kp1 = boardstate[20]
  kp1index = 20
  if whites_move :
    kp1 = boardstate[4]
    kp1index = 4
  if kp1 == 0 :
    return kp1index + 1
  if ambiguous != "" :
    if ambiguous.isdigit() :
      num = (ord(ambiguous) - ord('0')) * 8
      if kp1 >= num and kp1 <= (num + 8) :
        return kp1index
      else :
        return kp1index + 1
    else :
      char = 8 - (ord(ambiguous) - ord('a'))
      if (kp1 - 1) % 8 == char :
        return kp1index
      else :
        return kp1index + 1
  diff = kp1 - pos
  if (((diff == 6 or diff == -10) and ((pos - 1) % 8) > 1) or 
    ((diff == 15 or diff == -17) and ((pos - 1) % 8) > 0) or
    ((diff == 17 or diff == -15) and ((pos - 1) % 8) < 7) or
    ((diff == 10 or diff ==  -6) and ((pos - 1) % 8) < 6) or
    ((diff == -17 or diff == -15) and (pos > 16)) or
    ((diff == -10 or diff == -6) and (pos > 8)) or
    ((diff == 6 or diff == 10) and (pos < 57)) or
    ((diff == 15 or diff == 17) and (pos < 49)) ) :
    return kp1index
  else :
    return kp1index + 1


def get_pawn(pos, boardstate, whites_move, ambiguous, capture) :
  if capture == False :
    onespace = 32
    twospace = 32
    if whites_move == True :
      for i in range(8, 16) :
        if boardstate[i] > 0 and boardstate[i] == pos - 8 :
          onespace = i
        if boardstate[i] > 0 and boardstate[i] == pos - 16 :
          twospace = i
    else :
      for i in range(24, 32) :
        if boardstate[i] > 0 and boardstate[i] == pos + 8 :
          onespace = i
        if boardstate[i] > 0 and boardstate[i] == pos + 16 :
          twospace = i
    if onespace < 32 :
      return onespace
    else :
      return twospace
  else :
    if whites_move == True :
      if ambiguous != "" :
        if ambiguous.isdigit() :
          rowmin = (ord(ambiguous) - ord('1')) * 8
          for i in range(8, 16) :
            if boadstate[i] > rowmin and boardstate[i] <= rowmin + 8 and (boardstate[i] == pos - 7 or boardstate[i] == pos - 9) :
              return i
        else :
          col = 8 - (ord(ambiguous) - ord('a'))
          for i in range(8, 16) :
            if ((col - 1) % 8 == (boardstate[i] - 1) % 8) and (boardstate[i] == pos - 7 or boardstate[i] == pos - 9) :
              return i
      else :
        for i in range(8, 16) :
          if (boardstate[i] == pos - 7 or boardstate[i] == pos - 9) :
            return i
    else :
      if ambiguous != "" :
        if ambiguous.isdigit() :
          rowmin = (ord(ambiguous) - ord('1')) * 8
          for i in range(24, 32) :
            if boadstate[i] > rowmin and boardstate[i] <= rowmin + 8 and (boardstate[i] == pos + 7 or boardstate[i] == pos + 9) :
              return i
        else :
          col = 8 - (ord(ambiguous) - ord('a'))
          for i in range(24, 32) :
            if ((col - 1) % 8 == (boardstate[i] - 1) % 8) and (boardstate[i] == pos + 7 or boardstate[i] == pos + 9) :
              return i
      else :
        for i in range(24, 32) :
          if (boardstate[i] == pos + 7 or boardstate[i] == pos + 9) :
            return i
  return -1


def generate_cleos_command(move, boardstate, turn) :
  whites_move = turn % 2 == 1
  player = ""
  rstr = ""
  pos = 0
  idx = 0
  promotion = 0
  if whites_move :
    player = "alice"
  else :
    player = "bob"
  if move[0] == 'K' :
    #king
    move = move[1:]
    (c, a, pos) = parse_move(move)
    if whites_move != True :
      idx = 16
  elif move[0] == 'Q' :
    #queen
    move = move[1:]
    (c, a, pos) = parse_move(move)
    idx = 17
    if whites_move == True :
      idx = 1
  elif move[0] == 'R' :
    #rook
    move = move[1:]
    (c, a, pos) = parse_move(move)
    if c == True :
      for index, piece_pos in enumerate(boardstate) :
        if piece_pos == pos :
          boardstate[index] = 0
    idx = get_rook(pos, boardstate, whites_move, a)
    boardstate[idx] = pos
  elif move[0] == 'B' :
    #bishop
    move = move[1:]
    (c, a, pos) = parse_move(move)
    if c == True :
      for index, piece_pos in enumerate(boardstate) :
        if piece_pos == pos :
          boardstate[index] = 0
    idx = get_bishop(pos, boardstate, whites_move, a)
    boardstate[idx] = pos
  elif move[0] == 'N' :
    #knight
    move = move[1:]
    (c, a, pos) = parse_move(move)
    if c == True :
      for index, piece_pos in enumerate(boardstate) :
        if piece_pos == pos :
          boardstate[index] = 0
    idx = get_knight(pos, boardstate, whites_move, a)
    boardstate[idx] = pos
  elif move[0] == 'O' :
    #special case for castling
    if move == "O-O-O" :
      if whites_move == True :
        pos = 6
        boardstate[0] = 6
        boardstate[7] = 5
      else :
        idx = 16
        pos = 62
        boardstate[16] = 62
        boardstate[23] = 61
    else :
      if whites_move == True :
        pos = 2
        boardstate[0] = 2
        boardstate[6] = 3
      else :
        idx = 16
        pos = 58
        boardstate[16] = 58
        boardstate[22] = 59
  else :
    #pawn
    (c, a, pos) = parse_move(move)
    idx = get_pawn(pos, boardstate, whites_move, a, c)
    if c == True :
      found = False
      for index, piece_pos in enumerate(boardstate) :
        if piece_pos == pos :
          found = True
          boardstate[index] = 0
      if not found : # this should cover en passant rule
        for index, piece_pos in enumerate(boardstate) :
          if whites_move and piece_pos  == pos - 8 :
            boardstate[index] = 0
          if not whites_move and piece_pos == pos + 8 :
            boardstate[index] = 0
    boardstate[idx] = pos
  return (boardstate, "cleos push action chess move \'[\"" + player + "\", \"0\", \"" + str(idx) + "\", \"" + str(pos) + "\", \"" + str(promotion) + "\"]\' -p " + player + "@active\n")


def parse_move(movetext) :
  textindex = 0
  capture = False
  ambiguous_rank = ''
  if movetext[textindex + 1].isdigit() != True :
    if movetext[0] == 'x' :
      capture = True
    else :
      ambiguous_rank = movetext[0]
    textindex += 1
  if movetext[textindex] == 'x' :
    capture = True
    textindex += 1
  col = movetext[textindex]
  row = movetext[textindex + 1]
  position = (8 * (ord(row) - ord('1'))) + (8 - (ord(col) - ord('a')))
  return (capture, ambiguous_rank, position)


#start main script
for filename in sys.argv :
  if filename != "parse_pgn.py" :
    pgnfile = open(filename, "r")
    testfile = open(filename + ".sh", "w+")
    gamestring = ""
    gamestarted = False
    for line in pgnfile :
      if line[0] == '1' :
        gamestarted = True
      if gamestarted :
        gamestring += line
    gsindex = 0
    nextmove = ""
    moves = []
    while (gsindex < len(gamestring)) :
      if gamestring[gsindex] != ' ' :
        nextmove += gamestring[gsindex]
      else :
        if nextmove != "" :
          moves.append(nextmove)
          nextmove = ""
      gsindex += 1
    boardstate = [4, 5, 3, 6, 2, 7, 1, 8, 9, 10, 11, 12, 13, 14, 15, 16, 60, 61, 59, 62, 58, 63, 57, 64, 49, 50, 51, 52, 53, 54, 55, 56]
    gsindex = 0
    turn = 1
    for move in moves :
      if move.find('.') >= 0 :
        move = (move.split('.') [1])
      if move != "" :
        print(str(turn) + ":  " + str(boardstate))
        (boardstate, movestring) = generate_cleos_command(move, boardstate, turn)
        testfile.write(movestring)
        turn += 1
    pgnfile.close()
    testfile.close()
