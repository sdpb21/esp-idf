#include "esp32_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define OLD_FILE	"index.html"
#define NEW_FILE	"not_exist.txt"

extern "C" {
	void app_main();
}

void app_main() {
	SPIFFS spiffs;
	char *file_content;

	spiffs.list_all_files();
	file_content = spiffs.read_file(OLD_FILE);
	cout << file_content << endl;

	file_content = spiffs.read_file(NEW_FILE);

	if (file_content == NULL) {
		cout << "File doesn't exist\n";
		spiffs.create_file(NEW_FILE);
		spiffs.write_to_file(NEW_FILE, "Write string to a newly created file\n");
		file_content = spiffs.read_file(NEW_FILE);
		cout << file_content;

		spiffs.list_all_files();
		spiffs.delete_file(NEW_FILE);
		spiffs.list_all_files();
	}
}