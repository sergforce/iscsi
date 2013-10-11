#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include "conf_reader.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

static char* space_separate(char* string)
{
	char* space = string;
	while (!isspace(*space) && (*space != 0))
		space++;

	if (*space != 0) {
		*space = 0;
		space++;
		while (isspace(*space))
			space++;

		return space;
	}
	return NULL;
}

#define STR_BUF	256
int initConfigurationFromINI(struct configuration *cnf, const char* filename)
{
	FILE* f = fopen(filename, "r");
	struct confElement *targets;
	struct confElement *current_target = NULL;
	char confline[256];
	char *buf;

	int ignore_untill_next_target = FALSE;
	int line = 0;

	if (!f) {
		fprintf(stderr, "Can't open configuration file %s: %s\n",
			filename, strerror(errno));
		return -1; /* file erro found */
	}

	cleanConfiguration(cnf);
	targets = addMain(cnf, TARGETS, "");

	while (fgets(confline, STR_BUF-1, f)) {
		char* eq;

		buf = confline;
		line++;
		while (isspace(*buf))
			buf++;

		/* Skip comments */
		if (*buf == 0 || *buf == '#')
			continue;

		if (*buf == '[') {
			char* space;

			buf++;
			while (isspace(*buf))
				buf++;

			/* New target */
			space = space_separate(buf);

			if ((space == NULL) || (*space == 0) || (*space == ']')) {
				ignore_untill_next_target = TRUE;
				current_target = NULL;

				fprintf(stderr, "Error configuration file %s.%d: No driver and isn: %s\n",
					filename, line, buf);
			} else {
				char* end = strchr(space, ']');
				if (end)
					*end = 0;

				current_target = addElementFill(cnf, targets,
								space, buf);
				ignore_untill_next_target = FALSE;
			}
			continue;

		} else if (ignore_untill_next_target) {
			continue;
		}

		if (current_target == NULL) {
			fprintf(stderr, "Error configuration file %s.%d: No section for: %s\n",
				filename, line, buf);
			continue;
		}

		/* split to param = value */
		eq = strchr(buf, '=');
		if (eq) {
			*eq = 0;

			do {
				eq++;
			} while (isspace(eq));
		}

		if ((eq == NULL) || (*eq == 0) || (*buf == '=')) {
			fprintf(stderr, "Error configuration file %s.%d: Non parameter string: %s\n",
				filename, line, buf);
			continue;
		}

		space_separate(buf);
		space_separate(eq);

		addElementFill(cnf, current_target, buf, eq);
	}

	fclose(f);
	return 0;
}
