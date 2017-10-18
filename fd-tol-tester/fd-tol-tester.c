/*******************************************************************
  CAN FD tolerant cores tester utilizing kvaser mhydra

  fd-tol-tester.c - main and only file for now

  (C) Copyright 2017 by Pavel Pisa
      e-mail:   pisa@cmp.felk.cvut.cz
      license:  any combination GPL, LGPL, MPL or BSD licenses

  Tester code based on canfdwrite.c copyrighted as
         Copyright 2012-2016 by Kvaser AB, Molndal, Sweden
                        http://www.kvaser.com
  Dual licensed under the following two licenses:
  BSD-new and GPLv2. You may use either one. See the included
  COPYING file for details.

 *******************************************************************/
#define _POSIX_C_SOURCE 201100L

#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <canlib.h>

#define HAS_GETOPT_LONG

int param_channeltx = 0;
int param_channelrx = 1;
int param_bitrate =    500000;
int param_bitratefd = 2000000;

int cantx_hnd;
int canrx_hnd;
int canrx_set = 0;
int cantx_opened;
int canrx_opened;

typedef struct {
  int bitrate;
  int samplept;
  int constant;
} kvaser_bitratesp_t;

const kvaser_bitratesp_t
kvaser_bitratesp[] = {
  {  500000, 80, canFD_BITRATE_500K_80P },
  { 1000000, 80, canFD_BITRATE_1M_80P },
  { 2000000, 80, canFD_BITRATE_2M_80P },
  { 4000000, 80, canFD_BITRATE_4M_80P },
  { 4000000, 60, canFD_BITRATE_8M_60P },
  {       0,  0, 0 },
};

int canfd_dlc_list[] = {
  0,
  1,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  12,
  16,
  20,
  24,
  32,
  48,
  64
};

static void
check(char* id, canStatus stat)
{
  if (stat != canOK) {
    char buf[50];
    buf[0] = '\0';
    canGetErrorText(stat, buf, sizeof(buf));
    fprintf(stderr, "%s: failed, stat=%d (%s)\n", id, (int)stat, buf);
  }
}

int
write_next_seq_can_msg(int hnd, unsigned int sn)
{
  int stat;
  int dlc;
  unsigned int id = sn << 4;
  unsigned char msg[8];
  int i;

  dlc = sn & 0x7;


  msg[0] = (sn >> 0) & 0xff;
  msg[1] = (sn >> 8) & 0xff;

  for (i = 2; i < dlc; i++)
    msg[i] = rand();

  stat = canWrite (hnd, (id | 0) & 0x1fff, msg, dlc, canMSG_EXT * 0);
  check("canWrite seq msg std", stat);

  if (dlc < 2)
    dlc = 2;

  stat = canWrite (hnd, (id | 1) & 0x1fffffff, msg, dlc, canMSG_EXT * 1);
  check("canWrite seq msg ext", stat);

  return stat;
}

int
write_next_seq_can_msg_fd(int hnd, unsigned int sn)
{
  int stat;
  int dlc;
  unsigned int id = sn << 4;
  unsigned char msg[64];
  int i;

  dlc = canfd_dlc_list[sn & 0xf];

  msg[0] = (sn >> 0) & 0xff;
  msg[1] = (sn >> 8) & 0xff;

  for (i = 2; i < dlc; i++)
    msg[i] = rand();

  stat = canWrite (hnd, (id | 2) & 0x1fff, msg, dlc, canFDMSG_FDF);
  check("canWrite seq msg std", stat);

  if (dlc < 2)
    dlc = 2;

  for (i = 0; i < dlc; i++)
    msg[i] = 0xff;

  stat = canWrite (hnd, (id | 3) & 0x1fffffff, msg, dlc, canMSG_EXT |
                                          canFDMSG_FDF | canFDMSG_BRS );
  check("canWrite seq msg ext", stat);

  return stat;
}

void
can_bus_cleanup(void)
{
  int stat;
  if (cantx_opened) {
    stat = canBusOff(cantx_hnd);
    check("canBusOff", stat);
    stat = canClose(cantx_hnd);
    check("canClose", stat);
    cantx_opened = 0;
  }

  if (canrx_opened) {
    stat = canBusOff(canrx_hnd);
    check("canBusOff", stat);
    stat = canClose(canrx_hnd);
    check("canClose", stat);
    canrx_opened = 0;
  }
}

void
sig_handler(int sig)
{
  fprintf(stderr, "sinal %d received, exitting\n", sig);
  can_bus_cleanup();
  exit(1);
}

void
setup_sig_handler(void)
{
  struct sigaction sigact;

  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = sig_handler;
  sigaction(SIGINT, &sigact, NULL);
  sigaction(SIGTERM, &sigact, NULL);
}

static void
usage(void)
{
  printf("Usage:  <parameters>\n");
  printf("  -c, --channeltx <number> CAN channel number for messages send\n");
  printf("  -C, --channelrx <number> CAN channel number for receive\n");
  printf("  -b, --bitrate <number>   bitrate\n");
  printf("  -b, --bitratefd <number> data section bitrate\n");
  printf("  -s, --samplept <number>  samplepoint\n");
  printf("  -V, --version            show version\n");
  printf("  -h, --help               this usage screen\n");
}

