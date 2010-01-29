/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2009 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: userdb.cpp
**
**/

#include "nxcore.h"


//
// Password complexity options
//

#define PSWD_MUST_CONTAIN_DIGITS          0x0001
#define PSWD_MUST_CONTAIN_UPPERCASE       0x0002
#define PSWD_MUST_CONTAIN_LOWERCASE       0x0004
#define PSWD_MUST_CONTAIN_SPECIAL_CHARS   0x0008
#define PSWD_FORBID_ALPHABETICAL_SEQUENCE 0x0010
#define PSWD_FORBID_KEYBOARD_SEQUENCE     0x0020


//
// Externals
//

bool RadiusAuth(const TCHAR *pszLogin, const TCHAR *pszPasswd);


//
// Static data
//

static int m_userCount = 0;
static UserDatabaseObject **m_users = NULL;
static MUTEX m_mutexUserDatabaseAccess = INVALID_MUTEX_HANDLE;
static THREAD m_statusUpdateThread = INVALID_THREAD_HANDLE;


//
// Upgrade user accounts status in background
//

static THREAD_RESULT THREAD_CALL AccountStatusUpdater(void *arg)
{
	DbgPrintf(2, _T("User account status update thread started"));
	while(!SleepAndCheckForShutdown(60))
	{
		DbgPrintf(5, _T("AccountStatusUpdater: wakeup"));

		time_t blockInactiveAccounts = (time_t)ConfigReadInt(_T("BlockInactiveUserAccounts"), 0) * 86400;

		MutexLock(m_mutexUserDatabaseAccess, INFINITE);
		time_t now = time(NULL);
		for(int i = 0; i < m_userCount; i++)
		{
			if (m_users[i]->isDeleted() || (m_users[i]->getId() & GROUP_FLAG))
				continue;

			User *user = (User *)m_users[i];

			if (user->isDisabled() && (user->getReEnableTime() <= now))
			{
				// Re-enable temporary disabled user
				user->enable();
				WriteAuditLog(AUDIT_SECURITY, TRUE, 0xFFFFFFFF, "", 0, _T("Temporary disabled user account \"%s\" re-enabled"), user->getName());
				DbgPrintf(3, _T("Temporary disabled user account \"%s\" re-enabled"), user->getName());
			}

			if ((blockInactiveAccounts > 0) && (user->getLastLoginTime() > 0) && (user->getLastLoginTime() + blockInactiveAccounts < now))
			{
				user->disable();
				WriteAuditLog(AUDIT_SECURITY, TRUE, 0xFFFFFFFF, "", 0, _T("User account \"%s\" disabled due to inactivity"), user->getName());
				DbgPrintf(3, _T("User account \"%s\" disabled due to inactivity"), user->getName());
			}
		}
		MutexUnlock(m_mutexUserDatabaseAccess);
	}

	DbgPrintf(2, _T("User account status update thread stopped"));
	return THREAD_OK;
}


//
// Initialize user handling subsystem
//

void InitUsers()
{
   m_mutexUserDatabaseAccess = MutexCreate();
	m_statusUpdateThread = ThreadCreateEx(AccountStatusUpdater, 0, NULL);
}


//
// Cleanup user handling subsystem
//

void CleanupUsers()
{
	ThreadJoin(m_statusUpdateThread);
}


//
// Load user list from database
//

