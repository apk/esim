/* Mode:C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include "muxbmf.h"

MUX_TIMEOUT_TYP xTo;
MUX_FD_TYP xX;

MUX_BMFCONNLIST_TYP xConnList;
MUX_BMFLSTN_TYP xListen;

Display *mydisplay;
Window drawwindow;
Window basewindow;
Window smlwin;
Cursor mycurs;
Cursor mydotcurs;
GC mygc, myclrgc;
GC myredgc, myyellowgc, mygreengc, mybluegc, mygraygc, mywhitegc;
GC mysmlgc, mysmlredgc, mysmlclrgc, mysmlgreengc;
XFontStruct *myfontstruct;
long font_height;
long font_depth;
unsigned long myforeground, mybackground, markground, spaceground;

int FieldXSize = 24;	/* 27 */
int FieldYSize = 16;	/* 18 */

#define XXIII 12
#define KMH2MMS(x) (((x) * 10000) / 36)
#define MMS2KMH(x) (((x) * 36 + 5000) / 10000)

static int XCenter (int v) {
	int r = FieldXSize * v + FieldXSize / 2 - 1;
	return r / 2;
}

static int YCenter (int v) {
	int r = FieldYSize * v + FieldYSize / 2 - 1;
	return r / 2;
}

int todo = 1;

int emulsteps = 0;

void panic (char *s) {
	printf ("Panic: %s\n", s);
	fflush (stdout);
	*(int*)0=0;
	exit (1);
}

int checkfail (char *t, char *f, int l) {
	return printf ("%s: %d: Check failed: %s\n", f, l, t);
}

#define check(x) ((void)((x) || checkfail (#x, __FILE__, __LINE__)))

#define AT if (iDebugLevel > 2) printf ("At %s:%d\n", __FILE__, __LINE__)
	

int xsize = 1145, ysize = 700;
int totalysize;
int ypos = 0;

/* int xsize = 37 * FieldXSize - 1, ysize = 44 * FieldYSize - 1; */

#define Switch		0x100
#define Signal		0x200
#define Reverse		0x400
#define HpSig		0x800
#define LsSig		0x1000
#define CtlTwo		0x2000
#define Stop		0x4000
#define FullCross	0x8000
#define HalfCross	0x10000
#define Nothing		0x20000

#define Eoln		(0)
#define JoinSwitch	(1 | Switch)
#define BranchSwitch	(2 | Switch | Reverse)
#define FwdMain		(3 | Signal | HpSig)
#define FwdMainShnt	(4 | Signal | HpSig | LsSig)
#define FwdShnt		(5 | Signal | LsSig)
#define RevMain		(6 | Signal | Reverse | HpSig)
#define RevMainShnt	(7 | Signal | Reverse | HpSig | LsSig)
#define RevShnt		(8 | Signal | Reverse | LsSig)
#define Other		(9)
#define Error		(10)
#define Unit		(11)
#define Meter		(12)
#define Name		(13)
#define Begin		(14 | CtlTwo)
#define End		(15 | CtlTwo)
#define Speed		(16)
#define BeginStop	(17 | CtlTwo)
#define EndStop		(18 | CtlTwo)
#define FullCrossSwitch	(19 | Switch | FullCross)
#define FwdCrossSwitch	(20 | Switch | HalfCross)
#define RevCrossSwitch	(21 | Switch | HalfCross)
#define CurveCrossSwitch (22 | Switch | HalfCross)
#define Crossing	(23 | HalfCross | CtlTwo)

#define Straight	(23)
#define Curve		(24)

#define Dir_E		(32 | 0)
#define Dir_SE		(32 | 1)
#define Dir_S		(32 | 2)
#define Dir_SW		(32 | 3)
#define Dir_W		(32 | 4)
#define Dir_NW		(32 | 5)
#define Dir_N		(32 | 6)
#define Dir_NE		(32 | 7)

#define DirCode(n)	((n) & 7)
#define IsDir(n)	(((n) & ~7) == 32)
#define IsGoodSigDir(n)	(DirCode (n) == DirCode (Dir_E) || DirCode (n) == DirCode (Dir_W))
#define IsSameDir(n,m)	(DirCode (n) == DirCode (m))
#define IsCompatDir(n,m)	\
	(DirCode (n) == DirCode ((m) - 1) ||	\
	 DirCode (n) == DirCode (m) ||	\
	 DirCode (n) == DirCode ((m) + 1))
#define ReverseDir(x) (DirCode ((x) + 4))

struct {
	char *str;
	int tok;
} ttab [] = {
	{ "~" , Curve },
	{ ">w", JoinSwitch },
	{ "w<", BranchSwitch },
	{ "><", Crossing },
	{ ">w<", FullCrossSwitch },
	{ "ekw<", FwdCrossSwitch },
	{ ">ekw", RevCrossSwitch },
	{ "ekw^", CurveCrossSwitch },
	{ "hp>", FwdMain },
	{ "<hp", RevMain },
	{ "hs>", FwdMainShnt },
	{ "ls>", FwdShnt },
	{ "<hs", RevMainShnt },
	{ "<ls", RevShnt },
	{ "e", Dir_E },
	{ "se", Dir_SE },
	{ "s", Dir_S },
	{ "sw", Dir_SW },
	{ "w", Dir_W },
	{ "nw", Dir_NW },
	{ "n", Dir_N },
	{ "ne", Dir_NE },
	{ "begin", Begin },
	{ "end", End },
	{ "begin>", BeginStop },
	{ "<end", EndStop },
	{ 0, 0 }
};

int gettok (char **pp, int *val, char **cal) {
	char *p = *pp;
	char *q;
	char buf [256];
	int n = 0;
	while (*p == ' ' || *p == '\t' || *p == '\n') {
		p ++;
	}
	if (!*p || *p == '#') {
		*pp = p;
		return Eoln;
	}
	if (*p == '/') {
		*pp = p + 1;
		return Other;
	}
	if (*p == '"') {
		p ++;
		q = buf;
		while (*p != '"') {
			if (!*p) return Error;
			*q ++ = *p ++;
			if (q >= buf + sizeof (buf)) return Error;
		}
		*q = 0;
		*cal = strdup (buf);
		*pp = p + 1;
		return Name;
	}
	if (*p == '+' || *p == '-' ||  *p == '=' || (*p >= '0' && *p <= '9')) {
		char sg = *p;
		static int lastval = 0;
		static int offset = 0;
		char aaf = 0;
		int of = 0;
		if (sg == '+' || sg == '-' || sg == '=') {
			p ++;
			if (*p == '=') {
				aaf = 1;
				p ++;
			}
			n = 0;
		} else {
			sg = 0;
			n = *p ++ - '0';
		}
		while (*p >= '0' && *p <= '9') {
			if (n > 100000) {
				*pp = p;
				return Error;
			}
			n = 10 * n + *p ++ - '0';
		}
		if (sg == '-') {
			n = -n;
		} else if (sg == '=') {
			if (*p == '+') {
				p ++;
				while (*p >= '0' && *p <= '9') {
					if (of > 100000) {
						*pp = p;
						return Error;
					}
					of = 10 * of + *p ++ - '0';
				}
			} else if (*p == '-') {
				p ++;
				while (*p >= '0' && *p <= '9') {
					if (of > 100000) {
						*pp = p;
						return Error;
					}
					of = 10 * of + *p ++ - '0';
				}
				of = -of;
			}
			lastval = offset;
		}
		if (*p == 'u') {
			n *= 2;
			if (sg) {
				n = (lastval += n);
			}
			*pp = p + 1;
			*val = n;
			offset += 2 * of;
			return Unit;
		}
		if (*p == 'v') {
			if (sg) {
				n = (lastval += n);
			}
			*pp = p + 1;
			*val = n;
			offset += of;
			return Unit;
		}
		if (*p == 'd') {
			*pp = p + 1;
			*val = n * 100;
			return Speed;
		}
		if (*p == 'D') {
			*pp = p + 1;
			*val = n;
			return Speed;
		}
		if (*p == 'm') {
			*pp = p + 1;
			*val = n;
			return Meter;
		}
		*pp = p;
		return Error;
	}
	q = p;
	while (*p && *p != ' ' && *p != '\t' && *p != '\n' &&
	       *p != '/' && *p != '#' && *p != '"') {
		p ++;
	}
	for (n = 0; ttab [n].str; n ++) {
		if (strlen (ttab [n].str) == (p - q) &&
		    !strncmp (ttab [n].str, q, p - q)) {
			*pp = p;
			return ttab [n].tok;
		}
	}
	return Error;
}

#define Useen 1
#define Mseen 2
#define Dseen 4
#define Sseen 8

struct moff {
	int x;
	int y;
	char r;
};

struct moff defmoff = { 0, 0, 0 };

struct moff *minioff = &defmoff;

struct coord {
	int x;
	int y;
};

struct rect {
	int x;
	int y;
	int w;
	int h;
};

struct dnode {
	int type;
	struct sig *lock;	/* The locking signal (at end of path) */
	struct dnode *lknext;	/* Next in locking list */
	int state;
#define DST_Hp1 1
#define DST_Hp2 2
#define DST_Sh1 4
#define DST_Kl 8
#define DST_Vr0 16
#define DST_Vr1 32
#define DST_Vr2 64
#define DST_VrWh 128
	int propstate;

	int hps, vrs;

	int sigdir;				/* Direction of signal */
	int occcnt;				/* Occupation count */
	int mode;				/* Auto mode */
	struct dnode *tnext;			/* Total list */
	struct coord cent, one, two, three;	/* Display positions */
	struct conn *swtch;			/* Switch */
	struct sig *sig;			/* Signal */
	struct moff *moff;

	int cycno;				/* Cycle last affected in */
	struct train *ctrn;	/* Another hack */
	struct coord four;			/* Fourth display position */
	struct conn *swtcb;			/* Other of fullcross */
};

int cycle = 1;		/* Emulation cycle counter */

void DrawDIllum (struct dnode *x);
void DrawSmallDNode (struct dnode *x);

struct splan {
	char *plan;
	int n;
	struct splink **H;
};

struct splink {
	struct splink *next;
	char *pos;
};

void initplan (struct splan *spl, char *p) {
	spl->plan = p;
	spl->n = 5;
	spl->H = 0;
}

struct conn {
	struct trk *a, *c;	/* Backward conns */
	struct trk *b, *d;	/* Forward conns */
	struct sig *sf, *sr;	/* Signal forward, backward */
	int c_na, d_nb;		/* Switch flags */
	struct dnode *actl;	/* Backward switch field */
	struct dnode *bctl;	/* Forward switch field */
	struct conn *anxt;	/* Other conns of A motor (ring) */
	struct conn *bnxt;	/* Other conns of A motor (ring) */
};

struct conn *startconn = 0;	/** XXX Where to start... */

struct trk {
	struct conn *f;		/* Forward connector */
	struct conn *r;		/* Backward connector */
	int len;		/* Track length (mm) */
	int maxsp;		/* Max Speed (mm/s) */
	struct tip *t;		/* List of train ends on track */
	int use;		/* 1 = Train fully on trk */
	struct dnode *x;	/* Display node */
};

struct tip {
	struct train *trn;	/* The whole train */
	struct tip *next;	/* Next in list on trk */
	struct trk *on;		/* Track element this end is on */
	int pos;		/* Position on trk (mm) */
	int rev;		/* 1 = heading against trk direction */
};

struct train {
	struct train *next;	/* List of */
	int dir;		/* Index of front tip (dir of motion) */
	struct tip ends [2];	/* Both ends of the train */
	int oflg;
	int len;
	int speed;
	int accto;		/* Acceleration inhibit timer */
	int ztim;		/* Zero speed time */
	int maxspeed;
	char *name;
	struct splan plan;
	char *ppos;

	Window dwin;		/* The data window */
	char dtxt [100];	/* The current text */
	struct sig *dsig;	/* Where it is positioned */
	int sigdelay;			/* Delay until next sigopen allowed */
};

struct sig {
	struct sig *anext;	/* Next active path end */
	struct conn *c;
	char *name;
	struct dnode *d;
	struct dnode *pre;	/* The track in front */
	struct dnode *lcks;	/* List of locked dnodes, starting with sig */
	int sp;			/* Speed of open path to this sig */
	int len;		/* Length of path */
	struct dnode *flck;	/* Starting node of open path (may be inv.) */
	int im;			/* (Flag) Intermediate signal in path */
	struct ilg *grp;	/* Interlock group list */
	struct qlgm *qgrp;	/* Quorum interlock group list */
	int dsigcnt;		/* How many trains see me as stop? */
	int maxsp;		/* Posted maximum speed */
	int cnt;		/* Usage count */
};

struct ilg {
	struct ilg *next;
	struct sig *sig;
};

struct qlgm {
	struct qlgm *next;
	int num;
	struct sig *sig;
	struct qlg *grp;
};

struct qlg {
	struct qlg *next;
	struct sig *sig;
	int unchecked;
};

struct sig *actlist = 0;	/* Active path list */

struct train *currtrn = 0;	/* I hate globals, but is the easiest way... */

void do_paths (void) {
	struct sig *oact;
	struct sig *nx;
	struct dnode *d;
	for (oact = actlist, actlist = 0; oact; oact = nx) {
		/* do... */
		/* XXX This should be in a while loop, but it is repeated often enough to work this way. */
		d = oact->lcks;
		check (d->lock == oact);
		if (d->sig && (d->type & HpSig)) {
			if ((d->lknext &&
			     d->lknext->lknext &&
			     d->lknext->lknext->occcnt) || 
			    (d->lknext && d->lknext->occcnt &&
			     d->sig->pre && !d->sig->pre->occcnt)) {
				/* Second field occupied, halt */
				d->state = 0;
				d->lock = 0;
				d->sig->im = 0;
				oact->lcks = d->lknext;
				DrawDIllum (d);
				DrawSmallDNode (d);
				d = oact->lcks;
				if (!d->occcnt) {
					/* First field already free: drop */
					d->lock = 0;
					oact->lcks = d->lknext;
					DrawDIllum (d);
					DrawSmallDNode (d);
				}
			}
		} else if (d->sig && (d->type & LsSig)) {
			d->state = 0;
			d->lock = 0;
			oact->lcks = d->lknext;
			DrawDIllum (d);
			DrawSmallDNode (d);
		} else {
			if (!d->occcnt) {
				/* Became free... */
				d->lock = 0;
				oact->lcks = d->lknext;
				DrawDIllum (d);
				DrawSmallDNode (d);
			}
		}
		nx = oact->anext;
		if (oact->lcks) {
			oact->anext = actlist;
			actlist = oact;
		}
	}
}

int try_build_path (struct dnode *d) {
	struct dnode **dpp;
	struct dnode *droot;
	struct dnode *x;
	struct trk *tk;
	struct conn *c;
	struct sig *sig;
	int len = 0;
	int sp = 55555;
	int f;
	int vr;
	int vrl;
	int vrs;
	int kl_f;
	if (!d->sig) {
		printf ("No signal on trying dnode!\n");
		return 0;
	}
	/*
	 * Now first build a list of dnodes from here (including the
	 * signal itself) to a terminating signal. When the build fails
	 * the list has to be undone. (We do not know the terminating
	 * signal now, so have the root locally.)
	 */
	dpp = &droot;
	*dpp = 0;

	if (!(d->type & HpSig)) {
		/* Only do HpSigs */
		return 0;
	}

	if (d->lock) {
		/* Signal itself is locked */
		return 0;
	}
	*dpp = d;
	dpp = &d->lknext;
	*dpp = 0;

	c = d->sig->c;
	if (d->sig == c->sr) {
		/* Follow forward direction */
		f = 1;
		tk = c->c_na ? c->c : c->a;
	} else {
		check (d->sig == c->sf);
		/* Follow backward direction */
		f = 0;
		tk = c->d_nb ? c->d : c->b;
	}
	check (tk && tk->x);
	d->sig->pre = tk->x;
	while (1) {
		if (f) {
			/* Go forward from conn */
			/*
			 * (Switches need not be done since they always
			 * include some track that references the same dnode.
			 */
			tk = c->d_nb ? c->d : c->b;
		} else {
			tk = c->c_na ? c->c : c->a;
		}
		if (!tk) {
			/* At end of something? */
			break;
		}
		if (tk->maxsp && tk->maxsp < sp) {
			sp = tk->maxsp;
		}
		len += tk->len;
		x = tk->x;
		if (x) {
			/* (Some tracks are virtual) */
			if (x->occcnt || x->lock) {
				break;
			}
			*dpp = x;
			dpp = &x->lknext;
			*dpp = 0;
		}
		if (c == tk->r) {
			c = tk->f;
		} else {
			check (c == tk->f);
			c = tk->r;
		}
		if (tk == (c->c_na ? c->c : c->a)) {
			f = 1;
			sig = c->sr;
		} else if (tk == (c->d_nb ? c->d : c->b)) {
			f = 0;
			sig = c->sf;
		} else {
			break;
		}
		if (sig && sig->d->type & HpSig &&
		    len < 400000 && sp < 55555) {
			if (sig->d->lock) break;
			/* Short destination, must go to Kl */
			*dpp = sig->d;
			dpp = &sig->d->lknext;
			*dpp = 0;
		} else if (sig && sig->d->type & HpSig) {
			/* Found a terminating signal which is the lock */
			if (sig->lcks) {
				/* Already locked? */
				break;
			}
			if (sig->d->lock && sig->im) {
				/* Old path ahead */
				break;
			}
			if (sp > KMH2MMS (30) && sig->d->type & Stop) {
				sp = KMH2MMS (30);
			}
			sig->anext = actlist;
			actlist = sig;
			sig->lcks = droot;
			sig->flck = droot;
			sig->sp = sp;
			sig->len = len;
			for (x = droot; x; x = x->lknext) {
				x->lock = sig;
				if (x != droot) {
					if (x->sig && x->type & HpSig) {
						x->state = DST_Kl;
						x->sig->im = 1;
					} else if (x->sig && x->type & LsSig) {
						x->state = DST_Sh1;
					}
					DrawDIllum (x);
					DrawSmallDNode (x);
				}
			}
			/* Finally locate end of rotal path, then
			 * redo all signals on the path
			 */
			while (sig->d->lock) sig = sig->d->lock;
			len = 0;
			vr = DST_Vr0;
			vrs = 0;
			vrl = 0;
			sp = 55555;
			kl_f = 0;
			while (sig && sig->lcks && sig->lcks == sig->flck) {
				struct dnode *src = sig->lcks;
				len += sig->len;
				vrl += sig->len;
				if (sp > sig->sp) sp = sig->sp;
				if (vrl > 1410000) {
					if (kl_f) {
						vr = DST_Vr1;
						vrl -= 1100000;
						vrs = 55555;
						kl_f = 0;
					} else {
						vr = 0;
						vrs = -1;
					}
				}
				if (src->sig->maxsp > 0 && src->sig->maxsp <= sp) {
					sp = 55555;
				}
				if (44444 <= sp) {
					/* > 160: Hp1 */
					sp = 55555;
				}
				if (sp < 18000) {
					src->state = DST_Hp2;
					src->hps = sp;
					src->state |= vr;
					src->vrs = vrs;
					if (vrl < 800000 && vr) {
						src->state |= DST_VrWh;
					}
					len = 0;
					vrs = sp;
					sp = 55555;
					vr = DST_Vr2;
					vrl = 0;
					kl_f = 0;
				} else if (sig->len < 150000) {
					src->state = DST_Kl;
					src->hps = -1;
					src->vrs = -1;
				} else if ((len < 745000 && sp > 29166)
					|| (len < 595000 && sp > 26388)
					|| (len < 495000 && sp > 23611)
					|| (vrl < /*80*/740000 && src->state & DST_Kl)) {
					src->state = DST_Kl;
					src->state |= vr;
					src->hps = -1;
					src->vrs = vrs;
					if (vrl < 800000 && vr) {
						src->state |= DST_VrWh;
					} else {
						vr = 0;
					}
					kl_f = 0 /* XXX 1, does not quite work */;
					/* The green/white disappears again when the next one goes to Hp1, then the previous signal changes from Vr1Wh to Vr1, because there is no more immediate need for the VrWh */
				} else {
					src->state = DST_Hp1;
					src->hps = sp;
					src->vrs = vrs;
					if (sp < 55555) len = 0;
					vrs = sp;
					sp = 55555;
					src->state |= vr;
					if (vrl < 800000 && vr) {
						src->state |= DST_VrWh;
					} else {
						vr = 0;
					}
					vr = DST_Vr1;
					vrl = 0;
					kl_f = 0;
				}
				DrawDIllum (src);
				DrawSmallDNode (src);
				sig = src->sig;
			}
			return 1;
		} else if (sig && sig->d->type & LsSig) {
			if (sig->d->lock) break;
			*dpp = sig->d;
			dpp = &sig->d->lknext;
			*dpp = 0;
		} else if (sig) {
			if (sig->d->lock) break;
			printf ("Oops break...\n");
		}
	}
	/* Start undo (which is nothing to do) */
	return 0;
}

