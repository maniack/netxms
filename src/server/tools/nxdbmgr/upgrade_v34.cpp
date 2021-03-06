/*
** nxdbmgr - NetXMS database manager
** Copyright (C) 2004-2020 Raden Solutions
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
** File: upgrade_v34.cpp
**
**/

#include "nxdbmgr.h"
#include <nxevent.h>

/**
 * Upgrade from 34.3 to 34.4
 */
static bool H_UpgradeFromV3()
{
   //Recreate index for those who initialized database with an incorrect init script
   CHK_EXEC(DBDropPrimaryKey(g_dbHandle, _T("snmp_communities")));
   CHK_EXEC(DBDropPrimaryKey(g_dbHandle, _T("usm_credentials")));
   CHK_EXEC(DBDropPrimaryKey(g_dbHandle, _T("shared_secrets")));

   CHK_EXEC(DBAddPrimaryKey(g_dbHandle, _T("snmp_communities"), _T("id,zone")));
   CHK_EXEC(DBAddPrimaryKey(g_dbHandle, _T("usm_credentials"), _T("id,zone")));
   CHK_EXEC(DBAddPrimaryKey(g_dbHandle, _T("shared_secrets"),  _T("id,zone")));

   CHK_EXEC(SetMinorSchemaVersion(4));
   return true;
}

/**
 * Upgrade from 34.2 to 34.3
 */
static bool H_UpgradeFromV2()
{
   CHK_EXEC(DBDropColumn(g_dbHandle, _T("audit_log"), _T("value_diff")));

   if (g_dbSyntax == DB_SYNTAX_ORACLE)
   {
      CHK_EXEC(SQLQuery(_T("ALTER TABLE audit_log ADD (value_type char(1), hmac varchar(64))")));
   }
   else
   {
      static const TCHAR *batch =
         _T("ALTER TABLE audit_log ADD value_type char(1)\n")
         _T("ALTER TABLE audit_log ADD hmac varchar(64)\n")
         _T("<END>");
      CHK_EXEC(SQLBatch(batch));
   }

   CHK_EXEC(SetMinorSchemaVersion(3));
   return true;
}

/**
 * Upgrade from 34.1 to 34.2
 */
static bool H_UpgradeFromV1()
{
   CHK_EXEC(ConvertStrings(_T("conditions"), _T("id"), _T("script")));
   CHK_EXEC(ConvertStrings(_T("agent_configs"), _T("config_id"), _T("config_name")));
   CHK_EXEC(ConvertStrings(_T("agent_configs"), _T("config_id"), _T("config_file")));
   CHK_EXEC(ConvertStrings(_T("agent_configs"), _T("config_id"), _T("config_filter")));
   CHK_EXEC(ConvertStrings(_T("certificates"), _T("cert_id"), _T("subject")));
   CHK_EXEC(ConvertStrings(_T("certificates"), _T("cert_id"), _T("comments")));
   CHK_EXEC(SetMinorSchemaVersion(2));
   return true;
}

/**
 * Upgrade from 34.0 to 34.1
 */
static bool H_UpgradeFromV0()
{
   CHK_EXEC(ConvertStrings(_T("agent_pkg"), _T("pkg_id"), _T("description")));
   CHK_EXEC(SetMinorSchemaVersion(1));
   return true;
}

/**
 * Upgrade map
 */
static struct
{
   int version;
   int nextMajor;
   int nextMinor;
   bool (* upgradeProc)();
} s_dbUpgradeMap[] =
{
   { 3,  34, 4,  H_UpgradeFromV3  },
   { 2,  34, 3,  H_UpgradeFromV2  },
   { 1,  34, 2,  H_UpgradeFromV1  },
   { 0,  34, 1,  H_UpgradeFromV0  },
   { 0,  0,  0,  nullptr          }
};

/**
 * Upgrade database to new version
 */
bool MajorSchemaUpgrade_V34()
{
   INT32 major, minor;
   if (!DBGetSchemaVersion(g_dbHandle, &major, &minor))
      return false;

   while((major == 34) && (minor < DB_SCHEMA_VERSION_V34_MINOR))
   {
      // Find upgrade procedure
      int i;
      for(i = 0; s_dbUpgradeMap[i].upgradeProc != nullptr; i++)
         if (s_dbUpgradeMap[i].version == minor)
            break;
      if (s_dbUpgradeMap[i].upgradeProc == nullptr)
      {
         _tprintf(_T("Unable to find upgrade procedure for version 34.%d\n"), minor);
         return false;
      }
      _tprintf(_T("Upgrading from version 34.%d to %d.%d\n"), minor, s_dbUpgradeMap[i].nextMajor, s_dbUpgradeMap[i].nextMinor);
      DBBegin(g_dbHandle);
      if (s_dbUpgradeMap[i].upgradeProc())
      {
         DBCommit(g_dbHandle);
         if (!DBGetSchemaVersion(g_dbHandle, &major, &minor))
            return false;
      }
      else
      {
         _tprintf(_T("Rolling back last stage due to upgrade errors...\n"));
         DBRollback(g_dbHandle);
         return false;
      }
   }
   return true;
}
