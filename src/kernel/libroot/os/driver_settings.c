/* driver_settings - implements the driver settings API
**
** Initial version by Axel Dörfler, axeld@pinc-software.de
** This file may be used under the terms of the OpenBeOS License.
*/


#include <OS.h>
#include <driver_settings.h>

// this definition is currently missing from the headers above...
// just a small to let it compile fine in the OpenBeOS tree
#ifndef B_FILE_NAME_LENGTH
#	define B_FILE_NAME_LENGTH 256
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>


#define SETTINGS_DIRECTORY "/boot/home/config/settings/kernel/drivers/"
#define SETTINGS_MAGIC		'DrvS'

// Those maximum values are independent from the implementation - they
// have been chosen to make the code more robust against bad files
#define MAX_SETTINGS_SIZE	32768
#define MAX_SETTINGS_LEVEL	8

#define CONTINUE_PARAMETER	1

typedef struct settings_handle {
	void	*first_buffer;
	int32	magic;
	struct	driver_settings settings;
	char	*text;
} settings_handle;


//	Functions not part of the public API


static inline bool
is_parameter_separator(char c)
{
	return c == '\n' || c == ';';
}


static inline bool
is_parameter_assignment(char c)
{
	return isspace(c) || c == '=';
}


static inline bool
is_word_break(char c)
{
	return c == ' ' || c == '\t' || is_parameter_separator(c);
}


static inline bool
check_handle(settings_handle *handle)
{
	if (handle == NULL
		|| handle->magic != SETTINGS_MAGIC)
		return false;

	return true;
}


static driver_parameter *
get_parameter(settings_handle *handle, const char *name)
{
	int32 i;
	for (i = handle->settings.parameter_count; i-- > 0;) {
		if (!strcmp(handle->settings.parameters[i].name, name))
			return &handle->settings.parameters[i];
	}
	return NULL;
}


static status_t
get_word(char **_pos, char **_word, bool allowNewLine)
{
	char *pos = *_pos;
	char quoted = 0;
	bool newLine = false, end = false;
	int escaped = 0;
	bool charEscaped = false;

	// Skip any white space and comments
	while (pos[0]
		&& ((allowNewLine && (isspace(pos[0]) || is_parameter_separator(pos[0]) || pos[0] == '#'))
		|| (!allowNewLine && (pos[0] == '\t' || pos[0] == ' ')))) {
		// skip any comment lines
		if (pos[0] == '#') {
			while (pos[0] && pos[0] != '\n')
				pos++;
		}
		pos++;
	}

	if (pos[0] == '}') {
		*_pos = pos;
		return B_NO_ERROR;
	}

	// Read in a word - might contain escaped (\) spaces, or it
	// might also be quoted (" or ').

	if (pos[0] == '"' || pos[0] == '\'') {
		quoted = pos[0];
		pos++;
	}
	*_word = pos;

	while (pos[0]) {
		if (charEscaped)
			charEscaped = false;
		else if (pos[0] == '\\') {
			charEscaped = true;
			escaped++;
		} else if ((!quoted && is_word_break(pos[0])) || (quoted && pos[0] == quoted))
			break;
		pos++;
	}

	// "String exceeds line" - missing end quote
	if (quoted && pos[0] != quoted)
		return B_BAD_DATA;

	// last character is a backslash
	if (charEscaped)
		return B_BAD_DATA;

	end = pos[0] == '\0';
	newLine = is_parameter_separator(pos[0]) || end;
	pos[0] = '\0';

	// Correct name if there were any escaped characters
	if (escaped) {
		char *word = *_word;
		int offset = 0;
		while (word <= pos) {
			if (word[0] == '\\') {
				offset--;
				word++;
			}
			word[offset] = word[0];
			word++;
		}
	}

	if (end) {
		*_pos = pos;
		return B_OK;
	}

	// Scan for next beginning word, open brackets, or comment start

	pos++;
	while (true) {
		*_pos = pos;
		if (!pos[0])
			return B_NO_ERROR;

		if (is_parameter_separator(pos[0])) {
			// an open bracket '{' could follow after the first
			// newline, but not later
			if (newLine)
				return B_NO_ERROR;

			newLine = true;
		} else if (pos[0] == '{' || pos[0] == '}' || pos[0] == '#')
			return B_NO_ERROR;
		else if (!isspace(pos[0]))
			return newLine ? B_NO_ERROR : CONTINUE_PARAMETER;

		pos++;
	}
}


