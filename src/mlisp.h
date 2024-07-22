
#ifndef MLISP_H
#define MLISP_H

#ifndef MLISP_TOKEN_SZ_MAX
#  define MLISP_TOKEN_SZ_MAX 4096
#endif /* !MLISP_TOKEN_SZ_MAX */

#ifndef MLISP_STACK_DEPTH_MAX
#  define MLISP_STACK_DEPTH_MAX 100
#endif /* !MLISP_STACK_DEPTH_MAX */

#ifndef MLISP_TRACE_LVL
#  define MLISP_TRACE_LVL 0
#endif /* !MLISP_TRACE_LVL */

#ifndef MLISP_AST_IDX_CHILDREN_MAX
#  define MLISP_AST_IDX_CHILDREN_MAX 2
#endif /* !MLISP_AST_IDX_CHILDREN_MAX */

#define MLISP_AST_FLAG_ACTIVE 0x01

#define MLISP_ENV_TYPE_TABLE( f ) \
   f( 1, ssize_t,        str_table_index, STR,     "%s", &(str_table[val->str_table_index]) ) \
   f( 2, mlisp_env_cb_t, cb,              CB,      "%p", val->cb ) \
   f( 3, int16_t,        integer,         INT,     "%d", val->integer ) \
   f( 4, float,          floating,        FLOAT,   "%f", val->floating )

#define MLISP_PARSER_PSTATE_TABLE( f ) \
   f( MLISP_PSTATE_NONE, 0 ) \
   f( MLISP_PSTATE_SYMBOL_OP, 1 ) \
   f( MLISP_PSTATE_SYMBOL, 2 ) \
   f( MLISP_PSTATE_STRING, 3 )

union MLISP_VAL;

typedef MERROR_RETVAL (*mlisp_env_cb_t)(
   union MLISP_VAL* out, union MLISP_VAL* in, size_t in_sz );

#define _MLISP_ENV_TYPE_TABLE_FIELDS( idx, ctype, name, const_name, fmt, iv ) \
   ctype name;

union MLISP_VAL {
   MLISP_ENV_TYPE_TABLE( _MLISP_ENV_TYPE_TABLE_FIELDS );
};

struct MLISP_ENV_NODE {
   uint8_t type;
   ssize_t name_str_table_index;
   union MLISP_VAL value;
};

struct MLISP_STACK_NODE {
   uint8_t type;
   union MLISP_VAL value;
};

struct MLISP_AST_NODE {
   size_t token_idx;
   size_t token_sz;
   /**
    * \brief Next child to evaluate in MLISP_AST_NODE::ast_idx_children when
    *        _mlisp_step_iter() is called on this node.
    * \todo Don't like keeping this state in the AST... need a parallel
    *       structure with a stack and nested step counter state.
    */
   ssize_t step_iter;
   /*! \brief Number of children in MLISP_AST_NODE::ast_idx_children. */
   ssize_t step_iter_max;
   ssize_t ast_idx_parent;
   ssize_t env_idx_op;
   ssize_t ast_idx_children[MLISP_AST_IDX_CHILDREN_MAX];
};

struct MLISP_PARSER {
   struct MPARSER base;
   struct MDATA_STRTABLE str_table;
   MAUG_MHANDLE env_h;
   size_t env_sz;
   size_t env_sz_max;
   struct MDATA_VECTOR ast;
   struct MDATA_VECTOR env;
   ssize_t ast_node_iter;
   ssize_t ast_idx_children[MLISP_AST_IDX_CHILDREN_MAX];
   struct MLISP_STACK_NODE stack[MLISP_STACK_DEPTH_MAX];
   size_t stack_idx;
};

#define mlisp_parser_pstate( parser ) \
   ((parser)->base.pstate_sz > 0 ? \
      (parser)->base.pstate[(parser)->base.pstate_sz - 1] : MLISP_PSTATE_NONE)

#ifdef MPARSER_TRACE_NAMES
#  define mlisp_parser_pstate_push( parser, new_pstate ) \
      mparser_pstate_push( \
         "mlisp", &((parser)->base), new_pstate, gc_mlisp_pstate_names );

