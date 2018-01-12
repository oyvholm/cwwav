/*
 * cwwav - a morse code generator
 * Copyright (C) 2011 by Thomas Horsten
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sndfile.h>
#include <math.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_LAME
#include <lame/lame.h>
#endif
#include <locale.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>

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
struct waveform f_gap;

static SNDFILE *sf;
int outfd;
#ifdef HAVE_LAME
static lame_global_flags *lame_flags;
static char *mp3buf;
static int mp3buf_size;
#endif

char *outfname = NULL;

/* Set before calling init */
double wpm = 25.0;
double farnsworth_wpm = 0.0;
double phase_shift = 0.0;
int frequency = 660;
int stereo = 0;
double envelope = 10.0;
int samplerate = 16000;

/* Set by init() */
double ms_per_dit;

const double pi = 3.14159265358979323844;

enum {
	FORMAT_WAV = 1,
#ifdef HAVE_LAME
	FORMAT_MP3 = 2,
#endif
} output_format;

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
  { 5, 0x15 }, // 42: *: (CT) -.-.-
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
  { 6, 12 },     // 59: ;: -.-.-.
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
  { 0, 0 },     // 91: [
  { 0, 0 },     // 92: backslash
  { 0, 0 },     // 93: ]
  { 0, 0 },     // 94: ^
  { 6, 13 },    // 95: _: : ..__._

  // Extended UTF-8 characters translated in send_char()
  { 4, 0x05 },  // 96: Æ/Ä: ._._
  { 4, 0x0e },  // 97: Ø/Ö/Ó: ___.
  { 5, 0x0d },  // 98: Å,À: .__._
  { 5, 0x14 },  // 99: Ç: _._..
  { 4, 0x03 },  // 100: Ü: ..--
  { 5, 0x04 },  // 101: É: ..-..
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
  0x2a, /* ;: -.-.-. */
  0x0d, /* _: ..__._ */
};


void init()
{
	uint32_t c;
	int channels = stereo ? 2 : 1;
	double f_ta, f_tb;
	ms_per_dit = 1200.0/wpm;
	if (ms_per_dit < 2.0)
		ms_per_dit = 2.0;
	dit.length = ceil((samplerate/1000.0)*ms_per_dit);
	dit.samples = malloc(dit.length * sizeof(dit.samples[0]) * channels);
	dah.length = dit.length * 3;
	dah.samples = malloc(dah.length * sizeof(dah.samples[0]) * channels);
	gap.length = dit.length;
	gap.samples = malloc(gap.length * sizeof(gap.samples[0]) * channels);
	memset(gap.samples, 0, gap.length*sizeof(gap.samples[0])*channels);
	if (farnsworth_wpm != 0.0) {
		f_ta = 1000.0 * ((60.0*wpm - 37.2*farnsworth_wpm) / (farnsworth_wpm*wpm));
		// Not used: f_tc = (3.0*f_ta)/19.0;
		// Not used: f_tw = (7.0*f_ta)/19.0;
		f_tb = f_ta/19.0; // Farnsworth base delay
		//printf("f_tb: %f\n", f_tb);
		f_gap.length = ceil((samplerate/1000.0)*f_tb);
		f_gap.samples = malloc(f_gap.length * sizeof(f_gap.samples[0]) * channels);
		memset(f_gap.samples, 0, f_gap.length*sizeof(f_gap.samples[0])*channels);
	}
	dprintf("ms/samples per dit: %f/%d dah: %f/%d\n", ms_per_dit, dit.length, 3*ms_per_dit, dah.length);
	if (dit.samples == NULL || dah.samples == NULL || gap.samples == NULL) {
		fprintf(stderr, "Error allocating buffer\n");
		exit(1);
	}
	double samples_per_cycle = samplerate/(double)frequency;
	dprintf("frequency: %d samples per cycle: %f\n",
	       frequency, samples_per_cycle);
	/* Fill in the raw sine wave */
	for (c=0; c<dit.length; c++) {
		dit.samples[c*channels] = 32767 * sin((c/samples_per_cycle) * pi * 2.0);
		if (stereo)
			dit.samples[c*channels+1] = 32767 * sin(((c/samples_per_cycle) * pi * 2.0) + phase_shift);
	}
	for (c=0; c<dah.length; c++) {
		dah.samples[c*channels] = 32767 * sin((c/samples_per_cycle) * pi * 2.0);
		if (stereo)
			dah.samples[c*channels+1] = 32767 * sin(((c/samples_per_cycle) * pi * 2.0) + phase_shift);
	}

	/* Apply envelope, variable raised cosine rise and fall */
	double samples_in_envelope = envelope*samplerate/1000.0;
	if (samples_in_envelope*2 > dit.length) {
		fprintf(stderr, "Envelope cannot be longer than half a dit length.\n");
		exit(1);
	}
	for (c=0; c<samples_in_envelope; c++) {
		double factor = sin((pi/2.0)*(c/(double)samples_in_envelope));
		dit.samples[c*channels] = round((double)dit.samples[c*channels] * factor);
		dah.samples[c*channels] = round((double)dah.samples[c*channels] * factor);
		dit.samples[(dit.length-c)*channels] = round((double)dit.samples[(dit.length-c)*channels] * factor);
		dah.samples[(dah.length-c)*channels] = round((double)dah.samples[(dah.length-c)*channels] * factor);
		if (stereo) {
			dit.samples[c*channels+1] = round((double)dit.samples[c*channels+1] * factor);
			dah.samples[c*channels+1] = round((double)dah.samples[c*channels+1] * factor);
			dit.samples[(dit.length-c)*channels+1] = round((double)dit.samples[(dit.length-c)*channels+1] * factor);
			dah.samples[(dah.length-c)*channels+1] = round((double)dah.samples[(dah.length-c)*channels+1] * factor);
		}
	}
#if 0
	for (c=0; c<dit.length; c++)
	{
		dprintf("%d,%d\n", c, dit.samples[c]);
	}
#endif
}

