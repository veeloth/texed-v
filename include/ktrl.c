#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "keys.h"
#include "moda.c"
#include "term.c"

#define INT(x) (int)((x)%INT_MAX)

union input
  {
  int integer;
  unsigned char uchars[sizeof(int)];
  char chars[sizeof(int)];
  };
union input input;//note: c stands for character, cin is "character input"


//main flow declarations
void ktrl_exit()
  {//terminate function
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &prevstate);//precisely what it looks like, sets terminal to previous state / mode
  }


void ktrl_init(char** argv)
  {
  atexit(ktrl_exit);
  prepare_terminal("ktrl", &ws, &prevstate);
  strcpy(self, argv[0]);
  strcpy(buf_name, argv[1]);
  move = constrained_move;
  if (!(main_buffer = shtex_create(buf_name, 256*256)))
    exit((perror("couldn't create buffer"), 1));
  }

void ktrl(unsigned char length, unsigned char input_arr[static 4])
  {//input processing function
  input.integer = 0;
  if (length==255) exit(0);//l == 255 iff input == ctrl+c 
  //get input, can't assign *(int*)input_arr to input.integer directly bcs not all bytes are updated
  for (char i = 0; i < length; ++i)
    input.uchars[i] = input_arr[i];
  msg[0] = 0;
  if (iscntrl(input.chars[0]))
    {
    switch (input.integer)
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
      case supr:
        delete(-1);
        break;
      case 15:
        go(where("tengo"));
        break;
      case ctrl_f:
        search();
        break;
      case ctrl_l:
        buf[cur] = 0;
        break;
      case ctrl_w:
        delete(cur - last_word());
        break;
      case ctrl_e:
        delete(cur - next_word());
        break;
      case 11:
        insert("\e", 1);
        break;
      case enter:
        insert("\n", 1);
        break;
      default://this code runs when the keysym has no set behavior
        sprintf(msg, "\e[30m\e[41m0x");
        for (char i = 0; i < length; ++i)
          sprintf(msg+strlen(msg), "%02x", input.uchars[i]);
        sprintf(msg+strlen(msg), " %d %d\e[0m", length, input.integer);
      }
    }
  else insert(input.chars, length);
  }
