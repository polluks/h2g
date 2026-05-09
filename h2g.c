/*
 * h2g - Hubbard to GoatTracker converter
 * Original VB6 by Stilianos (Stello) Doussis (August 2005)
 * ANSI C conversion
 *
 * Usage: h2g <input.sid> [output.sng]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SID_MAX 65536
#define GOAT_MAX 999999

/* SID file data */
static unsigned char SIDfile[SID_MAX];
static long SIDlength;
static char SIDname[32];
static char SIDauthor[32];
static char SIDreleased[32];
static long SIDsubtunes;
static long SIDloadaddr;
static long SIDhlenght;

/* GoatTracker output data */
static unsigned char Goatfile[GOAT_MAX];
static long Goatfl;
static unsigned char GoatTableWave[256];
static unsigned char GoatTablePulse[256];
static long GoatTracksMax;
static long GoatTracks[256][256];

static long GoatPatternMax;
static long GoatPLength[256];
static long GoatPattern[256][8192];

/* Hubbard player detection results */
static long SIDRHreadTrackVersion;
static long SIDRHinstrStart;
static long SIDRHinstrUsed;
static long SIDRHtrackVoices;
static int SIDRHtrackSelector;
static long SIDRHtrackHi;
static long SIDRHtrackLo;
static long SIDRHPatternHi;
static long SIDRHPatternLo;
static long SIDRHPattternUsed;

static const long vwaveforms[20] = {
    0x00, 0x01, 0x09, 0x11, 0x13, 0x15, 0x17, 0x21,
    0x23, 0x25, 0x27, 0x41, 0x43, 0x45, 0x47, 0x51,
    0x53, 0x55, 0x57, 0x81
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void AddTextS(const char *text)
{
    printf("%s", text);
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static long HexToDec(const char *xHex)
{
    long c = 0;
    while (*xHex) {
        c = (c << 4) | hexval(*xHex);
        xHex++;
    }
    return c;
}

static void FormHex(long num, char *out)
{
    sprintf(out, "%02lX", num);
}

static int FileExist(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* SSearchfile - search for hex pattern with ?? wildcards              */
/* ------------------------------------------------------------------ */

static long SSearchfile(const char *pattern)
{
    long snumb[99];
    long findnumbers, inumb;
    long i, ix, ixf;
    int wildcard[99];
    char ss[3];
    const char *p;
    findnumbers = 0;
    inumb = 0;
    p = pattern;
    while (*p) {
        ss[0] = p[0];
        ss[1] = p[1];
        ss[2] = '\0';
        if (ss[0] == '?' && ss[1] == '?') {
            wildcard[inumb] = 1;
            snumb[inumb] = 0;
        } else {
            wildcard[inumb] = 0;
            snumb[inumb] = HexToDec(ss);
            findnumbers++;
        }
        inumb++;
        if (p[2] == ' ') p += 3;
        else if (p[2] == '\0') break;
        else p += 3;
    }

    for (i = 0; i <= SIDlength - inumb; i++) {
        ix = 0;
        ixf = 0;
        while (ix < inumb) {
            if (!wildcard[ix]) {
                if (snumb[ix] == (long)SIDfile[i + ix]) {
                    ixf++;
                    if (ixf >= findnumbers)
                        return i;
                } else {
                    break;
                }
            }
            ix++;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* GoatAddString                                                       */
/* ------------------------------------------------------------------ */

static void GoatAddString(const char *str)
{
    while (*str) {
        Goatfile[Goatfl++] = (unsigned char)*str;
        str++;
    }
}

/* ------------------------------------------------------------------ */
/* GoatClear                                                           */
/* ------------------------------------------------------------------ */

static void GoatClear(void)
{
    long i;

    Goatfl = 0;
    for (i = 0; i <= 0x61; i++)
        Goatfile[i] = 0;
    for (i = 0; i < 256; i++) {
        GoatTableWave[i] = 0;
        GoatTablePulse[i] = 0;
    }

    GoatAddString("GTS2");
    Goatfl = 4;
    GoatAddString(SIDname);
    Goatfl = 0x24;
    GoatAddString(SIDauthor);
    Goatfl = 0x44;
    GoatAddString(SIDreleased);
}

/* ------------------------------------------------------------------ */
/* GoatConvertTracks                                                   */
/* ------------------------------------------------------------------ */

static void GoatConvertTracks(void)
{
    long i, i2, voice, so, b1, wb, addr;

    GoatTracksMax = 0;
    for (i = 0; i < 256; i++)
        for (i2 = 0; i2 < 256; i2++)
            GoatTracks[i][i2] = 0;

    Goatfile[0x64] = (unsigned char)SIDsubtunes;

    for (i = 0; i < SIDsubtunes; i++) {
        for (voice = 0; voice < 3; voice++) {
            GoatTracks[GoatTracksMax][0] = 4;
            GoatTracks[GoatTracksMax][1] = 0;
            GoatTracks[GoatTracksMax][2] = 0xFF;
            GoatTracks[GoatTracksMax][3] = 0;

            if (voice >= SIDRHtrackVoices)
                goto SkipVoice;

            so = voice + (i * (SIDRHtrackVoices * 2));
            addr = (long)SIDfile[SIDRHtrackHi + so] * 256;
            addr += (long)SIDfile[SIDRHtrackLo + so];
            addr = addr - SIDloadaddr + SIDhlenght - 1;

            if (addr <= 1 || addr >= SIDlength) {
                char buf[128];
                sprintf(buf, "*** SUBTUNE $%lX (VOICE $%lX) ADDRESS OUT OF RANGE, CAN'T CONVERT ***",
                        i, voice);
                AddTextS(buf);
                goto SkipVoice;
            }

            i2 = 0;
            GoatTracks[GoatTracksMax][0] = 1;

            do {
                b1 = (long)SIDfile[addr + i2];
                if (GoatTracks[GoatTracksMax][0] >= 254)
                    b1 = 0xFF;
                i2++;

                switch (SIDRHreadTrackVersion) {
                case 4:
                    wb = -1;
                    if (b1 >= 0x80) {
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0]] = 0xFF;
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0] + 1] = 0;
                        GoatTracks[GoatTracksMax][0] += 2;
                        goto TrackEnd;
                    }
                    break;

                case 5: case 6: case 7: case 8:
                    wb = -1;
                    if (b1 >= 0x80 && b1 <= 0x8F) {
                        wb = (b1 - 0x80) + 0xF0;
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0]] = wb;
                        GoatTracks[GoatTracksMax][0]++;
                    }
                    if (b1 >= 0xEF && b1 <= 0xFE) {
                        wb = b1 ^ 0xFF;
                        wb = 0xF0 - wb;
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0]] = wb;
                        GoatTracks[GoatTracksMax][0]++;
                    }
                    if (b1 == 0xFF) {
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0]] = 0xFF;
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0] + 1] = 0;
                        GoatTracks[GoatTracksMax][0] += 2;
                        goto TrackEnd;
                    }
                    if (b1 <= 0x7F) {
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0]] = b1;
                        GoatTracks[GoatTracksMax][0]++;
                    }
                    break;

                case 0: case 1: case 2: case 3:
                    wb = -1;
                    if (b1 == 0xFE) {
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0]] = 0xFF;
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0] + 1] = 0xFD;
                        GoatTracks[GoatTracksMax][0] += 3;
                        goto TrackEnd;
                    }
                    if (b1 == 0xFF) {
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0]] = 0xFF;
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0] + 1] = 0;
                        GoatTracks[GoatTracksMax][0] += 3;
                        goto TrackEnd;
                    }
                    if (b1 <= 0xFD) {
                        GoatTracks[GoatTracksMax][GoatTracks[GoatTracksMax][0]] = b1;
                        GoatTracks[GoatTracksMax][0]++;
                    }
                    break;

                default:
                    goto TrackEnd;
                }
            } while (1);

