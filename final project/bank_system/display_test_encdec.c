#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/ioctl.h>
#include <signal.h>
#include "display.h"
#include "money.h"
#include "locker.h"
#include <pthread.h>
#include "../include/fpga_test.h"
#include "../include/fpga_dot_font.h"
// These are the sizes of the individual character arrays
#define CHAR_ARR__29x24 696
#define CHAR_ARR__10x14 168
#define MAINSTEP 0
#define SELECTSTEP 1
#define CHECKBALANCESTEP 2
#define CHECKHISTORYSTEP 3
#define CHECKSENDSTEP 4
#define LOGINSTEP 5
#define MAKEACCSTEP 6
#define LOGACCSTEP 7
#define SHOWACCINFOSTEP 9
#define PASSWDSTEP 10
#define MONEYSTEP 11
#define DRAGMONEYSTEP 14

#define LBALERT 12
#define SENDALERT 13

int otpflag = 0;
char otptmp[10] = "0";

char password[5], passwd_input[4];
/*******************FPGA Define*******************/
#define password_section 1
#define match_section 2
#define pass_match 1
#define pass_dismatch 2

int section = 0, compare_mat, next = 0;
int dev_fnd, dev_dot, dev_push_switch, dev_step_motor, dev_buzzer, dev_dip_switch, dev_text_lcd, dev_led;
char empty[16] = "---------------";
/**************CountDown Define***************/
#define stop_sign 0
int count_stop;
/********Data Structure Define Starts Here*********/

#define MAX_WORD_LENGTH 20

typedef struct
{
	char transName[MAX_WORD_LENGTH];
	int money;
} transInfo;

typedef struct
{
	char accountNum[MAX_WORD_LENGTH];
	char userName[MAX_WORD_LENGTH];
	transInfo *transinfo[10]; //TODO : Segmantation fault ?—?Ÿ¬ ê³ ì¹˜ê¸?
	char passwd[100];
	int randNum[4];
	int otp_Num;
	int money;
	int transCnt;
} element;

typedef struct treeNode
{
	element key;
	struct treeNode *left;
	struct treeNode *right;
} treeNode;

treeNode *insertKey(treeNode *p, element key)
{
	treeNode *newNode;
	int compare;

	// ?‚½?ž…?•  ?žë¦¬ì— ?ƒˆ ?…¸?“œë¥? êµ¬ì„±?•˜?—¬ ?—°ê²?
	if (p == NULL)
	{
		newNode = (treeNode *)malloc(sizeof(treeNode));
		newNode->key = key;
		newNode->left = NULL;
		newNode->right = NULL;
		return newNode;
	}
	// ?´ì§? ?Š¸ë¦¬ì—?„œ ?‚½?ž…?•  ?žë¦? ?ƒ?ƒ‰
	else
	{
		compare = strcmp(key.accountNum, p->key.accountNum);
		if (compare < 0)
			p->left = insertKey(p->left, key);
		else if (compare > 0)
			p->right = insertKey(p->right, key);
		else
			printf("\n ?´ë¯? ê°™ì?? ë²ˆí˜¸ë¡? ?“±ë¡ëœ ê³„ì¢Œê°? ?žˆ?Šµ?‹ˆ?‹¤. \n");

		return p; // ?‚½?ž…?•œ ?žë¦? ë°˜í™˜
	}
}

void insert(treeNode **root, element key)
{
	*root = insertKey(*root, key);
}

treeNode *searchBST(treeNode *root, element key)
{
	treeNode *p;
	int compare;
	p = root;
	while (p != NULL)
	{
		compare = strcmp(key.accountNum, p->key.accountNum);
		if (compare < 0)
			p = p->left;
		else if (compare > 0)
			p = p->right;
		else
		{
			printf("\nì°¾ì?? ê³„ì¢Œ : %s", p->key.accountNum);
			return p;
		}
	}
	return p;
}

/********Data Structure Define Ends Here*********/

pthread_mutex_t mtx;

int step = 0;

int x, y;
int cont_cnt=0;

int gtcnt = 0;
int clrcnt = 0;

unsigned char *ascii_characters_BIG[128];	// Store the ASCII character set, but can have some elements blank
unsigned char *ascii_characters_SMALL[128]; // Store the ASCII character set, but can have some eleunsigned char *c2[128];
unsigned char *numbers_BIG[10];				// For quicker number display routines, these arrays of pointers to the numbers
unsigned char *numbers_small[10];
// 'global' variables to store screen info and take the frame buffer.
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
// helper function to 'plot' a pixel in given color
// This is the heart of most of the drawing routines except where memory copy or move is used.
// application entry point

int cont_x;
int cont_y;

void dec(int* key, char* str, char* dec_pw){
	int i;
	for (i = 0; i < 4; i++){
		dec_pw[i] = str[i * 5] - key[i]; //the key for encryption is 3 that is subtracted to ASCII value
		if(dec_pw[i] == '0'){
			if((key[i] % 10) == 0)
				dec_pw[i] = (key[i] % 10) + 49;
			else
				dec_pw[i] = (key[i] % 10) + 48;
		}
	}
	printf("\nDecrypted pw is %s\n", dec_pw);
}

void *getTouch(void *data)
{
	char *thread_name = (char *)data;
	int fd, ret, i;
	int cnt = 0;
	const char *evdev_path = "/dev/input/by-path/platform-imx-i2c.2-event";
	// TODO : Find this file in host PC.
	struct input_event iev[3];
	fd = open(evdev_path, O_RDONLY);
	if (fd < 0)
	{
		perror("error: could not open evdev");
		return;
	}

	 //0 = children pross
		printf("child process\n");
		printf("child process is %d\n", getpid());

		while (1)
		{
			ret = read(fd, iev, sizeof(struct input_event) * 3);
			if (ret < 0)
			{
				printf("error: could not read input event\n");
				perror("error: could not read input event");
				break;
			}

			if (iev[0].type == 1 && iev[1].type == 3 && iev[2].type == 3)
			{
				printf("touch!!!!\n");
				printf("x = %d, y = %d \n", iev[1].value, iev[2].value);
				x = iev[1].value;
				y = iev[2].value;
			}
			else if (iev[0].type == 0 && iev[1].type == 1 && iev[2].type == 0)
			{
				printf("hands off!!!\n");
			}
			else if (iev[0].type == 0 && iev[1].type == 3 && iev[2].type == 0 ||
					 iev[0].type == 3 && iev[1].type == 3 && iev[2].type == 0)
			{
				printf("touching...\n");
				if(iev[0].value != 0){
					cont_x = iev[0].value;
				}
				cont_y = iev[1].value;
			}
		}
	
}

