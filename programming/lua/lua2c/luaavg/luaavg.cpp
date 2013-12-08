#include <stdio.h>

extern "C" {
	#include "lua.h"
	#include "lualib.h"
	#include "lauxlib.h"
}

static int average(lua_State *L)
{
	/* get number of arguments */
	int n = lua_gettop(L);
	double sum = 0;
	int i;

	/* loop through each argument */
	for (i = 1; i <= n; i++)
	{
		/* total the arguments */
		sum += lua_tonumber(L, i);
	}

	/* push the average */
	lua_pushnumber(L, sum / n);

	/* push the sum */
	lua_pushnumber(L, sum);

	/* return the number of results */
	return 2;
}

int main ( int argc, char *argv[] )
{
   lua_State* L; /*LUA interpreter.*/

#ifdef THIS_FOR_LUA51
	/* initialize Lua */
	L = lua_open();
#else /*LUA-5.2*/
	L = luaL_newstate();
#endif
	/* load Lua base libraries */
	luaL_openlibs(L);

	/* register our function */
	lua_register(L, "average", average);

	/* run the script */
	luaL_dofile(L, "avg.lua");

	/* cleanup Lua */
	lua_close(L);
 
	/* pause */
	printf( "Press enter to exit..." );
	getchar();

	return 0;
}
