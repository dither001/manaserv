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

#include "accounthandler.h"
#include "debug.h"
#include "storage.h"
#include "account.h"
#include "messageout.h"
#include "configuration.h"
#include "utils/logger.h"
#include "utils/slangsfilter.h"

using tmwserv::Account;
using tmwserv::AccountPtr;
using tmwserv::Storage;

/**
 * Generic interface convention for getting a message and sending it to the
 * correct subroutines. Account handler takes care of determining the
 * current step in the account process, be it creation, setup, or login.
 */
void AccountHandler::receiveMessage(NetComputer &computer, MessageIn &message)
{

    Storage &store = Storage::instance("tmw");

#if defined (SQLITE_SUPPORT)
    // Reopen the db in this thread for sqlite, to avoid
    // Library Call out of sequence problem due to thread safe.
    store.setUser(config.getValue("dbuser", ""));
    store.setPassword(config.getValue("dbpass", ""));
    store.close();
    store.open();
#endif

    MessageOut result;

    switch (message.getId())
    {
        case CMSG_LOGIN:
            {
                std::string username = message.readString();
                std::string password = message.readString();
                LOG_INFO(username << " is trying to login.", 1)

                if (computer.getAccount().get() != NULL) {
                    LOG_INFO("Already logged in as " << computer.getAccount()->getName()
                        << ".", 1)
                    LOG_INFO("Please logout first.", 1)
                    result.writeShort(SMSG_LOGIN_ERROR);
                    result.writeShort(LOGIN_ALREADY_LOGGED);
                    break;
                }

                // see if the account exists
                tmwserv::AccountPtr acc = tmwserv::AccountPtr(store.getAccount(username));

                if (!acc.get()) {
                    // account doesn't exist -- send error to client
                    LOG_INFO(username << ": Account does not exist.", 1)

                    result.writeShort(SMSG_LOGIN_ERROR);
                    result.writeByte(LOGIN_INVALID_USERNAME);
                } else if (acc->getPassword() != password) {
                    // bad password -- send error to client
                    LOG_INFO("Bad password for " << username, 1)

                    result.writeShort(SMSG_LOGIN_ERROR);
                    result.writeByte(LOGIN_INVALID_PASSWORD);
                } else {
                    LOG_INFO("Login OK by " << username, 1)

                    // Associate account with connection
                    computer.setAccount(acc);

                    result.writeShort(SMSG_LOGIN_CONFIRM);

                    // Return information about available characters
                    tmwserv::Beings &chars = computer.getAccount()->getCharacters();
                    result.writeByte(chars.size());

                    LOG_INFO(username << "'s account has " << chars.size() << " character(s).", 1)
                    std::string charNames = "";
                    for (unsigned int i = 0; i < chars.size(); i++)
                    {
                        result.writeString(chars[i]->getName());
                        if (i >0) charNames += ", ";
                        charNames += chars[i]->getName();
                    }
                    charNames += ".";
                    LOG_INFO(charNames.c_str(), 1)
                }
            }
            break;

        case CMSG_LOGOUT:
            {
                if ( computer.getAccount().get() == NULL )
                {
                    LOG_INFO("Can't logout. Not even logged in.", 1)
                    result.writeShort(SMSG_LOGOUT_ERROR);
                    result.writeByte(LOGOUT_UNSUCCESSFULL);
                }
                else
                {
                    std::string username = computer.getAccount()->getName();
                    if ( username == "" )
                    {
                        LOG_INFO("Account without name ? Logged out anyway...", 1)
                        // computer.unsetCharacter(); Done by unsetAccount();
                        computer.unsetAccount();
                        result.writeShort(SMSG_LOGOUT_ERROR);
                        result.writeByte(LOGOUT_UNKNOWN);
                    }
                    else
                    {
                        LOG_INFO(computer.getAccount()->getName() << " logs out.", 1)
                        // computer.unsetCharacter(); Done by unsetAccount();
                        computer.unsetAccount();
                        result.writeShort(SMSG_LOGOUT_CONFIRM);
                        result.writeByte(LOGOUT_OK);
                    }
                }
            }
            break;

        case CMSG_REGISTER:
            {
                std::string username = message.readString();
                std::string password = message.readString();
                std::string email = message.readString();

                // Checking if the Name is slang's free.
                if (!tmwserv::utils::filterContent(username))
                {
                    result.writeShort(SMSG_REGISTER_RESPONSE);
                    result.writeByte(REGISTER_INVALID_USERNAME);
                    LOG_INFO(username << ": has got bad words in it.", 1)
                    break;
                }
                // Checking conditions for having a good account.
                LOG_INFO(username << " is trying to register.", 1)

                bool emailValid = false;
                // Testing Email validity
                if ( (email.length() < MIN_EMAIL_LENGTH) || (email.length() > MAX_EMAIL_LENGTH))
                {
                    result.writeShort(SMSG_REGISTER_RESPONSE);
                    result.writeByte(REGISTER_INVALID_EMAIL);
                    LOG_INFO(email << ": Email too short or too long.", 1)
                    break;
                }
                if (store.doesEmailAlreadyExists(email)) // Search if Email already exists
                {
                    result.writeShort(SMSG_REGISTER_RESPONSE);
                    result.writeByte(REGISTER_EXISTS_EMAIL);
                    LOG_INFO(email << ": Email already exists.", 1)
                    break;
                }
                if ((email.find_first_of('@') != std::string::npos)) // Searching for an @.
                {
                    int atpos = email.find_first_of('@');
                    if (email.find_first_of('.', atpos) != std::string::npos) // Searching for a '.' after the @.
                    {
                        if (email.find_first_of(' ') == std::string::npos) // Searching if there's no spaces.
                        {
                            emailValid = true;
                        }
                    }
                }

                // see if the account exists
                Account *accPtr = store.getAccount(username);
                if ( accPtr ) // Account already exists.
                {
                    result.writeShort(SMSG_REGISTER_RESPONSE);
                    result.writeByte(REGISTER_EXISTS_USERNAME);
                    LOG_INFO(username << ": Username already exists.", 1)
                }
                else if ((username.length() < MIN_LOGIN_LENGTH) || (username.length() > MAX_LOGIN_LENGTH)) // Username length
                {
                    result.writeShort(SMSG_REGISTER_RESPONSE);
                    result.writeByte(REGISTER_INVALID_USERNAME);
                    LOG_INFO(username << ": Username too short or too long.", 1)
                }
                else if ((password.length() < MIN_PASSWORD_LENGTH) || (password.length() > MAX_PASSWORD_LENGTH))
                {
                    result.writeShort(SMSG_REGISTER_RESPONSE);
                    result.writeByte(REGISTER_INVALID_PASSWORD);
                    LOG_INFO(email << ": Password too short or too long.", 1)
                }
                else if (!emailValid)
                {
                    result.writeShort(SMSG_REGISTER_RESPONSE);
                    result.writeByte(REGISTER_INVALID_EMAIL);
                    LOG_INFO(email << ": Email Invalid, only a@b.c format is accepted.", 1)
                }
                else
                {
                    AccountPtr acc(new Account(username, password, email));
                    store.addAccount(acc);

                    result.writeShort(SMSG_REGISTER_RESPONSE);
                    result.writeByte(REGISTER_OK);

                    store.flush(); // flush changes
                    LOG_INFO(username << ": Account registered.", 1)
                }
            }
            break;

        case CMSG_UNREGISTER:
            {
                std::string username = message.readString();
                std::string password = message.readString();
                LOG_INFO(username << " wants to be deleted from our accounts.", 1)

                // see if the account exists
                Account *acc = store.getAccount(username);

                if (!acc) {
                    // account doesn't exist -- send error to client
                    LOG_INFO(username << ": Account doesn't exist anyway.", 1)

                    result.writeShort(SMSG_UNREGISTER_RESPONSE);
                    result.writeByte(UNREGISTER_INVALID_USERNAME);
                } else if (acc->getPassword() != password) {
                    // bad password -- send error to client
                    LOG_INFO("Won't delete it : Bad password for " << username << ".", 1)

                    result.writeShort(SMSG_UNREGISTER_RESPONSE);
                    result.writeByte(UNREGISTER_INVALID_PASSWORD);
                } else {

                    // If the account to delete is the current account we're logged in.
                    // Get out of it in memory.
                    if (computer.getAccount().get() != NULL )
                    {
                        if (computer.getAccount()->getName() == username )
                        {
                            // computer.unsetCharacter(); Done by unsetAccount();
                            computer.unsetAccount();
                        }
                    }
                    // delete account and associated characters
                    LOG_INFO("Farewell " << username << " ...", 1)
                    store.delAccount(username);
                    store.flush();
                    result.writeShort(SMSG_UNREGISTER_RESPONSE);
                    result.writeByte(UNREGISTER_OK);
                }
            }
            break;

        case CMSG_CHAR_CREATE:
            {
                if (computer.getAccount().get() == NULL) {
                    result.writeShort(SMSG_CHAR_CREATE_RESPONSE);
                    result.writeByte(CREATE_NOLOGIN);
                    LOG_INFO("Not logged in. Can't create a Character.", 1)
                    break;
                }

                // A player shouldn't have more than 3 characters.
                tmwserv::Beings &chars = computer.getAccount()->getCharacters();
                if (chars.size() >= MAX_OF_CHARACTERS)
                {
                    result.writeShort(SMSG_CHAR_CREATE_RESPONSE);
                    result.writeByte(CREATE_TOO_MUCH_CHARACTERS);
                    LOG_INFO("Already has " << MAX_OF_CHARACTERS
                    << " characters. Can't create another Character.", 1)
                    break;
                }

                std::string name = message.readString();
                // Check if the character's name already exists
                if (store.doesCharacterNameExists(name))
                {
                    result.writeShort(SMSG_CHAR_CREATE_RESPONSE);
                    result.writeByte(CREATE_EXISTS_NAME);
                    LOG_INFO(name << ": Character's name already exists.", 1)
                    break;
                }
                // Check for character's name length
                if ((name.length() < MIN_CHARACTER_LENGTH) || (name.length() > MAX_CHARACTER_LENGTH))
                {
                    result.writeShort(SMSG_CHAR_CREATE_RESPONSE);
                    result.writeByte(CREATE_INVALID_NAME);
                    LOG_INFO(name << ": Character's name too short or too long.", 1)
                    break;
                }
                //char hairStyle = message.readByte();
                //char hairColor = message.readByte();
                Genders sex = (Genders)message.readByte();

                // TODO: Customization of player's stats...
                tmwserv::RawStatistics stats = {10, 10, 10, 10, 10, 10};
                tmwserv::BeingPtr newCharacter(new tmwserv::Being(name, sex, 1, 0, stats));
                computer.getAccount()->addCharacter(newCharacter);

                LOG_INFO("Character " << name << " was created for " 
                    << computer.getAccount()->getName() << "'s account.", 1)

                store.flush(); // flush changes
                result.writeShort(SMSG_CHAR_CREATE_RESPONSE);
                result.writeByte(CREATE_OK);
            }
            break;

        case CMSG_CHAR_SELECT:
            {
                if (computer.getAccount().get() == NULL)
                {
                    result.writeShort(SMSG_CHAR_SELECT_RESPONSE);
                    result.writeByte(SELECT_NOLOGIN);
                    LOG_INFO("Not logged in. Can't select a Character.", 1)
                    break; // not logged in
                }

                unsigned char charNum = message.readByte();

                tmwserv::Beings &chars = computer.getAccount()->getCharacters();
                result.writeShort(SMSG_CHAR_SELECT_RESPONSE);
                if ( chars.size() == 0 )
                {
                    result.writeByte(SELECT_NOT_YET_CHARACTERS);
                    LOG_INFO("Character Selection : Yet no characters created.", 1)
                    break;
                }
                // Character ID = 0 to Number of Characters - 1.
                if (charNum >= chars.size()) {
                    // invalid char selection
                    result.writeByte(SELECT_INVALID);
                    LOG_INFO("Character Selection : Selection out of ID range.", 1)
                    break;
                }

                // set character
                computer.setCharacter(chars[charNum]);

                result.writeByte(SELECT_OK);
                LOG_INFO("Selected Character " << int(charNum)
                << " : " <<
                computer.getCharacter()->getName(), 1)
            }
            break;

        case CMSG_CHAR_DELETE:
            {
                if (computer.getAccount().get() == NULL)
                {
                    result.writeShort(SMSG_CHAR_DELETE_RESPONSE);
                    result.writeByte(DELETE_NOLOGIN);
                    LOG_INFO("Not logged in. Can't delete a Character.", 1)
                    break; // not logged in
                }

                unsigned char charNum = message.readByte();

                tmwserv::Beings &chars = computer.getAccount()->getCharacters();
                result.writeShort(SMSG_CHAR_DELETE_RESPONSE);
                if ( chars.size() == 0 )
                {
                    result.writeByte(DELETE_NO_MORE_CHARACTERS);
                    LOG_INFO("Character Deletion : No characters in " << computer.getAccount()->getName()
                             << "'s account.", 1)
                    break;
                }
                // Character ID = 0 to Number of Characters - 1.
                if (charNum >= chars.size()) {
                    // invalid char selection
                    result.writeByte(DELETE_INVALID_NAME);
                    LOG_INFO("Character Deletion : Selection out of ID range.", 1)
                    break;
                }

                // Delete the character
                // if the character to delete is the current character, get off of it in
                // memory.
                if ( computer.getCharacter().get() != NULL )
                {
                    if ( computer.getCharacter()->getName() == chars[charNum].get()->getName() )
                    {
                        computer.unsetCharacter();
                    }
                }

                std::string deletedCharacter = chars[charNum].get()->getName();
                computer.getAccount()->delCharacter(deletedCharacter);
                store.flush();
                LOG_INFO(deletedCharacter << ": Character deleted...", 1)
                result.writeByte(DELETE_OK);

            }
            break;

        case CMSG_CHAR_LIST:
            {
                if (computer.getAccount().get() == NULL)
                {
                    result.writeShort(SMSG_CHAR_LIST_RESPONSE);
                    result.writeByte(CHAR_LIST_NOLOGIN);
                    LOG_INFO("Not logged in. Can't list characters.", 1)
                    break; // not logged in
                }

                result.writeShort(SMSG_CHAR_LIST_RESPONSE);
                result.writeByte(CHAR_LIST_OK);
                // Return information about available characters
                tmwserv::Beings &chars = computer.getAccount()->getCharacters();
                result.writeByte(chars.size());

                LOG_INFO(computer.getAccount()->getName() << "'s account has "
                << chars.size() << " character(s).", 1)
                std::string charStats = "";
                for (unsigned int i = 0; i < chars.size(); i++)
                {
                    result.writeString(chars[i]->getName());
                    if (i >0) charStats += ", ";
                    charStats += chars[i]->getName();
                    result.writeByte(unsigned(short(chars[i]->getGender())));
                    result.writeByte(chars[i]->getLevel());
                    result.writeByte(chars[i]->getMoney());
                    result.writeByte(chars[i]->getStrength());
                    result.writeByte(chars[i]->getAgility());
                    result.writeByte(chars[i]->getVitality());
                    result.writeByte(chars[i]->getIntelligence());
                    result.writeByte(chars[i]->getDexterity());
                    result.writeByte(chars[i]->getLuck());
                }
                charStats += ".";
                LOG_INFO(charStats.c_str(), 1)
            }
            break;

        default:
            LOG_WARN("Invalid message type", 0)
            result.writeShort(SMSG_LOGIN_ERROR);
            result.writeByte(LOGIN_UNKNOWN);
            break;
    }

    // return result
    computer.send(result.getPacket());
}