static status_t
parse_parameter(struct driver_parameter *parameter, char **_pos, int32 level)
{
	char *pos = *_pos;
	status_t status;

	// initialize parameter first
	memset(parameter, 0, sizeof(struct driver_parameter));

	status = get_word(&pos, &parameter->name, true);
	if (status == CONTINUE_PARAMETER) {
		while (status == CONTINUE_PARAMETER) {
			char **newArray, *value;
			status = get_word(&pos, &value, false);
			if (status < B_OK)
				break;

			// the flat settings style allows to put a '=' between the
			// parameter name and its values
			if (!strcmp(value, "=") && parameter->value_count == 0)
				continue;

			// enlarge value array and save the value

			newArray = realloc(parameter->values, (parameter->value_count + 1) * sizeof(char *));
			if (newArray == NULL)
				return B_NO_MEMORY;

			parameter->values = newArray;
			parameter->values[parameter->value_count++] = value;
		}
	}

	*_pos = pos;
	return status;
}


static status_t
parse_parameters(struct driver_parameter **parameters, int *count, char **pos, int32 level)
{
	if (level > MAX_SETTINGS_LEVEL)
		return B_LINK_LIMIT;

	while (true) {
		struct driver_parameter *newArray, *parameter;
		status_t status;
		newArray = realloc(*parameters, (*count + 1) * sizeof(struct driver_parameter));
		if (newArray == NULL)
			return B_NO_MEMORY;

		parameter = &newArray[*count];

		status = parse_parameter(parameter, pos, level);
		if (status < B_OK)
			return status;

		*parameters = newArray;
		(*count)++;

		// check for level beginning and end
		if (**pos == '{') {
			// if we go a level deeper, just start all over again...
			(*pos)++;
			status = parse_parameters(&parameter->parameters, &parameter->parameter_count, pos, level + 1);
			if (status < B_OK)
				return status;
		}
		if ((**pos == '}' && level > 0)
			|| (**pos == '\0' && level == 0)) {
			// take the closing bracket from the stack
			(*pos)++;
			return B_OK;
		}

		// obviously, something has gone wrong
		if (**pos == '}' || **pos == '\0')
			return B_ERROR;
	}
}


static status_t
parse_settings(settings_handle *handle)
{
	char *text = handle->text;

	memset(&handle->settings, 0, sizeof(struct driver_settings));

	// empty settings are allowed
	if (text == NULL)
		return B_OK;

	return parse_parameters(&handle->settings.parameters, &handle->settings.parameter_count, &text, 0);
}


static void
free_parameter(struct driver_parameter *parameter)
{
	int32 i;
	for (i = parameter->parameter_count; i-- > 0;)
		free_parameter(&parameter->parameters[i]);

	free(parameter->parameters);
	free(parameter->values);
}


static void
free_settings(settings_handle *handle)
{
	int32 i;
	for (i = handle->settings.parameter_count; i-- > 0;)
		free_parameter(&handle->settings.parameters[i]);

	free(handle->settings.parameters);

	free(handle->text);
	free(handle);
}


static settings_handle *
load_driver_settings_from_file(int file)
{
	struct stat stat;

	// Allocate a buffer and read the whole file into it.
	// We will keep this buffer in memory, until the settings
	// are unloaded.
	// The driver_parameter::name field will point directly
	// to this buffer.

	if (fstat(file, &stat) < B_OK)
		return NULL;

	if (stat.st_size > B_OK && stat.st_size < MAX_SETTINGS_SIZE) {
		char *text = (char *)malloc(stat.st_size + 1);
		if (text != NULL && read(file, text, stat.st_size) == stat.st_size) {
			settings_handle *handle = malloc(sizeof(settings_handle));
			if (handle != NULL) {
				text[stat.st_size] = '\0';

				handle->magic = SETTINGS_MAGIC;
				handle->text = text;

				if (parse_settings(handle) == B_OK) {
					return handle;
				}

				free(handle);
			}
		}
		// "text" might be NULL here, but that's allowed
		free(text);
	}

	return NULL;
}


