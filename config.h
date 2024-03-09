/* See LICENSE file for copyright and license details. */

/*
 * appearance
 *
 * font: see http://freedesktop.org/software/fontconfig/fontconfig-user.html
 */
static char *font = "Liberation Mono:pixelsize=22:antialias=true:autohint=true";
static char *font2[] = { "JoyPixels:pixelsize=10:antialias=true:autohint=true" };

/* disable bold, italic and roman fonts globally */
int disablebold = 1;
int disableitalic = 0;
int disableroman = 0;

static int borderpx = 2;

/*
 * Synchronized-Update timeout in ms
 * https://gitlab.com/gnachman/iterm2/-/wikis/synchronized-updates-spec
 */
/*
In a nutshell: allow an application to suspend drawing until it has
completed some output - so that the terminal will not flicker/tear by
rendering partial content. If the end-of-suspension sequence doesn't
arrive, the terminal bails out after a timeout (default: 200 ms).

The feature is supported and pioneered by iTerm2. There are probably
very few other terminals or applications which support this feature
currently.

One notable application which does support it is tmux (master as of
2020-04-18) - where cursor flicker is completely avoided when a pane
has new content. E.g. run in one pane: `while :; do cat x.c; done'
while the cursor is at another pane.

The terminfo string `Sync' added to `st.info' is also a tmux extension
which tmux detects automatically when `st.info` is installed.

Notes:

- Draw-suspension begins on BSU sequence (Begin-Synchronized-Update),
  and ends on ESU sequence (End-Synchronized-Update).

- BSU, ESU are "\033P=1s\033\\", "\033P=2s\033\\" respectively (DCS).

- SU doesn't support nesting - BSU begins or extends, ESU always ends.

- ESU without BSU is ignored.

- BSU after BSU extends (resets the timeout), so an application could
  send BSU in a loop and keep drawing suspended - exactly like it can
  not-draw anything in a loop. But as soon as it exits/aborted then
  drawing is resumed according to the timeout even without ESU.

- This implementation focuses on ESU and doesn't really care about BSU
  in the sense that it tries hard to draw exactly once ESU arrives (if
  it's not too soon after the last draw - according to minlatency),
  and doesn't try to draw the content just up-to BSU. These two sides
  complement eachother - not-drawing on BSU increases the chance that
  ESU is not too soon after the last draw. This approach was chosen
  because the application's main focus is that ESU indicates to the
  terminal that the content is now ready - and that's when we try to
  draw.
 */
static uint su_timeout = 200;

/*
 * my ~/.Xresource defaults:
 * st.font:       Liberation Mono:pixelsize=22:antialias=true:autohint=true
 * st.alpha:      0.96
 * st.foreground: #c5c5c5
 * !! gray st.background: #2e2e26
 * st.background: #14292e
 * geometry:      194x51+690+678 (not an X resource)
 */

/*
 * What program is execed by st depends of these precedence rules:
 * 1: program passed with -e
 * 2: scroll and/or utmp
 * 3: SHELL environment variable
 * 4: value of shell in /etc/passwd
 * 5: value of shell in config.h
 */
static char *shell = "/bin/bash";
char *utmp = NULL;
/* scroll program: to enable use a string like "scroll" */
char *scroll = NULL;
char *stty_args = "stty raw pass8 nl -echo -iexten -cstopb 38400";

/* identification sequence returned in DA and DECID */
char *vtiden = "\033[?6c";

/* Kerning / character bounding-box multipliers */
static float cwscale = 1.0;
static float chscale = 1.0;

/*
 * word delimiter string
 *
 * More advanced example: L" `'\"()[]{}"
 */
wchar_t *worddelimiters = L" ";

/* selection timeouts (in milliseconds) */
static unsigned int doubleclicktimeout = 300;
static unsigned int tripleclicktimeout = 600;

/* alt screens */
int allowaltscreen = 0;

/* allow certain non-interactive (insecure) window operations such as:
   setting the clipboard text */
// mck - set this to non-zero for osc 52 to work ...
int allowwindowops = 1;

/*
 * draw latency range in ms - from new content/keypress/etc until drawing.
 * within this range, st draws when content stops arriving (idle). mostly it's
 * near minlatency, but it waits longer for slow updates to avoid partial draw.
 * low minlatency will tear/flicker more, as it can "detect" idle too early.
 */
static double minlatency = 8;
static double maxlatency = 33;

/*
 * blinking timeout (set to 0 to disable blinking) for the terminal blinking
 * attribute.
 */
static unsigned int blinktimeout = 800;

/*
 * thickness of underline and bar cursors
 */
static unsigned int cursorthickness = 2;

/*
 * 1: render most of the lines/blocks characters without using the font for
 *    perfect alignment between cells (U2500 - U259F except dashes/diagonals).
 *    Bold affects lines thickness if boxdraw_bold is not 0. Italic is ignored.
 * 0: disable (render all U25XX glyphs normally from the font).
 */
const int boxdraw = 1;
const int boxdraw_bold = 0;

/* braille (U28XX):  1: render as adjacent "pixels",  0: use font */
const int boxdraw_braille = 0;

/*
 * bell volume. It must be a value between -100 and 100. Use 0 for disabling
 * it
 */
static int bellvolume = 0;

/* default TERM value */
char *termname = "st-256color";

/*
 * spaces per tab
 *
 * When you are changing this value, don't forget to adapt the »it« value in
 * the st.info and appropriately install the st.info in the environment where
 * you use this st version.
 *
 *	it#$tabspaces,
 *
 * Secondly make sure your kernel is not expanding tabs. When running `stty
 * -a` »tab0« should appear. You can tell the terminal to not expand tabs by
 *  running following command:
 *
 *	stty tabs
 */
unsigned int tabspaces = 8;

