#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashmap.h"

/*
	vtableKeyStore is used for using unknown type keys in the 
	hashmap. Because the variable type is unknown, the use
	of this struct plus the function (some function name)__hashmap() can
	be used to decipher any type of possible key in the hashmap

	SEE bottom of the page for the function declarations and uses
*/
typedef struct VTable {
	void *key;

	// these define how we utilize the key
	// DEFAULT behavior is using a char * setup
	void (*printKey)(void *);
	int (*compareKey)(void *, void *);
	void (*destroyKey)(void *);
} vtableKeyStore;

typedef struct ll_def {
	struct ll_def *next;
	
	vtableKeyStore key;
	int max__arrLength, arrIndex; // only for hash type 1
	int isArray;
	void *ll_meat; // single value pointer
				   // for hash__type = 0
				   // or array for
				   // hash__type = 1
} ll_main_t;

// will store a single hashmap
struct Store {
	int hashmap__size;
	// takes a type of hashmap
	// 0: replace value
	// 1: append to growing value list
	int hash__type;
	ll_main_t **map;

	// used for printing the hashmap values
	void (*printer)(void *);
	// destroying hashmap values
	void (*destroy)(void *);
};

const int MAX_BUCKET_SIZE = 5;
const int START_SIZE = 1023; // how many initial buckets in array

unsigned long hash(unsigned char *str) {
	unsigned long hash = 5381;
	int c;

	while ((c = *str++) != 0)
		hash = ((hash << 5) + hash) + c;

	return hash;
}


// define some linked list functions (see bottom of file for function write outs):
ll_main_t *ll_makeNode(vtableKeyStore key, void *value, int hash__type);
int ll_insert(ll_main_t *node, vtableKeyStore key, void *payload, int hash__type, void (*destroy)(void *));

ll_main_t *ll_next(ll_main_t *curr);

int ll_print(ll_main_t *curr, void (*printer)(void *));

int ll_isolate(ll_main_t *node);
int ll_destroy(ll_main_t *node, void (destroyObjectPayload)(void *));


hashmap *make__hashmap(int hash__type, void (*printer)(void *), void (*destroy)(void *)) {
	hashmap *newMap = (hashmap *) malloc(sizeof(hashmap));

	newMap->hash__type = hash__type;
	newMap->hashmap__size = START_SIZE;

	// define needed input functions
	newMap->printer = printer;
	newMap->destroy = destroy;

	newMap->map = (ll_main_t **) malloc(sizeof(ll_main_t *) * newMap->hashmap__size);

	for (int i = 0; i < newMap->hashmap__size; i++) {
		newMap->map[i] = NULL;
	}

	return newMap;
}

// take a previous hashmap and double the size of the array
// this means we have to take all the inputs and calculate
// a new position in the new array size
// within hash__m->map we can access each of the linked list pointer
// and redirect them since we store the keys
int re__hashmap(hashmap *hash__m) {
	// define the sizes / updates to size:
	int old__mapLength = hash__m->hashmap__size;
	int new__mapLength = hash__m->hashmap__size * 2;

	// create new map (twice the size of old map (AKA hash__m->map))
	ll_main_t **new__map = (ll_main_t **) malloc(sizeof(ll_main_t *) * new__mapLength);

	for (int set__newMapNulls = 0; set__newMapNulls < new__mapLength; set__newMapNulls++)
		new__map[set__newMapNulls] = NULL;

	int new__mapPos = 0;

	for (int old__mapPos = 0; old__mapPos < old__mapLength; old__mapPos++) {
		// look at each bucket
		// if there is contents
		while (hash__m->map[old__mapPos]) { // need to look at each linked node
			// recalculate hash
			new__mapPos = hash(hash__m->map[old__mapPos]->key.key) % new__mapLength;

			// store the node in temporary storage
			ll_main_t *currNode = hash__m->map[old__mapPos];

			// extract currNode from old map (hash__m->map)
			hash__m->map[old__mapPos] = ll_next(currNode); // advance root
			ll_isolate(currNode); // isolate old root

			// defines the linked list head in the new map
			ll_main_t *new__mapBucketPtr = new__map[new__mapPos];

			// if there is one, we have to go to the tail
			if (new__mapBucketPtr) {

				while (new__mapBucketPtr->next) new__mapBucketPtr = ll_next(new__mapBucketPtr);

				new__mapBucketPtr->next = currNode;
			} else
				new__map[new__mapPos] = currNode;
		}
	}

	free(hash__m->map);
	hash__m->map = new__map;
	hash__m->hashmap__size = new__mapLength;

	return 0;
}

