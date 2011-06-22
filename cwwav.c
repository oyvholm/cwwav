#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sndfile.h>
#include <math.h>
#include <getopt.h>

#if 0
#define dprintf(args...) fprintf(stderr, args)
#else
#define dprintf(args...)
#endif

struct waveform {
	uint32_t length;
	int16_t *samples;
};

struct waveform dit;
struct waveform dah;
struct waveform gap;

static SNDFILE *sf;

char *outfname = NULL;

/* Set before calling init */
int wpm = 25;
int frequency = 750;
int stereo = 0;
double envelope = 5.0;

/* Set by init() */
int samplerate;
int ms_per_dit;

const double pi = 3.14159265358979323844;

struct morse_code {
	uint8_t len:3;
	uint8_t code:5; // if len > 5 then code is index into extended tbl
};

static const struct morse_code morse_table[] = 
{
  { 0,0 }, {0,0}, {0,0}, {0,0}, // 00-03 Unused
  { 0,0 }, {0,0}, {0,0}, {0,0}, // 04-07 Unused
  { 0,0 }, {0,0}, {0,0}, {0,0}, // 08-11 Unused
  { 0,0 }, {0,0}, {0,0}, {0,0}, // 12-15 Unused
  { 0,0 }, {0,0}, {0,0}, {0,0}, // 16-19 Unused
  { 0,0 }, {0,0}, {0,0}, {0,0}, // 20-23 Unused
  { 0,0 }, {0,0}, {0,0}, {0,0}, // 24-27 Unused
  { 0,0 }, {0,0}, {0,0}, {0,0}, // 28-31 Unused
  { 0, 0 },    // 32: Space
  { 6, 0 },    // 33: !
  { 6, 1 },    // 34: "
  { 0, 0 },    // 35: #
  { 7, 2 },    // 36: $
  { 6, 11},    // 37: % (SK) ...-.-
  { 0, 0 },    // 38: &
  { 6, 3 },    // 39: ': .----.
  { 5, 0x16 }, // 40: (: -.--.
  { 6, 4 },    // 41: ): -.--.-
  { 0, 0 },    // 42: *
  { 5, 0x0a }, // 43: +: .-.-.
  { 6, 5 },    // 44: ,: --..--
  { 6, 6 },    // 45: -: -....-
  { 6, 7 },    // 46: .: .-.-.-
  { 5, 0x12 },  // 47: /: -..-.
  { 5, 0x1f },  // 48: 0: -----
  { 5, 0x0f },  // 49: 1: .----
  { 5, 0x07 },  // 50: 2: ..---
  { 5, 0x03 },  // 51: 3: ...--
  { 5, 0x01 },  // 52: 4: ....-
  { 5, 0x00 },  // 53: 5: .....
  { 5, 0x10 },  // 54: 6: -....
  { 5, 0x18 },  // 55: 7: --...
  { 5, 0x1c },  // 56: 8: ---..
  { 5, 0x1e },  // 57: 9: ----.
  { 6, 8 },     // 58: :: ---...
  { 0, 0 },     // 59: ;
  { 0, 0 },     // 60: <
  { 5, 0x11 },  // 61: =: -...-
  { 0, 0 },     // 62: >
  { 6, 9 },     // 63: ?: ..--..
  { 6, 10 },    // 64: @: .--.-.
  { 2, 0x01 },  // 65: a: .-
  { 4, 0x08 },  // 66: b: -...
  { 4, 0x0a },  // 67: c: -.-.
  { 3, 0x04 },  // 68: d: -..
  { 1, 0x00 },  // 69: e: .
  { 4, 0x02 },  // 70: f: ..-.
  { 3, 0x06 },  // 71: g: --.
  { 4, 0x00 },  // 72: h: ....
  { 2, 0x00 },  // 73: i: ..
  { 4, 0x07 },  // 74: j: .---
  { 3, 0x05 },  // 75: k: -.-
  { 4, 0x04 },  // 76: l: .-..
  { 2, 0x03 },  // 77: m: --
  { 2, 0x02 },  // 78: n: -.
  { 3, 0x07 },  // 79: o: ---
  { 4, 0x06 },  // 80: p: .--.
  { 4, 0x0d },  // 81: q: --.-
  { 3, 0x02 },  // 82: r: .-.
  { 3, 0x00 },  // 83: s: ...
  { 1, 0x01 },  // 84: t: -
  { 3, 0x01 },  // 85: u: ..-
  { 4, 0x01 },  // 86: v: ...-
  { 3, 0x03 },  // 87: w: .--
  { 4, 0x09 },  // 88: x: -..-
  { 4, 0x0b },  // 89: y: -.--
  { 4, 0x0c },  // 90: z: --..
};