void *mainThread(void *data)
{
	char *thread_name = (char *)data;

	char dec_pw[5];
	int passwd;
	int rannum;
	int j;
	char temp_str[5];
	int temp_rand;

	struct fb_var_screeninfo orig_vinfo;
	long int screensize = 0;

	char tmp_money[8];

	element e;
	//temp is for my acc, temp2 is for opponent's acc
	treeNode *root = NULL, *temp = NULL, *temp2 = NULL;
	int tmp;
	char tmp2[10];

	// The actual glyphs here. Discard that which is not used to save memory
	{
		// Elements actually correspond to the ASCII chart
		ascii_characters_BIG[32] = SPACE__29x24;
		ascii_characters_BIG[48] = AR0__29x24;
		ascii_characters_BIG[49] = AR1__29x24;
		ascii_characters_BIG[50] = AR2__29x24;
		ascii_characters_BIG[51] = AR3__29x24;
		ascii_characters_BIG[52] = AR4__29x24;
		ascii_characters_BIG[53] = AR5__29x24;
		ascii_characters_BIG[54] = AR6__29x24;
		ascii_characters_BIG[55] = AR7__29x24;
		ascii_characters_BIG[56] = AR8__29x24;
		ascii_characters_BIG[57] = AR9__29x24;
		ascii_characters_BIG[58] = COLON__29x24;
		ascii_characters_BIG[65] = A__29x24;
		ascii_characters_BIG[66] = B__29x24;
		ascii_characters_BIG[67] = C__29x24;
		ascii_characters_BIG[68] = D__29x24;
		ascii_characters_BIG[69] = E__29x24;
		ascii_characters_BIG[70] = F__29x24;
		ascii_characters_BIG[71] = G__29x24;
		ascii_characters_BIG[72] = H__29x24;
		ascii_characters_BIG[73] = I__29x24;
		ascii_characters_BIG[74] = J__29x24;
		ascii_characters_BIG[75] = K__29x24;
		ascii_characters_BIG[76] = L__29x24;
		ascii_characters_BIG[77] = M__29x24;
		ascii_characters_BIG[78] = N__29x24;
		ascii_characters_BIG[79] = O__29x24;
		ascii_characters_BIG[80] = P__29x24;
		ascii_characters_BIG[81] = Q__29x24;
		ascii_characters_BIG[82] = R__29x24;
		ascii_characters_BIG[83] = S__29x24;
		ascii_characters_BIG[84] = T__29x24;
		ascii_characters_BIG[85] = U__29x24;
		ascii_characters_BIG[86] = V__29x24;
		ascii_characters_BIG[87] = W__29x24;
		ascii_characters_BIG[88] = X__29x24;
		ascii_characters_BIG[89] = Y__29x24;
		ascii_characters_BIG[90] = Z__29x24;

		ascii_characters_SMALL[32] = SPACE__10x14;
		ascii_characters_SMALL[48] = AR0__10x14;
		ascii_characters_SMALL[49] = AR1__10x14;
		ascii_characters_SMALL[50] = AR2__10x14;
		ascii_characters_SMALL[51] = AR3__10x14;
		ascii_characters_SMALL[52] = AR4__10x14;
		ascii_characters_SMALL[53] = AR5__10x14;
		ascii_characters_SMALL[54] = AR6__10x14;
		ascii_characters_SMALL[55] = AR7__10x14;
		ascii_characters_SMALL[56] = AR8__10x14;
		ascii_characters_SMALL[57] = AR9__10x14;
		ascii_characters_SMALL[58] = COLON__10x14;
		ascii_characters_SMALL[65] = A__10x14;
		ascii_characters_SMALL[66] = B__10x14;
		ascii_characters_SMALL[67] = C__10x14;
		ascii_characters_SMALL[68] = D__10x14;
		ascii_characters_SMALL[69] = E__10x14;
		ascii_characters_SMALL[70] = F__10x14;
		ascii_characters_SMALL[71] = G__10x14;
		ascii_characters_SMALL[72] = H__10x14;
		ascii_characters_SMALL[73] = I__10x14;
		ascii_characters_SMALL[74] = J__10x14;
		ascii_characters_SMALL[75] = K__10x14;
		ascii_characters_SMALL[76] = L__10x14;
		ascii_characters_SMALL[77] = M__10x14;
		ascii_characters_SMALL[78] = N__10x14;
		ascii_characters_SMALL[79] = O__10x14;
		ascii_characters_SMALL[80] = P__10x14;
		ascii_characters_SMALL[81] = Q__10x14;
		ascii_characters_SMALL[82] = R__10x14;
		ascii_characters_SMALL[83] = S__10x14;
		ascii_characters_SMALL[84] = T__10x14;
		ascii_characters_SMALL[85] = U__10x14;
		ascii_characters_SMALL[86] = V__10x14;
		ascii_characters_SMALL[87] = W__10x14;
		ascii_characters_SMALL[88] = X__10x14;
		ascii_characters_SMALL[89] = Y__10x14;
		ascii_characters_SMALL[90] = Z__10x14;

		numbers_small[0] = AR0__10x14; // For number displays
		numbers_small[1] = AR1__10x14;
		numbers_small[2] = AR2__10x14;
		numbers_small[3] = AR3__10x14;
		numbers_small[4] = AR4__10x14;
		numbers_small[5] = AR5__10x14;
		numbers_small[6] = AR6__10x14;
		numbers_small[7] = AR7__10x14;
		numbers_small[8] = AR8__10x14;
		numbers_small[9] = AR9__10x14;

		numbers_BIG[0] = AR0__29x24;
		numbers_BIG[1] = AR1__29x24;
		numbers_BIG[2] = AR2__29x24;
		numbers_BIG[3] = AR3__29x24;
		numbers_BIG[4] = AR4__29x24;
		numbers_BIG[5] = AR5__29x24;
		numbers_BIG[6] = AR6__29x24;
		numbers_BIG[7] = AR7__29x24;
		numbers_BIG[8] = AR8__29x24;
		numbers_BIG[9] = AR9__29x24;
	}

	// Open the framebuffer file for reading and writing
	fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd == -1)
	{
		printf("Error: cannot open framebuffer device.\n");
		return;
	}

	// Get variable screen information
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo))
	{
		printf("Error reading variable information.\n");
	}

	// Store for reset (copy vinfo to vinfo_orig)
	memcpy(&orig_vinfo, &vinfo, sizeof(struct fb_var_screeninfo));
	// Change variable info
	vinfo.bits_per_pixel = 32;
	// Can change res here, or leave what was found originally
	vinfo.xres = 1024;
	vinfo.yres = 600;
	vinfo.xres_virtual = vinfo.xres;
	vinfo.yres_virtual = vinfo.yres * 2;
	if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo))
	{
		printf("Error setting variable information.\n");
	}

	// hide cursor
	char *kbfds = "/dev/tty";
	int kbfd = open(kbfds, O_WRONLY);
	if (kbfd >= 0)
	{
		ioctl(kbfd, KDSETMODE, KD_GRAPHICS);
	}
	else
	{
		printf("Could not open %s.\n", kbfds);
	}

	// Get fixed screen information
	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo))
	{
		printf("Error reading fixed information.\n");
	}

	page_size = finfo.line_length * vinfo.yres;

	while (1)
	{

		if (step == 0)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				// draw...
				//-----------------------------------------------------------graphics loop here

				//	draw();

				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;
				for (int i = 1; i < 3; i++)
				{
					clear_screen(0);
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					drawline(100, 400, xloc + 222, 555);
					draw_string(650, 20, (char *)"AES FINAL PROJECT", 17, 65535, 0, 10, 2);
					draw_string(850, 80, (char *)"YECHAN YUN", 10, 6, 0, 10, 1);
					draw_string(850, 100, (char *)"KIDEOK KIM", 10, 6, 0, 10, 1);
					draw_string(805, 140, (char *)"B A S S", 7, 6, 0, 10, 2);
					draw_string(880, 200, (char *)"START", 5, 6, 9, 10, 2);
					draw_char(locker, 600, 125, 150, 50, 6);
					draw_char(locker, 1160, 125, 150, 50, 6);

					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			//step forwarwd to  step 1
				//if (x >= 430)
				//{
				//}
			while(1){
				if(x >= 430 && x <= 520 && y >= 400 && y<= 460){
					clrcnt = 0;
					x=0;
					y=0;
					step = LOGINSTEP;
					break;
				}
			}
		}

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == LOGINSTEP)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;
				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0)
						clear_screen(0);
					drawline(100, 400, xloc + 222, 555);
					draw_string(880, 80, (char *)"MAKE NEW ACCOUNT", 16, 6, 9, 10, 2);
					draw_string(880, 160, (char *)"LOG IN WITH YOUR ACCOUNT", 24, 6, 9, 10, 2);
					draw_string(400, 50, (char *)"B", 1, 6, 9, 10, 2);
					draw_string(400, 100, (char *)"A", 1, 6, 9, 10, 2);
					draw_string(400, 150, (char *)"S", 1, 6, 9, 10, 2);
					draw_string(400, 200, (char *)"S", 1, 6, 9, 10, 2);
					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);
					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			//step backwarwd to step 0
			while (1)
			{
				if (x >= 800 && x <= 940 && y >= 0 && y <= 60)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = 0;
					break;
				}
				else if (x >= 430 && x <= 880 && y >= 300 && y <= 360)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					e.accountNum[0] = '\0';
					temp = NULL;
					step = LOGACCSTEP;
					break;
				}
				else if (x >= 430 && x <= 740 && y >= 160 && y <= 220)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					e.userName[0] = '\0';
					step = MAKEACCSTEP;
					break;
				}
			}
		}
		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == LOGACCSTEP)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;
				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0)
						clear_screen(0);
					drawline(100, 400, xloc + 222, 555);
					draw_string(200, 10, (char *)"ENTER YOUR ACCOUNT", 18, 65535, 9, 10, 2);
					draw_string(880, 120, (char *)"ENTER", 5, 6, 9, 10, 2);
					draw_string(400, 50, (char *)"2", 1, 6, 9, 10, 2);
					draw_string(250, 50, (char *)"1", 1, 6, 9, 10, 2);
					draw_string(550, 50, (char *)"3", 1, 6, 9, 10, 2);
					draw_string(400, 100, (char *)"5", 1, 6, 9, 10, 2);
					draw_string(250, 100, (char *)"4", 1, 6, 9, 10, 2);
					draw_string(550, 100, (char *)"6", 1, 6, 9, 10, 2);
					draw_string(400, 150, (char *)"8", 1, 6, 9, 10, 2);
					draw_string(250, 150, (char *)"7", 1, 6, 9, 10, 2);
					draw_string(550, 150, (char *)"9", 1, 6, 9, 10, 2);
					draw_string(400, 200, (char *)"0", 1, 6, 9, 10, 2);
					draw_string(550, 200, (char *)"D", 1, 6, 9, 10, 2);

					drawline(600, 290, 1480, 290);
					drawline(600, 291, 1480, 291);
					drawline(600, 292, 1480, 292);
					drawline(600, 293, 1480, 293);
					draw_string(620, 255, e.accountNum, strlen(e.accountNum), 6, 9, 10, 2);

					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);
					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			//step backwarwd to step 0
			if (step == LOGACCSTEP)
			{
				if (x >= 800 && x <= 940 && y >= 0 && y <= 60)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = 0;
				}
				else if (y >= 100 - 10 && y <= 170)
				{
					if (x >= 125 - 10 && x <= 135 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "1");
						else
							strcat(e.accountNum, "1");
						x = 0;
						y = 0;
					}
					else if (x >= 200 - 10 && x <= 210 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "2");
						else
							strcat(e.accountNum, "2");
						x = 0;
						y = 0;
					}
					else if (x >= 275 - 10 && x <= 285 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "3");
						else
							strcat(e.accountNum, "3");
						x = 0;
						y = 0;
					}
				}
				else if (y >= 200 - 10 && y <= 270)
				{
					if (x >= 150 - 5 && x <= 160 + 5)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "4");
						else
							strcat(e.accountNum, "4");
						x = 0;
						y = 0;
					}
					else if (x >= 200 - 10 && x <= 210 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "5");
						else
							strcat(e.accountNum, "5");
						x = 0;
						y = 0;
					}
					else if (x >= 275 - 10 && x <= 285 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "6");
						else
							strcat(e.accountNum, "6");
						x = 0;
						y = 0;
					}
				}
				else if (y >= 300 - 10 && y <= 370)
				{
					if (x >= 125 - 10 && x <= 135 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "7");
						else
							strcat(e.accountNum, "7");
						x = 0;
						y = 0;
					}
					else if (x >= 200 - 10 && x <= 210 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "8");
						else
							strcat(e.accountNum, "8");
						x = 0;
						y = 0;
					}
					else if (x >= 275 - 10 && x <= 285 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "9");
						else
							strcat(e.accountNum, "9");
						x = 0;
						y = 0;
					}
				}
				else if (x >= 200 - 10 && x <= 210 + 10 && y >= 400 - 10 && y <= 470)
				{
					if (e.accountNum == NULL)
						strcpy(e.accountNum, "0");
					else
						strcat(e.accountNum, "0");
					x = 0;
					y = 0;
				}
				else if (x >= 275 - 10 && x <= 285 + 10 && y >= 400 - 10 && y <= 470)
				{
					e.accountNum[0] = '\0';
					clrcnt = 0;
					x = 0;
					y = 0;
				}
				else if (x >= 430 && x <= 520 && y >= 240 - 5 && y <= 300 + 5)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					temp = searchBST(root, e);
					if (temp != NULL)
					{
						printf("\n%s", temp->key.userName);
						printf("\n%s", temp->key.accountNum);
						step = PASSWDSTEP;
					}else{
						
					}
				}
			}
		}
		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

        /*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == PASSWDSTEP)     //read fnd & dip switch input to authorize account
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;

				

				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0)
						clear_screen(0);
						
					drawline(100, 400, xloc + 222, 555);
					draw_string(880, 140, (char *)"ENTER YOUR PASSWORD", 19, 6, 9, 10, 2);
					draw_string(880, 200, (char *)"ENTER OTP NUM", 13, 6, 9, 10, 2);
					draw_string(400, 50, (char *)"B", 1, 6, 9, 10, 2);
					draw_string(400, 100, (char *)"A", 1, 6, 9, 10, 2);
					draw_string(400, 150, (char *)"S", 1, 6, 9, 10, 2);
					draw_string(400, 200, (char *)"S", 1, 6, 9, 10, 2);
					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);
					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			dec(temp->key.randNum, temp->key.passwd, password);
			if(step == PASSWDSTEP)
			{
				section = password_section;
				while(!next);
				count_stop = stop_sign;
				section = match_section;
					if(password[0] == passwd_input[0] &&
						password[1] == passwd_input[1] &&
						password[2] == passwd_input[2] &&
						password[3] == passwd_input[3])
					{
						compare_mat = pass_match;
						for(int i = 0; i < 4; i++)
							passwd_input[i] = '0';
					}
					else 
					{
						compare_mat = pass_dismatch;
						for(int i = 0; i < 4; i++)
							passwd_input[i] = '0';
					}
				//next = 1;
			}
		}
		/*--------------------------Get Touch And Redraw Display Here-------------------------*/



		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == MAKEACCSTEP)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;
				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0)
						clear_screen(0);
					drawline(100, 400, xloc + 222, 555);
					draw_string(600, 10, (char *)"MAKE NEW ACCOUNT", 16, 6, 9, 10, 2);
					draw_string(1080, 50, (char *)"ENTER YOUR NAME", 15, 6, 9, 10, 2);
					draw_string(400, 50, (char *)"B", 1, 6, 9, 10, 2);
					draw_string(400, 100, (char *)"A", 1, 6, 9, 10, 2);
					draw_string(400, 150, (char *)"S", 1, 6, 9, 10, 2);
					draw_string(400, 200, (char *)"S", 1, 6, 9, 10, 2);

					draw_string(880, 150 - 50, (char *)"Q", 1, 6, 9, 10, 2);
					draw_string(980, 150 - 50, (char *)"W", 1, 6, 9, 10, 2);
					draw_string(1080, 150 - 50, (char *)"E", 1, 6, 9, 10, 2);
					draw_string(1180, 150 - 50, (char *)"R", 1, 6, 9, 10, 2);
					draw_string(1280, 150 - 50, (char *)"T", 1, 6, 9, 10, 2);
					draw_string(1380, 150 - 50, (char *)"Y", 1, 6, 9, 10, 2);
					draw_string(1480, 150 - 50, (char *)"U", 1, 6, 9, 10, 2);
					draw_string(1580, 150 - 50, (char *)"I", 1, 6, 9, 10, 2);
					draw_string(1680, 150 - 50, (char *)"O", 1, 6, 9, 10, 2);
					draw_string(1780, 150 - 50, (char *)"P", 1, 6, 9, 10, 2);

					draw_string(880 + 50, 200 - 50, (char *)"A", 1, 6, 9, 10, 2);
					draw_string(980 + 50, 200 - 50, (char *)"S", 1, 6, 9, 10, 2);
					draw_string(1080 + 50, 200 - 50, (char *)"D", 1, 6, 9, 10, 2);
					draw_string(1180 + 50, 200 - 50, (char *)"F", 1, 6, 9, 10, 2);
					draw_string(1280 + 50, 200 - 50, (char *)"G", 1, 6, 9, 10, 2);
					draw_string(1380 + 50, 200 - 50, (char *)"H", 1, 6, 9, 10, 2);
					draw_string(1480 + 50, 200 - 50, (char *)"J", 1, 6, 9, 10, 2);
					draw_string(1580 + 50, 200 - 50, (char *)"K", 1, 6, 9, 10, 2);
					draw_string(1680 + 50, 200 - 50, (char *)"L", 1, 6, 9, 10, 2);

					draw_string(980 + 50, 250 - 50, (char *)"Z", 1, 6, 9, 10, 2);
					draw_string(1080 + 50, 250 - 50, (char *)"X", 1, 6, 9, 10, 2);
					draw_string(1180 + 50, 250 - 50, (char *)"C", 1, 6, 9, 10, 2);
					draw_string(1280 + 50, 250 - 50, (char *)"V", 1, 6, 9, 10, 2);
					draw_string(1380 + 50, 250 - 50, (char *)"B", 1, 6, 9, 10, 2);
					draw_string(1480 + 50, 250 - 50, (char *)"N", 1, 6, 9, 10, 2);
					draw_string(1580 + 50, 250 - 50, (char *)"M", 1, 6, 9, 10, 2);

					drawline(600, 290, 1480, 290);
					drawline(600, 291, 1480, 291);
					drawline(600, 292, 1480, 292);
					drawline(600, 293, 1480, 293);
					draw_string(620, 255, e.userName, strlen(e.userName), 6, 9, 10, 2);

					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);
					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			//step backwarwd to step 0
			if (step == MAKEACCSTEP)
			{
				if (x >= 800 && x <= 940 && y >= 0 && y <= 60)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = 0;
				}
				else if (y >= 200 - 5 && y <= 260 + 5)
				{
					if (x >= 435 && x <= 455)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "Q");
						else
							strcat(e.userName, "Q");
						x = 0;
						y = 0;
					}
					else if (x >= 490 - 5 && x <= 500 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "W");
						else
							strcat(e.userName, "W");
						x = 0;
						y = 0;
					}
					else if (x >= 540 - 5 && x <= 550 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "E");
						else
							strcat(e.userName, "E");
						x = 0;
						y = 0;
					}
					else if (x >= 590 - 5 && x <= 600 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "R");
						else
							strcat(e.userName, "R");
						x = 0;
						y = 0;
					}
					else if (x >= 640 - 5 && x <= 650 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "T");
						else
							strcat(e.userName, "T");
						x = 0;
						y = 0;
					}
					else if (x >= 690 - 5 && x <= 700 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "Y");
						else
							strcat(e.userName, "Y");
						x = 0;
						y = 0;
					}
					else if (x >= 740 - 5 && x <= 750 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "U");
						else
							strcat(e.userName, "U");
						x = 0;
						y = 0;
					}
					else if (x >= 790 - 5 && x <= 800 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "I");
						else
							strcat(e.userName, "I");
						x = 0;
						y = 0;
					}
					else if (x >= 840 - 5 && x <= 850 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "O");
						else
							strcat(e.userName, "O");
						x = 0;
						y = 0;
					}
					else if (x >= 890 - 5 && x <= 900 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "P");
						else
							strcat(e.userName, "P");
						x = 0;
						y = 0;
					}
				}
				else if (y >= 300 - 5 && y <= 360 + 5)
				{
					if (x >= 465 - 5 && x <= 475 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "A");
						else
							strcat(e.userName, "A");
						x = 0;
						y = 0;
					}
					else if (x >= 515 - 5 && x <= 525 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "S");
						else
							strcat(e.userName, "S");
						x = 0;
						y = 0;
					}
					else if (x >= 565 - 5 && x <= 575 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "D");
						else
							strcat(e.userName, "D");
						x = 0;
						y = 0;
					}
					else if (x >= 615 - 5 && x <= 625 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "F");
						else
							strcat(e.userName, "F");
						x = 0;
						y = 0;
					}
					else if (x >= 665 - 5 && x <= 675 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "G");
						else
							strcat(e.userName, "G");
						x = 0;
						y = 0;
					}
					else if (x >= 715 - 5 && x <= 725 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "H");
						else
							strcat(e.userName, "H");
						x = 0;
						y = 0;
					}
					else if (x >= 765 - 5 && x <= 775 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "J");
						else
							strcat(e.userName, "J");
						x = 0;
						y = 0;
					}
					else if (x >= 815 - 5 && x <= 825 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "K");
						else
							strcat(e.userName, "K");
						x = 0;
						y = 0;
					}
					else if (x >= 865 - 5 && x <= 875 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "L");
						else
							strcat(e.userName, "L");
						x = 0;
						y = 0;
					}
				}
				else if (y >= 400 - 5 && y <= 460 + 5)
				{
					if (x >= 515 - 5 && x <= 525)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "Z");
						else
							strcat(e.userName, "Z");
						x = 0;
						y = 0;
					}
					else if (x >= 565 - 5 && x <= 575 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "X");
						else
							strcat(e.userName, "X");
						x = 0;
						y = 0;
					}
					else if (x >= 615 - 5 && x <= 625 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "C");
						else
							strcat(e.userName, "C");
						x = 0;
						y = 0;
					}
					else if (x >= 665 - 5 && x <= 675 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "V");
						else
							strcat(e.userName, "V");
						x = 0;
						y = 0;
					}
					else if (x >= 715 - 5 && x <= 725 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "B");
						else
							strcat(e.userName, "B");
						x = 0;
						y = 0;
					}
					else if (x >= 765 - 5 && x <= 775 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "N");
						else
							strcat(e.userName, "N");
						x = 0;
						y = 0;
					}
					else if (x >= 815 - 5 && x <= 825 + 5)
					{
						if (e.userName == NULL)
							strcpy(e.userName, "M");
						else
							strcat(e.userName, "M");
						x = 0;
						y = 0;
					}
				}
				else if (x >= 540 && x <= 820 && y >= 100 && y <= 150)
				{
					tmp = system("./prng");
					sprintf(e.accountNum, "%d", tmp);
					tmp = system("./prng");
					sprintf(tmp2, "%d", tmp);
					strcat(e.accountNum, tmp2);
					e.money = 500000;
					e.transCnt = 0;

					/*****Encrypt Passwd*****/

					printf("\n\n/*****Encrypt Passwd*****/\n");
					passwd = system("./prng");
					if (passwd > 9999)
						passwd /= 10;

					sprintf(e.passwd, "%d", passwd);

					for(int j = 0; j < 3; j++){
						passwd = system("./prng");
						if (passwd > 9999)
							passwd /= 10;


						sprintf(temp_str, "%d", passwd);

						strcat(e.passwd, temp_str);
					}

					printf("\nStr : %s\n", e.passwd);
					rannum = system("./prng");

					if (rannum > 9999)
						rannum /= 10;

					//printf("\n%d\n", rannum);

					e.randNum[0] = (rannum % 10) * 10 + (rannum % 10);
					e.randNum[1] = (rannum / 10)%100;
					e.randNum[2] = (rannum / 100);
					e.randNum[3] = (rannum / 1000) * 10 + (rannum / 1000);

					for(j = 0; j < 4; j++){
						if(e.randNum[j] > 37)
							e.randNum[j] %= 10;
						printf("\nrand %d is : %d\n", j+1 ,e.randNum[j]);
					}

					for (j = 0; (j < 100 && e.passwd[j] != '\0'); j++)
						e.passwd[j] = e.passwd[j] + e.randNum[j % 4]; //the key for encryption is 3 that is added to ASCII value

					printf("\nEncrypted string: %s\n", e.passwd);
					printf("\n\n/*****Encrypt Passwd End*****/\n");

					/*****Initialize Transinfo*****/

					for (int i = 0; i < 10; i++)
					{
						e.transinfo[i] = (transInfo *)malloc(sizeof(transInfo));
					}
					insert(&root, e);
					clrcnt = 0;
					x = 0;
					y = 0;
					step = SHOWACCINFOSTEP;
				}
			}
		}
		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == SHOWACCINFOSTEP)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;

				/*****Decrypt Passwd*****/
				dec(e.randNum, e.passwd, dec_pw);

				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0)
						clear_screen(0);
					drawline(100, 400, xloc + 222, 555);
					draw_string(880, 40, (char *)"ACCOUNT NUMBER", 14, 6, 9, 10, 2);
					draw_string(880, 80, e.accountNum, strlen(e.accountNum), 6, 9, 10, 2);
					draw_string(880, 140, (char *)"PASSWORD", 8, 6, 9, 10, 2);
					draw_string(880, 180, dec_pw, strlen(dec_pw), 6, 9, 10, 2);
					draw_string(400, 50, (char *)"B", 1, 6, 9, 10, 2);
					draw_string(400, 100, (char *)"A", 1, 6, 9, 10, 2);
					draw_string(400, 150, (char *)"S", 1, 6, 9, 10, 2);
					draw_string(400, 200, (char *)"S", 1, 6, 9, 10, 2);
					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);
					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			//step backwarwd to step 0
			while (1)
			{
				if (x >= 800 && x <= 940 && y >= 0 && y <= 60)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = 0;
					break;
				}
			}
		}
		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == LBALERT)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;

				/*****Decrypt Passwd*****/
				dec(e.randNum, e.passwd, dec_pw);

				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0)
						clear_screen(0);
					drawline(100, 400, xloc + 222, 555);
					draw_string(880, 40, (char *)"ALERT",5, 6, 9, 10, 2);
					draw_string(880, 80, (char *)"ALERT",5, 250, 9, 10, 2);
					draw_string(880, 120, (char *)"ALERT",5, 2250, 9, 10, 2);
					draw_string(500, 180, (char *)"LACK OF BALANCE ALERT", 21, 6, 9, 10, 2);
					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);
					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			sleep(3);
			step = MONEYSTEP;
		}
		/*--------------------------Get Touch And Redraw Display Here-------------------------*/


		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == SENDALERT)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;

				/*****Decrypt Passwd*****/
				dec(e.randNum, e.passwd, dec_pw);

				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0)
						clear_screen(0);
					drawline(100, 400, xloc + 222, 555);
					draw_string(880, 40, (char *)"ALERT",5, 6, 9, 10, 2);
					draw_string(880, 80, (char *)"ALERT",5, 250, 9, 10, 2);
					draw_string(880, 120, (char *)"ALERT",5, 2250, 9, 10, 2);
					draw_string(400, 180, tmp_money, strlen(tmp_money), 2250, 9, 10, 2);
					draw_string(800, 180, (char *)"SEND SUCCESSFULLY", 17, 6, 9, 10, 2);
					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);
					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			sleep(3);
			tmp_money[0] = '\0';
			step = 0;
		}
		/*--------------------------Get Touch And Redraw Display Here-------------------------*/
		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == SELECTSTEP)
		{
			otpflag = 0;
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;
				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0)
						clear_screen(0);
					drawline(100, 400, xloc + 222, 555);
					draw_string(880, 40, (char *)"CHECK BALANCE", 13, 6, 9, 10, 2);
					draw_string(880, 120, (char *)"CHECK TRANSACTION HISTORY", 25, 6, 9, 10, 2);
					draw_string(880, 200, (char *)"SEND", 4, 6, 9, 10, 2);
					draw_string(400, 50, (char *)"B", 1, 6, 9, 10, 2);
					draw_string(400, 100, (char *)"A", 1, 6, 9, 10, 2);
					draw_string(400, 150, (char *)"S", 1, 6, 9, 10, 2);
					draw_string(400, 200, (char *)"S", 1, 6, 9, 10, 2);
					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);
					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			//step backwarwd to step 0
			while (1)
			{
				if (x >= 800 && x <= 940 && y >= 0 && y <= 60)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = 0;
				}
				
				if (x >= 430 && x <= 670 && y >= 90 && y <= 150)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = CHECKBALANCESTEP;
					break;
				}
				
				if (x >= 150 && x <= 250 && y >= 90 && y <= 150)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					break;
				}
				if (x >= 430 && x <= 870 && y >= 240 && y <= 300)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = CHECKHISTORYSTEP;
					break;
				}
				else if (x >= 430 && x <= 520 && y >= 390 && y <= 460)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					e.accountNum[0] = '\0';
					step = CHECKSENDSTEP;
					break;
				}
			}
		}

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == CHECKHISTORYSTEP)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;
				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0)
						clear_screen(0);
					drawline(100, 400, xloc + 222, 555);
					
					draw_string(50, 10, (char *)"BACK", 4, 6, 9, 10, 2);
					draw_string(400, 50, (char *)"YOUR ACCOUNT", 12, 6, 9, 10, 2);
					draw_string(1200, 50, temp->key.accountNum, strlen(temp->key.accountNum), 6, 9, 10, 2);
					draw_string(400, 100, (char *)"YOUR TRANSACTION HISTORY", 24, 6, 9, 10, 2);
					
					for(int k=0; k<10; k++){
						if(temp->key.transinfo[k]->money != 0){
							sprintf(tmp2, "%d", temp->key.transinfo[k]->money);
							draw_string(600, (140+(k*20)), tmp2, strlen(tmp2), 6, 9, 10, 1);
							draw_string(400, (140+(k*20)), temp->key.transinfo[k]->transName, strlen(temp->key.transinfo[k]->transName), 6, 9, 10, 1);
						}
					}
					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);
					drawline(590, 25, 590, 290);
					drawline(591, 25, 591, 290);
					drawline(592, 25, 592, 290);
					drawline(593, 25, 593, 290);

					drawline(1190, 50, 1190, 290);
					drawline(1191, 50, 1191, 290);
					drawline(1192, 50, 1192, 290);
					drawline(1193, 50, 1193, 290);
					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			//step backwarwd to step 0
			if (step == CHECKHISTORYSTEP)
			{
				if (x >= 800 && x <= 940 && y >= 0 && y <= 60)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = 0;
				}else if (x >= 20 && x <= 200 && y >= 0 && y <= 100)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = SELECTSTEP;
				}
			}
		}

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/


		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == CHECKBALANCESTEP)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;
				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0)
						clear_screen(0);
					drawline(100, 400, xloc + 222, 555);
					
					draw_string(50, 10, (char *)"BACK", 4, 6, 9, 10, 2);
					draw_string(400, 50, (char *)"YOUR ACCOUNT", 12, 6, 9, 10, 2);
					draw_string(1200, 50, temp->key.accountNum, strlen(temp->key.accountNum), 6, 9, 10, 2);
					draw_string(400, 100, (char *)"YOUR BALANCE", 12, 6, 9, 10, 2);
					sprintf(tmp2, "%d", temp->key.money);
					draw_string(1200, 100, tmp2, strlen(tmp2), 6, 9, 10, 2);
					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);

					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			//step backwarwd to step 0
			if (step == CHECKBALANCESTEP)
			{
				if (x >= 800 && x <= 940 && y >= 0 && y <= 60)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = 0;
				}else if (x >= 20 && x <= 200 && y >= 0 && y <= 100)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = SELECTSTEP;
				}

			}
		}

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == CHECKSENDSTEP)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;
				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0)
						clear_screen(0);
					drawline(100, 400, xloc + 222, 555);
					
					draw_string(200, 10, (char *)"ENTER ACC NUM", 13, 65535, 9, 10, 2);
					draw_string(880, 120, (char *)"ENTER", 5, 6, 9, 10, 2);
					draw_string(50, 250, (char *)"BACK", 4, 6, 9, 10, 2);
					draw_string(400, 50, (char *)"2", 1, 6, 9, 10, 2);
					draw_string(250, 50, (char *)"1", 1, 6, 9, 10, 2);
					draw_string(550, 50, (char *)"3", 1, 6, 9, 10, 2);
					draw_string(400, 100, (char *)"5", 1, 6, 9, 10, 2);
					draw_string(250, 100, (char *)"4", 1, 6, 9, 10, 2);
					draw_string(550, 100, (char *)"6", 1, 6, 9, 10, 2);
					draw_string(400, 150, (char *)"8", 1, 6, 9, 10, 2);
					draw_string(250, 150, (char *)"7", 1, 6, 9, 10, 2);
					draw_string(550, 150, (char *)"9", 1, 6, 9, 10, 2);
					draw_string(400, 200, (char *)"0", 1, 6, 9, 10, 2);
					draw_string(550, 200, (char *)"D", 1, 6, 9, 10, 2);

					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);

					drawline(600, 290, 1480, 290);
					drawline(600, 291, 1480, 291);
					drawline(600, 292, 1480, 292);
					drawline(600, 293, 1480, 293);
					draw_string(620, 255, e.accountNum, strlen(e.accountNum), 6, 9, 10, 2);
					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			//step backwarwd to step 0
			if (step == CHECKSENDSTEP)
			{
				if (x >= 800 && x <= 940 && y >= 0 && y <= 60)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					e.accountNum[0] = '\0';
					step = 0;
				}
				else if (y >= 100 - 10 && y <= 170)
				{
					if (x >= 125 - 10 && x <= 135 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "1");
						else
							strcat(e.accountNum, "1");
						x = 0;
						y = 0;
					}
					else if (x >= 200 - 10 && x <= 210 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "2");
						else
							strcat(e.accountNum, "2");
						x = 0;
						y = 0;
					}
					else if (x >= 275 - 10 && x <= 285 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "3");
						else
							strcat(e.accountNum, "3");
						x = 0;
						y = 0;
					}
				}
				else if (y >= 200 - 10 && y <= 270)
				{
					if (x >= 150 - 5 && x <= 160 + 5)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "4");
						else
							strcat(e.accountNum, "4");
						x = 0;
						y = 0;
					}
					else if (x >= 200 - 10 && x <= 210 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "5");
						else
							strcat(e.accountNum, "5");
						x = 0;
						y = 0;
					}
					else if (x >= 275 - 10 && x <= 285 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "6");
						else
							strcat(e.accountNum, "6");
						x = 0;
						y = 0;
					}
				}
				else if (y >= 300 - 10 && y <= 370)
				{
					if (x >= 125 - 10 && x <= 135 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "7");
						else
							strcat(e.accountNum, "7");
						x = 0;
						y = 0;
					}
					else if (x >= 200 - 10 && x <= 210 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "8");
						else
							strcat(e.accountNum, "8");
						x = 0;
						y = 0;
					}
					else if (x >= 275 - 10 && x <= 285 + 10)
					{
						if (e.accountNum == NULL)
							strcpy(e.accountNum, "9");
						else
							strcat(e.accountNum, "9");
						x = 0;
						y = 0;
					}
				}
				else if (x >= 200 - 10 && x <= 210 + 10 && y >= 400 - 10 && y <= 470)
				{
					if (e.accountNum == NULL)
						strcpy(e.accountNum, "0");
					else
						strcat(e.accountNum, "0");
					x = 0;
					y = 0;
				}
				else if (x >= 275 - 10 && x <= 285 + 10 && y >= 400 - 10 && y <= 470)
				{
					e.accountNum[0] = '\0';
					clrcnt = 0;
					x = 0;
					y = 0;
				}

				else if (x >= 430 && x <= 520 && y >= 240 - 5 && y <= 300 + 5)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					temp2 = searchBST(root, e);
					if (temp2 != NULL)
					{
						printf("\n%s", temp2->key.userName);
						step = MONEYSTEP;
					}else{
						
					}
				}else if (x >= 20 && x <= 200 && y >= 500 && y <= 590)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = SELECTSTEP;
				}
			}
		}

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		// draw...
		//-----------------------------------------------------------graphics loop here

		//	draw();
		if (step == MONEYSTEP)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;
				for (int i = 1; i < 3; i++)
				{
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					if (clrcnt == 0){
						clear_screen(0);
						sleep(0.1);
					}
					drawline(100, 400, xloc + 222, 555);
					
					draw_string(200, 10, (char *)"SEND MONEY", 10, 65535, 9, 10, 2);
					draw_string(880, 120, (char *)"SEND", 4, 6, 9, 10, 2);
					draw_string(50, 255, (char *)"BACK", 4, 6, 9, 10, 2);
					draw_string(400, 50, (char *)"2", 1, 6, 9, 10, 2);
					draw_string(250, 50, (char *)"1", 1, 6, 9, 10, 2);
					draw_string(550, 50, (char *)"3", 1, 6, 9, 10, 2);
					draw_string(400, 100, (char *)"5", 1, 6, 9, 10, 2);
					draw_string(250, 100, (char *)"4", 1, 6, 9, 10, 2);
					draw_string(550, 100, (char *)"6", 1, 6, 9, 10, 2);
					draw_string(400, 150, (char *)"8", 1, 6, 9, 10, 2);
					draw_string(250, 150, (char *)"7", 1, 6, 9, 10, 2);
					draw_string(550, 150, (char *)"9", 1, 6, 9, 10, 2);
					draw_string(400, 200, (char *)"0", 1, 6, 9, 10, 2);
					draw_string(550, 200, (char *)"D", 1, 6, 9, 10, 2);
					draw_string(1650, 10, (char *)"BACK TO MAIN", 12, 6, 9, 10, 1);

					drawline(600, 290, 1480, 290);
					drawline(600, 291, 1480, 291);
					drawline(600, 292, 1480, 292);
					drawline(600, 293, 1480, 293);
					draw_string(620, 255, tmp_money, strlen(tmp_money), 6, 9, 10, 2);
					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}
			
			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			//step backwarwd to step 0
			if (step == MONEYSTEP)
			{
				if (x >= 800 && x <= 940 && y >= 0 && y <= 60)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					step = 0;
				}
				else if (y >= 100 - 10 && y <= 170)
				{
					if (x >= 125 - 10 && x <= 135 + 10)
					{
						if (tmp_money == NULL)
							strcpy(tmp_money, "1");
						else
							strcat(tmp_money, "1");
						x = 0;
						y = 0;
					}
					else if (x >= 200 - 10 && x <= 210 + 10)
					{
						if (tmp_money == NULL)
							strcpy(tmp_money, "2");
						else
							strcat(tmp_money, "2");
						x = 0;
						y = 0;
					}
					else if (x >= 275 - 10 && x <= 285 + 10)
					{
						if (tmp_money == NULL)
							strcpy(tmp_money, "3");
						else
							strcat(tmp_money, "3");
						x = 0;
						y = 0;
					}
				}
				else if (y >= 200 - 10 && y <= 270)
				{
					if (x >= 125 - 10 && x <= 135 + 10)
					{
						if (tmp_money == NULL)
							strcpy(tmp_money, "4");
						else
							strcat(tmp_money, "4");
						x = 0;
						y = 0;
					}
					else if (x >= 200 - 10 && x <= 210 + 10)
					{
						if (tmp_money == NULL)
							strcpy(tmp_money, "5");
						else
							strcat(tmp_money, "5");
						x = 0;
						y = 0;
					}
					else if (x >= 275 - 10 && x <= 285 + 10)
					{
						if (tmp_money == NULL)
							strcpy(tmp_money, "6");
						else
							strcat(tmp_money, "6");
						x = 0;
						y = 0;
					}
				}
				else if (y >= 300 - 10 && y <= 370)
				{
					if (x >= 125 - 10 && x <= 135 + 10)
					{
						if (tmp_money == NULL)
							strcpy(tmp_money, "7");
						else
							strcat(tmp_money, "7");
						x = 0;
						y = 0;
					}
					else if (x >= 200 - 10 && x <= 210 + 10)
					{
						if (tmp_money == NULL)
							strcpy(tmp_money, "8");
						else
							strcat(tmp_money, "8");
						x = 0;
						y = 0;
					}
					else if (x >= 275 - 10 && x <= 285 + 10)
					{
						if (tmp_money == NULL)
							strcpy(tmp_money, "9");
						else
							strcat(tmp_money, "9");
						x = 0;
						y = 0;
					}
				}
				else if (x >= 200 - 10 && x <= 210 + 10 && y >= 400 - 10 && y <= 470)
				{
					if (tmp_money == NULL)
						strcpy(tmp_money, "0");
					else
						strcat(tmp_money, "0");
					x = 0;
					y = 0;
				}
				else if (x >= 275 - 10 && x <= 285 + 10 && y >= 400 - 10 && y <= 470)
				{
					tmp_money[0] = '\0';
					clrcnt = 0;
					x = 0;
					y = 0;
				}else if (x >= 20 && x <= 200 && y >= 500 && y <= 590)
				{
					clrcnt = 0;
					tmp_money[0] - '\0';
					x = 0;
					y = 0;
					step = SELECTSTEP;
				}
				else if (x >= 430 && x <= 520 && y >= 240 - 5 && y <= 300 + 5)
				{
					clrcnt = 0;
					x = 0;
					y = 0;
					e.money = atoi(tmp_money);

					if(temp->key.money < e.money)
						step = LBALERT;


					temp2->key.money = temp2->key.money + e.money;
					strcpy(temp2->key.transinfo[temp2->key.transCnt]->transName, temp->key.userName);
					temp2->key.transinfo[temp2->key.transCnt]->money = e.money;
					temp2->key.transCnt++;

					temp->key.money = temp->key.money - e.money;
					strcpy(temp->key.transinfo[temp->key.transCnt]->transName, temp2->key.userName);
					temp->key.transinfo[temp->key.transCnt]->money = e.money;
					temp->key.transCnt++;
					
					step = DRAGMONEYSTEP;
				}
			}
		}

		/*--------------------------Get Touch And Redraw Display Here-------------------------*/

		if (step == DRAGMONEYSTEP)
		{
			screensize = finfo.smem_len;
			fbp = (char *)mmap(0,
							   screensize,
							   PROT_READ | PROT_WRITE,
							   MAP_SHARED,
							   fbfd,
							   0);
			if ((int)fbp == -1)
			{
				printf("Failed to mmap\n");
			}
			else
			{
				// draw...
				//-----------------------------------------------------------graphics loop here

				//	draw();

				int fps = 60;
				int secs = 10;
				int xloc = 1;
				int yloc = 1;
				for (int i = 1; i < 3; i++)
				{
					if (clrcnt == 0)
						clear_screen(0);
					// change page to draw to (between 0 and 1)
					cur_page = (cur_page + 1) % 2;
					// clear the previous image (= fill entire screen)
					drawline(100, 400, xloc + 222, 555);
					draw_char(money, (cont_x*2-120), (cont_y/2-25), 350, 70, 6);
					draw_char(locker, 1460, 125, 150, 50, 6);

					// switch page
					vinfo.yoffset = cur_page * vinfo.yres;
					ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
					// the call to waitforvsync should use a pointer to a variable
					// https://www.raspberrypi.org/forums/viewtopic.php?f=67&t=19073&p=887711#p885821
					// so should be in fact like this:
					__u32 dummy = 0;
					ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
					// also should of course check the return values of the ioctl calls...
					if (yloc >= vinfo.yres / 2)
						yloc = 1;
					if (xloc >= 100)
						yloc = 1;
					yloc++;
					xloc++;
				}
				clrcnt = 1;
				//-----------------------------------------------------------graphics loop here
			}

			// unmap fb file from memory
			munmap(fbp, screensize);
			// reset cursor
			if (kbfd >= 0)
			{
				ioctl(kbfd, KDSETMODE, KD_TEXT);
				// close kb file
				close(kbfd);
			}
			// reset the display mode
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
			{
				printf("Error re-setting variable information.\n");
			}

			//step forwarwd to  step 1
				//if (x >= 430)
				//{
				//}
			if(step == DRAGMONEYSTEP){
				cont_cnt++;
				if(cont_cnt > 100){
					clrcnt = 0;
				}	
				if(cont_x >= 730 && cont_x <= 780 && cont_y >= 190 && cont_y<= 310){
					clrcnt = 0;
					x=0;
					y=0;
					step = SENDALERT;
				}
			}
		}
	}
}

