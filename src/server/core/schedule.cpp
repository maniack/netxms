/*
** NetXMS - Network Management System
** Copyright (C) 2003-2020 Raden Solutions
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
** File: schedule.cpp
**
**/

#include "nxcore.h"

#define DEBUG_TAG _T("scheduler")

/**
 * Static fields
 */
static StringObjectMap<SchedulerCallback> s_callbacks(Ownership::True);
static ObjectArray<ScheduledTask> s_cronSchedules(5, 5, Ownership::True);
static ObjectArray<ScheduledTask> s_oneTimeSchedules(5, 5, Ownership::True);
static Condition s_wakeupCondition(false);
static Mutex s_cronScheduleLock;
static Mutex s_oneTimeScheduleLock;

/**
 * Scheduled task execution pool
 */
ThreadPool *g_schedulerThreadPool = NULL;

/**
 * Task handler replacement for missing handlers
 */
static void MissingTaskHandler(const shared_ptr<ScheduledTaskParameters>& parameters)
{
}

/**
 * Callback definition for missing task handlers
 */
static SchedulerCallback s_missingTaskHandler(MissingTaskHandler, 0);

/**
 * Constructor for scheduled task transient data
 */
ScheduledTaskTransientData::ScheduledTaskTransientData()
{
}

/**
 * Destructor for scheduled task transient data
 */
ScheduledTaskTransientData::~ScheduledTaskTransientData()
{
}

/**
 * Create recurrent task object
 */
ScheduledTask::ScheduledTask(uint32_t id, const TCHAR *taskHandlerId, const TCHAR *schedule,
         shared_ptr<ScheduledTaskParameters> parameters, bool systemTask) :
         m_taskHandlerId(taskHandlerId), m_schedule(schedule)
{
   m_id = id;
   m_parameters = parameters;
   m_scheduledExecutionTime = NEVER;
   m_lastExecutionTime = NEVER;
   m_recurrent = true;
   m_flags = systemTask ? SCHEDULED_TASK_SYSTEM : 0;
   m_mutex = MutexCreate();
}

/**
 * Create one-time execution task object
 */
ScheduledTask::ScheduledTask(uint32_t id, const TCHAR *taskHandlerId, time_t executionTime,
         shared_ptr<ScheduledTaskParameters> parameters, bool systemTask) :
         m_taskHandlerId(taskHandlerId)
{
   m_id = id;
   m_parameters = parameters;
   m_scheduledExecutionTime = executionTime;
   m_lastExecutionTime = NEVER;
   m_recurrent = false;
   m_flags = systemTask ? SCHEDULED_TASK_SYSTEM : 0;
   m_mutex = MutexCreate();
}

/**
 * Create task object from database record
 * Expected field order: id,taskid,schedule,params,execution_time,last_execution_time,flags,owner,object_id,comments,task_key
 */
ScheduledTask::ScheduledTask(DB_RESULT hResult, int row)
{
   m_id = DBGetFieldULong(hResult, row, 0);
   m_taskHandlerId = DBGetFieldAsSharedString(hResult, row, 1);
   m_schedule = DBGetFieldAsSharedString(hResult, row, 2);
   m_scheduledExecutionTime = DBGetFieldULong(hResult, row, 4);
   m_lastExecutionTime = DBGetFieldULong(hResult, row, 5);
   m_flags = DBGetFieldULong(hResult, row, 6);
   m_recurrent = !m_schedule.isEmpty();
   m_mutex = MutexCreate();

   TCHAR persistentData[1024];
   DBGetField(hResult, row, 3, persistentData, 1024);
   uint32_t userId = DBGetFieldULong(hResult, row, 7);
   uint32_t objectId = DBGetFieldULong(hResult, row, 8);
   TCHAR key[256], comments[256];
   DBGetField(hResult, row, 9, comments, 256);
   DBGetField(hResult, row, 10, key, 256);
   m_parameters = make_shared<ScheduledTaskParameters>(key, userId, objectId, persistentData, nullptr, comments);
}

/**
 * Destructor
 */
ScheduledTask::~ScheduledTask()
{
   MutexDestroy(m_mutex);
}

/**
 * Update task
 */
void ScheduledTask::update(const TCHAR *taskHandlerId, const TCHAR *schedule,
         shared_ptr<ScheduledTaskParameters> parameters, bool systemTask, bool disabled)
{
   m_taskHandlerId = CHECK_NULL_EX(taskHandlerId);
   m_schedule = CHECK_NULL_EX(schedule);
   m_parameters = parameters;
   m_recurrent = true;

   if (systemTask)
      m_flags |= SCHEDULED_TASK_SYSTEM;
   else
      m_flags &= ~SCHEDULED_TASK_SYSTEM;

   if (disabled)
      m_flags |= SCHEDULED_TASK_DISABLED;
   else
      m_flags &= ~SCHEDULED_TASK_DISABLED;
}

/**
 * Update task
 */
