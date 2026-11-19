#include "variadic_functions.h"

/**
 * print_all - prints anything, followed by a new line
 * @format: list of the types of the arguments (c, i, f or s)
 *
 * Return: Nothing.
 */
void print_all(const char * const format, ...)
{
	va_list args;
	unsigned int i = 0;
	const char *str;
	const char *sep = "";

	va_start(args, format);

	while (format != NULL && format[i] != '\0')
	{
		switch (format[i])
		{
		case 'c':
			printf("%s%c", sep, va_arg(args, int));
			sep = ", ";
			break;
		case 'i':
			printf("%s%d", sep, va_arg(args, int));
			sep = ", ";
			break;
		case 'f':
			printf("%s%f", sep, va_arg(args, double));
			sep = ", ";
			break;
		case 's':
			str = va_arg(args, char *);
			if (str == NULL)
				str = "(nil)";
			printf("%s%s", sep, str);
			sep = ", ";
			break;
		}
		i++;
	}

	va_end(args);

	printf("\n");
}