//TODO : Make Main Thread.

/*TODO : fnd Ãâ·Â, Çª½Ã¹öÆ° ÀÔ·Â(1~9±îÁö), dip½ºÀ§Ä¡ ÀÔ·Â, ÅØ½ºÆ® lcdÃâ·Â, 
		 ºÎÀú Ãâ·Â, ¼­º¸¸ðÅÍ, º£ÆÃ, ÄûÁî ¹®Á¦(Çì´õ)
*/
void fnd(char* str) {
	int j;
	unsigned char data1[4];
	ssize_t ret;

	memset(data1, 0, sizeof(data1));

	for(j = 0; j < 4; j++)
		data1[j] = str[j] - '0';
	
	ret = write(dev_fnd, data1, FND_MAX_DIGIT);

	sleep(1);

	memset(data1, 0, sizeof(data1));
	ret = read(dev_fnd, data1, FND_MAX_DIGIT);

}

void reset_fnd(){
	int j;
	unsigned char data1[4] = {0};
	ssize_t ret;

	memset(data1, 0, sizeof(data1));

	ret = write(dev_fnd, data1, FND_MAX_DIGIT);

	sleep(1);

	memset(data1, 0, sizeof(data1));
	ret = read(dev_fnd, data1, FND_MAX_DIGIT);
}

