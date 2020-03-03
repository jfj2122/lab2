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
int convert_key(uint8_t mod, uint8_t key, uint8_t key2);
int fb_place, flag;
pthread_mutex_t lock;
pthread_cond_t cond;
int valid;

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

  //mutex intiation 
  if (pthread_mutex_init(&lock, NULL) != 0) {
    fprintf(stderr, "Could not initiate mutex");
    exit(1);
  }


  /* Start the network thread */
  pthread_create(&network_thread, NULL, network_thread_f, NULL);

  /* Look for and handle keypresses */
  int key, state, buf_end;
  char sendbuf[BUFFER_SIZE];
  memset(sendbuf, 0, BUFFER_SIZE);
  char half1[BUFFER_SIZE/2 + 1];
  char half2[BUFFER_SIZE/2];
  char last4[4];
  buf_end = 0;
  flag = 0;
  for (;;) {
    libusb_interrupt_transfer(keyboard, endpoint_address,
			      (unsigned char *) &packet, sizeof(packet),
			      &transferred, 0);
    if (transferred == sizeof(packet)) {
      sprintf(keystate, "%02x %02x %02x", packet.modifiers, packet.keycode[0],
	      packet.keycode[1]);
      //printf("%s\n", keystate);
      state = checkcurr(cur_row, cur_col);
      key = convert_key(packet.modifiers, packet.keycode[0], packet.keycode[1]);
      if (key != 0) {
	if (key != 1 && key != 2 && key != 3 && key != 8 && state != 4) {
	    sendbuf[buff_col] = key;
	    if (state == 2) {
	      cur_col = 0;
	      cur_row = 22;
	    } else cur_col++;
	    buff_col++;
	    buf_end++;
	} else {
	  if (key == 8) { //backspace
	    if(state != 1) {
	      memcpy(sendbuf + buff_col - 1, sendbuf + buff_col, (buf_end - buff_col) + 1);
	      if(state == 3) {
		cur_col = 63;
		cur_row = 21;
		} else cur_col--;
	      buff_col--;
	      buf_end--;
	    }
	  }
	  else if (key == 1) { // enter
	    sendbuf[buf_end] = 0;
	    write(sockfd, sendbuf, BUFFER_SIZE);
	    pthread_mutex_lock(&lock);
	    fbputs("ME: ", fb_place, 0);
	    //fbputs( sendbuf, fb_place, 4);
	    if (strlen(sendbuf) >= 64) {
	      strncpy(half2, sendbuf + 64, 64);
	      fbputs(half2, fb_place + 1, 0);
	      strncpy(half1, sendbuf, 60);
	      half1[60] = 0;
	      fbputs(half1, fb_place, 4);
	      fb_place++;
	      if (strlen(sendbuf) >= 124) {
		strncpy(last4, sendbuf + 124, 4);
		half2[64] = 0;
		fb_place++;
		fbputs(last4, fb_place, 0);
	      }
	    } else fbputs(sendbuf, fb_place, 0);
	    fb_place++;
	    if(fb_place >= 19) fb_place = 8;
	    pthread_mutex_unlock(&lock);
	    memset(sendbuf, ' ', sizeof(sendbuf));
	    sendbuf[BUFFER_SIZE - 1] = 0;//'\n';
	    fbputs(sendbuf, fb_place, 0);
	    memset(sendbuf, 0, sizeof(sendbuf));
	    cur_col = 0;
	    buff_col = 0;
	    cur_row = 21;
	    buf_end = 0;
	  }
	  else if (key == 2) { //left arrow
	    if(state != 1) {
	      buff_col--;
	      if(state == 3) {
		cur_col = 64;
		cur_row = 21;
	      } else cur_col--;
	    }
	  }
	  else if (key == 3) {
	    if (cur_col < buf_end) {
	      buff_col++;
	      if(state == 2) {
		cur_col = 0;
		cur_row = 22;
	      } else cur_col++;
	    }
	  }
	}
      }
      clear(23,21,64,0);
      if (strlen(sendbuf) >= 64) {
	strncpy(half2, sendbuf + 64, 64);
	fbputs(half2, 22, 0);
	strncpy(half1, sendbuf, 64);
	half1[64] = 0;
	fbputs(half1, 21, 0);
      } else fbputs(sendbuf, 21, 0);
      fbputchar('_', cur_row, cur_col);
      //fbputs(keystate, 6, 0);
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
  fb_place = 8;
  /* Receive data */
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
    pthread_mutex_lock(&lock);
    fbputs(recvBuf, fb_place, 0);
    fb_place++;
    if (fb_place >= 20) fb_place = 8;
    pthread_mutex_unlock(&lock);
    memset(recvBuf, ' ', sizeof(recvBuf));
    recvBuf[BUFFER_SIZE - 1] = '\n';
    fbputs(recvBuf, fb_place, 0);
  }

  return NULL;
}

