# wwce
World's Worst Chess Engine

I challenged myself to write a rudimenatary chess engine in four hours. That initial challenge was a qualified success, and you can find it on the main branch. Here on the bugfixes branch I'm making small improvements and bug fixes based on that initial four hour work. The resulting executable is under 1000 lines of code including comments, and under 64K compiled (on a Macbook).

Currently not implemented:
1) Human player input (plays only with itself)
2) Pawn promotion to Bishop or Rook (Knight and Queen implemented)

Changes that would make it a better engine:
1) Understanding of openings
2) Understanding of end game
3) Better position evaluation (currently avoids check and stalemate, seeks checkmate, and min-maxes the point value of pieces remaining on the board)
4) Saving the transcript of a game
5) Loading a partial game transcript (e.g. to solve simple chess problems)

Known bugs:
1) Missing algebraic notation for checkmate and stalemate

Don't code like this. MIT License. Have fun!

Chris Burke
chris@cyberabi.com
01/25/2023
