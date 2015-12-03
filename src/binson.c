/*
 *  Copyright (c) 2015 Contributors as noted in the AUTHORS file
 *
 *  This file is part of binson-c, BINSON serialization format library in C.
 *
 *  binson-c is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License (LGPL) as published
 *  by the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, the Contributors give you permission to link
 *  this library with independent modules to produce an executable,
 *  regardless of the license terms of these independent modules, and to
 *  copy and distribute the resulting executable under terms of your choice,
 *  provided that you also meet, for each linked independent module, the
 *  terms and conditions of the license of that module. An independent
 *  module is a module which is not derived from or based on this library.
 *  If you modify this library, you must extend this exception to your
 *  version of the library.
 *
 *  binson-c is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/********************************************//**
 * \file binson.h
 * \brief Binson-c library public API implementation file
 *
 * \author Alexander Reshniuk
 * \date 20/11/2015
 *
 ***********************************************/

#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <assert.h>

#include "binson.h"
#include "binson_error.h"
#include "binson_io.h"
#include "binson_util.h"
#include "binson_writer.h"
#include "binson_parser.h"

#define BINSON_VERSION_HEX    ((BINSON_MAJOR_VERSION << 16) |   \
                              (BINSON_MINOR_VERSION << 8)  |   \
                              (BINSON_MICRO_VERSION << 0))

/* Binson context type */
typedef struct binson_ {

  binson_node     *root;

  binson_writer   *writer;
  binson_parser   *parser;

  binson_io       *error_io;

  /*binson_strstack_t  str_stack;*/

} binson_;



typedef union binson_node_val_ {

    bool      b_data;
    int64_t   i_data;
    double    d_data;

      /* vector-like data stored in separate memory block, used for STRING, BYTES */
      struct v_data
      {  /* vector data */
         uint8_t*    ptr;   /* strings are zero-terminated, 'size' field takes ending '\0' into account */
         size_t      size;  /* size in bytes of payload data in block, pointed by ptr, see section 2. of BINSON-SPEC-1 */
      } v_data;

    /*binson_node_val_composite  children;*/   /* for ARRAY, OBJECT only */

      struct children {

        binson_node   *first_child;
        binson_node   *last_child;

      } children;

} binson_node_val_;

#ifndef binson_node_val_DEFINED
typedef union binson_node_val_             binson_node_val;
# define binson_node_val_DEFINED
#endif

/* each node (both terminal and nonterminal) is 'binson_node' instance */
typedef struct binson_node_ {

    /* tree navigation refs */
    binson_node       *parent;
    binson_node       *prev;
    binson_node       *next;

    /* payload */
    char               *key;
    binson_node_type   type;
    binson_node_val    val;

} binson_node_;


typedef struct binson_traverse_cb_status_    /* set by iterating function (caller) */
{
    /* initial parameters */
    binson                         *obj;
    binson_node                    *root_node;
    binson_traverse_method          t_method;
    int                             max_depth;
    binson_traverse_callback        cb;
    void*                           param;

    /* processing */
    binson_node                     *current_node;   /* last visited node */

    binson_traverse_dir             dir;             /* traverse direction */
    binson_child_num                child_num;       /*  current node is 'child_num's parent's child */
    int                             depth;           /*  curent node's depth */

    bool                            done;            /*  no more nodes to process; */

} binson_traverse_cb_status_;

/* used to pass parameters to/from callbacks */
typedef struct binson_traverse_cb_param_
{
    union in_param
    {
        binson_child_num  idx;
        const char*       key;
        binson_writer    *writer;
        binson_parser    *parser;

    } in_param;

    union out_param
    {
        binson_node       *node;
        binson_node_num    node_num;
        int                cmp_res;   /* node key to str key comparison result */

    } out_param;

} binson_traverse_cb_param_;

/* private helper functions */
binson_res  binson_node_add_empty( binson *obj, binson_node *parent, binson_node_type node_type, const char* key, binson_node **dst );
binson_res  binson_node_copy_val( binson_node_type node_type, binson_node_val *dst_val, binson_node_val *src_val );

