/**************************************************************************
 * mouse.c                                                                *
 * written by David Brackeen                                              *
 * http://www.brackeen.com/home/vga/                                      *
 *                                                                        *
 * This is a 16-bit program.                                              *
 * Tab stops are set to 2.                                                *
 * Remember to compile in the LARGE memory model!                         *
 * To compile in Borland C: bcc -ml mouse.c                               *
 *                                                                        *
 * This program will only work on DOS- or Windows-based systems with a    *
 * VGA, SuperVGA or compatible video adapter.                             *
 *                                                                        *
 * Please feel free to copy this source code.                             *
 *                                                                        *
 * DESCRIPTION: This program demostrates mouse functions.                 *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>

#define VIDEO_INT           0x10      /* the BIOS video interrupt. */
#define SET_MODE            0x00      /* BIOS func to set the video mode. */
#define VGA_256_COLOR_MODE  0x13      /* use to set 256-color mode. */
#define TEXT_MODE           0x03      /* use to set 80x25 text mode. */

#define PALETTE_INDEX       0x03c8
#define PALETTE_DATA        0x03c9
#define INPUT_STATUS        0x03da
#define VRETRACE            0x08

#define SCREEN_WIDTH        320       /* width in pixels of mode 0x13 */
#define SCREEN_HEIGHT       200       /* height in pixels of mode 0x13 */
#define NUM_COLORS          256       /* number of colors in mode 0x13 */

#define MOUSE_INT           0x33
#define MOUSE_RESET         0x00
#define MOUSE_GETPRESS      0x05
#define MOUSE_GETRELEASE    0x06
#define MOUSE_GETMOTION     0x0B
#define LEFT_BUTTON         0x00
#define RIGHT_BUTTON        0x01
#define MIDDLE_BUTTON       0x02


#define MOUSE_WIDTH         24
#define MOUSE_HEIGHT        24
#define MOUSE_SIZE          (MOUSE_HEIGHT*MOUSE_WIDTH)

#define BUTTON_WIDTH        48
#define BUTTON_HEIGHT       24
#define BUTTON_SIZE         (BUTTON_HEIGHT*BUTTON_WIDTH)
#define BUTTON_BITMAPS      3
#define STATE_NORM          0
#define STATE_ACTIVE        1
#define STATE_PRESSED       2
#define STATE_WAITING       3

#define NUM_BUTTONS         2
#define NUM_MOUSEBITMAPS    9

typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned long  dword;
typedef short sword;                  /* signed word */

byte *VGA=(byte *)0xA0000000L;        /* this points to video memory. */
word *my_clock=(word *)0x0000046C;    /* this points to the 18.2hz system
                                         clock. */

typedef struct                         /* the structure for a bitmap. */
{
  word width;
  word height;
  byte palette[256*3];
  byte *data;
} BITMAP;
                                      /* the structure for animated
                                         mouse pointers. */
typedef struct tagMOUSEBITMAP MOUSEBITMAP;
struct tagMOUSEBITMAP
{
  int hot_x;
  int hot_y;
  byte data[MOUSE_SIZE];
  MOUSEBITMAP *next;   /* points to the next mouse bitmap, if any */
};

typedef struct            /* the structure for a mouse. */
{
  byte on;
  byte button1;
  byte button2;
  byte button3;
  int num_buttons;
  sword x;
  sword y;
  byte under[MOUSE_SIZE];
  MOUSEBITMAP *bmp;

} MOUSE;

typedef struct              /* the structure for a button. */
{
  int x;
  int y;
  int state;
  byte bitmap[BUTTON_BITMAPS][BUTTON_SIZE];

} BUTTON;

/**************************************************************************
 *  fskip                                                                 *
 *     Skips bytes in a file.                                             *
 **************************************************************************/

void fskip(FILE *fp, int num_bytes)
{
   int i;
   for (i=0; i<num_bytes; i++)
      fgetc(fp);
}

/**************************************************************************
 *  set_mode                                                              *
 *     Sets the video mode.                                               *
 **************************************************************************/

void set_mode(byte mode)
{
  union REGS regs;

  regs.h.ah = SET_MODE;
  regs.h.al = mode;
  int86(VIDEO_INT, &regs, &regs);
}

/**************************************************************************
 *  load_bmp                                                              *
 *    Loads a bitmap file into memory.                                    *
 **************************************************************************/

