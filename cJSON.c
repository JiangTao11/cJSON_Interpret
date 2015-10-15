#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "cJSON.h"

static const char *ep;
const char *getErrorPointer(void)
{
  return ep;
}

static void *(*cJSON_malloc) (size_t sz) = malloc;
static void (*cJSON_free) (void *ptr) = free;

void cJSON_InitHooks(cJSON_Hooks* hook)
{
  if (!hook) {
    hook = malloc(sizeof(cJSON_Hooks));
    hook->malloc_fn = malloc;
    hook->free_fn = free;
  }
    
  cJSON_malloc = !hook->malloc_fn ? malloc : hook->malloc_fn;
  cJSON_free = !hook->free_fn ? free : hook->free_fn;
}

static cJSON *cJSON_New_Item(void)
{
  cJSON *item = (cJSON *)cJSON_malloc(sizeof(cJSON));
  if (item)
    memset(item, 0, sizeof(cJSON));
  return item;
}

void cJSON_Delete(cJSON *root)
{
  cJSON *next;
  while (root) {
    next = root->next;
    if (root->name)
      cJSON_free(root->name);
    if (root->type == cJSON_STRING)
      cJSON_free(root->valueOfString);
    if (root->child)
      cJSON_Delete(root->child);
    cJSON_free(root);
    root = next;
  }
}

static const char *parse_number(cJSON *item, const char *num)
{
  if (!num)
    return 0;
  
  int sign = 1, scale = 0;
  double value = 0;
  int sub_sign = 1, sub_scale = 0;
  if (*num == '-') {
    sign = -1;
    ++num;
  }
  if (*num >= '1' && *num <= '9') {
    value = value * 10 + (*num++ - '0');
    while (*num >= '0' && *num <= '9')
      value = value * 10 + (*num++ - '0');
  }
  if (*num == '0')
    ++num;
  
  if (*num == '.') {
    ++num;
    while (*num >= '0' && *num <= '9') {
      value = value * 10 + (*num++ - '0');
      --scale;
    }
  }
  
  if (*num == 'e' || *num == 'E') {
    ++num;
    if (*num == '+' || *num == '-') {
      sub_sign = *num == '+' ? 1 : -1;
      ++num;
    }
    while (*num >= '0' && *num <= '9')
      sub_scale = sub_scale * 10 + (*num++ - '0');
  }

  value = sign * value * pow(10.0, (scale + sub_sign * sub_scale));    // value = +/- number.fraction * 10^ +/- exponent
  item->type = cJSON_NUMBER;
  item->valueOfDouble = value;
  item->valueOfInt = (int)value;
  return num;
}
  
static const char *parse_string(cJSON *item, const char *string_ptr)
{
  
  if (*string_ptr != '\"') {
    ep = string_ptr;
    return 0;
  }
  const char *travel_ptr = string_ptr + 1;
  int len = 0;
  while (*travel_ptr != '\"') {
    if (*travel_ptr == '\\') {
      travel_ptr += 2;
      ++len;
    }
    else
      ++len, ++travel_ptr;
  }
  item->valueOfString = (char *) cJSON_malloc(len+1);
  
  char *valString = item->valueOfString;
  if (!valString) 
    return 0;
 
  ++string_ptr;
  while (*string_ptr != '\"') {
    if (*string_ptr == '\\') {
      ++string_ptr;
      switch(*string_ptr) {
        case 'b': *valString++ = '\b'; break;
        case 'f': *valString++ = '\f'; break;
        case 'n': *valString++ = '\n'; break;
        case 'r': *valString++ = '\r'; break;
        case 't': *valString++ = '\t'; break;
        case '\"': *valString++ = '\"'; break;
        case '\\': *valString++ = '\\'; break;
      }
      ++string_ptr;
    }
    else
      *valString++ = *string_ptr++;
  }
  *valString = '\0';
  item->type = cJSON_STRING;
  ++string_ptr;
  return string_ptr; 
}

// Predeclare these prototypes.
static const char *parse_value(cJSON*, const char*);
static const char *parse_array(cJSON*, const char*);
static const char *parse_object(cJSON*, const char*);
static char *print_value(cJSON*);
static char *print_array(cJSON*);
static char *print_object(cJSON*);

cJSON *cJSON_parse(cJSON *item, char *input)
{
  item = cJSON_New_Item();
  parse_value(item, input);
  return item;
}

static const char *parse_value(cJSON *item, const char *val)
{
  if (*val == '\"') 
    return parse_string(item, val);
  else if (*val == '-' || (*val >= '0' && *val <= '9'))
    return parse_number(item, val);
  else if (*val == '[')
    return parse_array(item, val);
  else if (*val == '{')
    return parse_object(item, val);
  else {
    ep = val;
    return 0;
  }
}

