#include "file.h"
#include "ff.h"
#include "pico/stdlib.h"
#include <algorithm>
#include <string>
#include <vector>

static std::vector<std::string> entry;
static FATFS fs;
static FIL fil;
static int index, index_next;

void file_init() {
	index_next = -1;
	DIR dir;
	FILINFO info;
	FRESULT fr;
	if (f_mount(&fs, "", 1) != FR_OK) panic("mount failed.\n");
	if (f_chdir("MOVIE") != FR_OK) panic("/MOVIE not found.\n");
	if (f_opendir(&dir, ".") != FR_OK) panic("opendir failed.\n");
	for (int i = 0; f_readdir(&dir, &info) == FR_OK && *info.fname;)
		entry.push_back(info.fname);
	f_closedir(&dir);
	sort(entry.begin(), entry.end());
}

void file_open() {
	if (index >= entry.size()) index = 0;
	printf("name: %s\n", entry[index].c_str());
	if (f_open(&fil, entry[index].c_str(), FA_READ) != FR_OK) panic("open failed.\n");
}

void file_close() {
	f_close(&fil);
	if (index_next >= 0) {
		index = index_next;
		index_next = -1;
	}
	else index++;
}

void file_read(void *buf, u32 len) {
	UINT br;
	if (f_read(&fil, buf, len, &br) != FR_OK || br != len) panic("file read error\n");
}

u32 file_tell() {
	return index_next < 0 ? f_tell(&fil) : -1;
}

void file_skip(u32 len) {
	if (f_lseek(&fil, f_tell(&fil) + len) != FR_OK) panic("file seek error\n");
}

int file_getN() {
	return entry.size();
}

int file_get_index() {
	return index;
}

void file_set_next(int _index) {
	index_next = _index;
}

const char *file_get_name() {
	return entry[index].c_str();
}
