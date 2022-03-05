#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "typeinfer.h"

hashmap *infer_load() {
	hashmap *load_map = make__hashmap(0, NULL, destroyCharKey);

	char *text_plain = malloc(sizeof(char) * 11); strcpy(text_plain, "text/plain");
	char *text_html = malloc(sizeof(char) * 10); strcpy(text_html, "text/html");
	char *text_css = malloc(sizeof(char) * 9); strcpy(text_css, "text/css");

	insert__hashmap(load_map, "txt", text_plain, "", compareCharKey, NULL);
	insert__hashmap(load_map, "html", text_html, "", compareCharKey, NULL);
	insert__hashmap(load_map, "css", text_css, "", compareCharKey, NULL);

	return load_map;
}

// reads final values from filename (after find_p) and looks in load_map
char *end_type_script_check(hashmap *load_map, char *filename, int find_p) {
	char *sub_filename = malloc(sizeof(char) * 8);
	int max_sub_filename = 8, sub_filename_index = 0;

	for (int read_file_name = find_p + 1; filename[read_file_name]; read_file_name++) {
		sub_filename[sub_filename_index] = filename[read_file_name];
		sub_filename_index++;

		if (sub_filename_index + 1 == max_sub_filename) {
			max_sub_filename *= 2;

			sub_filename = realloc(sub_filename, sizeof(char) * max_sub_filename);
		}
	}

	sub_filename[sub_filename_index] = '\0';
	char *res = (char *) get__hashmap(load_map, sub_filename, "");

	free(sub_filename);
	return res ? res : "text/html"; // default handle
}

char *content_type_infer(hashmap *load_map, char *filename, char *data, int data_length) {
	// first check the file for an ending within the system
	// find last "." and read data after that
		// if no ".", try inferring from the data within char *data

	int find_p;
	for (find_p = strlen(filename) - 1; find_p >= 0; find_p--)
		if (filename[find_p] == '.')
			break;

	if (find_p >= 0)
		return end_type_script_check(load_map, filename, find_p);

	return "text/plain"; // no value
}