/* tree traversal iteration callbacks (iterators) */
binson_res  binson_cb_lookup_key( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param );
binson_res  binson_cb_lookup_idx( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param );
binson_res  binson_cb_count( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param );
binson_res  binson_cb_dump( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param );
binson_res  binson_cb_key_compare( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param );
binson_res  binson_cb_remove( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param );

/*
binson_res  _binson_node_serialize_to_io( binson *obj, binson_node *node, binson_io *io );
*/

/* \brief Return version of libbinson.so installed
 *
 * \return uint32_t
 */
uint32_t  binson_lib_get_version()
{
   return BINSON_VERSION_HEX;
}

/* \brief Check if headers matches binson lib version installed
 *
 * \param void
 * \return bool
 */
bool  binson_lib_is_compatible()
{
    uint16_t major = binson_lib_get_version() >> 16;
    return (major == BINSON_MAJOR_VERSION)? true:false;
}

/* \brief Initialize binson context object
 *
 * \param obj binson*
 * \param writer binson_writer*
 * \param parser binson_parser*
 * \return binson_res
 */
binson_res  binson_init( binson *obj, binson_writer *writer, binson_parser *parser, binson_io *error_io )
{
  binson_res  res;

  obj->root       = NULL;
  obj->writer     = writer;
  obj->parser     = parser;
  obj->error_io   = error_io;

  res = binson_error_init( error_io );
  if (FAILED(res)) return res;

  /* add empty root OBJECT */
  res = binson_node_add_empty( obj, NULL, BINSON_TYPE_OBJECT, NULL, &obj->root );
  if (FAILED(res)) return res;

  return res;
}

/* \brief Allocate storage and attach new empty node to binson DOM tree
 *
 * \param obj binson*
 * \param parent binson_node*
 * \param node_type binson_node_type
 * \param key const char*
 * \param dst binson_node**
 * \return binson_res
 */
binson_res  binson_node_add_empty( binson *obj, binson_node *parent, binson_node_type node_type, const char* key, binson_node **dst )
{
  binson_node  *me;

  /* arg's validation */
  if (!obj || (!key && parent && (parent->type != BINSON_TYPE_ARRAY)))  /* missing key for non-array parent */
    return BINSON_RES_ERROR_ARG_WRONG;

   me = (binson_node *)calloc(1, sizeof(binson_node));

  if (!me)
    return BINSON_RES_ERROR_OUT_OF_MEMORY;

  /* prepare our node structure */
  me->type = node_type;
  me->parent = parent;


   /* allocating and cloning key */
   if (parent->type == BINSON_TYPE_ARRAY)  /* no keys for ARRAY children */
   {
     me->key = NULL;
   }
   else
   {
     me->key = (char*)malloc(strlen(key)+1);
     strcpy(me->key, key);
   }

   /* val field is zero initiated due to calloc call */

  me->next = NULL;

  /* connect new node to tree \
     [2DO] lexi order list insert instead of list add (for all parent types but ARRAY) */
  if (parent->val.children.last_child)  /* parent is not empty */
  {
    me->prev = parent->val.children.last_child;
    parent->val.children.last_child->next = me;
    parent->val.children.last_child = me;
  }
  else /* parent is  empty */
  {
    me->prev = NULL;
    parent->val.children.last_child = parent->val.children.first_child = me;
  }

  if (dst)
   *dst = me;

  return BINSON_RES_OK;
}

/* \brief Adds node with prefilled value in binson_node_val struct
 *
 * \param obj binson*
 * \param parent binson_node*
 * \param node_type binson_node_type
 * \param key const char*
 * \param dst binson_node**
 * \param tmp_val binson_node_val*
 * \return binson_res
 */
binson_res  binson_node_add( binson *obj, binson_node *parent, binson_node_type node_type, const char* key, binson_node **dst, binson_node_val *tmp_val )
{
  binson_node  *node_ptr = NULL;
  binson_res   res = binson_node_add_empty( obj, parent, node_type, key, &node_ptr );

  if (!SUCCESS(res))
    return res;

  if (dst)
    *dst = node_ptr;

  res = binson_node_copy_val( node_type, &(node_ptr->val), tmp_val );

  if (!SUCCESS(res))
    return res;

  /* just to be sure */
  node_ptr->val.children.first_child = NULL;
  node_ptr->val.children.last_child = NULL;

  return BINSON_RES_OK;
}