/* bg opacity */
float alpha = 0.8;

/* Terminal colors (16 first used in escape sequence) */
static const char *colorname[] = {
	"#282828", /* hard contrast: #1d2021 / soft contrast: #32302f */
	"#cc241d",
	"#98971a",
	"#d79921",
	"#458588",
	"#b16286",
	"#689d6a",
	"#a89984",
	"#928374",
	"#fb4934",
	"#b8bb26",
	"#fabd2f",
	"#83a598",
	"#d3869b",
	"#8ec07c",
	"#ebdbb2",
	[255] = 0,
	/* more colors can be added after 255 to use with DefaultXX */
	"#add8e6", /* 256 -> cursor */
	"#555555", /* 257 -> rev cursor*/
	"#282828", /* 258 -> bg */
	"#ebdbb2", /* 259 -> fg */
};

unsigned const int ncolors = sizeof(colorname)/sizeof(*colorname);

/*
 * Default colors (colorname index)
 * foreground, background, cursor, reverse cursor
 */
unsigned int defaultfg = 259;
unsigned int defaultbg = 258;
unsigned int defaultcs = 256;
unsigned int defaultrcs = 257;

/*
 * https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-Ps-SP-q.1D81
 * Default style of cursor
 * 0: Blinking block
 * 1: Blinking block (default)
 * 2: Steady block ("█")
 * 3: Blinking underline
 * 4: Steady underline ("_")
 * 5: Blinking bar
 * 6: Steady bar ("|")
 * 7: Blinking st cursor
 * 8: Steady st cursor
  */
static unsigned int cursorstyle = 1;
static Rune stcursor = 0x2603; /* snowman (U+2603) */

/*
 * Default columns and rows numbers
 */

static unsigned int cols = 80;
static unsigned int rows = 24;

/*
 * Default colour and shape of the mouse cursor
 */
static unsigned int mouseshape = XC_xterm;
static unsigned int mousefg = 7;
static unsigned int mousebg = 0;

/*
 * Color used to display font attributes when fontconfig selected a font which
 * doesn't match the ones requested.
 */
static unsigned int defaultattr = 11;

/*
 * Force mouse select/shortcuts while mask is active (when MODE_MOUSE is set).
 * Note that if you want to use ShiftMask with selmasks, set this to an other
 * modifier, set to 0 to not use it.
 */
static uint forcemousemod = ShiftMask;

// enable for raw X shift (and alt+shift for rect and shift+double/
//     triple click for word/line) selection and copy to clipboard.
//     And shift+wheel up/down for alt screen scroll.
#define RAW_MOUSE_SEL 1

/*
 * Xresources preferences to load at startup
 */
ResourcePref resources[] = {
		{ "font",         STRING,  &font },
		{ "fontalt0",     STRING,  &font2[0] },
		{ "color0",       STRING,  &colorname[0] },
		{ "color1",       STRING,  &colorname[1] },
		{ "color2",       STRING,  &colorname[2] },
		{ "color3",       STRING,  &colorname[3] },
		{ "color4",       STRING,  &colorname[4] },
		{ "color5",       STRING,  &colorname[5] },
		{ "color6",       STRING,  &colorname[6] },
		{ "color7",       STRING,  &colorname[7] },
		{ "color8",       STRING,  &colorname[8] },
		{ "color9",       STRING,  &colorname[9] },
		{ "color10",      STRING,  &colorname[10] },
		{ "color11",      STRING,  &colorname[11] },
		{ "color12",      STRING,  &colorname[12] },
		{ "color13",      STRING,  &colorname[13] },
		{ "color14",      STRING,  &colorname[14] },
		{ "color15",      STRING,  &colorname[15] },
		{ "background",   STRING,  &colorname[258] },
		{ "foreground",   STRING,  &colorname[259] },
		{ "cursorColor",  STRING,  &colorname[256] },
		{ "termname",     STRING,  &termname },
		{ "shell",        STRING,  &shell },
		{ "minlatency",   INTEGER, &minlatency },
		{ "maxlatency",   INTEGER, &maxlatency },
		{ "blinktimeout", INTEGER, &blinktimeout },
		{ "bellvolume",   INTEGER, &bellvolume },
		{ "tabspaces",    INTEGER, &tabspaces },
		{ "borderpx",     INTEGER, &borderpx },
		{ "cwscale",      FLOAT,   &cwscale },
		{ "chscale",      FLOAT,   &chscale },
		{ "alpha",        FLOAT,   &alpha },
};

/* Internal keyboard shortcuts. */
#define MODKEY Mod1Mask
#define TERMMOD (Mod1Mask|ShiftMask)
#define CTRLMOD (ControlMask|ShiftMask)
#define CTRLALT (ControlMask|Mod1Mask)

static char *openurlcmd[] = { "/bin/sh", "-c", "st-urlhandler -o", "externalpipe", NULL };
static char *copyurlcmd[] = { "/bin/sh", "-c", "st-urlhandler -c", "externalpipe", NULL };
static char *copyoutput[] = { "/bin/sh", "-c", "st-copyout", "externalpipe", NULL };

/*
 * Internal mouse shortcuts.
 * Beware that overloading Button1 will disable the selection.
 */
static MouseShortcut mshortcuts[] = {
	/* mask                 button   function        argument       release */
#if defined(RAW_MOUSE_SEL)
	{ XK_NO_MOD,            Button4, kscrollup,      {.i = 1} },
	{ XK_NO_MOD,            Button5, kscrolldown,    {.i = 1} },
#endif

    // ----------
	// { XK_ANY_MOD,           Button2, selpaste,       {.i = 0},      1 },
	{ TERMMOD,              Button2, selpaste,       {.i = 0},      1 },
    // ----------

    // ----------
	// { CTRLMOD,              Button1, externalpipe,   {.v = openurlcmd } },
    // ----------

#if defined(RAW_MOUSE_SEL)
	{ ShiftMask,            Button4, ttysend,        {.s = "\033[5;2~"} },
	{ XK_ANY_MOD,           Button4, ttysend,        {.s = "\031"} },
	{ ShiftMask,            Button5, ttysend,        {.s = "\033[6;2~"} },
	{ XK_ANY_MOD,           Button5, ttysend,        {.s = "\005"} },
#endif
};

