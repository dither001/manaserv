/*
 *  The Mana World Server
 *  Copyright 2007 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana World is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana World; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _TMWSERV_SCRIPTING_LUAUTIL_HPP
#define _TMWSERV_SCRIPTING_LUAUTIL_HPP

extern "C" {
#include <lualib.h>
#include <lauxlib.h>
}
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>

class Being;
class NPC;
class Character;
class Thing;

void raiseScriptError(lua_State *s, const char *format, ...);

NPC *getNPC(lua_State *s, int p);
Character *getCharacter(lua_State *s, int p);
Being *getBeing(lua_State *s, int p);


/* Polymorphic wrapper for pushing variables.
   Useful for templates.*/
void push(lua_State *s, int val);
void push(lua_State *s, const std::string &val);
void push(lua_State *s, Thing* val);
void push(lua_State *s, double val);


/*  Pushes an STL LIST */
template <typename T> void pushSTLContainer(lua_State *s, const std::list<T> &container)
{
    int len = container.size();
    lua_newtable(s);
    int table = lua_gettop(s);
    typename std::list<T>::const_iterator i;
    i = container.begin();

    for (int key = 1; key <= len; key++)
    {
        push(s, key);
        push(s, *i);
        lua_settable(s, table);
        i++;
    }
}

/*  Pushes an STL VECTOR */
template <typename T> void pushSTLContainer(lua_State *s, const std::vector<T> &container)
{
    int len = container.size();
    lua_createtable(s, 0, len);
    int table = lua_gettop(s);

    for (int key = 0; key < len; key++)
    {
        push(s, key+1);
        push(s, container.at(key).c_str());
        lua_settable(s, table);
    }
}

/*  Pushes an STL MAP */
template <typename Tkey, typename Tval> void pushSTLContainer(lua_State *s, const std::map<Tkey, Tval> &container)
{
    int len = container.size();
    lua_createtable(s, 0, len);
    int table = lua_gettop(s);
    typename std::map<Tkey, Tval>::const_iterator i;
    i = container.begin();

    for (int key = 1; key <= len; key++)
    {
        push(s, i->first.c_str());
        push(s, i->second.c_str());
        lua_settable(s, table);
        i++;
    }
}

/*  Pushes an STL SET */
template <typename T> void pushSTLContainer(lua_State *s, const std::set<T> &container)
{
    int len = container.size();
    lua_newtable(s);
    int table = lua_gettop(s);
    typename std::set<T>::const_iterator i;
    i = container.begin();

    for (int key = 1; key <= len; key++)
    {
        push(s, key);
        push(s, *i);
        lua_settable(s, table);
        i++;
    }
}

#endif
