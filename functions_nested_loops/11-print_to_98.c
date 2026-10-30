#include "main.h"
#include <stdio.h>

/**
 * print_to_98 - prints all integers from n to 98, followed by a new line
 * @n: the first number to print
 *
 * Return: void
 */
void print_to_98(int n)
{
	while (n < 98)
		printf("%d, ", n++);

	while (n > 98)
		printf("%d, ", n--);

	printf("98\n");
}