void turn_iring (struct conn *c, struct dnode *x, int v) {
	struct conn *cc = c;
	while (1) {
		if (x == cc->actl) {
			cc->c_na = v;
			cc = cc->anxt;
		} else if (x == cc->bctl) {
			cc->d_nb = v;
			cc = cc->bnxt;
		} else {
			check (0);
			cc = c;
		}
		if (c == cc) break;
	}
}

void turn_ring (struct conn *c, struct dnode *x, int v) {
	turn_iring (c, x, v);
	if (x->type == HalfCross) {
		/* Must turn the other point sometimes */
		if (c == x->swtch && v == 0) {
			turn_iring (x->swtcb, x, 0);
		} else if (c == x->swtcb && v == 1) {
			turn_iring (x->swtch, x, 1);
		}
	}
	DrawDIllum (x);
	DrawSmallDNode (x);
}

void turn_a (struct conn *c, int v) {
	if (c->c_na != v) {
		turn_ring (c, c->actl, v);
	}
}

void turn_b (struct conn *c, int v) {
	if (c->d_nb != v) {
		turn_ring (c, c->bctl, v);
	}
}

int switch_to_a (struct conn *c) {
	struct dnode *x = c->actl;
	if (c->c_na != 0) {
		if (x->ctrn != currtrn && x->cycno == cycle) return 1;
		turn_a (c, 0);
		x->cycno = cycle;
		x->ctrn = currtrn;
		return 1;
	}
	if (x) x->cycno = cycle;
	if (x) x->ctrn = currtrn;
	return 0;
}

int switch_to_b (struct conn *c) {
	struct dnode *x = c->bctl;
	if (c->d_nb != 0) {
		if (x->ctrn != currtrn && x->cycno == cycle) return 1;
		turn_b (c, 0);
		x->cycno = cycle;
		x->ctrn = currtrn;
		return 1;
	}
	if (x) x->cycno = cycle;
	if (x) x->ctrn = currtrn;
	return 0;
}

int switch_to_c (struct conn *c) {
	struct dnode *x = c->actl;
	if (c->c_na != 1) {
		if (x->ctrn != currtrn && x->cycno == cycle) return 1;
		turn_a (c, 1);
		x->cycno = cycle;
		x->ctrn = currtrn;
		return 1;
	}
	if (x) x->cycno = cycle;
	if (x) x->ctrn = currtrn;
	return 0;
}

int switch_to_d (struct conn *c) {
	struct dnode *x = c->bctl;
	if (c->d_nb != 1) {
		if (x->ctrn != currtrn && x->cycno == cycle) return 1;
		turn_b (c, 1);
		x->cycno = cycle;
		x->ctrn = currtrn;
		return 1;
	}
	if (x) x->cycno = cycle;
	if (x) x->ctrn = currtrn;
	return 0;
}

int try_make_subpath_c (struct conn *c,
			struct trk *tk,
			int dist,
			int m,
			char *n,
			int sp);

int try_make_subpath (struct conn *c,
		      int f,
		      int dist,
		      int m,
		      char *n,
		      int sp) {
	/* (Don't check the signal on c */
	int r;
	int ra, rb;
	if (f) {
		/* Go forward from conn */
		/*
		 * (Switches need not be done since they always
		 * include some track that references the same dnode.
		 */
		if (c->b) {
			ra = try_make_subpath_c (c, c->b, dist, 0, n, sp);
			if (ra < 0) return ra;
		} else {
			ra = 0;
		}
		if (c->d && n) {
			rb = try_make_subpath_c (c, c->d, dist, 0, n, sp);
			if (rb < 0) return rb;
		} else {
			rb = 0;
		}
		if (ra == 0 && rb == 0) return 0;
		if (ra >= rb && c->b) {
if (!m) return ra;
			r = try_make_subpath_c (c, c->b, dist, m, n, sp);
			if (r < 0) return r;
			if (r) {
				if (m) {
					if (switch_to_b (c)) return -1;
				}
				return r;
			}
		}
		if (rb >= ra && c->d && n) {
			if (sp > (60 * 10000) / 36) return 0;
if (!m) return rb;
			r = try_make_subpath_c (c, c->d, dist, m, n, sp);
			if (r < 0) return r;
			if (r) {
				if (m) {
					if (switch_to_d (c)) return -1;
				}
				return r;
			}
		}
	} else {
		if (c->a) {
			ra = try_make_subpath_c (c, c->a, dist, 0, n, sp);
			if (ra < 0) return ra;
		} else {
			ra = 0;
		}
		if (c->c && n) {
			rb = try_make_subpath_c (c, c->c, dist, 0, n, sp);
			if (rb < 0) return rb;
		} else {
			rb = 0;
		}
		if (ra == 0 && rb == 0) return 0;
		if (ra >= rb && c->a) {
if (!m) return ra;
			r = try_make_subpath_c (c, c->a, dist, m, n, sp);
			if (r < 0) return r;
			if (r) {
				if (m) {
					if (switch_to_a (c)) return -1;
				}
				return r;
			}
		}
		if (rb >= ra && c->c && n) {
			if (sp > (60 * 10000) / 36) return 0;
if (!m) return rb;
			r = try_make_subpath_c (c, c->c, dist, m, n, sp);
			if (r < 0) return r;
			if (r) {
				if (m) {
					if (switch_to_c (c)) return -1;
				}
				return r;
			}
		}
	}
	return 0;
}

int check_oppose (struct conn *c, struct sig *sig) {
	/* Return 1 when there is an opposing path before the
	 * next switch.
	 */

	struct trk *tk;
	int f;

	if (sig->grp) {
		struct ilg *ip;
		int n = 0;
if (iDebugLevel > 2) printf ("Check group:");
		for (ip = sig->grp; ip; ip = ip->next) {
if (iDebugLevel > 2) printf (" %s%s", ip->sig->name, ip->sig->lcks ? (ip->sig->anext ? "++" : "*") : "");
			/* if (ip->sig->lcks) continue; */
			if (currtrn && currtrn->dsig == ip->sig) {
				n ++;
				continue;
			}
			if (ip->sig->dsigcnt) continue;
			n ++;
		}
		if (n < 2) {
			if (iDebugLevel > 2) printf (": inhibit\n");
			return 1;
		}
		if (iDebugLevel > 2) printf (": ok\n");
	}

	if (sig->qgrp) {
		struct qlgm *im;
		for (im = sig->qgrp; im; im = im->next) {
			struct qlg *ip;
			int n = 0;
			int fl = 0;
if (iDebugLevel > 2) printf ("Check quorum:");
			for (ip = im->grp; ip; ip = ip->next) {
if (iDebugLevel > 2) printf (" %s%s", ip->sig->name, ip->sig->lcks ? (ip->sig->anext ? "++" : "*") : "");
				if (ip->sig == sig) {
					if (ip->unchecked) {
						/* May always approach */
						n = 0;
						break;
					}
				}
				if (currtrn && currtrn->dsig == ip->sig) {
					if (!ip->unchecked) {
						/* Already in group */
						fl = 1;
					}
				}
				/* if (ip->sig->lcks) { */
				if (ip->sig->dsigcnt) {
					if (!currtrn || currtrn->dsig != ip->sig) {
						n ++;
					}
				}
			}
			if (n >= im->num && !fl) {
				if (iDebugLevel > 2) printf (": inhibit\n");
				return 1;
			}
			if (iDebugLevel > 2) printf (": ok\n");
		}
	}

if (iDebugLevel > 2) printf ("Oppose %s", sig->name);

	if (sig == c->sf) {
		f = 0;
	} else if (sig == c->sr) {
		f = 1;
	} else {
		if (iDebugLevel > 2) printf (" fail\n");
		return 0;
	}

	while (1) {
		if (c->d || c->c) {
			if (iDebugLevel > 2) printf (" switch\n");
			return 0;	/* Has a switch */
		}
		if (f) {
if (c->sr) if (iDebugLevel > 2) printf (" [%s]", c->sr->name);
			sig = c->sf;
			tk = c->b;
		} else {
if (c->sf) if (iDebugLevel > 2) printf (" [%s]", c->sf->name);
			sig = c->sr;
			tk = c->a;
		}
		if (sig) {
			if (iDebugLevel > 2) printf (" %s", sig->name);
		}
		if (sig && sig->lcks) {
			if (iDebugLevel > 2) printf ("opposed\n");
			return 1;
		}
		if (!tk) {
			if (iDebugLevel > 2) printf (" eot\n");
			return 0;	/* End of track */
		}

		if (tk->f == c) {
			c = tk->r;
		} else {
			check (tk->r == c);
			c = tk->f;
		}

		if (tk == c->a || tk == c->c) {
			f = 1;
		} else {
			check (tk == c->b || tk == c->d);
			f = 0;
		}
	}
}

int try_make_subpath_c (struct conn *c, struct trk *tk, int dist, int m, char *n, int sp) {
	struct dnode *x;
	int f;
	struct sig *sig;
	int r = tk->maxsp ? tk->maxsp : 55555;
	int ra;
	dist += tk->len;
	x = tk->x;
	if (5 * sp > 6 * r) return 0;
	if (x && (x->occcnt || x->lock)) {
		return 0;
	}
	if (c == tk->r) {
		c = tk->f;
	} else {
		check (c == tk->f);
		c = tk->r;
	}
	if (tk == c->a) {
		if (m) if (switch_to_a (c)) return -1;
		f = 1;
		sig = c->sr;
	} else if (tk == c->c) {
		if (m) if (switch_to_c (c)) return -1;
		f = 1;
		sig = c->sr;
	} else if (tk == c->b) {
		if (m) if (switch_to_b (c)) return -1;
		f = 0;
		sig = c->sf;
	} else if (tk == c->d) {
		if (m) if (switch_to_d (c)) return -1;
		f = 0;
		sig = c->sf;
	} else {
		check (0);
		return 0;
	}
		
	if (sig && sig->d->type & HpSig) {
		/* Found a terminating signal which is the lock */
		if (sig->lcks) {
			/* Already locked */
			return 0;
		}
		if (!n || strcmp (n, sig->name) == 0) {
			/* Found matching signal */
			if (check_oppose (c, sig)) return 0;
			if (tk->maxsp) r --;
			return r;
		}
		if (dist > 3000000) {
			/* Too far to look further */
			return 0;
		}
	}
	ra = try_make_subpath (c, f, dist, m, n, sp);
	if (ra < 0) return ra;
	if (ra < r) r = ra;
	/* This does not do as expected, because we lose
	 * the point count from higher speeds once it gets lower.
	 * Would have to reserve lower bits for point count.
	 */
	if (r > 1 && tk->maxsp) r --;
	return r;
}

int try_opt_make_subpath (struct conn *c, int f, int dist, char *n, int sp) {
	int r = try_make_subpath (c, f, dist, 0, n, sp);
	if (r > 0) {
		r = try_make_subpath (c, f, dist, 1, n, sp);
	}
	return r;
}

int try_make_path (struct sig *sig, char *n, int sp) {
	struct dnode *d = sig->d;
	int r;
	if (!d || !d->sig) {
		printf ("No dnode on trying signal!\n");
		return 0;
	}

	if (!(d->type & HpSig)) {
		/* Only do HpSigs */
		return 0;
	}

	if (d->lock) {
		/* Signal itself is locked */
		return 0;
	}

	if (sig == sig->c->sr) {
		/* Follow forward direction */
		r = try_opt_make_subpath (sig->c, 1, 0, n, sp);
	} else {
		check (d->sig == sig->c->sf);
		/* Follow backward direction */
		r = try_opt_make_subpath (sig->c, 0, 0, n, sp);
	}
#if 0
	if (r > 0) {
		printf ("Made path from %s to %s\n",
			sig->name, n ? n : "<null>");
	}
#endif
	return r;
}

struct train *trlist = 0;

struct trk *find_next_trk (struct conn *c, struct trk *tk) {
	if (tk == c->a || tk == c->c) {
		/* Forward connector */
		if ((tk == c->c) != c->c_na) {
			return 0;
		}
		return c->d_nb ? c->d : c->b;
	} else if (tk == c->b || tk == c->d) {
		/* Backward connector */
		if ((tk == c->d) != c->d_nb) {
			return 0;
		}
		return c->c_na ? c->c : c->a;
	} else {
		panic ("Connector failure");
	}
	return 0;
}

int move_tip (struct tip *t, int adv) {
	/* adv < 0 means moving the tip backwards */
	struct conn *c;
	struct trk *tk = t->on;
	struct tip *tip, **tipp;
	struct tip *pr;
	int l;
	int dist = 0;
	int o_occ = tk->use || tk->t;	/* Old occ state */
	int n_occ;
	while (adv != 0) {
		int oadv = adv;
		struct trk *otk;
		check (!(t->rev & ~1));
		check (adv != 0);
		check (t->pos >= 0 && t->pos <= tk->len);
		for (pr = 0, tipp = &tk->t;
		     (tip = *tipp);
		     pr = tip, tipp = &tip->next) {
			if (tip == t) break;
		}
		tk->use = 0;
		check (tip == t);
		if (t->rev != (oadv > 0)) {
			/* Move in direction of track */
			if (t->next) {
				check (t->next->rev != t->rev);
				l = t->next->pos - t->pos;
			} else {
				l = tk->len - t->pos;
			}
			check (l >= 0);
			if (adv > 0) {
				if (l > adv) l = adv;
				dist += l;
				adv -= l;
			} else {
				if (l > -adv) l = -adv;
				dist += l;
				adv += l;
			}
			t->pos += l;
			if (t->pos < tk->len || t->next) {
				/* Not off the end */
				break;
			}
			if (t->rev == 0 && !adv) {
				/* Front end not passed if end of movement */
				break;
			}
			if (t->rev == 0) {
				tk->use = 1;
			}
			c = tk->f;
		} else {
			/* Move towards zero */
			if (pr) {
				check (pr->rev != t->rev);
				l = t->pos - pr->pos;
			} else {
				l = t->pos;
			}
			check (l >= 0);
			if (adv > 0) {
				if (l > adv) l = adv;
				dist += l;
				adv -= l;
			} else {
				if (l > -adv) l = -adv;
				dist += l;
				adv += l;
			}
			t->pos -= l;
			if (t->pos > 0 || pr) {
				/* Not off the end */
				break;
			}
			if (t->rev == 1 && !adv) {
				/* Reverse end did not pass track end */
				break;
			}
			if (t->rev == 1) {
				tk->use = 1;
			}
			c = tk->r;
		}
		/* Now try to localize next trk, then move tip onto there */
		otk = tk;
		tk = find_next_trk (c, tk);
#if 0
		printf ("Now: otk=%x, c=%x, tk=%x\n",
			(unsigned)otk, (unsigned)c, (unsigned)tk);
#endif
		if (!tk) {
			/* Cannot go on */
			tk = otk;	/* Needed for finish */
			break;
		}
		*tipp = tip->next;	/* Unchain from current */
		n_occ = otk->use || otk->t;
		if (n_occ != o_occ && otk->x) {
			otk->x->occcnt += n_occ - o_occ;
			DrawDIllum (otk->x);
			DrawSmallDNode (otk->x);
		}
		o_occ = tk->use || tk->t;
		if (c == tk->r) {
			/* Put into begin of track */
			t->next = tk->t;
			tk->t = t;
			t->pos = 0;
			t->rev = oadv < 0;
			t->on = tk;
		} else if (c == tk->f) {
			/* Put into end of track */
			for (tipp = &tk->t; (tip = *tipp); tipp = &tip->next);
			t->next = *tipp;
			*tipp = t;
			t->pos = tk->len;
			t->rev = oadv > 0;
			t->on = tk;
		} else {
			panic ("Next track not directed");
		}
	}
#if 0
	printf ("tk=%x\n", (unsigned)tk);
#endif
	n_occ = tk->use || tk->t;
	if (n_occ != o_occ && tk->x) {
#if 0
		printf ("tk=%x\n", (unsigned)tk);
		printf ("tk->x=%x\n", (unsigned)tk->x);
#endif
		tk->x->occcnt += n_occ - o_occ;
		DrawDIllum (tk->x);
		DrawSmallDNode (tk->x);
	}
	return dist;
}

