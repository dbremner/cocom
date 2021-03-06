/*
   Copyright (C) 1997-2012 Vladimir Makarov.

   Written by Vladimir Makarov <vmakarov@users.sourceforge.net>

   This file is part of interpreter of DINO.

   This is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This software is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU CC; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.

*/

#include "d_run.h"
#include "d_runtab.h"

/* The following is true if we are executing sync block.  */
int sync_flag;

/* Current program counter of command being executed.  This value is
   used to execution of commands for interpreter (see comments for
   typedef pc_t). */
pc_t cpc;

/* Pointer to the stack frame of the block in which cpc is being
   placed. */
ER_node_t cstack;

/* Pointer to the stack frame of the uppest block in which environment
   variables are placed. */
ER_node_t uppest_stack;

/* Start of the vars of cstack.  */
ER_node_t cvars;

/* Pointers to the var on the top and below the top of stack cstack.
   Don't use the variables during GC. */
ER_node_t ctop;

/* Pointers to a heap objects.  Garbage collector will change it
   correspondingly too.  If the pointer refers for vector, it can not
   be packed during GC or its size can not be changed. */
vlo_t heap_temp_refs;

/* Temporary variable length objects. */
vlo_t temp_vlobj;
vlo_t temp_vlobj2;

/* Current (active) process. */
ER_node_t cprocess;

/* Variable used to assign unique number to the contexts. */
int_t context_number;

/* Variable used to assign unique number to the processes. */
int_t process_number;

/* The following variable value is the first process blocked by a wait
   stmt and there is no process started after this.  This variable is
   used for searching deadlocks. */
ER_node_t first_process_not_started;



size_t type_size_table [ER_NM__error + 1];
size_t val_displ_table [ER_NM__error + 1];

void
initiate_int_tables (void)
{
  int i;
  ER_node_t v = NULL;

  for (i = 0; i <= ER_NM__error; i++)
    type_size_table [i] = 0;
  type_size_table [ER_NM_nil] = 0;
  type_size_table [ER_NM_hide] = sizeof (hide_t);
  type_size_table [ER_NM_char] = sizeof (char_t);
  type_size_table [ER_NM_int] = sizeof (int_t);
  type_size_table [ER_NM_float] = sizeof (floating_t);
  type_size_table [ER_NM_type] = sizeof (IR_node_mode_t);
  type_size_table [ER_NM_vect] = type_size_table [ER_NM_tab]
    = type_size_table [ER_NM_instance] = type_size_table [ER_NM_process]
    = type_size_table [ER_NM_stack] = sizeof (ER_node_t);
  type_size_table [ER_NM_func] = sizeof (struct _ER_S_func);
  type_size_table [ER_NM_thread] = sizeof (struct _ER_S_thread);
  type_size_table [ER_NM_class] = sizeof (struct _ER_S_class);

  for (i = 0; i <= ER_NM__error; i++)
    val_displ_table [i] = 0;
  val_displ_table [ER_NM_nil] = 0;
  val_displ_table [ER_NM_hide]
    = (char *) &((_ER_hide *) v)->_ER_S_hide - (char *) v;
  val_displ_table [ER_NM_hideblock]
    = (char *) &((_ER_hideblock *) v)->_ER_S_hideblock - (char *) v;
  val_displ_table [ER_NM_char]
    = (char *) &((_ER_char *) v)->_ER_S_char - (char *) v;
  val_displ_table [ER_NM_int]
    = (char *) &((_ER_int *) v)->_ER_S_int - (char *) v;
  val_displ_table [ER_NM_float]
    = (char *) &((_ER_float *) v)->_ER_S_float - (char *) v;
  val_displ_table [ER_NM_type]
    = (char *) &((_ER_type *) v)->_ER_S_type - (char *) v;
  val_displ_table [ER_NM_vect]
    = (char *) &((_ER_vect *) v)->_ER_S_vect - (char *) v;
  val_displ_table [ER_NM_tab]
    = (char *) &((_ER_tab *) v)->_ER_S_tab - (char *) v;
  val_displ_table [ER_NM_instance]
    = (char *) &((_ER_instance *) v)->_ER_S_instance - (char *) v;
  val_displ_table [ER_NM_process]
    = (char *) &((_ER_process *) v)->_ER_S_process - (char *) v;
  val_displ_table [ER_NM_func]
    = (char *) &((_ER_func *) v)->_ER_S_func - (char *) v;
  val_displ_table [ER_NM_thread]
    = (char *) &((_ER_thread *) v)->_ER_S_thread - (char *) v;
  val_displ_table [ER_NM_class]
    = (char *) &((_ER_class *) v)->_ER_S_class - (char *) v;
  val_displ_table [ER_NM_stack]
    = (char *) &((_ER_stack *) v)->_ER_S_stack - (char *) v;
}



/* See definitions of the corresponding nodes. */
#if INLINE
__inline__
#endif
size_t
type_size (ER_node_mode_t type)
{
  size_t res = type_size_table [type];
  return res;
}

/* See definitions of the corresponding nodes. */
#if INLINE
__inline__
#endif
size_t
val_displ (ER_node_t var)
{
  size_t res = val_displ_table [ER_NODE_MODE (var)];
  return res;
}



#if !defined(NDEBUG)

size_t
_alloc_size (size_t s)
{
  s = (s + MAX_ALIGNMENT - 1) / MAX_ALIGNMENT * MAX_ALIGNMENT;
  return s;
}

char *
_hideblock_start (ER_node_t hideblock)
{
  char *res;

  assert (ER_NODE_MODE (hideblock) == ER_NM_heap_hideblock);
  res = (char *) hideblock + ALLOC_SIZE (sizeof (_ER_heap_hideblock));
  return res;
}

char *
_pack_els (ER_node_t vect)
{
  char *res;
  
  assert (ER_NODE_MODE (vect) == ER_NM_heap_pack_vect);
  res = ((char *) vect + ALLOC_SIZE (sizeof (_ER_heap_pack_vect))
	 + ER_disp (vect));
  return res;
}

ER_node_t
_unpack_els (ER_node_t vect)
{
  ER_node_t res;
  
  assert (ER_NODE_MODE (vect) == ER_NM_heap_unpack_vect);
  res = (ER_node_t) ((char *) vect
		     + ALLOC_SIZE (sizeof (_ER_heap_unpack_vect))
		     + ER_disp (vect));
  return res;
}

ER_node_t
_tab_els (ER_node_t tab)
{
  ER_node_t res;

  assert (ER_NODE_MODE (tab) == ER_NM_heap_tab);
  res = (ER_node_t) ((char *) tab + ALLOC_SIZE (sizeof (_ER_heap_tab)));
  return res;
}

ER_node_t
_instance_vars (ER_node_t instance)
{
  ER_node_t res;
  
  assert (ER_NODE_MODE (instance) == ER_NM_heap_instance);
  res = (ER_node_t) ((char *) instance
		     + ALLOC_SIZE (sizeof (_ER_heap_instance)));
  return res;
}

ER_node_t
_stack_vars (ER_node_t stack)
{
  ER_node_t res;
  
  assert (ER_NODE_MODE (stack) == ER_NM_heap_stack);
  res = (ER_node_t) ((char *) stack
		     + ALLOC_SIZE (sizeof (_ER_heap_stack)));
  return res;
}

stack_ptr_t
_stacks_table (ER_node_t process)
{
  stack_ptr_t res;
  
  assert (ER_NODE_MODE (process) == ER_NM_heap_process);
  res = (stack_ptr_t) ((char *) process
		       + ALLOC_SIZE (sizeof (_ER_heap_process)));
  return res;
}

ER_node_t
_indexed_val (ER_node_t first_var, int index)
{
  ER_node_t res;
  
  res = (ER_node_t) ((char *) first_var + index * (int) sizeof (val_t));
  return res;
}

ER_node_t
_indexed_entry_key (ER_node_t first_entry, int index)
{
  ER_node_t res;
  
  res = (ER_node_t) ((char *) first_entry + 2 * index * (int) sizeof (val_t));
  return res;
}

ER_node_t
_indexed_entry_val (ER_node_t first_entry, int index)
{
  ER_node_t res;
  
  res = (ER_node_t) ((char *) first_entry
		     + (2 * index + 1) * (int) sizeof (val_t));
  return res;
}

#endif



#if INLINE
__inline__
#endif
size_t
instance_size (IR_node_t class)
{
  assert (class != NULL && IR_NODE_MODE (class) == IR_NM_class);
  return (ALLOC_SIZE (sizeof (_ER_heap_instance))
	  + sizeof (val_t) * IR_vars_number (IR_next_stmt (class)));
}

int
eq_val (val_t *val1_ptr, val_t *val2_ptr, size_t number)
{
  size_t i, displ;

  for (i = 0; i < number; i++)
    if (ER_NODE_MODE ((ER_node_t) &val1_ptr [i])
	!= ER_NODE_MODE ((ER_node_t) &val2_ptr [i]))
      return FALSE;
    else
      {
	displ = val_displ ((ER_node_t) &val1_ptr [i]);
	if (memcmp ((char *) &val1_ptr [i] + displ,
		    (char *) &val2_ptr [i] + displ,
		    type_size (ER_NODE_MODE ((ER_node_t) &val1_ptr [i]))) != 0)
	  return FALSE;
      }
  return TRUE;
}

/* The func returns size (in bytes) of the stack of the block node given
   as BLOCK_NODE_PTR. */
#if INLINE
__inline__
#endif
static size_t
block_stack_size (IR_node_t block_node_ptr)
{
  assert (block_node_ptr != NULL
          && IR_NODE_MODE (block_node_ptr) == IR_NM_block);
  return ((real_block_vars_number (block_node_ptr)
	   + IR_temporary_vars_number (block_node_ptr)) * sizeof (val_t)
	  + ALLOC_SIZE (sizeof (_ER_heap_stack)));
}



/* This page contains definitions and funcs needed for work with heap. */

/* The heap objects are placed on one side of the heap chunks. New
   chunk is created when there is no memory in GC.

                 heap growth
              ----->     <---- 
         __________________________       __________________________   
	|        |         |       |     |        |          |      |  
	| objects|         |stacks |     | objects|          |stacks|  ...
	 --------------------------       --------------------------   
	          ^	    ^	           
		  |         |
	chunk_free         chunk_stack_top */

struct heap_chunk
{
  /* First and first after the last byte of the heap chunk. */
  char *chunk_start, *chunk_bound;
  char *chunk_free, *chunk_stack_top;
};

/* Array of heap descriptors. */

static vlo_t heap_chunks;

static struct heap_chunk *curr_heap_chunk;

/* The following VLO contains registered external variable
   addresses. */
static vlo_t external_vars;

/* Number of identifier `destroy'. */
static int destroy_ident_number;

/* The number of made GC. */
unsigned int gc_number;
/* Average percent of free memory after all GCs. */
int free_gc_memory_percent;
/* Current size of all heap. */
size_t heap_size;
/* Free heap memory */
size_t free_heap_memory;
/* Number of heap_chunks */
int heap_chunks_number;
/* TRUE if we are making GC. */
int in_gc_p;
#ifndef NO_CONTAINER_CACHE
/* All container cache entries are valid when their ticks are equal to
   the following variable value. */
int current_cached_container_tick;
#endif
#ifndef NO_PROFILE
#if HAVE_SETITIMER
/* Number of interrupts in GC. */
integer_t gc_interrupts_number;
#else
/* Time in GC. */
ticker_t gc_ticker;
#endif
#endif

/* EXECUTED_STMTS_COUNT when we recognized that we need GC.  If it is
   possitive we don't need GC.  */
int GC_executed_stmts_count;

/* Each heap object has unique number for hashing purpose. */
static size_t unique_number;

static void
new_heap_chunk (size_t size)
{
  char *str;

  VLO_EXPAND (heap_chunks, sizeof (struct heap_chunk));
  curr_heap_chunk
    = &((struct heap_chunk *) VLO_BEGIN (heap_chunks))
      [VLO_LENGTH (heap_chunks) / sizeof (struct heap_chunk) - 1];
  if (size + free_heap_memory < (heap_size - free_heap_memory) / 3)
    size = (heap_size - 4 * free_heap_memory) / 3;
  size = ALLOC_SIZE (size);
  CALLOC (str, 1, size);
  curr_heap_chunk->chunk_start = curr_heap_chunk->chunk_free = str;
  curr_heap_chunk->chunk_stack_top = curr_heap_chunk->chunk_bound = str + size;
  heap_size += size;
  free_heap_memory += size;
  heap_chunks_number++;
}

