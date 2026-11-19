#include <stdio.h>

/**
 * main - prints the name of the program, followed by a new line
 * @argc: number of arguments passed to the program
 * @argv: array of the arguments passed to the program
 *
 * Return: Always 0 (Success)
 */
int main(int argc __attribute__((unused)), char *argv[])
{
	printf("%s\n", argv[0]);

	return (0);
}