void ScheduledTask::update(const TCHAR *taskHandlerId, time_t nextExecution,
         shared_ptr<ScheduledTaskParameters> parameters, bool systemTask, bool disabled)
{
   m_taskHandlerId = CHECK_NULL_EX(taskHandlerId);
   m_schedule = _T("");
   m_parameters = parameters;
   m_scheduledExecutionTime = nextExecution;
   m_recurrent = false;

   if (systemTask)
      m_flags |= SCHEDULED_TASK_SYSTEM;
   else
      m_flags &= ~SCHEDULED_TASK_SYSTEM;

   if (disabled)
      m_flags |= SCHEDULED_TASK_DISABLED;
   else
      m_flags &= ~SCHEDULED_TASK_DISABLED;
}

/**
 * Save task to database
 */
void ScheduledTask::saveToDatabase(bool newObject) const
{
   DB_HANDLE db = DBConnectionPoolAcquireConnection();

   DB_STATEMENT hStmt;
   if (newObject)
   {
		hStmt = DBPrepare(db,
                    _T("INSERT INTO scheduled_tasks (taskId,schedule,params,execution_time,")
                    _T("last_execution_time,flags,owner,object_id,comments,task_key,id) VALUES (?,?,?,?,?,?,?,?,?,?,?)"));
   }
   else
   {
      hStmt = DBPrepare(db,
                    _T("UPDATE scheduled_tasks SET taskId=?,schedule=?,params=?,")
                    _T("execution_time=?,last_execution_time=?,flags=?,owner=?,object_id=?,")
                    _T("comments=?,task_key=? WHERE id=?"));
   }

   if (hStmt != NULL)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_VARCHAR, m_taskHandlerId, DB_BIND_STATIC);
      DBBind(hStmt, 2, DB_SQLTYPE_VARCHAR, m_schedule, DB_BIND_STATIC);
      DBBind(hStmt, 3, DB_SQLTYPE_VARCHAR, m_parameters->m_persistentData, DB_BIND_STATIC, 1023);
      DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, static_cast<int64_t>(m_scheduledExecutionTime));
      DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, static_cast<int64_t>(m_lastExecutionTime));
      DBBind(hStmt, 6, DB_SQLTYPE_INTEGER, m_flags);
      DBBind(hStmt, 7, DB_SQLTYPE_INTEGER, m_parameters->m_userId);
      DBBind(hStmt, 8, DB_SQLTYPE_INTEGER, m_parameters->m_objectId);
      DBBind(hStmt, 9, DB_SQLTYPE_VARCHAR, m_parameters->m_comments, DB_BIND_STATIC, 255);
      DBBind(hStmt, 10, DB_SQLTYPE_VARCHAR, m_parameters->m_taskKey, DB_BIND_STATIC, 255);
      DBBind(hStmt, 11, DB_SQLTYPE_INTEGER, m_id);

      DBExecute(hStmt);
      DBFreeStatement(hStmt);
   }
	DBConnectionPoolReleaseConnection(db);
	NotifyClientSessions(NX_NOTIFY_SCHEDULE_UPDATE, 0);
}

/**
 * Scheduled task comparator (used for task sorting)
 */
static int ScheduledTaskComparator(const ScheduledTask **e1, const ScheduledTask **e2)
{
   const ScheduledTask *s1 = *e1;
   const ScheduledTask *s2 = *e2;

   // Executed schedules should go down
   if (s1->checkFlag(SCHEDULED_TASK_COMPLETED) != s2->checkFlag(SCHEDULED_TASK_COMPLETED))
   {
      return s1->checkFlag(SCHEDULED_TASK_COMPLETED) ? 1 : -1;
   }

   // Schedules with execution time 0 should go down, others should be compared
   if (s1->getScheduledExecutionTime() == s2->getScheduledExecutionTime())
   {
      return 0;
   }

   return (((s1->getScheduledExecutionTime() < s2->getScheduledExecutionTime()) && (s1->getScheduledExecutionTime() != 0)) || (s2->getScheduledExecutionTime() == 0)) ? -1 : 1;
}

/**
 * Start task execution
 */
void ScheduledTask::startExecution(SchedulerCallback *callback)
{
   lock();
   if (!(m_flags & SCHEDULED_TASK_RUNNING))
   {
      m_flags |= SCHEDULED_TASK_RUNNING;
      ThreadPoolExecute(g_schedulerThreadPool, this, &ScheduledTask::run, callback);
   }
   else
   {
      nxlog_write_tag(NXLOG_WARNING, DEBUG_TAG,
               _T("Internal error - attempt to start execution of already running scheduled task %s [%u]"),
               m_taskHandlerId.cstr(), m_id);
   }
   unlock();
}

/**
 * Run scheduled task
 */