int move_train (struct train *trn, int dist) {
	int a = move_tip (&trn->ends [trn->dir], dist > 0 ? dist : 0);
	int b = move_tip (&trn->ends [1 - trn->dir], -a);
	if (b != a) {
		printf ("Synchronicity error in move_train...\n");
	}
	return a;
}

char *find_dest (struct splan *spl, char *sign, char **ppos) {
	static char destname [100];
	char *p = spl->plan;
	int z;
	int i;
	if (!spl->plan || !sign) {
		return 0;
	}
	if (spl->n >= 0 && !spl->H) {
		/* Create the hash table */
		char *cp, *bp;
		struct splink *sp, **spp;
		printf ("Converting plan of %d chars\n", strlen (spl->plan));
		z = 0;
		if (spl->n == 0) spl->n = 53;
		spl->H = malloc (spl->n * sizeof (*spl->H));
		for (i = 0; i < spl->n; i ++) {
			spl->H [i] = 0;
		}
		cp = spl->plan;
		while (1) {
			unsigned int h = 0;
			bp = cp;
			while (*cp && *cp != '-') {
				if (*cp == '/') goto again;
				if (*cp == ',') goto again;
				h = (13 * h + (*cp)) % spl->n;
				cp ++;
			}
			for (spp = &spl->H [h]; *spp; spp = &(*spp)->next);
			sp = malloc (sizeof (struct splink));
			sp->pos = bp;
			sp->next = *spp;
			*spp = sp;
			while (*cp && *cp != ',' && *cp != '-') cp ++;
			if (!*cp) break;
			z ++;
again:
			cp ++;
		}
		printf ("Converted %d plan entries\n", z);
	}
	if (spl->n >= 0 && spl->H) {
		/* Use the hash table */
		unsigned int h = 0;
		int i;
		char *s, *q;
		struct splink *sp;
		for (i = 0; sign [i]; i ++) {
			h = (13 * h + (sign [i])) % spl->n;
		}
		for (sp = spl->H [h]; sp; sp = sp->next) {
			if (ppos && (*ppos > sp->pos)) continue;
			if (strncmp (sign, sp->pos, i) == 0) {
				if (sp->pos [i] == '-') {
					break;
				}
			}
		}
		if (!sp) {
			for (sp = spl->H [h]; sp; sp = sp->next) {
				if (strncmp (sign, sp->pos, i) == 0) {
					if (sp->pos [i] == '-') {
						break;
					}
				}
			}
		}
		if (!sp) return 0;
		s = destname;
		q = sp->pos + i + 1;
		while (*q && *q != ',' && *q != '-') {
			*s ++ = *q ++;
		}
		*s = 0;
		if (ppos) *ppos = sp->pos;
		return destname;
	}
	if (ppos && *ppos) {
		p = *ppos;
	}
	z = 0;
	while (1) {
		char *s = sign;
		char *q = p;
		while (*p == *s) {
			p ++;
			s ++;
		}
		if (*p == '-' && *s == 0) {
			/* Next is target */
			p ++;
			s = destname;
			while (*p && *p != ',' && *p != '-') {
				*s ++ = *p ++;
			}
			*s = 0;
			if (ppos) *ppos = q;
			return destname;
		}
		while (*p && *p != ',' && *p != '-') p ++;
		if (!*p) {
			if (++ z > 1) return 0;
			p = spl->plan;
			continue;
		}
		p ++;
	}
}

struct splan defplan = { 0 };
int useplans = 1;

int test_lookahead (struct train *trn,
		    int opendist,
		    int longopendist,
		    int *opensigs,
		    char *sigs,
		    int *eqdist,
		    struct sig **firstsigp) {
	int dist = 0;
	struct tip *t = &trn->ends [trn->dir];
	struct tip *tip, **tipp;
	struct trk *tk = t->on;
	struct tip *pr;
	struct conn *c;
	int tried_open = 0;

	if (trn->sigdelay > 0) {
		tried_open = 1;
	}

	/* Speed limit on current track */
	if (eqdist && tk->maxsp) {
		int eq = (tk->maxsp / 10) * (tk->maxsp / 100) + dist;
		if (*eqdist > eq) *eqdist = eq;
	}

	/* Process the current piece of track */
	for (pr = 0, tipp = &tk->t;
	     (tip = *tipp);
	     pr = tip, tipp = &tip->next) {
		if (tip == t) break;
	}
	switch (t->rev) {
	case 0:
		/* Goes forward */
		check (t->pos >= 0 && t->pos <= tk->len);
		tip = t->next;
		if (tip) {
			/* Another train end on this track */
			strcat (sigs, "*");
			check (tip->rev == 1);
			check (tip->pos >= t->pos);
			return dist += tip->pos = t->pos;
		}
		dist += tk->len - t->pos;
		c = tk->f;
		break;
	case 1:
		/* Goes backward */
		check (t->pos <= tk->len && t->pos >= 0);
		if (pr) {
			/* Another train before us */
			strcat (sigs, "*");
			check (pr->rev == 0);
			check (pr->pos <= t->pos);
			return dist += t->pos - pr->pos;
		}
		dist += t->pos;
		c = tk->r;
		break;
	default:
		panic ("Bad t->rev");
		return -2;
	}
	while (1) {
		struct sig *sig;
		/* Switch to next conn */
#if 0
		printf ("dist=%d, tk=%x, c=%x, c->a=%x, c->b=%x, c->c=%x, c->d=%x\n",
			dist, tk, c, c->a, c->b, c->c, c->d);
#endif
		if (tk == c->a || tk == c->c) {
			/* Forward connector */
			if ((tk == c->c) != c->c_na) {
				strcpy (sigs, ">");
				dist -= tk->len;
				if (dist < 0) dist = 0;
				return dist;
			}
			sig = c->sr;
		} else if (tk == c->b || tk == c->d) {
			/* Backward connector */
			if ((tk == c->d) != c->d_nb) {
				strcpy (sigs, ">");
				dist -= tk->len;
				if (dist < 0) dist = 0;
				return dist;
			}
			sig = c->sf;
		} else {
			panic ("Connector failure");
			return dist;
		}
#if 0
		if (c->c_na || c->d_nb) {
			/* Goes nonstraight, limit speed */
			int sp = (60 * 10000) / 36;
			int eq = (sp / 10) * (sp / 100) + dist;
			if (eqdist && *eqdist > eq) {
				*eqdist = eq;
			}
		}
#endif
		if (sig && !sig->d->state && !tried_open) {
			tried_open = 1;
			if (dist > 0) {
				strcpy (sigs, sig->name ? sig->name : "none");
				if (firstsigp /* && !*firstsigp */) {
					*firstsigp = sig;
				}
			}
			if (dist < longopendist && sig->d->mode) {
				int r;
				char *n = 0;
				char buf [8192];
				if (sig->d->type & Stop) {
					if (trn->speed == 0) {
						return -1;
					}
				}
				if (useplans) {
					n = find_dest (&trn->plan, sig->name,
						     &trn->ppos);
				}
				if (sig->d->mode == -1) {
					if (try_make_path (sig, n, trn->speed) > 0) {
						try_build_path (sig->d);
						trn->oflg ++;
					}
				} else if (sig->d->mode == -2) {
					try_build_path (sig->d);
				} else if (sig->d->mode == -3) {
					if (trn->speed == 0) {
						return -1;
					}
				} else if (n || (defplan.plan && (n = find_dest (&defplan, sig->name, 0)))) {
					int nth = 0;
					char *p, *q;
					strcpy (buf, n);
					p = buf;
					while (*p) {
						if (nth ++ > 0 && trn->speed * 36 > 100 * 10000) {
							/* No second choice at high speed */
							break;
						}
						if (nth > 1 && dist > opendist) {
							/* No far lookahead for second choice */
							break;
						}
						for (q = p;
						     *q && *q != '/';
						     q ++);
						if (q == p + 1 && *p == '$') {
							if (trn->speed == 0) {
								return -1;
							}
						}
						if (*q) {
							*q = 0;
/* These 'nth' references never work because always > 0 */
							r = try_make_path (sig, p, nth ? 0 : trn->speed);
							if (r < 0) break;
							if (r > 0) {
								sig->d->mode --;
								try_build_path (sig->d);
								trn->oflg ++;
								break;
							}
							p = q + 1;
							if (*p == '=') {
								p ++;
								if (trn->speed > 0) break;
								if (trn->ztim < 100) break;
							} else if (*p == '+') {
								if (p [1] == '+') {
									if (trn->speed * 36 > 160 * 1000) break;
									p ++;
								}
								nth = 0;
								p ++;
							}
						} else {
							r = try_make_path (sig, p, nth ? 0 : trn->speed);
							if (r < 0) break;
							if (r > 0) {
								sig->d->mode --;
								try_build_path (sig->d);
								trn->oflg ++;
								break;
							}
							p = q;
						}
					}
				} else {
					r = try_make_path (sig, n, trn->speed);
					if (r > 0) {
						sig->d->mode --;
						try_build_path (sig->d);
						trn->oflg ++;
					}
				}
			}
			/* return dist; replaced by try_open logic */
		}
		if (sig && !sig->d->state) {
			if (firstsigp /* && !*firstsigp */) {
				*firstsigp = sig;
			}
			return dist;
		}
		if (sig) {
			if (opensigs) (*opensigs) ++;
			strcpy (sigs, sig->name ? sig->name : "none");
			while (*sigs) sigs ++;
			*sigs ++ = '.';
			*sigs = '?';
			sigs [1] = 0;
#if 0
			if (firstsigp && !*firstsigp) {
				*firstsigp = sig;
			}
#endif
		}
		if (tk == c->a || tk == c->c) {
			/* Forward connector */
			tk = c->d_nb ? c->d : c->b;
		} else if (tk == c->b || tk == c->d) {
			/* Backward connector */
			tk = c->c_na ? c->c : c->a;
		} else {
			panic ("Connector failure");
			return dist;
		}
		if (!tk) {
			strcpy (sigs, "$");
			if (dist < 1) return -1;
			return dist;
		}
		/* Speed on next track */
		if (eqdist && tk->maxsp) {
			int eq = (tk->maxsp / 10) * (tk->maxsp / 100) + dist;
			if (*eqdist > eq) *eqdist = eq;
		}
		/* Then go into next track */
		if (c == tk->r) {
			/* Scan forward */
			if (tk->t) {
				strcpy (sigs, "%");
				check (tk->t->pos >= 0);
				check (tk->t->rev == 1);
				return dist + tk->t->pos;
			}
			dist += tk->len;
			c = tk->f;
		} else if (c == tk->f) {
			/* Scan backward */
			if (tk->t) {
				strcpy (sigs, "%");
				for (pr = tk->t; pr->next; pr = pr->next);
				check (pr->pos <= tk->len);
				check (pr->rev == 0);
				return dist + tk->len - pr->pos;
			}
			dist += tk->len;
			c = tk->r;
		} else {
			panic ("Track failure");
		}
	}
	return dist;
}

struct pln {
	char *pl;
	int len;
	int v;
	char *n;
} plans [300] = {
	{ "2N3-31,A-S3-21,4P5-1,1A-1N2" },
	{ "2N3-3B-3S2,4S24-4N5/4S34,4N5-1F/4N4,2A-2N3" },
	{ "2N3-31,4S24-4N5/4S34,4N5-1F/4N4,2A-2N3" },
	{ "2N3-31,4G-4R3,4R3-4P1,4C-4S2/4S3,4N3-A,4S3-4N4" },
	{ "2N3-31,4G-4R3,4A-4S2/4S3/4S24,4S3-4N4/4N3,4N3-B/A,B-N4,1A-1X3" },
	{ "2N3-31,4G-4R3,4R3-4P1,4C-4S34" },
	{ "2N3-31,4G-4R3,4R3-4P2/4P1,4P2-11,4C-4S34" },
	{ "2N3-31,4G-4R3,4A-4S2/4S3/4S24,4N3-A,4S3-4N4" },
	{ 0 }
};

void setup_train (void) {
	struct train *trn;
	static int cnt = 0;
	static struct pln *nextplan = plans;
	int l;
	if (!nextplan->pl) return;		/* XXX */
	if (!startconn) {
		return;
	}
	if (!startconn->b) {
		return;
	}
	if (startconn->b->t) {
		return;	/* Already something in here */
	}
	if (cnt ++ < 50) return;
	cnt = 0;
	trn = malloc (sizeof (struct train));
	if (!trn) return;
	trn->oflg = 0;
	trn->dir = 0;
	trn->next = trlist;
	trn->speed = 0;
	trn->accto = 0;
	trn->ztim = 0;
	trlist = trn;

	l = nextplan->len;
if (iDebugLevel > 2) printf ("LENGTH %d\n", l);
	if (l < 25000 || l > 750000) {
		l = 200000;
	}
if (iDebugLevel > 2) printf ("LENGTH %d\n", l);

	trn->ends [0].trn = trn;
	trn->ends [0].next = 0;
	trn->ends [0].on = startconn->b;
	trn->ends [0].pos = l + 10000;
	trn->ends [0].rev = 0;

	trn->ends [1].trn = trn;
	trn->ends [1].next = &trn->ends [0];
	trn->ends [1].on = startconn->b;
	trn->ends [1].pos = 10000;
	trn->ends [1].rev = 1;

	trn->dir = 0;
	trn->len = l;

	initplan (&trn->plan, nextplan->pl);
	trn->maxspeed = nextplan->v;
	trn->name = nextplan->n;
	if (nextplan->pl) nextplan ++;
	trn->ppos = 0;

	startconn->b->t = &trn->ends [1];
	startconn->b->x->occcnt ++;

	trn->sigdelay = 0;

	trn->dwin = XCreateSimpleWindow (mydisplay, drawwindow,
					 0, 0,
					 5, 5, 1, 
					 myforeground,
					 spaceground);
    	XSelectInput (mydisplay, trn->dwin,
		      ButtonPressMask|ButtonReleaseMask|Button1MotionMask|KeyPressMask|ExposureMask|StructureNotifyMask);
    	XDefineCursor(mydisplay,trn->dwin,mydotcurs);

	strcpy (trn->dtxt, "");
	trn->dsig = 0;

	DrawDIllum (startconn->b->x);
	DrawSmallDNode (startconn->b->x);
}

void train_ddraw (struct train *trn) {
	    XClearArea (mydisplay, trn->dwin, 0, 0, 0, 0, False);
	    XDrawString (mydisplay, trn->dwin, mygc, 1, font_height + 1,
			 trn->dtxt, strlen (trn->dtxt));
}

void stash (int f);

struct train *followtrain = 0;

void set_followtrain (struct train *trn) {
	if (followtrain != trn) {
		if (followtrain) {
			XSetWindowBackground (mydisplay,
				followtrain->dwin, spaceground);
			train_ddraw (followtrain);
		}
		followtrain = trn;
		if (followtrain) {
			XSetWindowBackground (mydisplay,
				trn->dwin, markground);
			train_ddraw (trn);
		}
	} else {
		if (trn) {
			XSetWindowBackground (mydisplay,
				trn->dwin, spaceground);
			train_ddraw (trn);
		}
		followtrain = 0;
	}
}

void train_dwin (struct train *trn, struct sig *sig) {
	int h, w;
	int x, y;
	int ry;
	char buf [100];
	if (trn->dsig) trn->dsig->dsigcnt --;
	trn->dsig = sig;
	if (trn->dsig) trn->dsig->dsigcnt ++;
	if (sig == 0) {
		XUnmapWindow (mydisplay, trn->dwin);
		return;
	}
	sprintf (buf,
		 "%.30s %3d", trn->name,
		 (36 * trn->speed + 5000) / 10000);
	h = font_height + font_depth + 1;
	w = 2 + XTextWidth (myfontstruct, buf, strlen (buf));
	x = XCenter (sig->d->cent.x);
	y = YCenter (sig->d->cent.y);
	ry = y;
	if (IsSameDir (Dir_W, sig->d->sigdir)) {
		x -= w;
		y -= h + 2;
	} else {
		y += 1;
	}
	XMoveResizeWindow (mydisplay, trn->dwin, x, y, w, h);
	XMapWindow (mydisplay, trn->dwin);
	if (strcmp (buf, trn->dtxt)) {
		strcpy (trn->dtxt, buf);

		if (ry < ypos - 4 * FieldYSize ||
		    ry > ypos + 4 * FieldYSize + ysize) {
			/* Out of sight */
		} else {
			train_ddraw (trn);
		}
	}
	if (trn == followtrain) {
		if (ypos - 15 * FieldYSize > ry
		 || ry > ypos + ysize + 15 * FieldYSize) {
			ypos = y - ysize / 2;
			stash (1);
		} else if (ypos + 5 * FieldYSize > ry) {
			ypos = ry - 12 * FieldYSize;
			stash (1);
		} else if (ry > ypos + ysize - 5 * FieldYSize) {
			ypos = y - ysize + 12 * FieldYSize;
			stash (1);
		}
	}
}

void train_event (struct train *trn, XEvent myevent) {
    /* struct dnode *dp; */
    {
	KeySym mykey;
	char text[10];
	int i;
	switch(myevent.type) {
	  case KeyPress:
	    i=XLookupString(&myevent,text,10,&mykey,0);
	    if(i==1) switch(text[0]) {
	      case ' ':
		set_followtrain (trn);
		break;
	      default:
	    } else {
	      switch (mykey) {
		case XK_R9:
		case XK_Prior:
		  break;
		case XK_R15:
		case XK_Next:
		  break;
		case XK_R14:
		case XK_Down:
		  break;
		case XK_R8:
		case XK_Up:
		  break;
	      }
	    }
	  case Expose:
	    train_ddraw (trn);
	    break; }}}

struct sig *MkSig (struct conn *c, char *name, struct dnode *x) {
	struct sig *s = malloc (sizeof (struct sig));
	s->c = c;
	s->name = name;
	s->d = x;
	s->lcks = 0;
	s->pre = 0;
	s->sp = 0;
	s->im = 0;
	s->grp = 0;
	s->qgrp = 0;
	s->dsigcnt = 0;
	s->maxsp = 0;
	s->cnt = 0;
	return s;
}

struct trk *MkTrk (int m, struct conn *a, struct conn *b, struct dnode *x) {
	struct trk *t = malloc (sizeof (struct trk));
	t->len = m > 0 ? 1000 * m : 1;
	t->maxsp = 0;
	t->f = b;
	t->r = a;
	t->use = 0;
	t->t = 0;
	t->x = x;
	return t;
}

