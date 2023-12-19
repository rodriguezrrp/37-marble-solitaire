#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

#define STATES_ARR_LEN 255

#define PRINT_STATS_EVERY_CHECKED
#define PRINT_STATS_EVERY_CHECKED_N 1000000

#define FULL_BOARD_BITS 0b0011100011111011111111111111111111101111100011100uLL
#define FULL_BOARD_CT 37

// board positions that moves can be made at;
//   as min and max index limits (BOTH inclusive)
//   (carves out the three unavailable positions in each corner)

const int BOARD_MOVABLE_IND_LIM_MIN[] = {3,2,1,0,1,2,3};
const int BOARD_MOVABLE_IND_LIM_MAX[] = {3,4,5,6,5,4,3};

// enum for directions

#define LEFT 1
#define RIGHT 2
#define UP 3
#define DOWN 4

const int DIRS[] = {LEFT, RIGHT, UP, DOWN};
const int DIRS_LEN = 4;

// directional offsets (ADDING to a row or column index, respectively)

#define LEFT_ROW_OFFSET 0
#define RIGHT_ROW_OFFSET 0
#define UP_ROW_OFFSET -1
#define DOWN_ROW_OFFSET 1

#define LEFT_COL_OFFSET -1
#define RIGHT_COL_OFFSET 1
#define UP_COL_OFFSET 0
#define DOWN_COL_OFFSET 0

/* TODO: PLANS for FUTURE FLAGS:
    [(-s | --silent) [(b | d | x)][p]] only output solution(s?) in a listed minimal representation, and no debug info.
                                    (can specify binary, dec, or hex output. default binary.)
                                    (can specify p alongside to pad to 64 bits for binary and hex output formats.
                                     defaults to padding 49 bits for binary format and no padding for decimal or hex.)
                                    minimal output would be like:
                                    the best ct, followed by each state. all in a list (i.e. separated by \n)
    [(-p | --processes) <num>] number of threads to create and run. 0 = automatic via CPU detection. default 1.
    [-e | --exhaustive] exhaustive search for solutions that match the solution condition.
    
    allow multiple starting board states and/or multiple target board states!
*/

// FLAGS_HELP array is structured like:
// {
//    {flags, usage, short_desc, extra_desc},
//    ...
// }

struct _flags_help {
    char *flags;
    char *usage;
    char *short_desc;
    char *extra_desc;
};

const struct _flags_help FLAGS_HELP[] = {
    {"-h | --help", "[(-h | --help) [<flag>]]",
        "Display command-line usage of this program.",
        "If another flag is optionally specified following this flag, will display help only regarding that flag."},
    {"-t | --target", "[(-t | --target) <board-state>]",
        "Specify a board state as a solution condition instead of a threshold.",
        "This will override the normal behavior to reach a threshold of marbles remaining in any arrangement."
        " If this flag is specified multiple times, only the first will be used."
        " Program's default solution condition is a threshold of one (1) marble remaining."
        " This flag cannot be used with -c | --count."
        " See --help or --help board-state for how to input a board state."},
    {"-c | --count", "[(-c | --count) <threshold>]",
        "Specify a custom number of marbles remaining as a solution condition.",
        "The threshold number must be an integer greater than 1."
        " If this flag is specified multiple times, only the first will be used."
        " Program's default solution condition is a threshold of one (1) marble remaining."
        " This flag cannot be used with -t | --target."},
    {"-x", "[-x]",
        "Parse board-states in hexidecimal instead of binary.",
        "This flag cannot be used with -d."},
    {"-d", "[-d]",
        "Parse board-states in decimal instead of binary.",
        "This flag cannot be used with -x."},
    NULL
};
const int FLAGS_HELP_LEN = 5;

// FLAGS_HELP_MAP[] contains mappings between a string (which is a flag),
// and a corresponding struct _flags_help that came from FLAGS_HELP[].
// THE INDEXES MATTER! i.e. the order of elements in FLAGS_HELP_MAP.

struct _flags_help_mapping {
    char *flag;
    struct _flags_help help;
};

const struct _flags_help_mapping FLAGS_HELP_MAP[] = {
    {"-h", FLAGS_HELP[0]},
    {"--help", FLAGS_HELP[0]},
    {"-t", FLAGS_HELP[1]},
    {"--target", FLAGS_HELP[1]},
    {"-c", FLAGS_HELP[2]},
    {"--count", FLAGS_HELP[2]},
    {"-x", FLAGS_HELP[3]},
    {"-d", FLAGS_HELP[4]},
    NULL
};
const int FLAGS_HELP_MAP_LEN = 8;

