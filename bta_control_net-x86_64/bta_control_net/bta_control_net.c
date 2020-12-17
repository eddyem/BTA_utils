/* (C) V.S. Shergin, SAO RAS */
/* Программа сетевой связи АСУ БТА  */
/* трансляция межпрограммного интерфейса по сети */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/times.h>

/*#define SHM_OLD_SIZE*/
#include "bta_shdata.h"

#ifndef TRUE
#define TRUE (1)
#define FALSE (0)
#endif


static int dsock;                     /* Data  socket */
static int csock;                     /* Command  socket */
static struct sockaddr_in from;       /* Sending host address */
static struct sockaddr_in data;       /* Address for data socket */
static struct sockaddr_in cmd;        /* Address for command socket */
static int dport = 7655;              /* Data port */
static int cport = 7656;              /* Command port */
static int fromlen;           /* Length of sending host address */
static double tsec=0.333333;          /* timeout 1/3sec. */
static double tsync=0.75;             /* "send sync" timeout */
const char *mask_sao = "255.255.224.0";
const char *mcast_base = "239.0.0.0";

static union{
    unsigned char ubuf[4100]; /* buffer area >= sdat.maxsize+2 ! */
    int ival;
} u_buff;
//static unsigned char buff[4100];  /* buffer area >= sdat.maxsize+2 ! */

#define buff u_buff.ubuf

static char *myname;
static double prog_time();
static void my_sleep(double);
static void log_message(char *);
static void myabort(int);

