// c-preprocessor smoke example: comments, macros, and multi-file includes.
#include "constants.h"
#include "geometry.h"
#include "util.h"

/* This block comment
   spans multiple lines
   before the next declaration. */
int main(void) {
    int max = MAX_SCORE;        // MAX_SCORE -> 100
    int min = MIN_SCORE;        // MIN_SCORE -> 0
    int circumference = TWO_PI; // nested macro expansion -> (3 * 2)
    const char *note = "See http://example.com for details"; // // must stay literal
    printf(GREETING);
    printf("%s\n", UNIT_LABEL);
    return max - min - circumference EMPTY_FLAG;
}