struct conn *GetConn (char *name, struct conn *d) {
	struct conn *c;
	static struct hcn {
		struct hcn *next;
		struct conn *c;
		char *name;
	} *hcroot = 0;
	struct hcn **hp, *h;
	if (name) {
		struct sig *hs;
		struct conn *hc;
		for (hp = &hcroot; (h = *hp); hp = &h->next) {
			if (!strcmp (h->name, name)) {
				c = h->c;
if (iDebugLevel > 2) printf ("Reverse conn %s\n", name);
				/* Found, have to reverse it */
				/* Second end not yet used... */
				/* Reverse only when a is free (and not b) */
				if (!c->a && !c->c) {
					check (!c->a && !c->c && !c->actl);
					c->a = c->b; c->b = 0;
					c->c = c->d; c->d = 0;
					hs = c->sr; c->sr = c->sf; c->sf = hs;
					c->c_na = c->d_nb; c->d_nb = 0;
					c->actl = c->bctl; c->bctl = 0;
					hc = c->anxt; c->anxt = c->bnxt; c->bnxt = hc;
				} else {
					check (!c->b && !c->d && !c->bctl);
				}
				*hp = h->next;
				free (h);
				return c;
			}
		}
		h = malloc (sizeof (struct hcn));
		h->next = *hp;
		h->name = name;
		*hp = h;
		if (d) {
if (iDebugLevel > 2) printf ("Forward default %s\n", name);
			h->c = d;
			return d;
		}
if (iDebugLevel > 2) printf ("Forward conn %s\n", name);
	} else {
		h = 0;
	}
	c = malloc (sizeof (struct conn));
	c->a = 0; c->b = 0; c->c = 0; c->d = 0;
	c->sf = 0; c->sr = 0;
	c->c_na = 0; c->d_nb = 0;
	c->bctl = 0; c->actl = 0;
	c->bnxt = c; c->anxt = c;
	if (h) h->c = c;
if (!startconn) startconn = c;
	return c;
}

struct coord dirtab [8] = {
	{ 1, 0 },
	{ 1, 1 },
	{ 0, 1 },
	{ -1, 1 },
	{ -1, 0 },
	{ -1, -1 },
	{ 0, -1 },
	{ 1, -1 }
};

void advpos (struct coord *p, int u, int d) {
	p->x += u * dirtab [DirCode (d)].x;
	p->y += u * dirtab [DirCode (d)].y;
}

struct dnode *dnode_list = 0;

struct dnode *mkdnode (int t) {
	struct dnode *x = malloc (sizeof (struct dnode));
	x->type = t;
	x->tnext = dnode_list;
	x->state = 0;
	x->propstate = 0;
	x->hps = 0;
	x->vrs = 0;
	x->lock = 0;
	x->occcnt = 0;
	x->mode = 1 << 31;
	x->swtch = 0;
	x->swtcb = 0;
	x->sig = 0;
	x->moff = minioff;
	dnode_list = x;
	return x;
}

struct coord currpos = { 10, 10 };
int max_ypos = 10;

int currdir = 0;

struct conn *currconn = 0;

#define ErrDiag(t) return errdiag (p, s, t)

int errdiag (char *p, char *s, char *t) {
	printf ("Error \"%s\" in: ", t);
	while (*p) {
		if (p == s) {
			printf ("!");
		}
		printf ("%c", *p);
		p ++;
	}
	return 1;
}

int nplans = 0;

struct macdef {
	struct macdef *next;
	char *val;
	char name [1];
} *macs = 0;

static int ismacchar (int c) {
	return (c >= 'A' && c <= 'Z') ||
	       (c >= 'a' && c <= 'z') ||
	       (c >= '0' && c <= '9') ||
	       c == '-';
}

static char *asdf (char *x, char *prep) {
	char *res = 0;
	char *cur;
	int nl = strlen (x) + 1;
	if (prep) nl += strlen (prep) + 1;
	while (1) {
		char *dest = res;
		if (dest && prep) {
			char *q = prep;
			while (*q) *dest ++ = *q ++;
			*dest ++ = ',';
			free (prep);
			prep = 0;
		}
		cur = x;
		while (*cur) {
			if (*cur == '%') {
				char *b = ++ cur;
				struct macdef *mac;
				while (ismacchar (*cur)) cur++;
				for (mac = macs; mac; mac = mac->next) {
					if (strncmp (mac->name, b, cur - b) == 0 &&
					    mac->name [cur - b] == 0) {
						break;
					}
				}
				if (mac) {
					if (dest) {
						char *q = mac->val;
						while (*q) *dest ++ = *q ++;
					} else {
						nl += strlen (mac->val);
					}
				} else if (dest) {
					printf ("Undefined %.*s in %s\n",
						cur - b, b, x);
				}
			} else if (res) {
				*dest ++ = *cur ++;
			} else {
				cur ++;
			}
		}
		if (res) {
			char *q;
			*dest = 0;
			for (q = res; *q; q ++) {
				if (*q == '\n') {
					*q = 0;
					break;
				}
			}
			return res;
		}
		res = malloc (nl);
		*res = 0;	/* Check malloc */
	}
}

int gettlen (char **pp) {
	int l;
	char *p = *pp;
	char *q;
if (iDebugLevel > 2) printf ("gettlen %s", p);
	if (strncmp (p, "len=", 4)) return 0;
	l = strtol (p + 4, &q, 10);
	if (q == p + 4) return 0;
	if (l && (l < 25 || l > 750)) l = 25;
	while (*q == ' ') q ++;
	*pp = q;
	return l * 1000;
}

int gettspd (char **pp) {
	int l;
	char *p = *pp;
	char *q;
if (iDebugLevel > 2) printf ("gettspd %s", p);
	if (strncmp (p, "v=", 2)) return 0;
	l = strtol (p + 2, &q, 10);
	if (q == p + 4) return 0;
	if (l < 40 || l > 200) l = 160;
	while (*q == ' ') q ++;
	*pp = q;
	return KMH2MMS (l);
}

char *gettnam (char **pp) {
	char *p = *pp;
	char *q, *b;
if (iDebugLevel > 2) printf ("gettnam %s", p);
	if (strncmp (p, "n=", 2)) return 0;
	b = p + 2;
	q = b;
	while (*q && *q != ' ') q ++;
	p = q;
	while (*q == ' ') q ++;
	*pp = q;
	*p = 0;
	return strdup (b);
}

int readgpfile (char *p);

int isquo (char **pp) {
	char *p = *pp;
	int n;
	char *q;
	if (strncmp (p, "%quo", 4)) return 0;
	p += 4;
	n = strtol (p, &q, 10);
	if (q == p) return 0;
	if (n <= 0) return 0;
	*pp = q;
	return n;
}

int parsedef (char *p) {
	int num;
	if (strncmp (p, "%def ", 5) == 0) {
		defplan.plan = asdf (p + 5, defplan.plan);
	} else if (strncmp (p, "%inc ", 5) == 0) {
		char *q;
		p += 5;
		while (*p == ' ') p ++;
		q = p;
		while (*p && *p != '\n' && *p != ' ') p ++;
		*p = 0;
		return readgpfile (q);
	} else if (strncmp (p, "%mac ", 5) == 0) {
		struct macdef *mac;
		char *q = (p += 5);
		int l;
		while (ismacchar (*q)) q ++;
		l = q - p;
		if (l < 1 || *q != ' ') return 1;
		q ++;
		for (mac = macs; mac; mac = mac->next) {
			if (strncmp (p, mac->name, l) == 0 &&
			    mac->name [l] == 0) {
				break;
			}
		}
		if (!mac) {
			mac = malloc (sizeof (struct macdef) + (q - p));
			mac->next = macs;
			macs = mac;
			strncpy (mac->name, p, l);
			mac->name [l] = 0;
			mac->val = 0;
		}
		if (iDebugLevel > 2) printf ("Raw:: %s as %s\n", mac->name, q);
		mac->val = asdf (q, mac->val);
		if (iDebugLevel > 2) printf ("Redef %s as %s\n", mac->name, mac->val);
	} else if (strncmp (p, "%exc ", 5) == 0) {
		char *q;
		struct dnode *dn;
		struct ilg *ip = 0;
		struct ilg *ii;
		p += 5;
		while (*p) {
if (iDebugLevel > 2) printf ("Exc: %s", p);
			for (q = p; *q && *q != ' ' && *q != '\n'; q ++);
if (iDebugLevel > 2) printf ("Exc: %s", p);
			for (dn = dnode_list; dn; dn = dn->tnext) {
				if (!dn->sig) continue;
				if (strlen (dn->sig->name) + p != q) continue;
				if (strncmp (dn->sig->name, p, q - p)) continue;
				if (dn->sig->grp) {
					printf ("\007Sig already has a group...\n");
					continue;
				}
AT;
				ii = malloc (sizeof (struct ilg));
				ii->sig = dn->sig;
				ii->next = ip;
				ip = ii;
			}
AT;
			while (*q == ' ' || *q == '\n') q ++;
			p = q;
		}
if (iDebugLevel > 2) printf ("3\n");
		for (ii = ip; ii; ii = ii->next) {
			ii->sig->grp = ip;
		}
if (iDebugLevel > 2) printf ("4\n");
	} else if ((num = isquo (&p)) > 0) {
		char *q;
		struct dnode *dn;
		struct qlg *ip = 0;
		struct qlg *ii;
		while (*p) {
			int u = 0;
if (iDebugLevel > 2) printf ("Exc: %s", p);
			if (*p == '*') {
				u = 1;
				p ++;
			}
			for (q = p; *q && *q != ' ' && *q != '\n'; q ++);
if (iDebugLevel > 2) printf ("Exc: %s", p);
			ii = 0;
			if (q > p) {
				for (dn = dnode_list; dn; dn = dn->tnext) {
					if (!dn->sig) continue;
					if (strlen (dn->sig->name) + p != q) continue;
					if (strncmp (dn->sig->name, p, q - p)) continue;
	AT;
					ii = malloc (sizeof (struct qlg));
					ii->sig = dn->sig;
					ii->next = ip;
					ii->unchecked = u;
if (iDebugLevel > 4) if (u) printf ("Unchecked signal %.*s for quote\n", q - p, p);
					ip = ii;
				}
				if (ii == 0) {
					printf ("Not found signal %.*s for quote\n", q - p, p);
				}
			}
AT;
			while (*q == ' ' || *q == '\n') q ++;
			p = q;
		}
if (iDebugLevel > 2) printf ("3\n");
		for (ii = ip; ii; ii = ii->next) {
			struct qlgm *im = malloc (sizeof (struct qlgm));
			im->next = ii->sig->qgrp;
			ii->sig->qgrp = im;
			im->grp = ip;
			im->sig = ii->sig;
			im->num = num;
		}
if (iDebugLevel > 2) printf ("4\n");
	} else if (strncmp (p, "%trn ", 5) == 0) {
		int l;
		int v;
		p += 5;
		l = gettlen (&p);
		v = gettspd (&p);
		if (v == 0) v = 55555;
		if ((nplans + 2) * sizeof (plans [0]) >= sizeof (plans)) {
			printf ("Too many plans\n");
			return 1;
		}
		plans [nplans].len = l;
		plans [nplans].v = v;
		plans [nplans].n = gettnam (&p);
		plans [nplans ++].pl = asdf (p, 0);
		plans [nplans].pl = 0;
	} else if (strncmp (p, "%nomini", 7) == 0) {
		minioff = &defmoff;
	} else if (strncmp (p, "%mini ", 6) == 0) {
		char *q;
		int x, y;
		p += 6;
		x = strtol (p, &q, 10);
		if (q == p) return 1;
		p = q;
		y = strtol (p, &q, 10);
		if (q == p) return 1;
		minioff = malloc (sizeof (struct moff));
		minioff->x = x;
		minioff->y = y;
		minioff->r = (*q == '-');
	} else {
		return 1;
	}
	return 0;
}