TrackEnd:
            ;
SkipVoice:
            GoatTracksMax++;
        }
    }
}

/* ------------------------------------------------------------------ */
/* GoatConvertPattern                                                  */
/* ------------------------------------------------------------------ */

static int GoatConvertPattern(void)
{
    long i, i2, i3, i4, b1;
    long nWait, nNoNote, nNoADSR, nGetNext, nPitch;
    long gNoteNr, gInstrument, gCommandVal1, gCommandVal2;
    long gOLDInstrument1, gOLDInstrument2;
    long gTMaxPatternLen, gTNoNote;
    long pataddr, addr, RescInstrNr;
    long NewGoatPatternMax;
    long NewGoatPatternLenght[256];
    long (*NewGoatPattern)[8192];
    long (*TrackIndex)[8192];
    long NewTrackLenght;
    long NewTrack[256];
    long ActTrack, PNum;
    int PEndMarker;

    NewGoatPattern = calloc(256, 8192 * sizeof(long));
    TrackIndex = calloc(256, 8192 * sizeof(long));
    if (!NewGoatPattern || !TrackIndex) {
        AddTextS("*** OUT OF MEMORY ***\r\n");
        free(NewGoatPattern);
        free(TrackIndex);
        return -1;
    }

    NewGoatPatternMax = 0;
    gTMaxPatternLen = 94 * 4;
    gTNoNote = 0xBD;
    GoatPatternMax = 0;

    for (i = 0; i < 256; i++) {
        GoatPLength[i] = 0;
        NewGoatPatternLenght[i] = 0;
        for (i2 = 0; i2 < 8192; i2++) {
            TrackIndex[i][i2] = -1;
            GoatPattern[i][i2] = 0;
            NewGoatPattern[i][i2] = 0;
        }
    }

    for (i = 0; i <= SIDRHPattternUsed; i++) {
        addr = (long)SIDfile[SIDRHPatternHi + i] * 256;
        addr += (long)SIDfile[SIDRHPatternLo + i];
        addr = addr - SIDloadaddr + SIDhlenght - 1;

        gInstrument = 0;
        gOLDInstrument1 = -1;
        gOLDInstrument2 = -2;
        gCommandVal1 = 0;
        gCommandVal2 = 0;
        pataddr = addr;
        i2 = 0;

        do {
            if ((pataddr + i2) <= 1 || (pataddr + i2) >= SIDlength) {
                GoatPLength[i] = 8;
                GoatPattern[i][0] = 0xBD;
                GoatPattern[i][1] = 0;
                GoatPattern[i][2] = 0;
                GoatPattern[i][3] = 0;
                GoatPattern[i][4] = 0xFF;
                GoatPattern[i][5] = 0;
                GoatPattern[i][6] = 0;
                GoatPattern[i][7] = 0;
                {
                    char buf[128];
                    sprintf(buf, "*** PATTERN $%lX ADDRESS OUT OF RANGE, CAN'T CONVERT ***", i);
                    AddTextS(buf);
                }
                goto SkipVoice;
            }

            gNoteNr = gTNoNote;
            gCommandVal1 = 0;
            gCommandVal2 = 0;

            b1 = (long)SIDfile[pataddr + i2];

            if (b1 == 0xFF) {
                GoatPattern[i][GoatPLength[i]] = 0xFF;
                GoatPattern[i][GoatPLength[i] + 1] = 0;
                GoatPattern[i][GoatPLength[i] + 2] = 0;
                GoatPattern[i][GoatPLength[i] + 3] = 0;
                GoatPLength[i] += 4;
                break;
            }

            nGetNext = b1 & 0x80;
            nNoNote = b1 & 0x40;
            nNoADSR = b1 & 0x20;
            nWait = b1 & 0x1F;

            if (nGetNext) {
                i2++;
                b1 = (long)SIDfile[pataddr + i2];

                if (nNoADSR) {
                    if (gOLDInstrument1 == gOLDInstrument2) {
                        gCommandVal1 = 3;
                        gCommandVal2 = 0;
                    }
                    gOLDInstrument2 = gOLDInstrument1;
                }

                nPitch = b1 & 0x80;
                if (nPitch) {
                    if ((b1 & 1) == 0) {
                        gCommandVal1 = 1;
                        gInstrument = 0;
                    } else {
                        gCommandVal1 = 2;
                        gInstrument = 0;
                    }
                    gCommandVal2 = (b1 & 0x7F);
                    gCommandVal2 /= 4;
                } else {
                    gInstrument = b1 & 0x7F;
                    gInstrument += 2;
                    gOLDInstrument2 = gOLDInstrument1;
                    gOLDInstrument1 = gInstrument;
                }
            }

            if (nGetNext || !nNoNote) {
                i2++;
                gNoteNr = (long)SIDfile[pataddr + i2];
                if (gNoteNr >= 0x5C) gNoteNr = 0x5C;
                gNoteNr += 0x60;
            }

            RescInstrNr = -1;
            if (gNoteNr == gTNoNote && gInstrument != 0) {
                gNoteNr = 0x60;
                RescInstrNr = gInstrument;
                gInstrument = 1;
            }

            if (nWait >= 1) {
                GoatPattern[i][GoatPLength[i]] = gNoteNr;
                GoatPattern[i][GoatPLength[i] + 1] = gInstrument;
                GoatPattern[i][GoatPLength[i] + 2] = gCommandVal1;
                GoatPattern[i][GoatPLength[i] + 3] = gCommandVal2;
                GoatPLength[i] += 4;

                if (gCommandVal1 == 3)
                    gCommandVal1 = 0;

                if (nWait >= 1) {
                    long wi;
                    gNoteNr = gTNoNote;
                    for (wi = 0; wi < nWait; wi++) {
                        GoatPattern[i][GoatPLength[i]] = gTNoNote;
                        GoatPattern[i][GoatPLength[i] + 1] = 0;
                        GoatPattern[i][GoatPLength[i] + 2] = gCommandVal1;
                        GoatPattern[i][GoatPLength[i] + 3] = gCommandVal2;
                        GoatPLength[i] += 4;
                    }
                }
            }

            if (RescInstrNr != -1)
                gInstrument = RescInstrNr;

            i2++;
        } while (1);

SkipVoice:
        i3 = 0;
        i4 = 0;
        for (i2 = 0; i2 <= GoatPLength[i]; i2++) {
            if (i3 >= gTMaxPatternLen) {
                i3 &= 0xFFFC;
                NewGoatPatternLenght[NewGoatPatternMax] = i3;
                NewGoatPattern[NewGoatPatternMax][i3] = 0xFF;
                NewGoatPattern[NewGoatPatternMax][i3 + 1] = 0;
                NewGoatPattern[NewGoatPatternMax][i3 + 2] = 0;
                NewGoatPattern[NewGoatPatternMax][i3 + 3] = 0;
                i3 = 0;
                i4++;
                NewGoatPatternMax++;
                {
                    char buf[128];
                    sprintf(buf, "Extending Pattern: $%lX ($%lX)", i, NewGoatPatternMax);
                    AddTextS(buf);
                }
                if (NewGoatPatternMax >= 0xD0) {
                    AddTextS("*** TOO MANY NEW PATTERN CREATED, CAN'T EXPORT TO GOATTRACKER ***");
                    free(NewGoatPattern);
                    free(TrackIndex);
                    return -1;
                }
            }
            TrackIndex[i][i4] = NewGoatPatternMax;
            NewGoatPattern[NewGoatPatternMax][i3] = GoatPattern[i][i2];
            NewGoatPatternLenght[NewGoatPatternMax] = i3;
            i3++;
        }
        NewGoatPatternMax++;
        if (NewGoatPatternMax >= 0xD0) {
            AddTextS("*** TOO MANY NEW PATTERN CREATED, CAN'T EXPORT TO GOATTRACKER ***");
            free(NewGoatPattern);
            free(TrackIndex);
            return -1;
        }
    }

    GoatPatternMax = NewGoatPatternMax;
    for (i = 0; i < NewGoatPatternMax; i++) {
        GoatPLength[i] = NewGoatPatternLenght[i];
        for (i2 = 0; i2 <= NewGoatPatternLenght[i]; i2++)
            GoatPattern[i][i2] = NewGoatPattern[i][i2];
    }

    for (i = 0; i < GoatTracksMax; i++) {
        PEndMarker = 0;
        NewTrack[0] = 0;
        NewTrackLenght = 0;
        for (i2 = 1; i2 < GoatTracks[i][0]; i2++) {
            ActTrack = GoatTracks[i][i2];
            if (ActTrack >= 0xD0 || PEndMarker) {
                PEndMarker = 1;
                NewTrack[NewTrackLenght] = ActTrack;
                NewTrackLenght++;
            } else {
                i3 = 0;
                do {
                    PNum = TrackIndex[ActTrack][i3];
                    if (PNum == -1) break;
                    NewTrack[NewTrackLenght] = PNum;
                    NewTrackLenght++;
                    if (NewTrackLenght >= 0xFF) {
                        AddTextS("*** TRACKLIST TOO LONG, CAN'T EXPORT TO GOATTRACKER ***");
                        free(NewGoatPattern);
                        free(TrackIndex);
                        return -1;
                    }
                    i3++;
                } while (1);
            }
            if (NewTrackLenght >= 0xFF) {
                AddTextS("*** TRACKLIST TOO LONG, CAN'T EXPORT TO GOATTRACKER ***");
                free(NewGoatPattern);
                free(TrackIndex);
                return -1;
            }
        }
        GoatTracks[i][0] = NewTrackLenght;
        for (i2 = 0; i2 <= NewTrackLenght; i2++)
            GoatTracks[i][i2 + 1] = NewTrack[i2];
    }

    free(NewGoatPattern);
    free(TrackIndex);
    return 0;
}

