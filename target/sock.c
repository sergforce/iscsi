#include "sock.h"

int readBytes(SOCKET s, char *ptr, int bytes2read, int timeout)
{
	int len = 0;
	int res = 0;

	while (res != bytes2read) {
		res = recv(s, ptr + len, bytes2read - len, 0);
		if (res == 0) {			/* connection shutdowned */
			return 0;
		} else if (res == -1) { /* connection reseted */
			return -1;
		}
		
		len += res;
	}
	return len;
}

int readWords(SOCKET s, uint32_t *ptr, int bytes2read, int timeout)
{
	int len = 0;
	int res = readBytes(s, (char*)ptr, bytes2read, timeout);
	if (res != bytes2read) {
		return -1;
	}

	for (len = 0; len < bytes2read; len += sizeof(uint32_t), ptr++) {
		*ptr = ntohl(*ptr);
	}

	return len;
}


/*************************************************************************/

int writeBytes(SOCKET s, char *ptr, int bytes2write, int timeout)
{
	int len = 0;
	int res = 0;

	while (res != bytes2write) {
		res = send(s, ptr + len, bytes2write - len, 0);
		if (res == 0) {			/* connection shutdowned */
			return 0;
		} else if (res == -1) { /* connection reseted */
			return -1;
		}
		
		len += res;
	}
	return len;
}

int writeWords(SOCKET s, uint32_t *ptr, int bytes2write, int timeout)
{
	int len;
	int res;
	uint32_t *tmp = ptr;

	for (len = 0; len < bytes2write; len += sizeof(uint32_t), tmp++) {
		*tmp = htonl(*tmp);
	}

	res = writeBytes(s, (char*)ptr, bytes2write, timeout);

	return res;
}



