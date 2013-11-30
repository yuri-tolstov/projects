#include <lua.hpp>
#include <iostream>
#include <list>
#include <assert.h>

extern "C" {
  static int l_list_push(lua_State *L) { // Push elements from LUA
    assert(lua_gettop(L) == 2); // check that the number of args is exactly 2 
    std::list<int> **ud = static_cast<std::list<int> **>(luaL_checkudata(L,1, "ListMT")); // first arg is the list
    int v =luaL_checkint(L,2); // seconds argument is the integer to be pushed to the std::list<int>
    (*ud)->push_back(v); // perform the push on C++ object through the pointer stored in user data
    return 0; // we return 0 values in the lua stack
  }
  static int l_list_pop(lua_State *L) {
    assert(lua_gettop(L) == 1); // check that the number of args is exactly 1
    std::list<int> **ud = static_cast<std::list<int> **>(luaL_checkudata(L, 1, "ListMT")); // first arg is the userdata
    if ((*ud)->empty()) {
      lua_pushnil(L);
      return 1; // if list is empty the function will return nil
    }
    lua_pushnumber(L,(*ud)->front()); // push the value to pop in the lua stack
                                      // it will be the return value of the function in lua
    (*ud)->pop_front(); // remove the value from the list
    return 1; //we return 1 value in the stack
  }
}

class Main
{
public:
  Main();
  ~Main();
  void run();

  /* data */
private:
  lua_State *L;
  std::list<int> theList;
  void registerListType();
  void runScript();
};

Main::Main() {
  L = luaL_newstate();
  luaL_openlibs(L);
}

Main::~Main() {
  lua_close(L);
}

void Main::runScript() {
  lua_settop(L,0); //empty the lua stack
  if(luaL_dofile(L, "./samplescript.lua")) {
    fprintf(stderr, "error: %s\n", lua_tostring(L,-1));
    lua_pop(L,1);
    return; // exit(1);
  }
  assert(lua_gettop(L) == 0); //empty the lua stack
}

void Main::registerListType() {
  std::cout << "Set the list object in lua" << std::endl;
  luaL_newmetatable(L, "ListMT");
  lua_pushvalue(L,-1);
  lua_setfield(L,-2, "__index"); // ListMT .__index = ListMT
  lua_pushcfunction(L, l_list_push);
  lua_setfield(L,-2, "push"); // push in lua will call l_list_push in C++
  lua_pushcfunction(L, l_list_pop);
  lua_setfield(L,-2, "pop"); // pop in lua will call l_list_pop in C++
}

void Main::run() {
  for(unsigned int i = 0; i<10; i++) // add some input data to the list
    theList.push_back(i*100);
  registerListType();
  std::cout << "creating an instance of std::list in lua" << std::endl;
  std::list<int> **ud = static_cast<std::list<int> **>(lua_newuserdata(L, sizeof(std::list<int> *)));
  *(ud) = &theList;
  luaL_setmetatable(L, "ListMT"); // set userdata metatable
  lua_setglobal(L, "the_list"); // the_list in lua points to the new userdata

  runScript();

  while(!theList.empty()) { // read the data that lua left in the list
    std::cout << "from C++: pop value " << theList.front() << std::endl;
    theList.pop_front();
  }
}

int main(int argc, char const *argv[])
{
  Main m;
  m.run();
  return 0;
}