static void
del_heap_chunks (int number_saved_chunks)
{
  int i;

  for (i = number_saved_chunks;
       i < VLO_LENGTH (heap_chunks) / sizeof (struct heap_chunk);
       i++)
    {
      curr_heap_chunk = &((struct heap_chunk *) VLO_BEGIN (heap_chunks)) [i];
      FREE (curr_heap_chunk->chunk_start);
      heap_chunks_number--;
      free_heap_memory
	-= curr_heap_chunk->chunk_bound - curr_heap_chunk->chunk_start;
      heap_size
	-= curr_heap_chunk->chunk_bound - curr_heap_chunk->chunk_start;
    }
  VLO_SHORTEN (heap_chunks,
	       VLO_LENGTH (heap_chunks)
	       - number_saved_chunks * sizeof (struct heap_chunk));
  curr_heap_chunk = (&((struct heap_chunk *) VLO_BEGIN (heap_chunks))
		     [number_saved_chunks - 1]);
}

/* The func create the heap with SIZE zero bytes and initiate all
   members of the heap descriptor.  The func also initiate cstack.
   The func is to be called only once before any heap usage. */
void
initiate_heap ()
{
  VLO_CREATE (heap_chunks, 0);
  VLO_CREATE (external_vars, 0);
  /* 0 is reserved for NULL. */
  unique_number = 1;
  heap_size = 0;
  heap_chunks_number = 0;
  free_heap_memory = 0;
  new_heap_chunk (heap_chunk_size);
  cstack = NULL;
  uppest_stack = NULL;
  VLO_CREATE (temp_vlobj, 256);
  VLO_CREATE (temp_vlobj2, 256);
  gc_number = 0;
  GC_executed_stmts_count = 1;
  free_gc_memory_percent = 0;
  context_number = 0;
  in_gc_p = FALSE;
  destroy_ident_number = IR_block_decl_ident_number (destroy_unique_ident);
  assert (destroy_ident_number >= 0);
#ifndef NO_CONTAINER_CACHE
  current_cached_container_tick = 0;
#endif
#ifndef NO_PROFILE
#if HAVE_SETITIMER
  gc_interrupts_number = 0;
#else
  if (profile_flag)
    {
      gc_ticker = create_ticker ();
      ticker_off (&gc_ticker);
    }
#endif
#endif
}

/* Some forward declarations. */
static void clean_heap_object_process_flag (void);
static int mark_instances_need_destroying (int mark_dependent_p);
static void destroy_instances (void);

/* Numbers of the last and before the last temporary variables.  */
int tvar_num1, tvar_num2;

/* Set up tvar_num1 and tvar_num2 for block BLOCK_NODE_PTR.  */
#if INLINE && !defined (SMALL_CODE)
__inline__
#endif
static void
set_tvars (IR_node_t block_node_ptr)
{
  tvar_num1 = (real_block_vars_number (block_node_ptr)
	       + IR_temporary_vars_number (block_node_ptr) - 1);
  tvar_num2 = tvar_num1 - 1;
}

/* Just call destroy functions. */
void
final_call_destroy_functions (void)
{
  IR_node_t block_node_ptr;

  if (uppest_stack == NULL)
    /* Heap is not initialized.  */
    return;
  assert (curr_heap_chunk->chunk_start > (char *) uppest_stack
	  || (char *) uppest_stack >= curr_heap_chunk->chunk_bound
	  || (char *) uppest_stack < curr_heap_chunk->chunk_free
	  || (char *) uppest_stack >= curr_heap_chunk->chunk_stack_top);
  clean_heap_object_process_flag ();
  if (mark_instances_need_destroying (FALSE))
    {
      assert (cstack != NULL);
      if (cstack == uppest_stack)
	{
	  block_node_ptr = ER_block_node (cstack);
	  cvars = ER_stack_vars (cstack);
	  ctop = (ER_node_t) ((char *) cvars
			      + real_block_vars_number (block_node_ptr)
			      * sizeof (val_t) - sizeof (val_t));
	  set_tvars (block_node_ptr);
	  cpc = IR_next_pc (ER_call_pc (cstack));
	  if (cprocess != NULL)
	    ER_set_saved_cstack (cprocess, cstack);
	}
      destroy_instances ();
    }
}

void
finish_heap (void)
{
  for (curr_heap_chunk = VLO_BEGIN (heap_chunks);
       (char *) curr_heap_chunk <= (char *) VLO_END (heap_chunks);
       curr_heap_chunk++)
    FREE (curr_heap_chunk->chunk_start);
  VLO_DELETE (heap_chunks);
  VLO_DELETE (external_vars);
  VLO_DELETE (temp_vlobj2);
  VLO_DELETE (temp_vlobj);
}

/* The following func is used for allocation of any heap object
   (except for the stack frame) with SIZE (!=0).  The func returns
   pointer to the allocated heap memory.  If there is no heap memory
   than fatal error is fixed.  The allocated memory is zeroed (see
   initiate_heap and compact_heap). */
void *
heap_allocate (size_t size, int stack_p)
{
  void *result;

  assert (size > 0);
  size = ALLOC_SIZE (size);
  if (curr_heap_chunk->chunk_stack_top - curr_heap_chunk->chunk_free < size)
    {
      /* We need GC.  Flag this.  */
      assert (executed_stmts_count <= 0);
      GC_executed_stmts_count = executed_stmts_count;
      executed_stmts_count = 0;
      if (curr_heap_chunk->chunk_stack_top - curr_heap_chunk->chunk_free
	  < size)
	new_heap_chunk (size > heap_chunk_size ? size : heap_chunk_size);
    }
  if (stack_p)
    {
      curr_heap_chunk->chunk_stack_top -= size;
      result = curr_heap_chunk->chunk_stack_top;
    }
  else
    {
      result = curr_heap_chunk->chunk_free;
      curr_heap_chunk->chunk_free += size;
    }
  assert (curr_heap_chunk->chunk_free <= curr_heap_chunk->chunk_stack_top);
  free_heap_memory -= size;
  ER_SET_MODE ((ER_node_t) result, ER_NM_heap_instance);
  ER_set_unique_number ((ER_node_t) result, unique_number);
  unique_number++;
  return result;
}

#if INLINE
__inline__
#endif
static size_t
heap_object_size (ER_node_t obj)
{
  size_t size = 1;

  switch (ER_NODE_MODE (obj))
    {
    case ER_NM_heap_pack_vect:
    case ER_NM_heap_unpack_vect:
    case ER_NM_heap_tab:
    case ER_NM_heap_redir:
      size = ER_allocated_length (obj);
      break;
    case ER_NM_heap_instance:
      size = instance_size (ER_instance_class (obj));
      break;
    case ER_NM_heap_stack:
      size = block_stack_size (ER_block_node (obj));
      break;
    case ER_NM_heap_process:
      size = ALLOC_SIZE (sizeof (_ER_heap_process));
      break;
    case ER_NM_heap_hideblock:
      size = (ALLOC_SIZE (sizeof (_ER_heap_hideblock))
	      + ER_hideblock_length (obj));
      break;
    default:
      assert (FALSE);
    }
  return ALLOC_SIZE (size);
}

#if INLINE
__inline__
#endif
static void
try_heap_stack_free (void *from, size_t size)
{
  if (curr_heap_chunk->chunk_stack_top == (char *) from)
    curr_heap_chunk->chunk_stack_top += size;
}

#if INLINE
__inline__
#endif
static int
instance_with_destroy (ER_node_t obj)
{
  IR_node_t decl;

  if (ER_NODE_MODE (obj) != ER_NM_heap_instance)
    return FALSE;
  decl = LV_BLOCK_IDENT_DECL (IR_block_number (ER_block_node (obj)),
			      destroy_ident_number);
  if (decl == NULL)
    return FALSE;
  return IR_IS_OF_TYPE (decl, IR_NM_func);
}


static size_t
tailored_heap_object_size (ER_node_t obj)
{
  size_t size, el_size, head_size, all_els_size, optimal_size;
  ER_node_mode_t node_mode = ER_NODE_MODE (obj);
  
  if (node_mode == ER_NM_heap_pack_vect || node_mode == ER_NM_heap_unpack_vect)
    {
      size = ER_allocated_length (obj);
      el_size = (node_mode == ER_NM_heap_pack_vect
		 ? type_size (ER_pack_vect_el_type (obj)) : sizeof (val_t));
      all_els_size = ER_els_number (obj) * el_size;
      head_size = ALLOC_SIZE (ER_node_size [node_mode]);
      optimal_size = (head_size + OPTIMAL_ELS_SIZE (all_els_size));
      if (optimal_size < head_size + all_els_size + ER_disp (obj))
	optimal_size = head_size + all_els_size + ER_disp (obj);
      if (size > optimal_size)
	size = optimal_size;
    }
  else
    size = heap_object_size (obj);
  return ALLOC_SIZE (size);
}

#if INLINE
__inline__
#endif
static ER_node_t 
next_heap_object (ER_node_t obj)
{
  assert (ER_it_was_processed (obj) == 0 || ER_it_was_processed (obj) == 1);
  return (ER_node_t) ((char *) obj + heap_object_size (obj));
}

static void
clean_heap_object_process_flag (void)
{
  ER_node_t curr_obj;
  struct heap_chunk *curr_descr;

  for (curr_descr = VLO_BEGIN (heap_chunks);
       (char *) curr_descr <= (char *) VLO_END (heap_chunks);
       curr_descr++)
    {
      for (curr_obj = (ER_node_t) curr_descr->chunk_start;
	   (char *) curr_obj < curr_descr->chunk_free;
	   curr_obj = next_heap_object (curr_obj))
	ER_set_it_was_processed (curr_obj, FALSE);
      for (curr_obj = (ER_node_t) curr_descr->chunk_stack_top;
	   (char *) curr_obj < curr_descr->chunk_bound;
	   curr_obj = next_heap_object (curr_obj))
	ER_set_it_was_processed (curr_obj, FALSE);
    }
}

static void traverse_used_heap_object (ER_node_t start_obj);

static void
traverse_used_var (ER_node_t var)
{
  switch (ER_NODE_MODE (var))
    {
    case ER_NM_nil:
    case ER_NM_hide:
    case ER_NM_char:
    case ER_NM_int:
    case ER_NM_float:
    case ER_NM_type:
      return;
    case ER_NM_hideblock:
      traverse_used_heap_object (ER_hideblock (var));
      return;
    case ER_NM_vect:
      traverse_used_heap_object (ER_vect (var));
      return;
    case ER_NM_tab:
      traverse_used_heap_object (ER_tab (var));
      return;
    case ER_NM_instance:
      traverse_used_heap_object (ER_instance (var));
      return;
    case ER_NM_func:
      traverse_used_heap_object (ER_func_context (var));
      return;
    case ER_NM_thread:
      traverse_used_heap_object (ER_thread_context (var));
      return;
    case ER_NM_process:
      traverse_used_heap_object (ER_process (var));
      return;
    case ER_NM_class:
      traverse_used_heap_object (ER_class_context (var));
      return;
    case ER_NM_stack:
      traverse_used_heap_object (ER_stack (var));
      return;
    case ER_NM_external_var_ref:
      return;
    default:
      assert (FALSE);
    }
}

