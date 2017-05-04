#include <stdio.h>

int main(int argc, char **argv)
{
	int i;

	for (i = 10; i < 101; i++)
		printf("{ \"name\":\"number %d\", \"tag\":%d, \"down_nodes\":[  ] },\n", i, i+1);

	return 0;
}