void ScheduledTask::run(SchedulerCallback *callback)
{
	NotifyClientSessions(NX_NOTIFY_SCHEDULE_UPDATE, 0);

   callback->m_handler(m_parameters);

   lock();
   m_lastExecutionTime = time(NULL);
   m_flags &= ~SCHEDULED_TASK_RUNNING;
   m_flags |= SCHEDULED_TASK_COMPLETED;
   saveToDatabase(false);
   unlock();

   if (!m_recurrent)
   {
      s_oneTimeScheduleLock.lock();
      s_oneTimeSchedules.sort(ScheduledTaskComparator);
      s_oneTimeScheduleLock.unlock();

      if (isSystem())
         DeleteScheduledTask(m_id, 0, SYSTEM_ACCESS_FULL);
   }
}

/**
 * Fill NXCP message with task data
 */
void ScheduledTask::fillMessage(NXCPMessage *msg) const
{
   lock();
   msg->setField(VID_SCHEDULED_TASK_ID, m_id);
   msg->setField(VID_TASK_HANDLER, m_taskHandlerId);
   msg->setField(VID_SCHEDULE, m_schedule);
   msg->setField(VID_PARAMETER, m_parameters->m_persistentData);
   msg->setFieldFromTime(VID_EXECUTION_TIME, m_scheduledExecutionTime);
   msg->setFieldFromTime(VID_LAST_EXECUTION_TIME, m_lastExecutionTime);
   msg->setField(VID_FLAGS, m_flags);
   msg->setField(VID_OWNER, m_parameters->m_userId);
   msg->setField(VID_OBJECT_ID, m_parameters->m_objectId);
   msg->setField(VID_COMMENTS, m_parameters->m_comments);
   msg->setField(VID_TASK_KEY, m_parameters->m_taskKey);
   unlock();
}

/**
 * Fill NXCP message with task data
 */
void ScheduledTask::fillMessage(NXCPMessage *msg, uint32_t base) const
{
   lock();
   msg->setField(base, m_id);
   msg->setField(base + 1, m_taskHandlerId);
   msg->setField(base + 2, m_schedule);
   msg->setField(base + 3, m_parameters->m_persistentData);
   msg->setFieldFromTime(base + 4, m_scheduledExecutionTime);
   msg->setFieldFromTime(base + 5, m_lastExecutionTime);
   msg->setField(base + 6, m_flags);
   msg->setField(base + 7, m_parameters->m_userId);
   msg->setField(base + 8, m_parameters->m_objectId);
   msg->setField(base + 9, m_parameters->m_comments);
   msg->setField(base + 10, m_parameters->m_taskKey);
   unlock();
}

/**
 * Check if user can access this scheduled task
 */
bool ScheduledTask::canAccess(uint32_t userId, uint64_t systemAccess) const
{
   if (userId == 0)
      return true;

   if (systemAccess & SYSTEM_ACCESS_ALL_SCHEDULED_TASKS)
      return true;

   bool result = false;
   if (systemAccess & SYSTEM_ACCESS_USER_SCHEDULED_TASKS)
   {
      result = !isSystem();
   }
   else if (systemAccess & SYSTEM_ACCESS_OWN_SCHEDULED_TASKS)
   {
      lock();
      result = (userId == m_parameters->m_userId);
      unlock();
   }

   return false;
}

/**
 * Function that adds to list task handler function
 */
void RegisterSchedulerTaskHandler(const TCHAR *id, ScheduledTaskHandler handler, uint64_t accessRight)
{
   s_callbacks.set(id, new SchedulerCallback(handler, accessRight));
   nxlog_debug_tag(DEBUG_TAG, 6, _T("Registered scheduler task %s"), id);
}

/**
 * Scheduled task creation function
 */
uint32_t NXCORE_EXPORTABLE AddRecurrentScheduledTask(const TCHAR *taskHandlerId, const TCHAR *schedule, const TCHAR *persistentData,
         ScheduledTaskTransientData *transientData, uint32_t owner, uint32_t objectId, uint64_t systemRights,
         const TCHAR *comments, const TCHAR *key, bool systemTask)
{
   if ((systemRights & (SYSTEM_ACCESS_ALL_SCHEDULED_TASKS | SYSTEM_ACCESS_USER_SCHEDULED_TASKS | SYSTEM_ACCESS_OWN_SCHEDULED_TASKS)) == 0)
      return RCC_ACCESS_DENIED;

   nxlog_debug_tag(DEBUG_TAG, 7, _T("AddSchedule: Add recurrent task %s, %s, %s"), taskHandlerId, schedule, persistentData);
   auto task = new ScheduledTask(CreateUniqueId(IDG_SCHEDULED_TASK), taskHandlerId, schedule,
            make_shared<ScheduledTaskParameters>(key, owner, objectId, persistentData, transientData, comments), systemTask);

   s_cronScheduleLock.lock();
   task->saveToDatabase(true);
   s_cronSchedules.add(task);
   s_cronScheduleLock.unlock();

   return RCC_SUCCESS;
}

/**
 * Create scheduled task only if task with same task ID does not exist
 */
