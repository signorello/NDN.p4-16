#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ether.h>
#include <time.h>
#include <unistd.h>

#include "defs.h"


extern unsigned int PARAM_pktcount;
extern unsigned int PARAM_sendpktinterval;
extern char * ifacename;
extern char custom_deth;//bool
extern unsigned char * name;
extern char * filename;



//=========================================================================
int main(int argc, char** args) {

  int sockfd;
  struct ifreq ifr;
  unsigned int ptr = sizeof(struct ether_header) + 1;

  unsigned char sendbuf[256];
  
   /*
  struct timespec ts;  //to wait in sending multiple packets
  ts.tv_sec = 1;
  ts.tv_nsec = 0;
*/
  //Initialize globals
  ifacename = NULL;
  name = NULL;
  PARAM_pktcount = 1;


  // 0 -- Interpret program arguments
  interpret(argc, args, sendbuf);



  // 1 -- Open RAW socket to send on
  // ------ SOCK_RAW leaves the Ethernet header to be written by the programmer
  // ------ SOCK_DGRAM makes the operating system fill it in automatically
  if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(0x8624))) == -1) {
    perror("socket()");
    //exit(3);
  }


  // 2 -- Requesting to know an interface's MAC address, given its name
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, ifacename, IFNAMSIZ-1);
  
    //SIOCGIFHWADDR, SIOCSIFHWADDR
    //   Get or set the hardware address of a device using ifr_hwaddr. (from Linux man)
  if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0)
    perror("Error was SIOCGIHWADDR");

  const unsigned char * mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;

  // 3 -- Filling in packet
  struct ether_header * eh = (struct ether_header *) sendbuf;


  // Ethernet header
   //NOTICE: We're pointing to the buffer! The following statements set data!
  if (!custom_deth) {
   eh->ether_dhost[0] = 0;
   eh->ether_dhost[1] = 1;
   eh->ether_dhost[2] = 0;
   eh->ether_dhost[3] = 0;
   eh->ether_dhost[4] = 0;
   eh->ether_dhost[5] = 1 - mac[5]; //yields 0 or 1 -- it's for the simple topo with 2 hosts
  }

  eh->ether_shost[0] = mac[0];
  eh->ether_shost[1] = mac[1];
  eh->ether_shost[2] = mac[2];
  eh->ether_shost[3] = mac[3];
  eh->ether_shost[4] = mac[4];
  eh->ether_shost[5] = mac[5];

  eh->ether_type = htons(0x8624);



  const size_t pktsize = filename != NULL ? fill_data(sendbuf, name, &ptr, filename) :
	fill_interest(sendbuf, name, &ptr);
  // fill_interest() returns pktsize on top of filling the Interest's fields

  unsigned int i;
/*
  for(i=0 ; i < sizeof(struct ether_header) + pktsize + TYPE + LENCODE ; i++)
    if (i < sizeof(struct ether_header) )
	printf("Character no %d: %02x\n", i, sendbuf[i]);
    else
	printf("Character no %d: %c\n", i, sendbuf[i]);
*/
  //SIOCGIFINDEX - Retrieve the index of the interface into ifr_ifindex.
  if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0)
    perror("ERROR: SIOCGIFINDEX or ifr_ifindex.");

  struct sockaddr_ll socket_address;

  //Prepare the socket_address struct
  socket_address.sll_family = PF_PACKET;
  //socket_address.sll_protocol = 0xaaaa;
  socket_address.sll_ifindex = ifr.ifr_ifindex;
  socket_address.sll_hatype = ARPHRD_ETHER;
  socket_address.sll_pkttype = PACKET_OTHERHOST;
  socket_address.sll_halen = ETH_ALEN;

  // Destination MAC. No need to fill this because we're using SOCK_RAW
  // That means the operating system will not fill an Ethernet header
  /*socket_address.sll_addr[0] = 0;
  socket_address.sll_addr[1] = 0;
  socket_address.sll_addr[2] = 0;
  socket_address.sll_addr[3] = 0;
  socket_address.sll_addr[4] = 0;
  socket_address.sll_addr[5] = 3 - mac[5];*/

  //send() is only used while in a connected state with known recipient
  //sendto() is what we're looking for.
  //unsigned int acc = 0;
  for(i=0 ; i < PARAM_pktcount; i++) {
    if (usleep(PARAM_sendpktinterval) != 0) {
      perror("Didn't wait fully!\n");
    } 
    if ( sendto(sockfd, sendbuf, pktsize+TYPE+LENCODE+sizeof(struct ether_header), 0, 
	      (struct sockaddr*)&socket_address, sizeof(socket_address)) < 0) {

       perror("ERROR on sending the packet :'(");
    }
  }


  return 0;
}
