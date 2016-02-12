/* Mode:C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#include "muxbmf.h"

int karo = 4;
#ifndef Karo
#define Karo karo
#endif
#define KaroH (Karo / 2)

MUX_TIMEOUT_TYP xTo;
MUX_TIMEOUT_TYP xTick;
MUX_FD_TYP xX;

MUX_BMFCLT_TYP xIo;

Display *mydisplay;
Window basewindow;
Cursor mycurs;
GC mygc, mythingc, myclrgc;
GC myredgc, myyellowgc, mygreengc, mybluegc, mygraygc, mywhitegc;
XFontStruct *myfontstruct;
long font_height;
long font_depth;
unsigned long myforeground, mybackground;

char *grayname = "gray";
char *yellowname = "orange";
char *greenname = "green";

int todo = 1;

int ks = 0;
int ak = 0;

int xsize = 300, ysize = 100;

int Debug=0;

int done=0;

int r_flg = 0, t_flg = 0, a_flg = 0;

char *fnam=0;

void everysec (void);

char *pszHost;

struct sig {
	char *name;
	int num;
	int nmq;
	int typ;
#define T_HS 1
#define T_VR 2
#define T_WH 4
#define T_ZS 8
#define T_ZV 16
#define T_NM 32
#define T_HP2 64
#define T_VR2 128
#define T_KL 256
	int hp; 	/* -1 == Kl, others == speed */
	int vr;		/* Also speed */
	int wh;
	int zs3;
	int zs3v;
	int ks;
	int ak;
	MUX_TIMEOUT_TYP tim;
	int bst;
} sigarr [100];

void DrawPicture (void);
void DrawSig (struct sig *d);

int nsig = 0;

void handle_window(myevent) XEvent myevent; {
    int x,y; {
#if 0
	KeySym mykey;
	char text[10];
	int i;
#endif
	switch(myevent.type) {
	  case KeyPress:
#if 0
	    i=XLookupString(&myevent,text,10,&mykey,0);
	    if(i==1) {
	      printf ("txt=%d\n", text [0]);
	    } else {
	      printf ("i=%d\n", i);
	    }
#endif
	    break;
	  case Expose:
	    if(myevent.xexpose.count) break;
	    /* XClearArea(mydisplay,basewindow,R.x,R.y,R.w,R.h,False); */
	    DrawPicture();
	    break;
	  case GraphicsExpose:
	    break;
	  case NoExpose:
	    break;
	  case ConfigureNotify:
	    if(xsize!=myevent.xconfigure.width || ysize!=myevent.xconfigure.height) {
		xsize=myevent.xconfigure.width;
		ysize=myevent.xconfigure.height;
		/* todo=1; */ }
	    break;
	  case ButtonPress:
	    switch(myevent.xbutton.button) {
	      case 1:
		x = myevent.xbutton.x;
		y = myevent.xbutton.y;
		break;
	      case 2:
		x = myevent.xbutton.x;
		y = myevent.xbutton.y;
		break;
	      case 3:
		done=1;
		break; }
	    break;
	  case MotionNotify:
	    break;
	  case ButtonRelease:
	    break; }}}

int X_req (MUX_FD_TYP *x) {
	return MUX_POLLIN;
}

void X_hdl (MUX_FD_TYP *x, int rev) {
	XEvent myevent;
	if (rev & MUX_POLLIN) {
		while (XEventsQueued (mydisplay, QueuedAfterReading) > 0) {
			XNextEvent(mydisplay,&myevent);
			if(myevent.type==MappingNotify) {
				XRefreshKeyboardMapping(&myevent); }
			else if(myevent.xany.window==basewindow) {
				handle_window(myevent); }}
		if (todo) {
			DrawPicture ();
		}
		XFlush (mydisplay);
	}
}

void X_fail (MUX_FD_TYP *x, int rev) {
	printf ("X connection failed...\n");
	exit (1);
}

void tick_fire (MUX_TIMEOUT_TYP *pTo, struct timeval dt) {
    	MUX_SendToBmfClt (&xIo, bmfMakeMessage (
			BMF_NAME, "#",
			BMF_NAME, "tick",
			BMF_END));
	
	dt = MUX_GetTime ();
	TV_AddInt (&dt, 5 * 60, 0);
	MUX_SetTimeout (pTo, &dt);
}

void to_fire (MUX_TIMEOUT_TYP *pTo, struct timeval dt) {
	everysec ();
	if (todo) {
		XClearArea(mydisplay,basewindow,0,0,0,0,False);
		DrawPicture ();
	}
	XFlush (mydisplay);
	if (done) exit (0);

	dt = MUX_GetTime ();
	TV_AddFrac (&dt, 1, 4);
	MUX_SetTimeout (pTo, &dt);
}

	/*
	gettimeofday (&currtime, 0);
	currtime.tv_usec = 102000 - ((currtime.tv_usec + 1000000) % 100000);
	currtime.tv_sec = 0;
    XFreeGC(mydisplay,mygc);
    XDestroyWindow(mydisplay,basewindow);
    XCloseDisplay(mydisplay);
    exit(0); }
	*/

