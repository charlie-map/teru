#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "typeinfer.h"

typedef struct InferedLoad {
	char *data_value;
	int is_binary;
} if_t;

if_t *createInferenceLoad(char *data, int is_binary) {
	char *data_value = malloc(sizeof(char) * (strlen(data) + 1)); strcpy(data_value, data);

	if_t *new_infer_load = malloc(sizeof(if_t));

	new_infer_load->data_value = data_value;
	new_infer_load->is_binary = is_binary;

	return new_infer_load;
}

void destroyInferenceLoad(void *map) {
	free(((if_t *)map)->data_value);

	free((if_t *) map);

	return;
}

hashmap *infer_load() {
	hashmap *load_map = make__hashmap(0, NULL, destroyInferenceLoad);

	if_t *text_plain = createInferenceLoad("text/plain", 0);
	if_t *text_html = createInferenceLoad("text/html;charset=UTF-8", 0);
	if_t *text_css =createInferenceLoad("text/css", 0);
	if_t *text_javascript = createInferenceLoad("text/javascript", 0);
	if_t *image_png = createInferenceLoad("image/png", 1);
	if_t *image_jpg = createInferenceLoad("image/jpg", 1);

	insert__hashmap(load_map, "txt", text_plain, "", compareCharKey, NULL);
	insert__hashmap(load_map, "html", text_html, "", compareCharKey, NULL);
	insert__hashmap(load_map, "css", text_css, "", compareCharKey, NULL);
	insert__hashmap(load_map, "js", text_javascript, "", compareCharKey, NULL);
	insert__hashmap(load_map, "png", image_png, "", compareCharKey, NULL);
	insert__hashmap(load_map, "jpg", image_jpg, "", compareCharKey, NULL);

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
	if_t *res = (if_t *) get__hashmap(load_map, sub_filename, "");

	free(sub_filename);
	return res ? res->data_value : "text/html;charset=UTF-8"; // default handle
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

int is_binary(hashmap *load_map, char *filename) {
	int find_p;
	for (find_p = strlen(filename) - 1; find_p >= 0; find_p--)
		if (filename[find_p] == '.')
			break;

	if (find_p >= 0) {
		char *sub_string = malloc(sizeof(char) * (strlen(filename) - find_p));
		strcpy(sub_string, filename + (sizeof(char) * (find_p + 1)));

		if_t *res = (if_t *) get__hashmap(load_map, sub_string, "");

		free(sub_string);
		return res ? res->is_binary : 0;
	}

	return 0;
}