/* ------------------------------------------------------------------ */
/* GoatSave                                                            */
/* ------------------------------------------------------------------ */

static int GoatSave(const char *outpath)
{
    FILE *f;
    long i, i2, i3;
    unsigned char b1;
    long WTableStart, PTableStart, ArpStyle, ArpNote, ArpSetkeybit;
    char iName[32];
    int inamelen;

    if (FileExist(outpath))
        remove(outpath);

    f = fopen(outpath, "wb");
    if (!f) {
        AddTextS("*** CAN'T OPEN OUTPUT FILE ***");
        return -1;
    }

    /* write header */
    for (i = 0; i <= 0x63; i++)
        fputc(Goatfile[i], f);

    /* subtune count */
    fputc((unsigned char)SIDsubtunes, f);

    /* track data */
    for (i = 0; i < GoatTracksMax; i++) {
        b1 = (unsigned char)(GoatTracks[i][0] - 2);
        fputc(b1, f);
        for (i2 = 1; i2 < GoatTracks[i][0]; i2++) {
            b1 = (unsigned char)GoatTracks[i][i2];
            fputc(b1, f);
        }
    }

    /* instruments */
    SIDRHinstrUsed++;
    if (SIDRHinstrUsed >= 50) SIDRHinstrUsed = 50;
    fputc((unsigned char)SIDRHinstrUsed, f);

    /* empty instrument */
    WTableStart = 6;
    PTableStart = 3;

    fputc(0, f); fputc(0, f);
    fputc(1, f); fputc(1, f);
    fputc(0, f); fputc(0, f); fputc(0, f);
    fputc(2, f); fputc(9, f);

    {
        const char *iname = "Clear Voice";
        int len = strlen(iname);
        int k;
        fwrite(iname, 1, len, f);
        for (k = 0; k < (16 - len); k++)
            fputc(0, f);
    }

    /* real instruments */
    for (i = 0; i < SIDRHinstrUsed - 2; i++) {
        i2 = i * 8;
        b1 = SIDfile[SIDRHinstrStart + i2 + 3];
        fputc(b1, f);
        b1 = SIDfile[SIDRHinstrStart + i2 + 4];
        if (b1 >= 0xF0)
            b1 &= 0xEF;
        fputc(b1, f);
        b1 = (unsigned char)((i * 5) + WTableStart);
        fputc(b1, f);
        b1 = (unsigned char)((i * 2) + PTableStart);
        fputc(b1, f);
        fputc(0, f);
        fputc(0, f);
        fputc(0, f);
        fputc(2, f);
        fputc(9, f);

        /* instrument name */
        {
            char num[16];
            FormHex(i + 2, num);
            sprintf(iName, "%s:", num);
            FormHex(SIDfile[SIDRHinstrStart + i2 + 5], num);
            strcat(iName, num);
            strcat(iName, "-");
            FormHex(SIDfile[SIDRHinstrStart + i2 + 6], num);
            strcat(iName, num);
            strcat(iName, "-");
            FormHex(SIDfile[SIDRHinstrStart + i2 + 7], num);
            strcat(iName, num);

            inamelen = strlen(iName);
            fwrite(iName, 1, inamelen, f);
            for (i3 = 0; i3 < (16 - inamelen); i3++)
                fputc(0, f);
        }
    }

    /* wavetable - length */
    b1 = (unsigned char)(SIDRHinstrUsed * 5);
    fputc(b1, f);

    /* empty instrument wavetable (left) */
    fputc(0x09, f); fputc(0xFF, f);
    fputc(0, f); fputc(0, f); fputc(0, f);

    /* instrument wavetable data (left) */
    for (i = 0; i < SIDRHinstrUsed - 2; i++) {
        i2 = i * 8;
        ArpStyle = (long)SIDfile[SIDRHinstrStart + i2 + 7];
        if (ArpStyle & 1)
            ArpSetkeybit = 0;
        else
            ArpSetkeybit = 1;

        b1 = SIDfile[SIDRHinstrStart + i2 + 2];
        fputc(b1, f);

        if (ArpStyle & 1) {
            b1 = (unsigned char)(0x80 | ArpSetkeybit);
            fputc(b1, f);
        } else {
            b1 = (SIDfile[SIDRHinstrStart + i2 + 2] & 0xFE) | ArpSetkeybit;
            fputc(b1, f);
        }

        b1 = (SIDfile[SIDRHinstrStart + i2 + 2] & 0xFE) | (unsigned char)ArpSetkeybit;
        if (ArpStyle & 4) {
            fputc(b1, f);
            fputc(b1, f);
            fputc(0xFF, f);
        } else {
            fputc(b1, f);
            fputc(0xFF, f);
            fputc(0xFF, f);
        }
    }

    /* empty instrument wavetable (right) */
    fputc(0, f); fputc(0, f); fputc(0, f); fputc(0, f); fputc(0, f);

    /* instrument wavetable data (right) */
    for (i = 0; i < SIDRHinstrUsed - 2; i++) {
        i2 = i * 8;
        ArpStyle = (long)SIDfile[SIDRHinstrStart + i2 + 7];
        ArpNote = (ArpStyle & 0xF0) / 16;
        if (ArpNote == 0) ArpNote = 0x74;

        fputc(0, f);

        if ((ArpStyle & 1) || (ArpStyle & 4)) {
            b1 = (unsigned char)(0x80 - ArpNote);
            fputc(b1, f);
        } else {
            fputc(0, f);
        }

        if (ArpStyle & 4) {
            fputc(0, f);
            b1 = (unsigned char)(0x80 - ArpNote);
            fputc(b1, f);
            b1 = (unsigned char)(((i + 2) * 5) - 2);
            fputc(b1, f);
        } else {
            fputc(0, f); fputc(0, f); fputc(0, f);
        }
    }

    /* pulsetable - length */
    b1 = (unsigned char)(SIDRHinstrUsed * 2);
    fputc(b1, f);
    fputc(0x80, f);
    fputc(0xFF, f);

    for (i = 0; i < SIDRHinstrUsed - 2; i++) {
        i2 = i * 8;
        b1 = SIDfile[SIDRHinstrStart + i2 + 1] | 0x80;
        fputc(b1, f);
        fputc(0xFF, f);
    }

    fputc(0, f); fputc(0, f);

    for (i = 0; i < SIDRHinstrUsed - 2; i++) {
        i2 = i * 8;
        b1 = SIDfile[SIDRHinstrStart + i2 + 0];
        fputc(b1, f);
        fputc(0, f);
    }

    /* empty filter table */
    fputc(2, f); fputc(0x11, f); fputc(0xFF, f); fputc(0x22, f); fputc(1, f);

    /* pattern data */
    fputc((unsigned char)GoatPatternMax, f);
    for (i = 0; i < GoatPatternMax; i++) {
        i2 = GoatPLength[i] / 4;
        fputc((unsigned char)i2, f);
        for (i2 = 0; i2 < GoatPLength[i]; i2++) {
            unsigned char val = (unsigned char)GoatPattern[i][i2];
            fputc(val, f);
        }
    }

    fclose(f);

    AddTextS("\r\n*** CONVERTED FILE SAVED SUCCESSFULLY TO: ***\r\n");
    AddTextS(outpath);
    AddTextS("\r\n");

    return 0;
}

