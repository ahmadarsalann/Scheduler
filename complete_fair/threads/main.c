#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(){
	int random = (rand() % 10) - 7;
	printf("random: %d\n", random);
}