int METAinsert__hashmap(hashmap *hash__m, vtableKeyStore key, void *value) {
	int mapPos = hash(key.key) % hash__m->hashmap__size;
	int bucketLength = 0; // counts size of the bucket at mapPos

	// see if there is already a bucket defined at mapPos
	if (hash__m->map[mapPos])
		bucketLength = ll_insert(hash__m->map[mapPos], key, value, hash__m->hash__type, hash__m->destroy);
	else
		hash__m->map[mapPos] = ll_makeNode(key, value, hash__m->hash__type);

	// if bucketLength is greater than an arbitrary amount,
	// need to grow the size of the hashmap (doubling)
	if (bucketLength >= MAX_BUCKET_SIZE)
		re__hashmap(hash__m);

	return 0;
}

int ll_get_keys(ll_main_t *ll_node, void **keys, int *max_key, int key_index) {
	while(ll_node) {
		keys[key_index++] = ll_node->key.key;

		if (key_index == *max_key) { // resize
			*max_key *= 2;

			keys = realloc(keys, sizeof(void *) * *max_key);
		}

		ll_node = ll_node->next;
	}

	return key_index;
}

// creates an array of all keys in the hash map
void **keys__hashmap(hashmap *hash__m, int *max_key) {
	int key_index = 0;
	*max_key = 16;
	void **keys = malloc(sizeof(void *) * *max_key);

	for (int find_keys = 0; find_keys < hash__m->hashmap__size; find_keys++) {
		if (hash__m->map[find_keys]) {
			// search LL:
			key_index = ll_get_keys(hash__m->map[find_keys], keys, max_key, key_index);
		}
	}

	*max_key = key_index;

	return keys;
}

/*
	get__hashmap search through a bucket for the inputted key
	the response varies widely based on hash__type

	-- for all of them: NULL is return if the key is not found

		TYPE 0:
			Returns the single value that is found
		TYPE 1:
			Returns a pointer to a struct (hashmap__response) with an array that can be
			searched through and a length of said array.
			This array should be used with caution because accidental
			free()ing of this array will leave the key to that array
			pointing to unknown memory. However, the freeing of the
			returned struct will be left to the user
	-- TERU update: update for "wildcarding" the get request
		use "i" to implement this and an
			int is_match(void *, void *);
		to compare keys
		leave blank for no is_match option

		use "w" to implement a weight function
			int is_lower(void *, void *);
		to compare values
		leave blank for no is_lower option
*/
void *get__hashmap(hashmap *hash__m, void *key, char *ep, ...) {
	if (!hash__m)
		return NULL;

	int (*is_match)(void *, void *) = NULL;
	int (*is_lower)(void *, void *) = NULL;

	va_list new_poly_reader;
	va_start(new_poly_reader, ep);

	for (int check_ep = 0; ep[check_ep]; check_ep++) {
		if (ep[check_ep] == 'w') // found match
			is_lower = va_arg(new_poly_reader, int (*)(void *, void *));
		if (ep[check_ep] == 'i') // found match
			is_match = va_arg(new_poly_reader, int (*)(void *, void *));
	}

	// get hash position
	int mapPos;

	for (mapPos = is_match ? 0 : hash(key) % hash__m->hashmap__size; mapPos < hash__m->hashmap__size; mapPos++) {
		ll_main_t *ll_search = hash__m->map[mapPos];
		
		// search through the bucket to find any keys that match
		while (ll_search) {
			if ((is_match && is_match(ll_search->key.key, key)) || ll_search->key.compareKey(ll_search->key.key, key)) { // found a match

				// depending on the type and mode, this will just return
				// the value:
				if (hash__m->hash__type == 0)
					return ll_search->ll_meat;
				else {
					hashmap__response *returnMeat = malloc(sizeof(hashmap__response));

					if (ll_search->isArray) {
						returnMeat->payload = ((void **) ll_search->ll_meat)[0];
						returnMeat->next = NULL;

						hashmap__response *set_intern_meat = returnMeat;

						for (int set_return_meat = 1; set_return_meat < ll_search->arrIndex + 1; set_return_meat++) {
							// if is_lower exists, find position in return:
							if (is_lower) {
								set_intern_meat = returnMeat;

								// search for the position of the next value based on is_lower
								while (set_intern_meat->next && is_lower(set_intern_meat->next->payload, ((void **) ll_search->ll_meat)[set_return_meat])) {
									set_intern_meat = set_intern_meat->next;
								}

								hashmap__response *new_node_element = malloc(sizeof(hashmap__response));
								new_node_element->payload = ((void **) ll_search->ll_meat)[set_return_meat];

								// check for setting as head
								if (is_lower(((void **) ll_search->ll_meat)[set_return_meat], set_intern_meat->payload)) {
									returnMeat->next = returnMeat;
									returnMeat = new_node_element;
								} else { // insert as next of set_intern_meat
									hashmap__response *curr_next = set_intern_meat->next;

									// splicing ll_meat value into the linked list
									set_intern_meat->next = new_node_element;
									new_node_element->next = curr_next;
								}

								continue;
							}

							// otherwise add the value at the end of the linked list
							set_intern_meat->next = malloc(sizeof(hashmap__response));
							set_intern_meat = set_intern_meat->next;

							set_intern_meat->payload = ((void **) ll_search->ll_meat)[set_return_meat];
						}
					} else { // define array
						void *ll_tempMeatStorage = ll_search->ll_meat;

						ll_search->max__arrLength = 2;
						ll_search->arrIndex = 0;

						ll_search->ll_meat = malloc(sizeof(void *) * ll_search->max__arrLength * 2);
						((void **) ll_search->ll_meat)[0] = ll_tempMeatStorage;

						returnMeat->payload = ll_search->ll_meat;
						returnMeat->next = NULL;

						ll_search->isArray = 1;
					}

					return returnMeat;
				}
			}

			ll_search = ll_next(ll_search);
		}

		if (!is_match)
			break;
	}

	// no key found
	return NULL;
}