void load_bmp(char *file,BITMAP *b)
{
  FILE *fp;
  long index;
  word num_colors;
  int x;

  /* open the file */
  if ((fp = fopen(file,"rb")) == NULL)
  {
    printf("Error opening file %s.\n",file);
    exit(1);
  }

  /* check to see if it is a valid bitmap file */
  if (fgetc(fp)!='B' || fgetc(fp)!='M')
  {
    fclose(fp);
    printf("%s is not a bitmap file.\n",file);
    exit(1);
  }

  /* read in the width and height of the image, and the
     number of colors used; ignore the rest */
  fskip(fp,16);
  fread(&b->width, sizeof(word), 1, fp);
  fskip(fp,2);
  fread(&b->height,sizeof(word), 1, fp);
  fskip(fp,22);
  fread(&num_colors,sizeof(word), 1, fp);
  fskip(fp,6);

  /* assume we are working with an 8-bit file */
  if (num_colors==0) num_colors=256;

  /* try to allocate memory */
  if ((b->data = (byte *) malloc((word)(b->width*b->height))) == NULL)
  {
    fclose(fp);
    printf("Error allocating memory for file %s.\n",file);
    exit(1);
  }

  /* read the palette information */
  for(index=0;index<num_colors;index++)
  {
    b->palette[(int)(index*3+2)] = fgetc(fp) >> 2;
    b->palette[(int)(index*3+1)] = fgetc(fp) >> 2;
    b->palette[(int)(index*3+0)] = fgetc(fp) >> 2;
    x=fgetc(fp);
  }

  /* read the bitmap */
  for(index=(b->height-1)*b->width;index>=0;index-=b->width)
    for(x=0;x<b->width;x++)
      b->data[(word)(index+x)]=(byte)fgetc(fp);

  fclose(fp);
}

/**************************************************************************
 *  set_palette                                                           *
 *    Sets all 256 colors of the palette.                                 *
 **************************************************************************/

void set_palette(byte *palette)
{
  int i;

  outp(PALETTE_INDEX,0);              /* tell the VGA that palette data
                                         is coming. */
  for(i=0;i<256*3;i++)
    outp(PALETTE_DATA,palette[i]);    /* write the data */
}

/**************************************************************************
 *  wait_for_retrace                                                      *
 *    Wait until the *beginning* of a vertical retrace cycle (60hz).      *
 **************************************************************************/

void wait_for_retrace(void)
{
    /* wait until done with vertical retrace */
    while  ((inp(INPUT_STATUS) & VRETRACE));
    /* wait until done refreshing */
    while (!(inp(INPUT_STATUS) & VRETRACE));
}

/**************************************************************************
 *  get_mouse_motion                                                      *
 *    Returns the distance the mouse has moved since it was lasted        *
 *    checked.                                                            *
 **************************************************************************/

void get_mouse_motion(sword *dx, sword *dy)
{
  union REGS regs;

  regs.x.ax = MOUSE_GETMOTION;
  int86(MOUSE_INT, &regs, &regs);
  *dx=regs.x.cx;
  *dy=regs.x.dx;
}

/**************************************************************************
 *  init_mouse                                                            *
 *    Resets the mouse.  Returns 0 if mouse not found.                    *
 **************************************************************************/

sword init_mouse(MOUSE *mouse)
{
  sword dx,dy;
  union REGS regs;

  regs.x.ax = MOUSE_RESET;
  int86(MOUSE_INT, &regs, &regs);
  mouse->on=regs.x.ax;
  mouse->num_buttons=regs.x.bx;
  mouse->button1=0;
  mouse->button2=0;
  mouse->button3=0;
  mouse->x=SCREEN_WIDTH/2;
  mouse->y=SCREEN_HEIGHT/2;
  get_mouse_motion(&dx,&dy);
  return mouse->on;
}

/**************************************************************************
 *  get_mouse_press                                                       *
 *    Returns 1 if a button has been pressed since it was last checked.   *
 **************************************************************************/

sword get_mouse_press(sword button)
{
  union REGS regs;

  regs.x.ax = MOUSE_GETPRESS;
  regs.x.bx = button;
  int86(MOUSE_INT, &regs, &regs);
  return regs.x.bx;
}

/**************************************************************************
 *  get_mouse_release                                                     *
 *    Returns 1 if a button has been released since it was last checked.  *
 **************************************************************************/

sword get_mouse_release(sword button)
{
  union REGS regs;

  regs.x.ax = MOUSE_GETRELEASE;
  regs.x.bx = button;
  int86(MOUSE_INT, &regs, &regs);
  return regs.x.bx;
}

/**************************************************************************
 *  show_mouse                                                            *
 *    Displays the mouse.  This code is not optimized.                    *
 **************************************************************************/

