/*
 * eeprog28.c
 *
 *  ___ ___ ___ ___  ___  __  __                                                     
 * | __| __| _ \ _ \/ _ \|  \/  |___ _ __ _ _ ___  __ _ _ _ __ _ _ __  _ __  ___ _ _ 
 * | _|| _||  _/   / (_) | |\/| |___| '_ \ '_/ _ \/ _` | '_/ _` | '  \| '  \/ -_) '_|
 * |___|___|_| |_|_\\___/|_|  |_|   | .__/_| \___/\__, |_| \__,_|_|_|_|_|_|_\___|_|  
 *                                  |_|           |___/                              
 *
 * Author.....: Alessandro Fraschetti (mail: gos95@gommagomma.net)
 * Target.....: RaspberryPI
 * Version....: 1.3.0 2026/10/03
 * Description: EEPROM 28C-family programmer utility
 * URL........: https://github.com/gom9000/xp-eeprom-28C-programmer
 * License....: this program is under the terms of MIT License
 * Notes......: ADDR BUS : A0-A14 = 8,9,7  0,2,3  12,13,14  30,21,22,23,24,25 (wiringPi pin numbers)
 *              DATA BUS : D0-D7  = 15,16,1  4,5  6,10,11
 *              CTRL BUS : CE=27, OE=28, WE=29
 *
 */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringPi.h>
#include "eeprom.h"


#define ROM_MAX_SIZE 0x8000 /* 32K: A0-A14 */


/* parse a hex value (max 5 digits); returns 0 on success */
static int parseHex(const char *str, unsigned int *value)
{
    char *end;
    unsigned long v;

    if (!str || !*str) return -1;
    v = strtoul(str, &end, 16);
    if (*end || v > 0xFFFFF) return -1;
    *value = (unsigned int)v;
    return 0;
}

/*
 * getopt optional arguments must be attached (-w100): also accept "-w 100".
 * The optional LENGTH of -w/-v must start with a decimal digit (e.g. 0A0):
 * this way "-wa" or "-vf FILE" are rejected instead of being read as the
 * hex lengths 0xA and 0xF.
 */
static const char *optionalArg(int argc, char **argv, int *bad)
{
    *bad = 0;
    if (optarg)
    {
        if (!isdigit((unsigned char)optarg[0])) *bad = 1;
        return optarg;
    }
    if (optind < argc && isdigit((unsigned char)argv[optind][0]))
        return argv[optind++];
    return NULL;
}

static void usage(const char *prog)
{
    printf("usage: %s option [params...]\n", prog);
    printf("EEPROM 28C programmer utility\n");
    printf("  options:\n");
    printf("    -t <LENGTH> [-s <START>] [-z <BLANK>]\n");
    printf("    \ttest if rom is filled with BLANK data values\n");
    printf("    -d <LENGTH> [-s <START>] [-f <DATAFILE>] [-a]\n");
    printf("    \tdump LENGTH bytes of rom, starting from START address, into DATAFILE (stdout: always ascii, with addresses)\n");
    printf("    -e <LENGTH> [-s <START>] [-z <BLANK>] [-b <PAGESIZE>]\n");
    printf("    \terase LENGTH bytes (filled with BLANK values) of rom, starting from START address\n");
    printf("    -v [<LENGTH>] [-s <START>] [-f <DATAFILE>] [-a]\n");
    printf("    \tverify LENGTH bytes of rom, starting from START address, with the contents of DATAFILE\n");
    printf("    \tif LENGTH is not specified or LENGTH is greater than DATAFILE length, the latter is used.\n");
    printf("    -w [<LENGTH>] [-s <START>] [-z <BLANK>] [-f <DATAFILE>] [-a] [-b <PAGESIZE>]\n");
    printf("    \twrite LENGTH bytes of rom, starting from START address, with the contents of DATAFILE\n");
    printf("    \tif LENGTH is not specified, DATAFILE length is used. If LENGTH is greater than DATAFILE length, diff is filled with BLANK values\n");
    printf("    -l <MODE>\n");
    printf("    \tenable (MODE=1) or disable (MODE=0) rom Software Data Protection\n");
    printf("    -h: show this help and exit\n");
    printf("  only one of -t, -d, -e, -v, -w, -l can be given\n");
    printf("  params (LENGTH, START, BLANK and PAGESIZE are hex values; the optional LENGTH of -v/-w must start with a digit, e.g. 0A0):\n");
    printf("    -s <START> set the start hex address, default value is 0x0\n");
    printf("    -f <DATAFILE> set the datafile where to dump/read values, default is stdout (dump) or stdin (write/verify)\n");
    printf("    -z <BLANK> set the blank hex data value to fill/test the rom, default value is 0xFF\n");
    printf("    -b <PAGESIZE> enable page write mode with PAGESIZE hex bytes (40 for 28C64/28C256), default is byte mode\n");
    printf("    -a set the mode of the datafile as ascii hex (default mode is binary); \"XXXXX:\" address labels are skipped\n");
    printf("    -p show the value of input params and bus pin numbers\n");
    printf("  exit status: 0 on success, 1 on error or verify/test mismatch\n");
}