/* \brief Copy 'binson_node_val' structure, allocating memory for STRING and BYTES
 *
 * \param node_type binson_node_type
 * \param dst_val binson_node_val*
 * \param src_val binson_node_val*
 * \return binson_res
 */
binson_res  binson_node_copy_val( binson_node_type node_type, binson_node_val *dst_val, binson_node_val *src_val )
{
  memcpy( dst_val, src_val, sizeof(binson_node_val) );

  /* allocate new blocks.  'v_data.size'  value in  'src_val' arg must be valid */
  if (node_type == BINSON_TYPE_STRING || node_type == BINSON_TYPE_BYTES)
  {
    dst_val->v_data.ptr = (uint8_t*) malloc(src_val->v_data.size);

    if (!dst_val->v_data.ptr)
      return BINSON_RES_ERROR_OUT_OF_MEMORY;

    memcpy( dst_val->v_data.ptr, src_val->v_data.ptr, src_val->v_data.size );
  }

  return BINSON_RES_OK;
}

/* \brief Key lookup iterator (callback) to use with binson_traverse_*() functions
 *
 * \param obj binson*
 * \param node binson_node*
 * \param status binson_traverse_cb_status*
 * \param param void*
 * \return binson_res
 */
binson_res _binson_cb_lookup_key( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param )
{
   binson_traverse_cb_param *p = (binson_traverse_cb_param *)param;
   binson_child_num  requested_idx = p->in_param.idx;

   if (!obj || !node || !status || !param)
    return BINSON_RES_ERROR_ARG_WRONG;

   if (status->child_num == requested_idx)
   {
     p->out_param.node = node;
     status->done = true;  /* this force calling function to stop iterating; */
   }
   else
     p->out_param.node = NULL;

   return BINSON_RES_OK;
}

/* \brief Index lookup iterator (callback) to use with binson_traverse_*() functions
 *
 * \param obj binson*
 * \param node binson_node*
 * \param status binson_traverse_cb_status*
 * \param param void*
 * \return binson_res
 */
binson_res binson_cb_lookup_idx( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param )
{
   binson_traverse_cb_param *p = (binson_traverse_cb_param *)param;
   const char *requested_key = p->in_param.key;

   if (!obj || !node || !status || !param)
    return BINSON_RES_ERROR_ARG_WRONG;


   if (!strcmp(requested_key, node->key))
   {
     p->out_param.node = node;
     status->done = true;  /* this force calling function to stop iterating; */
   }
   else
     p->out_param.node = NULL;

   return BINSON_RES_OK;
}

/* \brief Node count iterator (callback) to use with binson_traverse_*() functions
 *
 * \param obj binson*
 * \param node binson_node*
 * \param status binson_traverse_cb_status*
 * \param param void*
 * \return binson_res
 */
binson_res binson_cb_count( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param )
{
  binson_traverse_cb_param *p = (binson_traverse_cb_param *)param;

   if (!obj || !node || !status || !param)
    return BINSON_RES_ERROR_ARG_WRONG;

  if (!status->done && status->current_node == status->root_node)  /* initialize counter in case of 1st iteration */
    p->out_param.node_num = 0;

  p->out_param.node_num++;

  return BINSON_RES_OK;
}

/* \brief Callback used to convert in-memory node to raw binson data or to it's JSON string representation
 *
 * \param obj binson*
 * \param node binson_node*
 * \param status binson_traverse_cb_status*
 * \param param void*
 * \return binson_res
 */
