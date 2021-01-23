#include "charmcc.h"

int main(int argc, char **argv) {
    if (argc < 2 || 3 < argc) {
        error("%s: invalid number of arguments\n", argv[0]);
    }

    bool debug = false;
    char *source;
    if (argc == 2) {
        source = argv[1];
    } else {
        if (strlen(argv[1]) == 7 && strncmp(argv[1], "--debug", 7) == 0) {
            source = argv[2];
            debug = true;
        } else {
            error("%s: invalid flag %s\n", argv[0], argv[1]);
        }
    }

    GC gc = {NULL, NULL, NULL};
    Token *tok = tokenize(source);
    Function *prog = parse(tok, &gc);

    if (debug) {
        debug_ast(prog);
    } else {
        codegen(prog);
    }

    free_tokens(tok);
    cleanup(&gc);

    return 0;
}
