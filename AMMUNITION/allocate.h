/*
   FILE NAME:   allocate.h

   TITLE:       Allocation package include file

   DESCRIPTION:
       This is header file contains macros and the ANSI C prototype
       definitions for the allocation package  (allocation with fixing
       error) and class for the allocation package (allocation with
       fixing error).

   SPECIAL CONSIDERATION:
         The function which processes allocation error should never
       return control back because after calling the function the
       function `abort' is always called in the debug regime.
         Defining macro `NDEBUG' (e.g. by option `-D' in C/C++ compiler
       command line) before the package macros usage disables to
       fix some internal errors and errors of usage of the package.

*/


#ifndef __ALLOCATE__
#define __ALLOCATE__

#ifdef HAVE_CONFIG_H
#include "cocom-config.h"
#else /* In this case we are oriented to ANSI C */
#ifndef HAVE_ASSERT_H
#define HAVE_ASSERT_H
#endif
#endif /* #ifdef HAVE_CONFIG_H */

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_ASSERT_H
#include <assert.h>
#else
#ifndef assert
#define assert(code) do { if (code == 0) abort ();} while (0)
#endif
#endif

#ifndef __cplusplus

/* This macro is analogous to ANSI C library function `malloc'.  But
   the macro has another way of passing input and output data and has
   automatic reaction on the situation `no memory'.  The macro has not
   side effects. */

#define MALLOC(ptr, size)\
  do\
  {\
    void *_memory;\
    _memory = malloc (size);\
    if (_memory == NULL)\
      {\
        _allocation_error_function ();\
        assert (0 /* FALSE */);\
      }\
    (ptr) = _memory;\
  }\
  while (0)

/* This macro is analogous to ANSI C library function `calloc'.  But
   the macro has another way of passing input and output data and has
   automatic reaction on the situation `no memory'.  The macro has not
   side effects. */

#define CALLOC(ptr, nel, size)\
  do\
  {\
    void *_memory;\
    _memory = calloc ((nel), (size));\
    if (_memory == NULL)\
      {\
        _allocation_error_function ();\
        assert (0 /* FALSE */);\
      }\
    (ptr) = _memory;\
  }\
  while (0)

/* This macro is analogous to ANSI C library function `free'.  But the
   macro can accept NULL pointer value.  In this case the macro does
   nothing.  The macro has not side effects. */

#define FREE(ptr)\
  do\
  {\
    void *_memory = (void *) (ptr);\
    if (_memory != NULL)\
      free (_memory);\
  }\
  while (0)

/* This macro is analogous to ANSI C library function `realloc'.  But
   the macro has another way of passing input and output data and has
   automatic reaction on the situation `no memory'.  The macro has not
   side effects. */

#define REALLOC(new, old, size)\
  do\
  {\
    void *_memory;\
    _memory = realloc ((old), (size));\
    if (_memory == NULL)\
      {\
        _allocation_error_function ();\
        assert (0 /* FALSE */);\
      }\
    (new) = _memory;\
  }\
  while (0)

/* The following function is to be used only by the package macros.
   Remember that she is internal function - all work with her is
   executed through the macros. */

extern void _allocation_error_function (void);

extern void default_allocation_error_function (void);

extern void
  (*change_allocation_error_function (void (*error_function) (void))) (void);



#else /* #ifndef __cplusplus */



class allocate
{
  /* The following function is to be used only by the package
     functions. */
  static void allocation_error_function (void);
  allocate (void) {}
  allocate (const allocate& a) {}

public:

  /* This function is analogous to ANSI C library function `malloc'.
     But the function has automatic reaction on the situation `no
     memory'. */

  static inline void *malloc (size_t size)
    {
      void *memory;

      memory = ::malloc (size);
      if (memory == NULL)
        {
          allocation_error_function ();
          assert (0 /* FALSE */);
        }
      return memory;
    }

  /* This function is analogous to ANSI C library function `calloc'.
     But the function has automatic reaction on the situation `no
     memory'. */

  static inline void *calloc (size_t nmemb, size_t size)
    {
      void *memory;
      
      memory = ::calloc (nmemb, size);
      if (memory == NULL)
        {
          allocation_error_function ();
          assert (0 /* FALSE */);
        }
      return memory;
    }

  /* This function is analogous to ANSI C library function `free'.
     But the function can accept NULL pointer value.  In this case the
     function does nothing. */

  static inline void free (void *ptr)
    {
      if (ptr != NULL)
        ::free (ptr);
    }

  static inline void *realloc (void *old, size_t size)
    {
      void *memory;
      
      memory = ::realloc (old, size);
      if (memory == NULL)
        {
          allocation_error_function ();
          assert (0 /* FALSE */);
        }
      return memory;
    }

  friend void no_warning (void);
  static void default_error_function (void);
  static void (*change_error_function (void (*error_function) (void))) (void);
};


#endif


#endif /* #ifndef __ALLOCATE__ */