binson_res binson_cb_dump( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param )
{
  binson_traverse_cb_param *p = (binson_traverse_cb_param *)param;
  binson_res  res = BINSON_RES_OK;

  UNUSED(p);

  if (!obj || !node || !status || !status->current_node )
    return BINSON_RES_ERROR_ARG_WRONG;

  switch (node->type)
  {
    case BINSON_TYPE_OBJECT:
      if (status->dir == BINSON_TRAVERSE_DIR_UP)
        res = binson_writer_write_object_end( obj->writer );
      else
        res = binson_writer_write_object_begin( obj->writer, node->key );
    break;

    case BINSON_TYPE_ARRAY:
      if (status->dir == BINSON_TRAVERSE_DIR_UP)
        res = binson_writer_write_array_end( obj->writer );
      else
        res = binson_writer_write_array_begin( obj->writer, node->key );
    break;

    case BINSON_TYPE_BOOLEAN:
      res = binson_writer_write_boolean( obj->writer, node->key, node->val.b_data );
    break;

    case BINSON_TYPE_INTEGER:
      res = binson_writer_write_integer( obj->writer, node->key, node->val.i_data );
    break;

    case BINSON_TYPE_DOUBLE:
      res = binson_writer_write_double( obj->writer, node->key, node->val.d_data );
    break;

    case BINSON_TYPE_STRING:
      res = binson_writer_write_str( obj->writer, node->key, (const char*)node->val.v_data.ptr );
    break;

    case BINSON_TYPE_BYTES:
      res = binson_writer_write_bytes( obj->writer, node->key, node->val.v_data.ptr, node->val.v_data.size );
    break;

    case BINSON_TYPE_UNKNOWN:
    default:
      res = BINSON_RES_ERROR_TYPE_UNKNOWN;
    break;
  }

  return res;
}

/* \brief Callback used to node key against str key passed via param
 *
 * \param obj binson*
 * \param node binson_node*
 * \param status binson_traverse_cb_status*
 * \param param void*
 * \return binson_res
 */
binson_res binson_cb_key_compare( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param )
{
  binson_traverse_cb_param   *p = (binson_traverse_cb_param *)param;

  if (!obj || !node || !status || !param)
    return BINSON_RES_ERROR_ARG_WRONG;

  p->out_param.cmp_res = strcmp(node->key, p->in_param.key);  /* is ok for UTF-8 strings since strcmp() preserves lexicographic order */

  return BINSON_RES_OK;
}

/* \brief Callback used free node value's memory and finally node itself
 *
 * \param obj binson*
 * \param node binson_node*
 * \param status binson_traverse_cb_status*
 * \param param void*
 * \return binson_res
 */
binson_res binson_cb_remove( binson *obj, binson_node *node, binson_traverse_cb_status *status, void* param )
{
  binson_traverse_cb_param   *p = (binson_traverse_cb_param *)param;
  UNUSED(p);

  if (!obj || !node || !status)
    return BINSON_RES_ERROR_ARG_WRONG;

  /* frees node's key memory */
  if (node->key)
  {
     free(node->key);
     node->key = NULL;
  }

  /* frees node's value memory */
  if (node->val.v_data.ptr)
  {
     free(node->val.v_data.ptr);
     node->val.v_data.ptr = NULL;
  }

  /* frees node itself */
  free(node);

  return BINSON_RES_OK;
}

/* \brief Creates empty OBJECT node and connects it to specified parent
 *
 * \param obj binson*
 * \param parent binson_node*
 * \param key const char*
 * \param dst binson_node**
 * \return binson_res
 */
binson_res  binson_node_add_object_empty( binson *obj, binson_node *parent, const char* key, binson_node **dst )
{
  return  binson_node_add_empty( obj, parent, BINSON_TYPE_OBJECT, key, dst );
}

/* \brief Creates BOOLEAN node and connects it to specified parent
 *
 * \param obj binson*
 * \param parent binson_node*
 * \param key const char*
 * \param dst binson_node**
 * \param val bool
 * \return binson_res
 */
binson_res  binson_node_add_boolean( binson *obj, binson_node *parent, const char* key, binson_node **dst, bool val )
{
  binson_node_val  tmp_val;

  tmp_val.b_data = val;
  return binson_node_add( obj, parent, BINSON_TYPE_BOOLEAN, key, dst, &tmp_val );
}

/* \brief Creates INTEGER node and connects it to specified parent
 *
 * \param obj binson*
 * \param parent binson_node*
 * \param key const char*
 * \param dst binson_node**
 * \param val int64_t
 * \return binson_res
 */