const char BOARD_STATE_DESC[] = "<board-state>\n"
                    "A board state is represented with 49 binary bits, representing the 7*7 solitaire grid."
                        " An occupied space is represented with a 1, and an empty space with a 0."
                        " The three spaces in each corner are always expected to be 0."
                        " As a board state is therefore also an integer,"
                        " it can be represented in other bases (hex or decimal).\n"
                    "The order of bits is in reading order; left to right, top to bottom. Each row is 7 bits long, and there are 7 rows.\n"
                    "Specify a board state in one of the following representations:\n"
                        "\t- Provide it as a binary number, all 49 bits in order.\n"
                        "\t- Using the -d flag: provide it as a decimal number, in base 10.\n"
                        "\t- Using the -x flag: provide it as a hexidecimal number, in base 16.\n"
                    "Example board state: 0011100011011011101111001111111111101111100011100\n"
                    "Represents the board:\n\t0011100\n\t0110110\n\t1110111\n\t1001111\n\t1111111\n\t0111110\n\t0011100\n"
                    "\n";

typedef struct {
    unsigned long long bits;    // 7*7 layout of the board state in bits
                                //      (Row first. 1 == marble, 0 == empty.)
    int ct;                     // number of marbles
    int pindex;                 // index of parent state   (-1 if none)
    bool visited;               // a marker for removal from the list next time it is visited
                                //      (I.e., keep parent around until done with children.
                                //       Note the search is Depth-First)
} state_t;


state_t *sarr = NULL;
// int curindex = 0;
int sarrlen = 0;

state_t *solarr = NULL;
int solarrlen = 0;



