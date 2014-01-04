
CFLAGS=-O0 -g

xtsttopng: xtsttopng.c
	$(CC) $(CFLAGS) -o $@ xtsttopng.c -lpng -lm