binson_res  binson_node_add_integer( binson *obj, binson_node *parent,  const char* key, binson_node **dst, int64_t val )
{
  binson_node_val  tmp_val;

  tmp_val.i_data = val;
  return binson_node_add( obj, parent, BINSON_TYPE_INTEGER, key, dst, &tmp_val );
}

/* \brief Creates DOUBLE node and connects it to specified parent
 *
 * \param obj binson*
 * \param key binson_node *parentconst char*
 * \param dst binson_node**
 * \param val double
 * \return binson_res
 */
binson_res  binson_node_add_double( binson *obj, binson_node *parent, const char* key, binson_node **dst, double val )
{
  binson_node_val  tmp_val;

  tmp_val.d_data = val;
  return binson_node_add( obj, parent, BINSON_TYPE_DOUBLE, key, dst, &tmp_val );
}

/* \brief Creates STRING node and connects it to specified parent
 *
 * \param
 * \param
 * \return
 */
binson_res  binson_node_add_str( binson *obj, binson_node *parent, const char* key, binson_node **dst, const char* val )
{
  binson_node_val  tmp_val;

  tmp_val.v_data.ptr = (uint8_t*)val;
  tmp_val.v_data.size = strlen(val)+1;  /* need to copy trailing zero also */

  return binson_node_add( obj, parent, BINSON_TYPE_STRING, key, dst, &tmp_val );
}

/* \brief Creates BYTES node and connects it to specified parent
 *
 * \param obj binson*
 * \param parent binson_node*
 * \param key const char*
 * \param dst binson_node**
 * \param src_ptr uint8_t
 * \param src_size size_t
 * \return binson_res
 */
binson_res  binson_node_add_bytes( binson *obj, binson_node *parent, const char* key, binson_node **dst, uint8_t *src_ptr,  size_t src_size )
{
  binson_node_val  tmp_val;

  tmp_val.v_data.ptr = src_ptr;
  tmp_val.v_data.size = src_size;

  return binson_node_add( obj, parent, BINSON_TYPE_BYTES, key, dst, &tmp_val );
}

/* \brief Add new node which is a copy of specified node
 *
 * \param obj binson*
 * \param parent binson_node*
 * \param dst binson_node**
 * \param node binson_node*
 * \param new_key const char*
 * \return binson_res
 */
binson_res  binson_node_clone( binson *obj, binson_node *parent, binson_node **dst, binson_node *node, const char* new_key )
{
  return  binson_node_add( obj, parent, node->type, new_key, dst, &node->val );
}

/* \brief Creates empty ARRAY node and connects it to specified parent
 *
 * \param obj binson*
 * \param parent binson_node*
 * \param key const char*
 * \param dst binson_node**
 * \return binson_res
 */
binson_res  binson_node_add_array_empty( binson *obj, binson_node *parent, const char* key, binson_node **dst )
{
  return  binson_node_add_empty( obj, parent, BINSON_TYPE_ARRAY, key, dst );
}

/* \brief Remove subtree with specified node as root
 *
 * \param obj binson*
 * \param node binson_node*
 * \return binson_res
 */
binson_res  binson_node_remove( binson *obj, binson_node *node )
{
  return binson_traverse( obj, node, BINSON_TRAVERSE_POSTORDER, BINSON_DEPTH_LIMIT, binson_cb_remove, NULL );
}

/* \brief
 *
 * \param obj binson*
 * \return binson_res
 *
 */
binson_res  binson_serialize( binson *obj )
{
  return binson_traverse( obj, obj->root, BINSON_TRAVERSE_BOTHORDER, BINSON_DEPTH_LIMIT, binson_cb_dump, NULL );
}

/* \brief
 *
 * \param obj binson*
 * \param validate_only bool
 * \return binson_res
 *
 */
binson_res  binson_deserialize( binson *obj, bool validate_only )
{
  if (!obj)
    return BINSON_RES_ERROR_ARG_WRONG;

  UNUSED(validate_only);

  return BINSON_RES_ERROR_NOT_SUPPORTED;
}

