# 37 marble solitaire game CLI-based solver

This project is a small script with a command line interface designed to take any desired starting state and start searching for solutions.
It will prune the search depending on custom parameters provided, such as a single target state to find.
- The next task is multithreading it.

I was playing the game of 37-marble solitaire many times, coming up with a couple ideas of strategy and general observations...
and as a programmer, I started thinking of ways to efficiently and thoroughly search possibilities 🙂.
I didn't want to spoil the solution by looking it up -- without feeling the reward of finding it myself (or at least my own code finding it).
Also, it was a perfect opportunity to revisit this language.

## Parameters (currently implemented):
- `[(-h | --help) [<flag>]]` : Prints general help for this program to the stdout. If a specific flag is given, prints detailed help for it instead.
- `[(-c | --count) <threshold>]` : Stop searching once a solution is found with `threshold` marbles or less. Default is `1` (one marble remaining).
  - Incompatible with `--target`.
- `[(-t | --target) <board-state>]` : Overrides default behavior, solving for a threshold count, with instead solving for a specific board state.
  - Useful if you were playing and can't remember how you got from one state to another. Or if you want to find out if you made a mistake along the way.
  - Incompatible with `--count`.
- `[-d]` : Parse board states in decimal (base 10) instead of binary.
  - Incompatible with `-x`.
- `[-x]` : Parse board states in hexidecimal (base 16) instead of binary.
  - Incompatible with `-d`.
  - If a leading `0x` is found at the beginning of the board state, when parsing in hex, it is skipped over before parsing.
 
Extra note: If a leading `0b` is found at the beginning of a board state (and parse format is binary), it is skipped over before parsing.
Binary is the default state

## Future features:
- **Multithreading!** This is definitely next on the list! I wanted to get CLI out of the way first, as it would be more complex up-front.
  Easier to not make a mistake if no extra threads are happening. Besides, how else are you going to tell the program where you want it to start?
- Silent mode; where the only output is the solution chain. I see this being useful for saving solutions to files or piping them into other commands if desired.
- An exhaustive search perhaps.
- Import states from files? Just a draft of an idea.

# 37 Marble Solitaire (or 37 peg hole solitaire)

This is a classic puzzle game, that uses 36 marbles (or pegs), and a board game of 37 holes arranged like so:  
🟫🟫⚫⚫⚫🟫🟫  
🟫⚫⚫⚫⚫⚫🟫  
⚫⚫⚫⚫⚫⚫⚫  
⚫⚫⚫⚫⚫⚫⚫  
⚫⚫⚫⚫⚫⚫⚫  
🟫⚫⚫⚫⚫⚫🟫  
🟫🟫⚫⚫⚫🟫🟫

🟫 = corner of the board; not a playable area  
⚫ = hole  
⚪ = marble

The game starts with all the board's holes filled in except one (hence 36 marbles). Any combination is legal; one such combination is:  
🟫🟫⚪⚪⚪🟫🟫  
🟫⚪⚪⚫⚪⚪🟫  
⚪⚪⚪⚪⚪⚪⚪  
⚪⚪⚪⚪⚪⚪⚪  
⚪⚪⚪⚪⚪⚪⚪  
🟫⚪⚪⚪⚪⚪🟫  
🟫🟫⚪⚪⚪🟫🟫

## Legal moves
Legal moves are to jump a marble over another marble into an empty hole, horizontally or vertically. (i.e., moves are **LEFT, RIGHT, UP, DOWN**.)
When a move is made, the marble that was jumped over is removed.

For example, a move upwards jumping over the center marble:  
⚪⚫⚪  
⚪⚪⚪  before  
⚪⚪⚪

⚪⚪⚪  
⚪⚫⚪  after  
⚪⚫⚪

## Winning
Simple: you win if you have one marble (or peg) remaining!

## Representing a board programmatically
You may notice a board can be treated as a 7*7 grid of 49 bits.  
🟫🟫⚪⚪⚪🟫🟫 0011100  
🟫⚪⚪⚫⚪⚪🟫 0110110  
⚪⚫⚪⚪⚫⚫⚪ 1011001  
⚪⚫⚫⚪⚫⚪⚪ 1001011  
⚪⚪⚫⚪⚪⚪⚪ 1101111  
🟫⚪⚪⚪⚪⚪🟫 0111110  
🟫🟫⚪⚪⚪🟫🟫 0011100

If space-saving is desired, the corners could also be omitted, leaving us with 37 bits.
However, computers work in multiples of 8 or powers of 2. Also, preserving the gridlike nature is convenient.
To simplify our movement calculations on a full grid, it is okay to waste a few bits on the unused corners.

Therefore, I store these 49 bits in a 64-bit number and perform bitwise operations on the number for calculating movements.
I find it simple, memory-efficient, and fast.

For representing the board state numerically: It's just a 49-bit number! Actually, less than 2^49.
This can be represented in hex, decimal, or binary.
For convenience, I have coded all three of those format options for providing a state into this solver program.
Provide it a flag [-d] or [-x] to indicate which format you are using other than binary.
