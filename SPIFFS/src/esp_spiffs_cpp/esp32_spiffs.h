#ifndef _INC_ESP32_SPIFFS
#define _INC_ESP32_SPIFFS

#include <iostream>
#include <string>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <dirent.h>

#define BASE_PATH "/data"
#define SPIFFS_TAG "SPIFFS"

using namespace std;

class SPIFFS {
public:
	SPIFFS();
	~SPIFFS();
	char *read_file(char *file_name);
	void write_to_file(char *file_name, char *data);
	void create_file(char *file_name);
	void delete_file(char *file_name);
	void list_all_files();
private:
	static bool is_init;
	char *file_content;
	string file_path;
	string file_path_forming(char *file_name);
	long get_file_size(char *file_name);
};

#endif