#  define mlisp_parser_pstate_pop( parser ) \
      mparser_pstate_pop( \
         "mlisp", &((parser)->base), gc_mlisp_pstate_names );
#else
#  define mlisp_parser_pstate_push( parser, new_pstate ) \
      mparser_pstate_push( "mlisp", &((parser)->base), new_pstate )

#  define mlisp_parser_pstate_pop( parser ) \
      mparser_pstate_pop( "mlisp", &((parser)->base) )
#endif /* MPARSER_TRACE_NAMES */

#define mlisp_parser_invalid_c( parser, c, retval ) \
   mparser_invalid_c( mlisp, &((parser)->base), c, retval )

#define mlisp_parser_reset_token( parser ) \
   mparser_reset_token( "mlisp", &((parser)->base) )

#define mlisp_parser_append_token( parser, c ) \
   mparser_append_token( "mlisp", &((parser)->base), c )

#define mlisp_parser_parse_token( parser ) \
   parser->token_parser( \
      (parser)->token, (parser)->token_sz, (parser)->token_parser_arg )

MERROR_RETVAL mlisp_ast_dump(
   struct MLISP_PARSER* parser, size_t ast_node_idx, size_t depth, char ab );

MERROR_RETVAL mlisp_parse_c( struct MLISP_PARSER* parser, char c );

MERROR_RETVAL mlisp_parser_init( struct MLISP_PARSER* parser );

void mlisp_parser_free( struct MLISP_PARSER* parser );

#ifdef MLISP_C

#define _MLISP_ENV_TYPE_TABLE_CONSTS( idx, ctype, name, const_name, fmt, iv ) \
   MAUG_CONST uint8_t SEG_MCONST MLISP_ENV_TYPE_ ## const_name = idx;

MLISP_ENV_TYPE_TABLE( _MLISP_ENV_TYPE_TABLE_CONSTS );

MLISP_PARSER_PSTATE_TABLE( MPARSER_PSTATE_TABLE_CONST )

MPARSER_PSTATE_NAMES( MLISP_PARSER_PSTATE_TABLE, mlisp )

static MERROR_RETVAL
_mlisp_ast_add_child( struct MLISP_PARSER* parser ) {
   MERROR_RETVAL retval = MERROR_OK;
   struct MLISP_AST_NODE* n_parent = NULL;
   struct MLISP_AST_NODE ast_node;
   ssize_t parent_child_idx = -1;
   ssize_t new_idx_out = 0;
   size_t i = 0;

   mdata_vector_lock( &(parser->ast) );

   /* Setup the new node to copy. */
   maug_mzero( &ast_node, sizeof( struct MLISP_AST_NODE ) );
   ast_node.ast_idx_parent = parser->ast_node_iter;
   for( i = 0 ; MLISP_AST_IDX_CHILDREN_MAX > i ; i++ ) {
      ast_node.ast_idx_children[i] = -1;
   }
   ast_node.token_idx = -1;
   ast_node.step_iter = 0;
   ast_node.step_iter_max = 0;

   /* Add the node to the AST and set it as the current node. */
   new_idx_out = mdata_vector_append(
      &(parser->ast), &ast_node, sizeof( struct MLISP_AST_NODE ) );
   if( 0 > new_idx_out ) {
      retval = mdata_retval( new_idx_out );
   }

   /* Find an available child slot on the parent, if there is one. */
   if( 0 <= ast_node.ast_idx_parent ) {
      n_parent = mdata_vector_get(
         &(parser->ast), ast_node.ast_idx_parent, struct MLISP_AST_NODE );

      /* Find the first free child slot. */
      parent_child_idx = 0;
      while( -1 != n_parent->ast_idx_children[parent_child_idx] ) {
         parent_child_idx++;
      }

      n_parent->ast_idx_children[parent_child_idx] = new_idx_out;
      n_parent->step_iter_max++;
   } else {
      /* Find the first free child slot *on the parser*. */
      parent_child_idx = 0;
      while( -1 != parser->ast_idx_children[parent_child_idx] ) {
         parent_child_idx++;
      }

      parser->ast_idx_children[parent_child_idx] = new_idx_out;
   }

   parser->ast_node_iter = new_idx_out;

   debug_printf( MLISP_TRACE_LVL, "setup node " SSIZE_T_FMT
      " under parent: " SSIZE_T_FMT " as child " SSIZE_T_FMT,
      new_idx_out, ast_node.ast_idx_parent, parent_child_idx );

cleanup:

   mdata_vector_unlock( &(parser->ast) );

   return retval;
}