void show_mouse(MOUSE *mouse)
{
  int x, y;
  int mx = mouse->x - mouse->bmp->hot_x;
  int my = mouse->y - mouse->bmp->hot_y;
  long screen_offset = (my<<8)+(my<<6);
  word bitmap_offset = 0;
  byte data;

  for(y=0;y<MOUSE_HEIGHT;y++)
  {
    for(x=0;x<MOUSE_WIDTH;x++,bitmap_offset++)
    {
      mouse->under[bitmap_offset] = VGA[(word)(screen_offset+mx+x)];
      /* check for screen boundries */
      if (mx+x < SCREEN_WIDTH  && mx+x >= 0 &&
          my+y < SCREEN_HEIGHT && my+y >= 0)
      {
        data = mouse->bmp->data[bitmap_offset];
        if (data) VGA[(word)(screen_offset+mx+x)] = data;
      }
    }
    screen_offset+=SCREEN_WIDTH;
  }
}

/**************************************************************************
 *  hide_mouse                                                            *
 *    hides the mouse.  This code is not optimized.                       *
 **************************************************************************/

void hide_mouse(MOUSE *mouse)
{
  int x, y;
  int mx = mouse->x - mouse->bmp->hot_x;
  int my = mouse->y - mouse->bmp->hot_y;
  long screen_offset = (my<<8)+(my<<6);
  word bitmap_offset = 0;

  for(y=0;y<MOUSE_HEIGHT;y++)
  {
    for(x=0;x<MOUSE_WIDTH;x++,bitmap_offset++)
    {
      /* check for screen boundries */
      if (mx+x < SCREEN_WIDTH  && mx+x >= 0 &&
          my+y < SCREEN_HEIGHT && my+y >= 0)
      {

        VGA[(word)(screen_offset+mx+x)] = mouse->under[bitmap_offset];
      }
    }

    screen_offset+=SCREEN_WIDTH;
  }
}

/**************************************************************************
 *  draw_button                                                           *
 *    Draws a button.                                                     *
 **************************************************************************/

void draw_button(BUTTON *button)
{
  int x, y;
  word screen_offset = (button->y<<8)+(button->y<<6);
  word bitmap_offset = 0;
  byte data;

  for(y=0;y<BUTTON_HEIGHT;y++)
  {
    for(x=0;x<BUTTON_WIDTH;x++,bitmap_offset++)
    {
      data = button->bitmap[button->state%BUTTON_BITMAPS][bitmap_offset];
      if (data) VGA[screen_offset+button->x+x] = data;
    }
    screen_offset+=SCREEN_WIDTH;
  }
}

/**************************************************************************
 *  Main                                                                  *
 **************************************************************************/