uint32_t NXCORE_EXPORTABLE AddUniqueRecurrentScheduledTask(const TCHAR *taskHandlerId, const TCHAR *schedule, const TCHAR *persistentData,
         ScheduledTaskTransientData *transientData, uint32_t owner, uint32_t objectId, uint64_t systemRights,
         const TCHAR *comments, const TCHAR *key, bool systemTask)
{
   if (FindScheduledTaskByHandlerId(taskHandlerId) != NULL)
      return RCC_SUCCESS;
   return AddRecurrentScheduledTask(taskHandlerId, schedule, persistentData, transientData, owner, objectId, systemRights, comments, key, systemTask);
}

/**
 * One time schedule creation function
 */
uint32_t NXCORE_EXPORTABLE AddOneTimeScheduledTask(const TCHAR *taskHandlerId, time_t nextExecutionTime, const TCHAR *persistentData,
         ScheduledTaskTransientData *transientData, uint32_t owner, uint32_t objectId, uint64_t systemRights,
         const TCHAR *comments, const TCHAR *key, bool systemTask)
{
   if ((systemRights & (SYSTEM_ACCESS_ALL_SCHEDULED_TASKS | SYSTEM_ACCESS_USER_SCHEDULED_TASKS | SYSTEM_ACCESS_OWN_SCHEDULED_TASKS)) == 0)
      return RCC_ACCESS_DENIED;

   nxlog_debug_tag(DEBUG_TAG, 5, _T("AddOneTimeAction: Add one time schedule %s, %d, %s"), taskHandlerId, nextExecutionTime, persistentData);
   auto task = new ScheduledTask(CreateUniqueId(IDG_SCHEDULED_TASK), taskHandlerId, nextExecutionTime,
            make_shared<ScheduledTaskParameters>(key, owner, objectId, persistentData, transientData, comments), systemTask);

   s_oneTimeScheduleLock.lock();
   task->saveToDatabase(true);
   s_oneTimeSchedules.add(task);
   s_oneTimeSchedules.sort(ScheduledTaskComparator);
   s_oneTimeScheduleLock.unlock();
   s_wakeupCondition.set();

   return RCC_SUCCESS;
}

/**
 * Recurrent scheduled task update
 */
uint32_t NXCORE_EXPORTABLE UpdateRecurrentScheduledTask(uint32_t id, const TCHAR *taskHandlerId, const TCHAR *schedule, const TCHAR *persistentData,
         ScheduledTaskTransientData *transientData, const TCHAR *comments, uint32_t owner, uint32_t objectId,
         uint64_t systemAccessRights, bool disabled)
{
   nxlog_debug_tag(DEBUG_TAG, 5, _T("UpdateRecurrentScheduledTask: task [%u]: handler=%s, schedule=%s, data=%s"), id, taskHandlerId, schedule, persistentData);

   uint32_t rcc = RCC_SUCCESS;

   bool found = false;
   s_cronScheduleLock.lock();
   for (int i = 0; i < s_cronSchedules.size(); i++)
   {
      ScheduledTask *task = s_cronSchedules.get(i);
      if (task->getId() == id)
      {
         if (!task->canAccess(owner, systemAccessRights))
         {
            rcc = RCC_ACCESS_DENIED;
            break;
         }
         task->update(taskHandlerId, schedule,
                  make_shared<ScheduledTaskParameters>(task->getTaskKey(), owner, objectId, persistentData, transientData, comments),
                  task->isSystem(), disabled);
         task->saveToDatabase(false);
         found = true;
         break;
      }
   }
   s_cronScheduleLock.unlock();

   if (!found)
   {
      // check in different queue and if exists - remove from one and add to another
      ScheduledTask *task = nullptr;
      s_oneTimeScheduleLock.lock();
      for(int i = 0; i < s_oneTimeSchedules.size(); i++)
      {
         if (s_oneTimeSchedules.get(i)->getId() == id)
         {
            if (!s_oneTimeSchedules.get(i)->canAccess(owner, systemAccessRights))
            {
               rcc = RCC_ACCESS_DENIED;
               break;
            }
            task = s_oneTimeSchedules.get(i);
            s_oneTimeSchedules.unlink(i);
            task->update(taskHandlerId, schedule,
                     make_shared<ScheduledTaskParameters>(task->getTaskKey(), owner, objectId, persistentData, transientData, comments),
                     task->isSystem(), disabled);
            task->saveToDatabase(false);
            found = true;
            break;
         }
      }
      s_oneTimeScheduleLock.unlock();

      if (found && (task != nullptr))
      {
         s_cronScheduleLock.lock();
         s_cronSchedules.add(task);
         s_cronScheduleLock.unlock();
      }
   }

   return rcc;
}

/**
 * One time action update
 */
