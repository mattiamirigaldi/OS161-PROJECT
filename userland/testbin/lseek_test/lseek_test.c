/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
// C program to read nth byte of a file and
// copy it to another file using lseek
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>


// Driver code
int main()
{
	char arr[100];
	int n;
	n = 5;

	// Calling for the function
	// Open the file for READ only.
	printf("Hello, opening the file\n");
	int f_write = open("start", O_RDONLY);

	// Open the file for WRITE and READ only.
	int f_read = open("end", O_WRONLY);

	int count = 0;
	printf("FIle opened\n");
	while (read(f_write, arr, 1))
	{
		// to write the 1st byte of the input file in
		// the output file
		if (count < n)
		{
			// SEEK_CUR specifies that
			// the offset provided is relative to the
			// current file position
			lseek (f_write, n, SEEK_CUR);
			write (f_read, arr, 1);
			count = n;
			printf("First\n");
		}

		// After the nth byte (now taking the alternate
		// nth byte)
		else
		{
			count = (2*n);
			lseek(f_write, count, SEEK_CUR);
			write(f_read, arr, 1);
			printf("Second\n");
			
		}
	}
	close(f_write);
	close(f_read);
	printf("Lseek test finished\n");
	return 0;
}