static void
traverse_used_heap_object (ER_node_t obj)
{
  size_t i;

  if (obj == NULL || ER_it_was_processed (obj))
    return;
  ER_set_it_was_processed (obj, TRUE);
  switch (ER_NODE_MODE (obj))
    {
    case ER_NM_heap_hideblock:
      return;
    case ER_NM_heap_pack_vect:
      {
	ER_node_mode_t el_type = ER_pack_vect_el_type (obj);
	size_t el_size;
	
	if (el_type == ER_NM_nil || el_type == ER_NM_hide
	    || el_type == ER_NM_char || el_type == ER_NM_int
	    || el_type == ER_NM_float || el_type == ER_NM_type)
	  return;
	el_size = type_size (el_type);
	if (el_type == ER_NM_hideblock || el_type == ER_NM_vect
	    || el_type == ER_NM_tab || el_type == ER_NM_instance
	    || el_type == ER_NM_process || el_type == ER_NM_stack)
	  for (i = 0; i < ER_els_number (obj); i++)
	    traverse_used_heap_object
	      (*(ER_node_t *) (ER_pack_els (obj) + i * el_size));
	else if (el_type == ER_NM_func)
	  for (i = 0; i < ER_els_number (obj); i++)
	    traverse_used_heap_object
	      (((struct _ER_S_func *) (ER_pack_els (obj) + i * el_size))
               ->func_context);
	else if (el_type == ER_NM_thread)
	  for (i = 0; i < ER_els_number (obj); i++)
	    traverse_used_heap_object
	      (((struct _ER_S_thread *) (ER_pack_els (obj) + i * el_size))
               ->thread_context);
	else if (el_type == ER_NM_class)
	  for (i = 0; i < ER_els_number (obj); i++)
	    traverse_used_heap_object
	      (((struct _ER_S_class *) (ER_pack_els (obj) + i * el_size))
               ->class_context);
	else
	  assert (FALSE);
	return;
      }
    case ER_NM_heap_unpack_vect:
      for (i = 0; i < ER_els_number (obj); i++)
	traverse_used_var (IVAL (ER_unpack_els (obj), i));
      return;
    case ER_NM_heap_tab:
      {
	ER_node_mode_t mode;

	ER_set_last_key_index (obj, -1);
	for (i = 0; i < ER_entries_number (obj); i++)
	  {
	    mode = ER_NODE_MODE (INDEXED_ENTRY_KEY (ER_tab_els (obj), i));
	    if (mode == ER_NM_empty_entry || mode == ER_NM_deleted_entry)
	      continue;
	    traverse_used_var (INDEXED_ENTRY_KEY (ER_tab_els (obj), i));
	    traverse_used_var (INDEXED_ENTRY_VAL (ER_tab_els (obj), i));
	  }
      }
      return;
    case ER_NM_heap_redir:
      traverse_used_heap_object (ER_redir (obj));
      return;
    case ER_NM_heap_instance:
      {
	int vars_number;
	
	traverse_used_heap_object (ER_context (obj));
	vars_number = IR_vars_number (IR_next_stmt (ER_instance_class (obj)));
	for (i = 0; i < vars_number; i++)
	  traverse_used_var (IVAL (ER_instance_vars (obj), i));
	return;
      }
    case ER_NM_heap_stack:
      {
	ER_node_t var;
	
	traverse_used_heap_object (ER_prev_stack (obj));
	traverse_used_heap_object (ER_context (obj));
	for (var = ER_stack_vars (obj);
	     (char *) var <= ER_ctop (obj);
	     var = IVAL (var, 1))
	  traverse_used_var (var);
	return;
      }
    case ER_NM_heap_process:
      traverse_used_heap_object (ER_context (obj));
      traverse_used_heap_object (ER_father (obj));
      traverse_used_heap_object (ER_prev (obj));
      traverse_used_heap_object (ER_next (obj));
      traverse_used_heap_object (ER_saved_cstack (obj));
      return;
    default:
      assert (FALSE);
    }
}

/* Return true if there are objects to be destroyed.  Mark objects
   achieved from instance to be destroyed if MARK_DEPENDENT_P.  */
static int
mark_instances_need_destroying (int mark_dependent_p)
{
  ER_node_t curr_obj;
  struct heap_chunk *curr_descr;
  int result = FALSE;

  for (curr_descr = VLO_BEGIN (heap_chunks);
       (char *) curr_descr <= (char *) VLO_END (heap_chunks);
       curr_descr++)
    {
      for (curr_obj = (ER_node_t) curr_descr->chunk_start;
	   (char *) curr_obj < curr_descr->chunk_free;
	   curr_obj = next_heap_object (curr_obj))
	if (!ER_it_was_processed (curr_obj) && instance_with_destroy (curr_obj)
	    && ER_state (curr_obj) != IS_destroyed)
	  {
	    ER_set_state (curr_obj, IS_to_be_destroyed);
	    result = TRUE;
	  }
    }
  if (mark_dependent_p && result)
    for (curr_descr = VLO_BEGIN (heap_chunks);
	 (char *) curr_descr <= (char *) VLO_END (heap_chunks);
	 curr_descr++)
      {
	for (curr_obj = (ER_node_t) curr_descr->chunk_start;
	     (char *) curr_obj < curr_descr->chunk_free;
	     curr_obj = next_heap_object (curr_obj))
	  if (ER_NODE_MODE (curr_obj) == ER_NM_heap_instance
	      && ER_state (curr_obj) == IS_to_be_destroyed)
	    traverse_used_heap_object (curr_obj);
      }
  return result;
}

static int
mark_used_heap_objects (void)
{
  int i;

  clean_heap_object_process_flag ();
  traverse_used_heap_object (cstack);
  traverse_used_heap_object (uppest_stack);
  for (i = 0; i < VLO_LENGTH (external_vars) / sizeof (void *); i++)
    traverse_used_var ((ER_node_t) ((void **) VLO_BEGIN (external_vars)) [i]);
  /* Current stack table is traversed with cprocess. */
  traverse_used_heap_object (cprocess);
  traverse_used_heap_object (first_process_not_started);
  /* Traverse all instances which needs to be destroyed. */
  return mark_instances_need_destroying (TRUE);
}

#if INLINE
__inline__
#endif
static char *
define_new_heap_object (ER_node_t obj, struct heap_chunk **descr, char *place)
{
  size_t size;

  if (ER_NODE_MODE (obj) == ER_NM_heap_redir)
    {
      /* Redirections do not survive GC. */
      ER_set_it_was_processed (obj, FALSE);
      ER_set_new_place (obj, NULL);
    }
  else if (ER_it_was_processed (obj))
    {
      if (ER_NODE_MODE (obj) == ER_NM_heap_unpack_vect)
	pack_vector_if_possible (obj);
      size = tailored_heap_object_size (obj);
      if (place + size > (*descr)->chunk_bound)
	{
	  (*descr)++;
	  place = (*descr)->chunk_start;
	}
      ER_set_new_place (obj, place);
      /* Packing will be only in compact_heap */
      place += size;
      assert (place <= (*descr)->chunk_bound);
    }
  else
    ER_set_new_place (obj, NULL);
  return place;
}

static int
define_new_heap_object_places (void)
{
  ER_node_t curr_obj;
  struct heap_chunk *curr_descr;
  struct heap_chunk *curr_place_descr;
  char *new_place;
  int result;

  result = mark_used_heap_objects ();
  curr_place_descr = VLO_BEGIN (heap_chunks);
  new_place = curr_place_descr->chunk_start;
  for (curr_descr = VLO_BEGIN (heap_chunks);
       (char *) curr_descr <= (char *) VLO_END (heap_chunks);
       curr_descr++)
    {
      for (curr_obj = (ER_node_t) curr_descr->chunk_start;
	   (char *) curr_obj < curr_descr->chunk_free;
	   curr_obj = next_heap_object (curr_obj))
	new_place
	  = define_new_heap_object (curr_obj, &curr_place_descr, new_place);
      for (curr_obj = (ER_node_t) curr_descr->chunk_stack_top;
	   (char *) curr_obj < curr_descr->chunk_bound;
	   curr_obj = next_heap_object (curr_obj))
	new_place
	  = define_new_heap_object (curr_obj, &curr_place_descr, new_place);
    }
  return result;
}

#define CHANGE_REF(ref)\
 do\
   if ((ref) != NULL)\
     {\
       (ref) = (ER_node_t) ER_new_place (ref);\
     }\
 while (0)

#define CHANGE_VECT_TAB_REF(ref)\
 do\
   if ((ref) != NULL)\
     {\
       GO_THROUGH_REDIR (ref);\
       (ref) = (ER_node_t) ER_new_place (ref);\
     }\
 while (0)

/* The function changes one value with given VAL_ADDR of packed
   vector. The type of the value is given in MODE.  Don't use SPRUT
   access macros here.  They can check the reference which will refer
   to correct object only after the heap compaction .*/
#if INLINE
__inline__
#endif
static void
change_val (ER_node_mode_t mode, ER_node_t *val_addr)
{
  switch (mode)
    {
    case ER_NM_nil:
    case ER_NM_hide:
    case ER_NM_char:
    case ER_NM_int:
    case ER_NM_float:
    case ER_NM_type:
      return;
    case ER_NM_vect:
    case ER_NM_tab:
      CHANGE_VECT_TAB_REF (*val_addr);
      return;
    case ER_NM_hideblock:
    case ER_NM_instance:
    case ER_NM_process:
    case ER_NM_stack:
    case ER_NM_func:
    case ER_NM_thread:
    case ER_NM_class:
      CHANGE_REF (*val_addr);
      return;
    default:
      assert (FALSE);
    }
}

/* The function changes variable VAR value if it refers to a heap
   object.  Don't use SPRUT access macros here.  They can check the
   reference which will refer to correct object only after the heap
   compaction .*/
static void
change_var (ER_node_t var)
{
  switch (ER_NODE_MODE (var))
    {
    case ER_NM_nil:
    case ER_NM_hide:
    case ER_NM_char:
    case ER_NM_int:
    case ER_NM_float:
    case ER_NM_type:
      return;
    case ER_NM_vect:
      CHANGE_VECT_TAB_REF (((_ER_vect *) var)->_ER_S_vect.vect);
      return;
    case ER_NM_tab:
      CHANGE_VECT_TAB_REF (((_ER_tab *) var)->_ER_S_tab.tab);
      return;
    case ER_NM_hideblock:
      CHANGE_REF (((_ER_hideblock *) var)->_ER_S_hideblock.hideblock);
      return;
    case ER_NM_instance:
      CHANGE_REF (((_ER_instance *) var)->_ER_S_instance.instance);
      return;
    case ER_NM_func:
      CHANGE_REF (((_ER_func *) var)->_ER_S_func.func_context);
      return;
    case ER_NM_thread:
      CHANGE_REF (((_ER_thread *) var)->_ER_S_thread.thread_context);
      return;
    case ER_NM_process:
      CHANGE_REF (((_ER_process *) var)->_ER_S_process.process);
      return;
    case ER_NM_class:
      CHANGE_REF (((_ER_class *) var)->_ER_S_class.class_context);
      return;
    case ER_NM_stack:
      CHANGE_REF (((_ER_stack *) var)->_ER_S_stack.stack);
      return;
    case ER_NM_external_var_ref:
      return;
    default:
      assert (FALSE);
    }
}

