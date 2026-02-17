#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "shtex.h"

struct tex* shtex;
char bufname[256];
char command[512];
char status[512];
char self[258];
void (*move)(int);

void go(size_t x)
  {
  size_t len = strnlen(shtex->data, shtex->size);
  if (x <= len) shtex->cur = x;
  }

void constrained_move(int x)
{ go(shtex->cur + x); }

void raw_move(int x)
  {
  shtex->cur += x;
  shtex->cur %= shtex->size;
  }

int detach(size_t x)
  {//shifts everything after cursor by x bytes
  char* pos = shtex->data+shtex->cur;//where we "are"; we'll move to the end of string, and start moving characters from there
  for (;*pos; ++pos) { }//go to end

  if (pos+x >= shtex->data+shtex->size)
    {//set status and return if there's no room
    sprintf(status, "error: buffer full");
    return 1;
    }

  for (;pos >= shtex->data+shtex->cur; --pos)
    pos[x] = pos[0];
  return 0;
  }

int attach(size_t x)
  {//shifts everything after cursor by x bytes
  char* pos = shtex->data+shtex->cur-x;//where we "are";
  if (pos < shtex->data)
    {//set status and return if there's no room
    sprintf(status, "error: no room to delete");
    return 1;
    }

  for (;pos[x]; ++pos)
    pos[0] = pos[x];
  pos[0] = pos[x];
  return 0;
  }


size_t where(char* word)
  {//find word in text after start, wraps around
  if (*word)
  for(size_t i = 0, pos = shtex->cur+1; pos+i != shtex->cur;
      shtex->data[pos+i]==word[i]? i++ : (pos+=i+1, i=0))
    {
    if (!word[i]) return pos;//if all characters in target coincide, return position
    if (!shtex->data[pos+i]) pos = 0;//wrap back to the start if at the end of data
    };
  return sprintf(status, "%.s not found", word), shtex->cur;
  }


int delete(size_t size)
  {
  if (attach(size)) return 1;
  move(-size);
  return 0;
  }

int insert(char* src, size_t size)
  {
  if (detach(size)) return 1;
  for (size_t i = 0; i<size; ++i)
    shtex->data[shtex->cur+i] = src[i];
  move(size);
  return 0;
  }

void search()
  {
  char* snip;//buffer containing whatever user wants to search for
  char snipname[256];

  sprintf(snipname, "%s.snip", bufname);
  snip = shtex_create(snipname, 256)->data;

  sprintf(command, "%s %s", self, snipname);
  system(command);

  go(where(snip));
  shm_unlink(snipname);
  }


