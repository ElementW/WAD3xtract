all: wad3xtract

wad3xtract: wad3xtract.c
	$(CC) -std=c99 -O3 -Os -o wad3xtract wad3xtract.c