/* program for making disk labels */
/* Runs on OS-9 MM/1 with Radio Shack DMP-420 printer */

main(argc, argv)
int argc;
char **argv;
{
	char command[256];
	int i, copies;

	if (argc < 3)
	{
		printf("Useage:  lab <number> <copies>\n");
		printf("Example: lab    1       100\n");
		exit(0);
	}

	sprintf(command, "list /dd/ccb/lab%s >/p", argv[1]);
	copies = atoi(argv[2]);

	for (i = 0; i < copies; i++)
	{
		system(command);
	}
}