static Shortcut shortcuts[] = {
	/* mask                 keysym          function        argument */
	{ XK_ANY_MOD,           XK_Break,       sendbreak,      {.i =  0} },
	{ ControlMask,          XK_Print,       toggleprinter,  {.i =  0} },
	{ ShiftMask,            XK_Print,       printscreen,    {.i =  0} },
	{ XK_ANY_MOD,           XK_Print,       printsel,       {.i =  0} },

	{ TERMMOD,              XK_Num_Lock,    numlock,        {.i =  0} },

    // ----------
    /*
	{ TERMMOD,              XK_Prior,       zoom,           {.f = +1} },
	{ TERMMOD,              XK_Next,        zoom,           {.f = -1} },
	{ TERMMOD,              XK_Home,        zoomreset,      {.f =  0} },
    */
	{ CTRLMOD,              XK_plus,        zoom,           {.f = +1} },
	{ CTRLMOD,              XK_underscore,  zoom,           {.f = -1} },
	{ CTRLMOD,              XK_parenright,  zoomreset,      {.f =  0} },
    // ----------

	{ CTRLALT,              XK_v,           clippaste,      {.i =  0} },

	{ CTRLALT,              XK_c,           ttysend,        {.s = "\033[2;5~"} },

    /*
	{ CTRLALT,              XK_c,           clipcopy,       {.i =  0} },

	{ TERMMOD,              XK_C,           clipcopy,       {.i =  0} },
	{ TERMMOD,              XK_V,           clippaste,      {.i =  0} },
	{ MODKEY,               XK_c,           clipcopy,       {.i =  0} },
	{ ShiftMask,            XK_Insert,      clippaste,      {.i =  0} },
	{ MODKEY,               XK_v,           clippaste,      {.i =  0} },
	{ ShiftMask,            XK_Insert,      selpaste,       {.i =  0} },

	{ ShiftMask,            XK_Page_Up,     kscrollup,      {.i = -1} },
	{ ShiftMask,            XK_Page_Down,   kscrolldown,    {.i = -1} },
	{ MODKEY,               XK_Page_Up,     kscrollup,      {.i = -1} },
	{ MODKEY,               XK_Page_Down,   kscrolldown,    {.i = -1} },
	{ MODKEY,               XK_k,           kscrollup,      {.i =  1} },
	{ MODKEY,               XK_j,           kscrolldown,    {.i =  1} },
	{ MODKEY,               XK_Up,          kscrollup,      {.i =  1} },
	{ MODKEY,               XK_Down,        kscrolldown,    {.i =  1} },
	{ MODKEY,               XK_u,           kscrollup,      {.i = -1} },
	{ MODKEY,               XK_d,           kscrolldown,    {.i = -1} },
	{ MODKEY,       		XK_s,		    changealpha,	{.f = -0.05} },
	{ MODKEY,	        	XK_a,		    changealpha,	{.f = +0.05} },
	{ TERMMOD,              XK_Up,          zoom,           {.f = +1} },
	{ TERMMOD,              XK_Down,        zoom,           {.f = -1} },
	{ TERMMOD,              XK_K,           zoom,           {.f = +1} },
	{ TERMMOD,              XK_J,           zoom,           {.f = -1} },
	{ TERMMOD,              XK_U,           zoom,           {.f = +2} },
	{ TERMMOD,              XK_D,           zoom,           {.f = -2} },
	{ MODKEY,               XK_l,           externalpipe,   {.v = openurlcmd } },
	{ MODKEY,               XK_y,           externalpipe,   {.v = copyurlcmd } },
	{ MODKEY,               XK_o,           externalpipe,   {.v = copyoutput } },
    */
};

/*
 * Special keys (change & recompile st.info accordingly)
 *
 * Mask value:
 * * Use XK_ANY_MOD to match the key no matter modifiers state
 * * Use XK_NO_MOD to match the key alone (no modifiers)
 * appkey value:
 * * 0: no value
 * * > 0: keypad application mode enabled
 * *   = 2: term.numlock = 1
 * * < 0: keypad application mode disabled
 * appcursor value:
 * * 0: no value
 * * > 0: cursor application mode enabled
 * * < 0: cursor application mode disabled
 *
 * Be careful with the order of the definitions because st searches in
 * this table sequentially, so any XK_ANY_MOD must be in the last
 * position for a key.
 */

/*
 * If you want keys other than the X11 function keys (0xFD00 - 0xFFFF)
 * to be mapped below, add them to this array.
 */
static KeySym mappedkeys[] = {
    // ----------
    XK_i,
    XK_I,
	XK_C,
	XK_V,
	XK_X,
	XK_J,
	XK_K,
	XK_H,
	XK_L,
	XK_G,
	XK_N,
	XK_P,
	XK_space,
	XK_equal,
	XK_minus,
	XK_bar,
	XK_question,
	XK_braceleft,
	XK_braceright,
    // ----------
};

/*
 * State bits to ignore when matching key or button events.  By default,
 * numlock (Mod2Mask) and keyboard layout (XK_SWITCH_MOD) are ignored.
 */
static uint ignoremod = Mod2Mask|XK_SWITCH_MOD;

/*
 * This is the huge key array which defines all compatibility to the Linux
 * world. Please decide about changes wisely.
 */
