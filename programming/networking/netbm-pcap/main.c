/*****************************************************************************/
/* File: netbm-pcap/main.c                                                   */
/*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <pcap.h>

/*---------------------------------------------------------------------------*/
#define VERSION	"1.0.0"
/*---------------------------------------------------------------------------*/
char *progid = ""
"NetBM PCAP: Version" VERSION " " __DATE__ " " __TIME__
"";

char *helpdump = ""
"Usage: netbm-pcap [options]\n"
"Options\n"
"   --dev=ethX          [default=eth1]\n"
"   --filter=\"<string>\" [default=\"ip\"]\n"
"";

/*---------------------------------------------------------------------------*/
/* Constants, types, and static/global variables                             */
/*---------------------------------------------------------------------------*/
int64_t g_pktcnt;
int64_t g_totlen;

/*---------------------------------------------------------------------------*/
/* Command-line options and defaults                                         */
/*---------------------------------------------------------------------------*/
#define FILTERSIZ   256

asm(" .align 4");
static int verbose = 0;
static char dev[IFNAMSIZ]; /*Device to sniff on.*/
static char filter[FILTERSIZ];

/*---------------------------------------------------------------------------*/
/* Funcgtion prototypes.                                                     */
/*---------------------------------------------------------------------------*/
void* monitor_thread(void *parm);
void gotpkt(u_char *args, const struct pcap_pkthdr *pkth, const u_char *pkt);
double tmdiff(struct timespec t0, struct timespec t1);

/*---------------------------------------------------------------------------*/
/* Macros and inline functions                                               */
/*---------------------------------------------------------------------------*/
#define LOG printf

/*****************************************************************************/
/* Function:    main                                                         */
/*****************************************************************************/
int main(int argc, char *argv[])
{
   int i, c, e, n;         /*index, return code, error number, just number*/
   char *sp;	           /*pointer to a symbol*/
   unsigned int alen;      /*socket address length*/
   int so, sotx = 0;       /*socket descriptor*/
   struct sockaddr_in sora; /*RX socket address*/
   struct sockaddr_in sosa; /*TX socket address*/
   char* buf, *p;          /*message buffer, pointer to data(payload)*/
   int vi;                 /*temporary integer variable*/
   char *vs;               /*temporary char string variable*/
   struct ifreq ifr;       /*socket control requiest*/
   fd_set rd;              /*receive socket descriptor*/
   struct timeval tv;      /*time specification*/
   struct ip *iph;         /*UDP header*/
   struct udphdr *udph;    /*UDP header*/
   uint32_t tu32;          /*temporary U32 variable*/
   uint16_t tu16;          /*temporary U16 variable*/

   /*-------------------------------------------------------------------------*/
   /* Defaults and initialization.                                            */
   /*-------------------------------------------------------------------------*/
   c = e = 0;

   /*-------------------------------------------------------------------------*/
   /* Pick up the cmd-line options.                                           */
   /*-------------------------------------------------------------------------*/
   for (c = 1; c < argc; c++) {
      sp = argv[c];
      /*Verbose*/
      if (strncmp(sp,"--verbose=",10) == 0) {
         sp = sp + 10;
         verbose = atoi(sp);
      /*Device*/
      } else if (strncmp(sp,"--dev=",6) == 0) {
         sp = sp + 6;
         strncpy(dev, sp, IFNAMSIZ);
      /*Filter Program*/
      } else if (strncmp(sp,"--filter=",9) == 0) {
         sp = sp + 9;
         strncpy(filter, sp, FILTERSIZ);
      /*Help*/
      } else if (strncmp(sp,"--help",6) == 0) {
         printf("%s\n", progid);
         printf("%s\n", helpdump);
         return EXIT_FAILURE;
      } else {
         printf("wrong option(s)\n");
         return EXIT_FAILURE;
      }
   }
   if (dev[0] == 0) strcpy(dev, "eth1");
   if (filter[0] == 0) strcpy(filter, "ip");

   /*Display program information.*/
   printf("%s\n", progid);
   printf("Device = %s\n", dev);
   printf("Filter = %s\n", filter);

   /*Setup data buffer.*/
   buf = malloc(BUFSIZ);
   if (buf == NULL) {e = 1;  c = errno;  goto main_exit;}

   /*-------------------------------------------------------------------------*/
   /* Open the access to the device.                                          */
   /*-------------------------------------------------------------------------*/
   pcap_t *pccd; /*PCAP session descriptor.*/
   char ebuf[PCAP_ERRBUF_SIZE];
   struct bpf_program fp; /*Compiled filter expression*/
   bpf_u_int32 mask; /*Netmask of the sniffing device*/
   bpf_u_int32 net; /*IP of the sniffing device*/

   if (pcap_lookupnet(dev, &net, &mask, ebuf) == -1) {
      net = 0;  mask = 0;
   }
   pccd = pcap_open_live(dev, BUFSIZ, 1, 1000, ebuf);
   if (pccd == NULL) {
      e = 2;  c = ENODEV;  goto main_exit;
   }
   if (pcap_compile(pccd, &fp, filter, 0, net) == -1) {
      e = 3;  c = errno;  goto main_exit;
   }
   if (pcap_setfilter(pccd, &fp) == -1) {
      e = 4;  c = errno;  goto main_exit;
   }
   /*-------------------------------------------------------------------------*/
   /* Start the Monitor thread.                                               */
   /*-------------------------------------------------------------------------*/
   pthread_t tid;
   pthread_create(&tid, NULL, monitor_thread, NULL);

   /*-------------------------------------------------------------------------*/
   /* Process the packets.                                                    */
   /*-------------------------------------------------------------------------*/
#if 0
   const u_char *pkt; /*Actual packet*/
   struct pcap_pkthdr pkth; /*The packet header*/
   pkt = pcap_next(pccd, &pkth);
   printf("Jacked a packet with length %d\n", pkth.len);
#else
   int npkts = 0;
   pcap_loop(pccd, npkts, gotpkt, NULL);
#endif

   /*-------------------------------------------------------------------------*/
   /* Close the Session.                                                      */
   /*-------------------------------------------------------------------------*/
   pcap_freecode(&fp);
   pcap_close(pccd);

   /*-------------------------------------------------------------------------*/
   /* Program's exit.                                                         */
   /*-------------------------------------------------------------------------*/
main_exit2:
   free(buf);
main_exit:
   LOG("Main thread is terminated (e=%d c=%d)\n", e, c);
   return EXIT_SUCCESS;
}