void output(struct waveform *v)
{
	dprintf("Output %s\n", v==&gap?"gap":v==&dit?"dit":v==&dah?"dah":"???");
	if (output_format == FORMAT_WAV) {
		sf_writef_short(sf, v->samples, v->length);
	}
#ifdef HAVE_LAME
	else if (output_format == FORMAT_MP3) {
		int r;
		if (!stereo) {
			r = lame_encode_buffer(lame_flags,
			                       v->samples,
			                       NULL,
			                       v->length,
			                       mp3buf,
			                       mp3buf_size);
		} else {
			r = lame_encode_buffer_interleaved(lame_flags,
			                                   v->samples,
			                                   v->length,
			                                   mp3buf,
			                                   mp3buf_size);
		}
		if (r < 0) {
			fprintf(stderr, "LAME encoding error: %d\n", r);
			exit(1);
		} else  if (r > 0) {
			r = write(outfd, mp3buf, r);
			if (r < 0) {
				fprintf(stderr, "Write failed: %s\n", strerror(errno));
				exit(1);
			}
		}
	}
#endif
}

void send_space(int len)
{
	int c;
	for (c=0; c<len; c++) {
		output(&gap);
	}
}

void send_fspace(int len)
{
	int c;
	for (c=0; c<len; c++) {
		output(&f_gap);
	}
}

wint_t translate_utf(wint_t ch)
{
	switch (ch) {
	case 0x00a0: // " ", non-breaking space
		ch = ' ';
		break;
	case 0x00ab:
		ch = '"'; // «
		break;
	case 0x00bb:
		ch = '"'; // »
		break;
	case 0x00c6: // Æ
	case 0x00c4: // Ä
		ch = 96;
		break;
	case 0xd8: // Ø
	case 0xd6: // Ö
	case 0xd3: // Ó
		ch = 97;
		break;
	case 0xc5: // Å
	case 0xc0: // À
		ch = 98;
		break;
	case 0xc7: // Ç
		ch = 99;
		break;
	case 0xdc: // Ü
		ch = 100;
		break;
	case 0xc9: // É
		ch = 101;
		break;

	// Transformed to lookalikes:
	case 0xc1: // Á
		ch = 'A';
		break;
	case 0xcf: // Ï
		ch = 'I';
		break;
	case 0xcb: // Ë
		ch = 'E';
		break;
	case 0x2019: // Another apostrophe
		ch = '\'';
		break;
	}
	return ch;
}

