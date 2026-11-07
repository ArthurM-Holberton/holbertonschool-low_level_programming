#include "main.h"

/**
 * in_set - checks if a character belongs to a set of characters
 * @c: the character to look for
 * @set: the string containing the allowed characters
 *
 * Return: 1 if c is in set, 0 otherwise
 */
static int in_set(char c, char *set)
{
	int i;

	for (i = 0; set[i] != '\0'; i++)
		if (c == set[i])
			return (1);

	return (0);
}

/**
 * _strspn - gets the length of a prefix substring
 * @s: the string to scan
 * @accept: the string containing the accepted characters
 *
 * Return: the number of bytes in the initial segment of s which consist
 * only of bytes from accept
 */
unsigned int _strspn(char *s, char *accept)
{
	unsigned int i;

	i = 0;
	while (s[i] != '\0' && in_set(s[i], accept))
		i++;

	return (i);
}
