/* Verifies that memory mappings persist after file close. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/arc4.h"
#include "tests/lib.h"
#include "tests/main.h"

#define ACTUAL ((void *) 0x10000000)

void
test_main (void)
{
  int handle;
  mapid_t map;

  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK ((map = mmap (handle, ACTUAL)) != MAP_FAILED, "mmap \"sample.txt\"");
  printf("Test Check 1\n");
  close (handle);
  printf("Test Check 2\n");
  if (memcmp (ACTUAL, sample, strlen (sample)))
    fail ("read of mmap'd file reported bad data");
  
  printf("Test Check 3\n");
  munmap (map);
}