int push_switch(void) {
	unsigned char push_sw_buf[PUSH_SWITCH_MAX_BUTTON];
	int i, data1, quit = 0;

	while (!quit) 
	{
		read(dev_push_switch, &push_sw_buf, sizeof(push_sw_buf));
		for(i = 0; i < PUSH_SWITCH_MAX_BUTTON; i++)
		{
			if(push_sw_buf[i])
			{
				data1 = i + 1;
				quit = 1;
			}
		}
	}
	return data1;
}

void step_motor(unsigned char action, unsigned char dir, unsigned char speed) {
	unsigned char state[3];

	memset(state, 0, sizeof(state));
	state[0] = (unsigned char)action;
	state[1] = (unsigned char)dir;
	state[2] = (unsigned char)speed;
	
	write(dev_step_motor, state, 3);
}

void buzzer(int cnt) {
	unsigned char state;
	int count;

	state = BUZZER_OFF;
	for(count = 1; count <= (cnt*2)+1; count++)
	{
		state = BUZZER_TOGGLE(state);
		write(dev_buzzer, &state, 1);
		sleep(1);
	}
}

void text_lcd(char *first, char *second)
{
	int i;
	int str_size;
	unsigned char string[32];
	
	memset(string,0,sizeof(string));
	
	str_size=strlen(first);
	if(str_size>0) {
		strncat(string,first,str_size);
		memset(string+str_size,' ',TEXT_LCD_LINE_BUF-str_size);
	}
	
	str_size=strlen(second);
	if(str_size>0) {
		strncat(string,second,str_size);
		memset(string+TEXT_LCD_LINE_BUF+str_size,' ',TEXT_LCD_LINE_BUF-str_size);
	}
	write(dev_text_lcd,string,TEXT_LCD_MAX_BUF);
}