static FILE *openFile(const char *filename, const char *mode, FILE *dflt)
{
    FILE *fp;

    if (!filename) return dflt;
    if (!(fp = fopen(filename, mode)))
        perror(filename);
    return fp;
}

static int hexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = toupper((unsigned char)c);
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/*
 * Read next byte from datafile; returns 1 on success, 0 on EOF, -1 on error.
 * Ascii files are whitespace separated tokens of hex digit pairs ("A1C4..."
 * or "A1 C4 ..."); tokens ending with ':' are address labels and are skipped,
 * so the stdout dump format can be read back as well.
 */
static int readByte(FILE *fp, unsigned char ascii, data_t *data)
{
    static char token[257]; /* even max width: pairs are never split */
    static size_t pos = 0, len = 0;

    if (!ascii)
    {
        if (fread(data, sizeof(data_t), 1, fp) == 1) return 1;
        if (ferror(fp)) { perror("datafile"); return -1; }
        return 0;
    }

    while (pos >= len)
    {
        if (fscanf(fp, "%256s", token) != 1)
        {
            if (ferror(fp)) { perror("datafile"); return -1; }
            return 0;
        }
        len = strlen(token);
        pos = 0;
        if (token[len-1] == ':') { len = 0; continue; } /* address label */
        for (size_t ii=0; ii<len; ii++)
            if (hexDigit(token[ii]) < 0 || len % 2)
            {
                fprintf(stderr, "Invalid ascii hex data `%s' in datafile.\n", token);
                return -1;
            }
    }
    *data = (data_t)(hexDigit(token[pos]) << 4 | hexDigit(token[pos+1]));
    pos += 2;
    return 1;
}


