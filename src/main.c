/**
 * Project: ip-accounting
 * Sub: cisco-collector
 *
 * main.c
 */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_WINSOCK2_H
# include <winsock2.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif

/* Read lines, params, etc. each to max. Length */
#define INPUT_LINE_LENGTH 512

/* structure of defined local ipv4 networks */
typedef struct {
  struct sockaddr_in network;
  struct sockaddr_in netmask;
}LOCALNET;

/**
 * array of gathered (and detected local) ipv4
 * for accounting traffic
 */
typedef struct {
  struct sockaddr_in ip;
  uint64_t packets;
  uint64_t bytes;
  uint64_t unaccounted_packets;
  uint64_t unaccounted_bytes;
}LOCALIPTRAF;

#ifdef HAVE_WINSOCK2_H
WSADATA wsaData;
#endif

/* globals (in this unit) */
static LOCALNET* localnet;
static LOCALIPTRAF* localiptraf;

/**
 * Import definitions of local ipv4 networks.
 *
 * Default File: ./localnet.cfg
 * Content:
 * 192.168.1.0/255.255.255.0
 * 10.5.0.0/255.255.0.0
 * 172.16.1.0/255.255.255.0
 *
 * Format needs to be in 4 Octets/4 Octets.
 * No comments, no spaces, no empty lines.
 */
static int import_localnets(char* localnet_cfg) {
  FILE *fp;
  fp = fopen(localnet_cfg, "r");
  char ch;
  int count;
  count = 0;
  char *line = malloc(sizeof(char) * INPUT_LINE_LENGTH);
  char *network_a = malloc(sizeof(char) * INPUT_LINE_LENGTH);
  char *netmask_a = malloc(sizeof(char) * INPUT_LINE_LENGTH);
  int i;

  if (fp == NULL) {
    fprintf(stderr, "Could not open %s.\n", localnet_cfg);
    return(0);
  }
  do {
    ch = fgetc(fp);
    if (ch == '\n') count++;
  } while (ch != EOF);
  rewind(fp);
  localnet = malloc(count * sizeof(LOCALNET));
  for (i = 0; i < count; i++) {
    if ( (fgets(line, sizeof(char) * INPUT_LINE_LENGTH, fp)) && (strlen(line) > 1) ) {
      sscanf(line, "%[^/]/%[^ \n]", network_a, netmask_a);
      if (inet_aton(network_a, &localnet[i].network.sin_addr) == 0) {
        fprintf(stderr, "Syntax error in line %d in %s. Expecting N.N.N.N/M.M.M.M (Invalid Network %s)\n",
            i+1,
            localnet_cfg,
            network_a);
        fclose(fp);
        free(line);
        free(network_a);
        free(netmask_a);
        return(0);
      }
      if (inet_aton(netmask_a, &localnet[i].netmask.sin_addr) == 0) {
        fprintf(stderr, "Syntax error in line %d in %s. Expecting N.N.N.N/M.M.M.M (Invalid Netmask %s)\n",
            i+1,
            localnet_cfg,
            netmask_a);
        fclose(fp);
        free(line);
        free(network_a);
        free(netmask_a);
        return(0);
      }
      //printf("Localnet %d added %s/%s\n", i+1 , inet_ntoa(localnet[i].network.sin_addr), inet_ntoa(localnet[i].netmask.sin_addr));
    }
  }
  fclose(fp);
  free(line);
  free(network_a);
  free(netmask_a);
  return count;
}

/**
 * check the given ip against the localnet definitions.
 */
static bool ip_is_local(struct sockaddr_in ip) {
  int i;
  for (i = 0; i < sizeof(LOCALNET)/sizeof(*localnet); i++) {
    if ((localnet[i].network.sin_addr.s_addr & localnet[i].netmask.sin_addr.s_addr) == (ip.sin_addr.s_addr & localnet[i].netmask.sin_addr.s_addr)) {
      return(1); // Match
    }
  }
  return(0); // No match
}

/**
 * insert (or update) an entry with traffic from a single
 * SRC/DST. This aggregates to accountable IP's.
 */
static bool add_traffic_to_ip(struct sockaddr_in ip, uint64_t packets, uint64_t bytes, uint64_t unaccounted_packets, uint64_t unaccounted_bytes) {
  long unsigned int i;
  int ret = 0;
  printf("sizeof localiptraf array %lu", sizeof(LOCALIPTRAF)/sizeof(*localiptraf));
  for ( i=0; i < sizeof(LOCALIPTRAF)/sizeof(*localiptraf); i++) {
    if (localiptraf[i].ip.sin_addr.s_addr == 0) {
      localiptraf[i].ip = ip;
      localiptraf[i].packets = packets;
      localiptraf[i].bytes = bytes;
      localiptraf[i].unaccounted_packets = unaccounted_packets;
      localiptraf[i].unaccounted_bytes = unaccounted_bytes;
      ret = 1;
      break;
    } else if (localiptraf[i].ip.sin_addr.s_addr == ip.sin_addr.s_addr) {
      localiptraf[i].packets += packets;
      localiptraf[i].bytes += bytes;
      localiptraf[i].unaccounted_packets += unaccounted_packets;
      localiptraf[i].unaccounted_bytes += unaccounted_bytes;
      ret = 1;
      break;
    }
  }
  return(ret);
}