int parseline (char *p) {
	struct coord np;
	struct dnode *x;
	char *s = p;
	struct {
		int flags;
		int tok;
		int u, m, d, s;
		char *name;
	} A [4];
	int v;
	char *cv;
	int z;
	int u, m, t, i;
	int d1, d2, d3;
	struct conn *c, *c2, *c3, *c1;
	struct trk *tk;
	struct sig *sg;
	if (*p == '%') {
		return parsedef (p);
	}
	for (z = 0; z < 4; z ++) {
		A [z].flags = 0;
		A [z].name = 0;
		while ((t = gettok (&s, &v, &cv)) != Eoln) {
			switch (t) {
			case Name:
				if (A [z].name) {
					ErrDiag ("Double name");
				}
				A [z].name = cv;
				continue;
			case Unit:
				if (A [z].flags & Useen) {
					ErrDiag ("Double unit");
				}
				A [z].flags |= Useen;
				A [z].u = v;
				continue;
			case Speed:
				if (A [z].flags & Sseen) {
					ErrDiag ("Double speed");
				}
				A [z].flags |= Sseen;
				A [z].s = KMH2MMS (v) / 10;
				continue;
			case Meter:
				if (A [z].flags & Mseen) {
					ErrDiag ("Double meter");
				}
				A [z].flags |= Mseen;
				A [z].m = v;
				continue;
			}
			if (IsDir (t)) {
				if (A [z].flags & Dseen) {
					ErrDiag ("Double direction");
				}
				A [z].flags |= Dseen;
				A [z].d = t;
				continue;
			}
			break;
		}
		A [z].tok = t;
		if (t == Eoln) {
			z ++;
			break;
		}
		switch (z) {
		case 0:
			if (t & (Switch | Signal | CtlTwo)) {
				continue;
			}
			if (t == Curve) {
				continue;
			}
			ErrDiag ("Bad item code");
		case 1:
		case 2:
			if (t == Other) {
				continue;
			}
		case 3:
			ErrDiag ("Bad item code");
		}
	}
	/* Now we have the line parsed, check attributes and do action */
	
	switch (z) {
	case 1:
		if (A [0].flags == 0 && A [0].name == 0) {
			/* Empty line, do nothing */
			return 0;
		}
		/* Make straight track */
		if (A [0].flags & Useen) {
			u = A [0].u;
		} else {
			u = 4;
		}
		if (A [0].flags & Mseen) {
			m = A [0].m;
		} else {
			if (A [0].flags == Dseen) {
				/* XXX Hack: Just change curdir */
				currdir = A [0].d;
				return 0;
			}
			ErrDiag ("No meter for straight");
		}
		if (A [0].name) {
			ErrDiag ("Name for straight");
		}
		if (A [0].flags & Dseen) {
			if (!IsCompatDir (currdir, A [0].d)) {
				ErrDiag ("Bad direction for straight");
			}
			currdir = A [0].d;
		}
		if (!currconn) {
			ErrDiag ("No current connector");
		}
		x = mkdnode (Straight);
		x->cent = currpos;
		advpos (&currpos, u, currdir);
		x->one = currpos;
		c = GetConn (0, 0);
		tk = MkTrk (m, currconn, c, x);
		if (A [0].flags & Sseen) {
			tk->maxsp = A [0].s;
		}
		currconn->b = tk;
		c->a = tk;
		currconn = c;
		return 0;
	case 2:
		if (A [0].tok == Crossing) {
			if ((A [0].flags | A [1].flags) & (Dseen | Mseen | Sseen)) {
				ErrDiag ("Only names allowed on crossing");
			}
			if (!A [0].name || !A [1].name) {
				ErrDiag ("Names required on crossing");
			}
			c = GetConn (A [0].name, 0);
			c2 = GetConn (A [1].name, 0);
			c1 = GetConn (A [0].name, 0);
			c3 = GetConn (A [1].name, 0);

			x = mkdnode (Nothing);

			tk = MkTrk (1, c, c1, x);
			c->b = tk;
			c1->b = tk;
			tk = MkTrk (1, c2, c3, x);
			c2->b = tk;
			c3->b = tk;
			return 0;
		}
		if (A [0].tok == Curve) {
			if (!currconn) {
				ErrDiag ("no circuit");
			}
			if ((A [0].flags ^ ~A [1].flags) & Mseen) {
				ErrDiag ("Exactly one length needed for curve");
			}
			if (A [0].flags & A [1].flags & Sseen) {
				ErrDiag ("At most one speed needed for curve");
			}
			if (A [0].flags & Dseen) {
				d1 = A [0].d;
			} else {
				d1 = currdir;
			}
			if (A [1].flags & Dseen) {
				d2 = A [1].d;
			} else {
				d2 = d1;
			}
			if (IsSameDir (d1, d2)) {
				ErrDiag ("Uncurved curve");
			}
			if (!IsCompatDir (d1, d2)) {
				ErrDiag ("Bend too sharp");
			}
			if (A [0].name || A [1].name) {
				ErrDiag ("Name on curve");
			}
			for (i = 0; i < 2; i ++) {
				if (!(A [i].flags & Useen)) {
					A [i].u = 2;
				}
			}
			x = mkdnode (Curve);
			x->one = currpos;
			advpos (&currpos, A [0].u, d1);
			x->cent = currpos;
			currdir = d2;
			advpos (&currpos, A [1].u, currdir);
			x->two = currpos;

		if (A [0].flags & Mseen) {
			m = A [0].m;
		} else {
			m = A [1].m;
		}
		c = GetConn (0, 0);
		tk = MkTrk (m, currconn, c, x);
		if (A [0].flags & Sseen) {
			tk->maxsp = A [0].s;
		}
		currconn->b = tk;
		c->a = tk;
		currconn = c;
		return 0;
#if 0
			...

			if (!A [2].name) {
				char buf [40];
				sprintf (buf, "%d\"%d", np.x, np.y);
				A [2].name = strdup (buf);
			}
			c = GetConn (A [2].name, 0);
			tk = MkTrk (A [2].m, currconn, c, x);
			if (A [2].flags & Sseen) {
				tk->maxsp = A [2].s;
			} else {
				tk->maxsp = KMH2MMS (60);
			}
			currconn->d = tk;
			c->b = tk;
			c = GetConn (0, 0);
			tk = MkTrk (A [1].m, currconn, c, x);
			if (A [1].flags & Sseen) {
				tk->maxsp = A [1].s;
			}
			currconn->b = tk;
			c->a = tk;
			currconn->bctl = x;
			x->swtch = currconn;
			currconn = c;
			return 0;
#endif
		}
		if (A [0].tok & Signal) {
			if (!currconn) {
				ErrDiag ("no circuit");
			}
			if (A [0].flags || (A [1].flags & ~Sseen)) {
				ErrDiag ("Signal needs no attr");
			}
			if (A [0].name) {
				ErrDiag ("Name before signal");
			}
			if (IsGoodSigDir (currdir)) {
				x = mkdnode (A [0].tok & (HpSig | LsSig));
				np = currpos;
				if (A [0].tok & HpSig) {
					advpos (&currpos, 1, currdir);
				}
				sg = MkSig (currconn, A [1].name,
					    x);
				if (A [1].flags & Sseen) {
					sg->maxsp = A [1].s;
				}
				if (A [0].tok & Reverse) {
					x->cent = currpos;
					x->one = np;
					if (currconn->sr) {
						ErrDiag ("Already a signal there");
					}
					currconn->sr = sg;
					x->sig = sg;
					x->sigdir = ReverseDir (currdir);
				} else {
					x->cent = np;
					x->one = currpos;
					if (currconn->sf) {
						ErrDiag ("Already a signal there");
					}
					currconn->sf = sg;
					x->sig = sg;
					x->sigdir = currdir;
				}
				return 0;
			} else {
				ErrDiag ("Bad direction for signal");
			}
		}
		if (A [0].tok == Begin) {
			if (currconn) {
				ErrDiag ("open circuit on begin");
			}
			if (A [0].flags != Useen ||
			    (A [1].flags & ~Dseen) != Useen) {
				ErrDiag ("Begin needs only units");
			}
			if (A [0].name) {
				ErrDiag ("Name before begin");
			}
			c = GetConn (A [1].name, 0);
			if (!c) {
				ErrDiag ("Named conn already done?");
			}
			currpos.x = A [0].u;
			currpos.y = A [1].u;
			if (A [1].flags & Dseen) {
				currdir = A [1].d;
			} else {
				currdir = Dir_E;
			}
			currconn = c;
			return 0;
		}
		if (A [0].tok == End) {
			if (!currconn) {
				ErrDiag ("no circuit on end");
			}
			if (A [0].flags || A [1].flags) {
				ErrDiag ("End needs only name");
			}
			if (A [0].name) {
				ErrDiag ("Name before end");
			}
			if (!A [1].name) {
				/* Simpler case, just drop */
				currconn = 0;
				return 0;
			}
			c = GetConn (A [1].name, currconn);
			if (c != currconn) {
				/* Second one, plumb */
				tk = MkTrk (0, currconn, c, 0);
				currconn->b = tk;
				c->b = tk;
			}
			currconn = 0;
			return 0;
		}
		if (A [0].tok == EndStop) {
			if (!currconn) {
				ErrDiag ("no circuit on end");
			}
			if (A [0].flags || A [1].flags || !A [1].name) {
				ErrDiag ("End needs only name");
			}
			if (A [0].name) {
				ErrDiag ("Name before end");
			}
			if (IsGoodSigDir (currdir)) {
				x = mkdnode (HpSig | Stop);
				np = currpos;
				if (A [0].tok & HpSig) {
					advpos (&currpos, 1, currdir);
				}
				sg = MkSig (currconn, A [1].name, x);
				x->cent = currpos;
				x->one = np;
				if (currconn->sr) {
					ErrDiag ("Already a signal there");
				}
				currconn->sr = sg;
				x->sig = sg;
				x->sigdir = ReverseDir (currdir);
			} else {
				ErrDiag ("Bad direction for stop");
			}
			currconn = 0;
			return 0;
		}
		ErrDiag ("Syntax");
	case 3:
		if (A [0].tok == BranchSwitch && A [1].tok == Other) {
			if (!currconn) {
				ErrDiag ("no circuit");
			}
			if (A [0].flags & (Mseen | Sseen)) {
				ErrDiag ("No length/speed allowed on joint end");
			}
			if (A [0].flags & Dseen) {
				ErrDiag ("Direction on joint end");
			}
			if (!(A [1].flags & Mseen)) {
				ErrDiag ("Length required on split end");
			}
			if (!(A [2].flags & Mseen)) {
				ErrDiag ("Length required on split end");
			}
			if (A [1].flags & Dseen) {
				d1 = A [1].d;
			} else {
				d1 = currdir;
			}
			if (A [2].flags & Dseen) {
				d2 = A [2].d;
			} else {
				d2 = currdir;
			}
			if (IsSameDir (d1, d2)) {
				ErrDiag ("Same direction both branches");
			}
			if (!IsCompatDir (d1, currdir)) {
				ErrDiag ("Bad direction main branch");
			}
			if (!IsCompatDir (d2, currdir)) {
				ErrDiag ("Bad direction side branch");
			}
			if (A [0].name || A [1].name) {
				ErrDiag ("Name on switch");
			}
			for (i = 0; i < 3; i ++) {
				if (!(A [i].flags & Useen)) {
					A [i].u = 2;
				}
			}
			x = mkdnode (Switch);
			x->one = currpos;
			advpos (&currpos, A [0].u, currdir);
			x->cent = currpos;
			np = currpos;
			currdir = d1;
			advpos (&currpos, A [1].u, currdir);
			advpos (&np, A [2].u, d2);
			x->two = currpos;
			x->three = np;

			if (!A [2].name) {
				char buf [40];
				sprintf (buf, "%d\"%d", np.x, np.y);
				A [2].name = strdup (buf);
			}
			c = GetConn (A [2].name, 0);
			tk = MkTrk (A [2].m, currconn, c, x);
			if (A [2].flags & Sseen) {
				tk->maxsp = A [2].s;
			} else {
				tk->maxsp = KMH2MMS (60);
			}
			currconn->d = tk;
			c->b = tk;
			c = GetConn (0, 0);
			tk = MkTrk (A [1].m, currconn, c, x);
			if (A [1].flags & Sseen) {
				tk->maxsp = A [1].s;
			}
			currconn->b = tk;
			c->a = tk;
			currconn->bctl = x;
			x->swtch = currconn;
			currconn = c;
			return 0;
		}
		if (A [0].tok == JoinSwitch && A [1].tok == Other) {
			if (!currconn) {
				ErrDiag ("no circuit");
			}
			if (A [1].flags & (Mseen | Sseen)) {
				ErrDiag ("No length allowed on joined end");
			}
			if (A [0].flags & Dseen) {
				ErrDiag ("Direction on incoming end");
			}
			if (!(A [0].flags & Mseen)) {
				ErrDiag ("Length required on incoming end");
			}
			if (!(A [2].flags & Mseen)) {
				ErrDiag ("Length required on split end");
			}
			if (A [1].flags & Dseen) {
				d1 = A [1].d;	/* Joined exit */
			} else {
				d1 = currdir;
			}
			if (A [2].flags & Dseen) {
				d2 = A [2].d;
			} else {
				d2 = currdir;
			}
			if (IsSameDir (currdir, d2)) {
				ErrDiag ("Same direction both branches");
			}
			if (!IsCompatDir (d1, currdir)) {
				ErrDiag ("Bad direction main branch");
			}
			if (!IsCompatDir (d2, ReverseDir (d1))) {
				ErrDiag ("Bad direction side branch");
			}
			if (A [0].name || A [1].name) {
				ErrDiag ("Name on switch");
			}
			for (i = 0; i < 3; i ++) {
				if (!(A [i].flags & Useen)) {
					A [i].u = 2;
				}
			}
			x = mkdnode (Switch);
			x->two = currpos;
			advpos (&currpos, A [0].u, currdir);
			x->cent = currpos;
			np = currpos;
			currdir = d1;
			advpos (&currpos, A [1].u, currdir);
			advpos (&np, A [2].u, d2);
			x->one = currpos;
			x->three = np;

			if (!A [2].name) {
				char buf [40];
				sprintf (buf, "%d\"%d", np.x, np.y);
				A [2].name = strdup (buf);
			}
			c = GetConn (0, 0);
			tk = MkTrk (A [0].m, currconn, c, x);
			if (A [0].flags & Sseen) {
				tk->maxsp = A [0].s;
			}
			currconn->b = tk;
			c->a = tk;
			currconn = c;
			c = GetConn (A [2].name, 0);
			tk = MkTrk (A [2].m, c, currconn, x);
			if (A [2].flags & Sseen) {
				tk->maxsp = A [2].s;
			} else {
				tk->maxsp = KMH2MMS (60);
			}
			currconn->c = tk;
			c->b = tk;
			currconn->actl = x;
			x->swtch = currconn;
			return 0;
		}
		ErrDiag ("Syntax");
	case 4:
		if (A [0].tok == FullCrossSwitch && A [1].tok == Other && A [2].tok == Other) {
			if (!currconn) {
				ErrDiag ("no circuit");
			}
			if (~(A [0].flags & A [1].flags & A [2].flags & A [3].flags) & Mseen) {
				ErrDiag ("Need lenghts on all ends");
			}
			if ((A [0].flags | A [1].flags | A [3].flags) & Sseen) {
				ErrDiag ("Speed for fwd branch only");
			}
			if (A [0].flags & Dseen) {
				ErrDiag ("Direction on incoming end");
			}
			if (A [1].flags & Dseen) {
				d1 = A [1].d;
			} else {
				d1 = currdir;
			}
			if (A [2].flags & Dseen) {
				d2 = A [2].d;
			} else {
				d2 = currdir;
			}
			if (A [3].flags & Dseen) {
				d3 = ReverseDir (A [3].d);
			} else {
				d3 = d2;
			}
			if (IsSameDir (d1, d2)) {
				ErrDiag ("Same direction both right branches");
			}
			if (IsSameDir (currdir, d3)) {
				ErrDiag ("Same direction both right branches");
			}
			if (!IsCompatDir (currdir, d1)) {
				ErrDiag ("Bad direction main/main");
			}
			if (!IsCompatDir (currdir, d2)) {
				ErrDiag ("Bad direction main/branch");
			}
			if (!IsCompatDir (d1, d3)) {
				ErrDiag ("Bad direction branch/main");
			}
			if (!IsCompatDir (d2, d3)) {
				ErrDiag ("Bad direction branch/branch");
			}
			d3 = ReverseDir (d3);
			if (A [0].name || A [1].name) {
				ErrDiag ("Name on cross switch");
			}
			for (i = 0; i < 4; i ++) {
				if (!(A [i].flags & Useen)) {
					A [i].u = 2;
				}
			}

			x = mkdnode (FullCross);
			x->one = currpos;
			advpos (&currpos, A [0].u, currdir);
			x->cent = currpos;
			x->three = currpos;
			advpos (&x->three, A [2].u, d2);
			x->four = currpos;
			advpos (&x->four, A [3].u, d3);
			currdir = d1;
			advpos (&currpos, A [1].u, currdir);
			x->two = currpos;

			if (!A [2].name) {
				char buf [40];
				sprintf (buf, "%d\"%d", x->three.x, x->three.y);
				A [2].name = strdup (buf);
			}
			if (!A [3].name) {
				char buf [40];
				sprintf (buf, "%d\"%d", x->four.x, x->four.y);
				A [3].name = strdup (buf);
			}

			if (strcmp (A [2].name, A [3].name) == 0) {
				ErrDiag ("Same names");
			}

			c = GetConn (0, 0);
			c2 = GetConn (A [2].name, 0);
			c3 = GetConn (A [3].name, 0);

			tk = MkTrk (A [0].m + A [1].m, currconn, c, x);
			currconn->b = tk;
			c->a = tk;

			tk = MkTrk (A [0].m + A [2].m, currconn, c2, x);
			currconn->d = tk;
			c2->b = tk;
			if (A [2].flags & Sseen) {
				tk->maxsp = A [2].s;
			} else {
				tk->maxsp = KMH2MMS (60);
			}

			tk = MkTrk (A [3].m + A [1].m, c3, c, x);
			c3->b = tk;
			c->c = tk;
			if (A [2].flags & Sseen) {
				tk->maxsp = A [2].s;
			} else {
				tk->maxsp = KMH2MMS (60);
			}

			tk = MkTrk (A [3].m + A [2].m, c3, c2, x);
			c3->d = tk;
			c2->d = tk;

			currconn->bctl = x;
			c->actl = x;
			c2->bctl = x;
			c3->bctl = x;

			currconn->bnxt = c3;
			c3->bnxt = currconn;
			c->anxt = c2;
			c2->bnxt = c;

			x->swtch = currconn;
			x->swtcb = c;

			currconn = c;
			return 0;
		}
		if (A [0].tok == FwdCrossSwitch && A [1].tok == Other && A [2].tok == Other) {
			if (!currconn) {
				ErrDiag ("no circuit");
			}
			if (~(A [0].flags & A [1].flags & A [2].flags & A [3].flags) & Mseen) {
				ErrDiag ("Need lenghts on all ends");
			}
			if ((A [0].flags | A [1].flags | A [3].flags) & Sseen) {
				ErrDiag ("Speed for fwd branch only");
			}
			if (A [0].flags & Dseen) {
				ErrDiag ("Direction on incoming end");
			}
			if (A [1].flags & Dseen) {
				d1 = A [1].d;
			} else {
				d1 = currdir;
			}
			if (A [2].flags & Dseen) {
				d2 = A [2].d;
			} else {
				d2 = currdir;
			}
			if (A [3].flags & Dseen) {
				d3 = ReverseDir (A [3].d);
			} else {
				d3 = d2;
			}
			if (IsSameDir (d1, d2)) {
				ErrDiag ("Same direction both right branches");
			}
			if (IsSameDir (currdir, d3)) {
				ErrDiag ("Same direction both left branches");
			}
			if (!IsCompatDir (currdir, d1)) {
				ErrDiag ("Bad direction main/main");
			}
			if (!IsCompatDir (currdir, d2)) {
				ErrDiag ("Bad direction main/branch");
			}
			if (!IsCompatDir (d2, d3)) {
				ErrDiag ("Bad direction branch/branch");
			}
			d3 = ReverseDir (d3);
			if (A [0].name || A [1].name) {
				ErrDiag ("Name on cross switch");
			}
			for (i = 0; i < 4; i ++) {
				if (!(A [i].flags & Useen)) {
					A [i].u = 2;
				}
			}

			x = mkdnode (HalfCross);
			x->one = currpos;
			advpos (&currpos, A [0].u, currdir);
			x->cent = currpos;
			x->three = currpos;
			advpos (&x->three, A [2].u, d2);
			x->four = currpos;
			advpos (&x->four, A [3].u, d3);
			currdir = d1;
			advpos (&currpos, A [1].u, currdir);
			x->two = currpos;

			if (!A [2].name) {
				char buf [40];
				sprintf (buf, "%d\"%d", x->three.x, x->three.y);
				A [2].name = strdup (buf);
			}
			if (!A [3].name) {
				char buf [40];
				sprintf (buf, "%d\"%d", x->four.x, x->four.y);
				A [3].name = strdup (buf);
			}

			if (strcmp (A [2].name, A [3].name) == 0) {
				ErrDiag ("Same names");
			}

			c = GetConn (0, 0);
			c2 = GetConn (A [2].name, 0);
			c3 = GetConn (A [3].name, 0);

			tk = MkTrk (A [0].m + A [1].m, currconn, c, x);
			currconn->b = tk;
			c->a = tk;

			tk = MkTrk (A [0].m + A [2].m, currconn, c2, x);
			currconn->d = tk;
			c2->b = tk;
			if (A [2].flags & Sseen) {
				tk->maxsp = A [2].s;
			} else {
				tk->maxsp = KMH2MMS (60);
			}

			tk = MkTrk (A [3].m + A [2].m, c3, c2, x);
			c3->b = tk;
			c2->d = tk;

			currconn->bctl = x;
			c2->bctl = x;

			x->swtch = currconn;
			x->swtcb = c2;

			currconn = c;
			return 0;
		}
		if (A [0].tok == CurveCrossSwitch && A [1].tok == Other && A [2].tok == Other) {
			if (!currconn) {
				ErrDiag ("no circuit");
			}
			if (~(A [0].flags & A [1].flags & A [2].flags & A [3].flags) & Mseen) {
				ErrDiag ("Need lenghts on all ends");
			}
			if ((A [0].flags | A [1].flags | A [3].flags) & Sseen) {
				ErrDiag ("Speed for fwd branch only");
			}
			if (A [0].flags & Dseen) {
				ErrDiag ("Direction on incoming end");
			}
			if (A [1].flags & Dseen) {
				d1 = A [1].d;
			} else {
				d1 = currdir;
			}
			if (A [2].flags & Dseen) {
				d2 = A [2].d;
			} else {
				d2 = currdir;
			}
			if (A [3].flags & Dseen) {
				d3 = ReverseDir (A [3].d);
			} else {
				d3 = d2;
			}
			if (IsSameDir (d1, d2)) {
				ErrDiag ("Same direction both right branches");
			}
			if (IsSameDir (currdir, d3)) {
				ErrDiag ("Same direction both left branches");
			}
			if (!IsCompatDir (currdir, d1)) {
				ErrDiag ("Bad direction main/main");
			}
			if (!IsCompatDir (currdir, d2)) {
				ErrDiag ("Bad direction main/branch");
			}
			if (!IsCompatDir (d1, d3)) {
				ErrDiag ("Bad direction branch/main");
			}
			d3 = ReverseDir (d3);
			if (A [0].name || A [1].name) {
				ErrDiag ("Name on cross switch");
			}
			for (i = 0; i < 4; i ++) {
				if (!(A [i].flags & Useen)) {
					A [i].u = 2;
				}
			}

			x = mkdnode (HalfCross);
			x->one = currpos;
			advpos (&currpos, A [0].u, currdir);
			x->cent = currpos;
			x->two = currpos;
			advpos (&x->two, A [2].u, d2);
			x->four = currpos;
			advpos (&x->four, A [3].u, d3);
			currdir = d1;
			advpos (&currpos, A [1].u, currdir);
			x->three = currpos;

			if (!A [2].name) {
				char buf [40];
				sprintf (buf, "%d\"%d", x->three.x, x->three.y);
				A [2].name = strdup (buf);
			}
			if (!A [3].name) {
				char buf [40];
				sprintf (buf, "%d\"%d", x->four.x, x->four.y);
				A [3].name = strdup (buf);
			}

			if (strcmp (A [2].name, A [3].name) == 0) {
				ErrDiag ("Same names");
			}

			c = GetConn (A [2].name, 0);
			c2 = GetConn (0, 0);
			c3 = GetConn (A [3].name, 0);

			tk = MkTrk (A [0].m + A [1].m, currconn, c, x);
			currconn->b = tk;
			c->b = tk;
			if (A [2].flags & Sseen) {
				tk->maxsp = A [2].s;
			} else {
				tk->maxsp = KMH2MMS (60);
			}

			tk = MkTrk (A [0].m + A [2].m, currconn, c2, x);
			currconn->d = tk;
			c2->a = tk;

			tk = MkTrk (A [3].m + A [2].m, c3, c2, x);
			c3->b = tk;
			c2->c = tk;
			if (A [2].flags & Sseen) {
				tk->maxsp = A [2].s;
			} else {
				tk->maxsp = KMH2MMS (60);
			}

			currconn->bctl = x;
			currconn->d_nb = 1;
			c2->actl = x;

			x->swtch = currconn;
			x->swtcb = c2;

			currconn = c2;
			return 0;
		}
		if (A [0].tok == RevCrossSwitch && A [1].tok == Other && A [2].tok == Other) {
			if (!currconn) {
				ErrDiag ("no circuit");
			}
			if (~(A [0].flags & A [1].flags & A [2].flags & A [3].flags) & Mseen) {
				ErrDiag ("Need lenghts on all ends");
			}
			if ((A [0].flags | A [1].flags | A [3].flags) & Sseen) {
				ErrDiag ("Speed for fwd branch only");
			}
			if (A [0].flags & Dseen) {
				ErrDiag ("Direction on incoming end");
			}
			if (A [1].flags & Dseen) {
				d1 = A [1].d;
			} else {
				d1 = currdir;
			}
			if (A [2].flags & Dseen) {
				d2 = A [2].d;
			} else {
				d2 = currdir;
			}
			if (A [3].flags & Dseen) {
				d3 = ReverseDir (A [3].d);
			} else {
				d3 = d2;
			}
			if (IsSameDir (d1, d2)) {
				ErrDiag ("Same direction both right branches");
			}
			if (IsSameDir (currdir, d3)) {
				ErrDiag ("Same direction both left branches");
			}
			if (!IsCompatDir (currdir, d1)) {
				ErrDiag ("Bad direction main/main");
			}
			if (!IsCompatDir (d1, d3)) {
				ErrDiag ("Bad direction branch/main");
			}
			if (!IsCompatDir (d2, d3)) {
				ErrDiag ("Bad direction branch/branch");
			}
			d3 = ReverseDir (d3);
			if (A [0].name || A [1].name) {
				ErrDiag ("Name on cross switch");
			}
			for (i = 0; i < 4; i ++) {
				if (!(A [i].flags & Useen)) {
					A [i].u = 2;
				}
			}

			x = mkdnode (HalfCross);
			x->four = currpos;
			advpos (&currpos, A [0].u, currdir);
			x->cent = currpos;
			x->two = currpos;
			advpos (&x->two, A [2].u, d2);
			x->one = currpos;
			advpos (&x->one, A [3].u, d3);
			currdir = d1;
			advpos (&currpos, A [1].u, currdir);
			x->three = currpos;

			if (!A [2].name) {
				char buf [40];
				sprintf (buf, "%d\"%d", x->three.x, x->three.y);
				A [2].name = strdup (buf);
			}
			if (!A [3].name) {
				char buf [40];
				sprintf (buf, "%d\"%d", x->four.x, x->four.y);
				A [3].name = strdup (buf);
			}

			if (strcmp (A [2].name, A [3].name) == 0) {
				ErrDiag ("Same names");
			}

			c = GetConn (A [2].name, 0);
			c2 = GetConn (0, 0);
			c3 = GetConn (A [3].name, 0);

			tk = MkTrk (A [0].m + A [1].m, c3, c, x);
			c3->b = tk;
			c->b = tk;

			tk = MkTrk (A [0].m + A [2].m, c3, c2, x);
			c3->d = tk;
			c2->a = tk;
			if (A [2].flags & Sseen) {
				tk->maxsp = A [2].s;
			} else {
				tk->maxsp = KMH2MMS (60);
			}

			tk = MkTrk (A [3].m + A [2].m, currconn, c2, x);
			currconn->b = tk;
			c2->c = tk;

			c3->bctl = x;
			c3->d_nb = 1;
			c2->actl = x;
			c2->c_na = 1;

			x->swtch = c3;
			x->swtcb = c2;

			currconn = c2;
			return 0;
		}
		ErrDiag ("Syntax");
	default:
		ErrDiag ("What?");
	}
}

