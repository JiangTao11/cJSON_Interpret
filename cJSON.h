#ifndef cJSON_h
#define cJSON_h

#define cJSON_NUMBER 3
#define cJSON_STRING 4
#define cJSON_ARRAY 5
#define cJSON_OBJECT 6

typedef struct cJSON {
  struct cJSON *next, *prev;
  struct cJSON *child;

  int type;
  int valueOfInt;
  double valueOfDouble;
  char *valueOfString;

  char *name;
} cJSON;

typedef struct cJSON_Hooks {
  void *(*(malloc_fn)) (size_t sz);
  void (*free_fn)(void *ptr);
} cJSON_Hooks;

extern void cJSON_InitHooks(cJSON_Hooks *hook);
extern void cJSON_Delete(cJSON*);
extern cJSON *cJSON_parse(cJSON*, char*);
extern char *cJSON_print(cJSON*);
extern const char *getErrorPointer(void);

extern int getArraySize(cJSON*);
extern cJSON *getArrayItem(cJSON*, int);
extern cJSON *getObjectItem(cJSON*, char*);

#endif