#if INLINE
__inline__
#endif
static void
change_obj_refs (ER_node_t obj)
{
  size_t i;

  if (ER_it_was_processed (obj))
    switch (ER_NODE_MODE (obj))
      {
      case ER_NM_heap_pack_vect:
	{
	  ER_node_mode_t el_type;
	  size_t el_size;
	  
	  el_type = ER_pack_vect_el_type (obj);
	  if (el_type == ER_NM_nil || el_type == ER_NM_hide
	      || el_type == ER_NM_char || el_type == ER_NM_int
	      || el_type == ER_NM_float || el_type == ER_NM_type)
	    break;
	  el_size = type_size (el_type);
	  if (el_type == ER_NM_hideblock || el_type == ER_NM_vect
	      || el_type == ER_NM_tab || el_type == ER_NM_instance
	      || el_type == ER_NM_process || el_type == ER_NM_stack)
	    for (i = 0; i < ER_els_number (obj); i++)
	      change_val (el_type,
			  (ER_node_t *) (ER_pack_els (obj) + i * el_size));
	  else if (el_type == ER_NM_func)
	    for (i = 0; i < ER_els_number (obj); i++)
	      change_val (el_type,
			  &((struct _ER_S_func *)
			    (ER_pack_els (obj) + i * el_size))->func_context);
	  else if (el_type == ER_NM_thread)
	    for (i = 0; i < ER_els_number (obj); i++)
	      change_val (el_type,
			  &((struct _ER_S_thread *)
			    (ER_pack_els (obj) + i * el_size))->thread_context);
	  else if (el_type == ER_NM_class)
	    for (i = 0; i < ER_els_number (obj); i++)
	      change_val (el_type,
			  &((struct _ER_S_class *)
			    (ER_pack_els (obj) + i * el_size))->class_context);
	  else
	    assert (FALSE);
	  break;
	}
      case ER_NM_heap_unpack_vect:
	for (i = 0; i < ER_els_number (obj); i++)
	  change_var (IVAL (ER_unpack_els (obj), i));
	break;
      case ER_NM_heap_tab:
	{
	  ER_node_mode_t el_type;
	  
	  for (i = 0; i < ER_entries_number (obj); i++)
	    {
	      el_type = ER_NODE_MODE (INDEXED_ENTRY_KEY (ER_tab_els (obj), i));
	      if (el_type == ER_NM_empty_entry
		  || el_type == ER_NM_deleted_entry)
		continue;
	      change_var (INDEXED_ENTRY_KEY (ER_tab_els (obj), i));
	      change_var (INDEXED_ENTRY_VAL (ER_tab_els (obj), i));
	    }
	  break;
	}
      case ER_NM_heap_instance:
	{
	  int vars_number;
	  
	  CHANGE_REF (((_ER_heap_instance *) obj)
		      ->_ER_S_heap_instance._ER_M_context_heap_obj.context);
	  vars_number = IR_vars_number (IR_next_stmt
					(ER_instance_class (obj)));
	  for (i = 0; i < vars_number; i++)
	    change_var (IVAL (ER_instance_vars (obj), i));
	  break;
	}
      case ER_NM_heap_stack:
	{
	  ER_node_t var;
	  size_t diff;
	  
	  CHANGE_REF (((_ER_heap_stack *) obj)
		      ->_ER_S_heap_stack._ER_M_context_heap_obj.context);
	  CHANGE_REF (((_ER_heap_stack *) obj)->_ER_S_heap_stack.prev_stack);
	  for (var = ER_stack_vars (obj);
	       (char *) var <= ER_ctop (obj);
	       var = IVAL (var, 1))
	    change_var (var);
	  diff = ((char *) ER_ctop (obj) - (char *) ER_stack_vars (obj));
	  ((_ER_heap_stack *) obj)->_ER_S_heap_stack.ctop
	    = (ER_new_place (obj)
	       + ALLOC_SIZE (sizeof (_ER_heap_stack)) + diff);
	  break;
	}
      case ER_NM_heap_process:
	CHANGE_REF (((_ER_heap_process *) obj)
		    ->_ER_S_heap_process._ER_M_context_heap_obj.context);
	CHANGE_REF (((_ER_heap_process *) obj)->_ER_S_heap_process.father);
	CHANGE_REF (((_ER_heap_process *) obj)->_ER_S_heap_process.prev);
	CHANGE_REF (((_ER_heap_process *) obj)->_ER_S_heap_process.next);
	CHANGE_REF (((_ER_heap_process *) obj)
		    ->_ER_S_heap_process.saved_cstack);
	break;
      case ER_NM_heap_hideblock:
	break;
      default:
	/* Redirections do not survive GC. */
	assert (FALSE);
      }
}

/* The function changes all heap references onto references which must
   be after the heap compaction Don't use SPRUT access macros here.
   They can check the reference which will refer to correct object
   only after the heap compaction .*/
static void
change_refs (void)
{
  ER_node_t curr_obj;
  struct heap_chunk *curr_descr;
  size_t i;

  for (curr_descr = VLO_BEGIN (heap_chunks);
       (char *) curr_descr <= (char *) VLO_END (heap_chunks);
       curr_descr++)
    {
      for (curr_obj = (ER_node_t) curr_descr->chunk_start;
	   (char *) curr_obj < curr_descr->chunk_free;
	   curr_obj = next_heap_object (curr_obj))
	change_obj_refs (curr_obj);
      for (curr_obj = (ER_node_t) curr_descr->chunk_stack_top;
	   (char *) curr_obj < curr_descr->chunk_bound;
	   curr_obj = next_heap_object (curr_obj))
	change_obj_refs (curr_obj);
    }
  CHANGE_REF (cstack);
  CHANGE_REF (uppest_stack);
  for (i = 0; i < VLO_LENGTH (external_vars) / sizeof (void *); i++)
    change_var ((ER_node_t) ((void **) VLO_BEGIN (external_vars)) [i]);
  CHANGE_REF (cprocess);
  CHANGE_REF (first_process_not_started);
}

#if INLINE
__inline__
#endif
static char *
move_object (ER_node_t obj, struct heap_chunk **descr,
	     size_t *curr_heap_size, char *place)
{
  size_t tailored_size;

  if (ER_it_was_processed (obj))
    {
      /* Tailor vector size only here although tailoring has been
	 taken into account in place value.  Remeber that temp
	 refs are already set up to new places. */
      tailored_size = tailored_heap_object_size (obj);
      assert (tailored_size <= heap_object_size (obj));
      if (place + tailored_size > (*descr)->chunk_bound)
	{
	  /* Zero e.g. because we compare var as full structs. */
	  if ((*descr)->chunk_free > place)
	    memset (place, 0, (*descr)->chunk_free - place);
	  if ((*descr)->chunk_stack_top > place)
	    memset ((*descr)->chunk_stack_top, 0,
		    (*descr)->chunk_bound - (*descr)->chunk_stack_top);
	  else
	    memset (place, 0, (*descr)->chunk_bound - place);
	  free_heap_memory += (*descr)->chunk_bound - place;
	  (*descr)->chunk_free = place;
	  (*descr)->chunk_stack_top = (*descr)->chunk_bound;
	  (*descr)++;
	  place = (*descr)->chunk_start;
	  *curr_heap_size += ((*descr)->chunk_bound - (*descr)->chunk_start);
	}
      if (ER_NODE_MODE (obj) == ER_NM_heap_pack_vect
	  || ER_NODE_MODE (obj) == ER_NM_heap_unpack_vect)
	ER_set_allocated_length (obj, tailored_size);
      assert (ER_new_place (obj) == place);
      if (place != (char *) obj)
	memmove (place, obj, tailored_size);
      place += tailored_size;
      assert (place <= (*descr)->chunk_bound);
    }
  return place;
}

static void
compact_heap (void)
{
  ER_node_t curr_obj, next_obj;
  struct heap_chunk *curr_descr;
  struct heap_chunk *curr_place_descr;
  size_t curr_heap_size;
  char *new_place;

  /* Compact the heap */
  curr_place_descr = VLO_BEGIN (heap_chunks);
  new_place = curr_place_descr->chunk_start;
  free_heap_memory = 0;
  curr_heap_size = (curr_place_descr->chunk_bound
		    - curr_place_descr->chunk_start);
  for (curr_descr = VLO_BEGIN (heap_chunks);
       (char *) curr_descr <= (char *) VLO_END (heap_chunks);
       curr_descr++)
    {
      for (curr_obj = (ER_node_t) curr_descr->chunk_start;
	   (char *) curr_obj < curr_descr->chunk_free;
	   curr_obj = next_obj)
	{
	  next_obj = next_heap_object (curr_obj);
	  new_place = move_object (curr_obj, &curr_place_descr,
				   &curr_heap_size, new_place);
	}
      for (curr_obj = (ER_node_t) curr_descr->chunk_stack_top;
	   (char *) curr_obj < curr_descr->chunk_bound;
	   curr_obj = next_obj)
	{
	  next_obj = next_heap_object (curr_obj);
	  new_place = move_object (curr_obj, &curr_place_descr,
				   &curr_heap_size, new_place);
	}
    }
  curr_heap_chunk = curr_place_descr;
  assert ((char *) (curr_heap_chunk + 1) <= (char *) VLO_END (heap_chunks)
	  || curr_heap_chunk->chunk_free + (curr_heap_chunk->chunk_bound
					    - curr_heap_chunk->chunk_stack_top)
	  >= new_place);
  /* Zero e.g. because we compare var as full structs: */
  /* Possible situation when e.g. two chunks are compacted in one. */
  if (curr_heap_chunk->chunk_free >= new_place)
    memset (new_place, 0, curr_heap_chunk->chunk_free - new_place);
  if (curr_heap_chunk->chunk_stack_top > new_place)
    memset (curr_heap_chunk->chunk_stack_top, 0,
	    curr_heap_chunk->chunk_bound - curr_heap_chunk->chunk_stack_top);
  else
    memset (new_place, 0, curr_heap_chunk->chunk_bound - new_place);
  curr_heap_chunk->chunk_free = new_place;
  curr_heap_chunk->chunk_stack_top = curr_heap_chunk->chunk_bound;
  free_heap_memory += curr_heap_chunk->chunk_bound - new_place;
  for (;;)
    {
      if (free_heap_memory >= curr_heap_size / 4)
	break;
      curr_heap_chunk++;
      if ((char *) curr_heap_chunk > (char *) VLO_END (heap_chunks))
	{
	  curr_heap_chunk--;
	  break;
	}
      free_heap_memory += (curr_heap_chunk->chunk_bound
			   - curr_heap_chunk->chunk_start);
      curr_heap_size += (curr_heap_chunk->chunk_bound
			 - curr_heap_chunk->chunk_start);
      memset (curr_heap_chunk->chunk_start, 0,
	      curr_heap_chunk->chunk_free - curr_heap_chunk->chunk_start);
      memset (curr_heap_chunk->chunk_stack_top, 0,
	      curr_heap_chunk->chunk_bound - curr_heap_chunk->chunk_stack_top);
      curr_heap_chunk->chunk_free = curr_heap_chunk->chunk_start;
      curr_heap_chunk->chunk_stack_top = curr_heap_chunk->chunk_bound;
    }
  del_heap_chunks (curr_heap_chunk
		   - (struct heap_chunk *) VLO_BEGIN (heap_chunks) + 1);
}

/* Sort the chunks in descending size order. */
static int
compare_chunks (const void *chunk1, const void *chunk2)
{
  const struct heap_chunk *c1 = chunk1;
  const struct heap_chunk *c2 = chunk2;

  if (c1->chunk_bound - c1->chunk_start > c2->chunk_bound - c2->chunk_start)
    return -1;
  else if (c1->chunk_bound - c1->chunk_start
	   < c2->chunk_bound - c2->chunk_start)
    return 1;
  else
    return 0;
}

static void
destroy_instances (void)
{
  IR_node_t decl;
  ER_node_t curr_obj;
  struct heap_chunk *curr_descr;

  for (curr_descr = VLO_BEGIN (heap_chunks);
       (char *) curr_descr <= (char *) VLO_END (heap_chunks);
       curr_descr++)
    {
      for (curr_obj = (ER_node_t) curr_descr->chunk_start;
	   (char *) curr_obj < curr_descr->chunk_free;
	   curr_obj = next_heap_object (curr_obj))
	if (ER_NODE_MODE (curr_obj) == ER_NM_heap_instance
	    && ER_state (curr_obj) == IS_to_be_destroyed)
	  {
	    decl = LV_BLOCK_IDENT_DECL (IR_block_number (ER_block_node (curr_obj)),
					destroy_ident_number);
	    TOP_UP;
	    ER_SET_MODE (ctop, ER_NM_func);
	    ER_set_func_context (ctop, curr_obj);
	    ER_set_func_no (ctop, FUNC_CLASS_NO (decl));
	    /* We mark it before the call to prevent infinite loop if
	       the exception occurs during the call. */
	    ER_set_state (curr_obj, IS_destroyed);
	    call_func_class (0);
	  }
    }
}

/* Redirections never survive GC.  The unpacked vectors may be
   transformed to packed (see comments for variable heap_temp_refs).
   The vector size may be tailored (but not increased) if its size is
   more than optimal one (see commentaries for variable heap_temp_refs
   again). */
void
GC (void)
{
  int flag;

  /* Mark that we don't need GC anymore.  */
  assert (GC_executed_stmts_count <= 0);
  executed_stmts_count = GC_executed_stmts_count;
  GC_executed_stmts_count = 1;
#if !defined (NO_PROFILE) && !HAVE_SETITIMER
  if (profile_flag)
    ticker_on (&gc_ticker);
#endif
  in_gc_p = TRUE;
  ER_set_ctop (cstack, (char *) ctop);
#ifndef NO_CONTAINER_CACHE
  current_cached_container_tick++;
#endif
  qsort (VLO_BEGIN (heap_chunks),
	 VLO_LENGTH (heap_chunks) / sizeof (struct heap_chunk),
	 sizeof (struct heap_chunk), compare_chunks);
  flag = define_new_heap_object_places ();
  change_refs ();
  compact_heap ();
  cvars = ER_stack_vars (cstack);
  ctop = (ER_node_t) ER_ctop (cstack);
  set_tvars (ER_block_node (cstack));
  gc_number++;
  free_gc_memory_percent
    = (free_gc_memory_percent * (gc_number - 1)
       + 100 * free_heap_memory / heap_size) / gc_number;
  in_gc_p = FALSE;
#if !defined (NO_PROFILE) && !HAVE_SETITIMER
  if (profile_flag)
    ticker_on (&gc_ticker);
#endif
  if (flag)
    destroy_instances ();
}



#if ! defined (NO_PROFILE) && HAVE_SETITIMER
integer_t all_interrupts_number;

