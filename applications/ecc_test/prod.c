#include <stdlib.h>
#include <stdio.h>

#include <memphis.h>

#include "ecc_test_std.h"

int main()
{
	while(memphis_get_tick() < ECC_TEST_START);
	puts("Inicio da aplicacao prod");

	int *msg = malloc(ECC_TEST_FLITS*sizeof(int));

	for(int i = 0; i < ECC_TEST_FLITS; i++) 
		msg[i] = i;

	for(int i = 0; i < ECC_TEST_ITERATIONS; i++){
		memphis_send(msg, ECC_TEST_FLITS*sizeof(int), MEMPHIS_MSG_ECC | cons);
	}

	while(memphis_get_tick() < ECC_TEST_END);
	puts("Fim da aplicacao prod");
	
	return 0;
}