uint32_t NXCORE_EXPORTABLE UpdateOneTimeScheduledTask(uint32_t id, const TCHAR *taskHandlerId, time_t nextExecutionTime, const TCHAR *persistentData,
         ScheduledTaskTransientData *transientData, const TCHAR *comments, uint32_t owner, uint32_t objectId,
         uint64_t systemAccessRights, bool disabled)
{
   nxlog_debug_tag(DEBUG_TAG, 5, _T("UpdateOneTimeScheduledTask: task [%u]: handler=%s, time=") INT64_FMT _T(", data=%s"),
            id, taskHandlerId, static_cast<int64_t>(nextExecutionTime), persistentData);

   uint32_t rcc = RCC_SUCCESS;

   bool found = false;
   s_oneTimeScheduleLock.lock();
   for (int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      ScheduledTask *task = s_oneTimeSchedules.get(i);
      if (task->getId() == id)
      {
         if (!task->canAccess(owner, systemAccessRights))
         {
            rcc = RCC_ACCESS_DENIED;
            break;
         }
         task->update(taskHandlerId, nextExecutionTime,
                  make_shared<ScheduledTaskParameters>(task->getTaskKey(), owner, objectId, persistentData, transientData, comments),
                  task->isSystem(), disabled);
         task->saveToDatabase(false);
         s_oneTimeSchedules.sort(ScheduledTaskComparator);
         found = true;
         break;
      }
   }
   s_oneTimeScheduleLock.unlock();

   if (!found && (rcc == RCC_SUCCESS))
   {
      // check in different queue and if exists - remove from one and add to another
      ScheduledTask *task = nullptr;
      s_cronScheduleLock.lock();
      for (int i = 0; i < s_cronSchedules.size(); i++)
      {
         if (s_cronSchedules.get(i)->getId() == id)
         {
            if (!s_cronSchedules.get(i)->canAccess(owner, systemAccessRights))
            {
               rcc = RCC_ACCESS_DENIED;
               break;
            }
            task = s_cronSchedules.get(i);
            s_cronSchedules.unlink(i);
            task->update(taskHandlerId, nextExecutionTime,
                     make_shared<ScheduledTaskParameters>(task->getTaskKey(), owner, objectId, persistentData, transientData, comments),
                     task->isSystem(), disabled);
            task->saveToDatabase(false);
            found = true;
            break;
         }
      }
      s_cronScheduleLock.unlock();

      if (found && (task != nullptr))
      {
         s_oneTimeScheduleLock.lock();
         s_oneTimeSchedules.add(task);
         s_oneTimeSchedules.sort(ScheduledTaskComparator);
         s_oneTimeScheduleLock.unlock();
      }
   }

   if (found)
      s_wakeupCondition.set();
   return rcc;
}

/**
 * Removes scheduled task from database by id
 */
static void DeleteScheduledTaskFromDB(uint32_t id)
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   ExecuteQueryOnObject(hdb, id, _T("DELETE FROM scheduled_tasks WHERE id=?"));
	DBConnectionPoolReleaseConnection(hdb);
	NotifyClientSessions(NX_NOTIFY_SCHEDULE_UPDATE,0);
}

/**
 * Removes scheduled task by id
 */
uint32_t NXCORE_EXPORTABLE DeleteScheduledTask(uint32_t id, uint32_t user, uint64_t systemRights)
{
   uint32_t rcc = RCC_INVALID_OBJECT_ID;

   s_cronScheduleLock.lock();
   for(int i = 0; i < s_cronSchedules.size(); i++)
   {
      if (s_cronSchedules.get(i)->getId() == id)
      {
         if (!s_cronSchedules.get(i)->canAccess(user, systemRights))
         {
            rcc = RCC_ACCESS_DENIED;
            break;
         }
         s_cronSchedules.remove(i);
         rcc = RCC_SUCCESS;
         break;
      }
   }
   s_cronScheduleLock.unlock();

   if (rcc == RCC_INVALID_OBJECT_ID)
   {
      s_oneTimeScheduleLock.lock();
      for(int i = 0; i < s_oneTimeSchedules.size(); i++)
      {
         if (s_oneTimeSchedules.get(i)->getId() == id)
         {
            if (!s_cronSchedules.get(i)->canAccess(user, systemRights))
            {
               rcc = RCC_ACCESS_DENIED;
               break;
            }
            s_oneTimeSchedules.remove(i);
            s_oneTimeSchedules.sort(ScheduledTaskComparator);
            s_wakeupCondition.set();
            rcc = RCC_SUCCESS;
            break;
         }
      }
      s_oneTimeScheduleLock.unlock();
   }

   if (rcc == RCC_SUCCESS)
   {
      DeleteScheduledTaskFromDB(id);
      nxlog_debug_tag(DEBUG_TAG, 5, _T("RemoveSchedule: scheduled task [%u] removed"), id);
   }

   return rcc;
}

/**
 * Find scheduled task by task handler id
 */
ScheduledTask *FindScheduledTaskByHandlerId(const TCHAR *taskHandlerId)
{
   ScheduledTask *task;
   bool found = false;

   s_cronScheduleLock.lock();
   for (int i = 0; i < s_cronSchedules.size(); i++)
   {
      if (_tcscmp(s_cronSchedules.get(i)->getTaskHandlerId(), taskHandlerId) == 0)
      {
         task = s_cronSchedules.get(i);
         found = true;
         break;
      }
   }
   s_cronScheduleLock.unlock();

   if (found)
      return task;

   s_oneTimeScheduleLock.lock();
   for (int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      if (_tcscmp(s_oneTimeSchedules.get(i)->getTaskHandlerId(), taskHandlerId) == 0)
      {
         task = s_oneTimeSchedules.get(i);
         found = true;
         break;
      }
   }
   s_oneTimeScheduleLock.unlock();

   if (found)
      return task;

   return NULL;
}

