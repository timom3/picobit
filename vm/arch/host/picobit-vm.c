#include "picobit-vm.h"
#include "heap-layout.h"

/*---------------------------------------------------------------------------*/

// error handling

#ifdef WORKSTATION
void error (char *prim, char *msg)
{
	printf ("ERROR: %s: %s\n", prim, msg);
	exit (1);
}

void type_error (char *prim, char *type)
{
	printf ("ERROR: %s: An argument of type %s was expected\n", prim, type);
	exit (1);
}
#endif

#ifdef SIXPIC
void halt_with_error ()
{
	uart_write(101); // e
	uart_write(114); // r
	uart_write(114); // r
	uart_write(13);
	uart_write(10);
	exit();
}
#endif

/*---------------------------------------------------------------------------*/

#ifdef WORKSTATION
#define ROM_BYTES 8192
uint8 rom_mem[ROM_BYTES] = {
#define RED_GREEN
#define PUTCHAR_LIGHT_not
#ifdef RED_GREEN
	0xFB, 0xD7, 0x03, 0x00, 0x00, 0x00, 0x00, 0x32
	, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00
	, 0x08, 0x50, 0x80, 0x16, 0xFE, 0xE8, 0x00, 0xFC
	, 0x32, 0x80, 0x2D, 0xFE, 0xFC, 0x31, 0x80, 0x43
	, 0xFE, 0xFC, 0x33, 0x80, 0x2D, 0xFE, 0xFC, 0x31
	, 0x80, 0x43, 0xFE, 0x90, 0x16, 0x01, 0x20, 0xFC
	, 0x32, 0xE3, 0xB0, 0x37, 0x09, 0xF3, 0xFF, 0x20
	, 0xFC, 0x33, 0xE3, 0xB0, 0x40, 0x0A, 0xF3, 0xFF
	, 0x08, 0xF3, 0xFF, 0x01, 0x40, 0x21, 0xD1, 0x00
	, 0x02, 0xC0, 0x4C, 0x71, 0x01, 0x20, 0x50, 0x90
	, 0x51, 0x00, 0xF1, 0x40, 0xD8, 0xB0, 0x59, 0x90
	, 0x51, 0x00, 0xFF
#endif
#ifdef PUTCHAR_LIGHT
	0xFB, 0xD7, 0x00, 0x00, 0x80, 0x08, 0xFE, 0xE8
	, 0x00, 0xF6, 0xF5, 0x90, 0x08
#endif
};
#endif

// memory access

word ram_get_fieldn (obj o, word n)
{
	switch (n) {
	case 0:
		return ram_get_field0 (o);
	case 1:
		return ram_get_field1 (o);
	case 2:
		return ram_get_field2 (o);
	case 3:
		return ram_get_field3 (o);
	}
}
void ram_set_fieldn (obj o, uint8 n, word val)   // TODO have as a macro ?
{
	switch (n) {
	case 0:
		ram_set_field0 (o, val);
		break;
	case 1:
		ram_set_field1 (o, val);
		break;
	case 2:
		ram_set_field2 (o, val);
		break;
	case 3:
		ram_set_field3 (o, val);
		break;
	}
}

// these temporary variables are necessary with SIXPIC, or else the shift
// results will be 8 bits values, which is wrong
obj ram_get_car (obj o)
{
	uint16 tmp = ram_get_field0 (o) & 0x1f;
	return (tmp << 8) | ram_get_field1 (o);
}
obj rom_get_car (obj o)
{
	uint16 tmp = rom_get_field0 (o) & 0x1f;
	return (tmp << 8) | rom_get_field1 (o);
}
obj ram_get_cdr (obj o)
{
	uint16 tmp = ram_get_field2 (o) & 0x1f;
	return (tmp << 8) | ram_get_field3 (o);
}
obj rom_get_cdr (obj o)
{
	uint16 tmp = rom_get_field2 (o) & 0x1f;
	return (tmp << 8) | rom_get_field3 (o);
}

void ram_set_car (obj o, obj val)
{
	ram_set_field0 (o, (val >> 8) | (ram_get_field0 (o) & 0xe0));
	ram_set_field1 (o, val & 0xff);
}
void ram_set_cdr (obj o, obj val)
{
	ram_set_field2 (o, (val >> 8) | (ram_get_field2 (o) & 0xe0));
	ram_set_field3 (o, val & 0xff);
}

// function entry point
// the temporary variables are necessary with SIXPIC, see above
obj ram_get_entry (obj o)
{
	uint16 tmp  = ram_get_field2 (o);
	return ((tmp << 8) | ram_get_field3 (o));
}

obj get_global (uint8 i)
{
// globals occupy the beginning of ram, with 2 globals per word
	if (i & 1) {
		return ram_get_cdr (MIN_RAM_ENCODING + (i >> 1));
	} else {
		return ram_get_car (MIN_RAM_ENCODING + (i >> 1));
	}
}
void set_global (uint8 i, obj o)
{
	if (i & 1) {
		ram_set_cdr (MIN_RAM_ENCODING + (i >> 1), o);
	} else {
		ram_set_car (MIN_RAM_ENCODING + (i >> 1), o);
	}
}

// TODO generic functions (get_field0, get_car, etc) that work for both rom and ram were not used, are in garbage

/*---------------------------------------------------------------------------*/

#ifdef WORKSTATION