BOOL LoadUsers(void)
{
   int i;
   DB_RESULT hResult;

   // Load users
   hResult = DBSelect(g_hCoreDB,
	                   _T("SELECT id,name,system_access,flags,description,guid,")
							 _T("password,full_name,grace_logins,auth_method,")
							 _T("cert_mapping_method,cert_mapping_data,auth_failures,")
							 _T("last_passwd_change,min_passwd_length,disabled_until,")
							 _T("last_login FROM users"));
   if (hResult == NULL)
      return FALSE;

   m_userCount = DBGetNumRows(hResult);
   m_users = (UserDatabaseObject **)malloc(sizeof(UserDatabaseObject *) * m_userCount);
   for(i = 0; i < m_userCount; i++)
		m_users[i] = new User(hResult, i);

   DBFreeResult(hResult);

   // Check if user with UID 0 was loaded
   for(i = 0; i < m_userCount; i++)
		if (m_users[i]->getId() == 0)
         break;
         
   // Create superuser account if it doesn't exist
   if (i == m_userCount)
   {
      m_userCount++;
      m_users = (UserDatabaseObject **)realloc(m_users, sizeof(UserDatabaseObject *) * m_userCount);
		m_users[i] = new User();
      nxlog_write(MSG_SUPERUSER_CREATED, EVENTLOG_WARNING_TYPE, NULL);
   }

   // Load groups
   hResult = DBSelect(g_hCoreDB, "SELECT id,name,system_access,flags,description,guid FROM user_groups");
   if (hResult == NULL)
      return FALSE;

	int mark = m_userCount;
   m_userCount += DBGetNumRows(hResult);
   m_users = (UserDatabaseObject **)realloc(m_users, sizeof(UserDatabaseObject *) * m_userCount);
   for(i = mark; i < m_userCount; i++)
		m_users[i] = new Group(hResult, i - mark);

   DBFreeResult(hResult);

   // Check if everyone group was loaded
   for(i = mark; i < m_userCount; i++)
		if (m_users[i]->getId() == GROUP_EVERYONE)
         break;

   // Create everyone group if it doesn't exist
   if (i == m_userCount)
   {
      m_userCount++;
      m_users = (UserDatabaseObject **)realloc(m_users, sizeof(UserDatabaseObject *) * m_userCount);
		m_users[i] = new Group();
      nxlog_write(MSG_EVERYONE_GROUP_CREATED, EVENTLOG_WARNING_TYPE, NULL);
   }

   return TRUE;
}


//
// Save user list to database
//

void SaveUsers(DB_HANDLE hdb)
{
   int i;

   // Save users
   MutexLock(m_mutexUserDatabaseAccess, INFINITE);
   for(i = 0; i < m_userCount; i++)
   {
      if (m_users[i]->isDeleted())
      {
			m_users[i]->deleteFromDatabase(hdb);
			delete m_users[i];
         m_userCount--;
         memmove(&m_users[i], &m_users[i + 1], sizeof(UserDatabaseObject *) * (m_userCount - i));
         i--;
      }
		else if (m_users[i]->isModified())
      {
			m_users[i]->saveToDatabase(hdb);
      }
   }
   MutexUnlock(m_mutexUserDatabaseAccess);
}


//
// Authenticate user
// Checks if provided login name and password are correct, and returns RCC_SUCCESS
// on success and appropriate RCC otherwise. On success authentication, user's ID is stored
// int pdwId. If password authentication is used, dwSigLen should be set to zero.
//