/* === */

static MERROR_RETVAL _mlisp_ast_set_child_token(
   struct MLISP_PARSER* parser, ssize_t token_idx, size_t token_sz
) {
   MERROR_RETVAL retval = MERROR_OK;
   char* str_table = NULL;
   struct MLISP_AST_NODE* n = NULL;

   mdata_vector_lock( &(parser->ast) );

   n = mdata_vector_get(
      &(parser->ast), parser->ast_node_iter, struct MLISP_AST_NODE );

   /* Debug report. */
   mdata_strtable_lock( &(parser->str_table), str_table );
   assert( 0 < strlen( &(str_table[token_idx]) ) );
   debug_printf( MLISP_TRACE_LVL, "setting node " SSIZE_T_FMT
      " token: \"%s\"",
      parser->ast_node_iter, &(str_table[token_idx]) );
   mdata_strtable_unlock( &(parser->str_table), str_table );

   /* Set the token from the str_table. */
   n->token_idx = token_idx;
   n->token_sz = token_sz;

cleanup:

   mdata_vector_unlock( &(parser->ast) );

   return retval;
}

/* === */

static
MERROR_RETVAL _mlisp_ast_traverse_parent( struct MLISP_PARSER* parser ) {
   MERROR_RETVAL retval = MERROR_OK;
   struct MLISP_AST_NODE* n = NULL;

   mdata_vector_lock( &(parser->ast) );

   assert( 0 <= parser->ast_node_iter );

   n = mdata_vector_get(
      &(parser->ast), parser->ast_node_iter, struct MLISP_AST_NODE );

   parser->ast_node_iter = n->ast_idx_parent;

   debug_printf( MLISP_TRACE_LVL, "moved up to node: " SSIZE_T_FMT,
      parser->ast_node_iter );

cleanup:

   mdata_vector_unlock( &(parser->ast) );

   return retval;
}

/* === */

MERROR_RETVAL mlisp_ast_dump(
   struct MLISP_PARSER* parser, size_t ast_node_idx, size_t depth, char ab
) {
   MERROR_RETVAL retval = MERROR_OK;
   uint8_t autolock = 0;
   struct MLISP_AST_NODE* n = NULL;
   char indent[101];
   size_t i = 0;
   char* str_table = NULL;

   if( NULL == parser->ast.data_bytes ) {
      autolock = 1;
      mdata_vector_lock( &(parser->ast) );
      debug_printf( MLISP_TRACE_LVL, "--- BEGIN AST DUMP ---" );
   }

   /* Make indent. */
   maug_mzero( indent, 101 );
   assert( depth < 100 );
   for( i = 0 ; depth > i ; i++ ) {
      indent[i] = ' ';
   }

   if( 0 == ab ) {
      ab = 'X';
   }

   /* Iterate node and children .*/
   n = mdata_vector_get( &(parser->ast), ast_node_idx, struct MLISP_AST_NODE );
   mdata_strtable_lock( &(parser->str_table), str_table );
   debug_printf( MLISP_TRACE_LVL,
      "%s%c: \"%s\" (i: " SIZE_T_FMT ", t: " SIZE_T_FMT ", s: "
         SSIZE_T_FMT ", s_m: " SSIZE_T_FMT ")",
      indent, ab, &(str_table[n->token_idx]),
      ast_node_idx, n->token_idx, n->step_iter, n->step_iter_max );
   mdata_strtable_unlock( &(parser->str_table), str_table );
   for( i = 0 ; MLISP_AST_IDX_CHILDREN_MAX > i ; i++ ) {
      if( -1 == n->ast_idx_children[i] ) {
         continue;
      }

      mlisp_ast_dump( parser, n->ast_idx_children[i], depth + 1, '0' + i );
   }

cleanup:

   if( NULL != parser->ast.data_bytes && autolock ) {
      mdata_vector_unlock( &(parser->ast) );
      debug_printf( MLISP_TRACE_LVL, "--- END AST DUMP ---" );
   }

   return retval;
}

