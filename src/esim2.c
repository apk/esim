/* Mode:C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stropts.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/statvfs.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#define FieldXSize 60
#define FieldYSize 40

int todo = 1;

int xsize = 18* FieldXSize - 1, ysize = 8 * FieldYSize - 1;

Display *mydisplay;
Window basewindow;
Cursor mycurs;
GC mygc, myclrgc;
GC myredgc, myyellowgc, mygreengc, mybluegc, mygraygc;
XFontStruct *myfontstruct;
long font_height;
long font_depth;

#define NTX 30
#define NTY 15

#define Dir_W 1
#define Dir_SW 2
#define Dir_S 4
#define Dir_SE 8
#define Dir_E 16
#define Dir_NE 32
#define Dir_N 64
#define Dir_NW 128
#define Dir_WE (Dir_W | Dir_E)

struct tfield {	/* Table field */
	int x, y;	/* Position, redundant */
	enum {
		TF_Null = 0,
		TF_Straight,
		TF_Switch,	/* Weiche */
		TF_HpSig,	/* Hauptsignal */
		TF_Occ,		/* Freimelder */
		TF_Other
	} type;
	unsigned char blacks;
	unsigned char illum;
	enum {
		Ill_Dark = 0,
		Ill_White,
		Ill_Red
	} ilco;

	unsigned char i_base, i_str, i_branch;	/* Partials for switches */
	int is_branch;		/* Switch is on branch */

	int occup;		/* We are occupied (Straight, Switch) */
	int locked;		/* We are locked (Straight, Switch) */

	int is_open;		/* Signal is open */

	struct occ *occp;	/* For OCs */
	struct fbox *fbp;	/* For FBoxes and signals */

} ttable [NTX] [NTY];

struct occ {
	struct tfield **tf_list;
	struct tfield *disp;
	int cnt;
	struct tfield *lock;	/* Locking signal */
};

struct swval {
	struct tfield *swtch;	/* switch to get in state */
	int is_branched;	/* state needed */
};

struct fbox {
	struct fbox *next;	/* FBoxes chained on signals */
	struct occ **oclist;	/* Fields to monitor/lock */
	struct swval *swlist;	/* Switches to lock and the needed state */
	int minocclen;		/* Number of field before red */
	int slow;		/* Hp2 indicator */
	int on;			/* FBox is active */
	int occlen;		/* Number of fields already occupied */
	int len;		/* Number of fields already gone */
};

/* new */

#if 0
struct tr_conn {
};

struct track {
};

struct train {
};
#endif

/* end new */

struct tfield *ocl1 [] = {
	&ttable [1] [3],
	0
};

struct tfield *ocl2 [] = {
	&ttable [3] [3],
	0
};

struct tfield *ocl3 [] = {
	&ttable [4] [3],
	0
};

struct tfield *ocl4 [] = {
	&ttable [5] [3],
	&ttable [6] [3],
	&ttable [7] [3],
	0
};

struct tfield *ocl5 [] = {
	&ttable [5] [2],
	&ttable [6] [2],
	&ttable [7] [2],
	0
};

struct tfield *ocl6 [] = {
	&ttable [9] [3],
	0
};

struct tfield *ocl7 [] = {
	&ttable [9] [2],
	0
};

struct tfield *ocl8 [] = {
	&ttable [10] [3],
	0
};

struct tfield *ocl9 [] = {
	&ttable [11] [3],
	0
};

struct tfield *ocl10 [] = {
	&ttable [13] [3],
	&ttable [14] [3],
	0
};

struct tfield *ocl11 [] = {
	&ttable [16] [3],
	0
};

struct occ occtable [11] = {
	{ ocl1, 0, 0 },
	{ ocl2, 0, 0 },
	{ ocl3, 0, 0 },
	{ ocl4, 0, 0 },
	{ ocl5, 0, 0 },
	{ ocl6, 0, 0 },
	{ ocl7, 0, 0 },
	{ ocl8, 0, 0 },
	{ ocl9, 0, 0 },
	{ ocl10, 0, 0 },
	{ ocl11, 0, 0 }
};

struct occ *oct1a [] = {
	&occtable [1],
	&occtable [2],
	&occtable [3],
	0
};

struct swval swt1a [] = {
	{ &ttable [4] [3], 0 },
	{ 0, 0 }
};