/**
 * collect the snmptable content,
 * filter,
 * and fill an array of struct.
 */
static bool account_ip(char *cmd) {
  FILE *fp;
  FILE *pp;
  char *line;
  char *src_a;
  char *dst_a;
  struct sockaddr_in src;
  struct sockaddr_in dst;
  uint64_t packets;
  uint64_t bytes;
  uint64_t violations; // unused
  int count = 0;
  int i;
  char ch;

  line = malloc(sizeof(char) * INPUT_LINE_LENGTH);
  src_a = malloc(sizeof(char) * INPUT_LINE_LENGTH);
  dst_a = malloc(sizeof(char) * INPUT_LINE_LENGTH);
  fp = tmpfile();
  if (fp == NULL) {
    fprintf(stderr, "Could not execute %s.\n", cmd);
    fclose(fp);
  }
  pp = popen(cmd, "r");
  if (pp == NULL) {
    fprintf(stderr, "Could not execute %s.\n", cmd);
    pclose(pp);
  }
  if ( (fp == NULL) || (pp == NULL) ) {
    free(line);
    free(src_a);
    free(dst_a);
    return(0);
  }
  do {
    ch = fgetc(pp);
    fputc(ch, fp);
    if (ch == '\n') count++;
  } while (ch != EOF);
  pclose(pp);
  rewind(fp);
  localiptraf = malloc(count * sizeof(LOCALIPTRAF) * 2); // this might be more than we need, but we don't need to realloc() in every single iteration
  if (localiptraf == NULL) {
    fprintf(stderr, "Not enough memory, to allocate traffic table.\n");
    fclose(fp);
    free(line);
    free(src_a);
    free(dst_a);
    return(0);
  }

  for (i = 0; i < count; i++) {
    packets = 0;
    bytes = 0;
    violations = 0;
    if ( (fgets(line, sizeof(char) * INPUT_LINE_LENGTH, fp)) &&  (i > 2) ) {
      sscanf(line, "%[^,],%[^,],%"SCNu64",%"SCNu64",%"SCNu64"\n", src_a, dst_a, &packets, &bytes, &violations);
      if (inet_aton(src_a, &src.sin_addr) == 0) {
        fprintf(stderr, "Skipping Syntax error in input. Expecting S.S.S.S,D.D.D.D,packets,bytes,violations (Invalid Source IP %s)\n", src_a);
        continue;
      }
      if (inet_aton(dst_a, &dst.sin_addr) == 0) {
        fprintf(stderr, "Skipping Syntax error in input. Expecting S.S.S.S,D.D.D.D,packets,bytes,violations (Invalid Destination IP %s)\n", dst_a);
        continue;
      }
      if ( ip_is_local(src) && !ip_is_local(dst) )  {
        add_traffic_to_ip(src, packets, bytes, 0, 0);
      } else if ( ip_is_local(dst) && !ip_is_local(src) )  {
        add_traffic_to_ip(dst, packets, bytes, 0, 0);
      } else if ( ip_is_local(src) && ip_is_local(dst) )  { /* local traffic accounts to both: SRC and DST. */
        add_traffic_to_ip(src, 0, 0, packets, bytes);
        add_traffic_to_ip(dst, 0, 0, packets, bytes);
      }
    }
  }
  free(line);
  free(src_a);
  free(dst_a);
  fclose(fp);
  return(1);
}

static void print_accounted() {
  long unsigned int i;
  printf("IP,packets,bytes,unaccounted_packets,unaccounted_bytes\n");
  for ( i=0; i < sizeof(*localiptraf)/sizeof(LOCALIPTRAF); i++) {
    if (localiptraf[i].ip.sin_addr.s_addr != 0) {
      printf("%s,%"SCNu64",%"SCNu64",%"SCNu64",%"SCNu64"\n",
          inet_ntoa(localiptraf[i].ip.sin_addr),
          localiptraf[i].packets,
          localiptraf[i].bytes,
          localiptraf[i].unaccounted_packets,
          localiptraf[i].unaccounted_bytes);
    } else {
      break;
    }
  }
}
/**
 * well, the good old main()
 */
int main() {

  int ret = EXIT_FAILURE;
  char *localnet_cfg = "./../conf/localnet.cfg"; /* @TODO */
  char *account_cmd = "snmptable -m ALL -Cf , -Cb -CB -c public -v 2c oob-rtr-1-01 lipAccountingTable"; /* @TODO */
#ifdef HAVE_WINSOCK2_H
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

  if(iResult != NO_ERROR)
    fprintf(stderr, "Error at WSAStartup().\n");
  }
#endif

  if (import_localnets(localnet_cfg) == 0) {
    fprintf(stderr, "No useful N.N.N.N/M.M.M localnet definitions found in %s\n", localnet_cfg);
  } else {
    if (account_ip(account_cmd) == 0) {
      fprintf(stderr, "Error in account_ip()\n");
    } else {
      print_accounted();
      ret = EXIT_SUCCESS;
    }
  }

  free(localnet);
  free(localiptraf);
#ifdef HAVE_WINSOCK2_H
  WSACleanup();
#endif
  return(ret);
}