int readgpfile (char *fn) {
#if 0
	struct dnode *x;
#endif
	FILE *fp = fopen (fn, "r");
	if (fp) {
		char buf [512];
		while (fgets (buf, sizeof (buf), fp)) {
#if 0
printf ("%03d/%03d %s", currpos.x, currpos.y, buf);
#endif
			if (parseline (buf)) break;
			if (max_ypos < currpos.y) max_ypos = currpos.y;
		}
		fclose (fp);
	} else {
		printf ("Cannot open %s\n", fn);
		exit (1);
	}
#if 0
	for (x = dnode_list; x; x = x->tnext) {
		printf ("type %d: %03d,%03d %03d,%03d",
			x->type,
			x->cent.x, x->cent.y,
			x->one.x, x->one.y);
		if (x->two.x || x->two.y) {
			printf (" %03d,%03d",
				x->two.x, x->two.y);
		}
		if (x->three.x || x->three.y) {
			printf (" %03d,%03d",
				x->three.x, x->three.y);
		}
		printf ("\n");
	}
#endif
	return 0;
}

int done=0;

int r_flg = 0, t_flg = 0, a_flg = 0;

char *fnam=0;

void everysec (void);

char *pszHost;

int DNodeEvent_B1 (struct dnode *x, int xo, int yo);
int DNodeEvent_B2 (struct dnode *x, int xo, int yo, int state);

void DrawPicture (void);
void DrawSmallPicture (void);
void DrawPart (struct rect *);

void stash (int f) {
	if (ypos + ysize > totalysize) {
		ypos = totalysize - ysize;
		f = 1;
	}
	if (ypos < 0) {
		ypos = 0;
		f = 1;
	}
	if (f) {
		XMoveWindow (mydisplay, drawwindow, 0, -ypos);
	}
}