/* The following function processes interrupt from virtual alarm. */
void
profile_interrupt (void)
{
  IR_node_t block, func_class;
  
  all_interrupts_number++;
  if (in_gc_p)
    {
      gc_interrupts_number++;
      return;
    }
  for (block = ER_block_node (cstack);
       block != NULL && (func_class = IR_func_class_ext (block)) == NULL;
       block = IR_block_scope (block))
    ;
  if (block != NULL && func_class != NULL)
    IR_set_interrupts_number (func_class,
			      IR_interrupts_number (func_class) + 1);
}
#endif

/* The following func allocates heap memory for stack frame (heap
   stack header and vars) of the block given as BLOCK_NODE_PTR.  The
   func initiates all fields of stack and returns pointer to it.  The
   func also sets up value of cstack.  The func sets up mode of all
   permanent stack vars as ER_NM_NIL. Usually, CONTEXT is the same as
   CSTACK.  But in complex cases, the chain `context' contains the
   chain `prev_stack' except for class constructors stacks (the heap
   instance is used in this case). */
void
heap_push (IR_node_t block_node_ptr, ER_node_t context, int offset)
{
  ER_node_t stack;
  ER_node_t curr_var;
  IR_node_t func_class;

  /* Remember about possible GC. */
  stack = (ER_node_t) heap_allocate (block_stack_size (block_node_ptr), TRUE);
#ifndef NO_PROFILE
  if (profile_flag)
    {
      func_class = IR_func_class_ext (block_node_ptr);
      if (func_class != NULL)
	{
#if !HAVE_SETITIMER
	  if (IR_calls_number (func_class) == 0)
	    IR_set_exec_time (func_class, create_ticker ());
	  else
	    ticker_on (&IR_exec_time (func_class));
#endif
	  IR_set_calls_number (func_class, IR_calls_number (func_class) + 1);
	}
    }
#endif
  ER_SET_MODE (stack, ER_NM_heap_stack);
  ER_set_call_pc (stack, cpc);
  if (context != NULL && ER_NODE_MODE (context) != ER_NM_heap_instance)
    {
      func_class = IR_func_class_ext (ER_block_node (context));
      if (func_class != NULL && IR_IS_OF_TYPE (func_class, IR_NM_class))
	/* Remove class constructor from the context. */
	context = ER_context (context);
    }
#ifndef NO_CONTAINER_CACHE
  current_cached_container_tick++;
#endif
  ER_set_context (stack, context);
  ER_set_context_number (stack, context_number);
  context_number++;
  ER_set_prev_stack (stack, cstack);
  if (cstack != NULL)
    ER_set_ctop (cstack, (char *) ctop);
  cstack = stack;
  ER_set_block_node (stack, block_node_ptr);
  cvars = ER_stack_vars (cstack);
  ctop
     = (ER_node_t) ((char *) cvars
                    + real_block_vars_number (block_node_ptr) * sizeof (val_t)
	            - sizeof (val_t));
  set_tvars (block_node_ptr);
  /* Seting up mode of all permanent stack vars as nil. */
  for (curr_var = ER_stack_vars (stack);
       curr_var <= ctop;
       curr_var = IVAL (curr_var, 1))
    ER_SET_MODE (curr_var, ER_NM_nil);
  /* We set them only here because we need to set mode before.
     Remeber about possible field checking. */
  if (cprocess != NULL)
    ER_set_saved_cstack (cprocess, cstack);
}

/* The following func finishes work of the block, modifing cstack and
   cpc (only if the corresponding block is block of func or class).
   No GC during this function. */
void
heap_pop (void)
{
  ER_node_t stack = cstack;
  IR_node_t block_node = ER_block_node (cstack);
  IR_node_t func_class = IR_func_class_ext (block_node);

  if (func_class != NULL)
    {
      cpc = IR_next_pc (ER_call_pc (stack));
#if ! defined (NO_PROFILE) && !HAVE_SETITIMER
      if (profile_flag)
	ticker_off (&IR_exec_time (func_class));
#endif
    }
  ER_set_ctop (cstack, (char *) ctop);
  cstack = ER_prev_stack (cstack);
#ifndef NO_CONTAINER_CACHE
  current_cached_container_tick++;
#endif
  if (cstack == NULL)
    cvars = ctop = NULL;
  else
    {
      cvars = ER_stack_vars (cstack);
      ctop = (ER_node_t) ER_ctop (cstack);
      set_tvars (ER_block_node (cstack));
    }
  if (! IR_extended_life_context_flag (block_node))
    try_heap_stack_free (stack, heap_object_size (stack));
  ER_set_saved_cstack (cprocess, cstack);
}



ER_node_t
create_hideblock (size_t length)
{
  ER_node_t hideblock;
  size_t allocated_length;

  allocated_length = (ALLOC_SIZE (sizeof (_ER_heap_hideblock)) + length);
  hideblock = heap_allocate (allocated_length, FALSE);
  ER_SET_MODE (hideblock, ER_NM_heap_hideblock);
  ER_set_immutable (hideblock, FALSE);
  ER_set_hideblock_length (hideblock, length);
  return hideblock;
}



/* The page contains functions for vectors. */

/* Create mutable empty vector (it is always packed vector). */
ER_node_t
create_empty_vector (void)
{
  ER_node_t vect;

  vect = heap_allocate (ALLOC_SIZE (sizeof (_ER_heap_pack_vect)), FALSE);
  ER_SET_MODE (vect, ER_NM_heap_pack_vect);
  ER_set_immutable (vect, FALSE);
  ER_set_allocated_length (vect, ALLOC_SIZE (sizeof (_ER_heap_pack_vect)));
  ER_set_pack_vect_el_type (vect, ER_NM_nil);
  ER_set_els_number (vect, 0);
  ER_set_disp (vect, 0);
  return vect;
}

/* Create mutable unpacked vector with ELS_NUMBER elements (their
   value is undefined). */
ER_node_t
create_unpack_vector (size_t els_number)
{
  ER_node_t unpack_vect;
  size_t allocated_length;

  allocated_length = (ALLOC_SIZE (sizeof (_ER_heap_unpack_vect))
		      + OPTIMAL_ELS_SIZE (els_number * sizeof (val_t)));
  unpack_vect = heap_allocate (allocated_length, FALSE);
  ER_SET_MODE (unpack_vect, ER_NM_heap_unpack_vect);
  ER_set_immutable (unpack_vect, FALSE);
  ER_set_els_number (unpack_vect, els_number);
  ER_set_disp (unpack_vect, 0);
  ER_set_allocated_length (unpack_vect, allocated_length);
  return unpack_vect;
}

/* Create mutable packed vector with ELS_NUMBER elements of ELTYPE
   (their value is undefined). */
ER_node_t
create_pack_vector (size_t els_number, ER_node_mode_t eltype)
{
  ER_node_t pack_vect;
  size_t allocated_length;

  allocated_length = (ALLOC_SIZE (sizeof (_ER_heap_pack_vect))
		      + OPTIMAL_ELS_SIZE (els_number * type_size (eltype)));
  pack_vect = heap_allocate (allocated_length, FALSE);
  ER_SET_MODE (pack_vect, ER_NM_heap_pack_vect);
  ER_set_pack_vect_el_type (pack_vect, eltype);
  ER_set_immutable (pack_vect, FALSE);
  ER_set_els_number (pack_vect, els_number);
  ER_set_disp (pack_vect, 0);
  ER_set_allocated_length (pack_vect, allocated_length);
  return pack_vect;
}

/* Make vector VECT contains room for ELS_NUMBER.  Return the vector
   (may be with new place). */
ER_node_t
expand_vector (ER_node_t vect, size_t els_number)
{
  size_t disp, allocated_length, prev_vect_allocated_length;
  size_t el_length, header_length, vect_els_number;
  const char *els;
  ER_node_t prev_vect;

  disp = ER_disp (vect);
  vect_els_number = ER_els_number (vect);
  allocated_length = ER_allocated_length (vect);
  if (ER_NODE_MODE (vect) == ER_NM_heap_pack_vect
      && ER_pack_vect_el_type (vect) == ER_NM_char)
    {
      els_number++; /* for trailing zero byte */
      vect_els_number++;
    }
  prev_vect_allocated_length = allocated_length;
  if (ER_NODE_MODE (vect) == ER_NM_heap_unpack_vect)
    {
      header_length = ALLOC_SIZE (sizeof (_ER_heap_unpack_vect));
      els = (const char *) ER_unpack_els (vect);
      el_length = sizeof (val_t);
    }
  else 
    {
      header_length = ALLOC_SIZE (sizeof (_ER_heap_pack_vect));
      els = ER_pack_els (vect);
      el_length = type_size (ER_pack_vect_el_type (vect));
    }
  if (allocated_length < header_length + els_number * el_length)
    allocated_length = (header_length
			+ OPTIMAL_ELS_SIZE (els_number * el_length));
  if (allocated_length != prev_vect_allocated_length)
    {
      prev_vect = vect;
      vect = heap_allocate (allocated_length, FALSE);
      /* After this, vect has the same unique_number. */
      if (disp == 0)
	memcpy (vect, prev_vect, prev_vect_allocated_length);
      else
	{	
	  memcpy (vect, prev_vect, header_length);
	  /* Set it before getting els.  */
	  ER_set_disp (vect, 0);
	  memcpy (ER_NODE_MODE (vect) == ER_NM_heap_unpack_vect
		  ? (char *) ER_unpack_els (vect) : (char *) ER_pack_els (vect),
		  els, vect_els_number * el_length);
	}
      ER_set_allocated_length (vect, allocated_length);
      ER_SET_MODE (prev_vect, ER_NM_heap_redir);
      ER_set_allocated_length (prev_vect, prev_vect_allocated_length);
      ER_set_redir (prev_vect, vect);
    }
  else if (allocated_length < header_length + els_number * el_length + disp)
    {
      assert (allocated_length >= header_length + els_number * el_length);
      /* Set it before getting els.  */
      ER_set_disp (vect, 0);
      memmove (ER_NODE_MODE (vect) == ER_NM_heap_unpack_vect
	       ? (char*) ER_unpack_els (vect) : (char *) ER_pack_els (vect),
	       els, ER_els_number (vect) * el_length);
      
    }
  return vect;
}

/* Unpacking vector. */
ER_node_t
unpack_vector (ER_node_t vect)
{
  size_t allocated_length;
  int immutable;
  size_t els_number;
  ER_node_t prev_vect;
  ER_node_mode_t el_type;
  size_t el_size;
  size_t pack_vect_allocated_length;
  size_t displ;
  size_t i, disp;
  val_t temp_var;

  disp = ER_disp (vect);
  allocated_length = ER_allocated_length (vect);
  immutable = ER_immutable (vect);
  els_number = ER_els_number (vect);
  el_type = ER_pack_vect_el_type (vect);
  el_size = type_size (el_type);
  prev_vect = vect;
  pack_vect_allocated_length = allocated_length;
  if (allocated_length - disp < (ALLOC_SIZE (sizeof (_ER_heap_unpack_vect))
				 + els_number * sizeof (val_t)))
    {
      allocated_length = (ALLOC_SIZE (sizeof (_ER_heap_unpack_vect))
			  + OPTIMAL_ELS_SIZE (els_number * sizeof (val_t)));
      vect = heap_allocate (allocated_length, FALSE);
      ER_SET_MODE (vect, ER_NM_heap_unpack_vect);
      ER_set_disp (vect, 0);
    }
  ER_SET_MODE ((ER_node_t) &temp_var, el_type);
  displ = val_displ ((ER_node_t) &temp_var);
  if (els_number != 0)
    {
      ER_node_mode_t mode = ER_NODE_MODE (vect);
      ER_node_t els;

      ER_SET_MODE (vect, ER_NM_heap_unpack_vect);
      els = ER_unpack_els (vect);
      ER_SET_MODE (vect, mode);
      for (i = els_number - 1; ; i--)
	{
	  /* Use this order.  It is important when we have only one
             element. */
	  memcpy ((char *) IVAL (els, i) + displ,
		  (char *) ER_pack_els (prev_vect) + i * el_size,
		  el_size);
	  ER_SET_MODE (IVAL (els, i), el_type);
	  if (i == 0)
	    break;
	}
      }
  ER_SET_MODE (vect, ER_NM_heap_unpack_vect);
  ER_set_allocated_length (vect, allocated_length);
  ER_set_immutable (vect, immutable);
  ER_set_els_number (vect, els_number);
  if (prev_vect != vect)
    {
      ER_SET_MODE (prev_vect, ER_NM_heap_redir);
      ER_set_allocated_length (prev_vect, pack_vect_allocated_length);
      ER_set_redir (prev_vect, vect);
      ER_set_unique_number (vect, ER_unique_number (prev_vect));
    }
  return vect;
}