static const char *skip_whites(const char *ptr) 
{
  while (ptr && *ptr && (unsigned char)*ptr <= 32)
    ++ptr;
  return ptr;
}
  

static const char *parse_array(cJSON *root, const char *arr)
{
  if (*arr != '[') {
    ep = arr;
    return 0;
  }
  
  arr = skip_whites(arr + 1);
  if (*arr == ']')
    return ++arr;
  
  cJSON *child;
  root->child = child = cJSON_New_Item();
  if (!child)
    return 0;
  arr = parse_value(child, arr);
  arr = skip_whites(arr);
  while (*arr == ',') {
    arr = skip_whites(arr + 1);
    cJSON *next;
    child->next = next = cJSON_New_Item();
    arr = parse_value(next, arr);
    next->prev = child;
    child = child->next;
    arr = skip_whites(arr);
  }
  if (*arr == ']')
    ++arr;
  root->type = cJSON_ARRAY;
  return arr;
}
    
static const char *parse_object(cJSON *item, const char *obj)
{
  if (*obj != '{') {
    ep = obj;
    return 0;
  }
  obj = skip_whites(obj + 1);
  if (*obj == '}') 
    return obj + 1;
  
  item->type = cJSON_OBJECT;
  cJSON *child;
  item->child = child = cJSON_New_Item();
  if (!child)
    return 0;
  obj = skip_whites(parse_string(child, obj));
  child->name = child->valueOfString;
  child->valueOfString = 0;
  if (*obj != ':') {
    ep = obj;
    return 0;
  }
  obj = skip_whites(parse_value(child, skip_whites(obj + 1)));
  
  while (*obj == ',') {
    cJSON *next;
    child->next = next = cJSON_New_Item();
    if (!next)
      return 0;
    next->prev = child;
    obj = skip_whites(parse_string(next, skip_whites(obj + 1)));
    if (*obj != ':') {
      ep = obj;
      return 0;
    }
    next->name = next->valueOfString;
    next->valueOfString = 0;
    obj = skip_whites(parse_value(next, skip_whites(obj + 1)));
    child = child->next;
  }
  
  if (*obj != '}') {
    ep = obj;
    return 0;
  }
  return obj + 1;
}

static char *print_number(int numInt, double numDouble)
{
  char *str = 0;
  if (fabs(numDouble - (double)numInt) < DBL_EPSILON) {
    str = (char *)cJSON_malloc(21);   // 2^64 - 1 can be represented in 21 chars(including '\0')
    if (str)
      sprintf(str, "%d", numInt);
  }
  else {
    str = (char *)cJSON_malloc(64);   // this is a nice tradeoff
    if (str) {
      if (fabs(numDouble) > 1.0e9 || fabs(numDouble) < 1.0e-6)
        sprintf(str, "%e", numDouble);
      else
        sprintf(str, "%f", numDouble);
    }
  }
  return str;
}

unsigned int cJSON_get_str_memLength(const char *p)  // get the memory length of cJSON string
{
  unsigned int length = 0;
  while (p && *p) {
    if (*p == '\\')
      p += 2;
    else 
      ++p;
    ++length;
  }
  return length;
}

static char *print_string(char *str)
{
  unsigned int length = cJSON_get_str_memLength(str);
  unsigned int actural_length = length + 3;    //  we need add '\"' twice and '\0'.
  char *new_str;
  new_str = (char *)malloc(actural_length);
  char *str_start = 0;
  if (new_str) {
    str_start = new_str;
    *new_str++ = '\"';
    while (str && *str) {
      if (*str != '\\')
        *new_str++ = *str++;
      else {
        switch (*(++str)) {
          case 'b': *new_str++ = '\b'; break;
          case 'f': *new_str++ = '\f'; break;
          case 'n': *new_str++ = '\n'; break;
          case 'r': *new_str++ = '\r'; break;
          case 't': *new_str++ = '\t'; break;
          case '\"': *new_str++ = '\"'; break;
          case '\\': *new_str++ = '\\'; break;
        }
        ++str;
      }
    }
    *new_str++ = '\"';
    *new_str = '\0';
  }
  return str_start;
}

static char *print_value(cJSON *item)
{
  char *str;
  switch(item->type) {
    case cJSON_NUMBER: str = print_number(item->valueOfInt, item->valueOfDouble); break;
    case cJSON_STRING: str = print_string(item->valueOfString); break;
    case cJSON_ARRAY: str = print_array(item); break;
    case cJSON_OBJECT: str = print_object(item); break;
  }
  return str;
}

static unsigned int getArrayOrObjectSize(cJSON *root)
{
  unsigned int lenOfEntry = 0;
  if (!root)
    return 0;
  cJSON *child = root->child;
  while (child) {
    child = child->next;
    ++lenOfEntry;
  }
  return lenOfEntry;
}