DWORD AuthenticateUser(TCHAR *pszName, TCHAR *pszPassword,
							  DWORD dwSigLen, void *pCert, BYTE *pChallenge,
							  DWORD *pdwId, DWORD *pdwSystemRights,
							  bool *pbChangePasswd, bool *pbIntruderLockout)
{
   int i, j;
   DWORD dwResult = RCC_ACCESS_DENIED;
   BOOL bPasswordValid;

   MutexLock(m_mutexUserDatabaseAccess, INFINITE);
   for(i = 0; i < m_userCount; i++)
   {
		if ((!(m_users[i]->getId() & GROUP_FLAG)) &&
			 (!_tcscmp(pszName, m_users[i]->getName())) &&
			 (!m_users[i]->isDeleted()))
      {
			User *user = (User *)m_users[i];

         switch(user->getAuthMethod())
         {
            case AUTH_NETXMS_PASSWORD:
					if (dwSigLen == 0)
					{
						bPasswordValid = user->validatePassword(pszPassword);
					}
					else
					{
						// We got certificate instead of password
						bPasswordValid = FALSE;
					}
               break;
            case AUTH_RADIUS:
					if (dwSigLen == 0)
					{
	               bPasswordValid = RadiusAuth(pszName, pszPassword);
					}
					else
					{
						// We got certificate instead of password
						bPasswordValid = FALSE;
					}
               break;
				case AUTH_CERTIFICATE:
					if ((dwSigLen != 0) && (pCert != NULL))
					{
#ifdef _WITH_ENCRYPTION
						bPasswordValid = ValidateUserCertificate((X509 *)pCert, pszName, pChallenge,
						                                         (BYTE *)pszPassword, dwSigLen,
																			  user->getCertMappingMethod(),
																			  user->getCertMappingData());
#else
						bPasswordValid = FALSE;
#endif
					}
					else
					{
						// We got password instead of certificate
						bPasswordValid = FALSE;
					}
					break;
            default:
               nxlog_write(MSG_UNKNOWN_AUTH_METHOD, EVENTLOG_WARNING_TYPE, "ds",
					            user->getAuthMethod(), pszName);
               bPasswordValid = FALSE;
               break;
         }

         if (bPasswordValid)
         {
            if (!user->isDisabled())
            {
					user->resetAuthFailures();
					if (user->getFlags() & UF_CHANGE_PASSWORD)
               {
						DbgPrintf(4, _T("Password for user \"%s\" need to be changed"), user->getName());
						if (user->getId() != 0)	// Do not check grace logins for built-in admin user
						{
							if (user->getGraceLogins() <= 0)
							{
								DbgPrintf(4, _T("User \"%s\" has no grace logins left"), user->getName());
								dwResult = RCC_NO_GRACE_LOGINS;
								break;
							}
							user->decreaseGraceLogins();
						}
                  *pbChangePasswd = true;
               }
               else
               {
						// Check if password was expired
						int passwordExpirationTime = ConfigReadInt(_T("PasswordExpiration"), 0);
						if ((user->getAuthMethod() == AUTH_NETXMS_PASSWORD) && (passwordExpirationTime > 0) && (time(NULL) > user->getPasswordChangeTime() + passwordExpirationTime * 86400))
						{
							DbgPrintf(4, _T("Password for user \"%s\" has expired"), user->getName());
							if (user->getId() != 0)	// Do not check grace logins for built-in admin user
							{
								if (user->getGraceLogins() <= 0)
								{
									DbgPrintf(4, _T("User \"%s\" has no grace logins left"), user->getName());
									dwResult = RCC_NO_GRACE_LOGINS;
									break;
								}
								user->decreaseGraceLogins();
							}
							*pbChangePasswd = true;
						}
						else
						{
							*pbChangePasswd = false;
						}
               }
               *pdwId = user->getId();
               *pdwSystemRights = user->getSystemRights();
					user->updateLastLogin();
               dwResult = RCC_SUCCESS;
         
               // Collect system rights from groups this user belongs to
               for(j = 0; j < m_userCount; j++)
						if ((m_users[j]->getId() & GROUP_FLAG) &&
							 (((Group *)m_users[j])->isMember(user->getId())))
							*pdwSystemRights |= ((Group *)m_users[j])->getSystemRights();
            }
            else
            {
               dwResult = RCC_ACCOUNT_DISABLED;
            }
				*pbIntruderLockout = false;
         }
			else
			{
				user->increaseAuthFailures();
				*pbIntruderLockout = user->isIntruderLockoutActive();
			}
         break;
      }
   }
   MutexUnlock(m_mutexUserDatabaseAccess);
   return dwResult;
}


//
// Check if user is a member of specific group
//

bool NXCORE_EXPORTABLE CheckUserMembership(DWORD dwUserId, DWORD dwGroupId)
{
   bool result = false;

	if (!(dwGroupId & GROUP_FLAG))
		return false;

   if (dwGroupId == GROUP_EVERYONE)
		return true;

   MutexLock(m_mutexUserDatabaseAccess, INFINITE);
   for(int i = 0; i < m_userCount; i++)
		if (m_users[i]->getId() == dwGroupId)
		{
			result = ((Group *)m_users[i])->isMember(dwUserId);
			break;
		}
   MutexUnlock(m_mutexUserDatabaseAccess);
   return result;
}


//
// Resolve user's ID to login name
//

bool NXCORE_EXPORTABLE ResolveUserId(DWORD id, TCHAR *buffer, int bufSize)
{
	bool found = false;

   MutexLock(m_mutexUserDatabaseAccess, INFINITE);
   for(int i = 0; i < m_userCount; i++)
		if (m_users[i]->getId() == id)
		{
			nx_strncpy(buffer, m_users[i]->getName(), bufSize);
			found = true;
			break;
		}
   MutexUnlock(m_mutexUserDatabaseAccess);

	return found;
}


//
// Dump user list to stdout
//

