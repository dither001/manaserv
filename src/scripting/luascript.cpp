/*
 *  The Mana Server
 *  Copyright (C) 2007-2010  The Mana World Development Team
 *  Copyright (C) 2010  The Mana Developers
 *
 *  This file is part of The Mana Server.
 *
 *  The Mana Server is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana Server is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana Server.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "luascript.h"


#include "scripting/luautil.h"

#include "game-server/being.h"
#include "utils/logger.h"

#include <cassert>
#include <cstring>

LuaScript::~LuaScript()
{
    lua_close(mState);
}

void LuaScript::prepare(Ref function)
{
    assert(nbArgs == -1);
    lua_rawgeti(mState, LUA_REGISTRYINDEX, function);
    assert(lua_isfunction(mState, -1));
    nbArgs = 0;
    mCurFunction = "<callback>"; // We don't know the function name
}

void LuaScript::prepare(const std::string &name)
{
    assert(nbArgs == -1);
    lua_getglobal(mState, name.c_str());
    nbArgs = 0;
    mCurFunction = name;
}

void LuaScript::push(int v)
{
    assert(nbArgs >= 0);
    lua_pushinteger(mState, v);
    ++nbArgs;
}

void LuaScript::push(const std::string &v)
{
    assert(nbArgs >= 0);
    lua_pushstring(mState, v.c_str());
    ++nbArgs;
}

void LuaScript::push(Thing *v)
{
    assert(nbArgs >= 0);
    lua_pushlightuserdata(mState, v);
    ++nbArgs;
}

void LuaScript::push(const std::list<InventoryItem> &itemList)
{
    assert(nbArgs >= 0);
    int position = 0;

    lua_createtable(mState, itemList.size(), 0);
    int itemTable = lua_gettop(mState);

    for (std::list<InventoryItem>::const_iterator i = itemList.begin();
         i != itemList.end();
         i++)
    {
        // create the item structure
        std::map<std::string, int> item;
        item["id"] = i->itemId;
        item["amount"] = i->amount;
        pushSTLContainer<std::string, int>(mState, item);
        lua_rawseti(mState, itemTable, ++position);
    }
    ++nbArgs;
}

int LuaScript::execute()
{
    assert(nbArgs >= 0);
    int res = lua_pcall(mState, nbArgs, 1, 1);
    nbArgs = -1;
    if (res || !(lua_isnil(mState, -1) || lua_isnumber(mState, -1)))
    {
        const char *s = lua_tostring(mState, -1);

        LOG_WARN("Lua Script Error" << std::endl
                 << "     Script  : " << mScriptFile << std::endl
                 << "     Function: " << mCurFunction << std::endl
                 << "     Error   : " << (s ? s : "") << std::endl);
        lua_pop(mState, 1);
        return 0;
    }
    res = lua_tointeger(mState, -1);
    lua_pop(mState, 1);
    return res;
    mCurFunction.clear();
}

void LuaScript::assignCallback(Script::Ref &function)
{
    assert(lua_isfunction(mState, -1));

    // If there is already a callback set, replace it
    if (function != NoRef)
        luaL_unref(mState, LUA_REGISTRYINDEX, function);

    function = luaL_ref(mState, LUA_REGISTRYINDEX);
}

void LuaScript::load(const char *prog, const char *name)
{
    int res = luaL_loadbuffer(mState, prog, std::strlen(prog), name);
    if (res)
    {
        switch (res) {
        case LUA_ERRSYNTAX:
            LOG_ERROR("Syntax error while loading Lua script: "
                      << lua_tostring(mState, -1));
            break;
        case LUA_ERRMEM:
            LOG_ERROR("Memory allocation error while loading Lua script");
            break;
        }

        lua_pop(mState, 1);
    }
    else if (lua_pcall(mState, 0, 0, 1))
    {
        LOG_ERROR("Failure while initializing Lua script: "
                  << lua_tostring(mState, -1));
        lua_pop(mState, 1);
    }
}

void LuaScript::processDeathEvent(Being *being)
{
    prepare("death_notification");
    push(being);
    //TODO: get and push a list of creatures who contributed to killing the
    //      being. This might be very interesting for scripting quests.
    execute();
}

void LuaScript::processRemoveEvent(Thing *being)
{
    prepare("remove_notification");
    push(being);
    //TODO: get and push a list of creatures who contributed to killing the
    //      being. This might be very interesting for scripting quests.
    execute();

    being->removeListener(getScriptListener());
}

/**
 * Called when the server has recovered the value of a quest variable.
 */
void LuaScript::getQuestCallback(Character *q, const std::string &name,
                                 const std::string &value, void *data)
{
    LuaScript *s = static_cast< LuaScript * >(data);
    assert(s->nbArgs == -1);
    lua_getglobal(s->mState, "quest_reply");
    lua_pushlightuserdata(s->mState, q);
    lua_pushstring(s->mState, name.c_str());
    lua_pushstring(s->mState, value.c_str());
    s->nbArgs = 3;
    s->execute();
}

/**
 * Called when the server has recovered the post for a user
 */
void LuaScript::getPostCallback(Character *q, const std::string &sender,
                                const std::string &letter, void *data)
{
    // get the script
    LuaScript *s = static_cast<LuaScript*>(data);
    assert(s->nbArgs == -1);
    lua_getglobal(s->mState, "post_reply");
    lua_pushlightuserdata(s->mState, q);
    lua_pushstring(s->mState, sender.c_str());
    lua_pushstring(s->mState, letter.c_str());
    s->nbArgs = 3;
    s->execute();
}