int hidden_fgetc (FILE *f)
{
	int c = fgetc (f);
#if 0
	printf ("{%d}",c);
	fflush (stdout);
#endif
	return c;
}

#define fgetc(f) hidden_fgetc(f)

void write_hex_nibble (int n)
{
	putchar ("0123456789ABCDEF"[n]);
}

void write_hex (uint8 n)
{
	write_hex_nibble (n >> 4);
	write_hex_nibble (n & 0x0f);
}

int hex (int c)
{
	if (c >= '0' && c <= '9') {
		return (c - '0');
	}

	if (c >= 'A' && c <= 'F') {
		return (c - 'A' + 10);
	}

	if (c >= 'a' && c <= 'f') {
		return (c - 'a' + 10);
	}

	return -1;
}

int read_hex_byte (FILE *f)
{
	int h1 = hex (fgetc (f));
	int h2 = hex (fgetc (f));

	if (h1 >= 0 && h2 >= 0) {
		return (h1<<4) + h2;
	}

	return -1;
}

int read_hex_file (char *filename)
{
	int c;
	FILE *f = fopen (filename, "r");
	int result = 0;
	int len;
	int a, a1, a2;
	int t;
	int b;
	int i;
	uint8 sum;
	int hi16 = 0;

	for (i=0; i<ROM_BYTES; i++) {
		rom_mem[i] = 0xff;
	}

	if (f != NULL) {
		while ((c = fgetc (f)) != EOF) {
			if ((c == '\r') || (c == '\n')) {
				continue;
			}

			if (c != ':' ||
			    (len = read_hex_byte (f)) < 0 ||
			    (a1 = read_hex_byte (f)) < 0 ||
			    (a2 = read_hex_byte (f)) < 0 ||
			    (t = read_hex_byte (f)) < 0) {
				break;
			}

			a = (a1 << 8) + a2;

			i = 0;
			sum = len + a1 + a2 + t;

			if (t == 0) {
next0:

				if (i < len) {
					unsigned long adr = ((unsigned long)hi16 << 16) + a - CODE_START;

					if ((b = read_hex_byte (f)) < 0) {
						break;
					}

					if (adr >= 0 && adr < ROM_BYTES) {
						rom_mem[adr] = b;
					}

					a = (a + 1) & 0xffff;
					i++;
					sum += b;

					goto next0;
				}
			} else if (t == 1) {
				if (len != 0) {
					break;
				}
			} else if (t == 4) {
				if (len != 2) {
					break;
				}

				if ((a1 = read_hex_byte (f)) < 0 ||
				    (a2 = read_hex_byte (f)) < 0) {
					break;
				}

				sum += a1 + a2;

				hi16 = (a1<<8) + a2;
			} else {
				break;
			}

			if ((b = read_hex_byte (f)) < 0) {
				break;
			}

			sum = -sum;

			if (sum != b) {
				printf ("*** HEX file checksum error (expected 0x%02x)\n", sum);
				break;
			}

			c = fgetc (f);

			if ((c != '\r') && (c != '\n')) {
				break;
			}

			if (t == 1) {
				result = 1;
				break;
			}
		}

		if (result == 0) {
			printf ("*** HEX file syntax error\n");
		}

		fclose (f);
	}

	return result;
}

#endif

/*---------------------------------------------------------------------------*/

// When compiling with functions instead of macros, separate compilation must
// be disabled, otherwise functions are defined more than once.

#ifdef LESS_MACROS
#include "debug.c"
#include "gc.c"
#include "bignums.c"
#include "primitives.c"
#include "dispatch.c"
#endif

/*---------------------------------------------------------------------------*/

#ifdef WORKSTATION

void usage ()
{
	printf ("usage: sim file.hex\n");
	exit (1);
}

int main (int argc, char *argv[])
{
	int errcode = 0;
	rom_addr rom_start_addr = 0;

#ifdef TEST_BIGNUM
	test();
#endif

	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 's') {
		int h1;
		int h2;
		int h3;
		int h4;

		if ((h1 = hex (argv[1][2])) < 0 ||
		    (h2 = hex (argv[1][3])) < 0 ||
		    (h3 = hex (argv[1][4])) != 0 ||
		    (h4 = hex (argv[1][5])) != 0 ||
		    argv[1][6] != '\0') {
			usage ();
		}

		rom_start_addr = (h1 << 12) | (h2 << 8) | (h3 << 4) | h4;

		argv++;
		argc--;
	}

#ifdef DEBUG
	printf ("Start address = 0x%04x\n", rom_start_addr + CODE_START);
#endif

	if (argc != 2) {
		usage ();
	}

	if (!read_hex_file (argv[1])) {
		printf ("*** Could not read hex file \"%s\"\n", argv[1]);
	} else {
		int i;

		if (rom_get (CODE_START+0) != 0xfb ||
		    rom_get (CODE_START+1) != 0xd7) {
			printf ("*** The hex file was not compiled with PICOBIT\n");
		} else {
#if 0

			for (i=0; i<8192; i++)
				if (rom_get (i) != 0xff) {
					printf ("rom_mem[0x%04x] = 0x%02x\n", i, rom_get (i));
				}

#endif

			interpreter ();

#ifdef DEBUG_GC
			printf ("**************** memory needed = %d\n", max_live+1);
#endif
		}
	}

	return errcode;
}

#endif

#ifdef SIXPIC
interpreter();
#endif

#ifdef HI_TECH_C
void main ()
{
	interpreter();
}
#endif