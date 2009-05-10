/* Utility to split archive over multiple floppy disks */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUF_SIZE 1150000

int i, in, out, num_read;
int part = 0;
char name[30];
char *buffer;

main()
{
    buffer = (char *) malloc(BUF_SIZE);
    if (buffer == NULL)
    {
	printf("Out of memory!\n");
	exit(1);
    }
    in = open("/dd/ccb/n16e11n.exe", S_IREAD);
    if (in == -1)
    {
	printf("Can't open source file\n");
	exit(1);
    }

    do
    {
	printf("Insert disk number %d and press <enter>\n", ++part);
    	putchar(7);
	num_read = read(in, buffer, BUF_SIZE);
	if (num_read == 0)
	{
		printf("We won't need that last disk; we're all done.\n");
    		putchar(7);
		close(in);
		exit(0);
	}
	getchar(); /* wait for disk to be inserted! */
	sprintf(name, "/dd/ccb/p%d", part);
	out = creat(name, S_IWRITE, S_IREAD|S_IWRITE, BUF_SIZE);
	if (out == -1)
	{
		printf("Cannot create file!\n");
		exit(1);
	}
	if (write(out, buffer, num_read) != num_read)
	{
		printf("Incomplete disk file created!\n");
		exit(1);
	}
	close(out);
    }while(num_read == BUF_SIZE);
    close(in);
    printf("All done!\n");
    putchar(7);
}
