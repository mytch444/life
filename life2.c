#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#define LOCK_DELAY 100

typedef struct Cell Cell;

struct Cell {
	int x, y;
	int color;

	int dying;
	Cell *next;
};

void setupwindow();
void update();
void setcolor(int color);
void render();

void addcell(Cell *addto, Cell *n);

Cell *neighbours(Cell *c);
int nneighbours(Cell *c);
Cell *reproduce(Cell *c);

int placetaken(Cell *cells, Cell *c);

Cell *newcell(int x, int y);

Cell *copycell(Cell *c);

void key(XEvent *event);
void buttonpress(XEvent *event);
void buttonrelease(XEvent *event);
void motionnotify(XEvent *event);
void configurenotify(XEvent *event);

int quit, paused;

int locked;

int delay;
int width, height;
Cell *cells;

int buttonpressed;

int boxwidth, boxheight;

Display *display;
Window root, win;
GC gc;
XIC xic;

void (*handlers[LASTEvent])(XEvent *e) = {
	[KeyPress] = key,
	[ButtonPress] = buttonpress,
	[ButtonRelease] = buttonrelease,
	[ConfigureNotify] = configurenotify,
	[MotionNotify] = motionnotify,
};

Cell *neighbours(Cell *c) {
	int xd, yd;
	Cell *o, *n, *neighbour;

	neighbour = n = NULL;
	for (o = cells->next; o; o = o->next) {
		if (o != c) {
			xd = o->x - c->x;
			yd = o->y - c->y;
			if ((int) sqrt(xd * xd + yd * yd) == 1) {
				if (!n) {
					neighbour = n = copycell(o);
				} else {
					n->next = copycell(o);
					n = n->next;
				}
			}
		}
	}

	return neighbour;
}

int nneighbours(Cell *c) {
	Cell *p, *o = neighbours(c);
	int n = 0;	
	while (o) {
		p = o;
		o = o->next;
		free(p);
		n++;
	}

	return n;
}

int placetaken(Cell *cells, Cell *c) {
	Cell *o;
	if (c->x < 0 || c->x >= width || c->y < 0 || c->y >= height) return 1;
	for (o = cells; o; o = o->next)
		if (o != c && o->x == c->x && o->y == c->y)
			return 1;
	return 0;
}

Cell *reproduce(Cell *c) {
	Cell *o, *mate, *nn, *new;
	float a, i, ca, sa;

	nn = neighbours(c);
	if (!nn)
		return NULL;

	// Find mate from neighbours.
	for (mate = o = nn; o; o = o->next)
		if (o->color > mate->color)
			mate = o;

	c = copycell(c);
	c->next = nn;
	nn = c;

	new = malloc(sizeof(Cell));
	new->color = (mate->color + c->color) / 2 + rand() % 100 - 50;
	new->x = c->x;
	new->y = c->y;
	new->dying = 0;
	new->next = NULL;

	i = 0;
	while (placetaken(nn, new)) {
		a = (rand() % 9) * (6.28f / 9.0f);
		ca = cos(a); sa = sin(a);

		new->x = c->x + (int) (ca > 0 ? ceil(ca) : -ceil(-ca));
		new->y = c->y + (int) (sa > 0 ? ceil(sa) : -ceil(-sa));
		
		if (i++ > 18)
			return NULL;
	}

	o = nn;
	while (o) {
		nn = o;
		o = o->next;
		free(nn);
	}

	return new;
}

void update() {
	Cell *c, *prev, *new = malloc(sizeof(Cell));
	new->next = NULL;
	int n;
	for (c = cells->next; c; c = c->next) {
		n = nneighbours(c);

		if (n < 1 || n > 3)
			c->dying = 1;

		// If one or more neighbours try to reproduce.
		if (n >= 1) {
			if (new)
				addcell(new, reproduce(c));
			else
				new = reproduce(c);
		} 
	}

	// Remove the dead.
	prev = cells;
	c = cells->next;
	while (c) {
		if (c->dying) {
			prev->next = c->next;
			free(c);
			c = prev;
		}
		prev = c;
		c = c->next;
	}

	// Add the new.
	prev->next = new->next;
	free(new);
}

void setcolor(int color) {
	XGCValues values;
	values.foreground = color;
	XChangeGC(display, gc, GCForeground, &values);
}

void render() {
	Cell *c;
	
	setcolor(0);
	XFillRectangle(display, win, gc, 0, 0, 
			width * boxwidth, height * boxheight);

	for (c = cells->next; c; c = c->next) {
		setcolor(c->color);
		XFillRectangle(display, win, gc, c->x * boxwidth, c->y * boxheight,
				boxwidth, boxheight);
	}
}

