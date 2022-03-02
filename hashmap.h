#ifndef __HASH_T__
#define __HASH_T__

typedef struct ReturnHashmap { // used only for type 1
	void *payload;
	
	hashmap__response *next;
} hashmap__response;
int clear__hashmap__response(hashmap__response *hr);

typedef struct Store hashmap;

hashmap *make__hashmap(int hash__type, void (*printer)(void *), void (*destroy)(void *));

void **keys__hashmap(hashmap *hash__m, int *max_key);
void *get__hashmap(hashmap *hash__m, void *key, char *ep, ...);

int print__hashmap(hashmap *hash__m);

int delete__hashmap(hashmap *hash__m, void *key);

int deepdestroy__hashmap(hashmap *hash);

int batchInsert__hashmap(hashmap *hash__m, char *filename);
int insert__hashmap(hashmap *hash__m, void *key, void *value, ...);

// simple key type functions
void printCharKey(void *characters);
int compareCharKey(void *characters, void *otherValue);
void destroyCharKey(void *characters);

void printIntKey(void *integer);
int compareIntKey(void *integer, void *otherValue);
void destroyIntKey(void *integer);

#endif