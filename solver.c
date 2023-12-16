#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define STATES_LEN 255

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


void printstate(int index)
{
    state_t state = sarr[index];
    unsigned long long bits = state.bits;
    printf("pindex:\t%d\tvisited:\t%d\n", state.pindex, state.visited ? 1 : 0);
    // if(state.visited) { printf("visited yup\n"); }
    printf("ct:    \t%d\n", state.ct);
    printf("board base10:\t%llu\nboard bits:   ", bits);
    printbits_square(bits);
    printf("\n");
}

void printsolstate(int index)
{
    state_t state = solarr[index];
    unsigned long long bits = state.bits;
    printf("pindex:\t%d\tvisited:\t%d\n", state.pindex, state.visited ? 1 : 0);
    // if(state.visited) { printf("visited yup\n"); }
    printf("ct:    \t%d\n", state.ct);
    printf("board base10:\t%llu\nboard bits:   ", bits);
    printbits_square(bits);
    printf("\n");
}


void print_sarr()
{
    printf("State array (%d states):\n", sarrlen);
    int i;
    for(i = 0; i < sarrlen; i++) { printf("[%d]\n",i); printstate(i); }
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
    // if (curstate->visited) { printf("visited yup\n"); }

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



int main()
{
    printf("Hello world\n");

    // initialize
    // curindex = 0;
    sarr = (state_t*) malloc(sizeof *sarr * STATES_LEN);

    // // initialize first state in array
    // sarr[0] = (state_t){
    //     .bits = 0b0011100011111011111111111111111011101101100011100uLL,
    //     .ct = 36,
    //     .pindex = -1,
    //     .visited = false
    // };
    // sarrlen = 1;

    // // printf("First state in list:\n");
    // // printstate(0);
    // print_sarr();

    /*
    { // methods testing
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
    //     // A fabricated testing state, that has 24 moves remaining.
    //     .bits = 0b0011000011011001110111111111011100000011000011000uLL,
    //     .ct = 25,
    //     .pindex = -1,
    //     .visited = false
    // };
    sarr[0] = (state_t){
        // One possible real starting state that should lead to a win. Top left is removed.
        .bits = 0b0001100011111011111111111111111111101111100011100uLL,
        .ct = 36,
        .pindex = -1,
        .visited = false
    };
    sarrlen = 1;
    print_sarr();
    // return 0;


    // ================================
    // do real solving now

    const int targetct = 1;

    int bestct = FULL_BOARD_CT;
    int largestsarrlen = 0;
    int curindex;
    int newgen;
    unsigned long long checked = 0;
    unsigned long long generated = 0;
    while (sarrlen > 0 && bestct > targetct)
    {
        if (sarrlen > largestsarrlen) largestsarrlen = sarrlen;
        // generate the next moves. returns how many moves were generated and added to the list.
        // TODO NOTE: if going for a specific target board, no need to search deeper than that target's marble ct. Add that later.
        newgen = add_all_moves_latest();
        if (newgen > 0)
            generated += newgen;
        if (newgen == 0 && checked > 0)
            // if newgen == 0, then it was a leaf node.
            // If no states have been checked yet then we should still check this leaf node, as it must not have been generated.
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
        // check if the newly generated board state is better than our best so far.
        if (curstate.ct < bestct)
        {
            bestct = curstate.ct;
            printf("Found new best state with %d remaining (base10: %llu).\n", bestct, curstate.bits);
            // save chain of solutions
            save_parent_chain(curindex);
        }

        #ifdef PRINT_STATS_EVERY_CHECKED
        if(checked % PRINT_STATS_EVERY_CHECKED_N == 0)
            printf("...info: Checked %llu states, Generated %llu states; best %d, max sarrlen %d of %d...\n",
                checked, generated, bestct, largestsarrlen, STATES_LEN);
        #endif
    }

    if (sarrlen <= 0)
    {
        printf("Exhausted all child states; could not find a solution with %d marble%s.\n", targetct, targetct==1?"":"s or less");
    }
    if (bestct <= targetct)
    {
        printf("Found a solution with only %d marble%s remaining!\n", bestct, bestct==1 ? "" : "s");
    }
    print_solarr();

    printf("Info: Checked %llu states. Generated %llu states. Best %d remaining.\n", checked, generated, bestct);

    //
    printf("\nExecution ended normally.\n");
    return 0;
}