int otp_num(void)
{
	int rand_num = 0;
	rand_num = system("./prng");
	if(rand_num<0)
		rand_num = -rand_num;
	rand_num%=255;

	printf("%d\n", rand_num);
	return rand_num;
}

char *intToBinary(int i) 
{
	static char s[8];
	int count = 8;

	do { s[--count] = '0' + (char) (i & 1);
		i = i >> 1;
	} while (count);
	return s;
}

int dip_switch(void) {
	int dip_sw_buf = 0;
	int quit = 0;

	while (!quit) {
		if(push_switch()) quit = 1;
	}
	read(dev_dip_switch, &dip_sw_buf, 1);
	dip_sw_buf = 255 - dip_sw_buf;
	printf("%d\n", dip_sw_buf);
	return dip_sw_buf;
}

void led(char data1) {
	int dev_led;
	ssize_t ret;

	char usage[50];
	
	dev_led = open(LED_DEVICE, O_RDWR);

	ret = write(dev_led, &data1, 1);

	sleep(1);

	ret = read(dev_led, &data1, 1);

	close(dev_led);
}

void dot(int num) {
	ssize_t ret;

	char usage[50];

	dev_dot = open(DOT_DEVICE, O_WRONLY);

	ret = write(dev_dot, fpga_number[num], sizeof(fpga_number[num]));

	close(dev_dot);
}

