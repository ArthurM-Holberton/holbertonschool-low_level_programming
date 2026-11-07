#include <stdio.h>

/**
 * print_menu - displays the calculator menu and the choice prompt
 *
 * Return: void
 */
void print_menu(void)
{
	printf("1) Add\n");
	printf("2) Subtract\n");
	printf("3) Multiply\n");
	printf("4) Divide\n");
	printf("0) Quit\n");
	printf("Choice: ");
}

/**
 * main - Entry point
 *
 * Description: prints a welcome line, then displays a menu until the user
 * chooses 0 to quit
 *
 * Return: Always 0 (Success)
 */
int main(void)
{
	int choice;

	printf("Simple Calculator\n");
	choice = -1;

	while (choice != 0)
	{
		print_menu();
		if (scanf("%d", &choice) != 1)
			break;

		if (choice != 0)
			printf("Not implemented yet\n");
	}

	printf("Bye!\n");
	return (0);
}
