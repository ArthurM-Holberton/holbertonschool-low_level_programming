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
 * read_number - prompts for a number and reads it
 * @label: the prompt to display
 * @value: where the number read is stored
 *
 * Return: 1 on success, 0 if the input is not a number
 */
int read_number(char *label, double *value)
{
	int ret;

	printf("%s", label);
	ret = scanf("%lf", value);
	if (ret != 1)
		clear_line();

	return (ret == 1);
}

/**
 * add - asks for two numbers and prints their sum
 *
 * Return: void
 */
void add(void)
{
	double a, b;

	if (read_number("A: ", &a) && read_number("B: ", &b))
		printf("Result: %.10g\n", a + b);
	else
		printf("Invalid input\n");
}

/**
 * subtract - asks for two numbers and prints their difference
 *
 * Return: void
 */
void subtract(void)
{
	double a, b;

	if (read_number("A: ", &a) && read_number("B: ", &b))
		printf("Result: %.10g\n", a - b);
	else
		printf("Invalid input\n");
}

/**
 * multiply - asks for two numbers and prints their product
 *
 * Return: void
 */
void multiply(void)
{
	double a, b;

	if (read_number("A: ", &a) && read_number("B: ", &b))
		printf("Result: %.10g\n", a * b);
	else
		printf("Invalid input\n");
}

/**
 * main - Entry point
 *
 * Description: displays a menu in a loop, runs the chosen operation
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

		if (choice == 1)
			add();
		else if (choice == 2)
			subtract();
		else if (choice == 3)
			multiply();
		else if (choice == 4)
			printf("Not implemented yet\n");
		else if (choice != 0)
			printf("Invalid choice\n");
	}

	printf("Bye!\n");
	return (0);
}
