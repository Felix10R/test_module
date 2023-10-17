#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>

#define BUFFER_SIZE 1024

char write_buf[BUFFER_SIZE];
char read_buf[BUFFER_SIZE];

int main()
{
	int fd;
	char option;

	printf("Willkommen bei der Userspace Applikation für den Textaustausch\n\n");

	fd = open("/dev/module_device", O_RDWR);
	if(fd < 0) {
		printf("Kann Modul nicht oeffnen\n");
		return 0;
	}

	while (1) {
		printf("Bitte Option waehlen\n");
		printf("1: Text in das Kernel Modul schreiben\n");
		printf("2: Text aus dem Kernel Modul lesen\n");
		printf("3: Beenden\n");
		scanf(" %c", &option);
		printf("Auswahl: %c\n", option);

		switch (option) {
		case '1':
			printf("Text eingeben\n");
			scanf(" %[^\t\n]s", write_buf);
			write(fd, write_buf, BUFFER_SIZE);
			printf("Text uebertragen\n\n");
			break;
		case '2':
			read(fd, read_buf, BUFFER_SIZE);
			printf("Text: %s\n\n", read_buf);
			break;
		case '3':
			close(fd);
			exit(1);
			break;
		default:
			printf("Undefinierte Option\n\n");
			break;
		}
	}
close(fd);
}