#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "%s: invalid number of arguments\n", argv[0]);
        return 1;
    }

    printf(".global main\n\n");

    printf("main:\n");
    printf("  MOV   r0, #%d\n", atoi(argv[1]));
    printf("  BX    lr\n");

    return 0;
}
