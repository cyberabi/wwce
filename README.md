# wwce
World's Worst Chess Engine

I challenged myself to write a rudimenatary chess engine in four hours. That initial challenge was a qualified success.

Here's what's missing to make it really Chess:
1) Pawn promotion
2) En passant
3) Castling
4) Draw detection
5) Checkmate detection

Here's what's very basic:
1) No player input (plays only with itself)
2) No understanding of openings
3) No understanding of end game
4) No look-ahead
5) Trivial position evaluation (avoid check and min-max the point value of pieces remaining on the board)
6) Selects randomly among moves with equal position evaluations
7) No clock
8) Non-standard move notation
9) ASCII board render (upper case is White, lower case is Black, a "." indicates an empty black square.)

Known bugs:
1) Inverted numbering of board ranks

I gave myself this challenge after answering two chess-related questions on Quora. The first was whether an employer should be impressed that a candidate wrote a chess engine. The second was what's the next-worst chess engine after a "random mover."

MIT License. Have fun!
Chris Burke
chris@cyberabi.com
01/25/2023
