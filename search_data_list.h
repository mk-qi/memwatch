#ifndef SEARCH_DATA_LIST_H
#define SEARCH_DATA_LIST_H

#include "search_data.h"

typedef struct _MemorySearchDataList {
  int numSearches;
  MemorySearchData** searches;
} MemorySearchDataList;

MemorySearchDataList* CreateSearchList();
void DeleteSearchList(MemorySearchDataList* list);

int AddSearchToList(MemorySearchDataList* list, MemorySearchData* data);
int DeleteSearchByName(MemorySearchDataList* list, const char* name);
MemorySearchData* GetSearchByName(MemorySearchDataList* list, const char* name);

void PrintSearches(MemorySearchDataList* list);

#endif // SEARCH_DATA_LIST_H
