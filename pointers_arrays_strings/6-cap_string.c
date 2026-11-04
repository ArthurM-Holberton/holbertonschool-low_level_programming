#include "main.h"

/**
 * is_separator - checks if a character separates words
 * @c: the character to check
 *
 * Return: 1 if c is a word separator, 0 otherwise
 */
static int is_separator(char c)
{
	char *seps = " \t\n,;.!?\"(){}";
	int i;

	for (i = 0; seps[i] != '\0'; i++)
		if (c == seps[i])
			return (1);

	return (0);
}

/**
 * cap_string - capitalizes all words of a string
 * @str: the string to modify
 *
 * Return: a pointer to the modified string
 */
char *cap_string(char *str)
{
	int i;

	for (i = 0; str[i] != '\0'; i++)
		if (i == 0 || is_separator(str[i - 1]))
			if (str[i] >= 'a' && str[i] <= 'z')
				str[i] -= 32;

	return (str);
}