static char **cJSON_initialize_entry(unsigned int entryNums)
{
  char **entryOfItems = (char **)malloc(entryNums * sizeof(char *));
  if (entryOfItems)
    memset(entryOfItems, 0, sizeof(char *));
  return entryOfItems;
}

static char *cJSON_create_str(unsigned int lenOfStr, unsigned int lenOfEntry, char **entryOfItems, int type)   // if type == 0, is '[', else '{'
{
  char front, end, separator;
  if (type == 0) {
    front = '[';
    end = ']';
    separator = ',';
  }
  else {
    front = '{';
    end = '}';
    separator = '\n';
  }
  char *str = (char *)malloc(lenOfStr);
  char *str_start = str;
  if (str) {
    *str++ = front;
    for (unsigned int i = 0; i < lenOfEntry; ++i) {
      unsigned int lenOfItem = strlen(*(entryOfItems + i));
      memcpy(str, *(entryOfItems + i), lenOfItem);
      cJSON_free(*(entryOfItems + i));
      str += lenOfItem;
      if (i == lenOfEntry - 1)   // handle the last item
        break;
      *str++ = separator;
      *str++ = ' ';
    }
    *str++ = end;
    *str = '\0';
  }
  return str_start;
}

static char *print_array(cJSON *root)
{
  unsigned int lenOfEntry = getArrayOrObjectSize(root);
  char **entryOfItems = cJSON_initialize_entry(lenOfEntry);

  cJSON *child = root->child;
  unsigned int lenOfStr = 1;  // 1 for '['
  for (unsigned int i = 0; i < lenOfEntry; ++i) {
    *(entryOfItems + i) = print_value(child);
    lenOfStr += strlen(*(entryOfItems + i)) + 2;   // 2 for ',' and ' '
    child = child->next;
  }

  return cJSON_create_str(lenOfStr, lenOfEntry, entryOfItems, 0);  // 0 indicates array
}

static char *print_object(cJSON *root)
{
  unsigned int lenOfEntry = getArrayOrObjectSize(root);
  char **entryOfItems = cJSON_initialize_entry(lenOfEntry);

  cJSON *child = root->child;
  unsigned int lenOfStr = 1;  // 1 for '{'
  for (unsigned int i = 0; i < lenOfEntry; ++i) {
    char *obj_name = print_string(child->name);
    char *obj_value = print_value(child);
    unsigned int nameLen = strlen(obj_name);
    unsigned int valueLen = strlen(obj_value);
    *(entryOfItems + i) = (char *)malloc(nameLen + 2 + valueLen + 1);   // 2 for ':', ' ' and 1 for '\0'
    if (*(entryOfItems + i)) {
      memcpy(*(entryOfItems + i), obj_name, nameLen);
      *(*(entryOfItems + i) + nameLen) = ':';
      *(*(entryOfItems + i) + nameLen + 1) = ' ';
      memcpy(*(entryOfItems + i) + nameLen + 2, obj_value, valueLen + 1);
      lenOfStr += strlen(*(entryOfItems + i)) + 2;     // 2 for ',' and ' '
      child = child->next;
    }
    cJSON_free(obj_name);
    cJSON_free(obj_value);
  }
  return cJSON_create_str(lenOfStr, lenOfEntry, entryOfItems, 1);  // 1 indicates object
}

char *cJSON_print(cJSON *item)
{
  return print_value(item);
}

int getArraySize(cJSON *root)
{
  return getArrayOrObjectSize(root);
}

cJSON *getArrayItem(cJSON *root, int index)
{
  cJSON *child = root->child;
  int count = 0;
  while (count < index) {
    child = child->next;
    ++count;
  }
  return child;
}

cJSON *getObjectItem(cJSON *root, char *name)
{
  cJSON *child = root->child;
  while (child) {
    if (strcmp(child->name, name))
      return child;
    child = child->next;
  }
  return 0;
}

/* tests for some functions */

