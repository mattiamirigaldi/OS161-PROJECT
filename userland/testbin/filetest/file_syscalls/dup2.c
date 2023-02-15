#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int
main(void)
{
        static char readbuf[13];
	const char *file = "logfile";
	int logfd, rv;

	printf("Testing dup2 with STDOUT\n");
	logfd = open(file, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if (logfd<0) {
		err(1, "%s: open for write", file);
	}

	dup2(logfd, STDOUT_FILENO);
	close(logfd);

	printf("Hello there\n");
	
	rv = read(STDOUT_FILENO, readbuf, 12);
	/* ensure null termination */
	readbuf[12] = 0;
	if (rv<0) {
		err(1, "%s: read", file);
	}
	printf("read %s \n", readbuf);
	printf("Passed dup2 test.\n");
	return 0;
}