double betting(double i)
{
	double prof_los = 0;
	int rand_num = system("./prng");
	if(rand_num<0)
		rand_num = -rand_num;
	rand_num %= 100;
	if(rand_num > 97) prof_los = 10;
	else if(rand_num < 97 && rand_num > 95) prof_los = 9;
	else if(rand_num < 95 && rand_num > 93) prof_los = 8;
	else if(rand_num < 93 && rand_num > 91) prof_los = 7;
	else if(rand_num < 91 && rand_num > 89) prof_los = 6;
	else if(rand_num < 89 && rand_num > 87) prof_los = 5;
	else if(rand_num < 87 && rand_num > 85) prof_los = 4;
	else if(rand_num < 85 && rand_num > 83) prof_los = 3;
	else if(rand_num < 83 && rand_num > 80) prof_los = 2;
	else if(rand_num < 80 && rand_num > 50) prof_los = 1;
	else if(rand_num < 50 && rand_num > 30) prof_los = 0.9;
	else if(rand_num < 30 && rand_num > 25) prof_los = 0.8;
	else if(rand_num < 25 && rand_num > 22) prof_los = 0.7;
	else if(rand_num < 22 && rand_num > 18) prof_los = 0.6;
	else if(rand_num < 18 && rand_num > 15) prof_los = 0.5;
	else if(rand_num < 15 && rand_num > 12) prof_los = 0.4;
	else if(rand_num < 12 && rand_num > 9) prof_los = 0.3;
	else if(rand_num < 9 && rand_num > 5) prof_los = 0.2;
	else prof_los = 0.1;

	printf("배팅금액 : %.1f\n", i);
	i *= prof_los;

	printf("배팅결과 : %.1f\n", i);

	return i;
}

