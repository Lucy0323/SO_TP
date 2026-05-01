#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int main() {

    printf("=== Teste 1: comando simples ===\n");
    system("./bin/runner -e 1 \"echo hello\"");
    sleep(1);

    printf("\n=== Teste 2: comando demorado ===\n");
    system("./bin/runner -e 1 \"sleep 3\"");
    sleep(1);

    printf("\n=== Teste 3: status ===\n");
    system("./bin/runner -e 1 \"sleep 5\" &");
    sleep(1);
    system("./bin/runner -c");
    sleep(6);

    printf("\n=== Teste 4: queue ===\n");
    system("./bin/runner -e 1 \"sleep 5\" &");
    system("./bin/runner -e 2 \"echo second\" &");
    sleep(1);
    system("./bin/runner -c");
    sleep(6);

    printf("\n=== Teste 5: stop ===\n");
    system("./bin/runner -s");

    printf("\n=== Testes concluídos ===\n");

    return 0;
}