/**
 * Delete scheduled task(s) by task handler id
 */
bool NXCORE_EXPORTABLE DeleteScheduledTaskByHandlerId(const TCHAR *taskHandlerId)
{
   IntegerArray<uint32_t> deleteList;

   s_oneTimeScheduleLock.lock();
   for (int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      if (!_tcscmp(s_oneTimeSchedules.get(i)->getTaskHandlerId(), taskHandlerId))
      {
         deleteList.add(s_oneTimeSchedules.get(i)->getId());
         s_oneTimeSchedules.remove(i);
         i--;
      }
   }
   if (!deleteList.isEmpty())
      s_oneTimeSchedules.sort(ScheduledTaskComparator);
   s_oneTimeScheduleLock.unlock();

   s_cronScheduleLock.lock();
   for (int i = 0; i < s_cronSchedules.size(); i++)
   {
      if (!_tcscmp(s_cronSchedules.get(i)->getTaskHandlerId(), taskHandlerId))
      {
         deleteList.add(s_cronSchedules.get(i)->getId());
         s_cronSchedules.remove(i);
         i--;
      }
   }
   s_cronScheduleLock.unlock();

   for(int i = 0; i < deleteList.size(); i++)
   {
      DeleteScheduledTaskFromDB(deleteList.get(i));
   }

   return !deleteList.isEmpty();
}

/**
 * Delete scheduled task(s) by task key
 */
bool NXCORE_EXPORTABLE DeleteScheduledTaskByKey(const TCHAR *taskKey)
{
   IntegerArray<uint32_t> deleteList;

   s_oneTimeScheduleLock.lock();
   for (int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      String k = s_oneTimeSchedules.get(i)->getTaskKey();
      if (!_tcscmp(k, taskKey))
      {
         deleteList.add(s_oneTimeSchedules.get(i)->getId());
         s_oneTimeSchedules.remove(i);
         i--;
      }
   }
   if (!deleteList.isEmpty())
      s_oneTimeSchedules.sort(ScheduledTaskComparator);
   s_oneTimeScheduleLock.unlock();

   s_cronScheduleLock.lock();
   for (int i = 0; i < s_cronSchedules.size(); i++)
   {
      String k = s_cronSchedules.get(i)->getTaskKey();
      if (!_tcscmp(k, taskKey))
      {
         deleteList.add(s_cronSchedules.get(i)->getId());
         s_cronSchedules.remove(i);
         i--;
      }
   }
   s_cronScheduleLock.unlock();

   for(int i = 0; i < deleteList.size(); i++)
   {
      DeleteScheduledTaskFromDB(deleteList.get(i));
   }

   return !deleteList.isEmpty();
}

/**
 * Get number of scheduled tasks with given key
 */
int NXCORE_EXPORTABLE CountScheduledTasksByKey(const TCHAR *taskKey)
{
   int count = 0;

   s_oneTimeScheduleLock.lock();
   for (int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      const TCHAR *k = s_oneTimeSchedules.get(i)->getTaskKey();
      if ((k != NULL) && !_tcscmp(k, taskKey))
      {
         count++;
      }
   }
   s_oneTimeScheduleLock.unlock();

   s_cronScheduleLock.lock();
   for (int i = 0; i < s_cronSchedules.size(); i++)
   {
      const TCHAR *k = s_cronSchedules.get(i)->getTaskKey();
      if ((k != NULL) && !_tcscmp(k, taskKey))
      {
         count++;
      }
   }
   s_cronScheduleLock.unlock();

   return count;
}

/**
 * Fills message with scheduled tasks list
 */
void GetScheduledTasks(NXCPMessage *msg, uint32_t userId, uint64_t systemRights)
{
   int scheduleCount = 0;
   int base = VID_SCHEDULE_LIST_BASE;

   s_oneTimeScheduleLock.lock();
   for(int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      if (s_oneTimeSchedules.get(i)->canAccess(userId, systemRights))
      {
         s_oneTimeSchedules.get(i)->fillMessage(msg, base);
         scheduleCount++;
         base += 100;
      }
   }
   s_oneTimeScheduleLock.unlock();

   s_cronScheduleLock.lock();
   for(int i = 0; i < s_cronSchedules.size(); i++)
   {
      if (s_cronSchedules.get(i)->canAccess(userId, systemRights))
      {
         s_cronSchedules.get(i)->fillMessage(msg, base);
         scheduleCount++;
         base += 100;
      }
   }
   s_cronScheduleLock.unlock();

   msg->setField(VID_SCHEDULE_COUNT, scheduleCount);
}

/**
 * Fills message with task handlers list
 */