/* The function should not call GC. */
void
pack_vector_if_possible (ER_node_t unpack_vect)
{
  ER_node_t pack_vect;
  size_t i;
  size_t allocated_length;
  int immutable;
  size_t els_number;
  char *els;
  ER_node_mode_t el_type;
  size_t el_size, displ;

  immutable = ER_immutable (unpack_vect);
  els_number = ER_els_number (unpack_vect);
  el_type = (els_number != 0
	     ? ER_NODE_MODE (IVAL (ER_unpack_els (unpack_vect), 0))
	     : ER_NM_nil);
  for (i = 0; i < els_number; i++)
    if (ER_NODE_MODE (IVAL (ER_unpack_els (unpack_vect), i)) != el_type)
      break;
  if (i >= els_number)
    {
      /* Pack it */
      el_size = type_size (el_type);
      allocated_length = ER_allocated_length (unpack_vect);
      pack_vect = unpack_vect;
      ER_SET_MODE (pack_vect, ER_NM_heap_pack_vect);
      els = ER_pack_els (pack_vect) - ER_disp (pack_vect);
      ER_SET_MODE (unpack_vect, ER_NM_heap_unpack_vect);
      displ = val_displ (IVAL (ER_unpack_els (unpack_vect), 0));
      for (i = 0; i < els_number; i++)
	memcpy (els + i * el_size,
		(char *) IVAL (ER_unpack_els (unpack_vect), i) + displ,
		el_size);
      if (el_type == ER_NM_char)
	els [els_number] = '\0';
      ER_SET_MODE (pack_vect, ER_NM_heap_pack_vect);
      ER_set_immutable (pack_vect, immutable);
      ER_set_pack_vect_el_type (pack_vect, el_type);
      ER_set_els_number (pack_vect, els_number);
      ER_set_disp (pack_vect, 0);
      ER_set_allocated_length (pack_vect, allocated_length);
    }
}

int
eq_vector (ER_node_t v1, ER_node_t v2)
{
  GO_THROUGH_REDIR (v1);
  GO_THROUGH_REDIR (v2);
  if (v1 == v2)
    return TRUE;
  if (ER_els_number (v1) != ER_els_number (v2))
    return FALSE;
  if (ER_NODE_MODE (v1) == ER_NM_heap_pack_vect
      && ER_NODE_MODE (v2) == ER_NM_heap_pack_vect)
    return (ER_pack_vect_el_type (v1) == ER_pack_vect_el_type (v2)
	    && memcmp (ER_pack_els (v1), ER_pack_els (v2),
		       ER_els_number (v1)
		       * type_size (ER_pack_vect_el_type (v1))) == 0);
  if (ER_NODE_MODE (v1) == ER_NM_heap_unpack_vect)
    pack_vector_if_possible (v1);
  if (ER_NODE_MODE (v2) == ER_NM_heap_unpack_vect)
    pack_vector_if_possible (v2);
  if (ER_NODE_MODE (v1) != ER_NODE_MODE (v2))
    return FALSE;
  else if (ER_NODE_MODE (v1) == ER_NM_heap_pack_vect)
    return (ER_pack_vect_el_type (v1) == ER_pack_vect_el_type (v2)
	    && memcmp (ER_pack_els (v1), ER_pack_els (v2),
		       ER_els_number (v1)
		       * type_size (ER_pack_vect_el_type (v1))) == 0);
  else
    return eq_val ((val_t *) ER_unpack_els (v1), (val_t *) ER_unpack_els (v2),
		   ER_els_number (v1));
}

/* The folllowing function make copy the vector vect.  The copy is
   always mutable. */
ER_node_t
copy_vector (ER_node_t vect)
{
  size_t vect_size;
  ER_node_t new_vect;
  
  GO_THROUGH_REDIR (vect);
  vect_size = ER_allocated_length (vect);
  new_vect = heap_allocate (vect_size, FALSE);
  memcpy (new_vect, vect, vect_size);
  ER_set_unique_number (new_vect, unique_number);
  unique_number++;
  ER_set_immutable (new_vect, FALSE);
  return new_vect;
}

ER_node_t
create_empty_string (size_t min_length)
{
  ER_node_t vect;
  
  vect = create_pack_vector (min_length, ER_NM_char);
  ER_set_immutable (vect, TRUE);
  ER_set_els_number (vect, 0);
  ER_pack_els (vect) [0] = '\0';
  return vect;
}

ER_node_t
create_string (const char *string)
{
  ER_node_t vect;
  size_t chars_number;
  
  chars_number = strlen (string);
  vect = create_pack_vector (chars_number + 1, ER_NM_char);
  ER_set_immutable (vect, TRUE);
  ER_set_els_number (vect, chars_number);
  strcpy (ER_pack_els (vect), string);
  return vect;
}




#if INLINE
__inline__
#endif
int
eq_instance (ER_node_t i1, ER_node_t i2)
{
  return (i1 == i2
	  || ER_instance_class (i1) == ER_instance_class (i2)
	  && ER_context (i1) == ER_context (i2)
	  && eq_val ((val_t *) ER_instance_vars (i1),
		     (val_t *) ER_instance_vars (i2),
		     IR_vars_number (IR_next_stmt (ER_instance_class (i1)))));
}



/* The page contains functions for work with tables. */

unsigned int tab_collisions;
unsigned int tab_expansions;

void
initiate_tables (void)
{
  tab_collisions = 0;
  tab_expansions = 0;
}

int
eq_table (ER_node_t t1, ER_node_t t2)
{
  size_t i;
  ER_node_t key;

  GO_THROUGH_REDIR (t1);
  GO_THROUGH_REDIR (t2);
  if (t1 == t2)
    return TRUE;
  if (ER_els_number (t1) != ER_els_number (t2))
    return FALSE;
  for (i = 0; i < ER_entries_number (t1); i++)
    {
      if (ER_NODE_MODE (INDEXED_ENTRY_KEY (ER_tab_els (t1), i))
	  != ER_NM_empty_entry
	  && (ER_NODE_MODE (INDEXED_ENTRY_KEY (ER_tab_els (t1), i))
	      != ER_NM_deleted_entry))
	{
	  key = find_tab_entry (t2, INDEXED_ENTRY_KEY (ER_tab_els (t1), i),
				FALSE);
	  if (ER_NODE_MODE (key) == ER_NM_empty_entry
	      || ER_NODE_MODE (key) == ER_NM_deleted_entry
	      || !eq_val ((val_t *) ((char *) key + sizeof (val_t)),
			  (val_t *) INDEXED_ENTRY_VAL (ER_tab_els (t1), i), 1))
	    return FALSE;
	}
    }
  return TRUE;
}

#if INLINE
__inline__
#endif
static size_t
hash_ref (ER_node_t ref)
{
  if (ref == NULL)
    return 0;
  return ER_unique_number (ref);
}

#if INLINE
__inline__
#endif
static size_t
hash_val (ER_node_t val)
{
  size_t hash;
  size_t i;
  size_t length;
  unsigned char *string;

  assert (val != NULL);
  switch (ER_NODE_MODE (val))
    {
    case ER_NM_nil:
      return 0;
    case ER_NM_char:
      /* See also special case (string) in eq_key.  */
      return (size_t) ER_ch (val);
    case ER_NM_int:
      return (size_t) ER_i (val);
    case ER_NM_float:
      {
	floating_t f;
	
	length = sizeof (floating_t);
	f = ER_f (val);
	string = (unsigned char *) &f;
	for (hash = i = 0; i < length; i++)
	  hash += string[i] << (3 * i % (CHAR_BIT * (sizeof (size_t) - 1)));
	return hash;
      }
    case ER_NM_type:
      return (size_t) ER_type (val);
    case ER_NM_hide:
      return (size_t) ER_hide (val);
    case ER_NM_hideblock:
      return (size_t) ER_unique_number (ER_hideblock (val));
    case ER_NM_vect:
      return (size_t) ER_unique_number (ER_vect (val));
    case ER_NM_tab:
      return (size_t) ER_unique_number (ER_tab (val));
    case ER_NM_instance:
      return (size_t) ER_unique_number (ER_instance (val));
    case ER_NM_func:
      return ((size_t) ER_func_no (val)
	      + (size_t) ER_unique_number (ER_func_context (val)));
    case ER_NM_thread:
      return ((size_t) ER_thread_no (val)
	      + (size_t) ER_unique_number (ER_thread_context (val)));
    case ER_NM_process:
      return (size_t) ER_unique_number (ER_process (val));
    case ER_NM_class:
      return ((size_t) ER_class_no (val)
	      + (size_t) ER_unique_number (ER_class_context (val)));
    case ER_NM_stack:
      return (size_t) ER_unique_number (ER_stack (val));
    default:
      assert (FALSE);
    }
}

static size_t
hash_key (ER_node_t key)
{
  size_t hash, el_hash;
  size_t i;
  size_t length;
  unsigned char *string;
  int shift;

  switch (ER_NODE_MODE (key))
    {
    case ER_NM_nil:
    case ER_NM_char:
    case ER_NM_int:
    case ER_NM_float:
    case ER_NM_type:
    case ER_NM_hide:
    case ER_NM_func:
    case ER_NM_thread:
    case ER_NM_process:
    case ER_NM_class:
    case ER_NM_stack:
      hash = hash_val (key);
      break;
    case ER_NM_vect:
      {
	ER_node_t vect;

	vect = ER_vect (key);
	GO_THROUGH_REDIR (vect);
	ER_set_vect (key, vect);
	if (ER_NODE_MODE (ER_vect (key)) == ER_NM_heap_unpack_vect)
	  pack_vector_if_possible (ER_vect (key));
	if (ER_NODE_MODE (ER_vect (key)) == ER_NM_heap_pack_vect)
	  {
	    ER_node_t pv = ER_vect (key);

	    if (ER_pack_vect_el_type (pv) == ER_NM_char)
	      {
		/* Special frequent case.  */
		char *str;

		str = (char *) ER_pack_els (pv);
		for (hash = i = 0; i < ER_els_number (pv); i++)
		  {
		    el_hash = str [i];
		    shift = 13 * i % (CHAR_BIT * sizeof (size_t));
		    hash += (el_hash << shift) | (el_hash >> shift);
		  }
	      }
	    else
	      {
		val_t var;
		ER_node_t var_ref = (ER_node_t) &var;
		size_t displ;
		size_t el_size;
		
		ER_SET_MODE (var_ref, ER_pack_vect_el_type (pv));
		displ = val_displ (var_ref);
		el_size = type_size (ER_pack_vect_el_type (pv));
		for (hash = i = 0; i < ER_els_number (pv); i++)
		  {
		    memcpy ((char *) var_ref + displ,
			    (char *) ER_pack_els (pv) + i * el_size,
			    el_size);
		    el_hash = hash_val (var_ref);
		    shift = 13 * i % (CHAR_BIT * sizeof (size_t));
		    hash += (el_hash << shift) | (el_hash >> shift);
		  }
	      }
	  }
	else
	  {
	    ER_node_t unpv = ER_vect (key);
	    
	    for (hash = i = 0; i < ER_els_number (unpv); i++)
	      {
		el_hash = hash_val (IVAL (ER_unpack_els (unpv), i));
		shift = 13 * i % (CHAR_BIT * sizeof (size_t));
		hash += (el_hash << shift) | (el_hash >> shift);
	      }
	  }
	break;
      }
    case ER_NM_tab:
      {
	ER_node_t tab;
	ER_node_t entry_key;

	tab = ER_tab (key);
	GO_THROUGH_REDIR (tab);
	ER_set_tab (key, tab);
	for (i = 0; i < ER_entries_number (tab); i++)
	  {
	    entry_key = INDEXED_ENTRY_KEY (ER_tab_els (tab), i);
	    if (ER_NODE_MODE (entry_key) != ER_NM_empty_entry
		&& ER_NODE_MODE (entry_key) != ER_NM_deleted_entry)
	      /* We need here associative operation. */
	      hash += (hash_val (entry_key) + hash_val (IVAL (entry_key, 1)));
	  }
	break;
      }
    case ER_NM_instance:
      for (hash = 0,
	     i = IR_vars_number (IR_next_stmt (ER_instance_class
					       (ER_instance (key))));
	   i > 0; i--)
	{
	  el_hash
	    = hash_val (IVAL (ER_instance_vars (ER_instance (key)), i - 1));
	  shift = 13 * i % (CHAR_BIT * sizeof (size_t));
	  hash += (el_hash << shift) | (el_hash >> shift);
	}
      break;
    case ER_NM_hideblock:
      string = (unsigned char *) ER_hideblock_start (ER_hideblock (key));
      length = ER_hideblock_length (ER_hideblock (key));
      for (hash = i = 0; i < length; i++)
	{
	  el_hash = string[i];
	  shift = 13 * i % (CHAR_BIT * sizeof (size_t));
	  hash += (el_hash << shift) | (el_hash >> shift);
	}
      break;
    default:
      assert (FALSE);
    }
  return hash;
}