void main()
{
  BITMAP bmp;
  MOUSE  mouse;
  MOUSEBITMAP *mb[NUM_MOUSEBITMAPS],
    *mouse_norm, *mouse_wait, *mouse_new=NULL;

  BUTTON *button[NUM_BUTTONS];
  word redraw;
  sword dx = 0, dy = 0, new_x, new_y;
  word press, release;
  int i,j, done = 0, x,y;
  word last_time;

  for (i=0; i<NUM_MOUSEBITMAPS; i++)
  {
    if ((mb[i] = (MOUSEBITMAP *) malloc(sizeof(MOUSEBITMAP))) == NULL)
    {
      printf("Error allocating memory for bitmap.\n");
      exit(1);
    }
  }

  for (i=0; i<NUM_BUTTONS; i++)
  {
    if ((button[i] = (BUTTON *) malloc(sizeof(BUTTON))) == NULL)
    {
      printf("Error allocating memory for bitmap.\n");
      exit(1);
    }
  }
  mouse_norm = mb[0];
  mouse_wait = mb[1];

  mouse.bmp = mouse_norm;

  button[0]->x     = 48;               /* set button states */
  button[0]->y     = 152;
  button[0]->state = STATE_NORM;

  button[1]->x     = 224;
  button[1]->y     = 152;
  button[1]->state = STATE_NORM;

  if (!init_mouse(&mouse))            /* init mouse */
  {
    printf("Mouse not found.\n");
    exit(1);
  }

  load_bmp("images.bmp",&bmp);        /* load icons */
  set_mode(VGA_256_COLOR_MODE);       /* set the video mode. */


  for(i=0;i<NUM_MOUSEBITMAPS;i++)     /* copy mouse pointers */
    for(y=0;y<MOUSE_HEIGHT;y++)
      for(x=0;x<MOUSE_WIDTH;x++)
      {
        mb[i]->data[x+y*MOUSE_WIDTH] = bmp.data[i*MOUSE_WIDTH+x+y*bmp.width];
        mb[i]->next = mb[i+1];
        mb[i]->hot_x = 12;
        mb[i]->hot_y = 12;
      }

  mb[0]->next  = mb[0];
  mb[8]->next  = mb[1];
  mb[0]->hot_x = 7;
  mb[0]->hot_y = 2;
                                      /* copy button bitmaps */
  for(i=0;i<NUM_BUTTONS;i++)
    for(j=0;j<BUTTON_BITMAPS;j++)
      for(y=0;y<BUTTON_HEIGHT;y++)
        for(x=0;x<BUTTON_WIDTH;x++)
        {
          button[i]->bitmap[j][x+y*BUTTON_WIDTH] =
            bmp.data[i*(bmp.width>>1) + j*BUTTON_WIDTH + x +
            (BUTTON_HEIGHT+y)*bmp.width];
        }

  free(bmp.data);                     /* free up memory used */

  set_palette(bmp.palette);

  for(y=0;y<SCREEN_HEIGHT;y++)        /* display a background */
    for(x=0;x<SCREEN_WIDTH;x++)
      VGA[(y<<8)+(y<<6)+x]=y;

  new_x=mouse.x;
  new_y=mouse.y;
  redraw=0xFFFF;
  show_mouse(&mouse);
  last_time=*my_clock;
  while (!done)                       /* start main loop */
  {
    if (redraw)                       /* draw the mouse only as needed */
    {
      wait_for_retrace();
      hide_mouse(&mouse);
      if (redraw>1)
      {
        for(i=0;i<NUM_BUTTONS;i++)
          if (redraw & (2<<i)) draw_button(button[i]);
      }
      if (mouse_new!=NULL) mouse.bmp=mouse_new;
      mouse.x=new_x;
      mouse.y=new_y;
      show_mouse(&mouse);
      last_time=*my_clock;
      redraw=0;
      mouse_new=NULL;
    }

    do {                              /* check mouse status */
      get_mouse_motion(&dx,&dy);
      press   = get_mouse_press(LEFT_BUTTON);
      release = get_mouse_release(LEFT_BUTTON);
    } while (dx==0 && dy==0 && press==0 && release==0 &&
      *my_clock==last_time);

    if (*my_clock!=last_time)         /* check animation clock */
    {
      if (mouse.bmp!=mouse.bmp->next)
      {
        redraw=1;
        mouse.bmp = mouse.bmp->next;
      }
      else
        last_time = *my_clock;
    }

    if (press) mouse.button1=1;
    if (release) mouse.button1=0;

    if (dx || dy)                     /* calculate movement */
    {
      new_x = mouse.x+dx;
      new_y = mouse.y+dy;
      if (new_x<0)   new_x=0;
      if (new_y<0)   new_y=0;
      if (new_x>319) new_x=319;
      if (new_y>199) new_y=199;
      redraw=1;
    }

    for(i=0;i<NUM_BUTTONS;i++)        /* check button status */
    {
      if (new_x >= button[i]->x && new_x < button[i]->x+48 &&
          new_y >= button[i]->y && new_y < button[i]->y+24)
      {
        if (release && button[i]->state==STATE_PRESSED)
        {
          button[i]->state=STATE_ACTIVE;
          redraw|=(2<<i);
          if (i==0)
          {
            if (mouse.bmp==mouse_norm)
              mouse_new=mouse_wait;
            else
              mouse_new=mouse_norm;
          }
          else if (i==1) done=1;
        }
        else if (press)
        {
          button[i]->state=STATE_PRESSED;
          redraw|=(2<<i);
        }
        else if (button[i]->state==STATE_NORM && mouse.button1==0)
        {
          button[i]->state=STATE_ACTIVE;
          redraw|=(2<<i);
        }
        else if (button[i]->state==STATE_WAITING)
        {
          if (mouse.button1)
          {
            button[i]->state=STATE_PRESSED;
          }
          else
          {
            button[i]->state=STATE_ACTIVE;
          }
          redraw|=(2<<i);
        }
      }
      else if (button[i]->state==STATE_ACTIVE)
      {
        button[i]->state=STATE_NORM;
        redraw|=(2<<i);
      }
      else if (button[i]->state==STATE_PRESSED && mouse.button1)
      {
        button[i]->state=STATE_WAITING;
        redraw|=(2<<i);
      }
      else if (button[i]->state==STATE_WAITING && release)
      {
        button[i]->state=STATE_NORM;
        redraw|=(2<<i);
      }

    }
  }                                   /* end while loop */

  for (i=0; i<NUM_MOUSEBITMAPS; i++)  /* free allocated memory */
  {
    free(mb[i]);
  }

  for (i=0; i<NUM_BUTTONS; i++)       /* free allocated memory */
  {
    free(button[i]);
  }

  set_mode(TEXT_MODE);                /* set the video mode back to
                                         text mode. */

  return;
}