void GetSchedulerTaskHandlers(NXCPMessage *msg, uint64_t accessRights)
{
   uint32_t base = VID_CALLBACK_BASE;
   int count = 0;

   StringList *keyList = s_callbacks.keys();
   for(int i = 0; i < keyList->size(); i++)
   {
      if(accessRights & s_callbacks.get(keyList->get(i))->m_accessRight)
      {
         msg->setField(base, keyList->get(i));
         count++;
         base++;
      }
   }
   delete keyList;
   msg->setField(VID_CALLBACK_COUNT, (uint32_t)count);
}

/**
 * Creates scheduled task from message
 */
uint32_t CreateScheduledTaskFromMsg(NXCPMessage *request, uint32_t owner, uint64_t systemAccessRights)
{
   TCHAR *taskHandler = request->getFieldAsString(VID_TASK_HANDLER);
   TCHAR *persistentData = request->getFieldAsString(VID_PARAMETER);
   TCHAR *comments = request->getFieldAsString(VID_COMMENTS);
   uint32_t objectId = request->getFieldAsInt32(VID_OBJECT_ID);
   uint32_t rcc;
   if (request->isFieldExist(VID_SCHEDULE))
   {
      TCHAR *schedule = request->getFieldAsString(VID_SCHEDULE);
      rcc = AddRecurrentScheduledTask(taskHandler, schedule, persistentData, NULL, owner, objectId, systemAccessRights, comments);
      MemFree(schedule);
   }
   else
   {
      rcc = AddOneTimeScheduledTask(taskHandler, request->getFieldAsTime(VID_EXECUTION_TIME),
               persistentData, NULL, owner, objectId, systemAccessRights, comments);
   }
   MemFree(taskHandler);
   MemFree(persistentData);
   MemFree(comments);
   return rcc;
}

/**
 * Update scheduled task from message
 */
uint32_t UpdateScheduledTaskFromMsg(NXCPMessage *request,  uint32_t owner, uint64_t systemAccessRights)
{
   uint32_t taskId = request->getFieldAsInt32(VID_SCHEDULED_TASK_ID);
   TCHAR *taskHandler = request->getFieldAsString(VID_TASK_HANDLER);
   TCHAR *persistentData = request->getFieldAsString(VID_PARAMETER);
   TCHAR *comments = request->getFieldAsString(VID_COMMENTS);
   uint32_t objectId = request->getFieldAsInt32(VID_OBJECT_ID);
   bool disabled = request->getFieldAsBoolean(VID_TASK_IS_DISABLED);
   uint32_t rcc;
   if (request->isFieldExist(VID_SCHEDULE))
   {
      TCHAR *schedule = request->getFieldAsString(VID_SCHEDULE);
      rcc = UpdateRecurrentScheduledTask(taskId, taskHandler, schedule, persistentData, nullptr,
               comments, owner, objectId, systemAccessRights, disabled);
      MemFree(schedule);
   }
   else
   {
      rcc = UpdateOneTimeScheduledTask(taskId, taskHandler, request->getFieldAsTime(VID_EXECUTION_TIME),
               persistentData, nullptr, comments, owner, objectId, systemAccessRights, disabled);
   }
   MemFree(taskHandler);
   MemFree(persistentData);
   MemFree(comments);
   return rcc;
}

/**
 * Thread that checks one time schedules and executes them
 */
static THREAD_RESULT THREAD_CALL AdHocScheduler(void *arg)
{
   ThreadSetName("Scheduler/A");
   uint32_t sleepTime = 1;
   uint32_t watchdogId = WatchdogAddThread(_T("Ad hoc scheduler"), 5);
   nxlog_debug_tag(DEBUG_TAG, 3, _T("Ad hoc scheduler started"));
   while(true)
   {
      WatchdogStartSleep(watchdogId);
      s_wakeupCondition.wait(sleepTime * 1000);
      WatchdogNotify(watchdogId);

      if (g_flags & AF_SHUTDOWN)
         break;

      sleepTime = 3600;

      s_oneTimeScheduleLock.lock();
      time_t now = time(NULL);
      for(int i = 0; i < s_oneTimeSchedules.size(); i++)
      {
         ScheduledTask *task = s_oneTimeSchedules.get(i);
         if (task->isDisabled() || task->isRunning() || task->isCompleted())
            continue;

         if (task->getScheduledExecutionTime() == NEVER)
            break;   // there won't be any more schedulable tasks

         // execute all tasks that is expected to execute now
         if (now >= task->getScheduledExecutionTime())
         {
            nxlog_debug_tag(DEBUG_TAG, 6, _T("AdHocScheduler: run scheduled task with id = %d, execution time = ") INT64_FMT,
                     task->getId(), static_cast<int64_t>(task->getScheduledExecutionTime()));

            SchedulerCallback *callback = s_callbacks.get(task->getTaskHandlerId());
            if (callback == NULL)
            {
               nxlog_debug_tag(DEBUG_TAG, 3, _T("AdHocScheduler: task handler \"%s\" not registered"), task->getTaskHandlerId().cstr());
               callback = &s_missingTaskHandler;
            }

            task->startExecution(callback);
         }
         else
         {
            time_t diff = task->getScheduledExecutionTime() - now;
            if (diff < (time_t)3600)
               sleepTime = (uint32_t)diff;
            break;
         }
      }
      s_oneTimeScheduleLock.unlock();
      nxlog_debug_tag(DEBUG_TAG, 6, _T("AdHocScheduler: sleeping for %d seconds"), sleepTime);
   }
   nxlog_debug_tag(DEBUG_TAG, 3, _T("Ad hoc scheduler stopped"));
   return THREAD_OK;
}

