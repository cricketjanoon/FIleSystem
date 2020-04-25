#include <stdio.h>
#include <string.h>

#define FILENAME "disk.bin"

int main()
{
	int i;
	char data[1024];
	FILE *fp;

    fp  = fopen(FILENAME, "wb");
	if(fp==NULL)
	{
		printf("Error creating file %s\n", FILENAME);
		return -1;
	}

	bzero(data, sizeof(data));
	for(i=0; i<1024; i++)
	{
		fwrite(data, sizeof(data), 1, fp);
	}
	fclose(fp);

	return 0;
}