static const uint8_t morse_extended_table[] = 
{
  0x2b, /* !: -.-.-- */
  0x12, /* ": .-..-. */
  0x09, /* $: ...-..- */
  0x1e, /* ': .----. */
  0x2d, /* ') -.--.- */
  0x33, /* .: --..-- */
  0x21, /* -: -....- */
  0x15, /* .: .-.-.- */
  0x38, /* :: ---... */
  0x0c, /* ?: ..--.. */
  0x1a, /* @: .--.-. */
  0x05, /* % (SK): ...-.- */
};


void init()
{
	int c;
	samplerate = 44100;
	ms_per_dit = 1200/wpm;
	if (ms_per_dit < 2)
		ms_per_dit = 2;
	dit.length = ceil((samplerate/1000.0)*(double)ms_per_dit);
	dit.samples = malloc(dit.length * sizeof(dit.samples[0]) * (stereo ? 2 : 1));
	dah.length = dit.length * 3;
	dah.samples = malloc(dah.length * sizeof(dah.samples[0]) * (stereo ? 2 : 1));
	gap.length = dit.length;
	gap.samples = malloc(gap.length * sizeof(gap.samples[0]) * (stereo ? 2 : 1));
	dprintf("ms/samples per dit: %d/%d dah: %d/%d\n", ms_per_dit, dit.length, 3*ms_per_dit, dah.length);
	if (dit.samples == NULL || dah.samples == NULL || gap.samples == NULL) {
		fprintf(stderr, "Error allocating buffer\n");
		exit(1);
	}
	double samples_per_cycle = samplerate/(double)frequency;
	dprintf("frequency: %d samples per cycle: %f\n", 
	       frequency, samples_per_cycle);
	/* Fill in the raw sine wave */
	for (c=0; c<dit.length; c++) {
		dit.samples[c] = 16383 * sin((c/samples_per_cycle) * pi * 2.0);
	}
	for (c=0; c<dah.length; c++) {
		dah.samples[c] = 16383 * sin((c/samples_per_cycle) * pi * 2.0);
	}
	memset(gap.samples, 0, gap.length*sizeof(gap.samples[0])*stereo?2:1);
	/* Apply envelope, 5ms sine hold rise and fall */
	double samples_in_envelope = envelope*samplerate/1000.0;
	for (c=0; c<samples_in_envelope; c++) {
		double factor = sin((pi/2.0)*(c/(double)samples_in_envelope));
		dit.samples[c] = round((double)dit.samples[c] * factor);
		dah.samples[c] = round((double)dah.samples[c] * factor);
		dit.samples[dit.length-c] = round((double)dit.samples[dit.length-c] * factor);
		dah.samples[dah.length-c] = round((double)dah.samples[dah.length-c] * factor);
	}
#if 0
	for (c=0; c<dit.length; c++)
	{
		dprintf("%d,%d\n", c, dit.samples[c]);
	}
#endif
#if 1
	if (stereo) {
		for (c=2*dit.length-3; c>=0; c-=2) {
			dit.samples[c] = dit.samples[c+1] = dit.samples[c/2];
		}
		for (c=2*dah.length-3; c>=0; c-=2) {
			dah.samples[c] = dah.samples[c+1] = dah.samples[c/2];
		}
	}
#endif
}

void output(struct waveform *v)
{
	dprintf("Output %s\n", v==&gap?"gap":v==&dit?"dit":v==&dah?"dah":"???");
	sf_writef_short(sf, v->samples, v->length);
}

void send_space(int len)
{
	int c;
	for (c=0; c<len; c++) {
		output(&gap);
	}
}