int main(int argc, char *argv[])
{
   int i, length;
   char *host = NULL;
   char  myhost[128],msg[100];
   static struct sched_param shp;
   struct hostent *h;
   int ip, my_ip;
   struct in_addr acs_addr,my_addr,bcast_addr,mcast_addr;
   unsigned char ttl = 1;
   unsigned long maskC,maskSAO;
   struct ip_mreq mr;
   int sync=0,syncnt=0;
   double timeout, mcast_t=0.,mcast_tout=10.;

   myname = argv[0];
   if (argc>1) {
      host = strdup(argv[1]);
      for(i=2; i<argc; i++) {
     if (*argv[i]=='s'){
        sync=1;
        char *p = strchr(argv[i],'=');
        if(p) tsync=atof(p+1);
        if(tsync<0.4) tsync=0.4;
     }else if (*argv[i]=='t') {
        char *p = strchr(argv[i],'=');
        if(p!=NULL) tsec=atof(p+1);
        if(tsec<0.14) tsec=0.14;
     }
      }
   } else {
      fprintf(stderr, "Usage:\n");
      fprintf(stderr, "\t%s BTA_control_host[:mcast_addr] [sync[=sec]]\n",argv[0]);
      fprintf(stderr, "\t%s local [t=sec]\n",argv[0]);
      fprintf(stderr, "\t%s mcast[:mcast_addr][/ttl] [t=sec]\n",argv[0]);
      fprintf(stderr, "\t%s remote\n",argv[0]);
      exit(1);
   }
   signal(SIGHUP, myabort);
   signal(SIGINT, myabort);
   signal(SIGQUIT,myabort);
   signal(SIGFPE, myabort);
   signal(SIGPIPE,myabort);
   signal(SIGSEGV,myabort);
   signal(SIGTERM,myabort);


   maskC = htonl(IN_CLASSC_NET);
   inet_aton(mask_sao,(struct in_addr *)&maskSAO);

   if(gethostname(myhost, sizeof(myhost)) < 0) {
      fprintf(stderr,"Can't get my own host name!?.\n");
      exit(1);
   }
   if ((h = gethostbyname(myhost))) {
      if(h->h_addr_list[1])
     my_ip = *(int*)h->h_addr_list[1];
      else
     my_ip = *(int*)h->h_addr_list[0];
      my_addr.s_addr = my_ip;
      fprintf(stderr,"My host: %s (%s)\n", myhost, inet_ntoa(my_addr));
   } else {
      fprintf(stderr,"Can't get my own host IP-addr!?.\n");
      exit(1);
   }
   bcast_addr.s_addr = (INADDR_BROADCAST & ~maskC) | ( my_ip & maskC);

  /*Create the data socket to send to or to read from*/
   dsock = socket(AF_INET, SOCK_DGRAM, 0);
   if (dsock < 0) {
      perror("opening data socket");
      exit(1);
   }

 /*Create the socket to send or to get commands */
   csock = socket(AF_INET, SOCK_DGRAM, 0);
   if (csock < 0) {
      perror("opening command socket");
      exit(1);
   }

   if (strcmp(host,"local")==0) {              /* Send data to all local hosts?  */
     ip = bcast_addr.s_addr;
     host = NULL;
     i=TRUE;
     setsockopt(dsock, SOL_SOCKET, SO_BROADCAST, &i, sizeof(i));
     fprintf(stderr,"Broadcast(%s) ACS data to all local hosts\n", inet_ntoa(bcast_addr));
   }
   else if(strncmp(host,"mcast",5)==0 ||
       strncmp(host,"multicast",9)==0) { /* Send data to local netwrks?  */
     char *p;
     if((p=strchr(host,'/')) != NULL) {
    ttl = atoi(p+1);
    *p = '\0';
     }
     if((p=strchr(host,':')) != NULL) {
    int hb = atoi(p+1);
    if( (hb>239) | (hb<224) | (inet_aton(p+1,&mcast_addr)==0) ){
       fprintf(stderr,"Trying wrong multicast group address:%s?!\n", p+1);
       goto auto_mcast;
    }
     } else {
auto_mcast:
    inet_aton(mcast_base,&mcast_addr);
    mcast_addr.s_addr = (mcast_addr.s_addr & maskSAO) | ( my_ip & ~maskSAO);
     }
     ip = mcast_addr.s_addr;
     host = NULL;
     if(setsockopt(dsock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))<0) {
    sprintf(msg,"Can't use multicast %s/%d", inet_ntoa(mcast_addr),ttl);
    perror(msg);
    exit(1);
     } else
    fprintf(stderr,"Send ACS data to multicast group %s/%d for SAO hosts\n", inet_ntoa(mcast_addr),ttl);
   }
   else if (strcmp(host,"remote")==0) {        /* Send data to remote hosts?  */
     ip = 0;
     host = NULL;
     fprintf(stderr,"Wait for remote requests to supply with ACS data\n");
   }
   else {
     char *mga=NULL;
     if((mga=strchr(host,':')) != NULL) {
    int hb;
    *mga++ = '\0';
    hb = atoi(mga);
    if( (hb>239) | (hb<224) | (inet_aton(mga,&mcast_addr)==0) ){
       fprintf(stderr,"Trying wrong multicast group address:%s?!\n",mga);
       mga=NULL;
    }
     }
     if ((ip = inet_addr(host)) == (int)INADDR_NONE) { /* Get data from IP-number?   */
    h = gethostbyname(host);                  /* or from named host...*/
    if (h == 0) {
       fprintf(stderr, "%s: unknown host\n", host);
       exit(1);
    }
    ip = *(int*)h->h_addr;
    acs_addr.s_addr = ip;
    fprintf(stderr,"ACS host: %s (%s)\n", h->h_name, inet_ntoa(acs_addr));
     } else {
    acs_addr.s_addr = ip;
    h = gethostbyaddr((char*)&acs_addr, sizeof(acs_addr), AF_INET);
    if (h == 0)
       fprintf(stderr,"ACS host: %s \n", inet_ntoa(acs_addr));
    else
       fprintf(stderr,"ACS host: %s (%s)\n", h->h_name, inet_ntoa(acs_addr));
     }
     if(mga==NULL) {
    inet_aton(mcast_base,&mcast_addr);
    mcast_addr.s_addr = (mcast_addr.s_addr & maskSAO) | ( ip & ~maskSAO);
     }
     mr.imr_multiaddr.s_addr = mcast_addr.s_addr;
     mr.imr_interface.s_addr = htons(INADDR_ANY);
     if (setsockopt(dsock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mr, sizeof(mr)) < 0) {
    sprintf(msg,"Joining multicast group %s", inet_ntoa(mcast_addr));
    perror(msg);
     } else
    fprintf(stderr,"Join multicast group %s\n", inet_ntoa(mcast_addr));

   }
   data.sin_family = AF_INET;
   data.sin_port = htons(dport);
   cmd.sin_family = AF_INET;
   cmd.sin_port = htons(cport);
   if (host) {
      tsec = 0.05;
      data.sin_addr.s_addr = INADDR_ANY;
      cmd.sin_addr.s_addr = ip;
      sdat.mode |= 0200;
      sdat.atflag = 0;
      get_shm_block( &sdat, ServerSide);
      mcmd.mode = ocmd.mode = ucmd.mode = 0622;
      get_cmd_queue( &mcmd, ServerSide);
      get_cmd_queue( &ocmd, ServerSide);
      get_cmd_queue( &ucmd, ServerSide);
      if(ServPID>0 && kill(ServPID, 0) >= 0) {
     fprintf(stderr,"bta_control or bta_control_net server process  already running! (PID=%d)\n", ServPID);
     exit(1);
      }
      /* Listen and receive data packets form another host */
      if (bind(dsock, (struct sockaddr *)&data, sizeof(data)) < 0) {
     perror("binding data socket");
     exit(1);
      }
      length = sizeof(data);
      if (getsockname(dsock, (struct sockaddr *) &data, (socklen_t*)&length) < 0) {
     perror("getting data socket name");
     exit(1);
      }
   } else {
      data.sin_addr.s_addr = ip;
      cmd.sin_addr.s_addr = INADDR_ANY;
      get_shm_block( &sdat, ClientSide);
      get_cmd_queue( &mcmd, ClientSide);
      get_cmd_queue( &ocmd, ClientSide);
      get_cmd_queue( &ucmd, ClientSide);
      /* Listen and receive command packets form another host */
      if (bind(csock, (struct sockaddr *)&cmd, sizeof(cmd)) < 0) {
     perror("binding command socket");
     exit(1);
      }
      length = sizeof(cmd);
      if (getsockname(csock, (struct sockaddr *) &cmd, (socklen_t*)&length) < 0) {
     perror("getting command socket name");
     exit(1);
      }
   }

   shp.sched_priority = 1;
   if (sched_setscheduler(0, SCHED_FIFO, &shp)) {
      perror("Can't enter realtime mode! Not a SuperUser?");
      if (host) tsec = 0.2;
   } else {
      if (mlockall(MCL_CURRENT))
     perror("Can't lock process memory");
      else
     fprintf(stderr,"Entering realtime mode - Ok\n");
   }

   /* Wait and Read from the socket. */
   timeout = tsec;
   while (TRUE) {
      int rll;
      fd_set fdset;
      //unsigned char *p = host?sdat.addr:buff;
      int sock = host?dsock:csock;
      int size = host?(sdat.maxsize+2):1024;
      struct timeval tv;
      int ret, code, csize;
      union {
     unsigned char b[2];
     unsigned short w;
      } cs;
      struct my_msgbuf mbuf, *mbp;

      FD_ZERO(&fdset);
      FD_SET(sock, &fdset);
      tv.tv_sec=(int)timeout;
      tv.tv_usec=(int)((timeout-tv.tv_sec)*1000000.+0.5);

      if ((ret=select(FD_SETSIZE, &fdset, NULL, NULL, &tv)) < 0) {
      perror("select() fault");
      continue;
      }
      rll = 0;
      if (ret>0 && FD_ISSET(sock, &fdset)) {
     fromlen = sizeof(from);
     if ((rll = recvfrom(sock, buff, size, 0, (struct sockaddr *) &from, (socklen_t*)&fromlen)) < 0) {
        if (errno != EINTR) {
           perror("receiving UDP packet");
        }
     } else {
//fprintf(stderr, "Recv time %07.2f ", prog_time());
//fprintf(stderr, "Recv UDP pack (%d bytes) from  %s\n", rll,inet_ntoa(from.sin_addr));
     }
     if (host) {mcast_t = 0.;mcast_tout = 10.;}
      } else if (host) {         /* 2.4.x kernel iface down/up problem?  */
     mcast_t += timeout;                 /* (with recv multicast?) */
     if(mcast_t>mcast_tout) {   /* no packets? */
        mcast_t = 0.;           /* may be need to re-add to multicast group? */
        setsockopt(dsock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&mr, sizeof(mr));
        if (setsockopt(dsock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mr, sizeof(mr)) < 0) {
           sprintf(msg,"ReJoining multicast group %s", inet_ntoa(mcast_addr));
           perror(msg);
        } else if(mcast_tout<999.) {
           fprintf(stderr,"Multicast timeout? ReJoin group %s\n", inet_ntoa(mcast_addr));
           mcast_tout *= 10.;
        }
     }
      }
      if (host) {
     static int last_err = 0;
     static int err_type = 0;
     int currt = time(NULL);
     if(rll>2) {
        struct BTA_Data *pb = (void *)buff;
        ServPID = getpid();
        if(pb->magic != sdat.key.code) {
           if(currt-last_err>60 || err_type!=1) {
          fprintf(stderr,"Wrong shared data (maybe server %s turned off)\n", inet_ntoa(from.sin_addr));
          last_err=currt;
          err_type=1;
           }
        }
        else if(pb->version == 0) {
           if(currt-last_err>60 || err_type!=2) {
          fprintf(stderr,"Null shared data version (maybe server at %s turned off)\n",inet_ntoa(from.sin_addr));
          last_err=currt;
          err_type=2;
           }
        }
        else if(pb->size != sizeof(struct BTA_Data)) {
           int perr = (pb->size>sdat.size&&pb->size<sdat.maxsize)? 3600 : 60;
           if(currt-last_err>perr || err_type!=3) {
          fprintf(stderr,"Wrong shared area size: I needs - %zd, but server %s - %d ...\n",
                   sizeof(struct BTA_Data), inet_ntoa(from.sin_addr), pb->size );
          last_err=currt;
          err_type=3;
          if(pb->size > sdat.maxsize) pb->size = sdat.maxsize;
          if(pb->version != BTA_Data_Ver)
             goto wrongver;
           } else
          if(pb->size > sdat.maxsize) pb->size = sdat.maxsize;
        }
        else if(pb->version != BTA_Data_Ver) {
           if(currt-last_err>600 || err_type!=4) {
          last_err=currt;
          err_type=4;
     wrongver:
          fprintf(stderr,"Wrong shared data version: I'am - %d, but server %s - %d ...\n",
                   BTA_Data_Ver, inet_ntoa(from.sin_addr), pb->version );
           }
        }
        else {
           for(i=0,cs.w=0; i<rll-2; i++)
          cs.w += buff[i];
           if(buff[rll-2] != cs.b[0] || buff[rll-1] != cs.b[1]) {
          if(currt-last_err>60 || err_type!=5) {
             fprintf(stderr,"Wrong CS from %s! %2x%02x %4x\n",
                     inet_ntoa(from.sin_addr),
                  buff[rll-1], buff[rll-2], cs.w);
             last_err=currt;
             err_type=5;
          }
           } else
          err_type=0;
        }
        if(err_type==0 || err_type==3)
           memcpy(sdat.addr, buff, pb->size);
     }
     if(rll<=0 || err_type==0 || err_type==3) {
        if(sync) {
           int nsync = (int)(tsync/tsec+0.5); /* e.g. tsync=0.9,tsec=0.05 => nsync=18 */
           if(nsync<2) nsync=2;
           if(rll>0) syncnt = 1;  /* уже принимаем, не надо sync-запроса*/
           else syncnt = (syncnt+1)%nsync; /* e.g. nsync=18 => 18*0.05=0.9sec */
        }
       /* сначала проверим канал команд главного операторского интерфейса */
       /* и в первую очередь на "ОСТАНОВ"*/
        code = mcmd.key.code;
        ret = msgrcv ( mcmd.id, (struct msgbuf *)&mbuf, 112, StopTel, IPC_NOWAIT);
        if (ret <= 0 ) {
           if (errno != ENOMSG)
          perror("Getting command from 'MainOperator' fault");
        }
        else goto do_cmd;
       /* а затем на все прочие команды */
        ret = msgrcv ( mcmd.id, (struct msgbuf *)&mbuf, 112, 0, IPC_NOWAIT);
        if (ret <= 0 ) {
           if (errno != ENOMSG)
          perror("Getting command from 'MainOperator' fault");
        }
        else goto do_cmd;
       /* затем проверим канал вторичных операторских интерфейсов */
       /* и в первую очередь опять на "ОСТАНОВ"*/
        code = ocmd.key.code;
        ret = msgrcv ( ocmd.id, (struct msgbuf *)&mbuf, 112, StopTel, IPC_NOWAIT);
        if (ret <= 0 ) {
           if (errno != ENOMSG)
          perror("Getting command from 'Operator' fault");
        }
        else goto do_cmd;
       /* а затем на все прочие команды */
        ret = msgrcv ( ocmd.id, (struct msgbuf *)&mbuf, 112, 0, IPC_NOWAIT);
        if (ret <= 0 ) {
           if (errno != ENOMSG)
          perror("Getting command from 'Operator' fault");
        }
        else goto do_cmd;
       /* и наконец канал пользовательских интерфейсов */
        code = ucmd.key.code;
        ret = msgrcv ( ucmd.id, (struct msgbuf *)&mbuf, 112, 0, IPC_NOWAIT);
        if (ret <= 0 ) {
           if (errno != ENOMSG)
          perror("Getting command from 'User' fault");
        /* no commands at all... */
           if(sync && syncnt==0) {
          code = 0;         /*need to send sync pack to remote network */
          mbuf.mtype = 0;
          mbuf.acckey = 0;
          mbuf.src_pid = getpid();
          mbuf.src_ip = my_ip;
          mbuf.mtext[0] = 0;
          ret=1;
           } else
          continue;            /* nothing to send...*/
        }
      do_cmd:
        if(mbuf.src_ip == 0)
           mbuf.src_ip = my_ip;
        u_buff.ival = code;
        //*((int *)buff) = code;
        memcpy(buff+sizeof(code), &mbuf, sizeof(mbuf.mtype)+ret);
        csize = sizeof(code)+sizeof(mbuf.mtype)+ret;
        for(i=0,cs.w=0; i<csize; i++)
           cs.w += buff[i];
        buff[csize++] = cs.b[0];
        buff[csize++] = cs.b[1];
        if (sendto(csock, buff, csize,
           0, (struct sockaddr *)&cmd, sizeof(cmd)) < 0) {
           perror("sending command datagram");
           continue;
        }
/*fprintf(stderr, "Send %d bytes to %s.\n", csize, inet_ntoa(cmd.sin_addr));
 */
     } else {
        /* Shm-data error? Suspicious server! Cmd-queues Cleanup...*/
        ret = msgrcv ( mcmd.id, (struct msgbuf *)&mbuf, 112, 0, IPC_NOWAIT);
        ret = msgrcv ( ocmd.id, (struct msgbuf *)&mbuf, 112, 0, IPC_NOWAIT);
        ret = msgrcv ( ucmd.id, (struct msgbuf *)&mbuf, 112, 0, IPC_NOWAIT);
     }
      } else {
     static double last_recv=0.;
     static double last_send=0.;
     double tcurr = prog_time();
     if(rll > 0) {                              /* recv. somewhat packet ? */
        int id=-1;
        if(rll>2) {
           for(i=0,cs.w=0; i<rll-2; i++)
          cs.w += buff[i];
           if(buff[rll-2] != cs.b[0] || buff[rll-1] != cs.b[1]) {
          fprintf(stderr,"Wrong CS from %s! %2x%02x %4x\n",
                     inet_ntoa(from.sin_addr),
                  buff[rll-1], buff[rll-2], cs.w);
           } else {
          code = u_buff.ival;
          //code = *((int *)buff);
          csize = rll-sizeof(code)-sizeof(mbuf.mtype);
          if(code==mcmd.key.code) id=mcmd.id;
          else if(code==ocmd.key.code) id=ocmd.id;
          else if(code==ucmd.key.code) id=ucmd.id;
           }
        }
        if(id>=0) {                             /* command packet? */
           static unsigned long prev_ip=0;      /* IP-адр.предыдущей команды */
           unsigned long netaddr;
           struct in_addr src_addr;
           char *acc;
           static char *prev_acc=NULL;

           mbp = (struct my_msgbuf *)(buff+sizeof(code));
           netaddr = ntohl(mbp->src_ip);
           if(mbp->src_ip == 0) {
          fprintf(stderr,"Отсутствует адрес источника: 0.0.0.0 (получен от %s)!\n",
                 inet_ntoa(from.sin_addr));
          mbp->src_ip = from.sin_addr.s_addr;
           } else if(((mbp->src_ip&maskC)==(from.sin_addr.s_addr&maskC)) &&
             ((ntohl(from.sin_addr.s_addr)&ACSMask) != (ACSNet & ACSMask)) &&
             (mbp->src_ip != from.sin_addr.s_addr)) {
          src_addr.s_addr = mbp->src_ip;
          fprintf(stderr, "Подозрительный адрес источника: %s (получен от %s)!\n",
              inet_ntoa(src_addr),inet_ntoa(from.sin_addr));
          mbp->src_ip = from.sin_addr.s_addr;
           }
           if(((netaddr & NetMask) == (NetWork & NetMask)) ||
          ((netaddr & ACSMask) == (ACSNet & ACSMask))) {
          msgsnd(id, (struct msgbuf *)mbp, csize, IPC_NOWAIT);
          acc = "Accept";
           } else
          acc = "Failed";
           if( prev_ip != mbp->src_ip || prev_acc != acc) {
          src_addr.s_addr = mbp->src_ip;
          sprintf(msg, "Cmds from %s - %s", inet_ntoa(src_addr),acc);
          if((netaddr & ACSMask) != (ACSNet & ACSMask))
             log_message(msg);
           }
           prev_acc=acc;
           prev_ip=mbp->src_ip;
        }
        if(tcurr-last_recv < 0.1)       /* max 10 cmd-packs per second */
           my_sleep(0.11-(tcurr-last_recv));
        last_recv = tcurr;
     }
     if(ip==0) {         /*"remote"-mode ?*/
        if(rll <= 0)     /* recv. somewhat packet ? */
           continue;
        data.sin_addr.s_addr = from.sin_addr.s_addr;
     }
     if(ip==0 || rll==0 || fabs(last_send-tcurr)>tsec ) { /*"remote"||timeout*/
        struct BTA_Data *pb = (void *)buff;
        memcpy(buff, sdat.addr, sdat.size);
        csize = pb->size = sdat.size;
        for(i=0,cs.w=0; i<csize; i++)
           cs.w += buff[i];
        buff[csize++] = cs.b[0];
        buff[csize++] = cs.b[1];
        if (sendto(dsock, buff, csize,
           0, (struct sockaddr *)&data, sizeof(data)) < 0) {
           perror("sending datagram message");
           continue;
        }
        last_send = tcurr;
        timeout = tsec;

/*fprintf(stderr, "Send time %07.2f\n", last_send);*/
/*fprintf(stderr, "Send %d bytes to %s.\n", sdat.size,
 *          (ip==0)?inet_ntoa(data.sin_addr):"All");
 */
     } else {
        if(rll>0) { // send TCS data block as a reply to cmd
           struct BTA_Data *pb = (void *)buff;
           memcpy(buff, sdat.addr, sdat.size);
           csize = pb->size = sdat.size;
           for(i=0,cs.w=0; i<csize; i++)
                cs.w += buff[i];
           buff[csize++] = cs.b[0];
           buff[csize++] = cs.b[1];
           from.sin_port = htons(dport);
           if (sendto(dsock, buff, csize, 0, (struct sockaddr *)&from, fromlen) < 0) {
                perror("sending datagram message");
                continue;
           }
        }
        timeout = tsec - (last_send-tcurr);
        if(timeout<0.011) timeout=0.011;
     }
      }
   }
}