/* \brief Begin tree traversal and process first available node
 *
 * \param
 * \param
 * \return
 */
binson_res  binson_traverse_begin( binson *obj, binson_node *root_node, binson_traverse_method t_method, int max_depth, \
                                     binson_traverse_callback cb, binson_traverse_cb_status *status, void* param )
{
    binson_res res = BINSON_RES_OK;

    if (status->done)
      return BINSON_RES_TRAVERSAL_DONE;

    if (obj && root_node)  /* first iteration */
    {
      /* store parameters to status structure */
      status->obj         = obj;
      status->root_node   = root_node;
      status->t_method    = t_method;
      status->max_depth   = max_depth;
      status->cb          = cb;
      status->param       = param;

      /* initialize state variables */
      status->current_node = root_node;

      status->dir         = BINSON_TRAVERSE_DIR_UNKNOWN;
      status->child_num   = 0;
      status->depth       = 0;
      status->done        = false;

      /* check if we need to process root node at first iteration */
      if (status->t_method != BINSON_TRAVERSE_POSTORDER)
        return status->cb( status->obj, status->current_node, status, status->param );
    }

    /* non-first iteration. */
/*    if (binson_node_is_leaf_type(status->current_node))  // last processed was leaf  */
    if (status->dir == BINSON_TRAVERSE_DIR_UP ||
        status->depth >= status->max_depth ||
        status->current_node->val.children.first_child)  /* there is no way down */
    {
      if (status->current_node->next) /* we can move right */
      {
          status->current_node = status->current_node->next;  /* update current node to it */
          status->dir = BINSON_TRAVERSE_DIR_RIGHT;
          status->child_num++;  /* keep track of index */

          /* processing moment doesn't depend on preorder/postorder for leaves */
          return status->cb( status->obj, status->current_node, status, status->param );
      }
      else /* no more neighbors from the right, moving up */
      {
          if (status->t_method != BINSON_TRAVERSE_PREORDER)  /* for postorder and 'bothorder' */
            res = status->cb( status->obj, status->current_node, status, status->param );   /* invoke callback for previous node  */

          if (status->current_node == status->root_node)  /* can't move up, we are at root */
          {
            status->done = true;
          }
          else  /* next node to process will be parent of the current node */
          {
            status->current_node = status->current_node->parent;
            status->dir         = BINSON_TRAVERSE_DIR_UP;
            status->depth--;
          }

           /* ??? check need or not
          //if (status->t_method != BINSON_TRAVERSE_PREORDER)  // for postorder and 'bothorder'
          //  return status->cb( status->obj, status->current_node, status, status->param );  */
      }
    }
    else  /* last processed has some children */
    {
       status->current_node = status->current_node->val.children.first_child;   /* select leftmost child */
       status->dir         = BINSON_TRAVERSE_DIR_DOWN;
       status->child_num   = 0;
       status->depth++;

      if (status->t_method != BINSON_TRAVERSE_POSTORDER)
        return status->cb( status->obj, status->current_node, status, status->param );
    }

    return res;
}

/* \brief Continue tree traversal and process next available
 *
 * \param status binson_traverse_cb_status*
 * \return binson_res
 */
binson_res  binson_traverse_next( binson_traverse_cb_status *status )
{
    /* to have code logic located in one place let's reuse 'binson_traverse_begin' function.
    / calling it with NULL pointers will make it work like 'binson_traverse_next'. */
    return binson_traverse_begin( NULL, NULL, status->t_method, status->max_depth, status->cb, status, status->param);
}

/* \brief No more nodes to process?
 *
 * \param status binson_traverse_cb_status*
 * \return bool
 */
bool  binson_traverse_is_done( binson_traverse_cb_status *status )
{
  return status->done;
}

/* \brief Which is current node being traversed?
 *
 * \param status binson_traverse_cb_status*
 * \return binson_node*
 */
binson_node*    binson_traverse_get_current_node( binson_traverse_cb_status *status )
{
  return status->current_node;
}

/* \brief Traverse all subtree with 'root_node' as subtree root
 *
 * \param
 * \param
 * \return
 */