/*
void test_parse_number(void)
{
  cJSON *item = cJSON_New_Item();
  char *n1 = "1";
  char *n2 = "123";
  char *n3 = "1.23";
  char *n4 = "0.23";
  char *n5 = "1e5";
  char *n6 = "2E8";
  char *n7 = "1.1e2";
  parse_number(item, n1);
  printf("%d\n", item->valueOfInt);
  parse_number(item, n2);
  printf("%d\n", item->valueOfInt);
  parse_number(item, n3);
  printf("%f\n", item->valueOfDouble);
  parse_number(item, n4);
  printf("%f\n", item->valueOfDouble);
  parse_number(item, n5);
  printf("%f\n", item->valueOfDouble);
  parse_number(item, n6);
  printf("%f\n", item->valueOfDouble);
  parse_number(item, n7);
  printf("%f\n", item->valueOfDouble);
  cJSON_Delete(item);
}

void test_parse_string(void)
{
  cJSON *item = cJSON_New_Item();
  char *s1 = "\"loucq\"";
  char *s2 = "\"loucq\n\"";
  char *s3 = "\"\thello\"";
  char *s4 = "\"\\\\\"";
  parse_string(item, s1);
  printf("%s\n", item->valueOfString);
  parse_string(item, s2);
  printf("%s", item->valueOfString);
  parse_string(item, s3);
  printf("%s\n", item->valueOfString);
  printf("%s\n", s3);
  printf("%c\n", s3[0]);
  parse_string(item, s4);
  printf("%s\n", item->valueOfString);
  cJSON_Delete(item);
}

void test_parse_array(void)
{
  cJSON *item = cJSON_New_Item();
  char *a1 = "[1]";
  char *a2 = "[1, 2, 3]";
  char *a3 = "[\"l\"]";
  char *a4 = "[\"l\", \"c\", \"q\"]";
  parse_array(item, a1);
  printf("%d\n", item->child->valueOfInt);
  parse_array(item, a2);
  printf("%d, %d, %d\n", item->child->valueOfInt, item->child->next->valueOfInt, item->child->next->next->valueOfInt);
  parse_array(item, a3);
  printf("%s\n", item->child->valueOfString);
  parse_array(item, a4);
  printf("%s %s %s\n", item->child->valueOfString, item->child->next->valueOfString, item->child->next->next->valueOfString);
  cJSON_Delete(item);
}

void test_parse_object(void) 
{
  cJSON *item = cJSON_New_Item();
  char *o1 = "{\"lou\": 1}";
  char *o2 = "{\"chao\" : \"qi\"}";
  char *o3 = "{\"killer\" :[1]}";
  char *o4 = "{\"1\": {\"2\": 3}}";
  parse_object(item, o1);
  printf("%s : %d\n", item->child->name, item->child->valueOfInt);
  parse_object(item, o2);
  printf("%s : %s\n", item->child->name, item->child->valueOfString);
  parse_object(item, o3);
  printf("%s : %d\n", item->child->name, item->child->child->valueOfInt);
  parse_object(item, o4);
  printf("%s : %s : %d", item->child->name, item->child->child->name, item->child->child->valueOfInt);
  cJSON_Delete(item);
}

void test_print_number(void)
{
  int i = 0;
  double d = 0;
  printf("%s\n", print_number(i, d));
  i = 666;
  d = 666;
  printf("%s\n", print_number(i, d));
  d = 1.0e10;
  printf("%s\n", print_number(i, d));
  d = 1.0e-7;
  printf("%s\n", print_number(i, d));
  d = 6.66;
  printf("%s\n", print_number(i, d));
}

void test_print_string(void)
{
  char *s1 = "1";
  printf("%s\n", print_string(s1));
  char *s2 = "";
  printf("%s\n", print_string(s2));
  char *s3 = "\\t";
  printf("%s\n", print_string(s3));
  char *s4 = "lou\\tchao\n";
  printf("%s\n", print_string(s4));
}

void test_print_array(void)
{
  cJSON *item = cJSON_New_Item();
  char *s1 = "[1, 2,3]";
  parse_array(item, s1);
  printf("%s\n", print_array(item));
  char *s2 = "[\"lou\",\"chao\", \"qi\"]";
  parse_array(item, s2);
  printf("%s\n", print_array(item));
  char *s3 = "[1, [2, 3], \"hello\"]";
  parse_array(item, s3);
  printf("%s\n", print_array(item));
  cJSON_Delete(item);
}

void test_print_object(void)
{
  cJSON *item = cJSON_New_Item();
  char *s1 = "{\n\"name\": \"Jack (\\\"Bee\\\") Nimble\"}";
  parse_object(item, s1);
  printf("%s\n", print_object(item));
  char *s2 = "{\"hello\": \"world\", \"yes\": 255}";
  parse_object(item, s2);
  printf("%s\n", print_object(item));
  char *s3 = "{\"array\": [1, 2, 3], \"string\": \"hello, world\", \"number\": 666}";
  parse_object(item, s3);
  printf("%s\n", print_object(item));
  cJSON_Delete(item);
}

void test_total(void)
{
  cJSON *json;
  char test1[] = "{\n\"name\": \"Jack (\\\"Bee\\\") Nimble\", \n\"format\": {\"type\":       \"rect\", \n\"width\":      1920, \n\"height\":     1080, \n\"interlace\":  \"false\",\"frame rate\": 24\n}\n}";
  json = cJSON_parse(json, test1);
  printf("%s\n", cJSON_print(json));
}

int main()
{
  test_parse_number();
  test_parse_string();
  test_parse_array();
  test_parse_object();
  test_print_number();
  test_print_string();
  test_print_array();
  test_print_object();
  test_total();
  return 0;
}
*/