/* === */

MERROR_RETVAL mlisp_stack_dump( struct MLISP_PARSER* parser ) {
   MERROR_RETVAL retval = MERROR_OK;
   size_t i = 0;
   char* str_table = NULL;
   union MLISP_VAL* val = NULL;

#  define _MLISP_ENV_TYPE_TABLE_DUMP( idx, ctype, name, const_name, fmt, iv ) \
      if( MLISP_ENV_TYPE_ ## const_name == parser->stack[i].type ) { \
         val = &(parser->stack[i].value); \
         debug_printf( MLISP_TRACE_LVL, \
            "stack " SIZE_T_FMT " (" #const_name "): " fmt, \
            i, iv ); \
      }

   mdata_strtable_lock( &(parser->str_table), str_table ); \
   while( i < parser->stack_idx ) {
      MLISP_ENV_TYPE_TABLE( _MLISP_ENV_TYPE_TABLE_DUMP );
      i++;
   }
   mdata_strtable_unlock( &(parser->str_table), str_table ); \

cleanup:

   return retval;
}

/* === */

MERROR_RETVAL _mlisp_ast_add_raw_token( struct MLISP_PARSER* parser ) {
   MERROR_RETVAL retval = MERROR_OK;
   ssize_t str_idx = -1;

   str_idx = mdata_strtable_append( &(parser->str_table),
      parser->base.token, parser->base.token_sz );
   if( 0 > str_idx ) {
      error_printf( "invalid str_idx: " SSIZE_T_FMT, str_idx );
      retval = MERROR_ALLOC;
      goto cleanup;
   }

   _mlisp_ast_add_child( parser );
   _mlisp_ast_set_child_token( parser, str_idx, parser->base.token_sz );
   mlisp_parser_reset_token( parser );
   retval = _mlisp_ast_traverse_parent( parser );

cleanup:
   return retval;
}

/* === */