void send_char(wint_t ch)
{
	int c,len,code;
	wint_t ch1;
	ch = towupper(ch);
	if (ch > 95) {
		ch1 = translate_utf(ch);
		if (ch == ch1) {
			fprintf(stderr, "Unknown character '%lc' (%d)\n", ch, ch);
			return;
		}
		ch = ch1;
	}
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
	if (output_format == FORMAT_WAV) {
		if (sf) {
			sf_close(sf);
		}
	}
#ifdef HAVE_LAME
	else if (output_format == FORMAT_MP3) {
		int r;
		r = lame_encode_flush(lame_flags, mp3buf, mp3buf_size);
		if (r < 0) {
			fprintf(stderr, "LAME encoding error: %d\n", r);
			exit(1);
		} else  if (r > 0) {
			r = write(outfd, mp3buf, r);
			if (r < 0) {
				fprintf(stderr, "Write failed: %s\n", strerror(errno));
				exit(1);
			}
		}
		free(mp3buf);
		mp3buf = NULL;
		mp3buf_size = 0;
	}
#endif
}

void setup_output()
{
	if (output_format == FORMAT_WAV) {
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
#ifdef HAVE_LAME
	else if (output_format == FORMAT_MP3) {
		if (outfname && strcmp(outfname, "-")) {
			outfd = open(outfname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if (outfd < 1) {
				fprintf(stderr, "Open output failed: %s\n", strerror(errno));
				exit(1);
			}
		} else {
			outfd = 1;
		}
		lame_flags = lame_init();
		if (!lame_flags) {
			fprintf(stderr, "LAME init failed\n");
			exit(1);
		}
		lame_set_in_samplerate(lame_flags, samplerate);
		lame_set_num_channels(lame_flags, stereo ? 2 : 1);
		lame_set_mode(lame_flags, stereo ? STEREO : MONO);
		lame_set_quality(lame_flags, 9);
		lame_set_VBR(lame_flags, vbr_off);
		lame_set_preset(lame_flags, stereo ? 32 : 16);
		//lame_set_bWriteVbrTag(lame_flags, 1);
		if ( 0 > lame_init_params(lame_flags)) {
			fprintf(stderr, "LAME init params failed\n");
		}
		mp3buf_size = (int)ceil(1.25*dit.length*(stereo ? 2 : 1)) + 7200;
		mp3buf = malloc(mp3buf_size);
		if (!mp3buf) {
			fprintf(stderr, "Failed to allocate mp3 buffer\n");
			exit(1);
		}
	}
#endif
}
void text_to_morse(FILE *f) {
	int space = 0;
	int nl = 0;
	wint_t c;

	while ((c = fgetwc(f)) != WEOF) {
		if (c == 13)
			continue;
		if (c == 10) {
			nl++;
			space++;
			continue;
		}
		if (iswspace(c)) {
			space++;
			continue;
		}
		if (nl > 1) {
			space=0;
			if (nl == 2) {	
				dprintf("Paragraph break\n");
				send_space(14);
				if (farnsworth_wpm != 0.0)
					send_fspace(3*7);
			} else {
				dprintf("Section break\n");
				send_space(28);
				if (farnsworth_wpm != 0.0)
					send_fspace(6*7);
			}
			//dprintf("Paragraph break\n");
			//space=0;
			//send_space(7);
			//send_char('=');
			//send_space(4);
		}
		nl = 0;
		if (space) {
			dprintf("Space\n");
			send_space(4);
			if (farnsworth_wpm != 0.0)
				send_fspace(1*4);
			space=0;
		}
		dprintf("C: %d %c\n", c, c);
		send_space(3);
		if (farnsworth_wpm != 0.0)
			send_fspace(1*3);
		space=0;
		send_char(c);
	}
	dprintf("C: %d %m\n", c);
}

void print_help(const char *progname)
{
	printf("Usage: %s OPTIONS [FILENAME...]\n"
	       "Convert text into an audio file with morse code.\n"
	       "\n"
	       "Mandatory arguments to long options are mandatory for short options too.\n"
	       "  -s, --stereo        generate stereo output (by default 2 identical channels)\n"
	       "  -p, --phaseshift N  shift phase of right channel by N radians (stereo effect)\n"
	       "  -o, --output        specify output file (must be supplied for WAV output)\n"
#ifdef HAVE_LAME
	       "  -O, --output-format specify output file format (wav or mp3, default: wav)\n"
#else
	       "  -O, --output-format specify output file format (wav, default: wav)\n"
#endif
	       "  -f, --frequency=N   use sidetone frequency N Hz (default: 660)\n"
	       "  -r, --rate=N        sample rate N (default 16000)\n"
	       "  -w, --wpm=N         use N words per minute (default: 25)\n"
	       "  -F, --farnsworth=N  use N WPM Farnsworth speed (default: same as WPM)\n"
	       "  -e, --envelope=N    envelope N ms raised cosine (default=10.0)\n"
	       "  -h, --help          display this help and exit\n", progname);
}

int main(int argc, char *argv[]) {
	FILE *f;

	setlocale(LC_CTYPE, "");

	int option_index = 0;
	static struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "stereo", no_argument, 0, 's' },
			{ "phaseshift", required_argument, 0, 'p' },
			{ "rate", required_argument, 0, 'r' },
			{ "output", required_argument, 0, 'o' },
			{ "output-format", required_argument, 0, 'O' },
			{ "frequency", required_argument, 0, 'f' },
			{ "wpm", required_argument, 0, 'w' },
			{ "farnsworth", required_argument, 0, 'F' },
			{ "envelope", required_argument, 0, 'e' },
			{0, 0, 0, 0} };

	output_format = FORMAT_WAV;
	samplerate = 16000;
	frequency = 650;
	wpm = 25;
	envelope = 5.0; /* ms */

	while (1) {
		int c = getopt_long(argc, argv, "hsp:r:o:O:f:w:F:e:", long_options,
			&option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'h':
			print_help(argv[0]);
			exit(0);
		case 's':
			stereo = 1;
			break;
		case 'p':
			phase_shift = atof(optarg);
			break;
		case 'o':
			outfname = optarg;
			break;
		case 'r':
			samplerate = atoi(optarg);
			break;
		case 'f':
			frequency = atoi(optarg);
			break;
		case 'e':
			envelope = atof(optarg);
			break;
		case 'w':
			wpm = atof(optarg);
			break;
		case 'F':
			farnsworth_wpm = atof(optarg);
			break;
		case 'O':
			if (!strcmp(optarg, "wav")) {
				output_format = FORMAT_WAV;
#ifdef HAVE_LAME
			} else if (!strcmp(optarg, "mp3")) {
				output_format = FORMAT_MP3;
#endif
			} else {
				fprintf(stderr, "Unknown output format specified, supported formats: ");
#ifdef HAVE_LAME
				fprintf(stderr, "wav, mp3\n");
#else
				fprintf(stderr, "wav\n");
#endif
				exit(1);
			}
			break;
		case '?':
			/* getopt_long printed the error message */
			exit(1);
		default:
			fprintf(stderr, "Unexpected error parsing options\n");
			exit(1);
		}
	}

	if (wpm < 1.0)
		wpm = 1.0;
	if (farnsworth_wpm != 0.0 && (farnsworth_wpm >= wpm)) {
		fprintf(stderr, "Warning: Farnsworth speed must be lower than WPM, ignored.\n");
		farnsworth_wpm = 0.0;
	}

	if (output_format == FORMAT_WAV && (!outfname || !strcmp(outfname, "-"))) {
		fprintf(stderr, "Error: Cannot write WAV format to standard output, specify output file with\nthe --output option or use another format (try --help)\n");
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
