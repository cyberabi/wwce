# wwce
World's Worst Chess Engine

I challenged myself to write a rudimenatary chess engine in four hours. That initial challenge was a qualified success, and you can find it on the main branch.

Here on the bugfixes branch I'm making small improvements and bug fixes based on that initial four hour work. The resulting executable is under 1000 lines of code including comments, and under 64K compiled (on a Macbook). Despite its small size wwce can load a starting position from a FEN string (-fen <string> on the command line), to solve chess puzzles; it has a few puzzles built in (-puzzle n on the command line); and, it implements a trivial opening book of one move each for white and black.

Currently not implemented:
1) Human player input (plays only with itself)
2) Pawn promotion to Bishop or Rook (Knight and Queen implemented)

Changes that would make it a better engine:
1) Deeper understanding of openings
2) Understanding of end game
3) Better position evaluation (currently avoids check and stalemate, seeks checkmate, values center control, and min-maxes the point value of pieces remaining on the board)
4) Saving the transcript of a game

Known bugs:
1) Missing algebraic notation for stalemate

Don't code like this. MIT License. Have fun!

Chris Burke
chris@cyberabi.com
11/28/2025
