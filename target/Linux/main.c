#include <stdio.h>
#include "../debug/iscsi_debug.h"

int targetTest(void);


int main(int argc, char* argv[])
{	

#ifdef _DEBUG2	
	initDebug(stdout);
#endif
	targetTest();
	
	return 0;
}