// scroll through linked list and delete headers
int clear__hashmap__response(hashmap__response *hr) {
	while (hr) {
		hashmap__response *n = hr->next;
		free(hr);

		hr = n;
	}

	return 0;
}

int print__hashmap(hashmap *hash__m) {
	for (int i = 0; i < hash__m->hashmap__size; i++) {
		if (hash__m->map[i]) {
			printf("Linked list at index %d ", i);
			ll_print(hash__m->map[i], hash__m->printer);
			printf("\n");
		}
	}

	return 0;
}

// uses the same process as get__hashmap, but deletes the result
// instead. Unfortunately, the get__hashmap function cannot be
// utilized in this context because when the linked list node
// is being extracted, we need to know what the parent of
// the node is
int delete__hashmap(hashmap *hash__m, void *key) {
	// get hash position
	int mapPos = hash(key) % hash__m->hashmap__size;

	ll_main_t *ll_parent = hash__m->map[mapPos];
	ll_main_t *ll_search = ll_next(ll_parent);

	// check parent then move into children nodes in linked list
	if (ll_parent->key.compareKey(ll_parent->key.key, key)) {
		// extract parent from the hashmap:
		hash__m->map[mapPos] = ll_search;

		ll_destroy(ll_parent, hash__m->destroy);

		return 0;
	}

	// search through the bucket to find any keys that match
	while (ll_search) {
		if (ll_search->key.compareKey(ll_search->key.key, key)) { // found a match

			// we can then delete the key using the same approach as above
			// extract the key from the linked list
			ll_parent->next = ll_next(ll_search);

			ll_destroy(ll_search, hash__m->destroy);

			return 0;
		}

		ll_parent = ll_search;
		ll_search = ll_next(ll_search);
	}

	return 0;
}

int deepdestroy__hashmap(hashmap *hash) {
	// destroy linked list children
	for (int i = 0; i < hash->hashmap__size; i++) {
		if (hash->map[i]) {
			ll_destroy(hash->map[i], hash->destroy);
		}
	}

	// destroy map
	free(hash->map);
	free(hash);

	return 0;
}


ll_main_t *ll_makeNode(vtableKeyStore key, void *newValue, int hash__type) {
	ll_main_t *new__node = (ll_main_t *) malloc(sizeof(ll_main_t));

	new__node->isArray = 0;
	new__node->next = NULL;
	new__node->key = key;
	new__node->ll_meat = newValue;

	return new__node;
}

/*
	for hash__type = 0
		takes a linked list node value ptr
		and replaces the value with
		updated value
*/
void *ll_specialUpdateIgnore(void *ll_oldVal, void *newValue, void (*destroy)(void *)) {
	// clean up previous info at this pointer
	destroy(ll_oldVal);

	// update
	return newValue;
}