struct occ *oct1b [] = {
	&occtable [1],
	&occtable [2],
	&occtable [4],
	0
};

struct swval swt1b [] = {
	{ &ttable [4] [3], 1 },
	{ 0, 0 }
};

struct occ *oct2 [] = {
	&occtable [5],
	&occtable [7],
	&occtable [8],
	0
};

struct swval swt2 [] = {
	{ &ttable [10] [3], 0 },
	{ 0, 0 }
};

struct occ *oct3 [] = {
	&occtable [6],
	&occtable [7],
	&occtable [8],
	0
};

struct swval swt3 [] = {
	{ &ttable [9] [2], 1 },
	{ &ttable [10] [3], 1 },
	{ 0, 0 }
};

struct occ *oct4 [] = {
	&occtable [9],
	0
};

struct swval swt4 [] = {
	{ 0, 0 }
};

struct occ *oct5 [] = {
	&occtable [10],
	0
};

struct swval swt5 [] = {
	{ 0, 0 }
};

struct fbox fbox1a = { 0, oct1b, swt1b, 2, 1 };
struct fbox fbox1 = { &fbox1a, oct1a, swt1a, 2, 0 };
struct fbox fbox2 = { 0, oct2, swt2, 2, 0 };
struct fbox fbox3 = { 0, oct3, swt3, 2, 1 };
struct fbox fbox4 = { 0, oct4, swt4, 1, 0 };
struct fbox fbox5 = { 0, oct5, swt5, 1, 0 };

void initttable (void) {
	int x, y;
	int i;
	for (y = 0; y < NTY; y ++) {
		for (x = 0; x < NTX; x ++) {
			ttable [x] [y].type = TF_Null;
			ttable [x] [y].x = x;
			ttable [x] [y].y = y;
			ttable [x] [y].blacks = 0;
			ttable [x] [y].illum = 0;
			ttable [x] [y].ilco = Ill_Dark;
			ttable [x] [y].occup = 0;
			ttable [x] [y].fbp = 0;
			ttable [x] [y].locked = 0;
			ttable [x] [y].is_open = 0;
		}
	}

	for (i = 0; i < 11; i ++) {
		struct tfield **tpp;

		ttable [i + 3] [6].type = TF_Occ;
		ttable [i + 3] [6].blacks = 0;
		ttable [i + 3] [6].illum = 0;
		ttable [i + 3] [6].ilco = Ill_Dark;
		ttable [i + 3] [6].occp = &occtable [i];

		occtable [i].disp = &ttable [i + 3] [6];
		occtable [i].cnt = 0;

		for (tpp = occtable [i].tf_list; *tpp; tpp ++) {
			(*tpp)->occp = &occtable [i];
		}
	}


	ttable [4] [3].type = TF_Switch;
	ttable [4] [3].blacks = Dir_WE | Dir_NE;
	ttable [4] [3].illum = Dir_W | Dir_NE;
	ttable [4] [3].ilco = Ill_White;
	ttable [4] [3].i_base = Dir_W;
	ttable [4] [3].i_str = Dir_E;
	ttable [4] [3].i_branch = Dir_NE;

	ttable [10] [3].type = TF_Switch;
	ttable [10] [3].blacks = Dir_WE | Dir_NW;
	ttable [10] [3].illum = Dir_WE;
	ttable [10] [3].ilco = Ill_White;
	ttable [10] [3].i_base = Dir_E;
	ttable [10] [3].i_str = Dir_W;
	ttable [10] [3].i_branch = Dir_NW;

	ttable [9] [2].type = TF_Switch;
	ttable [9] [2].blacks = Dir_WE | Dir_SE;
	ttable [9] [2].illum = Dir_WE;
	ttable [9] [2].ilco = Ill_White;
	ttable [9] [2].i_base = Dir_W;
	ttable [9] [2].i_str = Dir_E;
	ttable [9] [2].i_branch = Dir_SE;


	ttable [5] [2].type = TF_Straight;
	ttable [5] [2].blacks = Dir_SW | Dir_E;
	ttable [5] [2].illum = Dir_SW | Dir_E;
	ttable [5] [2].ilco = Ill_White;

	for (i = 1; i < 17; i ++) {
		int j;

		if (i != 4 && i != 10) {
			ttable [i] [3].type = TF_Straight;
			ttable [i] [3].blacks = Dir_WE;
			ttable [i] [3].illum = Dir_WE;
			ttable [i] [3].ilco = Ill_White;
		}
	}

	for (i = 6; i < 9; i ++) {
		ttable [i] [2].type = TF_Straight;
		ttable [i] [2].blacks = Dir_WE;
		ttable [i] [2].illum = Dir_WE;
		ttable [i] [2].ilco = Ill_Red;
	}

	for (i = 5; i < 9; i ++) {
		ttable [i] [3].ilco = Ill_Dark;
	}
	for (i = 9; i < 12; i ++) {
		ttable [i] [3].ilco = Ill_Red;
	}


	ttable [2] [3].type = TF_HpSig;
	ttable [2] [3].blacks = Dir_WE;
	ttable [2] [3].illum = 0;
	ttable [2] [3].ilco = Ill_Dark;
	ttable [2] [3].fbp = &fbox1;

	ttable [8] [3].type = TF_HpSig;
	ttable [8] [3].blacks = Dir_WE;
	ttable [8] [3].illum = 0;
	ttable [8] [3].ilco = Ill_Dark;
	ttable [8] [3].fbp = &fbox2;

	ttable [8] [2].type = TF_HpSig;
	ttable [8] [2].blacks = Dir_WE;
	ttable [8] [2].illum = 0;
	ttable [8] [2].ilco = Ill_Dark;
	ttable [8] [2].fbp = &fbox3;

	ttable [12] [3].type = TF_HpSig;
	ttable [12] [3].blacks = Dir_WE;
	ttable [12] [3].illum = 0;
	ttable [12] [3].ilco = Ill_Dark;
	ttable [12] [3].fbp = &fbox4;

	ttable [15] [3].type = TF_HpSig;
	ttable [15] [3].blacks = Dir_WE;
	ttable [15] [3].illum = 0;
	ttable [15] [3].ilco = Ill_Dark;
	ttable [15] [3].fbp = &fbox5;
}