/* время от запуска программы */
static double prog_time() {
   static double st=-1.;
   struct timeval ct;
   struct timezone tz;
   gettimeofday(&ct, &tz);
   if(st<0.) {
      st = ct.tv_sec + ct.tv_usec/1e6;
      return 0.;
   } else
      return (ct.tv_sec + ct.tv_usec/1e6 - st);
}

static void my_sleep(double dt)
{
  int nfd;
  struct timeval tv;
   tv.tv_sec = (int)dt;
   tv.tv_usec = (int)((dt - tv.tv_sec)*1000000.);
slipping:
   nfd = select(0, (fd_set *)NULL,(fd_set *)NULL,(fd_set *)NULL, &tv);
   if(nfd < 0) {
      if(errno == EINTR)
    /*On  Linux,  timeout  is  modified to reflect the amount of
     time not slept; most other implementations DO NOT do this!*/
     goto slipping;
      fprintf(stderr,"Error in mydelay(){ select() }. %s\n",strerror(errno));
   }
}

/* заполнение стуктуры tm исходя из текущего моск.времени (на АСУ) M_time */
static struct tm *get_localtime() {    /* вместо localtime() */
    struct tm *tm;
    time_t t,tt,mt;
    int dt;
    static int ott=0, omt=0;

    time(&tt);
    tm = localtime(&tt);
    tt -= timezone; /*difference between UTC and local standard time (in sec)*/
    mt = (int)M_time;
    if((mt!=omt && omt!=0) || (tt-ott)<10) {  /* M_time с АСУ актуально? */
       dt = (int)(tt%(24*3600)) - (int)(mt%(24*3600));
       if(dt>12*3600)  dt -= 24*3600;
       if(dt<-12*3600) dt += 24*3600;
       t=tt-dt+timezone;                 /* корр-я времени машины по M_time */
       tm = localtime(&t);
    }
    if(mt!=omt) {
       if(omt!=0) ott=tt;   /* момент последнего изменения M_time */
       omt=mt;
    }
    return( tm );
}
/* распечатка сообщений для протокола*/
static void log_message(char *text) {
   static int mday=0, mon=0, year=0;
   struct tm *tm = get_localtime();
   if(tm->tm_mday!=mday || tm->tm_mon!=mon || tm->tm_year!=year) {
      mday = tm->tm_mday;  mon = tm->tm_mon;  year = tm->tm_year;
      printf("<======================================>\n");
      printf("Дата: %02d/%02d/%04d\n",mday,mon+1,1900+year);
   }
   printf("%02d:%02d:%02d %s\n",tm->tm_hour,tm->tm_min,tm->tm_sec,text);
   fflush(stdout);
}