void *fpgaThread(void *data)
{
	char *thread_name = (char *)data;	

	int data_ps[5] = {0}, i, otp_int, dip_int;
	char data_str[5] = {'0', '0', '0', '0', '0'};
	char value[4];
	char otp_bi[8] = {0}, dip_bi[8] = {0};
	char *mat = "MATCH!";
	char *dis_mat = "DISMATCH!";

	dev_fnd = open(FND_DEVICE, O_RDWR);
	dev_push_switch = open(PUSH_SWITCH_DEVICE, O_RDONLY);
	dev_step_motor = open(STEP_MOTOR_DEVICE, O_WRONLY);
	dev_buzzer = open(BUZZER_DEVICE, O_RDWR);
	dev_text_lcd = open(TEXT_LCD_DEVICE, O_WRONLY);
	dev_dip_switch = open(DIP_SWITCH_DEVICE, O_RDONLY);
	
	while(1)
	{
		if(section == password_section)
		{
			for(i = 0; i < 5; i++)
			{
				data_ps[i] = push_switch();
				sprintf(data_str, "%d%d%d%d%d", data_ps[0], data_ps[1], data_ps[2], data_ps[3], data_ps[4]);
				fnd(data_str);
			}
			for(i = 0; i < 4; i++)
				sprintf(value, "%d%d%d%d",data_ps[0], data_ps[1], data_ps[2], data_ps[3]);
			printf("%s\n", value);
			reset_fnd();

			//passwd_input = value;
			sprintf(passwd_input, "%d%d%d%d",data_ps[0], data_ps[1], data_ps[2], data_ps[3]);
			printf("%s\n", passwd_input);
			otp_int = otp_num();
			sprintf(otp_bi, "%s", intToBinary(otp_int));
			strcpy(otptmp, otp_bi);
			text_lcd(otp_bi, "");
			otpflag = 1;
			dip_int = dip_switch();
			sprintf(dip_bi, "%s", intToBinary(dip_int));
			if(dip_int == otp_int)	
			{
				text_lcd(mat, dip_bi);
				section = 0;
				next = 1;
			}
			else 
			{
				text_lcd(dis_mat, dip_bi);
				section = 0;
				next = 1;
			}
		}
		else if(section == match_section)
		{
			if(compare_mat == pass_match)
			{
				step_motor(1, 1, 5);
				sleep(3);
				step_motor(0, 1, 5);
				next = 0;
				compare_mat = 0;
				clrcnt = 0;
				sleep(0.1);
				step = SELECTSTEP;
				section = 0;
			} 
			if(compare_mat == pass_dismatch)
			{
				buzzer(2);
				compare_mat = 0;
				next = 0;
			}
			else;
		}
	}

	close(dev_fnd);
	close(dev_push_switch);
	close(dev_step_motor);
	close(dev_buzzer);
	close(dev_text_lcd);
	close(dev_dip_switch);
}