// takes the ll_pointer->ll_meat and doubles
// size of current array
int ll_resizeArray(ll_main_t *ll_pointer) {
	// declare new array
	void **new__arrayPtr = malloc(sizeof(void *) * ll_pointer->max__arrLength * 2);

	// copy values over
	for (int copyVal = 0; copyVal < ll_pointer->max__arrLength; copyVal++) {
		new__arrayPtr[copyVal] = (void *) ((void **) ll_pointer->ll_meat)[copyVal];
	}

	// free old array pointer
	free(ll_pointer->ll_meat);

	// update to new meat
	ll_pointer->ll_meat = new__arrayPtr;

	return 0;
}

/*
	for hash__type = 1
		takes linked list pointer
		- first makes sure that ll_pointer->ll_meat
		is an array, if not it creates and array
		- then appends value to end of array
*/
int ll_specialUpdateArray(ll_main_t *ll_pointer, void *newValue) {
	// The same "leave key be" for update works here as with ignore

	if (!ll_pointer->isArray) { // need to create an array
		void *ll_tempMeatStorage = ll_pointer->ll_meat;

		// update settings for this pointer
		ll_pointer->max__arrLength = 8;
		ll_pointer->arrIndex = 0;
		ll_pointer->isArray = 1;

		ll_pointer->ll_meat = (void **) malloc(sizeof(void *) * ll_pointer->max__arrLength);
		((void **) (ll_pointer->ll_meat))[0] = ll_tempMeatStorage;

		for (int memSet = 1; memSet < ll_pointer->arrIndex; memSet++)
			((void **) (ll_pointer->ll_meat))[memSet] = NULL;
	}

	// add new value
	ll_pointer->arrIndex++;
	((void **) (ll_pointer->ll_meat))[ll_pointer->arrIndex] = newValue;

	if (ll_pointer->arrIndex == ll_pointer->max__arrLength - 1)
		ll_resizeArray(ll_pointer);

	return 0;
}

// finds the tail and appends
int ll_insert(ll_main_t *crawler__node, vtableKeyStore key, void *newValue, int hash__type, void (*destroy)(void *)) {

	int bucket_size = 1, addedPayload = 0;

	// search through the entire bucket
	// (each node in this linked list)
	while (crawler__node->next) {
		// found a duplicate (only matters
		// for hash__type == 0 or 1)
		if (crawler__node->key.compareKey(crawler__node->key.key, key.key)) {
			if (hash__type == 0) {
				crawler__node->ll_meat = ll_specialUpdateIgnore(crawler__node->ll_meat, newValue, destroy);
				addedPayload = 1;
			} else if (hash__type == 1) {
				ll_specialUpdateArray(crawler__node, newValue);
				addedPayload = 1;
			}
		}

		crawler__node = ll_next(crawler__node);
		bucket_size++;
	}

	if (crawler__node->key.compareKey(crawler__node->key.key, key.key)) {
		if (hash__type == 0) {
			crawler__node->ll_meat = ll_specialUpdateIgnore(crawler__node->ll_meat, newValue, destroy);
			addedPayload = 1;
		} else if (hash__type == 1) {
			ll_specialUpdateArray(crawler__node, newValue);
			addedPayload = 1;
		}
	}

	if (addedPayload == 0) {
		crawler__node->next = ll_makeNode(key, newValue, hash__type);
	}

	// return same head
	return bucket_size;
}

ll_main_t *ll_next(ll_main_t *curr) {
	return curr->next;
}

int ll_printNodeArray(ll_main_t *curr, void (*printer)(void *)) {
	for (int printVal = 0; printVal < curr->arrIndex + 1; printVal++) {
		printer(((void **) curr->ll_meat)[printVal]);
	}

	return 0;
}

int ll_print(ll_main_t *curr, void (*printer)(void *)) {
	printf("\n\tLL node ");
	//printVoid()
	curr->key.printKey(curr->key.key);
	printf(" with payload(s):\n");
	if (curr->isArray)
		ll_printNodeArray(curr, printer);
	else
		printer(curr->ll_meat);

	while ((curr = ll_next(curr)) != NULL) {
		printf("\tLL node ");
		printf(" with payload(s):\n");
		if (curr->isArray)
			ll_printNodeArray(curr, printer);
		else
			printer(curr->ll_meat);
	}

	return 0;
}

int ll_isolate(ll_main_t *node) {
	node->next = NULL;

	return 0;
}

