#include "esp32_spiffs.h"

bool SPIFFS::is_init = false; // prevent second call for this init function

SPIFFS::SPIFFS() {
	file_content = NULL;
	if (is_init) return;
	is_init = true;

	esp_vfs_spiffs_conf_t conf = {
		.base_path = BASE_PATH,
		.partition_label = NULL,
		.max_files = 5,
		.format_if_mount_failed = true
	};

	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(SPIFFS_TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(SPIFFS_TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(SPIFFS_TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(SPIFFS_TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
	} else {
		ESP_LOGI(SPIFFS_TAG, "Partition size: total: %d, used: %d", total, used);
	}
}

SPIFFS::~SPIFFS() {
	delete[] file_content;
}

void SPIFFS::create_file(char *file_name) {
	file_path = file_path_forming(file_name);

	FILE* fp;
	fp = fopen(file_path.c_str(), "w+");
	if (!fp) {
		ESP_LOGE(SPIFFS_TAG, "Failed to open file for writing");
	} else {
		cout << "File create successfully\n";
		fclose(fp);
	}
}

char* SPIFFS::read_file(char *file_name) {
	FILE *fp;
	if (file_content) {
		delete[] file_content; // Delete file_content everytime reading a new file
		file_content = NULL;
	}

	long file_sz = get_file_size(file_name);
	if (file_sz == -1) {
		cout << "File " << file_name << " not exist\n";
		return NULL;
	}
	file_path = file_path_forming(file_name);
	fp = fopen(file_path.c_str(), "r");

	if (fp) {
		file_content = new char[file_sz + 1]; // +1 for null terminator
		fread(file_content, 1, file_sz, fp);
		file_content[file_sz] = '\0';
		fclose(fp);
	} else {
		file_content = new char[30];
		strcpy(file_content, "File not found");
	}
	return file_content;
}

void SPIFFS::write_to_file(char *file_name, char *data) {
	file_path = file_path_forming(file_name);

	FILE *fp;
	fp = fopen(file_path.c_str(), "w+");
	if (!fp) {
		cout << "Unable to open file\n";
	} else {
		fputs(data, fp);
		fclose(fp);
	}
}

void SPIFFS::delete_file(char *file_name) {
	file_path = file_path_forming(file_name);

	if (!remove(file_path.c_str())) cout << "Delete file " << file_name << " successfully\n";
	else cout << "Unable to delete file " << file_name << endl;
}

void SPIFFS::list_all_files() {
	DIR *d;
	struct dirent *dir;
	d = opendir(BASE_PATH);

	if (d) {
		while ((dir = readdir(d)) != NULL) {
			cout << dir->d_name << " ";
		}
		cout << endl;
		closedir(d);
	} else cout << "There is no file inside " << BASE_PATH << endl;
}

long SPIFFS::get_file_size(char *file_name) {
	FILE *fp;

	file_path = file_path_forming(file_name);
	long file_size;

	fp = fopen(file_path.c_str(), "r");

	if (fp) {
		fseek(fp, 0L, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		fclose(fp);
	} else {
		file_size = -1; // File not exist
	}
	return file_size;
}

string SPIFFS::file_path_forming(char *file_name) {
	return std::string(BASE_PATH) + "/" + file_name;
}