static Key key[] = {
	/* keysym           mask            string      appkey appcursor */
	{ XK_KP_Home,       ShiftMask,      "\033[2J",       0,   -1},
	{ XK_KP_Home,       ShiftMask,      "\033[1;2H",     0,   +1},
	{ XK_KP_Home,       XK_ANY_MOD,     "\033[H",        0,   -1},
	{ XK_KP_Home,       XK_ANY_MOD,     "\033[1~",       0,   +1},
	{ XK_KP_Up,         XK_ANY_MOD,     "\033Ox",       +1,    0},
	{ XK_KP_Up,         XK_ANY_MOD,     "\033[A",        0,   -1},
	{ XK_KP_Up,         XK_ANY_MOD,     "\033OA",        0,   +1},
	{ XK_KP_Down,       XK_ANY_MOD,     "\033Or",       +1,    0},
	{ XK_KP_Down,       XK_ANY_MOD,     "\033[B",        0,   -1},
	{ XK_KP_Down,       XK_ANY_MOD,     "\033OB",        0,   +1},
	{ XK_KP_Left,       XK_ANY_MOD,     "\033Ot",       +1,    0},
	{ XK_KP_Left,       XK_ANY_MOD,     "\033[D",        0,   -1},
	{ XK_KP_Left,       XK_ANY_MOD,     "\033OD",        0,   +1},
	{ XK_KP_Right,      XK_ANY_MOD,     "\033Ov",       +1,    0},
	{ XK_KP_Right,      XK_ANY_MOD,     "\033[C",        0,   -1},
	{ XK_KP_Right,      XK_ANY_MOD,     "\033OC",        0,   +1},
	{ XK_KP_Prior,      ShiftMask,      "\033[5;2~",     0,    0},
	{ XK_KP_Prior,      XK_ANY_MOD,     "\033[5~",       0,    0},
	{ XK_KP_Begin,      XK_ANY_MOD,     "\033[E",        0,    0},
	{ XK_KP_End,        ControlMask,    "\033[J",       -1,    0},
	{ XK_KP_End,        ControlMask,    "\033[1;5F",    +1,    0},
	{ XK_KP_End,        ShiftMask,      "\033[K",       -1,    0},
	{ XK_KP_End,        ShiftMask,      "\033[1;2F",    +1,    0},
	{ XK_KP_End,        XK_ANY_MOD,     "\033[4~",       0,    0},
	{ XK_KP_Next,       ShiftMask,      "\033[6;2~",     0,    0},
	{ XK_KP_Next,       XK_ANY_MOD,     "\033[6~",       0,    0},
	{ XK_KP_Insert,     ShiftMask,      "\033[2;2~",    +1,    0},
	{ XK_KP_Insert,     ShiftMask,      "\033[4l",      -1,    0},
	{ XK_KP_Insert,     ControlMask,    "\033[L",       -1,    0},
	{ XK_KP_Insert,     ControlMask,    "\033[2;5~",    +1,    0},
	{ XK_KP_Insert,     XK_ANY_MOD,     "\033[4h",      -1,    0},
	{ XK_KP_Insert,     XK_ANY_MOD,     "\033[2~",      +1,    0},
	{ XK_KP_Delete,     ControlMask,    "\033[M",       -1,    0},
	{ XK_KP_Delete,     ControlMask,    "\033[3;5~",    +1,    0},
	{ XK_KP_Delete,     ShiftMask,      "\033[2K",      -1,    0},
	{ XK_KP_Delete,     ShiftMask,      "\033[3;2~",    +1,    0},
	{ XK_KP_Delete,     XK_ANY_MOD,     "\033[P",       -1,    0},
	{ XK_KP_Delete,     XK_ANY_MOD,     "\033[3~",      +1,    0},
	{ XK_KP_Multiply,   XK_ANY_MOD,     "\033Oj",       +2,    0},
	{ XK_KP_Add,        XK_ANY_MOD,     "\033Ok",       +2,    0},
	{ XK_KP_Enter,      XK_ANY_MOD,     "\033OM",       +2,    0},
	{ XK_KP_Enter,      XK_ANY_MOD,     "\r",           -1,    0},
	{ XK_KP_Subtract,   XK_ANY_MOD,     "\033Om",       +2,    0},
	{ XK_KP_Decimal,    XK_ANY_MOD,     "\033On",       +2,    0},
	{ XK_KP_Divide,     XK_ANY_MOD,     "\033Oo",       +2,    0},
	{ XK_KP_0,          XK_ANY_MOD,     "\033Op",       +2,    0},
	{ XK_KP_1,          XK_ANY_MOD,     "\033Oq",       +2,    0},
	{ XK_KP_2,          XK_ANY_MOD,     "\033Or",       +2,    0},
	{ XK_KP_3,          XK_ANY_MOD,     "\033Os",       +2,    0},
	{ XK_KP_4,          XK_ANY_MOD,     "\033Ot",       +2,    0},
	{ XK_KP_5,          XK_ANY_MOD,     "\033Ou",       +2,    0},
	{ XK_KP_6,          XK_ANY_MOD,     "\033Ov",       +2,    0},
	{ XK_KP_7,          XK_ANY_MOD,     "\033Ow",       +2,    0},
	{ XK_KP_8,          XK_ANY_MOD,     "\033Ox",       +2,    0},
	{ XK_KP_9,          XK_ANY_MOD,     "\033Oy",       +2,    0},

	{ XK_Up,            ShiftMask,      "\033[1;2A",     0,    0},
	{ XK_Up,            Mod1Mask,       "\033[1;3A",     0,    0},
	{ XK_Up,         ShiftMask|Mod1Mask,"\033[1;4A",     0,    0},
	{ XK_Up,            ControlMask,    "\033[1;5A",     0,    0},
	{ XK_Up,      ShiftMask|ControlMask,"\033[1;6A",     0,    0},
	{ XK_Up,       ControlMask|Mod1Mask,"\033[1;7A",     0,    0},
	{ XK_Up,ShiftMask|ControlMask|Mod1Mask,"\033[1;8A",  0,    0},
	{ XK_Up,            XK_ANY_MOD,     "\033[A",        0,   -1},
	{ XK_Up,            XK_ANY_MOD,     "\033OA",        0,   +1},

	{ XK_Down,          ShiftMask,      "\033[1;2B",     0,    0},
	{ XK_Down,          Mod1Mask,       "\033[1;3B",     0,    0},
	{ XK_Down,       ShiftMask|Mod1Mask,"\033[1;4B",     0,    0},
	{ XK_Down,          ControlMask,    "\033[1;5B",     0,    0},
	{ XK_Down,    ShiftMask|ControlMask,"\033[1;6B",     0,    0},
	{ XK_Down,     ControlMask|Mod1Mask,"\033[1;7B",     0,    0},
	{ XK_Down,ShiftMask|ControlMask|Mod1Mask,"\033[1;8B",0,    0},
	{ XK_Down,          XK_ANY_MOD,     "\033[B",        0,   -1},
	{ XK_Down,          XK_ANY_MOD,     "\033OB",        0,   +1},

	{ XK_Left,          ShiftMask,      "\033[1;2D",     0,    0},
	{ XK_Left,          Mod1Mask,       "\033[1;3D",     0,    0},
	{ XK_Left,       ShiftMask|Mod1Mask,"\033[1;4D",     0,    0},
	{ XK_Left,          ControlMask,    "\033[1;5D",     0,    0},
	{ XK_Left,    ShiftMask|ControlMask,"\033[1;6D",     0,    0},
	{ XK_Left,     ControlMask|Mod1Mask,"\033[1;7D",     0,    0},
	{ XK_Left,ShiftMask|ControlMask|Mod1Mask,"\033[1;8D",0,    0},
	{ XK_Left,          XK_ANY_MOD,     "\033[D",        0,   -1},
	{ XK_Left,          XK_ANY_MOD,     "\033OD",        0,   +1},

	{ XK_Right,         ShiftMask,      "\033[1;2C",     0,    0},
	{ XK_Right,         Mod1Mask,       "\033[1;3C",     0,    0},
	{ XK_Right,      ShiftMask|Mod1Mask,"\033[1;4C",     0,    0},
	{ XK_Right,         ControlMask,    "\033[1;5C",     0,    0},
	{ XK_Right,   ShiftMask|ControlMask,"\033[1;6C",     0,    0},
	{ XK_Right,    ControlMask|Mod1Mask,"\033[1;7C",     0,    0},
	{ XK_Right,ShiftMask|ControlMask|Mod1Mask,"\033[1;8C",0,   0},
	{ XK_Right,         XK_ANY_MOD,     "\033[C",        0,   -1},
	{ XK_Right,         XK_ANY_MOD,     "\033OC",        0,   +1},

	{ XK_ISO_Left_Tab,  ShiftMask,      "\033[Z",        0,    0},

 // { XK_ISO_Left_Tab,  ShiftMask,             "\033[27;1;9~",  0,    0},
 // { XK_ISO_Left_Tab,  ControlMask,           "\033[27;5;9~",  0,    0},
 // { XK_ISO_Left_Tab,  ShiftMask|ControlMask, "\033[27;6;9~",  0,    0},

    { XK_Tab,           ShiftMask,      "\033[Z",        0,    0},
    { XK_Tab,           ControlMask,    "\033[27;5;9~",  0,    0},

	{ XK_Return,        Mod1Mask,       "\033\r",        0,    0},
    // ----------
	{ XK_Return,        ControlMask|Mod1Mask, "\036\015",0,    0},
    // ----------
	{ XK_Return,        XK_ANY_MOD,      "\r",           0,    0},

    // ----------
	{ XK_Insert,           ShiftMask,      "\033[2;2~",     0,    0},
	{ XK_Insert,           Mod1Mask,       "\033[2;3~",     0,    0},
	{ XK_Insert,       ShiftMask|Mod1Mask, "\033[2;4~",     0,    0},
	{ XK_Insert,           ControlMask,    "\033[2;5~",     0,    0},
	{ XK_Insert,    ShiftMask|ControlMask, "\033[2;6~",     0,    0},
	{ XK_Insert,     ControlMask|Mod1Mask, "\033[2;7~",     0,    0},
    { XK_Insert, ShiftMask|ControlMask|Mod1Mask,"\033[2;8~",0,    0},
    // ----------
	{ XK_Insert,        ShiftMask,      "\033[4l",      -1,    0},
	{ XK_Insert,        ShiftMask,      "\033[2;2~",    +1,    0},
	{ XK_Insert,        ControlMask,    "\033[L",       -1,    0},
	{ XK_Insert,        ControlMask,    "\033[2;5~",    +1,    0},
	{ XK_Insert,        XK_ANY_MOD,     "\033[4h",      -1,    0},
	{ XK_Insert,        XK_ANY_MOD,     "\033[2~",      +1,    0},

    // ----------
	{ XK_Delete,           ShiftMask,      "\033[3;2~",     0,    0},
	{ XK_Delete,           Mod1Mask,       "\033[3;3~",     0,    0},
	{ XK_Delete,       ShiftMask|Mod1Mask, "\033[3;4~",     0,    0},
	{ XK_Delete,           ControlMask,    "\033[3;5~",     0,    0},
	{ XK_Delete,    ShiftMask|ControlMask, "\033[3;6~",     0,    0},
	{ XK_Delete,     ControlMask|Mod1Mask, "\033[3;7~",     0,    0},
    { XK_Delete, ShiftMask|ControlMask|Mod1Mask,"\033[3;8~",0,    0},
    // ----------
	{ XK_Delete,        ControlMask,    "\033[M",       -1,    0},
	{ XK_Delete,        ControlMask,    "\033[3;5~",    +1,    0},
	{ XK_Delete,        ShiftMask,      "\033[2K",      -1,    0},
	{ XK_Delete,        ShiftMask,      "\033[3;2~",    +1,    0},
	{ XK_Delete,        XK_ANY_MOD,     "\033[P",       -1,    0},
	{ XK_Delete,        XK_ANY_MOD,     "\033[3~",      +1,    0},

	{ XK_BackSpace,     XK_NO_MOD,      "\177",          0,    0},
    // ----------
	{ XK_BackSpace,     Mod1Mask,       "\033\177",      0,    0},
	{ XK_BackSpace,     ShiftMask,      "\036\010",      0,    0},
	{ XK_BackSpace,     ControlMask,    "\036\177",      0,    0},
	{ XK_BackSpace,     ControlMask|ShiftMask,"\037\177",0,    0},
	{ XK_BackSpace,     ShiftMask|Mod1Mask,   "\033\100",0,    0},
	// { XK_BackSpace,     ShiftMask|Mod1Mask,   "",0,    0},
	{ XK_BackSpace,     ControlMask|Mod1Mask, "\033\100",0,    0},
    // ----------

    // ----------
	{ XK_bar,           ControlMask|ShiftMask, "\037\134", 0,    0},
	// { XK_backslash,     ControlMask|Mod1Mask,  "\000",     0,    0},
	// { XK_backslash,     ControlMask|Mod1Mask,  "",     0,    0},
	// send ctrl-s + ] (\x13\x5d) tmux copy-mode
	// { XK_backslash,     ControlMask|Mod1Mask,  "\023\135", 0,    0},
    // ----------

    // ----------
	{ XK_i,             ControlMask,           "\036\011", 0,    0},
	{ XK_I,             ShiftMask|ControlMask, "\033[Z",   0,    0},
    // ----------

    // ----------
    // send std C-Insert esc code, as sometimes cannot discern the Shift in C-S-<letter> ...
	{ XK_C,             ControlMask|ShiftMask, "\033[2;5~",0,    0},
    // send std S-Insert esc code, as sometimes cannot discern the Shift in C-S-<letter> ...
	{ XK_V,             ControlMask|ShiftMask, "\033[2;2~",0,    0},
    // ----------

    // ----------
	{ XK_braceleft,     ControlMask|ShiftMask, "\037\133", 0,    0},
	// { XK_bracketleft,   ControlMask|Mod1Mask,  "",     0,    0},
	{ XK_braceright,    ControlMask|ShiftMask, "\037\135", 0,    0},
	// { XK_bracketright,  ControlMask|Mod1Mask,  "",     0,    0},
    // ----------

	{ XK_question,      ControlMask|ShiftMask, "\037\057", 0,    0},

    // ----------
	// { XK_space,         Mod1Mask,         "\033\040",      0,    0},
	// { XK_space,         Mod1Mask,         "",      0,    0},
	{ XK_space,         ShiftMask,        "\036\040",      0,    0},
	{ XK_space,         ControlMask|ShiftMask, "\037\040", 0,    0},
	{ XK_space,         ShiftMask|Mod1Mask,    "\033\043", 0,    0},
	// { XK_space,         ShiftMask|Mod1Mask,    "", 0,    0},
	{ XK_space,         ControlMask|Mod1Mask,  "\033\043", 0,    0},
    // ----------

    // ----------
	{ XK_X,             ControlMask|ShiftMask, "\033\170", 0,    0},
    // ----------

    // ----------
	{ XK_J,             ControlMask|ShiftMask, "\037\112", 0,    0},
	{ XK_K,             ControlMask|ShiftMask, "\037\113", 0,    0},
	{ XK_H,             ControlMask|ShiftMask, "\037\110", 0,    0},
	{ XK_L,             ControlMask|ShiftMask, "\037\114", 0,    0},
    // ----------

    // ----------
	{ XK_G,             ControlMask|ShiftMask, "\037\107", 0,    0},
	{ XK_N,             ControlMask|ShiftMask, "\037\116", 0,    0},
	{ XK_P,             ControlMask|ShiftMask, "\037\120", 0,    0},
    // ----------

	{ XK_equal,         ControlMask,    "\036\075",      0,    0},
	// { XK_equal,         Mod1Mask,       "\075",          0,    0},
	{ XK_equal,    ControlMask|Mod1Mask, "\033\043",     0,    0},

	{ XK_minus,         ControlMask,    "\036\055",      0,    0},
	{ XK_minus,    ControlMask|Mod1Mask, "",             0,    0},

    // ----------
	{ XK_Home,          ShiftMask,      "\033[1;2H",     0,    0},
	{ XK_Home,          Mod1Mask,       "\033[1;3H",     0,    0},
	{ XK_Home,      ShiftMask|Mod1Mask, "\033[1;4H",     0,    0},
	{ XK_Home,          ControlMask,    "\033[1;5H",     0,    0},
	{ XK_Home,   ShiftMask|ControlMask, "\033[1;6H",     0,    0},
	{ XK_Home,    ControlMask|Mod1Mask, "\033[1;7H",     0,    0},
    { XK_Home,ShiftMask|ControlMask|Mod1Mask,"\033[1;8H",0,    0},
    // ----------
	{ XK_Home,          ShiftMask,      "\033[2J",       0,   -1},
	{ XK_Home,          XK_ANY_MOD,     "\033[H",        0,   -1},
	{ XK_Home,          XK_ANY_MOD,     "\033[1~",       0,   +1},

    // ----------
	{ XK_End,           ShiftMask,      "\033[1;2F",     0,    0},
	{ XK_End,           Mod1Mask,       "\033[1;3F",     0,    0},
	{ XK_End,       ShiftMask|Mod1Mask, "\033[1;4F",     0,    0},
	{ XK_End,           ControlMask,    "\033[1;5F",     0,    0},
	{ XK_End,    ShiftMask|ControlMask, "\033[1;6F",     0,    0},
	{ XK_End,     ControlMask|Mod1Mask, "\033[1;7F",     0,    0},
    { XK_End, ShiftMask|ControlMask|Mod1Mask,"\033[1;8F",0,    0},
    // ----------
	{ XK_End,           ShiftMask,      "\033[K",       -1,    0},
	{ XK_End,           ControlMask,    "\033[J",       -1,    0},
	{ XK_End,           XK_ANY_MOD,     "\033[4~",       0,    0},

    // ----------
	{ XK_Prior,           ShiftMask,      "\033[5;2~",     0,    0},
	{ XK_Prior,           Mod1Mask,       "\033[5;3~",     0,    0},
	{ XK_Prior,       ShiftMask|Mod1Mask, "\033[5;4~",     0,    0},
	{ XK_Prior,           ControlMask,    "\033[5;5~",     0,    0},
	{ XK_Prior,    ShiftMask|ControlMask, "\033[5;6~",     0,    0},
	{ XK_Prior,     ControlMask|Mod1Mask, "\033[5;7~",     0,    0},
    { XK_Prior, ShiftMask|ControlMask|Mod1Mask,"\033[5;8~",0,    0},
    // ----------
	{ XK_Prior,           XK_ANY_MOD,     "\033[5~",       0,    0},

    // ----------
	{ XK_Next,            ShiftMask,      "\033[6;2~",     0,    0},
	{ XK_Next,            Mod1Mask,       "\033[6;3~",     0,    0},
	{ XK_Next,        ShiftMask|Mod1Mask, "\033[6;4~",     0,    0},
	{ XK_Next,            ControlMask,    "\033[6;5~",     0,    0},
	{ XK_Next,     ShiftMask|ControlMask, "\033[6;6~",     0,    0},
	{ XK_Next,      ControlMask|Mod1Mask, "\033[6;7~",     0,    0},
    { XK_Next,  ShiftMask|ControlMask|Mod1Mask,"\033[6;8~",0,    0},
    // ----------
	{ XK_Next,          XK_ANY_MOD,     "\033[6~",       0,    0},

	{ XK_F1,            XK_NO_MOD,      "\033OP" ,       0,    0},
	{ XK_F1, /* F13 */  ShiftMask,      "\033[1;2P",     0,    0},
	{ XK_F1, /* F25 */  ControlMask,    "\033[1;5P",     0,    0},
	{ XK_F1, /* F37 */  Mod4Mask,       "\033[1;6P",     0,    0},
	{ XK_F1, /* F49 */  Mod1Mask,       "\033[1;3P",     0,    0},
	{ XK_F1, /* F61 */  Mod3Mask,       "\033[1;4P",     0,    0},
	{ XK_F2,            XK_NO_MOD,      "\033OQ" ,       0,    0},
	{ XK_F2, /* F14 */  ShiftMask,      "\033[1;2Q",     0,    0},
	{ XK_F2, /* F26 */  ControlMask,    "\033[1;5Q",     0,    0},
	{ XK_F2, /* F38 */  Mod4Mask,       "\033[1;6Q",     0,    0},
	{ XK_F2, /* F50 */  Mod1Mask,       "\033[1;3Q",     0,    0},
	{ XK_F2, /* F62 */  Mod3Mask,       "\033[1;4Q",     0,    0},
	{ XK_F3,            XK_NO_MOD,      "\033OR" ,       0,    0},
	{ XK_F3, /* F15 */  ShiftMask,      "\033[1;2R",     0,    0},
	{ XK_F3, /* F27 */  ControlMask,    "\033[1;5R",     0,    0},
	{ XK_F3, /* F39 */  Mod4Mask,       "\033[1;6R",     0,    0},
	{ XK_F3, /* F51 */  Mod1Mask,       "\033[1;3R",     0,    0},
	{ XK_F3, /* F63 */  Mod3Mask,       "\033[1;4R",     0,    0},
	{ XK_F4,            XK_NO_MOD,      "\033OS" ,       0,    0},
	{ XK_F4, /* F16 */  ShiftMask,      "\033[1;2S",     0,    0},
	{ XK_F4, /* F28 */  ControlMask,    "\033[1;5S",     0,    0},
	{ XK_F4, /* F40 */  Mod4Mask,       "\033[1;6S",     0,    0},
	{ XK_F4, /* F52 */  Mod1Mask,       "\033[1;3S",     0,    0},
	{ XK_F5,            XK_NO_MOD,      "\033[15~",      0,    0},
	{ XK_F5, /* F17 */  ShiftMask,      "\033[15;2~",    0,    0},
	{ XK_F5, /* F29 */  ControlMask,    "\033[15;5~",    0,    0},
	{ XK_F5, /* F41 */  Mod4Mask,       "\033[15;6~",    0,    0},
	{ XK_F5, /* F53 */  Mod1Mask,       "\033[15;3~",    0,    0},
	{ XK_F6,            XK_NO_MOD,      "\033[17~",      0,    0},
	{ XK_F6, /* F18 */  ShiftMask,      "\033[17;2~",    0,    0},
	{ XK_F6, /* F30 */  ControlMask,    "\033[17;5~",    0,    0},
	{ XK_F6, /* F42 */  Mod4Mask,       "\033[17;6~",    0,    0},
	{ XK_F6, /* F54 */  Mod1Mask,       "\033[17;3~",    0,    0},
	{ XK_F7,            XK_NO_MOD,      "\033[18~",      0,    0},
	{ XK_F7, /* F19 */  ShiftMask,      "\033[18;2~",    0,    0},
	{ XK_F7, /* F31 */  ControlMask,    "\033[18;5~",    0,    0},
	{ XK_F7, /* F43 */  Mod4Mask,       "\033[18;6~",    0,    0},
	{ XK_F7, /* F55 */  Mod1Mask,       "\033[18;3~",    0,    0},
	{ XK_F8,            XK_NO_MOD,      "\033[19~",      0,    0},
	{ XK_F8, /* F20 */  ShiftMask,      "\033[19;2~",    0,    0},
	{ XK_F8, /* F32 */  ControlMask,    "\033[19;5~",    0,    0},
	{ XK_F8, /* F44 */  Mod4Mask,       "\033[19;6~",    0,    0},
	{ XK_F8, /* F56 */  Mod1Mask,       "\033[19;3~",    0,    0},
	{ XK_F9,            XK_NO_MOD,      "\033[20~",      0,    0},
	{ XK_F9, /* F21 */  ShiftMask,      "\033[20;2~",    0,    0},
	{ XK_F9, /* F33 */  ControlMask,    "\033[20;5~",    0,    0},
	{ XK_F9, /* F45 */  Mod4Mask,       "\033[20;6~",    0,    0},
	{ XK_F9, /* F57 */  Mod1Mask,       "\033[20;3~",    0,    0},
	{ XK_F10,           XK_NO_MOD,      "\033[21~",      0,    0},
	{ XK_F10, /* F22 */ ShiftMask,      "\033[21;2~",    0,    0},
	{ XK_F10, /* F34 */ ControlMask,    "\033[21;5~",    0,    0},
	{ XK_F10, /* F46 */ Mod4Mask,       "\033[21;6~",    0,    0},
	{ XK_F10, /* F58 */ Mod1Mask,       "\033[21;3~",    0,    0},
	{ XK_F11,           XK_NO_MOD,      "\033[23~",      0,    0},
	{ XK_F11, /* F23 */ ShiftMask,      "\033[23;2~",    0,    0},
	{ XK_F11, /* F35 */ ControlMask,    "\033[23;5~",    0,    0},
	{ XK_F11, /* F47 */ Mod4Mask,       "\033[23;6~",    0,    0},
	{ XK_F11, /* F59 */ Mod1Mask,       "\033[23;3~",    0,    0},
	{ XK_F12,           XK_NO_MOD,      "\033[24~",      0,    0},
	{ XK_F12, /* F24 */ ShiftMask,      "\033[24;2~",    0,    0},
	{ XK_F12, /* F36 */ ControlMask,    "\033[24;5~",    0,    0},
	{ XK_F12, /* F48 */ Mod4Mask,       "\033[24;6~",    0,    0},
	{ XK_F12, /* F60 */ Mod1Mask,       "\033[24;3~",    0,    0},
	{ XK_F13,           XK_NO_MOD,      "\033[1;2P",     0,    0},
	{ XK_F14,           XK_NO_MOD,      "\033[1;2Q",     0,    0},
	{ XK_F15,           XK_NO_MOD,      "\033[1;2R",     0,    0},
	{ XK_F16,           XK_NO_MOD,      "\033[1;2S",     0,    0},
	{ XK_F17,           XK_NO_MOD,      "\033[15;2~",    0,    0},
	{ XK_F18,           XK_NO_MOD,      "\033[17;2~",    0,    0},
	{ XK_F19,           XK_NO_MOD,      "\033[18;2~",    0,    0},
	{ XK_F20,           XK_NO_MOD,      "\033[19;2~",    0,    0},
	{ XK_F21,           XK_NO_MOD,      "\033[20;2~",    0,    0},
	{ XK_F22,           XK_NO_MOD,      "\033[21;2~",    0,    0},
	{ XK_F23,           XK_NO_MOD,      "\033[23;2~",    0,    0},
	{ XK_F24,           XK_NO_MOD,      "\033[24;2~",    0,    0},
	{ XK_F25,           XK_NO_MOD,      "\033[1;5P",     0,    0},
	{ XK_F26,           XK_NO_MOD,      "\033[1;5Q",     0,    0},
	{ XK_F27,           XK_NO_MOD,      "\033[1;5R",     0,    0},
	{ XK_F28,           XK_NO_MOD,      "\033[1;5S",     0,    0},
	{ XK_F29,           XK_NO_MOD,      "\033[15;5~",    0,    0},
	{ XK_F30,           XK_NO_MOD,      "\033[17;5~",    0,    0},
	{ XK_F31,           XK_NO_MOD,      "\033[18;5~",    0,    0},
	{ XK_F32,           XK_NO_MOD,      "\033[19;5~",    0,    0},
	{ XK_F33,           XK_NO_MOD,      "\033[20;5~",    0,    0},
	{ XK_F34,           XK_NO_MOD,      "\033[21;5~",    0,    0},
	{ XK_F35,           XK_NO_MOD,      "\033[23;5~",    0,    0},
};

/*
 * Selection types' masks.
 * Use the same masks as usual.
 * Button1Mask is always unset, to make masks match between ButtonPress.
 * ButtonRelease and MotionNotify.
 * If no match is found, regular selection is used.
 */
static uint selmasks[] = {
	[SEL_RECTANGULAR] = ControlMask,
};

/*
 * Printable characters in ASCII, used to estimate the advance width
 * of single wide characters.
 */
static char ascii_printable[] =
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~";