#define XCenter(n) (FieldXSize * (n) + FieldXSize / 2 - 1)
#define YCenter(n) (FieldYSize * (n) + FieldYSize / 2 - 1)

struct dtabent {
	unsigned char bit;
	int xf;
	int yf;
} dirtab [8] = {
	{ Dir_W,  -1,  0 },
	{ Dir_SW, -1,  1 },
	{ Dir_S,   0,  1 },
	{ Dir_SE,  1,  1 },
	{ Dir_E,   1,  0 },
	{ Dir_NE,  1, -1 },
	{ Dir_N,   0, -1 },
	{ Dir_NW, -1, -1 }
};

RedrawIllum (struct tfield *tp) {
	GC *gcp;
	int b;
	int i = tp->x;
	int j = tp->y;

	switch (tp->ilco) {
	case Ill_Red:
		gcp = &myredgc;
		break;
	case Ill_White:
		gcp = &myclrgc;
		break;
	case Ill_Dark:
		gcp = &mygc;
		break;
	default:
		gcp = &mygraygc;
		break;
	}
	if (tp->blacks == Dir_WE && tp->illum == Dir_WE) {
		XDrawLine (mydisplay,
			   basewindow,
			   *gcp,
			   XCenter (i)
			     - (FieldXSize / 3) + 1,
			   YCenter (j),
			   XCenter (i)
			     + (FieldXSize / 3) - 1,
			   YCenter (j));
	} else for (b = 0; b < 8; b ++) {
		if (tp->illum & dirtab [b].bit) {
			XDrawLine (mydisplay,
				   basewindow,
				   *gcp,
				   XCenter (i)
				     + dirtab [b].xf
				       * (FieldXSize / 3),
				   YCenter (j)
				     + dirtab [b].yf
				       * (FieldYSize / 3),
				   XCenter (i)
				     + dirtab [b].xf
				       * (FieldXSize / 6),
				   YCenter (j)
				     + dirtab [b].yf
				       * (FieldYSize / 6));
		}
	}
}