binson_res  binson_traverse( binson *obj, binson_node *root_node, binson_traverse_method t_method, int max_depth, \
                               binson_traverse_callback cb, void* param )
{
  binson_traverse_cb_status  status;
  binson_res res = binson_traverse_begin( obj, root_node, t_method, max_depth, cb, &status, param );

  while (!binson_traverse_is_done(&status))
  {
    res = binson_traverse_next(&status);
  };

  return res;
}

/* \brief Free all memory used by binson object
 *
 * \param obj binson*
 * \return binson_res
 */
binson_res  binson_free( binson *obj )
{
  binson_res res = BINSON_RES_OK;

  if (obj->root)
    res = binson_node_remove( obj, obj->root );

  /*if (obj->str_stack)
    binson_strstack_release( obj->str_stack );
*/
  return res;
}

/* \brief Get root node pointer
 *
 * \param obj binson*
 * \param node_ptr binson_node**
 * \return binson_res
 */
binson_res  binson_get_root( binson *obj, binson_node  **node_ptr )
{
  if (obj == NULL || node_ptr == NULL)
    return BINSON_RES_ERROR_ARG_WRONG;

  *node_ptr = obj->root;

  return BINSON_RES_OK;
}

/* \brief Get node type
 *
 * \param node binson_node*
 * \return binson_node_type
 */
binson_node_type  binson_node_get_type( binson_node *node )
{
  return node->type;
}

/* \brief Get node key
 *
 * \param node binson_node*
 * \return const char*
 */
const char*    binson_node_get_key( binson_node *node )
{
  return node->key;
}

/* \brief Get node value struct pointer
 *
 * \param node binson_node*
 * \return binson_node_val*
 */
binson_node_val*    binson_node_get_val( binson_node *node )
{
  return &node->val;
}

/* \brief
 *
 * \param node binson_node*
 * \return bool
 */
bool binson_node_is_leaf_type( binson_node *node )
{
  return (node->type == BINSON_TYPE_OBJECT || node->type == BINSON_TYPE_ARRAY)? false:true;
}

/* \brief Get node's parent node
 *
 * \param node binson_node*
 * \return binson_node*
 */
binson_node*   binson_node_get_parent( binson_node *node )
{
  return node->parent;
}

/* \brief Get depth level of the node. Depth of root is 0.
 *
 * \param node binson_node*
 * \return int
 */
int  binson_node_get_depth(  binson_node *node )
{
   int depth = 0;

   while (node && node->parent)
   {
     node = node->parent;
     depth++;
   }

   return depth;
}

/* \brief Get previous (left) sibling of the node
 *
 * \param node binson_node*
 * \return binson_node*
 */
binson_node*    binson_node_get_prev( binson_node *node )
{
  return node->prev;
}

/* \brief Get next (right) sibling of the node
 *
 * \param node binson_node*
 * \return binson_node*
 */
binson_node*    binson_node_get_next( binson_node *node )
{
  return node->next;
}

/* \brief Get most left sibling of the node
 *
 * \param node binson_node*
 * \return binson_node*
 */
binson_node*  binson_node_get_first_sibling( binson_node *node )
{
   if (!node || !node->parent || (node == node->parent->val.children.first_child))
     return NULL;

   return node->parent->val.children.first_child;
}

/* \brief Get most right sibling of the node
 *
 * \param node binson_node*
 * \return binson_node*
 */
binson_node*  binson_node_get_last_sibling( binson_node *node )
{
  if (!node || !node->parent || (node == node->parent->val.children.last_child))
     return NULL;

   return node->parent->val.children.last_child;
}

/* \brief Get first child of the node
 *
 * \param node binson_node*
 * \return binson_node*
 */
binson_node*  binson_node_get_first_child( binson_node *node )
{
  if (!node || binson_node_is_leaf_type(node))
    return NULL;

  return node->val.children.first_child;
}

/* \brief Get last child of the node
 *
 * \param node binson_node*
 * \return binson_node*
 */
binson_node*  binson_node_get_last_child( binson_node *node )
{
  if (!node || binson_node_is_leaf_type(node))
    return NULL;

  return node->val.children.last_child;
}