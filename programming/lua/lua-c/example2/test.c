#include <string.h>
#include "lua.h"
#include "lauxlib.h"

int l_bar(lua_State *L)
{
   const char *str = lua_tostring(L, -1);
   int l = strlen(str);
   lua_pushstring(L, str);
   lua_pushnumber(L, l);
   return 2;
}


int main(int argc, char **argv)
{
   int c;
   lua_State *L = luaL_newstate();

   luaL_openlibs(L);
   luaL_dofile(L, "test.lua");

   lua_pushcfunction(L, l_bar);
   lua_setglobal(L, "bar");

   lua_getglobal(L, "foo");
   lua_pushstring(L, "casandra");
   lua_pcall(L, 1, 1, 0);
   c = lua_tonumber(L, -1);
   printf("C: String length is %d\n", c);

   return 0;
}