RedoState (struct tfield *tp) {
	unsigned char a, b;
	int xb, yb;
	switch (tp->type) {
	case TF_Straight:
		tp->ilco = tp->occup
			     ? Ill_Red
			     : tp->locked
				 ? Ill_White
				 : Ill_Dark;
		RedrawIllum (tp);
		break;
	case TF_Switch:
		a = tp->is_branch ? tp->i_branch : tp->i_str;
		if (tp->occup || tp->locked) {
			/* Draw double end */
			a |= tp->i_base;
		}
		tp->illum = tp->blacks & ~a;
		tp->ilco = Ill_Dark;
		RedrawIllum (tp);

		tp->illum = a;
		tp->ilco = tp->occup ? Ill_Red : Ill_White;
		RedrawIllum (tp);
		break;
	case TF_HpSig:
		xb = XCenter (tp->x) + 2;
		yb = YCenter (tp->y) + 12;
		XDrawLine (mydisplay,
			   basewindow,
			   tp->is_open
			     ? tp->is_open == 1
				 ? mygc
				 : myyellowgc
			     : myredgc,
			   xb, yb,
			   xb, yb);
		XDrawLine (mydisplay,
			   basewindow,
			   tp->is_open ? mygreengc : mygc,
			   xb + 9, yb,
			   xb + 9, yb);
		break;
	}
}

HpSig (struct tfield *tp) {
	struct fbox *fp;
	struct swval *sw;
	struct occ **opp;
	struct tfield **tpp;
	if (tp->type != TF_HpSig || !tp->fbp) {
		return 0;
	}
	if (tp->is_open) {
		/* Close all open FBoxes if any */
		for (fp = tp->fbp; fp; fp = fp->next) {
			if (!fp->on) {
				continue;
			}
			/* This one it is */
			/* Unlock all occs */
			for (opp = fp->oclist + fp->len; *opp; opp ++) {
				if ((*opp)->lock) {
					(*opp)->lock = 0;
					for (tpp = (*opp)->tf_list; *tpp; tpp ++) {
						(*tpp)->locked --;
						RedoState (*tpp);
					}
				}
			}
			fp->on = 0;
		}
		tp->is_open = 0;
		RedoState (tp);
	} else {
		/* Find one that we can open */
		for (fp = tp->fbp; fp; fp = fp->next) {
			if (fp->on) {
				continue;
			}
			for (sw = fp->swlist; sw->swtch; sw ++) {
				if (sw->swtch->type != TF_Switch
				    || sw->swtch->is_branch != sw->is_branched) {
					goto next;
				}
			}
			for (opp = fp->oclist; *opp; opp ++) {
				if ((*opp)->cnt != 0) {
					goto next;
				}
				for (tpp = (*opp)->tf_list; *tpp; tpp ++) {
					if ((*tpp)->locked) goto next;
				}
			}

			/* Now we got one, lock it */
			/* Need only to lock occs (will contain switches) */
			for (opp = fp->oclist; *opp; opp ++) {
				(*opp)->lock = tp;
				for (tpp = (*opp)->tf_list; *tpp; tpp ++) {
					(*tpp)->locked ++;
					RedoState (*tpp);
				}
			}
			fp->on = 1;
			tp->is_open = fp->slow ? 2 : 1;
			fp->len = 0;
			fp->occlen = 0;
			RedoState (tp);
			return;
next:			continue;
		}
	}
}

RedoFBox (struct tfield *tp) {
	struct fbox *fp;
	struct swval *sw;
	struct occ **opp;
	struct tfield **tpp;
	if (tp->type != TF_HpSig || !tp->fbp) {
		return 0;
	}
	/* Check all open FBoxes if any */
	for (fp = tp->fbp; fp; fp = fp->next) {
		if (!fp->on) {
			continue;
		}
		/* Advance occlen */
		while (fp->oclist [fp->occlen]) {
			if (!fp->oclist [fp->occlen]->cnt) {
				break;
			}
			fp->occlen ++;
		}
		if (fp->occlen == fp->minocclen) {
			/* First one occ'd, pull sig */
			if (tp->is_open) {
				tp->is_open = 0;
				RedoState (tp);
			}
		}
		/* Advance len, thereby freeing occs */
		while (fp->oclist [fp->len] && fp->len < fp->occlen) {
			if (fp->oclist [fp->len]->cnt) {
				/* Still occ'd */
				if (fp->oclist [fp->occlen]) {
					/* Not all occ'd */
					break;
				}
			}
			fp->oclist [fp->len]->lock = 0;
			for (tpp = fp->oclist [fp->len]->tf_list; *tpp; tpp ++) {
				(*tpp)->locked --;
				RedoState (*tpp);
			}
			fp->len ++;
		}
		if (!fp->oclist [fp->len]) {
			/* Final handling */
			fp->on = 0;
		}
	}
}