void key(XEvent *event) {
	XKeyEvent ev = event->xkey;
	char buf[32];
	int len;
	Status status;
	KeySym keysym = NoSymbol;

	len = XmbLookupString(xic, &ev, buf, sizeof(buf), &keysym, &status);
	if (status == XBufferOverflow)
		return;

	switch (keysym) {
		case XK_p:
			paused = !paused;
			break;
		case XK_q:
			quit = 1;
			break;
	}
}

void addcell(Cell *addto, Cell *n) {
	Cell *c;
	for (c = addto; c && c->next; c = c->next) ;
	c->next = n;
}

Cell *newcell(int x, int y) {
	Cell *n = malloc(sizeof(Cell));

	n->x = x;
	n->y = y;
	n->color = 1000;
	n->dying = 0;
	n->next = NULL;

	return n;
}

Cell *copycell(Cell *c) {
	Cell *n = malloc(sizeof(Cell));
	n->x = c->x;
	n->y = c->y;
	n->dying = c->dying;
	n->next = NULL;
	return n;
}

void buttonpress(XEvent *event) {
	XButtonEvent ev = event->xbutton;
	Cell *c;
	if (ev.button == Button1)
		buttonpressed = 1;
	else if (ev.button == Button3)
		buttonpressed = 10;

	if (buttonpressed) {
		c = newcell(ev.x / boxwidth, ev.y / boxheight);
		if (placetaken(cells, c)) 
			free(c);
		else 
			addcell(cells, c);
	}
}

void buttonrelease(XEvent *event) {
	buttonpressed = 0;
}

void motionnotify(XEvent *event) {
	XMotionEvent ev = event->xmotion;

	if (!buttonpressed) return;
	Cell *c = newcell(ev.x / boxwidth, ev.y / boxheight);
	if (placetaken(cells, c)) 
		free(c);
	else 
		addcell(cells, c);
}

void configurenotify(XEvent *event) {
	XConfigureEvent ev = event->xconfigure;
	printf("Resize\n");

	boxwidth = ev.width / width;
	boxheight = ev.height / height;
}

void setupwindow() {
	printf("Setting up window\n");

	XSetWindowAttributes attributes;

	display = XOpenDisplay(NULL);
	root = RootWindow(display, 0);

	attributes.event_mask = KeyPressMask | ButtonPressMask | ButtonReleaseMask 
		| PointerMotionMask | ExposureMask | StructureNotifyMask;

	printf("Creating window\n");
	win = XCreateWindow(display, root, 0, 0, 400, 400, 1,
			DefaultDepth(display, 0), CopyFromParent, CopyFromParent,
			CWEventMask,
			&attributes);

	xic = XCreateIC(XOpenIM(display, NULL, NULL, NULL), XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
			XNClientWindow, win, XNFocusWindow, win, NULL);

	gc = XCreateGC(display, win, 0, 0);

	printf("Mapping window\n");
	XMapWindow(display, win);
}

void usage(char *cmd) {
	printf("Usage %s\n", cmd);
	printf("	-r n	Adds n number of random points (default 100)\n");
	printf("	-w n	Sets number of horizontal boxs to n (default 25)\n");
	printf("	-h n	Sets number of vertical boxs to n (default 25)\n");
	printf("	-d n	Sets frame update delay to n milliseconds (default 1000)\n");
	printf("	-p	Will start paused\n");
	printf("	--help	Shows this\n");
}

int main(int argc, char *argv[]) {
	XEvent event;
	int pending, i, random;

	width = 25;
	height = 25;
	delay = 1000 * 1000;

	cells = malloc(sizeof(Cell));
	cells->next = NULL;
	cells->x = -100;
	cells->y = -100;

	buttonpressed = 0;
	paused = 0;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-w") == 0) {
			width = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-h") == 0) {
			height = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-d") == 0) {
			delay = atoi(argv[++i]) * 1000;
		} else if (strcmp(argv[i], "-p") == 0) {
			paused = 1;
		} else if (strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			exit(1);
		} else {
			printf("Unknown option '%s'\n", argv[i]);
			usage(argv[0]);
			exit(1);
		}
	}

	boxwidth = boxheight = 20;

	setupwindow();

	printf("Starting\n");

	quit = 0;
	for (i = 0; !quit; i++) {
		pending = XPending(display);
		if (!pending) {
			usleep(delay / 100);
		} else {
			XNextEvent(display, &event);
			if (handlers[event.type])
				handlers[event.type](&event);
		}	
	
		if (!paused && i % 100 == 0)
			update();
		render();
	}

	printf("Exiting\n");

	return 0;
}