/* ------------------------------------------------------------------ */
/* loadfile / convert                                                  */
/* ------------------------------------------------------------------ */

static int convert_sid(const char *inpath, const char *outpath)
{
    FILE *f;
    long fslen, i, i2;
    unsigned char b1, b2;
    int instrused, svcompare, DoRip;
    char txt[256];

    AddTextS("Loading:\r\n");
    AddTextS(inpath);
    AddTextS("\r\n");

    f = fopen(inpath, "rb");
    if (!f) {
        AddTextS("\r\n*** CAN'T OPEN INPUT FILE ***\r\n");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    fslen = ftell(f);
    if (fslen >= SID_MAX) {
        AddTextS("\r\n*** FILE TOO LARGE ***\r\n");
        fclose(f);
        return -1;
    }

    fseek(f, 0, SEEK_SET);
    b1 = (unsigned char)fgetc(f);
    fseek(f, 1, SEEK_SET);
    b2 = (unsigned char)fgetc(f);
    if (b2 != 'S') goto fileError;
    {
        unsigned char b3, b4;
        fseek(f, 2, SEEK_SET);
        b3 = (unsigned char)fgetc(f);
        if (b3 != 'I') goto fileError;
        fseek(f, 3, SEEK_SET);
        b4 = (unsigned char)fgetc(f);
        if (b4 != 'D') goto fileError;
    }
    AddTextS("------------------------------------------------------SID INFO---\r\n");

    /* SID name */
    memset(SIDname, 0, sizeof(SIDname));
    for (i = 0x17; i <= 0x17 + 0x1F; i++) {
        fseek(f, i - 1, SEEK_SET);
        b1 = (unsigned char)fgetc(f);
        if (b1 != 0) {
            size_t len = strlen(SIDname);
            if (len < sizeof(SIDname) - 1)
                SIDname[len] = (char)b1;
        }
    }
    sprintf(txt, "SID Name....: '%s'\r\n", SIDname);
    AddTextS(txt);

    /* author */
    memset(SIDauthor, 0, sizeof(SIDauthor));
    for (i = 0x37; i <= 0x37 + 0x1F; i++) {
        fseek(f, i - 1, SEEK_SET);
        b1 = (unsigned char)fgetc(f);
        if (b1 != 0) {
            size_t len = strlen(SIDauthor);
            if (len < sizeof(SIDauthor) - 1)
                SIDauthor[len] = (char)b1;
        }
    }
    sprintf(txt, "SID Author..: '%s'\r\n", SIDauthor);
    AddTextS(txt);

    /* released */
    memset(SIDreleased, 0, sizeof(SIDreleased));
    for (i = 0x57; i <= 0x57 + 0x1F; i++) {
        fseek(f, i - 1, SEEK_SET);
        b1 = (unsigned char)fgetc(f);
        if (b1 != 0) {
            size_t len = strlen(SIDreleased);
            if (len < sizeof(SIDreleased) - 1)
                SIDreleased[len] = (char)b1;
        }
    }
    sprintf(txt, "SID Released: '%s'\r\n", SIDreleased);
    AddTextS(txt);

    /* load address */
    fseek(f, 0x7D - 1, SEEK_SET);
    b1 = (unsigned char)fgetc(f);
    fseek(f, 0x7E - 1, SEEK_SET);
    b2 = (unsigned char)fgetc(f);
    SIDloadaddr = (long)b2 * 256 + (long)b1;
    sprintf(txt, "SID Loadaddr: $%lX\r\n", SIDloadaddr);
    AddTextS(txt);

    /* subtunes */
    fseek(f, 0x10 - 1, SEEK_SET);
    b1 = (unsigned char)fgetc(f);
    SIDsubtunes = (long)b1;
    sprintf(txt, "SID Subtunes: $%lX\r\n", SIDsubtunes);
    AddTextS(txt);

    /* read entire file */
    fseek(f, 0, SEEK_SET);
    for (i = 0; i < fslen; i++)
        SIDfile[i] = (unsigned char)fgetc(f);
    SIDlength = fslen;
    fclose(f);
    f = NULL;

    AddTextS("-----------------------------------------------------SEARCHING---\r\n");

    SIDRHinstrStart = -1;
    SIDRHinstrUsed = -1;
    SIDRHtrackHi = -1;
    SIDRHtrackLo = -1;
    SIDRHPatternHi = -1;
    SIDRHPatternLo = -1;

    /* find instruments */
    SIDhlenght = 0x7F;
    i = SSearchfile("BD ?? ?? 99 02 D4 48 BD ?? ?? 99 03 D4");
    if (i <= -1) i = SSearchfile("BD ?? ?? 99 02 D4 BD ?? ?? 99 03 D4");
    if (i <= -1) i = SSearchfile("BD ?? ?? 99 ?? ?? 48 BD ?? ?? 99 ?? ?? 48");
    if (i <= -1) {
        AddTextS("*** CAN'T FIND INSTRUMENTS ***\r\n");
        goto FindSubSongs;
    }
    b1 = SIDfile[i + 1];
    b2 = SIDfile[i + 2];
    i = (long)b2 * 256 + (long)b1;
    SIDRHinstrStart = i - SIDloadaddr + SIDhlenght - 1;
    sprintf(txt, "Found Instruments at....: $%lX\r\n", i);
    AddTextS(txt);

    i = SIDRHinstrStart + 2;
    instrused = 0;
    do {
        svcompare = 0;
        for (i2 = 0; i2 < 20; i2++) {
            if ((long)SIDfile[i] == vwaveforms[i2])
                svcompare++;
        }
        if (svcompare <= 0) break;
        i += 8;
        if (i >= SIDlength) {
            i = SIDRHinstrStart + 2 + 8;
            AddTextS("*** CAN'T FIND INSTRUMENT-END, SET TO DEFAULT (1 INSTRUMENT) ***\r\n");
            break;
        }
        instrused++;
    } while (1);
    sprintf(txt, "Instruments used........: $%X\r\n", instrused);
    AddTextS(txt);
    SIDRHinstrUsed = instrused;

FindSubSongs:
    SIDRHtrackVoices = 3;
    {
        long so = 3;
        i = SSearchfile("D0 ?? BD ?? ?? 85 ?? BD ?? ?? 85 ?? DE ?? ?? 30 ?? 4C");
        if (i <= -1) i = SSearchfile("D0 ?? BD ?? ?? 85 ?? BD ?? ?? 85 ?? D6 ?? 30 ?? 4C");
        if (i <= -1) i = SSearchfile("D0 ?? BD ?? ?? 85 ?? BD ?? ?? 85 ?? E0 ?? D0 ?? CE");
        if (i <= -1) { i = SSearchfile("8E ?? ?? A8 BD ?? ?? 85 ?? BD ?? ?? 85 ?? BD ?? ?? F0"); so = 5; }
        if (i <= -1) { i = SSearchfile("8D ?? ?? A8 BD ?? ?? 85 ?? BD ?? ?? 85 ?? DE ?? ?? 30"); so = 5; }
        i2 = i;
        if (i <= -1) {
            AddTextS("*** CAN'T FIND TRACKS/SUBSONGS ***\r\n");
            goto FindTrackSelector;
        }
        b1 = SIDfile[i + so];
        b2 = SIDfile[i + so + 1];
        i = (long)b2 * 256 + (long)b1;
        sprintf(txt, "Found Tracks LO at......: $%lX\r\n", i);
        AddTextS(txt);
        SIDRHtrackLo = i - SIDloadaddr + SIDhlenght - 1;

        i = i2;
        b1 = SIDfile[i + so + 5];
        b2 = SIDfile[i + so + 6];
        i = (long)b2 * 256 + (long)b1;
        sprintf(txt, "Found Tracks HI at......: $%lX\r\n", i);
        AddTextS(txt);
        SIDRHtrackHi = i - SIDloadaddr + SIDhlenght - 1;
    }

FindTrackSelector:
    SIDRHtrackSelector = 0;
    {
        long so = 6;
        i = SSearchfile("18 6D ?? ?? AA BD ?? ?? 99 ?? ?? E8 C8 C0 06");
        if (i <= -1) {
            i = SSearchfile("8A 0A 0A AA BD ?? ?? 99 ?? ?? E8 C8 C0 04");
            if (i >= 1) {
                so = 5;
                SIDRHtrackVoices = 2;
                AddTextS("'Human Race' player (2 voices) detected.\r\n");
            }
        }
        if (i <= -1) {
            goto FindPattern;
        }
        SIDRHtrackSelector = 1;
        b1 = SIDfile[i + so];
        b2 = SIDfile[i + so + 1];
        i = (long)b2 * 256 + (long)b1;
        sprintf(txt, "Found Music selector....: $%lX\r\n", i);
        AddTextS(txt);
        SIDRHtrackLo = i - SIDloadaddr + SIDhlenght - 1;
        SIDRHtrackHi = SIDRHtrackLo + SIDRHtrackVoices;
    }

FindPattern:
    {
        long so = 11;
        i = SSearchfile("9D ?? ?? 4C ?? ?? 4C ?? ?? A8 B9 ?? ?? 85 ?? B9 ?? ?? 85 ?? A9 ?? 9D");
        if (i <= -1) { i = SSearchfile("9D ?? ?? 4C ?? ?? A8 B9 ?? ?? 85 ?? B9 ?? ?? 85 ?? A9 ?? 9D"); so = 8; }
        if (i <= -1) { i = SSearchfile("4C ?? ?? 4C ?? ?? A8 B9 ?? ?? 85 ?? B9 ?? ?? 85 ?? BC ?? ?? A9"); so = 8; }
        if (i <= -1) { i = SSearchfile("4C ?? ?? 4C ?? ?? A8 B9 ?? ?? 85 ?? B9 ?? ?? 85 ?? A9 ?? 95"); so = 8; }
        if (i <= -1) { i = SSearchfile("20 ?? ?? 4C ?? ?? A8 B9 ?? ?? 85 ?? B9 ?? ?? 85 ?? A9 ?? 9D"); so = 8; }
        if (i <= -1) { i = SSearchfile("4C ?? ?? A8 B9 ?? ?? 85 ?? B9 ?? ?? 85 ?? A9 ?? 95"); so = 5; }
        if (i <= -1) {
            AddTextS("*** CAN'T FIND PATTERN ***\r\n");
            goto FindPlayerVersion;
        }

        i2 = i;
        b1 = SIDfile[i + so];
        b2 = SIDfile[i + so + 1];
        i = (long)b2 * 256 + (long)b1;
        sprintf(txt, "Found Pattern LO at.....: $%lX\r\n", i);
        AddTextS(txt);
        SIDRHPatternLo = i - SIDloadaddr + SIDhlenght - 1;

        i = i2;
        b1 = SIDfile[i + so + 5];
        b2 = SIDfile[i + so + 6];
        i = (long)b2 * 256 + (long)b1;
        sprintf(txt, "Found Pattern HI at.....: $%lX\r\n", i);
        AddTextS(txt);
        SIDRHPatternHi = i - SIDloadaddr + SIDhlenght - 1;
        SIDRHPattternUsed = (SIDRHPatternHi - SIDRHPatternLo) - 1;
        sprintf(txt, "Pattern used............: $%lX\r\n", SIDRHPattternUsed);
        AddTextS(txt);
    }

FindPlayerVersion:
    SIDRHreadTrackVersion = 0xFF;

    i = SSearchfile("BC ?? ?? B1 ?? C9 FF F0 ?? C9 FE");
    if (i >= 1) SIDRHreadTrackVersion = 0;
    if (i <= -1) {
        i = SSearchfile("BC ?? ?? B1 ?? C9 FE D0 ?? 4C ?? ?? C9 FF");
        if (i >= 1) SIDRHreadTrackVersion = 1;
    }
    if (i <= -1) {
        i = SSearchfile("BC ?? ?? B1 ?? 10 ?? C9 FF F0 ?? C9 FE F0");
        if (i >= 1) SIDRHreadTrackVersion = 2;
    }
    if (i <= -1) {
        i = SSearchfile("B4 ?? B1 ?? C9 FF F0 ?? C9 FE D0");
        if (i >= 1) SIDRHreadTrackVersion = 3;
    }
    if (i <= -1) {
        i = SSearchfile("BC ?? ?? B1 ?? 10 ?? A9 ?? 9D ?? ?? 9D ?? ?? 9D");
        if (i >= 1) SIDRHreadTrackVersion = 4;
    }
    if (i <= -1) {
        i = SSearchfile("BC ?? ?? B1 ?? C9 FF D0 ?? A9 ?? 9D ?? ?? 9D ?? ?? 9D");
        if (i >= 1) SIDRHreadTrackVersion = 5;
    }
    if (i <= -1) {
        i = SSearchfile("B4 ?? B1 ?? 10 ?? C9 FF F0 ?? 29 7F");
        if (i >= 1) SIDRHreadTrackVersion = 6;
    }
    if (i <= -1) {
        i = SSearchfile("BC ?? ?? B1 ?? 10 ?? C9 FF F0 ?? 29 7F");
        if (i >= 1) SIDRHreadTrackVersion = 7;
    }
    sprintf(txt, "Player Trackread version: $%lX\r\n", SIDRHreadTrackVersion);
    AddTextS(txt);

    DoRip = 1;
    AddTextS("--------------------------------------------------------STATUS---\r\n");
    if (SIDRHtrackLo <= 0 || SIDRHtrackHi <= 0) DoRip = 0;
    if (SIDRHPatternLo <= 0 || SIDRHPatternHi <= 0) DoRip = 0;

    if (!DoRip) {
        AddTextS("*** NO HUBBARD PLAYER DETECTED, CAN'T CONVERT, ABORTING ***\r\n");
    } else {
        AddTextS("*** HUBBARD PLAYER DETECTED, CONVERTING ***\r\n");
        AddTextS("----------------------------------------------------CONVERTING---\r\n");
        AddTextS("Please wait...\r\n");
        GoatClear();
        GoatConvertTracks();
        if (GoatConvertPattern() == 0)
            GoatSave(outpath);
        AddTextS("\r\n*** READY  ***\r\n");
    }
    return 0;

fileError:
    if (f) fclose(f);
    AddTextS("\r\n*** AN ERROR OCCURED ***\r\n");
    AddTextS("*** CHECK FILENAME/PATH - ONLY SID/PSID FILES SUPPORTED ***\r\n");
    AddTextS("*** NO FILE SAVED ***\r\n");
    return -1;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    char outpath[1024];
    const char *inpath;
    const char *p;
    int len;

    if (argc < 2) {
        fprintf(stderr, "Usage: h2g <input.sid> [output.sng]\n");
        return 1;
    }

    inpath = argv[1];

    if (argc >= 3) {
        strncpy(outpath, argv[2], sizeof(outpath) - 1);
        outpath[sizeof(outpath) - 1] = '\0';
    } else {
        strncpy(outpath, inpath, sizeof(outpath) - 1);
        outpath[sizeof(outpath) - 1] = '\0';
        /* replace extension */
        p = strrchr(outpath, '.');
        if (p && p > strrchr(outpath, '/') && p > strrchr(outpath, '\\'))
            len = p - outpath;
        else
            len = strlen(outpath);
        outpath[len] = '\0';
        strcat(outpath, ".sng");
    }

    return convert_sid(inpath, outpath);
}