#if INLINE
__inline__
#endif
static int
eq_key (ER_node_t entry_key, ER_node_t key)
{
  if (ER_NODE_MODE (entry_key) != ER_NODE_MODE (key))
    return FALSE;
  switch (ER_NODE_MODE (key))
    {
    case ER_NM_nil:
      return TRUE;
    case ER_NM_char:
      return ER_ch (key) == ER_ch (entry_key);
    case ER_NM_int:
      return ER_i (key) == ER_i (entry_key);
    case ER_NM_float:
      return ER_f (key) == ER_f (entry_key);
    case ER_NM_type:
      return ER_type (key) == ER_type (entry_key);
    case ER_NM_vect:
      return eq_vector (ER_vect (key), ER_vect (entry_key));
    case ER_NM_tab:
      return eq_table (ER_tab (key), ER_tab (entry_key));
    case ER_NM_instance:
      return eq_instance (ER_instance (key), ER_instance (entry_key));
    case ER_NM_func:
      return (ER_func_context (key) == ER_func_context (entry_key)
	      && ER_func_no (key) == ER_func_no (entry_key));
    case ER_NM_thread:
      return (ER_thread_context (key) == ER_thread_context (entry_key)
	      && ER_thread_no (key) == ER_thread_no (entry_key));
    case ER_NM_process:
      return ER_process (key) == ER_process (entry_key);
    case ER_NM_class:
      return (ER_class_context (key) == ER_class_context (entry_key)
	      && ER_class_no (key) == ER_class_no (entry_key));
    case ER_NM_stack:
      return ER_stack (key) == ER_stack (entry_key);
    default:
      assert (FALSE);
    }
  return FALSE; /* No warnings */
}

/* Start size of table with N elements.  The table must be not expand
   if we insert size elements number. */
#define START_TABLE_SIZE(n)  (3*(n) + 20)

/* Minimal optimal size of table with N elements.  If the real size is
   less, it is better to expand the table. */
#define MINIMAL_TABLE_SIZE(n)  (4*(n)/3)

/* The following function returns the nearest prime number which is
   greater than given source number. */
static unsigned long
higher_prime_number (unsigned long number)
{
  unsigned long i;

  for (number = (number / 2) * 2 + 3;; number += 2)
    {
      for (i = 3; i * i <= number; i += 2)
        if (number % i == 0)
          break;
      if (i * i > number)
        return number;
    }
}

/* This function creates table with length slightly longer than given
   source length.  Created hash table is initiated as empty.  The
   function returns the created hash table. */
ER_node_t
create_tab (size_t size)
{
  size_t entries_number;
  size_t allocated_length;
  ER_node_t result;
  size_t i;

  entries_number = higher_prime_number (START_TABLE_SIZE (size));
  allocated_length = (ALLOC_SIZE (sizeof (_ER_heap_tab))
		      + 2 * entries_number * sizeof (val_t));
  result = (ER_node_t) heap_allocate (allocated_length, FALSE);
  ER_SET_MODE (result, ER_NM_heap_tab);
  ER_set_immutable (result, FALSE);
  ER_set_els_number (result, 0);
  ER_set_allocated_length (result, allocated_length);
  ER_set_entries_number (result, entries_number);
  ER_set_deleted_els_number (result, 0);
  ER_set_last_key_index (result, -1);
  for (i = 0; i < ER_entries_number (result); i++)
    ER_SET_MODE (INDEXED_ENTRY_KEY (ER_tab_els (result), i),
		 ER_NM_empty_entry);
  return result;
}

static ER_node_t expand_tab (ER_node_t tab);

/* This function searches for table entry which contains key equal to
   given key or empty entry in which given value can be placed.  The
   function works in two modes.  The first mode is used only for
   search.  The second is used for search and reservation empty entry
   for given value (the value with given key should be inserted into
   the table entry before another call of the function).  The table is
   expanded in reservation mode if occupancy (taking into accout also
   deleted elements) is more than 75%.  Attention: this means that the
   place of the table may change after any function call with
   reservation. */
ER_node_t
find_tab_entry (ER_node_t tab, ER_node_t key, int reserve)
{
  ER_node_t entry_key;
  ER_node_t first_deleted_entry_key;
  size_t hash_value, secondary_hash_value;
  int_t last_key_index;
  
  if ((last_key_index = ER_last_key_index (tab)) >= 0)
    {
      entry_key = INDEXED_ENTRY_KEY (ER_tab_els (tab), last_key_index);
      if (eq_key (entry_key, key))
	return entry_key;
    }
  if (reserve
      && (ER_entries_number (tab)
	  <= MINIMAL_TABLE_SIZE (ER_els_number (tab)
				 + ER_deleted_els_number (tab))))
    tab = expand_tab (tab);
  hash_value = hash_key (key);
  secondary_hash_value = 1 + hash_value % (ER_entries_number (tab) - 2);
  hash_value %= ER_entries_number (tab);
  first_deleted_entry_key = NULL;
  for (;;)
    {
      entry_key = INDEXED_ENTRY_KEY (ER_tab_els (tab), hash_value);
      if (ER_NODE_MODE (entry_key) == ER_NM_empty_entry)
        {
          if (reserve)
	    {
	      ER_set_els_number (tab, ER_els_number (tab) + 1);
	      if (first_deleted_entry_key != NULL)
		{
		  assert (ER_deleted_els_number (tab) > 0);
		  ER_set_deleted_els_number
		    (tab, ER_deleted_els_number (tab) - 1);
		  entry_key = first_deleted_entry_key;
		  ER_SET_MODE (entry_key, ER_NM_empty_entry);
		}
	    }
	  ER_set_last_key_index (tab, -1);
          break;
        }
      else if (ER_NODE_MODE (entry_key) != ER_NM_deleted_entry)
        {
          if (eq_key (entry_key, key))
	    {
	      ER_set_last_key_index (tab, (int_t) hash_value);
	      break;
	    }
        }
      else if (first_deleted_entry_key == NULL)
	first_deleted_entry_key = entry_key;
      hash_value += secondary_hash_value;
      if (hash_value >= ER_entries_number (tab))
        hash_value -= ER_entries_number (tab);
      tab_collisions++;
    }
  return entry_key;
}

/* The following function changes expand given table by creating new
   table and inserting the table elements.  The occupancy of the table
   after the call will be about 33%.  Remember also that the place in
   the table of the table entries (identified by keys) are changed. */
static ER_node_t
expand_tab (ER_node_t tab)
{
  ER_node_t new_tab;
  ER_node_t new_entry;
  int immutable;
  size_t allocated_length;
  size_t i;

  immutable = ER_immutable (tab);
  tab_expansions++;
  new_tab = create_tab (ER_els_number (tab));
  ER_set_immutable (new_tab, immutable);
  assert (ER_allocated_length (new_tab) > ER_allocated_length (tab));
  for (i = 0; i < ER_entries_number (tab); i++)
    if (ER_NODE_MODE (INDEXED_ENTRY_KEY (ER_tab_els (tab), i))
	!= ER_NM_empty_entry
	&& (ER_NODE_MODE (INDEXED_ENTRY_KEY (ER_tab_els (tab), i))
	    != ER_NM_deleted_entry))
      {
        new_entry = find_tab_entry (new_tab,
				    INDEXED_ENTRY_KEY (ER_tab_els (tab), i),
				    TRUE);
        assert (ER_NODE_MODE (new_entry) == ER_NM_empty_entry);
        *(val_t *) new_entry
	  = *(val_t *) INDEXED_ENTRY_KEY (ER_tab_els (tab), i);
        ((val_t *) new_entry) [1]
	  = *(val_t *) INDEXED_ENTRY_VAL (ER_tab_els (tab), i);
	assert (ER_IS_OF_TYPE (INDEXED_ENTRY_VAL (ER_tab_els (tab), i),
			       ER_NM_val));
	assert (ER_IS_OF_TYPE ((ER_node_t) &((val_t *) new_entry) [1],
			       ER_NM_val));
      }
  allocated_length = ER_allocated_length (tab);
  ER_SET_MODE (tab, ER_NM_heap_redir);
  ER_set_allocated_length (tab, allocated_length);
  ER_set_redir (tab, new_tab);
  ER_set_unique_number (new_tab, ER_unique_number (tab));
  ER_set_last_key_index (new_tab, -1);
  return new_tab;
}

/* This function deletes element with given value from table.  The
   table entry value will be `ER_NM_deleted_entry' after the function
   call.  The function returns TRUE if the element has been removed.
   Otherwise, it returns FALSE.  That means that such element does not
   exist. */
int
remove_tab_el (ER_node_t tab, ER_node_t key)
{
  ER_node_t entry_key;

  entry_key = find_tab_entry (tab, key, FALSE);
  if (ER_NODE_MODE (entry_key) == ER_NM_empty_entry
      || ER_NODE_MODE (entry_key) == ER_NM_deleted_entry)
    return FALSE;
  ER_SET_MODE (entry_key, ER_NM_deleted_entry);
  assert (ER_els_number (tab) > 0);
  ER_set_els_number (tab, ER_els_number (tab) - 1);
  ER_set_deleted_els_number (tab, ER_deleted_els_number (tab) + 1);
  ER_set_last_key_index (tab, -1);
  return TRUE;
}

/* The following function make copy the table TAB.  The copy is
   always mutable. */
ER_node_t
copy_tab (ER_node_t tab)
{
  ER_node_t new_tab;
  ER_node_t new_entry;
  int immutable;
  size_t i;

  immutable = ER_immutable (tab);
  new_tab = create_tab (ER_els_number (tab));
  for (i = 0; i < ER_entries_number (tab); i++)
    if (ER_NODE_MODE (INDEXED_ENTRY_KEY (ER_tab_els (tab), i))
	!= ER_NM_empty_entry
	&& (ER_NODE_MODE (INDEXED_ENTRY_KEY (ER_tab_els (tab), i))
	    != ER_NM_deleted_entry))
      {
        new_entry = find_tab_entry (new_tab,
				    INDEXED_ENTRY_KEY (ER_tab_els (tab), i),
				    TRUE);
        assert (ER_NODE_MODE (new_entry) == ER_NM_empty_entry);
        *(val_t *) new_entry
	  = *(val_t *) INDEXED_ENTRY_KEY (ER_tab_els (tab), i);
        ((val_t *) new_entry) [1]
	  = *(val_t *) INDEXED_ENTRY_VAL (ER_tab_els (tab), i);
	assert (ER_IS_OF_TYPE (INDEXED_ENTRY_VAL (ER_tab_els (tab), i),
			       ER_NM_val));
	assert (ER_IS_OF_TYPE ((ER_node_t) &((val_t *) new_entry) [1],
			       ER_NM_val));
      }
  return new_tab;
}

/* The function returns the next key in TAB after given one or the
   first key if KEY == NULL. */
ER_node_t
find_next_key (ER_node_t tab, ER_node_t key)
{
  size_t start;
  size_t i;
  
  if (key == NULL)
    start = 0;
  else
    start = ((val_t *) find_tab_entry (tab, key, FALSE)
	     - (val_t *) ER_tab_els (tab)) / 2 + 1;
  for (i = start; i < ER_entries_number (tab); i++)
    {
      key = INDEXED_ENTRY_KEY (ER_tab_els (tab), i);
      if (ER_NODE_MODE (key) != ER_NM_empty_entry
	  && ER_NODE_MODE (key) != ER_NM_deleted_entry)
	{
	  ER_set_last_key_index (tab, i);
	  return key;
	}
    }
  for (i = 0; i < ER_entries_number (tab); i++)
    {
      key = INDEXED_ENTRY_KEY (ER_tab_els (tab), i);
      if (ER_NODE_MODE (key) == ER_NM_empty_entry
	  || ER_NODE_MODE (key) == ER_NM_deleted_entry)
	{
	  ER_set_last_key_index (tab, -1);
	  return key;
	}
    }
  assert (FALSE);
  return NULL;
}