/**
 * Wakes up for execution of one time schedule or for recalculation new wake up timestamp
 */
static THREAD_RESULT THREAD_CALL RecurrentScheduler(void *arg)
{
   ThreadSetName("Scheduler/R");
   uint32_t watchdogId = WatchdogAddThread(_T("Recurrent scheduler"), 5);
   nxlog_debug_tag(DEBUG_TAG, 3, _T("Recurrent scheduler started"));
   do
   {
      WatchdogNotify(watchdogId);
      time_t now = time(NULL);
      struct tm currLocal;
#if HAVE_LOCALTIME_R
      localtime_r(&now, &currLocal);
#else
      memcpy(&currLocal, localtime(&now), sizeof(struct tm));
#endif

      s_cronScheduleLock.lock();
      for(int i = 0; i < s_cronSchedules.size(); i++)
      {
         ScheduledTask *task = s_cronSchedules.get(i);
         if (task->isDisabled() || task->isRunning())
            continue;

         if (MatchSchedule(task->getSchedule(), &currLocal, now))
         {
            nxlog_debug_tag(DEBUG_TAG, 5, _T("RecurrentScheduler: starting scheduled task [%u] with handler \"%s\" (schedule \"%s\")"),
                     task->getId(), task->getTaskHandlerId().cstr(), task->getSchedule().cstr());

            SchedulerCallback *callback = s_callbacks.get(task->getTaskHandlerId());
            if (callback == NULL)
            {
               nxlog_debug_tag(DEBUG_TAG, 3, _T("RecurrentScheduler: task handler \"%s\" not registered"), task->getTaskHandlerId().cstr());
               callback = &s_missingTaskHandler;
            }

            task->startExecution(callback);
         }
      }
      s_cronScheduleLock.unlock();
      WatchdogStartSleep(watchdogId);
   } while(!SleepAndCheckForShutdown(60)); //sleep 1 minute
   nxlog_debug_tag(DEBUG_TAG, 3, _T("Recurrent scheduler stopped"));
   return THREAD_OK;
}

/**
 * Scheduler thread handles
 */
static THREAD s_oneTimeEventThread = INVALID_THREAD_HANDLE;
static THREAD s_cronSchedulerThread = INVALID_THREAD_HANDLE;

/**
 * Initialize task scheduler - read all schedules form database and start threads for one time and cron schedules
 */
void InitializeTaskScheduler()
{
   g_schedulerThreadPool = ThreadPoolCreate(_T("SCHEDULER"),
            ConfigReadInt(_T("ThreadPool.Scheduler.BaseSize"), 1),
            ConfigReadInt(_T("ThreadPool.Scheduler.MaxSize"), 64));

   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   DB_RESULT hResult = DBSelect(hdb, _T("SELECT id,taskId,schedule,params,execution_time,last_execution_time,flags,owner,object_id,comments,task_key FROM scheduled_tasks"));
   if (hResult != NULL)
   {
      int count = DBGetNumRows(hResult);
      for(int i = 0; i < count; i++)
      {
         ScheduledTask *task = new ScheduledTask(hResult, i);
         if (!_tcscmp(task->getSchedule(), _T("")))
         {
            nxlog_debug_tag(DEBUG_TAG, 7, _T("InitializeTaskScheduler: added one time task [%u] at ") INT64_FMT,
                     task->getId(), static_cast<int64_t>(task->getScheduledExecutionTime()));
            s_oneTimeSchedules.add(task);
         }
         else
         {
            nxlog_debug_tag(DEBUG_TAG, 7, _T("InitializeTaskScheduler: added recurrent task %u at %s"),
                     task->getId(), task->getSchedule().cstr());
            s_cronSchedules.add(task);
         }
      }
      DBFreeResult(hResult);
   }
   DBConnectionPoolReleaseConnection(hdb);
   s_oneTimeSchedules.sort(ScheduledTaskComparator);

   s_oneTimeEventThread = ThreadCreateEx(AdHocScheduler, 0, NULL);
   s_cronSchedulerThread = ThreadCreateEx(RecurrentScheduler, 0, NULL);
}

/**
 * Stop all scheduler threads and free all memory
 */
void ShutdownTaskScheduler()
{
   if (g_schedulerThreadPool == NULL)
      return;

   s_wakeupCondition.set();
   ThreadJoin(s_oneTimeEventThread);
   ThreadJoin(s_cronSchedulerThread);
   ThreadPoolDestroy(g_schedulerThreadPool);
}
