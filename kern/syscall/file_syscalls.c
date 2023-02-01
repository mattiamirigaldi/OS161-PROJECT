//implementazione sys_write() and sys_read() come prototipi read and write
//usa putch(), getch() per stdin/out/err
//putch() show char on stdout
//getch() acquire char on stdin
#include <lib.h>
#include <types.h>
#include <syscall.h>
#include <kern/unistd.h>
#include <clock.h>
#include <copyinout.h>

//size_t is the  size of buf
//buf is a const * 
int sys_read (int filehandle, userptr_t buf, size_t size){
	int num;
	char *stampato =(char *)buf;
	//to handle read you have have STDIN file, otherwise error
	if (filehandle!=STDIN_FILENO){
		kprintf("stdin support, no altri\n");
		return -1;
	}
	for(num=0; num<(int)size; num++){
		stampato[num]= getch();
		if (stampato[num]<0) return num;
	}
	return (int)size;
}


int sys_write (int filehandle,userptr_t	buf, size_t size){
        int num;
        char *stampato =(char *)buf;
        //to handle write you have have STDOUT file, otherwise error
        if (filehandle!=STDOUT_FILENO && filehandle!=STDERR_FILENO){
                kprintf("stdout support, no altri\n");
                return -1;
        }
        for(num=0; num<(int)size; num++){
                putch(stampato[num]);
        }
        return (int)size;
}
