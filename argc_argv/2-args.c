#include <stdio.h>

/**
 * main - prints all the arguments it receives
 * @argc: number of arguments, the name of the program included
 * @argv: array of the arguments passed to the program
 *
 * Return: Always 0 (Success)
 */
int main(int argc, char *argv[])
{
	int i;

	for (i = 0; i < argc; i++)
		printf("%s\n", argv[i]);

	return (0);
}
