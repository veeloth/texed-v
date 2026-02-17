#include <ctype.h>
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
struct winsize ws;
struct termios prevstate;
struct sigaction act;

bool done = false;
char bar[256] = BAR;


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

  move = constrained_move;

  if (!(shtex = shtex_create(bufname, 256*256)))
    exit(1);
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
      go(where("tengo"));
      break;
    case ctrl_f:
      search();
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
