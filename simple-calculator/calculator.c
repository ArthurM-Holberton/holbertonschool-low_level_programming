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
 * clear_line - discards the rest of the current input line
 *
 * Return: void
 */
void clear_line(void)
{
	int c;

	c = getchar();
	while (c != '\n' && c != EOF)
		c = getchar();
}

/**
 * main - Entry point
 *
 * Description: displays a menu in a loop, rejects invalid choices
 * and stops when the user chooses 0
 *
 * Return: Always 0 (Success)
 */
int main(void)
{
	int choice, ret;

	printf("Simple Calculator\n");
	choice = -1;

	while (choice != 0)
	{
		print_menu();
		ret = scanf("%d", &choice);
		if (ret == EOF)
			break;

		if (ret != 1)
		{
			clear_line();
			choice = -1;
		}

		if (choice >= 1 && choice <= 4)
			printf("Not implemented yet\n");
		else if (choice != 0)
			printf("Invalid choice\n");
	}

	printf("Bye!\n");
	return (0);
}
