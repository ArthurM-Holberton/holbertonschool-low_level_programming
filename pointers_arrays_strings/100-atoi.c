#include "main.h"

/**
 * _atoi - converts a string to an integer
 * @s: the string to convert
 *
 * Return: the converted number, or 0 if there is no number in the string
 */
int _atoi(char *s)
{
	int i, sign, res;

	i = 0;
	sign = 1;
	res = 0;

	while (s[i] != '\0' && (s[i] < '0' || s[i] > '9'))
	{
		if (s[i] == '-')
			sign = -sign;
		i++;
	}

	while (s[i] >= '0' && s[i] <= '9')
	{
		res = res * 10 - (s[i] - '0');
		i++;
	}

	if (sign == 1)
		return (-res);

	return (res);
}
