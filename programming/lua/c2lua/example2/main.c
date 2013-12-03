#include <stdio.h>
#include <stdlib.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

int main() {
   // Create new Lua state and load the lua libraries
   lua_State *L = luaL_newstate();
   luaL_openlibs(L);

   // Load, but do not run yet Lua script
   luaL_loadfile(L, "funcs.lua");

   // Priming
   lua_pcall(L, 0, 0, 0);

   // Execute the tellme function (from funcs.lua)
   // lua_pcall(L, number-of-args, number-of-returns, errfunc-idx);
   lua_getglobal(L, "tellme");
   printf("C: Callling LUA tellme function...\n");
   lua_pcall(L, 0, 0, 0);
   printf("C: Back in C.\n");

   // Execute the square function (from funcs.lua)
   printf("C: Callling LUA square function...\n");
   lua_getglobal(L, "square");
   lua_pushnumber(L, 6);
   lua_pcall(L, 1, 1, 0);
   int res = lua_tonumber(L, -1);
   printf("C: Result = %d\n", res);

   // Close the Lua state
   lua_close(L);
   return 0;
}