void DumpUsers(CONSOLE_CTX pCtx)
{
   int i;
   char szGUID[64];

   ConsolePrintf(pCtx, "Login name           GUID                                 System rights\n"
                       "-----------------------------------------------------------------------\n");
   MutexLock(m_mutexUserDatabaseAccess, INFINITE);
   for(i = 0; i < m_userCount; i++)
		if (!(m_users[i]->getId() & GROUP_FLAG))
			ConsolePrintf(pCtx, "%-20s %-36s 0x%08X\n", m_users[i]->getName(),
			              m_users[i]->getGuidAsText(szGUID), m_users[i]->getSystemRights());
   MutexUnlock(m_mutexUserDatabaseAccess);
   ConsolePrintf(pCtx, "\n");
}


//
// Delete user or group
// Will return RCC code
//

DWORD NXCORE_EXPORTABLE DeleteUserDatabaseObject(DWORD id)
{
   int i, j;

   DeleteUserFromAllObjects(id);

   MutexLock(m_mutexUserDatabaseAccess, INFINITE);

   for(i = 0; i < m_userCount; i++)
	{
		if (m_users[i]->getId() == id)
		{
			m_users[i]->setDeleted();
			if (!(id & GROUP_FLAG))
			{
				// Remove user from all groups
				for(j = 0; j < m_userCount; j++)
				{
					if (m_users[j]->getId() & GROUP_FLAG)
					{
						((Group *)m_users[j])->deleteUser(id);
					}
				}
			}
			break;
		}
	}

   MutexUnlock(m_mutexUserDatabaseAccess);

   SendUserDBUpdate(USER_DB_DELETE, id, NULL);
   return RCC_SUCCESS;
}


//
// Create new user or group
//

DWORD NXCORE_EXPORTABLE CreateNewUser(TCHAR *pszName, BOOL bIsGroup, DWORD *pdwId)
{
   DWORD dwResult = RCC_SUCCESS;
	UserDatabaseObject *object;
	int i;

   MutexLock(m_mutexUserDatabaseAccess, INFINITE);

   // Check for duplicate name
   for(i = 0; i < m_userCount; i++)
	{
      if (!_tcscmp(m_users[i]->getName(), pszName))
      {
         dwResult = RCC_ALREADY_EXIST;
         break;
      }
	}

	if (dwResult == RCC_SUCCESS)
	{
		if (bIsGroup)
		{
			object = new Group(CreateUniqueId(IDG_USER_GROUP), pszName);
		}
		else
		{
			object = new User(CreateUniqueId(IDG_USER), pszName);
		}

		m_users = (UserDatabaseObject **)realloc(m_users, sizeof(UserDatabaseObject *) * (m_userCount + 1));
		m_users[m_userCount] = object;
		m_userCount++;

		SendUserDBUpdate(USER_DB_CREATE, object->getId(), object);

		*pdwId = object->getId();
	}

   MutexUnlock(m_mutexUserDatabaseAccess);
	return dwResult;
}


//
// Modify user database object
//

DWORD NXCORE_EXPORTABLE ModifyUserDatabaseObject(CSCPMessage *msg)
{
   DWORD id, fields, dwResult = RCC_INVALID_USER_ID;
	int i;

	id = msg->GetVariableLong(VID_USER_ID);

   MutexLock(m_mutexUserDatabaseAccess, INFINITE);

   // Find object to be modified in list
   for(i = 0; i < m_userCount; i++)
		if (m_users[i]->getId() == id)
      {
			TCHAR name[MAX_USER_NAME];

			fields = msg->GetVariableLong(VID_FIELDS);
			if (fields & USER_MODIFY_LOGIN_NAME)
			{
				msg->GetVariableStr(VID_USER_NAME, name, MAX_USER_NAME);
				if (!IsValidObjectName(name))
				{
					dwResult = RCC_INVALID_OBJECT_NAME;
					break;
				}
			}

			m_users[i]->modifyFromMessage(msg);
         SendUserDBUpdate(USER_DB_MODIFY, id, m_users[i]);
         dwResult = RCC_SUCCESS;
         break;
      }

   MutexUnlock(m_mutexUserDatabaseAccess);
   return dwResult;
}


//
// Check if string contains subsequence of given sequence
//

