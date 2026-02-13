#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/mman.h>

#include "include/shtex.h"
#include "include/keys.h"
#include "include/moda.c"

typedef char bool;
#define true 1
#define false 0
#define BAR "%s[%zu]: %02x  size: %zu "

union cin
  {
  int i;
  unsigned char u[sizeof(int)];
  char c[sizeof(int)];
  };
struct s256
  {
  char c[256];
  };

union cin c;//note: c stands for character, cin is "character input"
struct tex* shtex;
struct winsize ws;
struct termios prevstate;
struct sigaction act;

bool done = false;
char bufname[256];
char command[512];
char status[512];
char self[258];
char bar[256] = BAR;
void (*move)(int);


//utility declarations
void sa_handler_callback(int x) {  }

void enable_raw_mode()
  {
  struct termios raw;
  tcgetattr(STDIN_FILENO, &raw);
  prevstate = raw;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  }

void prepare_terminal()
  {
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);//can be used to get window size
  act.sa_handler = sa_handler_callback;
  sigaction(SIGINT, &act, NULL);
  enable_raw_mode();
  setbuf(stdout, NULL);//disables out-buf so printf() prints immediately, I wonder if it can cause problems
  printf("%c]0;%s%c", '\033', "texd", '\007');//sets window title to texd
  }

//moda
void bound_move(int ox)
  {
  size_t nx = shtex->cur + ox;
  size_t len = strnlen(shtex->data, shtex->size);
  if (nx <= len) shtex->cur += ox;
  }

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


#define a (shtex->data + shtex->cur + pos)
#define b target

size_t find(char* target, unsigned char len)
  { for (size_t pos = 0;; ++pos) {//move along data by incrementing pos
    size_t i;

    for (i = 0; a[i] == b[i] && i < len; ++i)/*test for a matching character, if found keep going til you find a
                                              difference or exceed len (i.e. exceed how much text you want to compare*/
      if (shtex->cur + pos + i >= shtex->size)
        {//if you exceed the length of the main buffer, set status to not found and return
        sprintf(status, "%.*s not found", (int)(len%INT_MAX), target);
        return 0;
        }

    if (len == i)//if the amount of matching characters in a row was the full length of the target, return current position
      return pos;
    else//if it was less (it can't be more, remember, we stop before that) we know there's not a match til i so we skip it
      pos += i;
  } }

#undef a
#undef b


int delete(size_t size)
  {
  if (attach(size))
    return 1;
  move(-size);
  return 0;
  }

int insert(char* src, size_t size)
  {

  if (detach(size))
    return 1;

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

  move(find(snip, strlen(snip)));
  shm_unlink(snipname);
  }


void draweditedbar()
  {
  printf("\e[%d;1H", ws.ws_col);//move to lowest row
  printf("%.*s", (int)(shtex->cur%INT_MAX), shtex->data);
  printf("\e[30m\e[46m%c", shtex->data[shtex->cur]);//white bg, black fg / cursor colours
  printf("\e[40m\e[39m");//set color to normal
  printf("%.*s", (int)((shtex->size - shtex->cur)%INT_MAX), shtex->data+shtex->cur+1);
  printf(bar, bufname, shtex->cur, shtex->data[shtex->cur], shtex->size);//print bar and go back
  printf("status: %s ", status);
  }


//main flow declarations
void drawbar()
  {
  printf("\e[2J");
  printf("\e[%d;1H", ws.ws_col);//move to lowest row
  printf(bar, bufname, shtex->cur, shtex->data[shtex->cur], shtex->size);//print bar and go back
  printf("status: %s ", status);
  }


void drop()
  {//terminate function
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &prevstate);//precisely what it looks like, sets terminal to previous state / mode
  printf("\e[40m\e[39m\e[2J\e[1;1H");//clears screen... and sets colors to normal
  }


void init(char** argv)
  {

  atexit(drop);
  prepare_terminal();

  strcpy(self, argv[0]);
  strcpy(bufname, argv[1]);
  //strcpy(bar, BAR);//TODO: add error handling...

  move = bound_move;

  if (!(shtex = shtex_create(bufname, 256*256)))
    exit(1);
  }


void edit_bar()
  {
  unsigned char clen;
  struct tex* prevshtex = shtex;
  struct tex* newbar = malloc(sizeof(struct tex) + sizeof(bar));
  strcpy(newbar->data, bar);
  newbar->size = sizeof(bar);
  newbar->cur = 0;
  shtex = newbar;
edit_bar:
  strcpy(bar, newbar->data);
  //shtex = prevshtex;
  drawbar();
  c.i = 0;
  clen = read(STDIN_FILENO, c.u, 4);
  if (iscntrl(c.c[0]))
    {
    switch (c.i)
      {
    case enter:
      shtex = prevshtex;
      free(newbar);
      return;
    case left:
      move(-1);
      goto edit_bar;
    case right:
      move(1);
      goto edit_bar;
    case backspace:
      delete(1);
      goto edit_bar;
    default://this code runs when the key combination is undefined
      printf("\e[30m\e[41m");//red bg, black fg
      printf("hex: %02x%02x%02x%02x length: %d int: %d ", c.u[0], c.u[1], c.u[2], c.u[3], clen, c.i);
      //fprintf(stderr, "%d\n", c.i);
      goto edit_bar;//if it's a control key, bypass drawing again completely, so the output message stays on screen
      }
    }
  else
    {
    insert(c.c, clen);
    goto edit_bar;
    }
  }


void ktrl()
  {//input processing function
  unsigned char clen;
  ktrl:
  c.i = 0;
  status[0] = 0;
  if ( (clen = read(STDIN_FILENO, c.u, 4)) == 255 ) exit(0);//ctrl+c was pressed iff clen is 255
  if (iscntrl(c.c[0]))
    {
    switch (c.i)
      {
    case left:
      move(-1);
      break;
    case right:
      move(1);
      break;
    case backspace:
      delete(1);
      break;
    case 15:
      move(find("tengo", 5));
      break;
    case ctrl_f:
      search();
      break;
    case ctrl_b:
      edit_bar();
      break;
    case enter:
      insert("\n", 1);
      break;
    default://this code runs when the key combination is undefined
      printf("\e[30m\e[41m");//red bg, black fg
      printf("hex: %02x%02x%02x%02x length: %d int: %d ", c.u[0], c.u[1], c.u[2], c.u[3], clen, c.i);
      //fprintf(stderr, "%d\n", c.i);
      goto ktrl;//if it's a control key, bypass drawing again completely, so the output message stays on screen
      }
    }
  else
    {
    insert(c.c, clen);
    }
  }



//
int main(int argc, char* argv[argc+1])
  {
  if (argc!=2)
    {
    printf("usage: %s <buf name>", argv[0]);
    return 1;
    }

  init(argv);
  while (!done)
    {
    drawbar();
    ktrl();
    }
  drop();

  return 0;
  }
