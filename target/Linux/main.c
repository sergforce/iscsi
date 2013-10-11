#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "../debug/iscsi_debug.h"

int targetTest(void);

char inifile[256] = "/etc/iscsi-daemon-targets.conf";


int main(int argc, char* argv[])
{
	int daemonize = 0;
	int debug = 0;
	int opt;

	while ((opt = getopt(argc, argv, "c:dD")) != -1) {
		switch (opt) {
		case 'D':
			debug = 1;
			break;
		case 'd':
			daemonize = 1;
			break;
		case 'c':
			strncpy(inifile, optarg, 255);
			inifile[255] = 0;
			break;
		default: /* '?' */
			fprintf(stderr, "Usage: %s [-c configuration] [-d]\n",
				argv[0]);
			exit(EXIT_FAILURE);
		}
	}

#ifdef _DEBUG2
	if (debug) {
		initDebug(stdout);
	}
#endif
	targetTest();
	
	return 0;
}
