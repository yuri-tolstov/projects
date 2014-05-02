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
"   None.\n"
"";

/*---------------------------------------------------------------------------*/
/* Constants, types, and static/global variables                             */
/*---------------------------------------------------------------------------*/
#define MAXMSGLEN	0x10000	/*64 KB*/

/*---------------------------------------------------------------------------*/
/* Command-line options and defaults                                         */
/*---------------------------------------------------------------------------*/
asm(" .align 4");
static int verbose = 0;

/*---------------------------------------------------------------------------*/
/* Callback funciton.                                                        */
/*---------------------------------------------------------------------------*/
void gotpkt(u_char *args, const struct pcap_pkthdr *pkth, const u_char *pkt);

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
   /*Display program information.*/
   printf("%s\n", progid);

   /*Setup data buffer.*/
   buf = malloc(BUFSIZ);
   if (buf == NULL) {e = 1;  c = errno;  goto main_exit;}

   /*-------------------------------------------------------------------------*/
   /* Open the access to the device.                                          */
   /*-------------------------------------------------------------------------*/
   pcap_t *pccd; /*PCAP session descriptor.*/
   char *dev = "eth1"; /*Device to sniff on.*/
   char ebuf[PCAP_ERRBUF_SIZE];
   char filter[] = "port 5060";
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
   /* Process the packets.                                                    */
   /*-------------------------------------------------------------------------*/
#if 0
   const u_char *pkt; /*Actual packet*/
   struct pcap_pkthdr pkth; /*The packet header*/
   pkt = pcap_next(pccd, &pkth);
   printf("Jacked a packet with length %d\n", pkth.len);
#else
   int npkts = 100;
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
/* Function:   gotpkt                                                        */
/*****************************************************************************/
void gotpkt(u_char *args, const struct pcap_pkthdr *pkth, const u_char *pkt)
{
   static int count = 1; /* Packet counter */
	
   printf("Packet number %d:\n", count);
   count++;
}