static bool IsStringContainsSubsequence(const TCHAR *str, const TCHAR *sequence, int len)
{
	int sequenceLen = (int)_tcslen(sequence);
	if ((sequenceLen < len) || (len > 255))
		return false;

	TCHAR subseq[256];
	for(int i = 0; i < sequenceLen - len; i++)
	{
		nx_strncpy(subseq, &sequence[i], len + 1);
		if (_tcsstr(str, subseq) != NULL)
			return true;
	}

	return false;
}


//
// Check password's complexity
//

static bool CheckPasswordComplexity(const TCHAR *password)
{
	int flags = ConfigReadInt(_T("PasswordComplexity"), 0);

	if ((flags & PSWD_MUST_CONTAIN_DIGITS) && (_tcspbrk(password, _T("0123456789")) == NULL))
		return false;

	if ((flags & PSWD_MUST_CONTAIN_UPPERCASE) && (_tcspbrk(password, _T("ABCDEFGHIJKLMNOPQRSTUVWXYZ")) == NULL))
		return false;

	if ((flags & PSWD_MUST_CONTAIN_LOWERCASE) && (_tcspbrk(password, _T("abcdefghijklmnopqrstuvwxyz")) == NULL))
		return false;

	if ((flags & PSWD_MUST_CONTAIN_SPECIAL_CHARS) && (_tcspbrk(password, _T("`~!@#$%^&*()_-=+{}[]|\\'\";:,.<>/?")) == NULL))
		return false;

	if (flags & PSWD_FORBID_ALPHABETICAL_SEQUENCE)
	{
		if (IsStringContainsSubsequence(password, _T("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), 3))
			return false;
		if (IsStringContainsSubsequence(password, _T("abcdefghijklmnopqrstuvwxyz"), 3))
			return false;
	}

	if (flags & PSWD_FORBID_KEYBOARD_SEQUENCE)
	{
		if (IsStringContainsSubsequence(password, _T("~!@#$%^&*()_+"), 3))
			return false;
		if (IsStringContainsSubsequence(password, _T("1234567890-="), 3))
			return false;
		if (IsStringContainsSubsequence(password, _T("qwertyuiop[]"), 3))
			return false;
		if (IsStringContainsSubsequence(password, _T("asdfghjkl;�"), 3))
			return false;
		if (IsStringContainsSubsequence(password, _T("zxcvbnm,./"), 3))
			return false;
		if (IsStringContainsSubsequence(password, _T("QWERTYUIOP{}"), 3))
			return false;
		if (IsStringContainsSubsequence(password, _T("ASDFGHJKL:\""), 3))
			return false;
		if (IsStringContainsSubsequence(password, _T("ZXCVBNM<>?"), 3))
			return false;
	}

	return true;
}


//
// Set user's password
//

DWORD NXCORE_EXPORTABLE SetUserPassword(DWORD id, const TCHAR *newPassword, const TCHAR *oldPassword, bool changeOwnPassword)
{
	int i;
   DWORD dwResult = RCC_INVALID_USER_ID;

	if (id & GROUP_FLAG)
		return RCC_INVALID_USER_ID;

   MutexLock(m_mutexUserDatabaseAccess, INFINITE);

   // Find user
   for(i = 0; i < m_userCount; i++)
		if (m_users[i]->getId() == id)
      {
			User *user = (User *)m_users[i];
			if (changeOwnPassword)
			{
				if (!user->validatePassword(oldPassword))
				{
					dwResult = RCC_ACCESS_DENIED;
					break;
				}

				// Check password length
				int minLength = (user->getMinMasswordLength() == -1) ? ConfigReadInt(_T("MinPasswordLength"), 0) : user->getMinMasswordLength();
				if ((int)_tcslen(newPassword) < minLength)
				{
					dwResult = RCC_WEAK_PASSWORD;
					break;
				}

				// Check password complexity
				if (!CheckPasswordComplexity(newPassword))
				{
					dwResult = RCC_WEAK_PASSWORD;
					break;
				}

				// Update password history
				int passwordHistoryLength = ConfigReadInt(_T("PasswordHistoryLength"), 0);
				if (passwordHistoryLength > 0)
				{
					TCHAR query[8192], *ph = NULL;

					_sntprintf(query, 8192, _T("SELECT password_history FROM users WHERE id=%d"), id);
					DB_RESULT hResult = DBSelect(g_hCoreDB, query);
					if (hResult != NULL)
					{
						if (DBGetNumRows(hResult) > 0)
						{
							ph = DBGetField(hResult, 0, 0, NULL, 0);
						}
						DBFreeResult(hResult);
					}

					if (ph != NULL)
					{
						BYTE newPasswdHash[SHA1_DIGEST_SIZE];
#ifdef UNICODE
						char *mb = MBStringFromWideString(newPassword);
						CalculateSHA1Hash((BYTE *)mb, strlen(mb), newPasswdHash);
						free(mb);
#else
						CalculateSHA1Hash((BYTE *)newPassword, strlen(newPassword), newPasswdHash);
#endif

						int phLen = (int)_tcslen(ph) / (SHA1_DIGEST_SIZE * 2);
						if (phLen > passwordHistoryLength)
							phLen = passwordHistoryLength;

						for(int i = 0; i < phLen; i++)
						{
							BYTE hash[SHA1_DIGEST_SIZE];
							StrToBin(&ph[i * SHA1_DIGEST_SIZE * 2], hash, SHA1_DIGEST_SIZE);
							if (!memcmp(hash, newPasswdHash, SHA1_DIGEST_SIZE))
							{
								dwResult = RCC_REUSED_PASSWORD;
								break;
							}
						}
						
						if (dwResult != RCC_REUSED_PASSWORD)
						{
							if (phLen == passwordHistoryLength)
							{
								memmove(ph, &ph[SHA1_DIGEST_SIZE * 2], (phLen - 1) * SHA1_DIGEST_SIZE * 2 * sizeof(TCHAR));
							}
							else
							{
								ph = (TCHAR *)realloc(ph, (phLen + 1) * SHA1_DIGEST_SIZE * 2 * sizeof(TCHAR) + sizeof(TCHAR));
								phLen++;
							}
							BinToStr(newPasswdHash, SHA1_DIGEST_SIZE, &ph[(phLen - 1) * SHA1_DIGEST_SIZE * 2]);

							_sntprintf(query, 8192, _T("UPDATE users SET password_history='%s' WHERE id=%d"), ph, id);
							DBQuery(g_hCoreDB, query);
						}

						free(ph);
						if (dwResult == RCC_REUSED_PASSWORD)
							break;
					}
					else
					{
						dwResult = RCC_DB_FAILURE;
						break;
					}
				}

				user->updatePasswordChangeTime();
			}
			user->setPassword(newPassword, changeOwnPassword);
         dwResult = RCC_SUCCESS;
         break;
      }

   MutexUnlock(m_mutexUserDatabaseAccess);
   return dwResult;
}


//
// Open user database
//

UserDatabaseObject NXCORE_EXPORTABLE **OpenUserDatabase(int *count)
{
   MutexLock(m_mutexUserDatabaseAccess, INFINITE);
	*count = m_userCount;
	return m_users;
}


//
// Close user database
//

void NXCORE_EXPORTABLE CloseUserDatabase()
{
   MutexUnlock(m_mutexUserDatabaseAccess);
}


//
// Get custom attribute's value
//

const TCHAR NXCORE_EXPORTABLE *GetUserDbObjectAttr(DWORD id, const TCHAR *name)
{
	const TCHAR *value = NULL;

   MutexLock(m_mutexUserDatabaseAccess, INFINITE);

   for(int i = 0; i < m_userCount; i++)
		if (m_users[i]->getId() == id)
      {
			value = m_users[i]->getAttribute(name);
         break;
      }

   MutexUnlock(m_mutexUserDatabaseAccess);
	return value;
}

DWORD NXCORE_EXPORTABLE GetUserDbObjectAttrAsULong(DWORD id, const TCHAR *name)
{
	const TCHAR *value = GetUserDbObjectAttr(id, name);
	return (value != NULL) ? _tcstoul(value, NULL, 0) : 0;
}


//
// Set custom attribute's value
//

void NXCORE_EXPORTABLE SetUserDbObjectAttr(DWORD id, const TCHAR *name, const TCHAR *value)
{
   MutexLock(m_mutexUserDatabaseAccess, INFINITE);

   for(int i = 0; i < m_userCount; i++)
		if (m_users[i]->getId() == id)
      {
			m_users[i]->setAttribute(name, value);
         break;
      }

   MutexUnlock(m_mutexUserDatabaseAccess);
}
