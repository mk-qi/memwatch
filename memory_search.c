#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "vmmap.h"
#include "vmmap_data.h"
#include "search_data.h"
#include "parse_utils.h"
#include "process_utils.h"
#include "memory_search.h"
#include "freeze_region.h"

#define read_addr_size(addr, size) \
  { fscanf(stdin, "%llX", &addr); \
  command = fgetc(stdin); \
  if (command == ':') \
    fscanf(stdin, "%llX", &size); \
  else if (command == ' ') \
    fscanf(stdin, "%llu", &size); \
  else if (command == '\n') { \
    printf("invalid command format\n"); \
    ungetc('\n', stdin); \
    break; \
  } else { \
    printf("invalid command format\n"); \
    break; \
  } }

// memory searching user interface!
int memory_search(pid_t pid, int pause_during) {

  // first get the process name
  char processname[PROCESS_NAME_LENGTH];
  getpidname(pid, processname, PROCESS_NAME_LENGTH);

  // no process name? process doesn't exist!
  if (!strlen(processname)) {
    printf("process %u does not exist\n", pid);
    return (-2);
  }

  // init the region freezer
  InitRegionFreezer();
 
  // yep, lots of variables (see the code below for their explanations)
  int process_running = 1;
  int command = 1;
  char* arguments;
  int x, error;
  VMRegion vmr;
  unsigned long long size;
  unsigned long long addr;
  void* write_data = NULL;
  VMRegionDataMap* map = NULL;
  VMRegionDataMap* newmap;
  MemorySearchData* search = NULL;

  // while we have stuff to do...
  while (command) {

    // prompt the user
    if (search)
      printf("(memwatch:%u/%s#%s) ", pid, processname, search->name);
    else
      printf("(memwatch:%u/%s) ", pid, processname);

    // what is thy bidding, my master?
    command = fgetc(stdin);
    switch (command) {

      // TODO: print help message
      case 'h':
      case 'H':
        printf(
"memwatch memory search utility\n"
"\n"
"commands:\n"
"  l                     list memory regions\n"
"  d                     dump memory\n"
"  t <data>              find occurrences of data\n"
"  m <addr>              enable all access types on region containing address\n"
"  r <addr+size>         read from memory\n"
"  w <addr> <data>       write to memory\n"
"  W <addr> <filename>   write file to memory\n"
"  f <addr> <data>       freeze data in memory\n"
"  u                     list frozen regions\n"
"  u <index>             unfreeze frozen region\n"
"  S <type> [name]       begin new search\n"
"  s <operator> [value]  search for a changed value\n"
"  x                     print current search results\n"
"  -                     pause process\n"
"  +                     resume process\n"
"\n"
"<addr+size> may be either <addr(hex)> <size(dec)> or <addr(hex)>:<size(hex)>.\n"
"all <data> arguments are in immediate format (see main usage statement).\n"
"the f command is like the w command, but the write is repeated in the\n"
"  background until canceled by a u command.\n"
"new freezes will be named with the same name as the current search (if any).\n"
"valid types for S command: u8, u16, u32, u64, s8, s16, s32, s64, float, double.\n"
"any type except u8 and s8 may be prefixed with l to make it reverse-endian.\n"
"valid operators for s command: = < > <= >= != $\n"
"the $ operator does a flag search (finds values that differ by only one bit).\n"
"if the value on the s command is omitted, the previous value (from the last\n"
"  search) is used.\n"
"\n"
"example: find all occurrences of the string \"the quick brown fox\"\n"
"  t \"the quick brown fox\"\n"
"example: read 0x100 bytes from 0x00002F00\n"
"  r 2F00:100\n"
"example: write \"the quick brown fox\" and terminating \\0 to 0x2F00\n"
"  w 2F00 \"the quick brown fox\"00\n"
"example: open a search named \"score\" for a 16-bit signed int\n"
"  S s16 score\n"
"example: search for the value 2160\n"
"  s = 2160\n"
"example: search for values less than 30000\n"
"  s < 30000\n"
"example: search for values greater than or equal to the previous value\n"
"  s >=\n"
               );
        break;

      // list memory regions
      case 'l':
      case 'L':

        // stop the process if necessary, get the list, and resume the process
        if (process_running && pause_during)
          VMStopProcess(pid);
        map = GetProcessRegionList(pid, 0);
        if (process_running && pause_during)
          VMContinueProcess(pid);
        if (!map) {
          printf("get process region list failed\n");
          break;
        }

        // print it out and delete it
        print_region_map(map);
        DestroyDataMap(map);

        break;

      // dump memory
      case 'd':
      case 'D':

        // stop the process if necessary, dump memory, and resume the process
        if (process_running && pause_during)
          VMStopProcess(pid);
        map = DumpProcessMemory(pid, 0);
        if (process_running && pause_during)
          VMContinueProcess(pid);
        if (!map) {
          printf("memory dump failed\n");
          break;
        }

        // print it out and delete it
        // TODO: do something useful with the map, duh
        print_region_map(map);
        DestroyDataMap(map);
        break;

      // find a string in memory
      case 't':
      case 'T':

        // stop the process if necessary, dump memory, and resume the process
        printf("reading memory\n");
        if (process_running && pause_during)
          VMStopProcess(pid);
        map = DumpProcessMemory(pid, 0);
        if (process_running && pause_during)
          VMContinueProcess(pid);
        if (!map) {
          printf("memory dump failed\n");
          break;
        }

        // read the string
        size = read_stream_data(stdin, &write_data);

        // find the string in a region somewhere
        for (x = 0; x < map->numRegions; x++) {

          // skip regions with no data
          if (!map->regions[x].data)
            continue;

          int y;
          for (y = 0; y <= map->regions[x].region._size - size; y++) {
            if (!memcmp(&map->regions[x].s8_data[y], write_data, size)) {
              printf("string found at %016llX\n",
                     map->regions[x].region._address + y);
            }
          }
        }

        free(write_data);
        DestroyDataMap(map);
        break;

      // make a region all-access
      case 'm':
      case 'M':

        // read the address
        fscanf(stdin, "%llX", &addr);

        // attempt to make it all-access
        if (VMSetRegionProtection(pid, addr, 1, VMREGION_ALL, VMREGION_ALL))
          printf("region protection set to all access\n");
        else
          printf("failed to change region protection\n");
        break;

      // read from memory
      case 'r':
      case 'R':
        read_addr_size(addr, size);

        void* read_data = malloc(size);
        if (!read_data) {
          printf("failed to allocate memory for reading\n");
          break;
        }
        if (error = VMReadBytes(pid, addr, read_data, &size)) {
          print_process_data(processname, addr, read_data, size);
        } else
          printf("failed to read data from process\n");
        free(read_data);
        break;

      // write data to memory
      case 'w':

        // read the address
        fscanf(stdin, "%llX", &addr);

        // read the data
        size = read_stream_data(stdin, &write_data);

        // and write it
        if (error = VMWriteBytes(pid, addr, write_data, size))
          printf("wrote %llX bytes\n", size);
        else
          printf("failed to write data to process\n");
        free(write_data);
        break;

      // write file to memory
      case 'W':

        // read the parameters
        read_addr_size(addr, size);
        char* filename = read_string_delimited(stdin, '\n');
        trim_spaces(filename);

        // write the file
        write_file_to_process(filename, size, pid, addr);
        free(filename);

        break;

      // freeze value with immediate data
      case 'f':

        // read the address
        fscanf(stdin, "%llX", &addr);

        // read the data
        size = read_stream_data(stdin, &write_data);

        // and freeze it
        if (FreezeRegion(pid, addr, size, write_data,
                         search ? search->name : "[no associated search]"))
          printf("failed to freeze region\n");
        else
          printf("region frozen\n");
        free(write_data);
        break;

      // unfreeze a var by index, or print frozen regions
      case 'u':

        // read the address
        if (fgetc(stdin) != '\n') {
          fscanf(stdin, "%lld", &addr);
          if (UnfreezeRegionByIndex(addr))
            printf("failed to unfreeze region\n");
          else
            printf("region unfrozen\n");
        } else {
          ungetc('\n', stdin);
          printf("frozen regions:\n");
          PrintFrozenRegions(0);
        }
        break;

      // begin new search
      case 'S':

        // read the type
        arguments = read_string_delimited(stdin, '\n');
        trim_spaces(arguments);

        // delete the current search
        if (search) {
          DeleteSearch(search);
          printf("current search deleted\n");
        }

        // check if a name followed the type
        const char* name = skip_word(arguments);

        // make a new search
        search = CreateNewSearchByTypeName(arguments, name);
        if (search)
          printf("opened new search of type %s\n",
                 GetSearchTypeName(search->type));
        else
          printf("failed to open new search - did you use a valid typename?\n");

        free(arguments);
        break;

      // search for a value
      case 's':

        // make sure a search is opened
        if (!search) {
          printf("no search is currently open; use the S command to open a "
                 "search\n");
          break;
        }

        // read the predicate
        arguments = read_string_delimited(stdin, '\n');
        trim_spaces(arguments);
        int pred = GetPredicateByName(arguments);

        // check if a value followed the predicate
        void* value = NULL;
        char* value_text = skip_word(arguments);
        if (*value_text) {

          // we have a value... blargh
          unsigned long long ivalue;
          float fvalue;
          double dvalue;

          if (IsIntegerSearchType(search->type)) {
            sscanf(value_text, "%lld", &ivalue);
            value = &ivalue;
          } else if (search->type == SEARCHTYPE_FLOAT) {
            sscanf(value_text, "%f", &fvalue);
            value = &fvalue;
          } else if (search->type == SEARCHTYPE_DOUBLE) {
            sscanf(value_text, "%lf", &dvalue);
            value = &dvalue;
          }
        }

        // stop the process if necessary, dump memory, and resume the process
        if (process_running && pause_during)
          VMStopProcess(pid);
        map = DumpProcessMemory(pid, 0);
        if (process_running && pause_during)
          VMContinueProcess(pid);
        if (!map) {
          printf("memory dump failed\n");
          free(arguments);
          break;
        }

        // run the search
        MemorySearchData* search_result = ApplyMapToSearch(search, map, pred,
                                                           value);
        map = NULL;
        free(arguments);

        // success? then save the result
        if (!search_result) {
          printf("search failed\n");
          break;
        }

        DeleteSearch(search);
        search = search_result;
        if (search->numResults >= 100) {
          printf("results: %lld\n", search->numResults);
          break;
        }

        // fall through to display results

      // print search results
      case 'x':
      case 'X':
        for (x = 0; x < search->numResults; x++)
          printf("%016llX\n", search->results[x]);
        printf("results: %lld\n", search->numResults);

        break;

      // pause process
      case '-':
        VMStopProcess(pid);
        process_running = 0;
        printf("process suspended\n");
        break;

      // resume process
      case '+':
        VMContinueProcess(pid);
        process_running = 1;
        printf("process resumed\n");
        break;

      // end-of-line, herp derp
      case '\n':
        ungetc('\n', stdin);
        break;

      // quit
      case 'q':
      case 'Q':
        command = 0; // tell the outer loop to quit
        break;
    }

    // read extra junk at end of command
    while (fgetc(stdin) != '\n')
      usleep(10000);
  }

  // shut down the region freezer and return
  CleanupRegionFreezer();
  return 0;
}