ER_node_t
table_to_vector_conversion (ER_node_t tab)
{
  size_t i, index;
  ER_node_t vect;

  GO_THROUGH_REDIR (tab);
  index = 0;
  if (ER_els_number (tab) == 0)
    vect = create_empty_vector ();
  else
    vect = create_unpack_vector (ER_els_number (tab) * 2);
  for (i = 0; i < ER_entries_number (tab); i++)
    if (ER_NODE_MODE (INDEXED_ENTRY_KEY (ER_tab_els (tab), i))
	!= ER_NM_empty_entry
	&& (ER_NODE_MODE (INDEXED_ENTRY_KEY (ER_tab_els (tab), i))
	    != ER_NM_deleted_entry))
      {
	*(val_t *) IVAL (ER_unpack_els (vect), index)
	  = *(val_t *) INDEXED_ENTRY_KEY (ER_tab_els (tab), i);
	*(val_t *) IVAL (ER_unpack_els (vect), index + 1)
	  = *(val_t *) INDEXED_ENTRY_VAL (ER_tab_els (tab), i);
	index += 2;
      }
  return vect;
}

ER_node_t
vector_to_table_conversion (ER_node_t vect)
{
  size_t i, el_size_type;
  ER_node_t tab, tvar;
  ER_node_t entry;

  GO_THROUGH_REDIR (vect);
  tab = create_tab (ER_els_number (vect));
  tvar = IVAL (cvars, tvar_num1);
  for (i = 0; i < ER_els_number (vect); i++)
    {
      ER_SET_MODE ((ER_node_t) tvar, ER_NM_int);
      ER_set_i (tvar, i);
      entry = find_tab_entry (tab, tvar, TRUE);
      *(val_t *) entry = *(val_t *) tvar;
      /* Integer is always immutable. */
      if (ER_NODE_MODE (vect) == ER_NM_heap_unpack_vect)
	*((val_t *) entry + 1) = *(val_t *) IVAL (ER_unpack_els (vect), i);
      else
	{
	  el_size_type = type_size (ER_pack_vect_el_type (vect));
	  ER_SET_MODE ((ER_node_t) tvar, ER_pack_vect_el_type (vect));
	  memcpy ((char *) tvar + val_displ (tvar),
		  ER_pack_els (vect) + i * el_size_type, el_size_type);
	  *((val_t *) entry + 1) = *(val_t *) tvar;
	}
    }
  return tab;
}



void
make_immutable (ER_node_t obj)
{
  if (ER_NODE_MODE (obj) == ER_NM_vect)
    {
      ER_node_t vect;

      vect = ER_vect (obj);
      GO_THROUGH_REDIR (vect);
      ER_set_immutable (vect, TRUE);
    }
  else if (ER_NODE_MODE (obj) == ER_NM_tab)
    {
      ER_node_t tab;

      tab = ER_tab (obj);
      GO_THROUGH_REDIR (tab);
      ER_set_immutable (tab, TRUE);
    }
  else if (ER_NODE_MODE (obj) == ER_NM_instance)
    ER_set_immutable (ER_instance (obj), TRUE);
}



ER_node_t
create_process (pc_t start_process_pc, IR_node_t func, ER_node_t func_context)
{
  ER_node_t process;

  process = (ER_node_t) heap_allocate (ALLOC_SIZE (sizeof (_ER_heap_process)),
				       FALSE);
  ER_SET_MODE (process, ER_NM_heap_process);
  ER_set_context (process, func_context);
  ER_set_context_number (process, context_number);
  context_number++;
  ER_set_thread_func (process, func);
  ER_set_process_status (process, PS_READY);
  ER_set_process_number (process, process_number);
  process_number++;
  ER_set_saved_pc (process, start_process_pc);
  ER_set_saved_cstack (process, cstack);
  ER_set_father (process, cprocess);
  if (cprocess == NULL)
    {
      ER_set_prev (process, process);
      ER_set_next (process, process);
    }
  else
    {
      ER_set_prev (process, ER_prev (cprocess));
      ER_set_next (ER_prev (process), process);
      ER_set_prev (cprocess, process);
      ER_set_next (process, cprocess);
    }
  return process;
}

static void
activate_given_process (ER_node_t process)
{
  ER_node_t var;

#if ! defined (NO_PROFILE) && !HAVE_SETITIMER
  if (cprocess != process && profile_flag)
    {
      IR_node_t block, func_class;

      for (block = ER_block_node (ER_saved_cstack (cprocess));
	   (func_class = IR_func_class_ext (block)) == NULL;
	   block = IR_block_scope (block))
	;
      ticker_off (&IR_exec_time (func_class));
      for (block = ER_block_node (ER_saved_cstack (process));
	   (func_class = IR_func_class_ext (block)) == NULL;
	   block = IR_block_scope (block))
	;
      if (IR_calls_number (func_class) == 0)
	abort ();
      ticker_on (&IR_exec_time (func_class));
    }
#endif
  cprocess = process;
  cpc = ER_saved_pc (cprocess);
  if (cstack != NULL)
    ER_set_ctop (cstack, (char *) ctop);
  cstack = ER_saved_cstack (cprocess);
  cvars = ER_stack_vars (cstack);
  ctop = (ER_node_t) ER_ctop (cstack);
  set_tvars (ER_block_node (cstack));
#ifndef NO_CONTAINER_CACHE
  current_cached_container_tick++;
#endif
  ER_set_process_status (cprocess, PS_READY);
  var = IVAL (ER_stack_vars (uppest_stack),
	      IR_var_number_in_block (curr_thread_decl));
  ER_SET_MODE (var, ER_NM_process);
  ER_set_process (var, cprocess);
}

/* It is important for block_cprocess that the search of process for
   activation starts with process after cprocess. */
static void
activate_process (void)
{
  activate_given_process (ER_next (cprocess));
  if (first_process_not_started == cprocess)
    eval_error (deadlock_decl, errors_decl, IR_pos (cpc), DERR_deadlock);
  executed_stmts_count = -process_quantum; /* start new quantum */
}

void
block_cprocess (pc_t first_resume_pc, int wait_stmt_flag)
{
  assert (cprocess != NULL);
  if (sync_flag)
    {
      executed_stmts_count = -process_quantum; /* start new quantum */
      return;
    }
  ER_set_saved_pc (cprocess, first_resume_pc);
  ER_set_saved_cstack (cprocess, cstack);
  if (executed_stmts_count != -process_quantum)
    ER_set_process_status (cprocess, PS_STARTED);
  if (!wait_stmt_flag)
    {
      assert (ER_process_status (cprocess) == PS_STARTED);
      ER_set_process_status (cprocess, PS_BLOCKED_BY_QUANTUM_SWITCH);
      first_process_not_started = NULL;
    }
  else
    {
      if (ER_process_status (cprocess) == PS_READY)
	{
	  ER_set_process_status (cprocess, PS_NOT_STARTED);
	  if (first_process_not_started == NULL)
	    first_process_not_started = cprocess;
	}
      else
	{
	  assert (ER_process_status (cprocess) == PS_STARTED);
	  ER_set_process_status (cprocess, PS_BLOCKED_BY_WAIT);
	  first_process_not_started = NULL;
	}
    }
  activate_process ();
}

void
delete_cprocess (void)
{
  first_process_not_started = NULL;
  if (ER_next (cprocess) == cprocess)
    dino_finish (0);
  else
    {
      ER_set_prev (ER_next (cprocess), ER_prev (cprocess));
      ER_set_next (ER_prev (cprocess), ER_next (cprocess));
      cprocess = ER_prev (cprocess);
      activate_process ();
    }
}

/* Delete current process because of an exception. */
int
delete_cprocess_during_exception (void)
{
  if (ER_next (cprocess) == cprocess || ER_father (cprocess) == NULL)
    return FALSE;
  else
    {
      first_process_not_started = NULL;
      ER_set_prev (ER_next (cprocess), ER_prev (cprocess));
      ER_set_next (ER_prev (cprocess), ER_next (cprocess));
      activate_given_process (ER_father (cprocess));
      return TRUE;
    }
}

/* The following variables for switching process to prevent
   starvation.  The first one is approximate number of executed stmts
   after starting the process minus process quantum.  The second one is
   maximal number of stmts executed in a process without switching. */
int executed_stmts_count;
int process_quantum;

void 
initiate_processes (pc_t start_pc)
{
  cprocess = NULL;
  process_number = 0;
  process_quantum = 700;
  executed_stmts_count = -process_quantum;
  cprocess = create_process (start_pc, NULL, NULL);
  first_process_not_started = NULL;
  activate_process ();
}



/* Interrupt because of quantum expiring or GC is needed.  */
void
interrupt (pc_t first_resume_pc)
{
  if (GC_executed_stmts_count > 0)
    block_cprocess (first_resume_pc, FALSE);
  else
    {
      /* It is actually interrupt for GC.  */ 
      executed_stmts_count = GC_executed_stmts_count;
      GC ();
    }
}



static const char *
lib_name (const char *path_name)
{
  const char *name_start;
  const char *last_period;
  const char *curr_char_ptr;
  size_t len;
#define MAX_LIB_NAME_LENGTH 200
  static char result [MAX_LIB_NAME_LENGTH];

  assert (path_name != NULL);
  for (curr_char_ptr = path_name, name_start = path_name;
       *curr_char_ptr != '\0';
       curr_char_ptr++)
    if (*curr_char_ptr == '/')
      name_start = curr_char_ptr + 1;
  for (curr_char_ptr = name_start, last_period = NULL;
       *curr_char_ptr != '\0';
       curr_char_ptr++)
    if (*curr_char_ptr == '.')
      last_period = curr_char_ptr;
  
  if (last_period == NULL)
    len = strlen (name_start);
  else
    len = last_period - name_start;
  len = (len < MAX_LIB_NAME_LENGTH ? len : MAX_LIB_NAME_LENGTH - 1);
  strncpy (result, name_start, len);
  result [len] = '\0';
  return result;
}

void *
external_address (IR_node_t decl)
{
  void *address;
  const char *name;
  const char **curr_libname_ptr;
  int i;

  name = IR_ident_string (IR_unique_ident (IR_ident (decl)));
  if (IR_address (decl) != NULL)
    address = (external_func_t *) IR_address (decl);
  else
    {
#if defined(HAVE_DLOPEN) && !defined(NO_EXTERN_SHLIB)
      (void) dlopen (NULL, RTLD_NOW);
      for (curr_libname_ptr = libraries;
	   *curr_libname_ptr != NULL;
	   curr_libname_ptr++)
	{
	  void *handle;
	  FILE *f;

	  /* Dlopen on some system does not like unexisting libraries. */
	  f = fopen (*curr_libname_ptr, "r");
	  if (f == NULL)
	    continue;
	  fclose (f);
	  handle = dlopen (*curr_libname_ptr, RTLD_LAZY | RTLD_GLOBAL);
	  if (handle == NULL)
	    continue;
	  address = dlsym (handle, name);
	  if (dlerror () == NULL)
	    break;
	}
#else
      for (curr_libname_ptr = libraries;
	   *curr_libname_ptr != NULL;
	   curr_libname_ptr++)
	{
	  typedef void * (*address_function_t) (const char *);
	  void *handle;

	  handle = get_library_search_function (lib_name (*curr_libname_ptr));
	  if (handle == NULL)
	    continue;
	  address = (*(address_function_t) handle) (name);
	  if (address != NULL)
	    break;
	}
#endif
      if (*curr_libname_ptr == NULL)
	eval_error (noextern_decl, invexterns_decl, IR_pos (cpc),
		    DERR_no_such_external, name);
    }
  IR_set_address (decl, address);
  if (IR_IS_OF_TYPE (decl, IR_NM_external_var))
    {
      for (i = 0; i < VLO_LENGTH (external_vars) / sizeof (void *); i++)
	/* We need something faster (hashtab) ??? */
	if (((void **) VLO_BEGIN (external_vars)) [i] == address)
	  break;
      if (i >= VLO_LENGTH (external_vars) / sizeof (void *))
	VLO_ADD_MEMORY (external_vars, &address, sizeof (address));
    }
  return address;
}
