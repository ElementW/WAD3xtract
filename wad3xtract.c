/* WAD3xtract - extract WAD3 textures
Copyright (C) 2015  Dorian 'gravgun' Wouters

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _XOPEN_SOURCE 500
// ^ SUSv2, for realpath()
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>

typedef struct _WadHeader {
	char magic[4];
	int32_t dircount;
	int32_t diroffset;
} WadHeader;


#define MAXTEXTURENAME 16
typedef struct _WadDirEntry {
	int32_t fileOffset;			// offset in file
	int32_t fileStoreSize;		// stored size in file
	int32_t fileSize;			// uncompressed file size
	int8_t type;				// type of entry
	bool compressed;
	int16_t _dummy;
	char name[MAXTEXTURENAME];	// must be null terminated
} WadDirEntry;

enum {
	WadType_None = 0,
	WadType_Tex = 0x43,
	WadType_Miptex = 0x44,
	WadType_Qpic = 0x42,
	WadType_Font = 0x45
};

#define MAXTEXTURENAME 16
#define MIPLEVELS 4
typedef struct _BSPMipTex {
	char name[MAXTEXTURENAME];
	uint32_t width, height;
	uint32_t offsets[MIPLEVELS]; // offsets to mipmaps' data
} BSPMipTex;

static int ipow(int x, int n) {
	int r = 1;
	while (n--)
		r *= x;
	return r; 
}

int main(int argc, char **argv) {
	if (argc < 2) {
		puts  ("WAD3xtract - extract WAD3 textures to .pbm");
		printf("Usage: %s <file>\n", argv[0]);
		return EXIT_SUCCESS;
	}
	
	char targetdir[PATH_MAX+1];
	realpath(argv[1], targetdir);
	for (int i=strlen(targetdir); i > 0; --i) {
		if (targetdir[i] == '/') {
			// In case wad file doesn't have an extension
			char tmptargetdir[PATH_MAX+1]; // avoid risky self-writing snprintf
			strncpy(tmptargetdir, targetdir, PATH_MAX+1);
			snprintf(targetdir, PATH_MAX+1, "%s_wad/", tmptargetdir);
			break;
		}
		if (targetdir[i] == '.') {
			targetdir[i] = '/';
			targetdir[i+1] = 0;
			break;
		}
	}
	mkdir(targetdir, 0755);
	char targetfile[PATH_MAX+1];
	
	FILE *fp = fopen(argv[1], "rb");
	if (!fp) {
		return 1;
	}
	
	WadHeader hdr; fread(&hdr, sizeof(hdr), 1, fp);
	
	if (memcmp(hdr.magic, "WAD3", 4) != 0) {
		printf("Bad header: %.4s\n", hdr.magic);
		return 100;
	}
	
	printf("%d directory entries:\n", hdr.dircount);
	
	fseek(fp, hdr.diroffset, SEEK_SET);
	
	for (int i=0; i < hdr.dircount; ++i) {
		WadDirEntry entry; fread(&entry, sizeof(entry), 1, fp);
		printf("%s: %d %d\n", entry.name, entry.type, entry.fileSize);
		long pos = ftell(fp);
		fseek(fp, entry.fileOffset, SEEK_SET);
		
		if (entry.type == WadType_Tex) {
			BSPMipTex texinfo; fread(&texinfo, sizeof(texinfo), 1, fp);
			fseek(fp, entry.fileOffset+texinfo.offsets[0], SEEK_SET); // Read only mip 0
			
			long size = texinfo.width * texinfo.height;
			uint8_t *data = malloc(size);
			fread(data, size, 1, fp);
			
			fseek(fp, entry.fileOffset+texinfo.offsets[MIPLEVELS-1]+size/ipow(4, MIPLEVELS-1), SEEK_SET); // Get past last mip
			uint16_t palSize; fread(&palSize, sizeof(palSize), 1, fp);
			uint8_t *palette = malloc(palSize*3);
			fread(palette, palSize*3, 1, fp);
			// printf("%d palette entries\n", palSize);
			
			strcpy(targetfile, targetdir);
			strcat(targetfile, entry.name);
			strcat(targetfile, ".ppm");
			FILE *fd = fopen(targetfile, "wb");
			fprintf(fd, "P6 %d %d 255\n", texinfo.width, texinfo.height);
			uint8_t col[3];
			for (int n=0; n < size; ++n) {
				col[0] = palette[data[n]*3];
				col[1] = palette[data[n]*3+1];
				col[2] = palette[data[n]*3+2];
				fwrite(col, 3, 1, fd);
			}
			fclose(fd);
			free(data);
			free(palette);
		}
		
		fseek(fp, pos, SEEK_SET);
	}
	
	fclose(fp);
	
	return EXIT_SUCCESS;
}