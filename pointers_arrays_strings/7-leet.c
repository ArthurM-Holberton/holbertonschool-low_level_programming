#include "main.h"

/**
 * leet - encodes a string into 1337
 * @str: the string to encode
 *
 * Return: a pointer to the encoded string
 */
char *leet(char *str)
{
	char *from = "aAeEoOtTlL";
	char *to = "4433007711";
	int i, j;

	for (i = 0; str[i] != '\0'; i++)
		for (j = 0; from[j] != '\0'; j++)
			if (str[i] == from[j])
				str[i] = to[j];

	return (str);
}