static bool
put_string(char **_buffer, size_t *_bufferSize, char *string)
{
	char *buffer = *_buffer;
	size_t length;

	if (string == NULL)
		return true;

	length = strlen(string);

	// update _bufferSize in any way, so that we can chain several
	// of these calls without having to check the return value
	// everytime
	*_bufferSize -= length;

	if (*_bufferSize <= 0)
		return false;

	length = strlen(string);
	memcpy(buffer, string, length);
	buffer += length;

	buffer[0] = '\0';

	// update the buffer position
	*_buffer = buffer;

	return true;
}


static bool
put_char(char **_buffer, size_t *_bufferSize, char c)
{
	char *buffer = *_buffer;

	*_bufferSize -= 1;

	if (*_bufferSize <= 0)
		return false;

	buffer[0] = c;
	buffer[1] = '\0';

	// update the buffer position
	*_buffer = buffer + 1;

	return true;
}


static void
put_level_space(char **_buffer, size_t *_bufferSize, int32 level)
{
	while (level-- > 0)
		put_char(_buffer, _bufferSize, '\t');
}


static bool
put_parameter(char **_buffer, size_t *_bufferSize, struct driver_parameter *parameter, int32 level, bool flat)
{
	int32 i;

	if (!flat)
		put_level_space(_buffer, _bufferSize, level);

	put_string(_buffer, _bufferSize, parameter->name);
	if (flat && parameter->value_count > 0)
		put_string(_buffer, _bufferSize, " =");

	for (i = 0; i < parameter->value_count; i++) {
		put_char(_buffer, _bufferSize, ' ');
		put_string(_buffer, _bufferSize, parameter->values[i]);
	}

	if (parameter->parameter_count > 0) {
		put_string(_buffer, _bufferSize, " {");
		if (!flat)
			put_char(_buffer, _bufferSize, '\n');

		for (i = 0; i < parameter->parameter_count; i++) {
			put_parameter(_buffer, _bufferSize, &parameter->parameters[i], level + 1, flat);
			if (parameter->parameters[i].parameter_count == 0)
				put_char(_buffer, _bufferSize, flat ? ';' : '\n');
		}

		if (!flat)
			put_level_space(_buffer, _bufferSize, level);
		put_char(_buffer, _bufferSize, '}');
	}

	return *_bufferSize >= 0;
}


// ToDo: the API to add an item to the driver_settings is obviously accessable
//	to the kernel, so we should provide it, too (in BeOS this is used to add
//	driver settings at boot time, using the safe boot menu).

//static status_t
//add_driver_parameter(const char *name, )
//{
//}


//	#pragma mark -
//	The public API implementation


status_t
unload_driver_settings(void *handle)
{
	if (!check_handle(handle))
		return B_BAD_VALUE;

	free_settings(handle);
	return B_OK;
}


void *
load_driver_settings(const char *driverName)
{
	settings_handle *handle;
	int file;
	
	if (driverName == NULL)
		return NULL;

	// open the settings from the standardized location
	{
		char path[B_FILE_NAME_LENGTH + 64];

		// ToDo: use the kernel's find_directory for this
		strcpy(path, SETTINGS_DIRECTORY);
		strlcat(path, driverName, sizeof(path));

		file = open(path, O_RDONLY);
	}
	if (file < B_OK)
		return NULL;

	handle = load_driver_settings_from_file(file);

	close(file);
	return (void *)handle;
}


/** Loads a driver settings file using the full path, instead of
 *	only defining the leaf name (as load_driver_settings() does).
 *	I am not sure if this function is really necessary - I would
 *	probably prefer something like a search order (if it's not
 *	an absolute path):
 *		~/config/settings/kernel/driver
 *		current directory
 *	That would render this function useless.
 */

#if 0
void *
load_driver_settings_from_path(const char *path)
{
	settings_handle *handle;
	int file;

	if (path == NULL)
		return NULL;

	file = open(path, O_RDONLY);
	if (file < B_OK)
		return NULL;

	handle = load_driver_settings_from_file(file);

	close(file);
	return (void *)handle;
}
#endif

