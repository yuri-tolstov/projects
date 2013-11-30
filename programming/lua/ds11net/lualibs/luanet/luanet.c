/******************************************************************************/
/* File:   luanet.c                                                           */
/******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/*============================================================================*/
/* Definitions and Constants                                                  */
/*============================================================================*/
#define UDP_DGRAM_SZ   2048

/*============================================================================*/
/* Function prototypes                                                        */
/*============================================================================*/
static int luanet_socket(lua_State *L);
static int luanet_setsockopt(lua_State *L);
static int luanet_bind(lua_State *L);
static int luanet_recvfrom(lua_State *L);
static int luanet_sendto(lua_State *L);
static int luanet_close(lua_State *L);

/*============================================================================*/
/* LUA registration and required luaopen() function                           */
/*============================================================================*/
static const luaL_Reg luanet_efuncs[] = {
   {"socket",    luanet_socket},
   {"setsockopt",luanet_setsockopt},
   {"bind",      luanet_bind},
   {"recvfrom",  luanet_recvfrom},
   {"sendto",    luanet_sendto},
   {"close",     luanet_close},
   {NULL,      NULL}
};

LUALIB_API int luaopen_luanet(lua_State *L)
{
   luaL_register(L, "luanet", luanet_efuncs);
   return 0;
}

/*============================================================================*/
/* Function:   luanet_socket                                                  */
/*============================================================================*/
int luanet_socket(lua_State *L)
{
   int c; /*return code*/
   int soc; /*socket*/
   const char *proto; /*protocol*/

   /*Get input parameters*/
   proto = luaL_optstring(L, 1, ""); 
   if (strncmp(proto, "udp", 3) != 0) {
      c = EINVAL;  goto socket_errexit;
   }
   /*Create socket*/
   if ((soc = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
      c = errno;  goto socket_errexit;
   }
   /*Generate output parameters and exit*/
   lua_pushnumber(L, soc); /*socket*/
   return 1;

   /*Error processing*/
socket_errexit:
   // printf("luanet:socket failed, c=%d\n", c);
   lua_pushnil(L);
   lua_pushnumber(L, c);
   return 2;
}

/*============================================================================*/
/* Function:   luanet_setsockopt                                              */
/*============================================================================*/
int luanet_setsockopt(lua_State *L)
{
   int c; /*return code*/
   int soc; /*socket*/
   char *cmd; /*cmd*/
   char *val; /*value*/
   int t; /*temporary variable*/

   /*Get input parameters*/
   soc = (int)luaL_optnumber(L, 1, 0); /*socket*/
   cmd = (char *)luaL_optstring(L, 2, ""); /*cmd*/
   val = (char *)luaL_optstring(L, 3, ""); /*value*/

   /*Setup reqested parameters*/
   if (strncmp(cmd, "bcast", 5) == 0) {
      if (strncmp(val, "on", 2) == 0) {t = 1; /*on*/}
      else {t = 0; /*off*/}
      if (setsockopt(soc, SOL_SOCKET, SO_BROADCAST, (int *)&t, sizeof(t)) == -1) {
         c = errno;  goto setsockopt_errexit;
      }
   } else if (strncmp(cmd, "reuseaddr", 9) == 0) {
      if (strncmp(val, "on", 2) == 0) {t = 1; /*on*/}
      else {t = 0; /*off*/}
      if (setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, (int *)&t, sizeof(t)) == -1) {
         c = errno;  goto setsockopt_errexit;
      }
   }
   /*Generate output parameters and exit*/
   lua_pushnumber(L, 0);
   return 1;

   /*Error processing*/
setsockopt_errexit:
   // printf("luanet:setsockopt failed, c=%d\n", c);
   lua_pushnil(L);
   lua_pushnumber(L, c);
   return 2;
}