/* ----Login Message----
 * Accepts a login message and interprets it, assigning the proper
 * login
 * Preconditions: The requested handle is not logged in already. 
 *                The requested handle exists. 
 *                The requested handle is not banned or restricted. 
 *                The character profile is valid
 * Postconditions: The player recieves access through a character in
 *                 the world.
 * Return Value: SUCCESS if the player was successfully assigned the
 *               requested char, ERROR on early termination of the
 *               routine.
 */
int AccountHandler::loginMessage(NetComputer &computer, MessageIn &message)
{
    // Get the handle (account) the player is requesting
    // RETURN TMW_ACCOUNTERROR_NOEXIST if: requested does not handle exist
    // RETURN TMW_ACCOUNTERROR_BANNED if: the handle status is
    // HANDLE_STATUS_BANNED
    // RETURN TMW_ACCOUNTERROR_ALREADYASSIGNED if: the handle is already
    // assigned

    // Get the character within that handle that the player is requesting
    // RETURN TMW_ACCOUNTERROR_CHARNOTFOUND if: character not found

    // Assign the player to that character
    // RETURN TMW_ACCOUNTERROR_ASSIGNFAILED if: assignment not successful

    // return TMW_SUCCESS -- successful exit
    return TMW_SUCCESS;
}

/* ----Account Assignment----
 * Assigns the computer to this account, and allows it to make account
 * changes using this structure.
 * Preconditions: This structure already contains a valid accountHandle
 * Postconditions: The player is connected to the account through this handle
 * Return Value: SUCCESS if the player was successfully assigned the
 *               requested handle, ERROR on early termination of the
 *               routine.
 */
int
AccountHandler::assignAccount(NetComputer &computer, tmwserv::Account *account)
{
    // RETURN TMW_ACCOUNTERROR_ASSIGNFAILED if: the account was accessed before
    //                                          being initalized.

    // Assign the handle


    return TMW_SUCCESS;
}
