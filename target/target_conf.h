#ifndef _TARGET_CONF
#define _TARGET_CONF


#define MAX_SEG_LENGTH			(128*1024)

#define MAX_U_FIRSTBURSTLENGTH	(128*1024)
#define MAX_U_MAXBURSTLENGTH	(128*1024)

/* Chosing none digest if available */
#define PREF_NONE_HEADER_DIGEST			1
#define PREF_NONE_DATA_DIGEST			1


#define SESSION_BIGBUFFER		(1024*1024 + 4096 - sizeof(struct Buffer))
#define SESSION_MIDDLEBUFFER	(16384 - sizeof(struct Buffer))
#define SESSION_SMALLBUFFER		(4096 - sizeof(struct Buffer))

#define SESSION_BUFFER_CLASSES		2

#define SESSION_BUFFER_INITIALIZATOR	{	\
	{SESSION_BIGBUFFER, 2, 4, 4},			\
	{SESSION_MIDDLEBUFFER, 2, 4, 4},		\
	}


#endif
