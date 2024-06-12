#include <stdlib.h>
#include <stdio.h>

#include <memphis.h>

#include "ecc_test_std.h"

int main()
{
	unsigned *time = malloc(ECC_TEST_ITERATIONS*sizeof(unsigned));
	int *msg = malloc(ECC_TEST_FLITS*sizeof(int));

	while(memphis_get_tick() < ECC_TEST_START);
	puts("Inicio da aplicacao cons");

	for(int i = 0; i < ECC_TEST_ITERATIONS; i++){
		memphis_receive(msg, ECC_TEST_FLITS*sizeof(int), MEMPHIS_MSG_ECC | prod);
		time[i] = memphis_get_tick();
	}

	for(int i = 0; i < ECC_TEST_ITERATIONS; i++){
		printf("%d\n", time[i]);
	}

	while(memphis_get_tick() < ECC_TEST_END);
	puts("Fim da aplicacao cons");

	return 0;
}