void handle_in (bmfItem_t *l) {
	char *n;
	int i;
	if (!bmfIsName (l, "sig")) return;
	l = bmfGetNext (l);
	n = bmfGetString (l);
	if (!n) return;
	for (i = 0; i < nsig; i ++) {
		struct sig *d = sigarr + i;
		if (strcmp (d->name, n)) continue;
		d->typ |= T_NM;
		d->hp = 0;
		d->vr = -1;
		d->wh = 0;
		d->zs3 = 0;
		d->zs3v = 0;
		for (l = bmfGetNext (l); !bmfIsEnd (l); l = bmfGetNext (l)) {
			n = bmfGetAsgnName (l);
			if (n) {
				int z = bmfGetAsgnValInt (l);
				if (!strcmp (n, "Zs3")) {
					d->zs3 = z;
					d->typ |= T_ZS;
				} else if (!strcmp (n, "Zs3v")) {
					d->zs3v = z;
					d->typ |= T_ZV;
				}
				continue;
			}
			n = bmfGetName (l);
			if (!n) continue;
			if (!strcmp (n, "Hp0")) {
				d->hp = 0;
			} else if (!strcmp (n, "Hp00")) {
				d->hp = 0;
				d->typ |= T_HS;
			} else if (!strcmp (n, "Hp1")) {
				d->hp = 160;
			} else if (!strcmp (n, "Hp2")) {
				d->hp = 60;
				d->typ |= T_HP2;
			} else if (!strcmp (n, "Kl")) {
				d->hp = -1;
			} else if (!strcmp (n, "Vr0")) {
				d->vr = 0;
				d->typ |= T_VR;
			} else if (!strcmp (n, "Vr1")) {
				d->vr = 160;
				d->typ |= T_VR;
			} else if (!strcmp (n, "Vr2")) {
				d->vr = 60;
				d->typ |= (T_VR | T_VR2);
			} else if (!strcmp (n, "VrWh")) {
				d->wh = 1;
				d->typ |= T_WH;
			}
			if (d->hp == -1 && d->vr == -1) {
				d->typ |= T_KL;
			}
		}
		DrawSig (d);
		if (d->ks) {
			if (d->zs3v != 0) {
				if (d->bst == 0) {
					struct timeval dt = MUX_GetTime ();
					d->bst = 1;
					MUX_SetTimeout (&d->tim, &dt);
				}
			} else {
				if (d->bst != 0) {
					MUX_RemoveTimeout (&d->tim);
					d->bst = 0;
				}
			}
		}
		XFlush (mydisplay);
		break;
	}
}

void sig_fire (MUX_TIMEOUT_TYP *pTo, struct timeval dt);

void io_recv (MUX_BMFCLT_TYP *pIo, bmfItem_t *l) {
	handle_in (l);
	bmfDrop (l);
}

void io_reset (MUX_BMFCLT_TYP *pIo) {
	int i;
	for (i = 0; i < nsig; i ++) {
		sigarr [i].typ = 0;
		sigarr [i].hp = 0;
		sigarr [i].vr = 0;
		sigarr [i].wh = 0;
		sigarr [i].zs3 = 0;
		sigarr [i].zs3v = 0;
		sigarr [i].bst = 0;
	}
	XClearArea (mydisplay, basewindow, 0, 0, 0, 0, True);
	XFlush (mydisplay);
}

bmfItem_t *req_item = 0;

void io_open (MUX_BMFCLT_TYP *pIo) {
	MUX_SendToBmfClt (&xIo, bmfMakeMessage (
			BMF_NAME, "#set-id",
			BMF_NAME, "esimc",
			BMF_ASN, "host",
			BMF_STR, MUX_GetHostName (),
			BMF_ASN, "pid",
			BMF_INT, MUX_GetPid (),
			BMF_TAIL, req_item,
			BMF_END));
	MUX_SendToBmfClt (&xIo, bmfRef (req_item));
}

int qq = 0;
bmfBuild_t B;

void addsig (char *n) {
    if (nsig < sizeof (sigarr) / sizeof (sigarr [0])) {
	MUX_InitTimeout (&sigarr [nsig].tim, sig_fire, &sigarr [nsig]);
	sigarr [nsig].bst = 0;
	sigarr [nsig].num = nsig;
	sigarr [nsig].nmq = qq;
	sigarr [nsig].ks = ks;
	sigarr [nsig].ak = ak;
	sigarr [nsig ++].name = n;
	bmfBuildAddString (&B, n);
	qq += 4;
    }
}

