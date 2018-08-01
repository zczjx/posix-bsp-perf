#include <stdio.h>
#include "lib_hello.h"

int main(int argc, char *argv[])
{
    int err = 0;

    printf("hello world arm linux bsp! \n");
    err = print_hello("use bsp common lib to print \n");
    return 0;
}
