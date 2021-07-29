#ifndef SLI_WIN32_CRITICALSECTION_H
#define SLI_WIN32_CRITICALSECTION_H
// Copyright (C) 2003-2004 Software Leverage, Inc.  All rights reserved. 
// This is an unpublished work. 
#include <windows.h>
class CriticalSection
{
   mutable CRITICAL_SECTION critSect_;

   void LockIt()   const;
   void UnlockIt() const;
public:
   CriticalSection();
   ~CriticalSection();

   class Lock {
   // class that manages a lock on a critical section
      const CriticalSection& cs_;
      bool locked_;
   public:
      Lock(const CriticalSection& cs);
      Lock(const Lock&); // copy
      ~Lock();
      void Unlock(void);
      void Relock(void);
   private:
      // disallow assignment
      Lock& operator=(const Lock&);
   }; // end class Lock
   friend class CriticalSection::Lock;
};


#endif /*SLI_WIN32_CRITICALSECTION_H*/