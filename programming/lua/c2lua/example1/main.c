#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

int main() {
   // Create new Lua state and load the lua libraries
   lua_State *L = luaL_newstate();
   luaL_openlibs(L);

   // Load, but do not run yet Lua script
   luaL_loadfile(L, "sample.lua");

   // Run the script
   // lua_pcall(L, number-of-args, number-of-returns, errfunc-idx);
   lua_pcall(L, 0, 0, 0);

   // Close the Lua state
   lua_close(L);
   return 0;
}