static void myabort(int sig) {
    //int ret;
    char ss[10];//, tmp[80];
    signal(sig,SIG_IGN);
    switch (sig) {
    case SIGHUP : strcpy(ss,"SIGHUP");  break;
    case SIGINT : strcpy(ss,"SIGINT");  break;
    case SIGQUIT: strcpy(ss,"SIGQUIT"); break;
    case SIGFPE : strcpy(ss,"SIGFPE");  break;
    case SIGPIPE: strcpy(ss,"SIGPIPE"); break;
    case SIGSEGV: strcpy(ss,"SIGSEGV"); break;
    case SIGTERM: strcpy(ss,"SIGTERM"); break;
    default:  sprintf(ss,"SIG_%d",sig); break;
    }
    switch (sig) {
    default:
    case SIGHUP :
    case SIGINT :
    case SIGPIPE:
         fprintf(stderr,"%s: %s - Ignore .....\n",myname,ss);
         fflush(stderr);
         signal(sig, myabort);
         return;
    case SIGQUIT:
    case SIGFPE :
    case SIGSEGV:
    case SIGTERM:
         signal(SIGALRM, SIG_IGN);
         close_shm_block(&sdat);
         fprintf(stderr,"%s: %s - programm stop!\n",myname,ss);
         exit(sig);
    }
}