//*============================================================================*/
/* Function:   luanet_bind                                                    */
/*============================================================================*/
int luanet_bind(lua_State *L)
{
   int c; /*return code*/
   int soc; /*socket*/
   struct sockaddr_in sin; /*socket address*/
   const char *ip; /*IP address to bind to*/
   int16_t port; /*port to bind to*/

   /*Get input parameters*/
   soc = (int)luaL_optnumber(L, 1, 0); /*socket*/
   ip = luaL_optstring(L, 2, ""); /*IP address*/
   port = (int)luaL_optnumber(L, 3, 0); /*port*/

   /*Bind to the requested address*/
   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = inet_addr(ip);
   sin.sin_port = htons(port);
   if (bind(soc, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
      c = errno;  goto bind_errexit;
   }
   /*Generate output parameters and exit*/
   lua_pushnumber(L, 0);
   return 1;

   /*Error processing*/
bind_errexit:
   // printf("luanet:bind failed, c=%d\n", c);
   lua_pushnil(L);
   lua_pushnumber(L, c);
   return 2;
}

/*============================================================================*/
/* Function:   luanet_close                                                   */
/*============================================================================*/
int luanet_close(lua_State *L)
{
   int soc; /*socket*/

   /*Get input parameters*/
   soc = (int)luaL_optnumber(L, 1, 0); /*socket*/
   /*Close socket*/
   close(soc);
   /*Generate output parameters and exit*/
   lua_pushnumber(L, 0);
   return 1;
}

/*============================================================================*/
/* Function:   luanet_recvfrom                                                */
/*============================================================================*/
int luanet_recvfrom(lua_State *L)
{
   int c; /*return code*/
   int soc; /*socket*/
   char dbuf[UDP_DGRAM_SZ]; /*data buffer*/
   int nrecv; /*number of bytes to receive*/
   struct sockaddr_in sin; /*socket address*/
   socklen_t len; /*socket address size*/

   /*Get input parameters*/
   soc = (int)luaL_optnumber(L, 1, 0);

   /*Receive message*/
readagain:
   len = sizeof(sin);
   if ((nrecv = recvfrom(soc, dbuf, UDP_DGRAM_SZ, 0,
                         (struct sockaddr *)&sin, &len)) <= 0) {
      switch (errno) {
      case EINTR: goto readagain;
      case 0: c = EPIPE;  goto recvfrom_errexit;
      default: c = errno;  goto recvfrom_errexit;
      }
   }
   dbuf[nrecv] = 0;
   /*Generate output parameters and exit*/
   lua_pushlstring(L, dbuf, nrecv); /*data*/
   lua_pushstring(L, inet_ntoa(sin.sin_addr)); /*IP address*/
   lua_pushnumber(L, sin.sin_port); /*port*/
   return 3; /*3 = number of output parameters*/

   /*Error processing*/
recvfrom_errexit:
   // printf("luanet:recvfrom failed, c=%d\n", c);
   lua_pushnil(L);
   lua_pushnumber(L, c);
   return 2; /*2 = number of output parameters*/
}

/*============================================================================*/
/* Function:   luanet_sendto                                                  */
/*============================================================================*/
int luanet_sendto(lua_State *L)
{
   int c; /*return code*/
   int soc; /*socket*/
   size_t nsend; /*number of bytes to send*/
   const char *dbuf; /*data buffer*/
   const char *ip; /*destination IP address*/
   int port; /*destination port*/
   struct sockaddr_in sin; /*socket address*/

   /*Get input parameters*/
   soc = (int)luaL_optnumber(L, 1, 0); /*socket*/
   dbuf = luaL_checklstring(L, 2, (size_t *)&nsend); /*data buffer*/
   ip = luaL_checkstring(L, 3); /*IP address*/
   port = luaL_checknumber(L, 4); /*port*/

   /*Send message*/
   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = inet_addr(ip);
   sin.sin_port = htons(port);

sendagain:
   if ((nsend = sendto(soc, dbuf, nsend, 0,
                       (struct sockaddr *)&sin, sizeof(sin))) <= 0) {
      switch (errno) {
      case EINTR: goto sendagain;
      case 0: case EPIPE: c = EPIPE;  goto sendto_errexit;
      default: c = errno;  goto sendto_errexit;
      }
   }
   /*Generate output parameters and exit*/
   lua_pushnumber(L, nsend); /*number of bytes sent*/
   return 1;

   /*Error processing*/
sendto_errexit:
   // printf("luanet:sendto failed, c=%d\n", c);
   lua_pushnil(L);
   lua_pushnumber(L, c);
   return 2;
}