/*****************************************************************************/
/* Thread:   monitor_thread                                                  */
/*****************************************************************************/
void* monitor_thread(void *parm)
{
   int64_t n0, n1, dn, l0, l1, dl; /*Counters*/
   struct timespec t0, t1; /*Time*/
   double td; /*Time difference*/

   n0 = g_pktcnt;
   l0 = g_totlen;
   clock_gettime(CLOCK_REALTIME, &t0);

   while (1) {
      sleep(1);
      n1 = g_pktcnt;
      l1 = g_totlen;
      clock_gettime(CLOCK_REALTIME, &t1);
      
      td = tmdiff(t0, t1);
      dn = n1 - n0;
      dl = l1 - l0;

      if (dn > 0 && n1 > 0) {
         printf("dt=%1.3f dn=%ld clen=%ld alen=%ld tlen=%ld tpkts=%ld\n",
                td, dn, dl / dn, l1 / n1, l1, n1);
      }
      t0 = t1;
      n0 = n1;
      l0 = l1;
   }
   return NULL;
}

/*****************************************************************************/
/* Function:   gotpkt                                                        */
/*****************************************************************************/
void gotpkt(u_char *args, const struct pcap_pkthdr *pkth, const u_char *pkt)
{
   // if ((g_pktcnt % 10000) == 0) printf("count=%d\n", g_pktcnt);
   // printf("pktlen=%d\n", pkth->len);
   g_totlen += pkth->len;
   if (g_totlen < 0) { /*Take care of overllap*/
      g_pktcnt = g_totlen = 0;
   }
   else {
      g_pktcnt++;
   }
}

/*****************************************************************************/
/* Function:   tmdiff                                                        */
/*****************************************************************************/
double tmdiff(struct timespec t0, struct timespec t1)
{
   struct timespec tt;
   if ((t1.tv_nsec - t0.tv_nsec) < 0) {
      tt.tv_sec = t1.tv_sec - t0.tv_sec - 1;
      tt.tv_nsec = 1000000000 + t1.tv_nsec - t0.tv_nsec;
   } else {
      tt.tv_sec = t1.tv_sec - t0.tv_sec;
      tt.tv_nsec = t1.tv_nsec - t0.tv_nsec;
   }
   return (double)tt.tv_sec + (double)tt.tv_nsec / 1000000000;
}