int main(int argc, char **argv)
{
    int c;
    unsigned char t_flag = 0;
    unsigned char d_flag = 0;
    unsigned char e_flag = 0;
    unsigned char w_flag = 0;
    unsigned char v_flag = 0;
    unsigned char l_flag = 0;
    unsigned char a_flag = 0;
    unsigned char p_flag = 0;
    unsigned int start = 0;
    unsigned int length = 0;
    unsigned char mode = 0;
    unsigned int zerobyte = 0xFF;
    unsigned int pagesize = 1;
    const char *arg;
    int bad;
    int rc;
    char *filename = NULL;
    FILE *fp;


    opterr = 0; /* errors are reported below */
    while ((c = getopt(argc, argv, "s:f:z:b:t:d:e:w::v::l:aph")) != -1)
    {
        switch (c)
        {
            case 's':
                if (parseHex(optarg, &start) || start >= ROM_MAX_SIZE)
                {
                    fprintf(stderr, "Invalid START address `%s' (max %X).\n", optarg, ROM_MAX_SIZE-1);
                    return 1;
                }
                break;
            case 'b':
                if (parseHex(optarg, &pagesize) || !pagesize || pagesize > MAX_PAGE_SIZE || (pagesize & (pagesize-1)))
                {
                    fprintf(stderr, "Invalid PAGESIZE `%s' (hex power of 2, max %X).\n", optarg, MAX_PAGE_SIZE);
                    return 1;
                }
                break;
            case 'f':
                filename = optarg;
                break;
            case 'z':
                if (parseHex(optarg, &zerobyte) || zerobyte > 0xFF)
                {
                    fprintf(stderr, "Invalid BLANK value `%s'.\n", optarg);
                    return 1;
                }
                break;
            case 't':
            case 'd':
            case 'e':
            case 'w':
            case 'v':
                if (c == 't') t_flag = 1;
                if (c == 'd') d_flag = 1;
                if (c == 'e') e_flag = 1;
                if (c == 'w') w_flag = 1;
                if (c == 'v') v_flag = 1;
                bad = 0;
                arg = (c == 'w' || c == 'v')? optionalArg(argc, argv, &bad) : optarg;
                if (arg && (bad || parseHex(arg, &length) || length > ROM_MAX_SIZE))
                {
                    fprintf(stderr, "Invalid LENGTH `%s' for -%c (hex, max %X%s).\n", arg, c, ROM_MAX_SIZE,
                            (bad? "; an attached LENGTH must start with a digit, no other options can follow -w/-v in a group" : ""));
                    return 1;
                }
                break;
            case 'l':
                l_flag = 1;
                if (!strcmp(optarg, "1"))
                    mode = 1;
                else if (!strcmp(optarg, "0"))
                    mode = 0;
                else
                {
                    fprintf(stderr, "Invalid SDP MODE `%s' (use 0 or 1).\n", optarg);
                    return 1;
                }
                break;
            case 'a':
                a_flag = 1;
                break;
            case 'p':
                p_flag = 1;
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            case '?':
                if (optopt == 't' || optopt == 'd' || optopt == 'e')
                    fprintf(stderr, "Option -%c requires LENGTH as param.\n", optopt);
                else if (optopt == 's' || optopt == 'b' || optopt == 'f' || optopt == 'z' || optopt == 'l')
                    fprintf(stderr, "Option -%c requires a param.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                return 1;
            default:
                abort();
        }
    }

    if (optind < argc)
    {
        unsigned int dummy;
        fprintf(stderr, "Unexpected argument `%s' (%s).\n", argv[optind],
                parseHex(argv[optind], &dummy)? "use -f <DATAFILE> for the datafile"
                                               : "the LENGTH of -v/-w must start with a digit, e.g. 0A0");
        return 1;
    }
    if (!(t_flag || d_flag || e_flag || w_flag || v_flag || l_flag || p_flag))
    {
        usage(argv[0]);
        return 1;
    }
    if (t_flag + d_flag + e_flag + w_flag + v_flag + l_flag > 1)
    {
        fprintf(stderr, "Only one of -t, -d, -e, -v, -w, -l can be given.\n");
        return 1;
    }
    if (start + length > ROM_MAX_SIZE)
    {
        fprintf(stderr, "START+LENGTH (%X) exceeds the max ROM size (%X).\n", start+length, ROM_MAX_SIZE);
        return 1;
    }

    if (wiringPiSetup() == -1)
    {
        fprintf(stderr, "wiringPi setup failed.\n");
        return 1;
    }
    setup();


    if (p_flag)
    {
        printf("start-address=%.5X, data-length=%.5X, zero-byte=%.2X, page-size=%X, data-file=%s, data-file-mode=%s, SDP=%d\n",
               start, length, zerobyte, pagesize, (filename? filename : "-"), (a_flag? "ascii":"binary"), mode);
        printf("address bus pins: ");
        for (unsigned char ii=0; ii<BUS_SIZE(A); ii++) printf("%d ", A[ii]);
        printf("\ndata bus pins: ");
        for (unsigned char ii=0; ii<BUS_SIZE(D); ii++) printf("%d ", D[ii]);
        printf("\nWE pin: %d, CE pin: %d, OE pin: %d\n", WE, CE, OE);
    }
    if (t_flag)
    {
        length_t count;
        if ((count = testROM(start, length, zerobyte)))
        {
            printf("ROM is not empty, found 0x%X data bytes\n", count);
            return 1;
        }
        printf("ROM is empty\n");
        return 0;
    }
    if (d_flag)
    {
        if (!(fp = openFile(filename, a_flag? "w" : "wb", stdout))) return 1;
        fprintf(stderr, "Dumping of 0x%X locations of ROM from start 0x%X...\n", length, start);
        dumpROM(start, length, fp, (fp == stdout) || a_flag);
        if (fp != stdout) fclose(fp);
        fprintf(stderr, "ROM dumped.\n");
        return 0;
    }
    if (e_flag)
    {
        length_t errors;
        printf("Erasing (0x%X) of 0x%X locations of ROM from start 0x%X...\n", zerobyte, length, start);
        if ((errors = eraseROM(start, length, zerobyte, pagesize)))
        {
            printf("erase error, %s0x%X locations not written (Software Data Protection enabled?).\n",
                   (errors >= MAX_WRITE_ERRORS? "aborted, " : ""), errors);
            return 1;
        }
        printf("ROM erased.\n");
        return 0;
    }
    if (w_flag)
    {
        static data_t buf[ROM_MAX_SIZE];
        length_t count = 0;
        length_t total;
        rc = 0;
        length_t errors;
        length_t maxlen = length? length : ROM_MAX_SIZE - start;

        if (!(fp = openFile(filename, a_flag? "r" : "rb", stdin))) return 1;
        while (count < maxlen && (rc = readByte(fp, a_flag, &buf[count])) == 1)
            count++;
        if (fp != stdin) fclose(fp);
        if (rc < 0)
        {
            fprintf(stderr, "write aborted, datafile error after 0x%X bytes: ROM not modified.\n", count);
            return 1;
        }
        if (!count && !length)
        {
            fprintf(stderr, "write aborted, datafile is empty.\n");
            return 1;
        }
        total = length? length : count;
        for (length_t ii=count; ii<total; ii++) buf[ii] = (data_t)zerobyte;

        printf("Writing ROM with contents of %s file: %s from start 0x%X (%s mode)...\n",
               (a_flag? "ascii" : "binary"), (filename? filename : "stdin"), start,
               (pagesize > 1? "page" : "byte"));
        if (pagesize > 1) piHiPri(50); /* fewer preemptions during page loads (needs root) */
        errors = programROM(start, buf, total, pagesize);
        if (!errors && (errors = verifyROM(start, buf, total)))
        {
            printf("write error, final verify found 0x%X locations differing.\n", errors);
            return 1;
        }
        if (!errors)
        {
            printf("0x%X locations of ROM written", count);
            if (total > count) printf(", 0x%X filled with 0x%.2X", total-count, zerobyte);
            printf(".\n");
        }
        else
        {
            printf("write error, %s0x%X locations not written (Software Data Protection enabled?).\n",
                   (errors >= MAX_WRITE_ERRORS? "aborted, " : ""), errors);
            return 1;
        }
        return 0;
    }
    if (v_flag)
    {
        address_t address = 0;
        data_t data = 0;
        data_t actual = 0;
        length_t maxlen = length? length : ROM_MAX_SIZE - start;

        rc = 0;
        if (!(fp = openFile(filename, a_flag? "r" : "rb", stdin))) return 1;
        printf("Verifying ROM with contents of %s file: %s from start 0x%X...\n",
               (a_flag? "ascii" : "binary"), (filename? filename : "stdin"), start);
        while (address < maxlen && (rc = readByte(fp, a_flag, &data)) == 1)
        {
            actual = readROM(start+address);
            if (data != actual)
            {
                if (fp != stdin) fclose(fp);
                printf("verify error, found 0x%.2X (expected 0x%.2X) at location 0x%X.\n", actual, data, start+address);
                return 1;
            }
            address++;
        }
        if (fp != stdin) fclose(fp);
        if (rc < 0)
        {
            printf("verify error, datafile error after 0x%X locations.\n", address);
            return 1;
        }
        if (!address)
        {
            printf("verify error, datafile is empty.\n");
            return 1;
        }
        printf("verify done, 0x%X locations checked.\n", address);
        return 0;
    }
    if (l_flag)
    {
        setSDPMode(mode);
        printf("ROM Software Data Protection %s.\n", (mode? "Enabled" : "Disabled"));
        return 0;
    }
    return 0;
}
