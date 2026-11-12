#include "dog.h"
#include <stdlib.h>

/**
 * copy_string - makes a copy of a string in newly allocated memory
 * @str: the string to copy
 *
 * Return: a pointer to the copy, or NULL if malloc fails
 */
static char *copy_string(char *str)
{
	char *copy;
	size_t len = 0;
	size_t i;

	while (str[len] != '\0')
		len++;

	copy = malloc(len + 1);
	if (copy == NULL)
		return (NULL);

	for (i = 0; i <= len; i++)
		copy[i] = str[i];

	return (copy);
}

/**
 * new_dog - creates a new dog
 * @name: name of the dog
 * @age: age of the dog
 * @owner: owner of the dog
 *
 * Description: name and owner are copied into newly allocated memory.
 *
 * Return: a pointer to the new dog, or NULL if name or owner is NULL
 * or if an allocation fails
 */
dog_t *new_dog(char *name, float age, char *owner)
{
	dog_t *dog;

	if (name == NULL || owner == NULL)
		return (NULL);

	dog = malloc(sizeof(dog_t));
	if (dog == NULL)
		return (NULL);

	dog->name = copy_string(name);
	if (dog->name == NULL)
	{
		free(dog);
		return (NULL);
	}

	dog->owner = copy_string(owner);
	if (dog->owner == NULL)
	{
		free(dog->name);
		free(dog);
		return (NULL);
	}

	dog->age = age;
	return (dog);
}
