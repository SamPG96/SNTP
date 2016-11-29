sntpclient:
	gcc -Wall sntpclient.c reusedlib.c sntptools.c -o sntpclient -lconfig
