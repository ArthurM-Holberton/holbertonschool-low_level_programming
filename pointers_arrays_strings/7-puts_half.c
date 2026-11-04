#include "main.h"

/**
 * puts_half - prints half of a string, followed by a new line
 * @str: the string to print
 *
 * Return: void
 */
void puts_half(char *str)
{
	int len, i;

	len = 0;
	while (str[len] != '\0')
		len++;

	for (i = (len + 1) / 2; i < len; i++)
		_putchar(str[i]);

	_putchar('\n');
}
