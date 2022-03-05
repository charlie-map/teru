#ifndef __HASH_T__
#define __HASH_T__

typedef struct ReturnHashmap { // used only for type 1
	void *payload;
	
	struct ReturnHashmap *next;
} hashmap__response;
int clear__hashmap__response(hashmap__response *);

typedef struct Store hashmap;

hashmap *make__hashmap(int, void (*)(void *), void (*)(void *));

void **keys__hashmap(hashmap *, int *);
void *get__hashmap(hashmap *, void *, char *, ...);

int print__hashmap(hashmap *);

int delete__hashmap(hashmap *, void *);

int deepdestroy__hashmap(hashmap *);

int batchInsert__hashmap(hashmap *, char *);
int insert__hashmap(hashmap *, void *, void *, ...);

// simple key type functions
void printCharKey(void *);
int compareCharKey(void *, void *);
void destroyCharKey(void *);

void printIntKey(void *);
int compareIntKey(void *, void *);
void destroyIntKey(void *);

#endif