int Debug=0;

int done=0;

int r_flg = 0, t_flg = 0, a_flg = 0;

char *fnam=0;

void everysec (void);

char *pszHost;

handle_window(myevent) XEvent myevent; {
    struct tfield *tp;
    int x,y; {
	KeySym mykey;
	char text[10];
	int i;
	switch(myevent.type) {
	  case Expose:
	    if(myevent.xexpose.count) break;
	    XClearArea(mydisplay,basewindow,0,0,0,0,False);
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
		todo=1; }
	    break;
	  case ButtonPress:
	    switch(myevent.xbutton.button) {
	      case 1:
		x = myevent.xbutton.x / FieldXSize;
		y = myevent.xbutton.y / FieldYSize;
		if (x >= 0 && x < NTX && y >= 0 && y < NTY) {
			struct tfield **tpp;

			tp = &ttable [x] [y];
			switch (tp->type) {
			case TF_Straight:
				if (!tp->occp) {
					break;
				}
			case TF_Occ:
				tp->occp->cnt = !tp->occp->cnt;
				for (tpp = tp->occp->tf_list; *tpp; tpp ++) {
					(*tpp)->occup = !!tp->occp->cnt;
					RedoState (*tpp);
				}
				if (tp->occp->lock) {
					/* Cause reconsideration of FBox */
					RedoFBox (tp->occp->lock);
				}
				break;
			case TF_Switch:
				if (!tp->occup && !tp->locked) {
					tp->is_branch = !tp->is_branch;
					RedoState (tp);
				}
				break;
			case TF_HpSig:
				HpSig (tp);
				break;
			}
		}
		
		break;
	      case 2:
		break;
	      case 3:
		done=1;
		break; }
	    break;
	  case MotionNotify:
	    break;
	  case ButtonRelease:
	    break;
	  case KeyPress:
	    break; }}}