#define _MLISP_ENV_TYPE_TABLE_PUSH( idx, ctype, name, const_name, fmt, iv ) \
   MERROR_RETVAL mlisp_stack_push_ ## name( \
      struct MLISP_PARSER* parser, ctype i \
   ) { \
      MERROR_RETVAL retval = MERROR_OK; \
      char* str_table = NULL; \
      union MLISP_VAL* val = NULL; \
      if( parser->stack_idx + 1 >= MLISP_STACK_DEPTH_MAX ) { \
         error_printf( "stack overflow!" ); \
         retval = MERROR_OVERFLOW; \
         goto cleanup; \
      } \
      parser->stack[parser->stack_idx].type = MLISP_ENV_TYPE_ ## const_name; \
      parser->stack[parser->stack_idx].value.name = i; \
      mdata_strtable_lock( &(parser->str_table), str_table ); \
      val = &(parser->stack[parser->stack_idx].value); \
      debug_printf( MLISP_TRACE_LVL, "pushing " #const_name ": " fmt, iv ); \
      mdata_strtable_unlock( &(parser->str_table), str_table ); \
      parser->stack_idx++; \
   cleanup: \
      return retval; \
   }

MLISP_ENV_TYPE_TABLE( _MLISP_ENV_TYPE_TABLE_PUSH );

/* === */


#if 0
MERROR_RETVAL mlisp_stack_push_str(
   struct MLISP_PARSER* parser, const char* s
) {
   MERROR_RETVAL retval = MERROR_OK;
   ssize_t str_idx = -1;

   if( parser->stack_idx + 1 >= MLISP_STACK_DEPTH_MAX ) {
      error_printf( "stack overflow!" );
      retval = MERROR_OVERFLOW;
      goto cleanup;
   }

   debug_printf( MLISP_TRACE_LVL, "pushing STR: %s", s );

   str_idx = mdata_strtable_append( &(parser->str_table), s, 0 );
   if( 0 > str_idx ) {
      goto cleanup;
   }

   parser->stack[parser->stack_idx].type = MLISP_ENV_TYPE_STR;
   parser->stack[parser->stack_idx].value.str_table_index = str_idx;

cleanup:

   return retval;
}
#endif

/* === */

static MERROR_RETVAL _mlisp_step_iter(
   struct MLISP_PARSER* parser, struct MLISP_AST_NODE* n
) {
   MERROR_RETVAL retval = MERROR_OK;
   char* str_table = NULL;
   int16_t i_tmp = 0;
   uint8_t is_num = 0;

   if( n->step_iter_max > n->step_iter ) {
      /* Call the next uncalled child. */
      retval = _mlisp_step_iter( parser,
         mdata_vector_get( &(parser->ast), n->ast_idx_children[n->step_iter],
            struct MLISP_AST_NODE ) );
      if( MERROR_EXEC != retval ) {
         mdata_strtable_lock( &(parser->str_table), str_table );
         assert( 0 < strlen( &(str_table[n->token_idx]) ) );
         debug_printf( 1, SSIZE_T_FMT " increment: %s",
            n->step_iter, &(str_table[n->token_idx]) );
         mdata_strtable_unlock( &(parser->str_table), str_table );

         /* Increment this node, since the child actually executed. */
         n->step_iter++;
      }
      /* Could not exec *this* node yet, so don't increment its parent. */
      retval = MERROR_EXEC;
      goto cleanup;
   }

   /* Evaluate this node, assuming all the previously called children are on
    * the stack.
    */
   /* TODO */

   mdata_strtable_lock( &(parser->str_table), str_table );
   assert( 0 < strlen( &(str_table[n->token_idx]) ) );
   debug_printf( 1, SSIZE_T_FMT " exec: %s",
      n->step_iter, &(str_table[n->token_idx]) );
   if( maug_is_num( &(str_table[n->token_idx]), n->token_sz ) ) {
      is_num = 1;
      i_tmp = atoi( &(str_table[n->token_idx]) );
   }
   mdata_strtable_unlock( &(parser->str_table), str_table );

   /* TODO: Figure out: number? env? string? */
   if( is_num ) {
      mlisp_stack_push_integer( parser, i_tmp );
   } else {
      mlisp_stack_push_str_table_index( parser, n->token_idx );
   }

cleanup:

   return retval;
}

/* === */

MERROR_RETVAL mlisp_step( struct MLISP_PARSER* parser ) {
   MERROR_RETVAL retval = MERROR_OK;
   struct MLISP_AST_NODE* n = NULL;

   mdata_vector_lock( &(parser->ast) );

   if( 0 > parser->ast_node_iter ) {
      error_printf( "invalid AST node: " SSIZE_T_FMT, parser->ast_node_iter );
      retval = MERROR_EXEC;
      goto cleanup;
   }

   n = mdata_vector_get(
      &(parser->ast), parser->ast_node_iter, struct MLISP_AST_NODE );
   assert( NULL != n );

   /* Find next unevaluated symbol. */
   retval = _mlisp_step_iter( parser, n );
   if( MERROR_EXEC == retval ) {
      retval = 0;
   } else if( 0 == retval ) {
      retval = MERROR_EXEC;
   }
  
cleanup:

   debug_printf( MLISP_TRACE_LVL, "step: %x", retval );

   mdata_vector_unlock( &(parser->ast) );

   return retval;
}

/* === */

MERROR_RETVAL mlisp_parse_c( struct MLISP_PARSER* parser, char c ) {
   MERROR_RETVAL retval = MERROR_OK;
   ssize_t str_idx = -1;

   debug_printf( MLISP_TRACE_LVL,
      SIZE_T_FMT ": \"%c\" (last: \"%c\") (%s (%d)) (sz: " SIZE_T_FMT ")",
      parser->base.i, c, parser->base.last_c,
      gc_mlisp_pstate_names[mlisp_parser_pstate( parser )],
      mlisp_parser_pstate( parser ),
      parser->base.pstate_sz );

   switch( c ) {
   case '\r':
   case '\n':
   case '\t':
   case ' ':
      if(
         MLISP_PSTATE_SYMBOL_OP == mlisp_parser_pstate( parser )
         /* Don't terminate the current symbol if the last_c was *any* of the
          * other terminating characters.
          */
         && '\r' != parser->base.last_c
         && '\n' != parser->base.last_c
         && '\t' != parser->base.last_c
         && ' ' != parser->base.last_c
         && ')' != parser->base.last_c
         && '(' != parser->base.last_c
      ) {
         assert( 0 < parser->base.token_sz );
         debug_printf( MLISP_TRACE_LVL, "found symbol: %s (" SIZE_T_FMT ")",
            parser->base.token, parser->base.token_sz );

         /* Grab the symbol to use for the op of the child created by the last
          * open paren.
          */
         str_idx = mdata_strtable_append( &(parser->str_table),
            parser->base.token, parser->base.token_sz );
         mlisp_parser_reset_token( parser );
         _mlisp_ast_set_child_token( parser, str_idx, parser->base.token_sz );

         /* Switch from OP to SYMBOL for subsequent tokens. */
         mlisp_parser_pstate_pop( parser );
         retval = mlisp_parser_pstate_push( parser, MLISP_PSTATE_SYMBOL );
         maug_cleanup_if_not_ok();

         mlisp_ast_dump( parser, 0, 0, 0 );

      } else if(
         MLISP_PSTATE_SYMBOL == mlisp_parser_pstate( parser )
         /* Don't terminate the current symbol if the last_c was *any* of the
          * other terminating characters.
          */
         && '\r' != parser->base.last_c
         && '\n' != parser->base.last_c
         && '\t' != parser->base.last_c
         && ' ' != parser->base.last_c
         && ')' != parser->base.last_c
         && '(' != parser->base.last_c
      ) {
         assert( 0 < parser->base.token_sz );
         debug_printf( MLISP_TRACE_LVL, "found symbol: %s (" SIZE_T_FMT ")",
            parser->base.token, parser->base.token_sz );

         /* A raw token without parens terminated by whitespace can't have
          * children, so just create a one-off.
          */
         _mlisp_ast_add_raw_token( parser );

      } else if( MLISP_PSTATE_STRING == mlisp_parser_pstate( parser ) ) {
         retval = mlisp_parser_append_token( parser, c );
         maug_cleanup_if_not_ok();

      }
      break;

   case '(':
      if(
         MLISP_PSTATE_NONE == mlisp_parser_pstate( parser ) ||
         MLISP_PSTATE_SYMBOL == mlisp_parser_pstate( parser )
      ) {
         /* First symbol after an open paren is an op. */
         retval = mlisp_parser_pstate_push( parser, MLISP_PSTATE_SYMBOL_OP );
         maug_cleanup_if_not_ok();
         mlisp_parser_reset_token( parser );

         /* Add a new empty child to be filled out when tokens are parsed. */
         _mlisp_ast_add_child( parser );

      } else if( MLISP_PSTATE_STRING == mlisp_parser_pstate( parser ) ) {
         retval = mlisp_parser_append_token( parser, c );
         maug_cleanup_if_not_ok();

      } else {
         mlisp_parser_invalid_c( parser, c, retval );
      }
      break;

   case ')':
      if(
         MLISP_PSTATE_SYMBOL_OP == mlisp_parser_pstate( parser ) ||
         MLISP_PSTATE_SYMBOL == mlisp_parser_pstate( parser )
      ) {
         if( 0 < parser->base.token_sz ) {
            /* A raw token without parens terminated by whitespace can't have
            * children, so just create a one-off.
            */
            _mlisp_ast_add_raw_token( parser );
         }

         /* Reset the parser and AST cursor up one level. */
         mlisp_parser_pstate_pop( parser );
         _mlisp_ast_traverse_parent( parser );

         mlisp_ast_dump( parser, 0, 0, 0 );

      } else if( MLISP_PSTATE_STRING == mlisp_parser_pstate( parser ) ) {
         retval = mlisp_parser_append_token( parser, c );
         maug_cleanup_if_not_ok();

      } else {
         mlisp_parser_invalid_c( parser, c, retval );
      }
      break;

   default:
      retval = mlisp_parser_append_token( parser, c );
      maug_cleanup_if_not_ok();
      break;
   }

   mparser_wait( &(parser->base) );

   parser->base.i++;

cleanup:

   parser->base.last_c = c;

   return retval;
}

/* === */

MERROR_RETVAL mlisp_add_env(
   struct MLISP_PARSER* parser, const char* token, size_t token_sz,
   uint8_t env_type, void* data
) {
   MERROR_RETVAL retval = MERROR_OK;
   struct MLISP_ENV_NODE env_node;
   ssize_t new_idx_out = -1;

#  define _MLISP_ENV_TYPE_TABLE_ASGN( idx, ctype, name, const_name, fmt, iv ) \
      case idx: \
         memcpy( &(env_node.value.name), data, sizeof( ctype ) ); \
         break;

   mdata_vector_lock( &(parser->env) );

   /* Setup the new node to copy. */
   maug_mzero( &env_node, sizeof( struct MLISP_ENV_NODE ) );
   env_node.name_str_table_index =
      mdata_strtable_append( &(parser->str_table), token, token_sz );
   if( 0 > new_idx_out ) {
      retval = mdata_retval( env_node.name_str_table_index );
   }
   maug_cleanup_if_not_ok();
   switch( env_type ) {
      MLISP_ENV_TYPE_TABLE( _MLISP_ENV_TYPE_TABLE_ASGN );
   }

   /* Add the node to the env. */
   new_idx_out = mdata_vector_append(
      &(parser->env), &env_node, sizeof( struct MLISP_ENV_NODE ) );
   if( 0 > new_idx_out ) {
      retval = mdata_retval( new_idx_out );
   }
   maug_cleanup_if_not_ok();

   debug_printf( MLISP_TRACE_LVL, "setup env node " SSIZE_T_FMT ": %s",
      new_idx_out, token );

cleanup:

   mdata_vector_unlock( &(parser->env) );

   return retval;
}

/* === */

MERROR_RETVAL _mlisp_env_cb_multiply(
   union MLISP_VAL* out, union MLISP_VAL* in, size_t in_sz
) {
   MERROR_RETVAL retval = MERROR_OK;

   return retval;
}

/* === */

MERROR_RETVAL mlisp_parser_init( struct MLISP_PARSER* parser ) {
   MERROR_RETVAL retval = MERROR_OK;
   ssize_t append_retval = 0;
   size_t i = 0;

   debug_printf( MLISP_TRACE_LVL,
      "initializing mlisp parser (" SIZE_T_FMT " bytes)...",
      sizeof( struct MLISP_PARSER ) );

   maug_mzero( parser, sizeof( struct MLISP_PARSER ) );

   parser->ast_node_iter = -1;
   for( i = 0 ; MLISP_AST_IDX_CHILDREN_MAX > i ; i++ ) {
      parser->ast_idx_children[i] = -1;
   }

   /* Allocate the vectors for AST and ENV. */
 
   append_retval = mdata_vector_append(
      &(parser->ast), NULL, sizeof( struct MLISP_AST_NODE ) );
   if( 0 > append_retval ) {
      retval = mdata_retval( append_retval );
   }
   maug_cleanup_if_not_ok();

   append_retval = mdata_vector_append(
      &(parser->env), NULL, sizeof( struct MLISP_ENV_NODE ) );
   if( 0 > append_retval ) {
      retval = mdata_retval( append_retval );
   }
   maug_cleanup_if_not_ok();

   /* Setup initial env. */

   retval = mlisp_add_env(
      parser, "*", 1, MLISP_ENV_TYPE_CB, _mlisp_env_cb_multiply );

cleanup:

   if( MERROR_OK != retval ) {
      error_printf( "mlisp parser initialization failed: %d", retval );
   }

   return retval;
}

/* === */

void mlisp_parser_free( struct MLISP_PARSER* parser ) {
   mdata_strtable_free( &(parser->str_table) );
   mdata_vector_free( &(parser->ast) );
   mdata_vector_free( &(parser->env) );
   /* maug_mfree( parser->ast_h ); */
   /* TODO */
}

#else

#  define MLISP_PSTATE_TABLE_CONST( name, idx ) \
      extern MAUG_CONST uint8_t SEG_MCONST name;

MLISP_PARSER_PSTATE_TABLE( MLISP_PSTATE_TABLE_CONST )

#ifdef MPARSER_TRACE_NAMES
extern MAUG_CONST char* SEG_MCONST gc_mlisp_pstate_names[];
#endif /* MPARSER_TRACE_NAMES */

#endif /* MLISP_C */

#endif /* !MLISP_H */