void handle_smlwin(myevent) XEvent myevent; {
    /* struct dnode *dp; struct rect R; int x,y; */
    {
#if 0
	KeySym mykey;
	char text[10];
	int i;
#endif
	switch(myevent.type) {
	  case Expose:
	    if(myevent.xexpose.count) break;
#if 0
	    R.x =myevent.xexpose.x;
	    R.y =myevent.xexpose.y;
	    R.w =myevent.xexpose.width;
	    R.h =myevent.xexpose.height;
	    /* XClearArea(mydisplay,drawwindow,R.x,R.y,R.w,R.h,False); */
	    DrawPart(&R);
#endif
	    DrawSmallPicture ();
	    break;
	  case GraphicsExpose:
	    break;
	  case NoExpose:
	    break;
	  case ConfigureNotify:
#if 0
	    if(xsize!=myevent.xconfigure.width || ysize!=myevent.xconfigure.height) {
		xsize=myevent.xconfigure.width;
		ysize=myevent.xconfigure.height;
		/* todo=1; */ }
#endif
	    break;
	  case ButtonPress:
	    switch(myevent.xbutton.button) {
#if 0
	      case 1:
		x = myevent.xbutton.x;
		y = myevent.xbutton.y;
		for (dp = dnode_list; dp; dp = dp->tnext) {
			if (DNodeEvent_B1 (dp,
					   x - XCenter (dp->cent.x),
					   y - YCenter (dp->cent.y))) {
				break;
			}
		}
		break;
	      case 2:
		x = myevent.xbutton.x;
		y = myevent.xbutton.y;
		for (dp = dnode_list; dp; dp = dp->tnext) {
			if (DNodeEvent_B2 (dp,
					   x - XCenter (dp->cent.x),
					   y - YCenter (dp->cent.y),
					   myevent.xbutton.state)) {
				break;
			}
		}
		break;
#endif
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

void handle_bwin(myevent) XEvent myevent; {
    struct dnode *dp;
    struct rect R;
    int x,y; {
#if 0
	KeySym mykey;
	char text[10];
	int i;
#endif
	switch(myevent.type) {
	  case Expose:
#if 0
	    /* if(myevent.xexpose.count) break; */
	    R.x =myevent.xexpose.x;
	    R.y =myevent.xexpose.y;
	    R.w =myevent.xexpose.width;
	    R.h =myevent.xexpose.height;
	    /* XClearArea(mydisplay,drawwindow,R.x,R.y,R.w,R.h,False); */
	    DrawPart(&R);
#endif
	    break;
	  case GraphicsExpose:
	    break;
	  case NoExpose:
	    break;
	  case ConfigureNotify:
	    if(xsize!=myevent.xconfigure.width || ysize!=myevent.xconfigure.height) {
		xsize=myevent.xconfigure.width;
		ysize=myevent.xconfigure.height;
		stash (0);
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
	    break;
	  case KeyPress:
	    break; }}}

int oncesteps = 1;
int nstanding = 0;

void handle_window(myevent) XEvent myevent; {
    struct dnode *dp;
    struct rect R;
    int x,y; {
	KeySym mykey;
	char text[10];
	int i;
	switch(myevent.type) {
	  case Expose:
	    /* if(myevent.xexpose.count) break; */
	    R.x =myevent.xexpose.x;
	    R.y =myevent.xexpose.y;
	    R.w =myevent.xexpose.width;
	    R.h =myevent.xexpose.height;
	    /* XClearArea(mydisplay,drawwindow,R.x,R.y,R.w,R.h,False); */
	    DrawPart(&R);
	    break;
	  case GraphicsExpose:
	    break;
	  case NoExpose:
	    break;
	  case ConfigureNotify:
#if 0
	    if(xsize!=myevent.xconfigure.width || ysize!=myevent.xconfigure.height) {
		xsize=myevent.xconfigure.width;
		ysize=myevent.xconfigure.height;
		/* todo=1; */ }
#endif
	    break;
	  case ButtonPress:
	    switch(myevent.xbutton.button) {
	      case 1:
		x = myevent.xbutton.x;
		y = myevent.xbutton.y;
		for (dp = dnode_list; dp; dp = dp->tnext) {
			if (DNodeEvent_B1 (dp,
					   x - XCenter (dp->cent.x),
					   y - YCenter (dp->cent.y))) {
				break;
			}
		}
		break;
	      case 2:
		x = myevent.xbutton.x;
		y = myevent.xbutton.y;
		for (dp = dnode_list; dp; dp = dp->tnext) {
			if (DNodeEvent_B2 (dp,
					   x - XCenter (dp->cent.x),
					   y - YCenter (dp->cent.y),
					   myevent.xbutton.state)) {
				break;
			}
		}
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
	    i=XLookupString(&myevent,text,10,&mykey,0);
	    if(i==1) switch(text[0]) {
	      case '0':
		oncesteps = 0;
		break;
	      case '1':
		oncesteps = 1;
		break;
	      case '2':
		oncesteps = 2;
		break;
	      case '3':
		oncesteps = 3;
		break;
	      case '4':
		oncesteps = 4;
		break;
	      case '5':
		oncesteps = 5;
		break;
	      case '6':
		oncesteps = 6;
		break;
	      case '7':
		oncesteps = 7;
		break;
	      case '8':
		oncesteps = 8;
		break;
	      case '9':
		oncesteps = 9;
		break;
	      case '-':
		if (oncesteps > 0) oncesteps --;
		break;
	      case '+':
		if (oncesteps < 20) oncesteps ++;
		break;
	      case 'a':
		if (trlist && trlist->speed == 0 && trlist != followtrain) {
		  set_followtrain (trlist);
		}
		break;
	      case 'n':
		if (followtrain) {
		  set_followtrain (followtrain->next);
		}
		break;
	      default:
	    } else {
	      switch (mykey) {
		case XK_R9:
		case XK_Prior:
		  ypos -= ysize - 4 * FieldYSize;
		  stash (1);
		  break;
		case XK_R15:
		case XK_Next:
		  ypos += ysize - 4 * FieldYSize;
		  stash (1);
		  break;
		case XK_R14:
		case XK_Down:
		  ypos += (myevent.xkey.state & ShiftMask) ? 1 : FieldYSize;
		  stash (1);
		  break;
		case XK_R8:
		case XK_Up:
		  ypos -= (myevent.xkey.state & ShiftMask) ? 1 : FieldYSize;
		  stash (1);
		  break;
	      }
	    }
	    break; }}}

int X_req (MUX_FD_TYP *x) {
	return MUX_POLLIN;
}

void X_hdl (MUX_FD_TYP *x, int rev) {
	XEvent myevent;
	if (rev & MUX_POLLIN) {
		while (XEventsQueued (mydisplay, QueuedAfterReading) > 0) {
			struct train *curr;
			XNextEvent(mydisplay,&myevent);
			if(myevent.type==MappingNotify) {
				XRefreshKeyboardMapping(&myevent); }
			else if(myevent.xany.window==basewindow) {
				handle_bwin(myevent); }
			else if(myevent.xany.window==drawwindow) {
				handle_window(myevent); }
			else if(myevent.xany.window==smlwin) {
				handle_smlwin(myevent); }
			else for (curr = trlist; curr; curr = curr->next) {
				if (curr->dwin == myevent.xany.window) {
					train_event (curr, myevent);
					break; }}}
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

static FILE *dump_cnts (char *s) {
	FILE *fp = fopen (s, "w");
	struct dnode *dn;
	if (fp) {
		for (dn = dnode_list; dn; dn = dn->tnext) {
			if (dn->sig && dn->sig->name) {
				fprintf (fp, "%d %s\n", dn->sig->cnt, dn->sig->name);
			}
		}
		fclose (fp);
	}
	return fp;
}

void to_fire (MUX_TIMEOUT_TYP *pTo, struct timeval dt) {
	int i;
	struct timeval now = MUX_GetTime ();
	TV_AddFrac (&dt, 1, 10);
	TV_AddInt (&now, -1, 0);
	if (TV_LessThan (&dt, &now)) {
		MUX_SetTimeout (pTo, &now);
	} else {
		MUX_SetTimeout (pTo, &dt);
	}

	for (i = 0; i < oncesteps; i ++) {
		everysec ();
		emulsteps ++;
	}
	if (todo) {
		XClearArea(mydisplay,drawwindow,0,0,0,0,False);
		DrawPicture ();
	}
	XFlush (mydisplay);
	if (done) {
		dump_cnts ("e.cnt");
		exit (0);
	}
}

	/*
	gettimeofday (&currtime, 0);
	currtime.tv_usec = 102000 - ((currtime.tv_usec + 1000000) % 100000);
	currtime.tv_sec = 0;
    XFreeGC(mydisplay,mygc);
    XDestroyWindow(mydisplay,drawwindow);
    XCloseDisplay(mydisplay);
    exit(0); }
	*/

struct connsig {
	struct connsig *next;
	char name [1];
};

void fini_conn (MUX_BMFCONN_TYP *pC) {
	struct connsig *s;
	while ((s = pC->pvUserData)) {
		pC->pvUserData = s->next;
		free (s);
	}
}

bmfItem_t *bmfSigState (struct dnode *x) {
	int hk = MMS2KMH (x->hps) / 10;
	int vk = MMS2KMH (x->vrs) / 10;
	int s = x->state;
	bmfBuild_t B;
	bmfBuildInit (&B);
	bmfBuildAddName (&B, "sig");
	bmfBuildAddString (&B, x->sig->name);
	if (s & DST_Hp1) {
		bmfBuildAddName (&B, "Hp1");
		if (hk < 16) {
			bmfBuildAddAsgnInt (&B, "Zs3", hk);
		}
	} else if (s & DST_Hp2) {
		bmfBuildAddName (&B, "Hp2");
		if (hk != 4) {
			bmfBuildAddAsgnInt (&B, "Zs3", hk);
		}
	} else if (s & DST_Sh1) {
		bmfBuildAddName (&B, "Sh1");
	} else if (s & DST_Kl) {
		bmfBuildAddName (&B, "Kl");
	} else if (x->type & HpSig) {
		if (x->type & LsSig) {
			bmfBuildAddName (&B, "Hp00");
		} else {
			bmfBuildAddName (&B, "Hp0");
		}
	} else if (x->type & LsSig) {
		bmfBuildAddName (&B, "Sh0");
	}
	if (s & DST_Vr0) {
		bmfBuildAddName (&B, "Vr0");
	} else if (s & DST_Vr1) {
		bmfBuildAddName (&B, "Vr1");
		if (vk < 16) {
			bmfBuildAddAsgnInt (&B, "Zs3v", vk);
		}
	} else if (s & DST_Vr2) {
		bmfBuildAddName (&B, "Vr2");
		if (vk != 4) {
			bmfBuildAddAsgnInt (&B, "Zs3v", vk);
		}
	}
	if (s & DST_VrWh) {
		bmfBuildAddName (&B, "VrWh");
	}
	return bmfBuildFini (&B);
}

static BMFCONN_CMD_HDLR (BmfSig) {
	bmfItem_t *l;
	for (l = args; !bmfIsEnd (l); l = bmfGetNext (l)) {
		char *n = bmfGetString (l);
		if (n) {
			struct connsig *s;
			struct dnode *dn;
			s = malloc (sizeof (struct connsig) + strlen (n));
			if (s) {
				strcpy (s->name, n);
				s->next = pConn->pvUserData;
				pConn->pvUserData = s;
			}
			for (dn = dnode_list; dn; dn = dn->tnext) {
				if (dn->sig && !strcmp (dn->sig->name, n)) {
					MUX_SendMsgToBmfConn (pConn, bmfSigState (dn));
				}
			}
		}
	}
	return BMFCONN_CMD_OK ();
}

static BMFCONN_CMD_HDLR (BmfDumpCounts) {
	struct dnode *dn;
	FILE *fp = dump_cnts ("siglist");
	return BMFCONN_CMD_OK ();
}

static BMFCONN_CMD_HDLR (BmfUsePlan) {
	int d = useplans;
	if (bmfParseMessage (args,
			BMF_INT | BMF_OPT_OPT, &d,
			BMF_END) == 0) {
		useplans = d;
		return bmfMakeMessage (
			BMF_NAME, "ok",
			BMF_NAME, bmfGetName (cmd),
			BMF_INT, d,
			BMF_END);
	}
	return BMFCONN_CMD_SYNERR ();
}

static BMFCONN_CMD_HDLR (BmfGetSteps) {
	if (bmfIsEnd (args)) {
		return bmfMakeMessage (
			BMF_NAME, "ok",
			BMF_NAME, bmfGetName (cmd),
			BMF_INT, emulsteps,
			BMF_END);
	}
	return BMFCONN_CMD_SYNERR ();
}

static BMFCONN_CMD_HDLR (BmfNStanding) {
	if (bmfIsEnd (args)) {
		return bmfMakeMessage (
			BMF_NAME, "ok",
			BMF_NAME, bmfGetName (cmd),
			BMF_INT, nstanding,
			BMF_END);
	}
	return BMFCONN_CMD_SYNERR ();
}

static BMFCONN_CMD_HDLR (BmfOnceSteps) {
	int d = oncesteps;
	if (bmfParseMessage (args,
			BMF_INT | BMF_OPT_OPT, &d,
			BMF_END) == 0) {
		if (d < 1) d = 1;
		oncesteps = d;
		return bmfMakeMessage (
			BMF_NAME, "ok",
			BMF_NAME, bmfGetName (cmd),
			BMF_INT, oncesteps,
			BMF_END);
	}
	return BMFCONN_CMD_SYNERR ();
}

MUX_BMFCMDTABLE_TYP arCmdTable[] = {
	{ "sig", BmfSig, "sig [name:str]..." },
	{ "use-plans", BmfUsePlan, "use-plans [flag:int]" },
	{ "get-steps", BmfGetSteps, "get-steps" },
	{ "once-steps", BmfOnceSteps, "once-steps [steps:int]" },
	{ "nstanding", BmfNStanding, "nstanding" },
	{ "dump-counts", BmfDumpCounts, "dump-counts ..." },
	{ 0, 0, 0 }
};

int main(argc,argv) int argc; char **argv; {
    XSizeHints myhint;
    int myscreen;
    int i;
    struct timeval currtime;
    char *display_name;
    char *fontname=0;
    char *deffont="-adobe-helvetica-medium-r-normal--14-140-75-75-p-77-iso8859-1";
    int dummy;
    int pt;
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
                iDebugLevel ++; }
	    else if(!strncmp(argv[i],"-d",2)) {
                iDebugLevel = atoi (argv[i] + 2); }
	    else if(!strcmp(argv[i],"-r")) {
                r_flg++; }
	    else if(!strcmp(argv[i],"-t")) {
                t_flg++; }
	    else if(!strcmp(argv[i],"-x")) {
		FieldXSize -= 3; }
	    else if(!strcmp(argv[i],"-a")) {
		FieldXSize -= 3;
		FieldYSize -= 2;
                a_flg++; }
	    else if(!strncmp(argv[i],"-s",2)) {
		oncesteps = atoi (argv [i] + 2);
		if (oncesteps < 1) oncesteps = 1; }
	    else printf("Unknown option %s\n",argv[i]); }
	else {
	    fnam=argv[i]; }}
    if(!display_name) display_name="";

    pszHost = fnam;
    if (!pszHost) pszHost = "gp.in";

    xsize = (xsize * FieldXSize) / 24;

    readgpfile (pszHost);
    totalysize = YCenter (max_ypos + 4);
    if (ysize > totalysize) ysize = totalysize;

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
    myhint.x=100;
    myhint.y=100;
    myhint.width=xsize;
    myhint.height=ysize;
    myhint.flags=PSize;
    cmap = DefaultColormap (mydisplay, myscreen);
    if (XAllocNamedColor (mydisplay, cmap, "gray", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color gray failed\n");
    }
    basewindow=XCreateSimpleWindow(mydisplay,
       DefaultRootWindow(mydisplay),
       myhint.x,myhint.y,myhint.width,myhint.height,
       1, myforeground,color.pixel);
    drawwindow=XCreateSimpleWindow(mydisplay,
	basewindow,
	0, ypos, xsize, totalysize,
	0, myforeground,color.pixel);
    XSetStandardProperties(mydisplay,basewindow,"esim","ESim",
       None,argv,argc,&myhint);
    smlwin=XCreateSimpleWindow(mydisplay,
       DefaultRootWindow(mydisplay),
       myhint.x,myhint.y,myhint.width/6,myhint.height/6,
       1, myforeground,color.pixel);
    XSetStandardProperties(mydisplay,smlwin,"esimov","ESimOv",
       None,argv,argc,&myhint);
    mygc=XCreateGC(mydisplay,basewindow,0,0);
    XSetBackground (mydisplay, mygc, mybackground);
    XSetForeground (mydisplay, mygc, myforeground);
    mysmlgc=XCreateGC(mydisplay,smlwin,0,0);
    XSetBackground (mydisplay, mysmlgc, mybackground);
    XSetForeground (mydisplay, mysmlgc, myforeground);

    myclrgc=XCreateGC(mydisplay,basewindow,0,0);
    mysmlclrgc=XCreateGC(mydisplay,smlwin,0,0);
    XSetBackground (mydisplay, myclrgc, myforeground);
    XSetForeground (mydisplay, myclrgc, mybackground);
    XSetBackground (mydisplay, mysmlclrgc, myforeground);
    XSetForeground (mydisplay, mysmlclrgc, mybackground);

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
    mysmlredgc=XCreateGC(mydisplay,smlwin,0,0);
    if (XAllocNamedColor (mydisplay, cmap, "red", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color red failed\n");
    }
markground = color.pixel;
spaceground = mybackground;
    XSetForeground (mydisplay, myredgc, color.pixel);
    XSetBackground (mydisplay, myredgc, mybackground);
    XSetForeground (mydisplay, mysmlredgc, color.pixel);
    XSetBackground (mydisplay, mysmlredgc, mybackground);

    myyellowgc=XCreateGC(mydisplay,basewindow,0,0);
    if (XAllocNamedColor (mydisplay, cmap, "orange", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color yellow failed\n");
    }
    XSetForeground (mydisplay, myyellowgc, color.pixel);
    XSetBackground (mydisplay, myyellowgc, mybackground);

    mygreengc=XCreateGC(mydisplay,basewindow,0,0);
    mysmlgreengc=XCreateGC(mydisplay,smlwin,0,0);
    if (XAllocNamedColor (mydisplay, cmap, "green", &exact, &color) == 0) {
	color.pixel = myforeground;
	printf ("Color green failed\n");
    }
    XSetForeground (mydisplay, mygreengc, color.pixel);
    XSetBackground (mydisplay, mygreengc, mybackground);
    XSetForeground (mydisplay, mysmlgreengc, color.pixel);
    XSetBackground (mydisplay, mysmlgreengc, mybackground);

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

    if (XAllocNamedColor (mydisplay, cmap, "lightskyblue", &exact, &color) == 0) {
	printf ("Color lightskyblue failed\n");
    } else {
	markground = color.pixel;
    }

    if (XAllocNamedColor (mydisplay, cmap, "lemonchiffon", &exact, &color) == 0) {
	printf ("Color lemonchiffon failed\n");
    } else {
	spaceground = color.pixel;
    }

    XSetLineAttributes (mydisplay, mygc, 9, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, myredgc, 5, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, mygreengc, 5, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, myyellowgc, 5, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, mywhitegc, 3, LineSolid, CapRound, JoinRound);
    XSetLineAttributes (mydisplay, myclrgc, 5, LineSolid, CapRound, JoinRound);

    XSetFont(mydisplay,mygc,myfontstruct->fid);
    XTextExtents(myfontstruct,"",0,&dummy,&font_height,&font_depth,&Dummy);
    mycurs=XCreateFontCursor(mydisplay,XC_crosshair);
    mydotcurs=XCreateFontCursor(mydisplay,XC_dot);
    XDefineCursor(mydisplay,drawwindow,mycurs);
    XSelectInput(mydisplay,basewindow,
       StructureNotifyMask);
    XSelectInput(mydisplay,drawwindow,
       ButtonPressMask|ButtonReleaseMask
       |Button1MotionMask
       |KeyPressMask|ExposureMask|StructureNotifyMask);
    XSelectInput(mydisplay,smlwin,
       ButtonPressMask|ButtonReleaseMask
       |Button1MotionMask
       |KeyPressMask|ExposureMask|StructureNotifyMask);
    XMapRaised(mydisplay,basewindow);
    XMapRaised(mydisplay,drawwindow);
    XMapRaised(mydisplay,smlwin);

    MUX_InitTimeout (&xTo, to_fire, 0);
    currtime = MUX_GetTime ();
    MUX_SetTimeout (&xTo, &currtime);

    MUX_InitMuxFd (&xX, ConnectionNumber (mydisplay),
		X_req, X_hdl, X_fail, 0);
    MUX_AddMuxFd (&xX);

    MUX_InitBmfConnList (&xConnList, "esim",
		0, fini_conn, arCmdTable);
    for (pt = 8440; ; pt ++) {
	if (pt < 8450) {
		if (!MUX_CreateBmfTcpListener (&xListen, pt, &xConnList)) {
			if (pt > 8440) printf ("Listening on %d\n", pt);
			break;
		}
		continue;
	}
	printf ("Listening failed...\n");
	exit (1);
    }
    MUX_AddBmfListener(&xListen);

    MUX_DoServe ();
    exit (1);
}

void DrawLine (struct coord *a, struct coord *b) {
	XDrawLine (mydisplay, drawwindow, mygc,
		   XCenter (a->x),
		   YCenter (a->y),
		   XCenter (b->x),
		   YCenter (b->y));
}

void DrawSLine (struct moff *moff, struct coord *a, struct coord *b, GC *gcp) {
	if (moff->r) {
		XDrawLine (mydisplay, smlwin, *gcp,
		   	moff->x - a->x, moff->y - a->y,
			moff->x - b->x, moff->y - b->y);
	} else {
		XDrawLine (mydisplay, smlwin, *gcp,
		   	moff->x + a->x, moff->y + a->y,
			moff->x + b->x, moff->y + b->y);
	}
}

void DrawFLine (struct coord *a, struct coord *b, int f, int g, GC *gcp) {
	int xs = b->x - a->x;
	int ys = b->y - a->y;
	if (xs > 0) xs = FieldXSize;
	else if (xs < 0) xs = -FieldXSize;
	if (ys > 0) ys = FieldYSize;
	else if (ys < 0) ys = -FieldYSize;
	XDrawLine (mydisplay, drawwindow, *gcp,
		   XCenter (a->x) + (xs * f) / 12,
		   YCenter (a->y) + (ys * f) / 12,
		   XCenter (b->x) - (xs * g) / 12,
		   YCenter (b->y) - (ys * g) / 12);
}

static int conn_pred (MUX_BMFCONN_TYP *pConn, void *p) {
	struct connsig *s;
	for (s = pConn->pvUserData; s; s = s->next) {
		if (strcmp (s->name, p) == 0) {
			return 1;
		}
	}
	return 0;
}

void DrawDIllum (struct dnode *x) {
	int xb, yb, xc;
	GC *gcp, *gcpu;
	int fl;
	int ry;
	if (x->sig && x->sig->name) {
		if (x->propstate != x->state) {
			x->propstate = x->state;
			if (x->state == 0) {
				x->sig->cnt ++;
			}
			MUX_SendMsgToPredicatedBmfConn (
				&xConnList,
				bmfSigState (x),
				conn_pred,
				x->sig->name);
		}
	}

	ry = YCenter (x->cent.y);
	if (ry < ypos - 4 * FieldYSize ||
	    ry > ypos + 4 * FieldYSize + ysize) {
		/* Out of sight */
		return;
	}

	if (x->type == Straight) {
		if (x->occcnt) {
			gcp = &myredgc;
		} else if (x->lock) {
			gcp = &myclrgc;
		} else {
			gcp = &mygc;
		}
		DrawFLine (&x->cent, &x->one, 4, 4, gcp);
	} else if (x->type == Curve) {
		if (x->occcnt) {
			gcp = &myredgc;
		} else if (x->lock) {
			gcp = &myclrgc;
		} else {
			gcp = &mygc;
		}
		DrawFLine (&x->cent, &x->one, 0, 4, gcp);
		DrawFLine (&x->cent, &x->two, 0, 4, gcp);
	} else if (x->type == FullCross || x->type == HalfCross) {
		int nb = x->swtch->actl == x ? x->swtch->c_na : x->swtch->d_nb;
		int nd = x->swtcb->actl == x ? x->swtcb->c_na : x->swtcb->d_nb;
		if (x->occcnt) {
			gcp = &myredgc;
		} else {
			gcp = &myclrgc;
		}
		fl = 0;
		if (!x->lock && !x->occcnt) {
			fl = 4;
		}
		DrawFLine (&x->cent, &x->two, 0, 4, &mygc);
		DrawFLine (&x->cent, &x->three, 0, 4, &mygc);
		DrawFLine (&x->cent, &x->one, 0, 4, &mygc);
		DrawFLine (&x->cent, &x->four, 0, 4, &mygc);
		if (nb) {
			DrawFLine (&x->cent, &x->three, fl, 4, gcp);
		} else {
			DrawFLine (&x->cent, &x->two, fl, 4, gcp);
		}
		if (nd) {
			DrawFLine (&x->cent, &x->four, fl, 4, gcp);
		} else {
			DrawFLine (&x->cent, &x->one, fl, 4, gcp);
		}
	} else if (x->type == Switch) {
		if (x->occcnt) {
			gcp = &myredgc;
		} else {
			gcp = &myclrgc;
		}
#if 0
		DrawFLine (&x->cent, &x->one, 4, 4, 
			   x->lock || x->occcnt ? gcp : &mygc);
		if (x->swtch->actl == x ? x->swtch->c_na : x->swtch->d_nb) {
			DrawFLine (&x->cent, &x->two, 4, 4, &mygc);
			DrawFLine (&x->cent, &x->three, 4, 4, gcp);
		} else {
			DrawFLine (&x->cent, &x->three, 4, 4, &mygc);
			DrawFLine (&x->cent, &x->two, 4, 4, gcp);
		}
#else
		fl = 0;
		if (!x->lock && !x->occcnt) {
			DrawFLine (&x->cent, &x->one, 0, 4,  &mygc);
			fl = 4;
		}
		if (x->swtch->actl == x ? x->swtch->c_na : x->swtch->d_nb) {
			DrawFLine (&x->cent, &x->two, 0, 4, &mygc);
DrawFLine (&x->cent, &x->three, 0, 4, &mygc);
			DrawFLine (&x->cent, &x->three, fl, 4, gcp);
		} else {
			DrawFLine (&x->cent, &x->three, 0, 4, &mygc);
DrawFLine (&x->cent, &x->two, 0, 4, &mygc);
			DrawFLine (&x->cent, &x->two, fl, 4, gcp);
		}
		if (x->lock || x->occcnt) {
			DrawFLine (&x->cent, &x->one, 0, 4, gcp);
		}
#endif
	} else if (x->type & Stop) {
		/* End block, no changing illum */
	} else if (x->type & (HpSig | LsSig)) {
		/* Signals are only horizontal */
		GC *gclp;
		int xd;
		if (x->state & DST_Kl) {
			gcp = &mygc;
			gcpu = &mywhitegc;
		} else if (x->state & DST_Hp2) {
			gcp = &myyellowgc;
			gcpu = &mygreengc;
		} else if (x->state & DST_Hp1) {
			gcp = &mygc;
			gcpu = &mygreengc;
		} else if (x->state & DST_Sh1) {
			gcp = 0;	/* For white bar */
			gcpu = &myredgc;
		} else if (x->type & HpSig) {
			if (x->type & LsSig) {
				/* Hs, Hp00 */
				gcp = 0; /* &myclrgc; */
				gcpu = &myredgc;
			} else {
				/* Hp, Hp0 */
				gcp = &myredgc;
				gcpu = &mygc;
			}
		} else {
			/* Ls, Sh0 */
			gcp = 0;	/* For red bar */
			gcpu = &mygc;
		}
		if (IsSameDir (Dir_W, x->sigdir)) {
			/* Looking right */
			xb = XCenter (x->cent.x) - XXIII + 12;
			yb = YCenter (x->cent.y) + 12;
			xc = xb + 9;
			xd = xb - 16;
		} else {
			xb = XCenter (x->cent.x) + XXIII - 12;
			yb = YCenter (x->cent.y) - 12;
			xc = xb - 9;
			xd = xb + 16;
		}
		switch (x->mode) {
		case -3:
			gclp = &myredgc;
			break;
		case -2:
			gclp = &myyellowgc;
			break;
		case -1:
			gclp = &mygreengc;
			break;
		case 0:
			gclp = &mygraygc;
			break;
		case 1:
			gclp = &mywhitegc;
			break;
		default:
			gclp = &mybluegc;
			break;
		}
		XFillRectangle (mydisplay, drawwindow, *gclp,
				xd - 1, yb - 1, 3, 3);
		if (x->type & HpSig) {
			/* Only these have the upper light */
			if (gcpu == &mywhitegc) {
				/* Need clear the border */
				XDrawLine (mydisplay,
					   drawwindow,
					   mygc,
					   xc, yb,
					   xc, yb);
			}
			XDrawLine (mydisplay,
				   drawwindow,
				   *gcpu,
				   xc, yb,
				   xc, yb);
		}
		XDrawLine (mydisplay,
			   drawwindow,
			   mygc,
			   xb, yb,
			   xb, yb);
		if (gcp) {
			XDrawLine (mydisplay,
				   drawwindow,
				   *gcp,
				   xb, yb,
				   xb, yb);
			if (gcp == &mygc &&
			    !(x->state & DST_Hp2) &&
			    x->state & (DST_Vr0 | DST_Vr1 | DST_Vr2)) {
				/* May draw a Vr */
				int xv;
				int yv;
				int dv;
				GC *vgc = &myyellowgc;
				if (IsSameDir (Dir_W, x->sigdir)) {
					xv = xb - 2;
					yv = yb - 2;
					dv = 3;
				} else {
					xv = xb + 1;
					yv = yb + 1;
					dv = -3;
				}
				if (x->state & DST_Vr1) {
					vgc = &mygreengc;
				}
				XFillRectangle (mydisplay, drawwindow, *vgc,
						xv, yv, 2, 2);
				if (x->state & DST_Vr2) {
					vgc = &mygreengc;
				}
				XFillRectangle (mydisplay, drawwindow, *vgc,
						xv + dv, yv + dv, 2, 2);
				if (x->state & DST_VrWh) {
					XFillRectangle (mydisplay, drawwindow,
							mywhitegc,
							xv + dv, yv, 2, 2);
				}
			}
		} else {
			if (x->state & DST_Sh1) {
				/* White bar */
				int i;
				for (i = 0; i < 4; i ++) {
					XFillRectangle (mydisplay,
							drawwindow,
							myclrgc,
							xb - 2 + i, yb - 2 + i,
							2, 2);
				}
			} else {
				/* Red bar */
				XFillRectangle (mydisplay,
						drawwindow,
						myredgc,
						xb - 1, yb - 2,
						3, 5);
			}
		}
	}
}

void DrawDNode (struct dnode *x) {
	int xb, yb, l;
	if (x->type == Straight) {
		DrawLine (&x->cent, &x->one);
	} else if (x->type == Curve) {
		DrawLine (&x->cent, &x->one);
		DrawLine (&x->cent, &x->two);
	} else if (x->type == Switch) {
		DrawLine (&x->cent, &x->one);
		DrawLine (&x->cent, &x->two);
		DrawLine (&x->cent, &x->three);
	} else if (x->type == FullCross || x->type == HalfCross) {
		DrawLine (&x->cent, &x->one);
		DrawLine (&x->cent, &x->two);
		DrawLine (&x->cent, &x->three);
		DrawLine (&x->cent, &x->four);
	} else if (x->type & (Stop)) {
		xb = XCenter (x->cent.x);
		yb = YCenter (x->cent.y);
		if (IsSameDir (Dir_E, x->sigdir)) {
			xb -= 5;
		}
		XFillRectangle (mydisplay,
				drawwindow,
				mygc,
				xb, yb - 9,
				6, 19);
	} else if (x->type & (HpSig | LsSig)) {
		DrawLine (&x->cent, &x->one);
		/* Signals are only horizontal */
		if (x->type & HpSig) {
			l = 21;
		} else {
			l = 12;
		}
		if (IsSameDir (Dir_W, x->sigdir)) {
			/* Looking right */
			xb = XCenter (x->cent.x) - XXIII;
			yb = YCenter (x->cent.y) + 12;
			XFillRectangle (mydisplay,
					drawwindow,
					mygc,
					xb, yb - 4,
					1, 9);
			XFillRectangle (mydisplay,
					drawwindow,
					mygc,
					xb, yb - 1,
					10, 3);
			XDrawLine (mydisplay,
				   drawwindow,
				   mygc,
				   xb + 12, yb,
				   xb + l, yb);
		} else {
			/* Looking left */
			xb = XCenter (x->cent.x) + XXIII;
			yb = YCenter (x->cent.y) - 12;
			XFillRectangle (mydisplay,
					drawwindow,
					mygc,
					xb, yb - 4,
					1, 9);
			XFillRectangle (mydisplay,
					drawwindow,
					mygc,
					xb - 9, yb - 1,
					10, 3);
			XDrawLine (mydisplay,
				   drawwindow,
				   mygc,
				   xb - 12, yb,
				   xb - l, yb);
		}
	}
	DrawDIllum (x);
}

void DrawSmallDNode (struct dnode *d) {
	GC *gcp;
	return;
	if (d->type == Straight || d->type == Switch || d->type == Curve) {
		if (d->occcnt) {
			gcp = &mysmlredgc;
		} else if (d->lock) {
			gcp = &mysmlclrgc;
		} else {
			gcp = &mysmlgc;
		}
		DrawSLine (d->moff, &d->cent, &d->one, gcp);
		if (d->type == Curve) {
			DrawSLine (d->moff, &d->cent, &d->two, gcp);
		} else if (d->type == Switch) {
			if (d->swtch->actl == d ? d->swtch->c_na : d->swtch->d_nb) {
				DrawSLine (d->moff, &d->cent, &d->two, &mysmlgc);
				DrawSLine (d->moff, &d->cent, &d->three, gcp);
			} else {
				DrawSLine (d->moff, &d->cent, &d->three, &mysmlgc);
				DrawSLine (d->moff, &d->cent, &d->two, gcp);
			}
		}
	} else if (d->type & HpSig) {
		/* Signals are only horizontal */
		GC *gclp;
		/* int xd; */
		int x, y;
		if (d->state) {
			gclp = &mysmlgreengc;
		} else {
			gclp = &mysmlredgc;
		}
		if (d->moff->r) {
			x = d->moff->x - d->cent.x;
			y = d->moff->y - d->cent.y;
		} else {
			x = d->moff->x + d->cent.x;
			y = d->moff->y + d->cent.y;
		}
		if (IsSameDir (Dir_W, d->sigdir) != d->moff->r) {
			/* Looking right */
			y ++;
			x --;
		} else {
			y --;
		}
		XFillRectangle (mydisplay, smlwin, *gclp,
				x, y, 2, 1);
		DrawSLine (d->moff, &d->cent, &d->one, d->state ? &mysmlgreengc :  &mysmlgc);
	} else if (d->type & LsSig) {
		DrawSLine (d->moff, &d->cent, &d->one, d->state ? &mysmlclrgc :  &mysmlgc);
	} else {
		DrawSLine (d->moff, &d->cent, &d->one, &mysmlgc);
	}
}

void extend_rect (struct rect *a, int x, int y) {
	int z;
	z = a->x - x;
	if (z > 0) {
		a->w += z;
		a->x -= z;
	}
	z = x - (a->x + a->w);
	if (z > 0) {
		a->w += z;
	}
	z = a->y - y;
	if (z > 0) {
		a->h += z;
		a->y -= z;
	}
	z = y - (a->y + a->h);
	if (z > 0) {
		a->h += z;
	}
}

int IsInDNode (struct dnode *x, struct rect *q) {
	struct rect r;
	struct coord c;
	r.x = XCenter (x->cent.x) - 5;
	r.y = YCenter (x->cent.y) - 5;
	r.w = 11;
	r.h = 11;
	extend_rect (&r, XCenter (x->one.x), YCenter (x->one.y));
	if (x->type == Straight) {
	} else if (x->type == Curve) {
		extend_rect (&r, XCenter (x->two.x), YCenter (x->two.y));
	} else if (x->type == Switch) {
		extend_rect (&r, XCenter (x->two.x), YCenter (x->two.y));
		extend_rect (&r, XCenter (x->three.x), YCenter (x->three.y));
	} else if (x->type == FullCross || x->type == HalfCross) {
		extend_rect (&r, XCenter (x->two.x), YCenter (x->two.y));
		extend_rect (&r, XCenter (x->three.x), YCenter (x->three.y));
		extend_rect (&r, XCenter (x->four.x), YCenter (x->four.y));
	} else if (x->type & (HpSig | LsSig)) {
		if (x->cent.x > x->one.x) {
			/* Looking right */
			c.x = XCenter (x->cent.x) - XXIII;
			c.y = YCenter (x->cent.y) + 12 + 5;
		} else {
			c.x = XCenter (x->cent.x) + XXIII;
			c.y = YCenter (x->cent.y) - 12 - 5;
		}
		extend_rect (&r, c.x, c.y);
	}
	return r.x < q->x + q->w && q->x < r.x + r.w &&
		r.y < q->y + q->h && q->y < r.y + r.h;
}

int DNodeEvent_B1 (struct dnode *x, int xo, int yo) {
	if (x->type == Straight) {
	} else if (x->type == Switch) {
		if (xo * xo + yo * yo < 400) {
			if (x->lock || x->occcnt) {
				/* Nothing */
			} else {
				if (x->swtch->actl == x) {
					turn_a (x->swtch, !x->swtch->c_na);
				} else {
					turn_b (x->swtch, !x->swtch->d_nb);
				}
			}
			return 1;
		}
	} else if (x->type == FullCross) {
		if (xo * xo + yo * yo < 400) {
			if (x->lock || x->occcnt) {
				/* Nothing */
			} else {
				if ((x->mode ++) & 1) {
					if (x->swtch->actl == x) {
						turn_a (x->swtch, !x->swtch->c_na);
					} else {
						turn_b (x->swtch, !x->swtch->d_nb);
					}
				} else {
					if (x->swtcb->actl == x) {
						turn_a (x->swtcb, !x->swtcb->c_na);
					} else {
						turn_b (x->swtcb, !x->swtcb->d_nb);
					}
				}
			}
			return 1;
		}
	} else if (x->type == HalfCross) {
		if (xo * xo + yo * yo < 400) {
			if (x->lock || x->occcnt) {
				/* Nothing */
			} else {
				int m = x->mode ++;
				if ((m ^ (m >> 1)) & 1) {
					if (x->swtch->actl == x) {
						turn_a (x->swtch, !x->swtch->c_na);
					} else {
						turn_b (x->swtch, !x->swtch->d_nb);
					}
				} else {
					if (x->swtcb->actl == x) {
						turn_a (x->swtcb, !x->swtcb->c_na);
					} else {
						turn_b (x->swtcb, !x->swtcb->d_nb);
					}
				}
#if 0
				if ((x->swtcb->actl == x) ? x->swtcb->c_na : x->swtcb->d_nb) {
					if (x->swtch->actl == x) {
						turn_a (x->swtch, 0);
					} else {
						turn_b (x->swtch, 0);
					}
				} else if ((x->swtch->actl == x) ? x->swtch->c_na : x->swtch->d_nb) {
					if (x->swtcb->actl == x) {
						turn_a (x->swtcb, 1);
					} else {
						turn_b (x->swtcb, 1);
					}
				} else {
					if (x->swtch->actl == x) {
						turn_a (x->swtch, 1);
					} else {
						turn_b (x->swtch, 1);
					}
				}
#endif
			}
			return 1;
		}
	} else if (x->type & (HpSig | LsSig)) {
#if 0
		if (x->cent.x > x->one.x) {
			printf ("Right: xo = %d\n", xo);
		} else {
			printf ("Left: xo = %d\n", xo);
		}
#endif
		if (xo * xo + yo * yo < 400) {
			if (x->type & HpSig) {
				try_build_path (x);
				return 1;
			}
			switch (x->state) {
			case 0:
				x->state = DST_Hp1;
				if (x->type & HpSig) break;
			case DST_Hp1:
				x->state = DST_Hp2;
				if (x->type & HpSig) break;
			case DST_Hp2:
				x->state = DST_Sh1;
				if (x->type & LsSig) break;
			case DST_Sh1:
			default:
				x->state = 0;
				break;
			}
			DrawDIllum (x);
			DrawSmallDNode (x);
			return 1;
		}
	}
	return 0;
}

int DNodeEvent_B2 (struct dnode *x, int xo, int yo, int state) {
	if (x->type == Straight) {
	} else if (x->type == Switch) {
	} else if (x->type & (HpSig | LsSig)) {
#if 0
		if (x->cent.x > x->one.x) {
			printf ("Right: xo = %d\n", xo);
		} else {
			printf ("Left: xo = %d\n", xo);
		}
#endif
		if (xo * xo + yo * yo < 400) {
			if (state & ShiftMask) {
				switch (x->mode) {
				default:
					x->mode = -3;
					break;
				case -3:
					x->mode = -2;
					break;
				case -2:
					x->mode = -1;
					break;
				}
			} else {
				switch (x->mode) {
				case 0:
					x->mode = 1;
					break;
				case 1:
					x->mode = 1 << 31;
					break;
				default:
					x->mode = 0;
					break;
				}
			}
			DrawDIllum (x);
			DrawSmallDNode (x);
			return 1;
		}
	}
	return 0;
}

void DrawPart (struct rect *r) {
	struct dnode *X;
	for (X = dnode_list; X; X = X->tnext) {
		if (IsInDNode (X, r)) {
			DrawDNode (X);
		}
	}
}

void DrawSmallPicture (void) {
	struct dnode *x;
	for (x = dnode_list; x; x = x->tnext) {
		DrawSmallDNode (x);
	}
}

void DrawPicture (void) {
	struct dnode *x;
	for (x = dnode_list; x; x = x->tnext) {
		DrawDNode (x);
	}
	if(todo) {
		todo=0;
	}

#if 0
	for (j = 0; j <= (xsize / FieldYSize); j ++) {
		for (i = 0; i <= (xsize / FieldXSize); i ++) {

			XFillRectangle (mydisplay, drawwindow, mygraygc,
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
						drawwindow,
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
						drawwindow,
						mygc,
						xb, yb - 4,
						1, 9);
				XFillRectangle (mydisplay,
						drawwindow,
						mygc,
						xb, yb - 1,
						10, 3);
				XDrawLine (mydisplay,
					   drawwindow,
					   mygc,
					   xb + 12, yb,
					   xb + 21, yb);
				break;
			}

			for (b = 0; b < 8; b ++) {
				if (tp->blacks & dirtab [b].bit) {
					XDrawLine (mydisplay,
						   drawwindow,
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
					   drawwindow,
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
						   drawwindow,
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

#endif

#if 0
	XDrawLine (mydisplay, drawwindow, mygc,
		   XCenter (0), YCenter (5 + 3),
		   XCenter (17), YCenter (5 + 3));

	XDrawLine (mydisplay, drawwindow, mygc,
		   XCenter (4), YCenter (5 + 3),
		   XCenter (5), YCenter (5 + 2));

	XDrawLine (mydisplay, drawwindow, mygc,
		   XCenter (5), YCenter (5 + 2),
		   XCenter (10), YCenter (5 + 2));

	XDrawLine (mydisplay, drawwindow, mygc,
		   XCenter (9), YCenter (5 + 2),
		   XCenter (10), YCenter (5 + 3));

	for (i = 1; i < 17; i ++) {
		int j;

		if (i != 4 && i != 10) {
			XDrawLine (mydisplay, drawwindow, myclrgc,
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

		XDrawLine (mydisplay, drawwindow, myclrgc,
		   XCenter (i) - FieldXSize / 3, YCenter (5 + j),
		   XCenter (i) - FieldXSize / 6, YCenter (5 + j));
		XDrawLine (mydisplay, drawwindow, myclrgc,
		   XCenter (i) + FieldXSize / 6, YCenter (5 + j),
		   XCenter (i) + FieldXSize / 3, YCenter (5 + j));

	}

	for (i = 6; i < 9; i ++) {
		if (i != 4 && i != 10) {
			XDrawLine (mydisplay, drawwindow, myredgc,
			   XCenter (i) - FieldXSize / 3 + 1, YCenter (5 + 2),
			   XCenter (i) + FieldXSize / 3 - 1, YCenter (5 + 2));
		}
	}

	XDrawLine (mydisplay, drawwindow, myclrgc,
	   XCenter (5) - FieldXSize / 3, YCenter (5 + 2) + FieldYSize / 3,
	   XCenter (5) - FieldXSize / 6, YCenter (5 + 2) + FieldYSize / 6);
	XDrawLine (mydisplay, drawwindow, myclrgc,
	   XCenter (5) + FieldXSize / 6, YCenter (5 + 2),
	   XCenter (5) + FieldXSize / 3, YCenter (5 + 2));
#endif

#if 0
	XFillRectangle (mydisplay, drawwindow, mygc,
			35, 35, 120, 230);

	XFillArc (mydisplay, drawwindow, mygreengc,
		  50, 50, 30, 30, 0, 23040);
	XFillArc (mydisplay, drawwindow, myredgc,
		  50, 95, 30, 30, 0, 23040);
	XFillArc (mydisplay, drawwindow, myredgc,
		  110, 95, 30, 30, 0, 23040);
	XFillArc (mydisplay, drawwindow, myclrgc,
		  110 + 7, 140 + 7, 16, 16, 0, 23040);
	XFillArc (mydisplay, drawwindow, myclrgc,
		  50 + 7, 185 + 7, 16, 16, 0, 23040);
	XFillArc (mydisplay, drawwindow, myyellowgc,
		  50, 230, 30, 30, 0, 23040);
#endif

}

int spf = 4;

void everysec (void) {
	struct train *curr, *ol;
	struct train **curp;
	int nstand = 0;
#if 1
	if (iDebugLevel > 0) printf ("--------------\n");
#endif
	for (curr = trlist; curr; curr = curr->next) {
		int z;
		int sp = curr->speed;
		int q = (sp / 10) * (sp / 100);
		int la = q + 5 * sp;
		int lla;
		int eq;
		struct sig *firstsig = 0;
		char buf [1024];

		if (curr->sigdelay > 0) {
			curr->sigdelay --;
		}

/*
		if (curr->dsig) curr->dsig->dsigcnt --;
		curr->dsig = 0;
 */
		currtrn = curr;

		if (la < 50000) la = 50000;
		eq = la;
		lla = la;
if (curr->speed > KMH2MMS (5) && lla < 1500000) lla = 1500000;
		z = test_lookahead (curr, la, lla, 0, buf, &eq, &firstsig);
/* printf ("la=%d, z=%d, q=%d, eq=%d\n", la, z, q, eq); */
		if (curr->accto > 0) curr->accto --;
		if (z < 0) {
			curr->dir = 1 - curr->dir;
			sp = 0;
			if (curr->accto < 60) curr->accto = 60;
		} else {
			int m;
			/* int oz = z; */
			if (eq < z) z = eq;
			z -= 10;
			if (z < 0) z = 0;
			if (q > (z - 25000)) {
				sp -= 50 * spf;
				if (sp < 0) sp = 0;
				if (curr->accto < 30) curr->accto = 30;
			} else if (curr->accto > 0) {
				/* No acc */
			} else if (sp < KMH2MMS (30) || la <= (z + 20)) {
				int k = 360 * spf;
				k *= 50000;
				k /= (curr->len + 25000);
				if (sp > 36000) {
					k *= 36000;
					k /= sp;
				}
				sp += k;
				if (sp > curr->maxspeed) sp = curr->maxspeed;
			}
			if (sp > 10 * z) sp = 10 * z;
			curr->speed = sp;
			m = spf * sp / 10;
			if (m > z) m = z;
			move_train (curr, m);
		}
		if (curr->speed > 0) {
			curr->ztim = 0;
		} else if (curr->ztim < 1000000) {
			nstand ++;
			curr->ztim ++;
		}
		train_dwin (curr, firstsig);
#if 1
		if (iDebugLevel > 0) printf ("%12.12s %5d.%02d (%5d) v=%3d %s\n",
			curr->name ? curr->name : "Track",
			z / 1000, (z / 10) % 100,
			la / 1000, (36 * sp + 500) / 10000, buf);
#endif
		if (curr->speed == 0) {
			int sc = 0;
			curr->dir = 1 - curr->dir;
			z = test_lookahead (curr, 0, 0, &sc, buf, 0, 0);
			if (sc == 0) {
				curr->dir = 1 - curr->dir;
			}
		}
		currtrn = 0;
	}
	ol = 0;
	for (curp = &trlist; (curr = *curp); ) {
		if (curr->oflg) {
			curr->sigdelay = 6;
			curr->oflg --;
			*curp = curr->next;
			curr->next = ol;
			ol = curr;
		} else {
			curp = &(*curp)->next;
		}
	}
	*curp = ol;
	setup_train ();
	do_paths ();
	cycle ++;
	nstanding = nstand;
}