/** Returns a new driver_settings handle that has the parsed contents
 *	of the passed string.
 *	You can get an empty driver_settings object when you pass NULL as
 *	the "settingsString" parameter.
 */

void *
parse_driver_settings_string(const char *settingsString)
{
	// we simply copy the whole string to use it as our internal buffer
	char *text = strdup(settingsString);
	if (settingsString == NULL || text != NULL) {
		settings_handle *handle = malloc(sizeof(settings_handle));
		if (handle != NULL) {
			handle->magic = SETTINGS_MAGIC;
			handle->text = text;

			if (parse_settings(handle) == B_OK)
				return handle;

			free(handle);
		}
		free(text);
	}

	return NULL;
}


/** This function prints out a driver settings structure to a human
 *	readable string.
 *	It's either in standard style or the single line style speficied
 *	by the "flat" parameter.
 *	If the buffer is too small to hold the string, B_DEVICE_FULL (for now)
 *	is returned, and the needed amount of bytes if placed in the
 *	"_bufferSize" parameter.
 *	If the "handle" parameter is not a valid driver settings handle, or
 *	the "buffer" parameter is NULL, B_BAD_VALUE is returned.
 */

status_t
get_driver_settings_string(void *_handle, char *buffer, size_t *_bufferSize, bool flat)
{
	settings_handle *handle = (settings_handle *)_handle;
	size_t bufferSize = *_bufferSize;
	int32 i;

	if (!check_handle(handle) || !buffer || *_bufferSize == 0)
		return B_BAD_VALUE;

	for (i = 0; i < handle->settings.parameter_count; i++) {
		put_parameter(&buffer, &bufferSize, &handle->settings.parameters[i], 0, flat);
	}

	*_bufferSize -= bufferSize;
	return bufferSize >= 0 ? B_OK : B_DEVICE_FULL;
		// ToDo: ?? don't we have a B_BUFFER_OVERFLOW kind of error?
		//	find a better error code for this.
}


bool
get_driver_boolean_parameter(void *handle, const char *keyName, bool unknownValue, bool noArgValue)
{
	driver_parameter *parameter;
	char *boolean;

	if (!check_handle(handle))
		return unknownValue;

	// check for the parameter
	if ((parameter = get_parameter(handle, keyName)) == NULL)
		return unknownValue;

	// ToDo: This takes just the first argument/value, and checks that one;
	// I don't know if they are used to work that way in BeOS, though.
	// bonefish: Yep, exactly like that.

	// check for the argument
	if (parameter->value_count <= 0)
		return noArgValue;

	boolean = parameter->values[0];
	if (!strcmp(boolean, "1")
		|| !strcasecmp(boolean, "true")
		|| !strcasecmp(boolean, "yes")
		|| !strcasecmp(boolean, "on")
		|| !strcasecmp(boolean, "enable")
		|| !strcasecmp(boolean, "enabled"))
		return true;

	if (!strcmp(boolean, "0")
		|| !strcasecmp(boolean, "false")
		|| !strcasecmp(boolean, "no")
		|| !strcasecmp(boolean, "off")
		|| !strcasecmp(boolean, "disable")
		|| !strcasecmp(boolean, "disabled"))
		return false;

	// if no known keyword is found, "unknownValue" is returned
	return unknownValue;
}


const char *
get_driver_parameter(void *handle, const char *keyName, const char *unknownValue, const char *noArgValue)
{
	struct driver_parameter *parameter;

	if (!check_handle(handle))
		return unknownValue;

	// check for the parameter
	if ((parameter = get_parameter(handle, keyName)) == NULL)
		return unknownValue;

	// check for the argument
	if (parameter->value_count <= 0)
		return noArgValue;

	return parameter->values[0];
}


const driver_settings *
get_driver_settings(void *handle)
{
	if (!check_handle(handle))
		return NULL;

	return &((settings_handle *)handle)->settings;
}


// this creates an alias of the above function
// unload_driver_settings() is the same as delete_driver_settings()
extern __typeof(unload_driver_settings) delete_driver_settings __attribute__ ((alias ("unload_driver_settings")));