int main(int argc, char** argv)
{
  static struct option long_opts[] = {
    { "channeltx", 1, 0, 'c' },
    { "channelrx", 1, 0, 'C' },
    { "bitrate",   1, 0, 'b' },
    { "bitratefd", 1, 0, 'B' },
    { "samplept",  1, 0, 'b' },
    { "version",   0, 0, 'V' },
    { "help",      0, 0, 'h' },
    { 0, 0, 0, 0}
  };
  int opt;
  char *p;
  int bitratesp = canFD_BITRATE_500K_80P;
  int bitratespfd = canFD_BITRATE_2M_80P;
  int stat;
  unsigned int seqnum = 0;

 #ifndef HAS_GETOPT_LONG
  while ((opt = getopt(argc, argv, "c:C:b:B:Vh")) != EOF)
 #else
  while ((opt = getopt_long(argc, argv, "c:C:b:B:Vh",
                            &long_opts[0], NULL)) != EOF)
 #endif
  {
    switch(opt) {
    case 'c':
      param_channeltx = strtol(optarg, &p, 0);
      if (p == optarg) {
        fprintf(stderr, "%s: cannot parse channeltx\n", argv[0]);
        exit(1);
      }
      break;
    case 'C':
      param_channelrx = strtol(optarg, &p, 0);
      if (p == optarg) {
        fprintf(stderr, "%s: cannot parse channelrx\n", argv[0]);
        exit(1);
      }
      canrx_set = 1;
      break;
    case 'b':
      param_bitrate = strtol(optarg, &p, 0);
      if (p == optarg) {
        fprintf(stderr, "%s: cannot parse bitrate\n", argv[0]);
        exit(1);
      }
      break;
    case 'B':
      param_bitratefd = strtol(optarg, &p, 0);
      if (p == optarg) {
        fprintf(stderr, "%s: cannot parse bitratefd\n", argv[0]);
        exit(1);
      }
      break;
    case 'V':
      fputs("fd-tol-tester v0.0\n", stdout);
      exit(0);
    case 'h':
    default:
      usage();
      exit(opt == 'h' ? 0 : 1);
    }
  }

  setup_sig_handler();
  atexit(can_bus_cleanup);

  cantx_hnd = canOpenChannel(param_channeltx, canOPEN_CAN_FD |
                             canOPEN_EXCLUSIVE | canOPEN_REQUIRE_EXTENDED);
  if (cantx_hnd < 0) {
    fprintf(stderr, "canOpenChannel %d", param_channeltx);
    check("", cantx_hnd);
    return -1;
  }
  cantx_opened = 1;

  stat = canSetBusParams(cantx_hnd, bitratesp, 0, 0, 0, 0, 0);
  check("canSetBusParams", stat);
  if (stat != canOK) {
    goto ErrorExit;
  }

  stat = canSetBusParamsFd(cantx_hnd, bitratespfd, 0, 0, 0);
  check("canSetBusParamsFd", stat);
  if (stat != canOK) {
    goto ErrorExit;
  }

  if (canrx_set) {
    canrx_hnd = canOpenChannel(param_channelrx, canOPEN_CAN_FD |
                               canOPEN_EXCLUSIVE | canOPEN_REQUIRE_EXTENDED);
    if (canrx_hnd < 0) {
      fprintf(stderr, "canOpenChannel %d", param_channeltx);
      check("", canrx_hnd);
      return -1;
    }
    canrx_opened = 1;

    stat = canSetBusParams(canrx_hnd, bitratesp, 0, 0, 0, 0, 0);
    check("canSetBusParams", stat);
    if (stat != canOK) {
      goto ErrorExit;
    }

    stat = canSetBusParamsFd(canrx_hnd, bitratespfd, 0, 0, 0);
    check("canSetBusParamsFd", stat);
    if (stat != canOK) {
      goto ErrorExit;
    }
  }

  stat = canBusOn(cantx_hnd);
  check("canBusOn", stat);
  if (stat != canOK) {
    goto ErrorExit;
  }

  if (canrx_opened) {
    stat = canBusOn(canrx_hnd);
    check("canBusOn", stat);
    if (stat != canOK) {
      goto ErrorExit;
    }
  }

  while (1) {
    seqnum++;
    if (!(seqnum & 0x3)) {
      struct timespec wait_time;
      wait_time.tv_nsec = 100 * 1000*1000;
      wait_time.tv_sec = 0;
      clock_nanosleep(CLOCK_MONOTONIC, 0 * TIMER_ABSTIME, &wait_time, NULL);
    }
    write_next_seq_can_msg_fd(cantx_hnd, seqnum);
    write_next_seq_can_msg(cantx_hnd, seqnum);
    stat = canWriteSync(cantx_hnd, (seqnum << 2) & 0x1ff0);
    check("canWriteSync", stat);
  }

  can_bus_cleanup();

  return 0;

ErrorExit:
  can_bus_cleanup();
  exit(1);
}