void send_char(int ch)
{
	int c,len,code;
	ch = toupper(ch);
	if (ch > 90)
		return;
	len = morse_table[ch].len;
	if (len == 0)
		return;
	code = (len > 5 ? morse_extended_table[morse_table[ch].code] : 
		morse_table[ch].code);
	dprintf("Code: %d\n", code);
	for (c=0; c<len;c++) {
		if (c>0) {
			output(&gap);
		}
		output( ((code >> (len-c-1)) & 1) ? &dah:&dit );
	}
}

void close_output()
{
	if (sf)
		sf_close(sf);
}

void setup_output()
{
	struct SF_INFO i;
	memset(&i, 0, sizeof(i));
	i.samplerate = samplerate;
	i.channels = stereo ? 2 : 1;
	i.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	if (outfname) {
		sf = sf_open(outfname, SFM_WRITE, &i);
	} else {
		sf = sf_open_fd(1, SFM_WRITE, &i, 0);
	}
	if (!sf) {
		fprintf(stderr, "sndfile open failed: %s\n", sf_strerror(NULL));
		exit(1);
	}
}

int text_to_morse(FILE *f) {
	int space = 0;
	int nl = 0;
	int c;
	
	while ((c = fgetc(f)) != EOF) {
		if (c == 13)
			continue;
		if (c == 10) {
			nl++;
			space++;
			continue;
		}
		if (isspace(c)) {
			space++;
			continue;
		}
		if (nl > 1) {
			dprintf("Paragraph break\n");
			space=0;
			send_space(7);
			send_char('=');
			send_space(4);
		}
		nl = 0;
		if (space) {
			dprintf("Space\n");
			send_space(4);
			space=0;
		}
		dprintf("C: %d %c\n", c, c);
		send_space(3);
		send_char(c);
	}
}

void print_help(const char *progname)
{
	printf("Usage: %s OPTIONS [FILENAME...]\n"
	       "Convert text into a WAV file with morse code.\n"
	       "\n"
	       "Mandatory arguments to long options are mandatory for short options too.\n"
	       "  -s, --stereo       generate stereo output (two identical channels)\n"
	       "  -o, --output       specify output file (must be supplied)\n"
	       "  -f, --frequency=N  use sidetone frequency N Hz (default: 750)\n"
	       "  -w, --wpm=N        use N words per minute (default: 25)\n"
	       "  -e, --envelope=N   envelope N ms at start/end of each tone (default=10)\n"
	       "  -h, --help         display this help and exit\n", progname);
}

int main(int argc, char *argv[]) {
	int c;
	int space,nl;
	FILE *f;

	int option_index = 0;
	static struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "stereo", no_argument, 0, 's' },
			{ "output", required_argument, 0, 'o' },
			{ "frequency", required_argument, 0, 'f' },
			{ "wpm", required_argument, 0, 'w' },
			{ "envelope", required_argument, 0, 'e' },
			{0, 0, 0, 0} };

	frequency = 750;
	wpm = 25;
	envelope = 5.0; /* ms */

	while (1) {
		int c = getopt_long(argc, argv, "hso:f:w:e:", long_options,
			&option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'h':
			print_help(argv[0]);
			exit(0);
		case 's':
			stereo = 1;
		case 'o':
			outfname = optarg;
			break;
		case 'f':
			frequency = atoi(optarg);
			break;
		case 'e':
			envelope = atof(optarg);
			break;
		case 'w':
			wpm = atoi(optarg);
			break;
		case '?':
			/* getopt_long printed the error message */
			exit(1);
		default:
			fprintf(stderr, "Unexpected error parsing options\n");
			exit(1);
		}
	}

	if (!outfname) {
		fprintf(stderr, "Error: Must specify output file with --output (try --help)\n");
		exit(1);
	}

	init();
	setup_output();

	send_space(7);
	if (optind == argc) {
		fprintf(stderr, "Processing standard input\n");
		text_to_morse(stdin);
	}
	while (optind < argc) {
		fprintf(stderr, "Processing file: %s\n", argv[optind]);
		f = fopen(argv[optind], "rb");
		if (!f) {
			fprintf(stderr, "Error opening %s: %m\n",
				argv[optind]);
			exit(1);
		}
		text_to_morse(f);
		fclose(f);
		optind++;
	}
	send_space(7);
	close_output();
	exit(0);
}