int is_hex(char c)
{
    static int s_hex_digit[1 << CHAR_BIT] = {
        ['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1, ['4'] = 1,
        ['5'] = 1, ['6'] = 1, ['7'] = 1, ['8'] = 1, ['9'] = 1,
        ['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1, ['F'] = 1,
        ['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1
    };
    return s_hex_digit[(unsigned char)c];
}

int count_bits(unsigned long long bits)
{
    int bitct = 0;
    int i;
    for(i = sizeof bits * 8 - 1; i >= 0; i--)
    {
        if(bits & (1uLL << i))
            bitct++;
    }
    return bitct;
}


void printbits_all(unsigned long long bits)
{
    int i;
    for(i = sizeof bits * 8 - 1; i >= 0; i--)
    {
        putchar(bits & (1uLL << i) ? '1' : '0');
    }
}


void printbits_square(unsigned long long bits)
{
    int r, c;
    for(r = 6; r >= 0; r--)
    {
        printf("\n  ");
        for(c = 6; c >= 0; c--)
        {
            putchar(bits & (1uLL << (r*7+c)) ? '1' : '0');
        }
    }
}


int is_marble(const unsigned long long bits, const int row, const int col)
{
    // printf(" %d->%u\n",(49-(row * 7 + col)-1), (bits & (1uLL << (49-(row * 7 + col)-1)) ? 1 : 0));
    return (bits & (1uLL << (49-(row * 7 + col)-1)) ? 1 : 0);
}
static inline unsigned long long clear_marble(const unsigned long long bits, const int row, const int col)
{
    return (bits & ~(1uLL << (49-(row * 7 + col)-1)));
}
static inline unsigned long long set_marble(const unsigned long long bits, const int row, const int col)
{
    return (bits | (1uLL << (49-(row * 7 + col)-1)));
}
static inline unsigned long long toggle_marble(const unsigned long long bits, const int row, const int col)
{
    return (bits ^ (1uLL << (49-(row * 7 + col)-1)));
}


int is_legal_move(unsigned long long bits, int row, int col, int dir)
{
    // dir must be legal
    if (dir != LEFT && dir != RIGHT && dir != UP && dir != DOWN)
        return 0;
    
    int roff, coff; // row and column offsets
    if(dir == LEFT)  { roff = LEFT_ROW_OFFSET;  coff = LEFT_COL_OFFSET; }
    if(dir == RIGHT) { roff = RIGHT_ROW_OFFSET; coff = RIGHT_COL_OFFSET; }
    if(dir == UP)    { roff = UP_ROW_OFFSET;    coff = UP_COL_OFFSET; }
    if(dir == DOWN)  { roff = DOWN_ROW_OFFSET;  coff = DOWN_COL_OFFSET; }

    // check boundaries
    // if (row < 0 || row > 6 || col < 0 || col > 6)
    //     return 0;
    // if ((row == 0 && roff < 0) || (row == 6 && roff > 0) || (col == 0 && coff < 0) || (col == 6 && coff > 0))
    //     return 0;
    if (row < BOARD_MOVABLE_IND_LIM_MIN[col] || row > BOARD_MOVABLE_IND_LIM_MAX[col] || col < BOARD_MOVABLE_IND_LIM_MIN[row] || col > BOARD_MOVABLE_IND_LIM_MAX[row])
        return 0uLL;
    // cannot make a perpendicular-to-boundary move on the boundary
    if (((row == 6 || row == 0) && roff != 0) || ((col == 6 || col == 0) && coff != 0))
        return 0uLL;

    // there must be a marble to jump
    if (!is_marble(bits, row, col))
        return 0;
    
    // there must be a marble in the opposite direction and no marble in the target direction
    // i.e. to jump "from" behind, "to" target
    return (
        (is_marble(bits, row - roff, col - coff) && !is_marble(bits, row + roff, col + coff))
        ? 1 : 0
    );
}


unsigned long long attempt_move(unsigned long long bits, int row, int col, int dir)
{
    // dir must be legal
    if (dir != LEFT && dir != RIGHT && dir != UP && dir != DOWN)
        return 0uLL;
    
    int roff, coff; // row and column offsets
    if(dir == LEFT)  { roff = LEFT_ROW_OFFSET;  coff = LEFT_COL_OFFSET; }
    if(dir == RIGHT) { roff = RIGHT_ROW_OFFSET; coff = RIGHT_COL_OFFSET; }
    if(dir == UP)    { roff = UP_ROW_OFFSET;    coff = UP_COL_OFFSET; }
    if(dir == DOWN)  { roff = DOWN_ROW_OFFSET;  coff = DOWN_COL_OFFSET; }

    // check boundaries
    // if (row < 0 || row > 6 || col < 0 || col > 6)
    //     return 0uLL;
    // if ((row == 0 && roff < 0) || (row == 6 && roff > 0) || (col == 0 && coff < 0) || (col == 6 && coff > 0))
    //     return 0uLL;
    if (row < BOARD_MOVABLE_IND_LIM_MIN[col] || row > BOARD_MOVABLE_IND_LIM_MAX[col] || col < BOARD_MOVABLE_IND_LIM_MIN[row] || col > BOARD_MOVABLE_IND_LIM_MAX[row])
        return 0uLL;
    // cannot make a perpendicular-to-boundary move on the boundary
    if (((row == 6 || row == 0) && roff != 0) || ((col == 6 || col == 0) && coff != 0))
        return 0uLL;

    // there must be a marble to jump
    if (!is_marble(bits, row, col))
        return 0uLL;
    
    // there must be a marble in the opposite direction and no marble in the target direction
    // i.e. to jump "from" behind, "to" target
    return (
        (is_marble(bits, row - roff, col - coff) && !is_marble(bits, row + roff, col + coff))
        ? (
            // set target, clear behind, and clear middle
            set_marble(clear_marble(clear_marble(bits, row, col), row - roff, col - coff), row + roff, col + coff)
        )
        : 0uLL
    );
}


void printindstate(int index)
{
    state_t state = sarr[index];
    unsigned long long bits = state.bits;
    printf("pindex:\t%d\tvisited:\t%d\n", state.pindex, state.visited ? 1 : 0);
    printf("ct:    \t%d\n", state.ct);
    printf("board base16:\t0x %12llx\nboard bits:   ", bits);
    printbits_square(bits);
    printf("\n");
}

void printsolstate(int index)
{
    state_t state = solarr[index];
    unsigned long long bits = state.bits;
    printf("pindex:\t%d\tvisited:\t%d\n", state.pindex, state.visited ? 1 : 0);
    printf("ct:    \t%d\n", state.ct);
    printf("board base16:\t0x %012llx\nboard bits:   ", bits);
    printbits_square(bits);
    printf("\n");
}

void printstate(const state_t state)
{
    unsigned long long bits = state.bits;
    printf("pindex:\t%d\tvisited:\t%d\n", state.pindex, state.visited ? 1 : 0);
    printf("ct:    \t%d\n", state.ct);
    printf("board base16:\t0x %12llx\nboard bits:   ", bits);
    printbits_square(bits);
    printf("\n");
}


void print_sarr()
{
    printf("State array (%d states):\n", sarrlen);
    int i;
    for(i = 0; i < sarrlen; i++) { printf("[%d]\n",i); printindstate(i); }
}

void print_state_arr(const state_t *arr, const int arrlen)
{
    printf("State array (%d states):\n", arrlen);
    int i;
    for(i = 0; i < arrlen; i++) { printf("[%d]\n",i); printindstate(i); }
}

void print_solarr()
{
    printf("Solution state chain (%d states):\n", solarrlen);
    int i;
    for(i = 0; i < solarrlen; i++) { printf("[%d]\n",i); printsolstate(i); }
}


int add_all_moves_latest()
{
    // printf("sarrlen == %d\n", sarrlen);
    int curindex = sarrlen - 1; // the end of the array
    state_t *curstate = &sarr[curindex]; // by reference, for assignment to ->visited
    // handle removal
    if (curstate->visited)
    {
        // if marked as visited, that means it has been visited already by this Depth First search.
        sarrlen--; // "remove" from array
        return -1;
    }
    // if not marked as visited, then "visit" it and proceed to add its children to the array.
    curstate->visited = true;
    // sarr[curindex] = curstate; // if not using reference (& and ->), this is needed to save the assignment of .visited

    unsigned long long curbits, newbits;
    curbits = curstate->bits;

    // loop through all move positions, and test for moving in each direction
    int r, c, d;
    int added = 0;
    for (d = 0; d < DIRS_LEN; d++)
    {
        int dir = DIRS[d];
        // all positions
        for (r = 0; r < 7; r++)
        {
            for (c = 0; c < 7; c++)
            {
                // if move cannot be performed, result is 0
                newbits = attempt_move(curbits, r, c, dir);
                if (newbits)
                {
                    // if move succeeded, "append" new state onto the end of the array
                    sarr[sarrlen] = (state_t){
                        .bits = newbits,
                        .ct = curstate->ct - 1,
                        .pindex = curindex,
                        .visited = false
                    };
                    sarrlen++;
                    added++;
                }
            }
        }
    }
    return added;
}


void save_parent_chain(int index)
{
    int nextindex = sarr[index].pindex;
    int chainlen = 1;
    while (nextindex >= 0)
    {
        nextindex = sarr[nextindex].pindex;
        chainlen++;
    }
    
    // malloc a state array of chainlen size
    free(solarr); // according to standard, free(NULL) is no problem.
    solarr = (state_t*) malloc(sizeof *sarr * chainlen);
    
    // fill the solution state array
    nextindex = index; // start over with the index, using the initial index this time
    state_t curstate;
    int solind;
    for (solind = chainlen - 1; solind >= 0; solind--)
    {
        curstate = sarr[nextindex];
        nextindex = curstate.pindex;
        solarr[solind] = curstate;
    }
    solarrlen = chainlen;
}


unsigned long long _board_size_cap = FULL_BOARD_BITS;// (1uLL << 49);
unsigned long long parse_board_state(char *arg, bool parse_dec, bool parse_hex)
{
    // will not accept both parsing options as true
    assert(!(parse_dec && parse_hex));
    // parse the char *arg into a number, either binary, dec, or hex
    unsigned long long result;
    int arglen = strlen(arg);
    // if hex and it starts with '0x', or binary and it starts with '0b', skip past the 0x or 0b prefix.
    if (arglen >= 2 && arg[0]=='0'
        && ((parse_hex && arg[1]=='x') || (!parse_dec && !parse_hex && arg[1]=='b')))
    {
        // skip forward 2 chars
        arg = arg+2;
        arglen -= 2;
    }
    // check own stricter formatting
    int i;
    for (i = 0; i < arglen; i++)
    {
        if (parse_dec)
        {
            // decimal
            if(!(arg[i] >= '0' && arg[i] <= '9'))
                break;
        }
        else if (parse_hex)
        {
            // hex
            if(!is_hex(arg[i]))
                break;
        }
        else
        {
            // binary
            if(arg[i] != '0' && arg[i] != '1')
                break;
        }
    }

    // parse it into a number (if stricter formatting passed)
    if (i >= arglen) // stricter formatting passed; because for-loop completed.
    {
        result = strtoull(arg, NULL, parse_dec ? 10 : parse_hex ? 16 : 2);
        if(errno == ERANGE)
        {
            perror("parse_board_state() failed");
            errno = 0;
            result = 0;
        }
    }
    else // stricter formatting did not pass
        result = 0;
    
    // if result == 0, presume boardstate was invalid or an error occured.
    if (result == 0) // failed to parse successfully
    {
        fprintf(stderr, "Invalid board state representation \"%s\"; when parsing in %s.\n",
                arg, parse_dec ? "decimal [-d]" : parse_hex ? "hex [-x]" : "binary");
        exit(1);
    }
    // Otherwise, check the corners are empty and it has no extra bits anywhere
    // using FULL_BOARD_BITS as a mask
    if (result != (result & FULL_BOARD_BITS)) {
        fprintf(stderr, "Invalid board state representation \"%s\"; when parsing in %s.\n"
                "\tExcessive bits found in corners or to the left.\n",
                arg, parse_dec ? "decimal [-d]" : parse_hex ? "hex [-x]" : "binary");
        exit(1);
    }
    return result;
}

int parse_int(char *arg)
{
    bool problem = false;
    int result;
    // check own stricter formatting
    int arglen = strlen(arg);
    int i;
    for (i = 0; i < arglen; i++)
    {
        // decimal
        if(!(arg[i] >= '0' && arg[i] <= '9') && arg[i] != '-' && arg[i] != '+')
            break;
    }
    // parse it into a number (if stricter formatting passed)
    if (i >= arglen) // stricter formatting passed; because for-loop completed.
    {
        result = (int) strtol(arg, NULL, 10);
        if(errno == ERANGE)
        {
            perror("parse_int() failed");
            errno = 0;
            problem = true;
        }
    }
    else // stricter formatting did not pass
        problem = true;
    
    // if result == 0, presume int was invalid or an error occured.
    if (problem) // failed to parse successfully
    {
        fprintf(stderr, "Invalid number \"%s\".\n",
                arg);
        exit(1);
    }
    return result;
}



int main(int argc, char **argv)
{
    // printf("Hello world\n");
    
    // ================================
    // perform argument parsing
    unsigned long long arg_start_bits = 0uLL, arg_target_bits = 0uLL;
    bool arg_parse_boardstate_hex = false, arg_parse_boardstate_decimal = false;
    int arg_target_count = 0;
    {
        bool flags_encountered = false;
        bool target_bits_encountered = false;
        int i;
        char *arg;
        int argstrlen;
        for (i = 1; i < argc; i++)
        {
            arg = argv[i];
            argstrlen = strlen(arg);
            // printf("debug: argument received: \"%s\"\n", arg);
            if (argstrlen <= 0)
                continue;
            // printf("%d %d", strlen("-h"), strncmp(arg,"-h",strlen("-h")));
            // exit(0);
            if (strcmp(arg,"-h") == 0 || strcmp(arg,"--help") == 0)
            {
                // print help!

                i++;
                // search if there is a specified argument that help is desired about.
                if(i < argc)
                {
                    arg = argv[i]; // arg is now the flag or argument to provide specific help for.
                    int j;
                    struct _flags_help_mapping help_mapping;
                    struct _flags_help flag_help;
                    if (strcmp(arg, "board-state") == 0 || strcmp(arg, "<board-state>") == 0)
                    {
                        fputs(BOARD_STATE_DESC, stdout);
                        exit(0);
                    }
                    for (j = 0; j < FLAGS_HELP_MAP_LEN; j++)
                    {
                        help_mapping = FLAGS_HELP_MAP[j];
                        if (strcmp(arg, help_mapping.flag) == 0)
                        {
                            flag_help = help_mapping.help;
                            fprintf(stdout, "%s\n\tusage: %s\n%s\n%s\n",
                                    flag_help.flags, flag_help.usage, flag_help.short_desc, flag_help.extra_desc);
                            exit(0);
                        }
                    }
                    if(j >= FLAGS_HELP_MAP_LEN)
                    {
                        // null pointer; was unable to find it
                        fprintf(stdout, "Unknown flag or argument \"%s\". See --help for a list of arguments.\n", arg);
                    }
                    exit(0);
                }

                // otherwise, print general help and usage.
                fputs("usage: solver [(-h | --help) [<flag>]] [-x] [-d] <board-state> [(-t | --target) <board-state>]\n"
                    "\n", stdout);
                fputs(BOARD_STATE_DESC, stdout);

                // print all arguments' help
                // (ONLY THE SHORTENED DESCRIPTIONS. Use specific help for the extended descriptions.)
                int j;
                struct _flags_help flag_help;
                for (j = 0; j < FLAGS_HELP_LEN; j++)
                {
                    flag_help = FLAGS_HELP[j];
                    fprintf(stdout, "%s\n\tusage: %s\n\t%s\n", flag_help.flags, flag_help.usage, flag_help.short_desc);
                }
                exit(0);
            }
            else if (strcmp(arg,"-t") == 0 || strcmp(arg,"--target") == 0)
            {
                flags_encountered = true;
                target_bits_encountered = true;
                i++;
                if (i < argc && !arg_target_bits)
                {
                    arg = argv[i];
                    // parse board state
                    arg_target_bits = parse_board_state(arg, arg_parse_boardstate_decimal, arg_parse_boardstate_hex);
                }
                if (i >= argc)
                {
                    fputs("Target flag (-t | --target) must be followed by a board state. See --help for board state formatting."
                            " Usage: [-t <board-state>]\n", stderr);
                    exit(1);
                }
                if(arg_target_count)
                {
                    // intentionally put duplicate flag check after parsing, to provide parsing problem feedback first.
                    fputs("Cannot specify both a target board state (-t | --target) and a marble count threshold (-c | --count)!\n", stderr);
                    exit(1);
                }
            }
            else if (strcmp(arg,"-c") == 0 || strcmp(arg,"--count") == 0)
            {
                flags_encountered = true;
                i++;
                if (i < argc && !arg_target_count)
                {
                    arg = argv[i];
                    // parse number for count
                    arg_target_count = parse_int(arg);
                    if (arg_target_count < 1)
                    {
                        fputs("Target count threshold for marbles remaining must be greater than or equal to 1.\n", stderr);
                        exit(1);
                    }
                }
                if (i >= argc)
                {
                    fputs("Count threshold (-c | --count) must be followed by a number 1 or higher. See --help for board state formatting."
                            " Usage: [-c <threshold>]\n", stderr);
                    exit(1);
                }
                if(arg_target_bits)
                {
                    // intentionally put duplicate flag check after parsing, to provide parsing problem feedback first.
                    fputs("Cannot specify both a target board state (-t | --target) and a marble count threshold (-c | --count)!\n", stderr);
                    exit(1);
                }
            }
            else if (argstrlen >= 2 && arg[0] == '-' && arg[1] != '-')
            {
                // CHAINABLE abbreviated args (ex. "-a -b -c" would be chained as "-abc")
                flags_encountered = true;
                int j;
                for (j = 1; j < argstrlen; j++)
                {
                    switch (arg[j]) {
                        case 'd':
                            // decimal flag
                            if(arg_parse_boardstate_hex)
                            {
                                fputs("Cannot specify both board state formats decimal [-d] and hex [-x] together!\n", stderr);
                                exit(1);
                            }
                            arg_parse_boardstate_decimal = true;
                            break;
                        case 'x':
                            // hexidecimal flag
                            if(arg_parse_boardstate_decimal)
                            {
                                fputs("Cannot specify both board state formats decimal [-d] and hex [-x] together!\n", stderr);
                                exit(1);
                            }
                            arg_parse_boardstate_hex = true;
                            break;
                        default:
                            // unknown flag
                            fprintf(stderr, "Unexpected flag encountered: \"%c\", in argument \"%s\"\n", arg[j], arg);
                            exit(1);
                    }
                }
            }
            else
            {
                // arg was not a flag, so expecting just board states.
                // make sure we haven't seen target state yet (TODO: this limitation can be changed later)
                if (target_bits_encountered)
                {
                    // starting bits are expected to be specified before any target bits!
                    fprintf(stderr, "Unexpected value encountered in arguments: \"%s\".\n", arg);
                    exit(1);
                }
                arg_start_bits = parse_board_state(arg, arg_parse_boardstate_decimal, arg_parse_boardstate_hex);
            }
        }
    }


    if(arg_start_bits)
    {
        fprintf(stdout, "CLI argument: Starting state: 0x %012llx (base 16)", arg_start_bits);
        // fprintf(stdout, " (count_bits: %d)", count_bits(arg_start_bits));
        printbits_square(arg_start_bits);
        fputs("\n", stdout);
    }
    if(arg_target_bits)
    {
        fprintf(stdout, "CLI argument: Target state: 0x %012llx (base 16)", arg_target_bits);
        // fprintf(stdout, " (count_bits: %d)", count_bits(arg_target_bits));
        printbits_square(arg_target_bits);
        fputs("\n", stdout);
    }
    if(arg_target_count)
    {
        fprintf(stdout, "CLI argument: Marble threshold: %d\n", arg_target_count);
    }

    // return 0;

    // initialize
    // curindex = 0;
    sarr = (state_t*) malloc(sizeof *sarr * STATES_ARR_LEN);

    /*
    // methods testing
    {
        // initialize first state in array
        sarr[0] = (state_t){
            .bits = 0b0011100011111011111111111111111011101101100011100uLL,
            .ct = 36,
            .pindex = -1,
            .visited = false
        };
        sarrlen = 1;
        // printf("First state in list:\n");
        // printindstate(0);
        print_sarr();
        // now grab as usual for testing the methods
        unsigned long long b = sarr[0].bits;
        // testing is_marble
        printf("%d", is_marble(b, 0,3));
        printf("%d", is_marble(b, 1,3));
        printf("%d", is_marble(b, 2,3));
        printf("%d", is_marble(b, 2+DOWN_ROW_OFFSET,3));
        printf("%d", is_marble(b, 4,3));
        printf("%d", is_marble(b, 5,3));
        printf("%d", is_marble(b, 6,3));
        printf(" (expected 1111001)\n");
        // testing is_legal_move
        printf("%d (expect 0)\n", is_legal_move(b, 4,3, LEFT));
        printf("%d (expect 0)\n", is_legal_move(b, 4,3, RIGHT));
        printf("%d (expect 1)\n", is_legal_move(b, 3,3, DOWN));
        printf("%d (expect 0)\n", is_legal_move(b, 3,3, UP));
        printf("%d (expect 0)\n", is_legal_move(b, 3,3, RIGHT));
        printf("%d (expect 0)\n", is_legal_move(b, 5,2, LEFT));
        printf("%d (expect 1)\n", is_legal_move(b, 5,2, RIGHT));
        printf("%d (expect 0)\n", is_legal_move(b, 6,3, UP)); // can't move up from the bottom of the board
        printf("%d (expect 0)\n", is_legal_move(b, 6,3, DOWN)); // can't move down past the bottom of the board either
        // testing attempt_move
        printf("(expect not 0): %llu", attempt_move(b, 3,3, DOWN));
        printbits_square(attempt_move(b, 3,3, DOWN));
        printf("\n");
        printf("(expect 0): %llu\n", attempt_move(b, 3,3, UP));
        printf("(expect 0): %llu\n", attempt_move(b, 5,2, LEFT));
        printf("(expect not 0): %llu", attempt_move(b, 5,2, RIGHT));
        printbits_square(attempt_move(b, 5,2, RIGHT));
        printf("\n");
        printf("(expect not 0): %llu", attempt_move(b, 4,4, LEFT));
        printbits_square(attempt_move(b, 4,4, LEFT));
        printf("\n");
        printf("(expect 0): %llu\n", attempt_move(b, 1,1, LEFT)); // can't move into the three 0s in each corner
        
        // testing first iteration of moves
        add_all_moves_latest();
        print_sarr();
        // testing a simulated exhaustion of children
        sarrlen = 1; // "removing" the children that were added
        add_all_moves_latest(); // should now have 0 states.
        print_sarr();
    }
    */


    // note: 0011000011011001000100000000010100000011000001100 provides a best of 2, and 5218605 states checked.
    // note: 0011000011011001000100000000010100000011000011100 provides a best of 3, and 8466685 states checked.

    // note: 0011000011011001000100000000010100000111000011100 provides a best of 2, and 281654095 states checked.

    // sarr[0] = (state_t){
    //     // A fabricated testing state, that has four moves remaining.
    //     .bits = 0b0000000000000001000000011000000110000000000000000uLL,
    //     .ct = 5,
    //     .pindex = -1,
    //     .visited = false
    // };
    // sarr[0] = (state_t){
    //     // A fabricated testing state, whose best is 2 and has 2 moves remaining.
    //     .bits = 0b0010100001010000000000000000000000000000000000000uLL,
    //     .ct = 4,
    //     .pindex = -1,
    //     .visited = false
    // };
    // sarr[0] = (state_t){
    //     // A fabricated testing state, that has 14 moves remaining.
    //     .bits = 0b0001000010100000111001001100111100000011000000000uLL,
    //     .ct = 15,
    //     .pindex = -1,
    //     .visited = false
    // };
    // sarr[0] = (state_t){
    //     // A fabricated testing state, that has 14 moves remaining.
    //     .bits = 0b0001000010100000111001001100111100000011000000000uLL,
    //     .ct = 15,
    //     .pindex = -1,
    //     .visited = false
    // };
    // sarr[0] = (state_t){
    //     // A fabricated testing state, that has 14 moves remaining.
    //     .bits = 0b0000000000000011011101101000000011000111100000100uLL,
    //     .ct = 15,
    //     .pindex = -1,
    //     .visited = false
    // };
    // sarr[0] = (state_t){
    //     // A fabricated testing state, that has 24 moves remaining.
    //     .bits = 0b0011000011011001110111111111011100000011000011000uLL,
    //     .ct = 25,
    //     .pindex = -1,
    //     .visited = false
    // };
    // sarr[0] = (state_t){
    //     // One possible real starting state that _should_ lead to a win. Top left is removed.
    //     .bits = 0b0001100011111011111111111111111111101111100011100uLL,
    //     .ct = 36,
    //     .pindex = -1,
    //     .visited = false
    // };
    if(!arg_start_bits)
    {
        fputs("Must specify a board state to start solving from! See --help for command line usage\n", stderr);
        exit(1);
    }
    sarr[0] = (state_t){
        .bits = arg_start_bits,
        .ct = count_bits(arg_start_bits),
        .pindex = -1,
        .visited = false
    };
    sarrlen = 1;
    // print_sarr();
    // return 0;


    // ===================================
    // do real solving now

    const int targetct = arg_target_bits
                         ? count_bits(arg_target_bits)
                         : arg_target_count
                         ? arg_target_count
                         : 1;

    int bestct = sarr[sarrlen-1].ct;//FULL_BOARD_CT;
    int largestsarrlen = 0;
    int curindex;
    int newgen;
    unsigned long long checked = 0;
    unsigned long long generated = 0;
    // while (sarrlen > 0 && bestct > targetct)
    while (sarrlen > 0 && bestct > targetct)
    {
        if (sarrlen > largestsarrlen) largestsarrlen = sarrlen;
        // generate the next moves. returns how many moves were generated and added to the list.
        newgen = add_all_moves_latest();
        if (newgen > 0)
            generated += newgen;
        if (newgen == 0 && checked > 0)
            // if newgen == 0, then it was a leaf node.
            // If no states have been checked yet then we should still check this leaf node, as it must NOT have been generated.
            // Otherwise leaves can be ignored and removed, because they would have been checked when first generated.
            continue;
        // Note: even if newgen < 0, meaning a previsited node was removed,
        // that means a node was uncovered on the list that can be checked.
        // the uncovered node could still be visited, hence the curstate.visited check below.

        // inspect, or check, the latest board state.
        curindex = sarrlen - 1;
        state_t curstate = sarr[curindex];
        if (curstate.visited)
        {
            // no need to check a visited node. It would have been checked already.
            // (assuming the state array did not start polluted with a state with .visited==true)
            continue;
        }
        checked++; // the node being checked should be an entirely unseen node at this point.
        
        // check if the newly generated board state is a specific state we are looking for.
        if (arg_target_bits)
        {
            // Note: this is separated from bestct logic to prevent the bestct > targetct condition stopping the while-loop.
            
            if (arg_target_bits == curstate.bits)
            {
                printf("Found a target state with %d marbles remaining (state base16: 0x %012llx).\n", curstate.ct, curstate.bits);
                save_parent_chain(curindex);
                bestct = targetct;
            }
            // PRUNE depth first search by easy marble count guarantee
            else if (curstate.ct <= targetct)
            {
                // curstate is not the solution,
                // and also is easily guaranteed to never produce the solution later.
                //   (by its marble count not being higher)
                // Therefore, mark it for "removal".
                // This prunes the searching needed to be performed.
                curstate.visited = true;
                sarr[curindex] = curstate; // put changes back into state array.
            }
        }
        // check if the newly generated board state is better than our best so far.
        // (only if we are not looking for a specific board state)
        else
        if (curstate.ct < bestct)
        {
            bestct = curstate.ct;
            printf("Found new best state with %d marbles remaining (base16: 0x %012llx).\n", bestct, curstate.bits);
            // save chain of solutions
            save_parent_chain(curindex);
        }

        #ifdef PRINT_STATS_EVERY_CHECKED
        if(checked % PRINT_STATS_EVERY_CHECKED_N == 0)
            printf("...info: Checked %llu states, Generated %llu states; best %d, max sarrlen %d of %d...\n",
                checked, generated, bestct, largestsarrlen, STATES_ARR_LEN);
        #endif
    }

    if (sarrlen <= 0)
    {
        if(arg_target_bits)
            printf("Exhausted all child states; could not find the custom target state.\n");
        else
            printf("Exhausted all child states; could not find a solution with %d marble%s.\n", targetct, targetct==1?"":"s or less");
    }

    if (arg_target_bits)
    {
        if (solarr && solarrlen > 0 && arg_target_bits == solarr[solarrlen-1].bits) {
            // arg_target_bits was specified _and_ was found
            printf("Found the the custom target state 0x %012llx (base16) starting from 0x %012llx (base16)!\n",
                   arg_target_bits, arg_start_bits);
        }
        else
        {
            // arg_target_bits was specified but not found
            printf("Could not find the custom target state 0x %012llx (base16) starting from 0x %012llx (base16).\n",
                   arg_target_bits, arg_start_bits);
        }

    }
    else if (bestct <= targetct)
    {
        // arg_target_bits was not specified. Threshold solved instead.
        printf("Found a solution with only %d marble%s remaining!\n", bestct, bestct==1 ? "" : "s");
        if(arg_target_count) printf("  (Note: custom threshold of %d was specified.)\n", arg_target_count);
    }
    
    if(!(arg_start_bits && (solarr && solarrlen > 0 && arg_target_bits == solarr[solarrlen-1].bits)))
        // if either:
        //   solving by threshold (no specific target state was specified), or
        //   the specific target state was found
        print_solarr();

    fprintf(stdout, "Info: Checked %llu states. Generated %llu states. Found a best with %d remaining", checked, generated, bestct);
    if(arg_target_count) fprintf(stdout, " (custom threshold of %d)", arg_target_count);
    if(arg_target_bits) fprintf(stdout, " (custom target state of 0x %012llx in base16)", arg_target_bits);
    fputs(".\n", stdout);

    //
    // printf("\nExecution ended normally.\n");
    return 0;
}