int ll_destroy(ll_main_t *node, void (destroyObjectPayload)(void *)) {
	ll_main_t *node_nextStore;

	do {
		if (node->key.destroyKey)
			node->key.destroyKey(node->key.key);

		if (node->isArray) {
			if (destroyObjectPayload)
				for (int destroyVal = 0; destroyVal < node->arrIndex + 1; destroyVal++)
					destroyObjectPayload(((void **)node->ll_meat)[destroyVal]);

			free(node->ll_meat);
		} else if (destroyObjectPayload)
			destroyObjectPayload(node->ll_meat);

		node_nextStore = node->next;
		free(node);
	} while ((node = node_nextStore) != NULL);

	return 0;
}


// DEFAULT function declarations
void printCharKey(void *characters) {
	printf("%s", (char *) characters);
}
int compareCharKey(void *characters, void *otherValue) {
	return strcmp((char *) characters, (char *) otherValue) == 0;
}
void destroyCharKey(void *characters) { free(characters); }

void printIntKey(void *integer) {
	printf("%d", *((int *) integer));
}
int compareIntKey(void *integer, void *otherValue) {
	return *((int *) integer) == *((int *) otherValue);
}
void destroyIntKey(void *integer) { /* We can't free that! */ }


int insert__hashmap(hashmap *hash__m, void *key, void *value, ...) {
	va_list ap;
	vtableKeyStore inserter = { .key = key };

	va_start(ap, value);
	// could be param definer or the printKey function
	void *firstArgumentCheck = va_arg(ap, char *);

	// check for DEFAULT ("-d") or INT ("-i"):
	if (strcmp((char *) firstArgumentCheck, "-d") == 0) {// use default
		inserter.printKey = printCharKey;
		inserter.compareKey = compareCharKey;
		inserter.destroyKey = NULL;
	} else if (strcmp((char *) firstArgumentCheck, "-i") == 0) {
		inserter.printKey = printIntKey;
		inserter.compareKey = compareIntKey;
		inserter.destroyKey = NULL;
	} else {
		inserter.printKey = firstArgumentCheck;
		// do the same for compareKey 
		inserter.compareKey = va_arg(ap, int (*)(void *, void *));

		inserter.destroyKey = va_arg(ap, void (*)(void *));
		// if destroy is NULL, we don't want to add since this by DEFAULT
		// does no exist
	}	

	METAinsert__hashmap(hash__m, inserter, value);

	return 0;
}

char *read_key(char *buffer, int *key_len) {
	*key_len = 8;
	int key_index = 0;

	char *key = malloc(sizeof(char) * *key_len);

	while (buffer[key_index] != '|') {
		key[key_index] = buffer[key_index];
		key_index++;

		if (key_index + 1 == *key_len) {
			*key_len *= 2;

			key = realloc(key, sizeof(char) * *key_len);
		}

		key[key_index] = '\0';
	}

	*key_len = key_index;
	return key;
}
// assumes the pattern:
// name|Charlie\nname|Jack
// NOTE: hashmap should have a destroyValue function
int batchInsert__hashmap(hashmap *hash__m, char *filename) {
	void (*printKey)(void *) = printCharKey;
	int (*compareKey)(void *, void *) = compareCharKey;
	void (*destroyKey)(void *) = destroyCharKey;

	FILE *fp = fopen(filename, "r");

	if (!fp) {
		printf("\033[0;31m");
		printf("\n** Batch Insert File Error **\n");
		printf("\033[0;37m");

		return 1;
	}

	size_t buff_size = sizeof(char) * 8;
	char *buffer = malloc(buff_size);
	int buffer_len;

	int *key_len = malloc(sizeof(int));
	while ((buffer_len = getline(&buffer, &buff_size, fp)) != -1) {
		buffer_len = strlen(buffer);
		vtableKeyStore newtable = { .key = read_key(buffer, key_len),
									.printKey = printKey,
									.compareKey = compareKey,
									.destroyKey = destroyKey };

		// read value starting from where key_len stopped
		char *value = malloc(sizeof(char) * (buffer_len - *key_len - 1));
		int cp_value;
		for (cp_value = 0; cp_value < buffer_len - *key_len - 2; cp_value++) {
			value[cp_value] = *(buffer + sizeof(char) * (*key_len + cp_value + 1));
		}
		value[cp_value] = '\0';

		METAinsert__hashmap(hash__m, newtable, value);
	}

	free(buffer);
	free(key_len);
	fclose(fp);

	return 0;
}