void addsigs (char *n) {
	addsig (n);
}

int main(argc,argv) int argc; char **argv; {
	char *shost = "localhost";
    XSizeHints myhint;
    int myscreen;
    int i;
    struct timeval currtime;
    char *title = "esimc";
    char *display_name;
    char *fontname=0;
    char *deffont="-adobe-helvetica-medium-r-normal--14-140-75-75-p-77-iso8859-1";
    int pt = 8440;
    int dummy;
    XCharStruct Dummy;

    Colormap cmap;
    XColor exact, color;

    display_name=getenv("DISPLAY");

    bmfBuildInit (&B);
    bmfBuildAddName (&B, "sig");

    for(i=1;i<argc;i++) {
	if(argv[i][0]=='-') {
	    if(!strcmp(argv[i],"-display")) {
		if(++i==argc) printf("No display name\n");
		else display_name=argv[i]; }
	    else if(!strcmp(argv[i],"-h")) {
		if(++i==argc) printf("No host name\n");
                else shost=argv[i]; }
	    else if(!strcmp(argv[i],"-p")) {
		if(++i==argc) printf("No port number\n");
                else pt=atoi (argv[i]); }
	    else if(!strcmp(argv[i],"-fn")) {
		if(++i==argc) printf("No font name\n");
                else fontname=argv[i]; }
	    else if(!strcmp(argv[i],"-bg")) {
		if(++i==argc) printf("No background color name\n");
                else grayname=argv[i]; }
	    else if(!strcmp(argv[i],"-yl")) {
		if(++i==argc) printf("No yellow color name\n");
                else yellowname=argv[i]; }
	    else if(!strcmp(argv[i],"-gn")) {
		if(++i==argc) printf("No green color name\n");
                else greenname=argv[i]; }
	    else if(!strcmp(argv[i],"-title")) {
		if(++i==argc) printf("No title\n");
		else title = argv [i]; }
	    else if(!strcmp(argv[i],"-geometry")) {
		if(++i==argc) printf("No geometry\n"); }
	    else if(!strcmp(argv[i],"-hv")) {
		ak = 0;
		ks = 0; }
	    else if(!strcmp(argv[i],"-ks")) {
		ak = 0;
                ks = 1; }
	    else if(!strcmp(argv[i],"-ak")) {
		ak ++;
                ks = 0; }
	    else if(!strcmp(argv[i],"-big")) {
                karo = 8; }
	    else if(!strcmp(argv[i],"-xl")) {
                karo = 12; }
	    else if(!strcmp(argv[i],"-xxl")) {
                karo = 16; }
	    else if(!strcmp(argv[i],"-midi")) {
                karo = 3; }
	    else if(!strcmp(argv[i],"-small")) {
                karo = 2; }
	    else if(!strcmp(argv[i],"-b")) {
                grayname = "blue4"; }
	    else if(!strcmp(argv[i],"-d")) {
                Debug++; }
	    else if(!strcmp(argv[i],"-r")) {
                r_flg++; }
	    else if(!strcmp(argv[i],"-t")) {
                t_flg++; }
	    else if(!strcmp(argv[i],"-a")) {
                a_flg++; }
	    else if(!strcmp(argv[i],"--")) {
                qq++; }
	    else if(!strcmp(argv[i],"---")) {
                qq+=2; }
	    else printf("Unknown option %s\n",argv[i]); }
	else {
	    addsigs (argv [i]);
	}}
    if(!display_name) display_name="";

    pszHost = fnam;
    if (!pszHost) pszHost = "gp.in";

    mydisplay=XOpenDisplay(display_name);
    if(!mydisplay) {
	printf("Cannot open XWindow display %s\n",display_name);
	return 0; }
    if(!fontname) fontname="6x10";
    myfontstruct=XLoadQueryFont(mydisplay,fontname);
    if(myfontstruct==0) {
	printf("Could not load font %s, trying my default\n",fontname);
	myfontstruct=XLoadQueryFont(mydisplay,fontname=deffont);
	if(myfontstruct==0) {
	    printf("Failed too\n");
	    exit(1); }}
    if(myfontstruct->fid==0) {
	printf("No font id\n");
	exit(1); }
    myscreen=DefaultScreen(mydisplay);
    if(r_flg) {
	myforeground=WhitePixel(mydisplay,myscreen);
	mybackground=BlackPixel(mydisplay,myscreen); }
    else {
	mybackground=WhitePixel(mydisplay,myscreen);
	myforeground=BlackPixel(mydisplay,myscreen); }
    myhint.x=10;
    myhint.y=10;
    myhint.width=xsize=((14*Karo)*qq)/4;
    myhint.height=Karo*(ks?160:238)/4;
    myhint.flags=PSize;
    cmap = DefaultColormap (mydisplay, myscreen);
    if (XAllocNamedColor (mydisplay, cmap, grayname, &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color gray failed\n");
    }
    basewindow=XCreateSimpleWindow(mydisplay,
       DefaultRootWindow(mydisplay),
       myhint.x,myhint.y,myhint.width,myhint.height,
       1, myforeground,color.pixel);
    XSetStandardProperties(mydisplay,basewindow,title,"ESimC",
       None,argv,argc,&myhint);
    mygc=XCreateGC(mydisplay,basewindow,0,0);
    XSetBackground (mydisplay, mygc, mybackground);
    XSetForeground (mydisplay, mygc, myforeground);
    mythingc=XCreateGC(mydisplay,basewindow,0,0);
    XSetBackground (mydisplay, mythingc, mybackground);
    XSetForeground (mydisplay, mythingc, myforeground);

    myclrgc=XCreateGC(mydisplay,basewindow,0,0);
    XSetBackground (mydisplay, myclrgc, myforeground);
    XSetForeground (mydisplay, myclrgc, mybackground);

/*
@define @ # ## (    #=XCreateGC(mydisplay,basewindow,0,0);
    if (XAllocNamedColor (mydisplay, cmap, "##", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color ## failed\n");
    }
    XSetForeground (mydisplay, #, color.pixel);
    XSetBackground (mydisplay, #, mybackground);
)
@ myredgc red
@ myyellowgc yellow
@ mygreengc green
@ mybluegc blue
*/

    myredgc=XCreateGC(mydisplay,basewindow,0,0);
    if (XAllocNamedColor (mydisplay, cmap, "red", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color red failed\n");
    }
    XSetForeground (mydisplay, myredgc, color.pixel);
    XSetBackground (mydisplay, myredgc, mybackground);

    myyellowgc=XCreateGC(mydisplay,basewindow,0,0);
    if (XAllocNamedColor (mydisplay, cmap, yellowname, &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color yellow failed\n");
    }
    XSetForeground (mydisplay, myyellowgc, color.pixel);
    XSetBackground (mydisplay, myyellowgc, mybackground);

    mygreengc=XCreateGC(mydisplay,basewindow,0,0);
    if (XAllocNamedColor (mydisplay, cmap, greenname, &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color green failed\n");
    }
    XSetForeground (mydisplay, mygreengc, color.pixel);
    XSetBackground (mydisplay, mygreengc, mybackground);

    mybluegc=XCreateGC(mydisplay,basewindow,0,0);
    if (XAllocNamedColor (mydisplay, cmap, "blue", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color blue failed\n");
    }
    XSetForeground (mydisplay, mybluegc, color.pixel);
    XSetBackground (mydisplay, mybluegc, mybackground);

    mywhitegc=XCreateGC(mydisplay,basewindow,0,0);
    if (XAllocNamedColor (mydisplay, cmap, "white", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color white failed\n");
    }
    XSetForeground (mydisplay, mywhitegc, color.pixel);
    XSetBackground (mydisplay, mywhitegc, mybackground);

    mygraygc=XCreateGC(mydisplay,basewindow,0,0);
    if (XAllocNamedColor (mydisplay, cmap, "gray", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color gray failed\n");
    }
    XSetForeground (mydisplay, mygraygc, color.pixel);
    XSetBackground (mydisplay, mygraygc, mybackground);

    XSetLineAttributes (mydisplay, mygc, 5 * KaroH, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, myredgc, 5, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, mygreengc, 5, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, myyellowgc, 5, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, mywhitegc, 3, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, myclrgc, 5, LineSolid, CapRound, JoinRound);

    XSetFont(mydisplay,mygc,myfontstruct->fid);
    XTextExtents(myfontstruct,"",0,&dummy,&font_height,&font_depth,&Dummy);
    mycurs=XCreateFontCursor(mydisplay,XC_crosshair);
    XDefineCursor(mydisplay,basewindow,mycurs);
    XSelectInput(mydisplay,basewindow,
       ButtonPressMask|ButtonReleaseMask
       |Button1MotionMask
       |KeyPressMask|ExposureMask|StructureNotifyMask);
    XMapRaised(mydisplay,basewindow);

    currtime = MUX_GetTime ();
    MUX_InitTimeout (&xTo, to_fire, 0);
    MUX_SetTimeout (&xTo, &currtime);
    MUX_InitTimeout (&xTick, tick_fire, 0);
    MUX_SetTimeout (&xTick, &currtime);

    MUX_InitMuxFd (&xX, ConnectionNumber (mydisplay),
		X_req, X_hdl, X_fail, 0);
    MUX_AddMuxFd (&xX);

    MUX_InitBmfClt (&xIo, shost, pt, 250, io_recv, io_reset, io_open, 0);
    req_item = bmfBuildFini (&B);

    MUX_DoServe ();
    exit (1);
}

void draw_dot (int x, int y, int r, GC gc) {
	XFillArc (mydisplay, basewindow, gc,
		x, y, r, r, 0, 23040);
}

unsigned char zspat [16] [7] = {
	{  0 },
	{  4, 12, 20,  4,  4,  4, 14 },
	{ 30,  1,  1,  2,  4,  8, 31 },
	{ 30,  1,  1, 14,  1,  1, 30 },
/*4*/	{ 16, 18, 18, 31,  2,  2,  2 },
	{ 31, 16, 28,  2,  2,  2, 28 },
	{ 14, 16, 16, 30, 17, 17, 14 },
	{ 31,  1,  2,  4,  8,  8,  8 },
	{ 14, 17, 17, 14, 17, 17, 14 },
	{ 14, 17, 17, 15,  1,  1, 14 },
/*10*/	{ 18, 21, 21, 21, 21, 21, 18 },
	{ 27,  9,  9,  9,  9,  9,  9 },
	{ 22, 17, 17, 18, 20, 20, 23 },
	{ 22, 17, 17, 22, 17, 17, 22 },
/*14*/	{ 20, 21, 21, 23, 17, 17, 17 },
	{ 23, 20, 20, 22, 17, 17, 22 }
};

void DrawNum (int x, int y, int num, GC gc) {
	unsigned char *p;
	int i, j;
	XFillRectangle (mydisplay, basewindow, mygc,
		x - Karo, y - Karo, 7 * Karo, 9 * Karo);
	if (num < 1 || num > 15) return;
	p = zspat [num];
	for (j = 0; j < 7; j ++) {
		for (i = 0; i < 5; i ++) {
			if (p [j] & (16 >> i)) {
				XFillRectangle (mydisplay, basewindow, gc,
					x + i * Karo + KaroH / 2, y + j * Karo + KaroH / 2,
					Karo / 2, Karo / 2);
			}
		}
	}
}

void sig_fire (MUX_TIMEOUT_TYP *pTo, struct timeval dt) {
	struct sig *d = pTo->pvUserData;
	int x = (d->nmq * 14 * Karo) / 4 + 3 * Karo;
	int y = 11 * Karo;
	if (d->bst == 0) return;
	if (d->bst == 1) {
		/* Draw green */
		draw_dot (x + 1 * Karo, y + 11 * KaroH, 2 * Karo, mygreengc);
		d->bst = 2;
	} else {
		draw_dot (x + 1 * Karo, y + 11 * KaroH, 2 * Karo, mygc);
		d->bst = 1;
	}
	TV_AddFrac (&dt, 1, 2);
	MUX_SetTimeout (pTo, &dt);
	XFlush (mydisplay);
}

void DrawSig (struct sig *d) {
	XPoint pts [8];
	int x = (d->nmq * 14 * Karo) / 4 + 3 * Karo;
	int y = 11 * Karo;
	int yv = y + 24 * Karo;
	int wd = (XTextWidth (myfontstruct, d->name, strlen (d->name)) + 1) / 2;

	int f;

	if (d->ks) {
		if (d->hp > 0 && d->hp < 160 && d->zs3 == 0) {
			d->zs3 = 4;
			d->typ |= T_ZS;
		}
		if (d->vr > 0 && d->vr < 160 && d->zs3v == 0) {
			d->zs3v = 4;
			d->typ |= T_ZV;
		}
		if (d->typ & T_ZS) {
			DrawNum (x + 3 * KaroH, 2 * Karo, d->zs3, myclrgc);
		}
		if (d->typ & T_ZV) {
			DrawNum (x + 3 * KaroH, KaroH + 30 * Karo, d->zs3v, myyellowgc);
		}
		if (d->typ & T_NM && karo > 3) {
			XDrawRectangle (mydisplay, basewindow, mythingc,
					x + 4 * Karo - wd - 2, KaroH + 25 * Karo - 2, 2 * wd + 3, font_height + font_depth + 3);
			XFillRectangle (mydisplay, basewindow, mywhitegc,
					x + 4 * Karo - wd - 1, KaroH + 25 * Karo - 1, 2 * wd + 2, font_height + font_depth + 2);
			XDrawString (mydisplay, basewindow, mygc, x + 4 * Karo - wd, KaroH + 25 * Karo + font_height,
				     d->name, strlen (d->name));
		}
		pts [0].x = x;
		pts [0].y = 12 * Karo;
		pts [1].x = Karo;
		pts [1].y = -Karo;
		pts [2].x = 6 * Karo;
		pts [2].y = 0;
		pts [3].x = Karo;
		pts [3].y = Karo;
		pts [4].x = 0;
		pts [4].y = 13 * Karo;
		pts [5].x = -8 * Karo;
		pts [5].y = 0;
		XFillPolygon (mydisplay, basewindow, mygc, pts, 6, Convex, CoordModePrevious);

		if (d->hp == 0) {
			draw_dot (x + 3 * Karo, y + 6 * KaroH, 2 * Karo, myredgc);
			return;
		}
		if (d->hp == -1 && d->vr == -1) {
			draw_dot (x + 3 * KaroH, y + 3 * KaroH, Karo, myclrgc);
			return;
		}
		if (d->vr == 0) {
			draw_dot (x + 5 * Karo, y + 11 * KaroH, 2 * Karo, myyellowgc);
		} else {
			if (d->typ & T_VR) {
				if (d->bst != 1) draw_dot (x + 1 * Karo, y + 11 * KaroH, 2 * Karo, mygreengc);
			} else {
				draw_dot (x + 3 * Karo, y + 11 * KaroH, 2 * Karo, mygreengc);
			}
		}
		if (d->wh && (d->vr == 0 || d->zs3v)) {
			if (d->hp != -1) {
				draw_dot (x + 3 * KaroH, y + 3 * KaroH, Karo, myclrgc);
			} else {
				draw_dot (x + 3 * KaroH, y + 21 * KaroH, Karo, myclrgc);
			}
		}
		return;
	}
	if (d->ak == 2) {
		if (d->typ & T_ZS) {
			DrawNum (x + 3 * KaroH, 2 * Karo, d->zs3, myclrgc);
		}
		if (d->typ & T_ZV) {
			DrawNum (x + 3 * KaroH, KaroH + 32 * Karo, d->zs3v, myyellowgc);
		}
		if (d->typ & T_NM && karo > 3) {
			XDrawRectangle (mydisplay, basewindow, mythingc,
					x + 4 * Karo - wd - 2, KaroH + 27 * Karo - 2, 2 * wd + 3, font_height + font_depth + 3);
			XFillRectangle (mydisplay, basewindow, mywhitegc,
					x + 4 * Karo - wd - 1, KaroH + 27 * Karo - 1, 2 * wd + 2, font_height + font_depth + 2);
			XDrawString (mydisplay, basewindow, mygc, x + 4 * Karo - wd, KaroH + 27 * Karo + font_height,
				     d->name, strlen (d->name));
		}
		XFillArc (mydisplay, basewindow, mygc,
			  x - 2 * Karo, 13 * Karo, 12 * Karo, 12 * Karo,
			  0, 360 * 64);

		if (d->hp == 0) {
			draw_dot (x + 3 * Karo, y + 13 * KaroH, 2 * Karo, myredgc);
			return;
		}
		if (d->hp == -1 && d->vr == -1) {
			draw_dot (x + 2 * KaroH, y + 7 * KaroH, Karo, myclrgc);
			return;
		}
		if (/* d->vr == 0 || (d->hp >= 0 && d->hp <= 60)*/
		    d->hp > 0 && d->hp <= 60) {
			draw_dot (x + 5 * Karo, y + 20 * KaroH, 2 * Karo, myyellowgc);
		}
		if (d->vr != 0) {
			draw_dot (x + 5 * Karo, y + 7 * KaroH, 2 * Karo, mygreengc);
		}
		if (/*(d->vr > 0 && d->vr <= 60) ||
		      (d->hp > 0 && d->hp <= 60 && d->vr == 0)*/
		    d->vr >= 0 && d->vr <= 60) {
			draw_dot (x - 1 * KaroH, y + 13 * KaroH, 2 * Karo, myyellowgc);
		}
		if (d->wh && (d->vr >= 0 && d->vr < 160 || d->zs3v)) {
			draw_dot (x + 2 * KaroH, y + 7 * KaroH, Karo, myclrgc);
		}
		return;
	}
	if (d->ak) {
		int wh = 0;
		int ht = 7;
		int kl = 3;
		int vr = 13;
		int hp = 8;
		if (d->ak == 1) {
			wh = 2;
			ht= 15;
		} else {
			if ((d->wh && (d->vr > 0 && d->vr < 160 || d->zs3v)) ||
			    (d->vr == 0 && (d->hp > 0 && d->hp <= 60))
			    ) {
				d->typ |= T_VR2; /* Hack into wide form... */
			}
			if (d->typ & T_VR) {
				ht = 9;
			}
			if (d->typ & T_VR2) {
				wh = 2;
				ht = 11;
			} else {
				if ((d->typ & T_KL) || (d->typ & T_WH)) {
					hp += 3;
					kl = 8;
					ht += 2;
					vr += 3;
				}
				if (d->typ & T_HP2) {
					vr = 23;
				}
			}
			if (d->typ & T_HP2) {
				ht = 15;
			}
		}
		if (d->typ & T_ZS) {
			DrawNum (x + 3 * KaroH, 2 * Karo, d->zs3, myclrgc);
		}
		if (d->typ & T_ZV) {
			DrawNum (x + 3 * KaroH, KaroH + 32 * Karo, d->zs3v, myyellowgc);
		}
		if (d->typ & T_NM && karo > 3) {
			XDrawRectangle (mydisplay, basewindow, mythingc,
					x + 4 * Karo - wd - 2, KaroH + 27 * Karo - 2, 2 * wd + 3, font_height + font_depth + 3);
			XFillRectangle (mydisplay, basewindow, mywhitegc,
					x + 4 * Karo - wd - 1, KaroH + 27 * Karo - 1, 2 * wd + 2, font_height + font_depth + 2);
			XDrawString (mydisplay, basewindow, mygc, x + 4 * Karo - wd, KaroH + 27 * Karo + font_height,
				     d->name, strlen (d->name));
		}

		pts [0].x = x + (2 - wh) * Karo;
		pts [0].y = 12 * Karo;
		pts [1].x = Karo;
		pts [1].y = -Karo;
		pts [2].x = (2 * wh + 2) * Karo;
		pts [2].y = 0;
		pts [3].x = Karo;
		pts [3].y = Karo;
		pts [4].x = 0;
		pts [4].y = ht * Karo;
		pts [5].x = -(2 * wh + 4) * Karo;
		pts [5].y = 0;
		XFillPolygon (mydisplay, basewindow, mygc, pts, 6, Convex, CoordModePrevious);

		if (d->hp == 0) {
			draw_dot (x + (3 - wh) * Karo, y + hp * KaroH, 2 * Karo, myredgc);
			return;
		}
		if (d->hp == -1 && d->vr == -1) {
			/* Kl */
			draw_dot (x + (3 - wh) * Karo + KaroH, y + kl * KaroH, Karo, myclrgc);
			return;
		}
		if (/* d->vr == 0 || (d->hp >= 0 && d->hp <= 60)*/
		    d->hp > 0 && d->hp <= 60) {
			draw_dot (x + (3 + wh) * Karo, y + 23 * KaroH, 2 * Karo, myyellowgc);
		}
		if (d->vr != 0) {
			draw_dot (x + (3 + wh) * Karo, y + 3 * KaroH, 2 * Karo, mygreengc);
		}
		if (/*(d->vr > 0 && d->vr <= 60) ||
		      (d->hp > 0 && d->hp <= 60 && d->vr == 0)*/
		    d->vr >= 0 && d->vr <= 60) {
			draw_dot (x + (3 - wh) * Karo, y + vr * KaroH, 2 * Karo, myyellowgc);
		}
		if (d->wh && (d->vr >= 0 && d->vr < 160 || d->zs3v)) {
			/* Wh */
			draw_dot (x + (3 - wh) * Karo + KaroH, y + kl * KaroH, Karo, myclrgc);
#if 0
			draw_dot (x + 3 * KaroH, y + 3 * KaroH, Karo, myclrgc);
#endif
		}
		return;
	}

	f = 0;
	if (d->typ & (T_ZS | T_HP2 | T_HS)) {
		f = 0;
	} else if (d->typ & T_VR) {
		f = a_flg;
	}

	if (d->typ & T_ZS) DrawNum (x + 3 * KaroH, 2 * Karo, d->zs3, myclrgc);
	if (d->typ & T_ZV) DrawNum (x + 3 * KaroH, 50 * Karo, d->zs3v, myyellowgc);

	pts [0].x = x;
	pts [0].y = y + Karo;
	pts [1].x = Karo;
	pts [1].y = -Karo;
	pts [2].x = 6 * Karo;
	pts [2].y = 0;
	pts [3].x = Karo;
	pts [3].y = Karo;
	pts [4].x = 0;
	pts [4].y = 17 * Karo;
	pts [5].x = -8 * Karo;
	pts [5].y = 0;
	XFillPolygon (mydisplay, basewindow, f ? mygraygc : mygc, pts, 6, Convex, CoordModePrevious);

	if (f) {
		/* nix */
	} else if (d->typ & T_HS) {
		if (d->hp < 0) {
			/* Kl */
			draw_dot (x + 3 * KaroH, y + 17 * KaroH, Karo, myclrgc);
		} else if (d->hp < 1) {
			draw_dot (x + Karo, y + 9 * KaroH, 2 * Karo, myredgc);
			draw_dot (x + 5 * Karo, y + 9 * KaroH, 2 * Karo, myredgc);
			/* Hp00 */
		} else if (d->hp <= 60) {
			/* Hp2 */
			draw_dot (x + 1 * Karo, y + 2 * KaroH, 2 * Karo, mygreengc);
			draw_dot (x + 1 * Karo, y + 30 * KaroH, 2 * Karo, myyellowgc);
		} else {
			/* Hp1 */
			draw_dot (x + 1 * Karo, y + 2 * KaroH, 2 * Karo, mygreengc);
		}
	} else {
		if (d->hp < 0) {
			/* Kl */
			draw_dot (x + 7 * KaroH, y + 12 * KaroH, Karo, myclrgc);
		} else if (d->hp < 1) {
			/* Hp0 */
			draw_dot (x + 2 * Karo, y + 29 * KaroH, 2 * Karo, myredgc);
		} else if (d->hp <= 60) {
			/* Hp2 */
			draw_dot (x + 4 * Karo, y + 3 * KaroH, 2 * Karo, mygreengc);
			draw_dot (x + 4 * Karo, y + 29 * KaroH, 2 * Karo, myyellowgc);
		} else {
			/* Hp1 */
			draw_dot (x + 4 * Karo, y + 3 * KaroH, 2 * Karo, mygreengc);
		}
	}

	if (!(d->typ & T_VR)) return;

	pts [0].x = x + 5 * Karo;
	pts [0].y = yv + Karo;
	pts [1].x = 6 * KaroH;
	pts [1].y = 0;
	pts [2].x = 4 * KaroH;
	pts [2].y = 4 * KaroH;
	pts [3].x = 0;
	pts [3].y = 6 * KaroH;
	pts [4].x = -7 * Karo;
	pts [4].y = 7 * Karo;
	pts [5].x = -6 * KaroH;
	pts [5].y = 0;
	pts [6].x = -4 * KaroH;
	pts [6].y = -4 * KaroH;
	pts [7].x = 0;
	pts [7].y = -6 * KaroH;
	XFillPolygon (mydisplay, basewindow, mygc, pts, 8, Convex, CoordModePrevious);

	if (d->hp == 0) {
		if (f == 2) {
			draw_dot (x + Karo, yv + 18 * KaroH, 2 * Karo, myredgc);
		} else if (f) {
			draw_dot (x + 3 * Karo, yv + 12 * KaroH, 2 * Karo, myredgc);
		}
	}

	if (d->vr < 0) {
		if (f && d->hp > 0) draw_dot (x + 7 * Karo, yv + 6 * KaroH, 2 * Karo, mygreengc);
	} else if (d->vr < 1) {
		draw_dot (x + 5 * Karo, yv + 6 * KaroH, 2 * Karo, myyellowgc);
		draw_dot (x - Karo, yv + 18 * KaroH, 2 * Karo, myyellowgc);
	} else if (d->vr <= 60) {
		draw_dot (x + 7 * Karo, yv + 6 * KaroH, 2 * Karo, mygreengc);
		draw_dot (x - Karo, yv + 18 * KaroH, 2 * Karo, myyellowgc);
	} else {
		draw_dot (x + 7 * Karo, yv + 6 * KaroH, 2 * Karo, mygreengc);
		if (f != 2) {
			draw_dot (x + Karo, yv + 18 * KaroH, 2 * Karo, mygreengc);
		}
	}

	if (!(d->typ & T_WH)) return;

	XDrawLine (mydisplay, basewindow, mygc,
			x, yv + 8 * KaroH,
			x + 2 * Karo, yv + 12 * KaroH);

	if (d->wh) {
		draw_dot (x - KaroH, yv + 7 * KaroH, Karo, myclrgc);
	}

	/*
	XFillArc (mydisplay, basewindow, mygreengc,
		  50, 50, 30, 30, 0, 23040);
	XFillArc (mydisplay, basewindow, myredgc,
		  50, 95, 30, 30, 0, 23040);
	XFillArc (mydisplay, basewindow, myredgc,
		  110, 95, 30, 30, 0, 23040);
	XFillArc (mydisplay, basewindow, myclrgc,
		  110 + 7, 140 + 7, 16, 16, 0, 23040);
	XFillArc (mydisplay, basewindow, myclrgc,
		  50 + 7, 185 + 7, 16, 16, 0, 23040);
	XFillArc (mydisplay, basewindow, myyellowgc,
		  50, 230, 30, 30, 0, 23040);
		*/
}

void DrawPicture (void) {
	int i;
	for (i = 0; i < nsig; i ++) {
		DrawSig (sigarr + i);
	}
	if(todo) {
		todo=0;
	}
}

void everysec (void) {
}