int convert_key(uint8_t mod, uint8_t key, uint8_t key2) {
  int ikey = (int) key;
  int imod = (int) mod;
  int ikey2 = (int) key2;
  if (ikey2 > 0 && flag == 2) flag = 4;
  if (ikey >= 4 && ikey <= 29) { // letters
    if ((imod == 2 || imod == 32) && (flag != 2 && flag != 4)) { // capital
      ikey = ikey + 61;
      flag = 1;
    }
    else if(flag == 0 || flag == 4) { // lower case
      ikey = ikey + 93;
      flag = 2;
    }
    else ikey = 0;
  }
  else if (ikey == 0 && imod == 0 && (flag == 1 || flag == 2 || flag == 4)) flag = 0; //reset flag
  else if (ikey >= 30 && ikey <= 39) { //numbers
    if (imod == 2 || imod == 32 ) {
      if (ikey == 30) ikey = 33; // !
      else if (ikey == 31) ikey = 64; // @
      else if (ikey == 32) ikey = 35; // #
      else if (ikey == 33) ikey = 36; // $
      else if (ikey == 34) ikey = 37; // %
      else if (ikey == 35) ikey = 94; // ^
      else if (ikey == 36) ikey = 38; // &
      else if (ikey == 37) ikey = 42; // *
      else if (ikey == 38) ikey = 40; // (
      else if (ikey == 39) ikey = 41; // )
    }
    else {
      if(ikey != 39) ikey = ikey + 19; // 1 - 9
      else ikey = 48; // 0
    }
  }
  else if (key == 42) ikey = 8; //backspace
  else if (key == 44) ikey = 32; //space
  else if (key == 79) ikey = 3; //right arrow
  else if (key == 80) ikey = 2; //left arrow
  else if (key == 40) ikey = 1; //enter
  else if (ikey >= 45 && ikey <= 56) { //special characters
    if (imod == 2 || imod == 32) {
      if (ikey == 45) ikey = 95; // _
      if (ikey == 46) ikey = 43; // +
      if (ikey == 47) ikey = 123; // {
      if (ikey == 48) ikey = 125; // }
      if (ikey == 49) ikey = 124; // |
      if (ikey == 51) ikey = 58; // :
      if (ikey == 52) ikey = 34; // "
      if (ikey == 53) ikey = 126; // ~
      if (ikey == 54) ikey = 60; // <
      if (ikey == 55) ikey = 62; // >
      if (ikey == 56) ikey = 63; // ?
  } else {
      if (ikey == 45) ikey = 45; // -
      if (ikey == 46) ikey = 61; // =
      if (ikey == 47) ikey = 91; // [
      if (ikey == 48) ikey = 93; // ]
      if (ikey == 49) ikey = 92; // '\'
      if (ikey == 51) ikey = 59; // ;
      if (ikey == 52) ikey = 39; // '
      if (ikey == 53) ikey = 96; // `
      if (ikey == 54) ikey = 44; // ,
      if (ikey == 55) ikey = 46; // .
      if (ikey == 56) ikey = 47; // /
			}
  }
  else ikey = 0;
  return ikey;
}