void *countdownThread(void *data){
	char *thread_name = (char *)data;
	int i;
	int dot_val;
	int led_val;
	dev_led = open(LED_DEVICE, O_RDWR);
	dev_dot = open(DOT_DEVICE, O_WRONLY);

	while(1){
		for(i = 30; i >= 0; i--)
		{
			led_val = i / 10;
			dot_val = i % 10;
			led((char)led_val);
			dot(i);
			sleep(0.5);
			if(count_stop == stop_sign)
				break;
		}
		if(i == 0)
		{
			text_lcd("    Time Over   ", empty);
			//buzzer(2);
		}
		dot(0);
	}
	close(dev_led);
	close(dev_dot);
}

int main()
{

	pthread_t p_thread[3];
	int thr_id;
	int status;
	char p1[] = "thread_1"; // 1šř ž˛ˇšľĺ ŔĚ¸§
	char pM[] = "thread_m"; // ¸ŢŔÎ ž˛ˇšľĺ ŔĚ¸§
	char pfpga[] = "thread_fpga";
	//char pcdown[] = "thread_cdown";

	pthread_mutex_init(&mtx, NULL);

	sleep(1);

	thr_id = pthread_create(&p_thread[0], NULL, getTouch, (void *)p1);

	if (thr_id < 0)
	{
		printf("thread create error : getTouch");
		exit(0);
	}

	thr_id = pthread_create(&p_thread[1], NULL, mainThread, (void *)pM);

	if (thr_id < 0)
	{
		printf("thread create error : mainThread");
		exit(0);
	}

	thr_id = pthread_create(&p_thread[2], NULL, fpgaThread, (void *)pfpga);

	if (thr_id < 0)
	{
		printf("thread create error : fpgaThread");
		exit(0);
	}

	//thr_id = pthread_create(&p_thread[3], NULL, countdownThread, (void *)pcdown);
/*
	if (thr_id < 0)
	{
		printf("thread create error : countdownThread");
		exit(0);
	}
*/
	pthread_join(p_thread[0], (void *)&status);
	pthread_join(p_thread[1], (void *)&status);
	pthread_join(p_thread[2], (void *)&status);
	//pthread_join(p_thread[3], (void *)&status);

	return 0; 
}
