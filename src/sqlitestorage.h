/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
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
 *
 *  $Id$
 */

#ifndef TMWSERV_SQLITESTORAGE_H
#define TMWSERV_SQLITESTORAGE_H

#include "sqlite/SQLiteWrapper.h"
#include "storage.h"

/**
 * SQLite implemention of persistent dynamic data storage.
 */
class SQLiteStorage : public Storage
{
    public:
        /**
         * Constructor.
         */
        SQLiteStorage();

        /**
         * Destructor.
         */
        ~SQLiteStorage();

        /**
         * Save changes to database.
         */
        void flush();

        /**
         * Account count (test function)
         */
        unsigned int getAccountCount();

        /**
         * Get account & associated data
         */
        Account* getAccount(const std::string &username);

    protected:
        SQLiteWrapper sqlite;              /**< Database */
        std::vector<Account*> accounts;    /**< Loaded accounts */
        std::vector<Being*> characters;    /**< Loaded characters */

        /**
         * Create tables if master is empty.
         */
        void createTablesIfNecessary();
};

#endif /* TMWSERV_SQLITESTORAGE_H */