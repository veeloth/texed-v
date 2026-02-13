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

typedef char bool;
#define true 1
#define false 0
#define BAR "%s[%zu]: %02x  size: %zu "

struct tex* shtex;
struct winsize ws;
struct termios prevstate;
struct sigaction act;

size_t sposx = 0;
size_t sposy = 0;
bool done = false;
char bufname[256];
char status[512];
char self[258];
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
  //act.sa_handler = sa_handler_callback;
  //sigaction(SIGINT, &act, NULL);
  enable_raw_mode();
  setbuf(stdout, NULL);//disables out-buf so printf() prints immediately, I wonder if it can cause problems
  printf("%c]0;%s%c", '\033', "texd", '\007');//sets window title to texd
  }

void getcur()
  {
  char buf[32];
  int i = 0;
  printf("\e[6n");
  while (i < sizeof(buf) - 1)
    {
    read(STDIN_FILENO, &buf[i], 1);
    if (buf[i] == 'R') break;
    i++;
    }
  buf[i] = 0;
  sscanf(buf+2, "%zu;%zu", &sposx, &sposy);
  }



//func declarations
void drawbar()
  {
  printf("\e[%d;1H", ws.ws_col);//move to lowest row
  printf(bar, bufname, shtex->cur, shtex->data[shtex->cur], shtex->size);//print bar and go back
  printf("status: %s ", status);
  printf("x: %zu ; y: %zu", sposx, sposy);
  }


//main flow declarations
void draw()
  {//UI / printing function
  printf("\e[40m\e[39m \e[2J");//set color to normal and clear screen
  printf("\e[1;1H");//go back to beginning of screen
  printf("%.*s", (int)(shtex->cur%INT_MAX), shtex->data);
  getcur();
  printf("%.*s", (int)((shtex->size - shtex->cur)%INT_MAX), shtex->data+shtex->cur);
  drawbar();
  printf("\e[%zu;%zuH", sposx, sposy);//move cursor to its position on screen
  //fflush(stdout);
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

  if (!(shtex = shtex_create(bufname, 256*256)))
    exit(1);
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
    draw();
    usleep(16700);//60 fps
    }
  drop();

  return 0;
  }