main(argc,argv) int argc; char **argv; {
    XEvent myevent;
    XSizeHints myhint;
    int myscreen;
    unsigned long myforeground, mybackground;
    int i,j,x,y;
	struct pollfd pollitem;
	struct timeval currtime;
    char *b;
    char *display_name;
    char *fontname=0;
    char *deffont="-adobe-helvetica-medium-r-normal--14-140-75-75-p-77-iso8859-1";
    int dummy;
    XCharStruct Dummy;

    Colormap cmap;
    XColor exact, color;

    display_name=getenv("DISPLAY");

    for(i=1;i<argc;i++) {
	if(argv[i][0]=='-') {
	    if(!strcmp(argv[i],"-display")) {
		if(++i==argc) printf("No display name\n");
		else display_name=argv[i]; }
	    else if(!strcmp(argv[i],"-fn")) {
		if(++i==argc) printf("No font name\n");
                else fontname=argv[i]; }
	    else if(!strcmp(argv[i],"-geometry")) {
		if(++i==argc) printf("No geometry\n"); }
	    else if(!strcmp(argv[i],"-d")) {
                Debug++; }
	    else if(!strcmp(argv[i],"-r")) {
                r_flg++; }
	    else if(!strcmp(argv[i],"-t")) {
                t_flg++; }
	    else if(!strcmp(argv[i],"-a")) {
                a_flg++; }
	    else printf("Unknown option %s\n",argv[i]); }
	else {
	    fnam=argv[i]; }}
    if(!display_name) display_name="";

    pszHost = fnam;
    if (!pszHost) pszHost = "esp3";

    initttable ();

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
    myhint.x=FieldXSize * 20 - 1;
    myhint.y=FieldYSize * 7 - 1;
    myhint.width=xsize;
    myhint.height=ysize;
    myhint.flags=PSize;
    basewindow=XCreateSimpleWindow(mydisplay,
       DefaultRootWindow(mydisplay),
       myhint.x,myhint.y,myhint.width,myhint.height,
       1, myforeground,mybackground);
    XSetStandardProperties(mydisplay,basewindow,"esim","ESim",
       None,argv,argc,&myhint);
    mygc=XCreateGC(mydisplay,basewindow,0,0);
    XSetBackground (mydisplay, mygc, mybackground);
    XSetForeground (mydisplay, mygc, myforeground);

    myclrgc=XCreateGC(mydisplay,basewindow,0,0);
    XSetBackground (mydisplay, myclrgc, myforeground);
    XSetForeground (mydisplay, myclrgc, mybackground);

    cmap = DefaultColormap (mydisplay, myscreen);
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
    if (XAllocNamedColor (mydisplay, cmap, "yellow", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color yellow failed\n");
    }
    XSetForeground (mydisplay, myyellowgc, color.pixel);
    XSetBackground (mydisplay, myyellowgc, mybackground);

    mygreengc=XCreateGC(mydisplay,basewindow,0,0);
    if (XAllocNamedColor (mydisplay, cmap, "green", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color green failed\n");
    }
    XSetForeground (mydisplay, mygreengc, color.pixel);
    XSetBackground (mydisplay, mygreengc, mybackground);

    mybluegc=XCreateGC(mydisplay,basewindow,0,0);
    if (XAllocNamedColor (mydisplay, cmap, "lightskyblue", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color blue failed\n");
    }
    XSetForeground (mydisplay, mybluegc, color.pixel);
    XSetBackground (mydisplay, mybluegc, mybackground);

    mygraygc=XCreateGC(mydisplay,basewindow,0,0);
    if (XAllocNamedColor (mydisplay, cmap, "gray", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color gray failed\n");
    }
    XSetForeground (mydisplay, mygraygc, color.pixel);
    XSetBackground (mydisplay, mygraygc, mybackground);

    XSetLineAttributes (mydisplay, mygc, 9, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, myredgc, 5, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, mygreengc, 5, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, myyellowgc, 5, LineSolid, CapRound, JoinRound);
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

    done=0;
    do_disp();
    while(done==0) {
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
	pollitem.fd = ConnectionNumber (mydisplay);
	pollitem.events = POLLIN;
	gettimeofday (&currtime);
	i = poll (&pollitem, 1, (102 - ((currtime.tv_usec / 1000) % 100)));
	if (i == 0) {
		everysec ();
		if (todo) {
			XClearArea(mydisplay,basewindow,0,0,0,0,False);
			DrawPicture ();
		}
	}
    }
	
    XFreeGC(mydisplay,mygc);
    XDestroyWindow(mydisplay,basewindow);
    XCloseDisplay(mydisplay);
    exit(0); }

do_disp()
{
}

DrawPicture()
{
	int i, j;
	double s, t, u;
	int xx,yy;
	if(todo) {
		todo=0;
	}

	for (j = 0; j <= (xsize / FieldYSize); j ++) {
		for (i = 0; i <= (xsize / FieldXSize); i ++) {

			XFillRectangle (mydisplay, basewindow, mygraygc,
					FieldXSize * i, FieldYSize * j,
					FieldXSize - 1, FieldYSize - 1);
		}
	}

	for (j = 0; j <= (xsize / FieldYSize) && j < NTY; j ++) {
		for (i = 0; i <= (xsize / FieldXSize) && i < NTX; i ++) {
			struct tfield *tp = &ttable [i] [j];
			GC *gcp;
			int b, xb, yb;

			switch (tp->type) {
			case TF_Occ:
				XFillRectangle (mydisplay,
						basewindow,
						mygc,
						FieldXSize * i + 5,
						FieldYSize * j + 5,
						FieldXSize - 11,
						FieldYSize - 11);
				
				break;
			case TF_HpSig:
				xb = XCenter (i) - 10;
				yb = YCenter (j) + 12;
				XFillRectangle (mydisplay,
						basewindow,
						mygc,
						xb, yb - 4,
						1, 9);
				XFillRectangle (mydisplay,
						basewindow,
						mygc,
						xb, yb - 1,
						10, 3);
				XDrawLine (mydisplay,
					   basewindow,
					   mygc,
					   xb + 12, yb,
					   xb + 21, yb);
				break;
			}

			for (b = 0; b < 8; b ++) {
				if (tp->blacks & dirtab [b].bit) {
					XDrawLine (mydisplay,
						   basewindow,
						   mygc,
					   	   XCenter (i)
						     + dirtab [b].xf
						       * (FieldXSize / 2),
					   	   YCenter (j)
						     + dirtab [b].yf
						       * (FieldYSize / 2),
					   	   XCenter (i),
						   YCenter (j));
				}
			}

			RedoState (tp);

#if 0
			switch (tp->ilco) {
			case Ill_Red:
				gcp = &myredgc;
				break;
			case Ill_White:
				gcp = &myclrgc;
				break;
			case Ill_Dark:
				continue;
			default:
				gcp = &mygraygc;
				break;
			}
			if (tp->blacks == Dir_WE && tp->illum == Dir_WE) {
				XDrawLine (mydisplay,
					   basewindow,
					   *gcp,
					   XCenter (i)
					     - (FieldXSize / 3) + 1,
					   YCenter (j),
					   XCenter (i)
					     + (FieldXSize / 3) - 1,
					   YCenter (j));
			} else for (b = 0; b < 8; b ++) {
				if (tp->illum & dirtab [b].bit) {
					XDrawLine (mydisplay,
						   basewindow,
						   *gcp,
					   	   XCenter (i)
						     + dirtab [b].xf
						       * (FieldXSize / 3),
					   	   YCenter (j)
						     + dirtab [b].yf
						       * (FieldYSize / 3),
					   	   XCenter (i)
						     + dirtab [b].xf
						       * (FieldXSize / 6),
					   	   YCenter (j)
						     + dirtab [b].yf
						       * (FieldYSize / 6));
				}
			}
#endif

		}
	}

#if 0
	XDrawLine (mydisplay, basewindow, mygc,
		   XCenter (0), YCenter (5 + 3),
		   XCenter (17), YCenter (5 + 3));

	XDrawLine (mydisplay, basewindow, mygc,
		   XCenter (4), YCenter (5 + 3),
		   XCenter (5), YCenter (5 + 2));

	XDrawLine (mydisplay, basewindow, mygc,
		   XCenter (5), YCenter (5 + 2),
		   XCenter (10), YCenter (5 + 2));

	XDrawLine (mydisplay, basewindow, mygc,
		   XCenter (9), YCenter (5 + 2),
		   XCenter (10), YCenter (5 + 3));

	for (i = 1; i < 17; i ++) {
		int j;

		if (i != 4 && i != 10) {
			XDrawLine (mydisplay, basewindow, myclrgc,
			   XCenter (i) - FieldXSize / 3 + 1, YCenter (5 + 3),
			   XCenter (i) + FieldXSize / 3 - 1, YCenter (5 + 3));
		}

		switch (i) {
		case 4:
		case 10:
			j = 3;
			break;
		case 9:
			j = 2;
			break;
		default:
			continue;
		}

		XDrawLine (mydisplay, basewindow, myclrgc,
		   XCenter (i) - FieldXSize / 3, YCenter (5 + j),
		   XCenter (i) - FieldXSize / 6, YCenter (5 + j));
		XDrawLine (mydisplay, basewindow, myclrgc,
		   XCenter (i) + FieldXSize / 6, YCenter (5 + j),
		   XCenter (i) + FieldXSize / 3, YCenter (5 + j));

	}

	for (i = 6; i < 9; i ++) {
		if (i != 4 && i != 10) {
			XDrawLine (mydisplay, basewindow, myredgc,
			   XCenter (i) - FieldXSize / 3 + 1, YCenter (5 + 2),
			   XCenter (i) + FieldXSize / 3 - 1, YCenter (5 + 2));
		}
	}

	XDrawLine (mydisplay, basewindow, myclrgc,
	   XCenter (5) - FieldXSize / 3, YCenter (5 + 2) + FieldYSize / 3,
	   XCenter (5) - FieldXSize / 6, YCenter (5 + 2) + FieldYSize / 6);
	XDrawLine (mydisplay, basewindow, myclrgc,
	   XCenter (5) + FieldXSize / 6, YCenter (5 + 2),
	   XCenter (5) + FieldXSize / 3, YCenter (5 + 2));
#endif

#if 0
	XFillRectangle (mydisplay, basewindow, mygc,
			35, 35, 120, 230);

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
#endif

}

void everysec (void) {
}
