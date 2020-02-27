/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * JJ & BW t
 */
#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* micro32.ee.columbia.edu */
#define SERVER_HOST "128.59.64.152"
#define SERVER_PORT 42000

#define BUFFER_SIZE 128

/*
 * References:
 *
 * http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */

int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;
void *network_thread_f(void *);
int convert_key(uint8_t mod, uint8_t key);

void clear(int r, int rs, int c, int cs) {
  for (int col = cs; col < c; col++) {
    for (int row = rs; row < r; row++) {
      fbputchar(' ', row, col);
    }
  }
}

int checkcurr(int row, int col) {
  if (row == 21) {
    if (col == 0) return 1;
    if (col == 63) return 2;
  } else if(row == 22) {
    if (col == 0) return 3;
    if (col == 63) return 4;
  } return 0;
}
  

int main()
{
  //delete cur_row
  int err, col, cur_row, cur_col, buff_col;
  
  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  //clear screen
  clear(24,0,64,0);
  
  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('*', 0, col);
    fbputchar('_', 20, col);
    fbputchar('*', 23, col);
  }
  cur_col = 0;
  cur_row = 21;
  buff_col = 0;

  fbputs("Hello CSEE 4840 World!", 4, 10);

  /* Open the keyboard */
  if ( (keyboard = openkeyboard(&endpoint_address)) == NULL ) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }
    
  /* Create a TCP communications socket */
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if ( inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  /* Start the network thread */
  pthread_create(&network_thread, NULL, network_thread_f, NULL);

  /* Look for and handle keypresses */
  int key, state;
  char hold;
  char sendbuf[BUFFER_SIZE];
  for (;;) {
    libusb_interrupt_transfer(keyboard, endpoint_address,
			      (unsigned char *) &packet, sizeof(packet),
			      &transferred, 0);
    if (transferred == sizeof(packet)) {
      sprintf(keystate, "%02x %02x %02x", packet.modifiers, packet.keycode[0],
	      packet.keycode[1]);
      printf("%s\n", keystate);
      state = checkcurr(cur_row, cur_col);
      printf("row: %d, col: %d, state: %d\n", cur_row, cur_col, state);
      key = convert_key(packet.modifiers, packet.keycode[0]);
      if (key != 0) {
	if (key != 1 && key != 2 && key != 3 && key != 8 && state != 4) {
	  //fbputchar(' ', cur_row, cur_col);
	    fbputchar(key, cur_row, cur_col);
	    sendbuf[buff_col] = key;
	    if (state == 2) {
	      cur_col = 0;
	      cur_row = 22;
	    } else cur_col++;
	    buff_col++;
	  //if (packet.keycode[0] != 0x00) cur_col++;
	} else {
	  if (key == 8) { //backspace
	    if(state != 1) {
	      fbputchar(hold, cur_row, cur_col);
	      if(state == 3) {
		fprintf(stderr, "in if, state: %d", state);
		cur_col = 63;
		cur_row = 21;
		} else cur_col--;
	      buff_col--;
	      sendbuf[buff_col] = ' ';
	    }
	  }
	  else if (key == 1) { // enter
	    sendbuf[cur_col] = 0;
	    fprintf(stderr, "%s\n", sendbuf);
	    write(sockfd, sendbuf, BUFFER_SIZE);
	    clear(23,21,64,0);
	    cur_col = 0;
	    buff_col = 0;
	    cur_row = 21;
	    sendbuf[0] = 0;
	  }
	  else if (key == 2) { //left arrow
	    if(state != 1) {
	      fbputchar(hold, cur_row, cur_col);
	      buff_col--;
	      if(state == 3) {
		cur_col = 64;
		cur_row = 21;
	      } else cur_col--;
	    }
	  }
	  else if (key == 3) {
	    if (cur_col < strlen(sendbuf) - 1) {
	      fbputchar(hold, cur_row, cur_col);
	      buff_col++;
	      if(state == 2) {
		cur_col = 0;
		cur_row = 22;
	      } else cur_col++;
	    }
	  }
	}
      }
      hold = sendbuf[buff_col];
      fbputchar('_', cur_row, cur_col);
      fbputs(keystate, 6, 0);
      fprintf(stderr, "bufpos: %d \nbuffer: %s\n", buff_col, sendbuf);
      if (packet.keycode[0] == 0x29) { /* ESC pressed? */
	break;
      }
    }
  }

  /* Terminate the network thread */
  pthread_cancel(network_thread);

  /* Wait for the network thread to finish */
  pthread_join(network_thread, NULL);

  return 0;
}

void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;
  int place = 8;
  /* Receive data */
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
    fbputs(recvBuf, place, 0);
    if (strlen(recvBuf) > 64) place++;
    //place++;
    if (place >= 21) place = 8;
    memset(recvBuf, ' ', sizeof(recvBuf));
    recvBuf[BUFFER_SIZE - 1] = '\n';
    fbputs(recvBuf, place, 0);
  }

  return NULL;
}

int convert_key(uint8_t mod, uint8_t key) {
  //int out;
  int ikey = (int) key;
  int imod = (int) mod;
  if (ikey >= 4 && ikey <= 29) { // letters
    ikey = ikey + 93;
    fprintf(stderr, "ikey is %d\n", ikey);
    if (imod == 2 /*|| imod ==*/ ) ikey = ikey - 32;
  }
  else if (ikey >= 30 && ikey <= 39) { //numbers
    if (imod == 2 /*|| imod ==*/ ) {
      if (ikey == 30) ikey = 33; // !
      if (ikey == 31) ikey = 64; // @
      if (ikey == 32) ikey = 35; // #
      if (ikey == 33) ikey = 36; // $
      if (ikey == 34) ikey = 25; // %
      if (ikey == 35) ikey = 94; // ^
      if (ikey == 36) ikey = 38; // &
      if (ikey == 37) ikey = 42; // *
      if (ikey == 38) ikey = 40; // (
      if (ikey == 39) ikey = 41; // )
    }
    else ikey = ikey + 18;
  }
  else if (key == 42) ikey = 8; //backspace
  else if (key == 44) ikey = 32; //space
  else if (key == 79) ikey = 3; //right arrow
  else if (key == 80) ikey = 2; //left arrow
  else if (key == 40) ikey = 1; //enter
  else ikey = 0;
  return ikey;
}
