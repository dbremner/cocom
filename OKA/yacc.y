/*
   FILE NAME:   yacc.y

   Copyright (C) 1997-2007 Vladimir Makarov.

   Written by Vladimir Makarov <vmakarov@users.sourceforge.net>

   This file is part of the tool OKA.

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

   TITLE:       Parser of OKA (pipeline hazards description translator)
                described as YACC file

   DESCRIPTION: This file makes lexical, syntactic analysis
                (with possible syntactic error recovery) of a file and
                builds up internal representation of parsed pipeline
                hazards description.

   SPECIAL CONSIDERATION:
         Defining macro `NDEBUG' (e.g. by option `-D' in C compiler
       command line) during the file compilation disables to fix
       some internal errors of parser work and errors of its usage.

*/

%{

#ifdef HAVE_CONFIG_H
#include "cocom-config.h"
#else /* In this case we are oriented to ANSI C */
#ifndef HAVE_ASSERT_H
#define HAVE_ASSERT_H
#endif
#endif /* #ifdef HAVE_CONFIG_H */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "position.h"
#include "errors.h"
#include "ird.h"
#include "common.h"
#include "tab.h"
#include "yacc.h"

#ifdef HAVE_ASSERT_H
#include <assert.h>
#else
#ifndef assert
#define assert(code) do { if (code == 0) abort ();} while (0)
#endif
#endif



/* The following variables and forward definitions of functions are
   used in actions of YACC grammar. */

/* The following variable value is begin position of the last lexema returned
   by the function `yylex'. */

static position_t current_lexema_position;

static void syntax_error (const char *s);

%}


/* Attributes of all YACC symbols (token and nonterminals) will be
   `node'.  Some construction YACC action will be `position'. */

%union
{
  IR_node_t node;
  position_t position;
  integer_t inumber;
}

/* The attributes of the following tokens are not defined and not used. */

%token PERCENTS COMMA COLON SEMICOLON LEFT_PARENTHESIS RIGHT_PARENTHESIS
       LEFT_BRACKET RIGHT_BRACKET LEFT_ANGLE_BRACKET RIGHT_ANGLE_BRACKET
       PLUS BAR STAR
       LOCAL IMPORT EXPORT AUTOMATON UNIT EXCLUSION
       NOTHING INPUT RESULT INSTRUCTION RESERVATION 

/* The attributes of the following tokens are defined and used. */

%token <node> IDENTIFIER NUMBER CODE_INSERTION ADDITIONAL_C_CODE

%type <node>  declaration_part  declaration  identifier_declaration
              instruction_declaration reservation_declaration
              unit_declaration optional_automaton_identifier
              automaton_declaration  exclusion_clause  identifier_list
              expression_definition_list  expression_definition
              instruction_or_reservation_identifier_list
              instruction_or_reservation_identifier  expression
              unit_or_reservation_identifier

%type <inumber> input_result_number

%left BAR
%left PLUS
      IDENTIFIER NOTHING INPUT RESULT
      LEFT_PARENTHESIS LEFT_BRACKET /* for concatenation. */
%left STAR

%start description

%%

description : declaration_part  PERCENTS
                {
                  $<position>$ = current_lexema_position;
                }
              expression_definition_list  ADDITIONAL_C_CODE
                {
                  IR_node_t first_declaration;
                  IR_node_t first_expression_definition;
                  
                  /* Transform cyclic declaration list to usual list. */
                  if ($1 != NULL)
                    {
                      first_declaration = IR_next_declaration ($1);
                      IR_set_next_declaration ($1, NULL);
                      $1 = first_declaration;
                    }
                  /* Transform cyclic expression definition list to
                     usual list. */
                  if ($4 != NULL)
                    {
                      first_expression_definition
                        = IR_next_expression_definition ($4);
                      IR_set_next_expression_definition ($4, NULL);
                      $4 = first_expression_definition;
                    }
                  description
                    = IR_new_description
                      (($1 == NULL ? $<position>3 : IR_position ($1)),
                       $1, $4, $5);
                }
            ;

/* The attribute of the following nonterminal is the last declaration
   of given declaration list.  The declaration list is cyclic. */

declaration_part :                                  {$$ = NULL;}
                 | declaration_part  declaration
                     {
                       if ($1 == NULL)
                         /* Make cycle. */
                         IR_set_next_declaration ($2, $2);
                       else
                         {
                           /* Add element to cyclic list */
                           IR_set_next_declaration ($2,
                                                    IR_next_declaration ($1));
                           IR_set_next_declaration ($1, $2);
                         }
                       $$ = $2;
                     }
                 | declaration_part  error
                     {
                       syntax_error ("syntax error in declaration");
                       $$ = $1;  /* Ignore error declaration. */
                     }
                 ;

declaration : identifier_declaration
                {
                  IR_node_t first_identifier;

                  if (IR_identifier_list ($1) != NULL)
                    {
                      /* Transform cyclic list to usual list. */
                      first_identifier
                        = IR_next_identifier (IR_identifier_list ($1));
                      IR_set_next_identifier (IR_identifier_list ($1), NULL);
                      IR_set_identifier_list ($1, first_identifier);
                    }
                  if (IR_IS_OF_TYPE ($1, IR_NM_exclusion_clause)
		      && IR_identifier_list_2 ($1) != NULL)
                    {
                      /* Transform cyclic list to usual list. */
                      first_identifier
                        = IR_next_identifier (IR_identifier_list_2 ($1));
                      IR_set_next_identifier (IR_identifier_list_2 ($1), NULL);
                      IR_set_identifier_list_2 ($1, first_identifier);
                    }
                  $$ = $1;
                }
            | LOCAL  {$<position>$ = current_lexema_position;}  CODE_INSERTION
                {
                  $$ = IR_new_local_code ($<position>2, NULL, $3);
                }
            | IMPORT  {$<position>$ = current_lexema_position;}  CODE_INSERTION
                {
                  $$ = IR_new_import_code ($<position>2, NULL, $3);
                }
            | EXPORT  {$<position>$ = current_lexema_position;}  CODE_INSERTION
                {
                  $$ = IR_new_export_code ($<position>2, NULL, $3);
                }
            ;

/* The attribute of the following nonterminal is an identifier
   declaration whose identifier list refers to last identifier and the
   identifier list is cyclic. */

identifier_declaration : instruction_declaration         {$$ = $1;}
                       | reservation_declaration         {$$ = $1;}
                       | unit_declaration                {$$ = $1;}
                       | automaton_declaration           {$$ = $1;}
                       | exclusion_clause                {$$ = $1;}
                       ;

/* The attribute of the following nonterminal is instruction
   declaration whose identifier list refers to last identifier and the
   identifier list is cyclic. */

instruction_declaration : INSTRUCTION
                            {
                              $$
                                = IR_new_instruction_declaration
                                  (current_lexema_position, NULL, NULL);
                            } 
                        | instruction_declaration  IDENTIFIER
                            {
                              if (IR_identifier_list ($1) == NULL)
                                /* Make cycle */
                                IR_set_next_identifier ($2, $2);
                              else
                                {
                                  /* Add element to cyclic list */
                                  IR_set_next_identifier
                                    ($2, IR_next_identifier (IR_identifier_list
                                                             ($1)));
                                  IR_set_next_identifier
                                    (IR_identifier_list ($1), $2);
                                }
                              IR_set_identifier_list ($1, $2);
                              $$ = $1;
                            }
                        ;

/* The attribute of the following nonterminal is reservation
   declaration whose identifier list refers to last identifier and the
   identifier list is cyclic. */

reservation_declaration : RESERVATION
                            {
                               $$
                                 = IR_new_reservation_declaration
                                   (current_lexema_position, NULL, NULL);
                             } 
                        | reservation_declaration  IDENTIFIER
                            {
                               if (IR_identifier_list ($1) == NULL)
                                 /* Make cycle */
                                IR_set_next_identifier ($2, $2);
                               else
                                 {
                                   /* Add element to cyclic list */
                                   IR_set_next_identifier
                                     ($2,
                                      IR_next_identifier (IR_identifier_list
                                                          ($1)));
                                   IR_set_next_identifier
                                     (IR_identifier_list ($1), $2);
                                 }
                               IR_set_identifier_list ($1, $2);
                               $$ = $1;
                             }
                        ;

/* The attribute of the following nonterminal is unit declaration whose
   identifier list refers to last identifier and the identifier list
   is cyclic. */

unit_declaration : UNIT  optional_automaton_identifier
                     {
                       $$ = IR_new_unit_declaration (current_lexema_position,
                                                     NULL, NULL, $2);
                     } 
                 | unit_declaration  IDENTIFIER
                     {
                       if (IR_identifier_list ($1) == NULL)
                         /* Make cycle */
                         IR_set_next_identifier ($2, $2);
                       else
                         {
                           /* Add element to cyclic list */
                           IR_set_next_identifier
                             ($2, IR_next_identifier (IR_identifier_list
                                                      ($1)));
                           IR_set_next_identifier
                             (IR_identifier_list ($1), $2);
                         }
                       IR_set_identifier_list ($1, $2);
                       $$ = $1;
                     }
                 ;

exclusion_clause : EXCLUSION {$<position>$ = current_lexema_position;}
                   identifier_list COLON identifier_list
                     {
		       $$ = IR_new_exclusion_clause ($<position>2,
						     NULL, $3, $5);
		     }
                 ;

/* The attribute of the following nonterminal is the last element in
   the cyclic identifier list. */

identifier_list : IDENTIFIER
                    {
                      IR_set_next_identifier ($1, $1);
                      $$ = $1;
                    }
                | identifier_list IDENTIFIER
                    {
		      /* Add element to cyclic list */
		      IR_set_next_identifier ($2, IR_next_identifier ($1));
		      IR_set_next_identifier ($1, $2);
		      $$ = $1;
                    }
                ;

/* The attribute of the following nonterminal is automaton identifier
   or NULL in the case of its absence. */

optional_automaton_identifier :   {$$ = NULL;}
                              | LEFT_ANGLE_BRACKET
                                  IDENTIFIER  RIGHT_ANGLE_BRACKET
                                  {$$ = $2;}
                              ;

/* The attribute of the following nonterminal is automaton declaration
   whose identifier list refers to last identifier and the identifier
   list is cyclic. */

automaton_declaration
  : AUTOMATON
      {
        $$ = IR_new_automaton_declaration (current_lexema_position,
                                           NULL, NULL);
      } 
  | automaton_declaration  IDENTIFIER
      {
        if (IR_identifier_list ($1) == NULL)
          /* Make cycle */
          IR_set_next_identifier ($2, $2);
        else
          {
            /* Add element to cyclic list */
            IR_set_next_identifier
              ($2, IR_next_identifier (IR_identifier_list
                                       ($1)));
            IR_set_next_identifier
              (IR_identifier_list ($1), $2);
          }
        IR_set_identifier_list ($1, $2);
        $$ = $1;
      }
;

/* The attribute of the following nonterminal is last expression
   definition of given expression definition list.  The expression
   definition list is cyclic. */

expression_definition_list :   {$$ = NULL; }
                           | expression_definition_list  expression_definition
                               {
                                 if ($1 != NULL)
                                   {
                                     IR_node_t first_expression;

                                     /* Add cyclic list to the first
                                        cyclic list. */
                                     first_expression
                                       = IR_next_expression_definition ($2);
                                     IR_set_next_expression_definition
                                       ($2,
                                        IR_next_expression_definition ($1));
                                     IR_set_next_expression_definition
                                       ($1, first_expression);
                                   }
                                 $$ = $2;
                               }
                           | expression_definition_list  error  SEMICOLON
                               {
                                 syntax_error
                                   ("syntax error in expression definition");
                                 $$ = $1; /* Ignore error expression. */
                               }
                           | expression_definition_list error ADDITIONAL_C_CODE
                               {
                                 yychar = ADDITIONAL_C_CODE;
                                 syntax_error
                                   ("syntax error in expression definition");
                                 $$ = $1; /* Ignore error expression. */
                               }
                           ;

/* Attribute of this nonterminal is node representing last of
   expression definition of expression definitions list corresponding
   to given construction. */

expression_definition : instruction_or_reservation_identifier_list  COLON 
                        expression  SEMICOLON
                          {
                            IR_node_t first_expression_definition;
                            IR_node_t current_expression_definition;

                            assert ($1 != NULL);
                            /* Set up expression for expression definitions. */
                            first_expression_definition
                              = IR_next_expression_definition ($1);
                            current_expression_definition
                              = first_expression_definition;
                            for (;;)
                              {
                                IR_set_expression
                                  (current_expression_definition, $3);
                                if (current_expression_definition == $1)
                                  break;
                                current_expression_definition
                                  = IR_next_expression_definition
                                    (current_expression_definition);
                              }
                            $$ = $1;
                          }
                      ;

/* Attribute of this nonterminal is node representing last of
   expression definition of expression definitions list in given
   expression definition identifier list. */

instruction_or_reservation_identifier_list
  : instruction_or_reservation_identifier
       {
         IR_set_next_expression_definition ($1, $1);
         $$ = $1;
       }
  | instruction_or_reservation_identifier_list
       COMMA  instruction_or_reservation_identifier
       {
         assert ($1 != NULL);
         /* Add element to cyclic list */
         IR_set_next_expression_definition
           ($3, IR_next_expression_definition ($1));
         IR_set_next_expression_definition ($1, $3);
         $$ = $3;
       }
  ;

instruction_or_reservation_identifier
  : IDENTIFIER
      {
        $$ = IR_new_expression_definition (current_lexema_position, $1);
      }
  ;

expression : expression  IDENTIFIER
                {
                  yychar = IDENTIFIER;
                  $<position>$ = current_lexema_position;
                }
             expression
                {
                  $$ = IR_new_new_cycle_concatenation ($<position>3, $1, $4);
                }
           | expression  NOTHING
                {
                  yychar = NOTHING;
                  $<position>$ = current_lexema_position;
                }
             expression
                {
                  $$ = IR_new_new_cycle_concatenation ($<position>3, $1, $4);
                }
           | expression  RESULT
                {
                  yychar = RESULT;
                  $<position>$ = current_lexema_position;
                }
             expression
                {
                  $$ = IR_new_new_cycle_concatenation ($<position>3, $1, $4);
                }
           | expression  INPUT
                {
                  yychar = INPUT;
                  $<position>$ = current_lexema_position;
                }
             expression
                {
                  $$ = IR_new_new_cycle_concatenation ($<position>3, $1, $4);
                }
           | expression  LEFT_PARENTHESIS
                {
                  yychar = LEFT_PARENTHESIS;
                  $<position>$ = current_lexema_position;
                }
             expression
                {
                  $$ = IR_new_new_cycle_concatenation ($<position>3, $1, $4);
                }
           | expression  LEFT_BRACKET
                {
                  yychar = LEFT_BRACKET;
                  $<position>$ = current_lexema_position;
                }
             expression
                {
                  $$ = IR_new_new_cycle_concatenation ($<position>3, $1, $4);
                }
           | expression  PLUS {$<position>$ = current_lexema_position;}
                expression
                {
                  $$ = IR_new_concatenation ($<position>3, $1, $4);
                }
           | expression STAR {$<position>$ = current_lexema_position;} NUMBER
                {
                  $$ = IR_new_repetition ($<position>3, $1, $4);
                }
           | LEFT_PARENTHESIS  expression  RIGHT_PARENTHESIS   {$$ = $2;}
           | LEFT_BRACKET {$<position>$ = current_lexema_position;}
                expression  RIGHT_BRACKET
                {
                  $$ = IR_new_optional_expression ($<position>2, $3);
                }
           | expression  BAR {$<position>$ = current_lexema_position;}
                expression
                {
                  $$ = IR_new_alternative ($<position>3, $1, $4);
                }
           | unit_or_reservation_identifier
                {
                  $$ = IR_new_expression_atom (IR_position ($1), $1);
                }
           | NOTHING
                {
                  $$ = IR_new_nothing (current_lexema_position);
                }
           | RESULT input_result_number
                {
                  $$ = IR_new_result (current_lexema_position, $2);
                }
           | INPUT input_result_number
                {
                  $$ = IR_new_input (current_lexema_position, $2);
                }
           ;

input_result_number :        {$$ = 0;}
                    | NUMBER {$$ = IR_number_value ($1);}
                    ;

unit_or_reservation_identifier : IDENTIFIER   {$$ = $1;}
                               ;


%%



/* This page contains abstract data `scanner'.  This abstract data
   divides input stream characters on tokens (lexemas). */

/* The following structure describes current input stream and scanner
   state. */

struct input_stream_state
{
  FILE *input_file;
  /* The following member is TRUE if the first `%%' in given input stream
     was already processed or FALSE otherwise. */
  int it_is_first_percents;
  /* The following member is TRUE if the ADDITIONAL_CODE lexema was already
     returned by the scanner or FALSE otherwise.  Remember that lexema
     ADDITIONAL_CODE is always returned by the scanner
     even if second `%%' in the input stream is absent. */
  int additional_c_code_was_returned;
};

/* The following structure contains current input stream and
   scanner state.  If the member `input_file' value is NULL
   then input stream is not defined. */

static struct input_stream_state current_input_stream_state;


/* The function returns lexical code of corresponding keyword or 0 if
   given identifier (parameter `str' is identifier (without start `%')
   string with end marker '\0'; `length' is length of identifier
   string without end marker '\0') is not keyword.  Fast search is
   implemented by minimal pruned O-trie forest.  */

static int
find_keyword (const char *str, int length)
{
  switch (length)
    {
    case 4:
      return (strcmp (str, "unit") == 0 ? UNIT : 0);
    case 5:
      return (strcmp (str, "local") == 0 ? LOCAL : 0);
    case 6:
      switch (*str)
        {
        case 'e':
          return (strcmp (str, "export") == 0 ? EXPORT : 0);
        case 'i':
          return (strcmp (str, "import") == 0 ? IMPORT : 0);
        default:
          return 0;
        }
    case 7:
      return (strcmp (str, "nothing") == 0 ? NOTHING : 0);
    case 9:
      if (*str == 'e')
	return (strcmp (str, "exclusion") == 0 ? EXCLUSION : 0);
      else
	return (strcmp (str, "automaton") == 0 ? AUTOMATON : 0);
    case 11:
      switch (*str)
        {
        case 'i':
          return (strcmp (str, "instruction") == 0 ? INSTRUCTION : 0);
        case 'r':
          return (strcmp (str, "reservation") == 0 ? RESERVATION : 0);
        default:
          return 0;
        }
    default:
      return 0;
    }
}

/* The following value is used if previous_char does not contain an
   input char */

#define EMPTY_PREVIOUS_CHAR (-2000)

/* The following variable is used for storing '\r'. */

static int previous_char;

/* The following two functions for replacing "\r\n" onto "\n". */

static int
getch (FILE *f)
{
  int c;

  if (previous_char != '\r')
    c = getc (f);
  else
    {
      c = previous_char;
      previous_char = EMPTY_PREVIOUS_CHAR;
    }
  if (c == '\r')
    {
      c = getc (f);
      if (c != '\n')
        {
          ungetc (c, f);
          c = '\r';
        }
    }
  return c;
}

static void
ungetch (int c, FILE *f)
{
  if (c != '\r')
    ungetc (c, f);
  else
    previous_char = c;
}


/* The function skips C commentary.  Parameter `store_flag' is flag of
   storing commentary characters onto top internal representation
   object.  The function returns TRUE if no error (EOF) was fixed,
   otherwise FALSE
     Current position corresponds to `*' after commentary start ('/')
   before the function call.  Current_position corresponds to first
   character after skipped commentary ('/') or EOF after the function
   call. */

static int
skip_commentary (int store_flag)
{
  int input_char;
  
  current_position.column_number++;
  if (store_flag)
    IR_TOP_ADD_BYTE ('*');
  for (;;)
    {
      input_char = getch (current_input_stream_state.input_file);
      /* Here `current_position' corresponds to `input_char'. */
      if (input_char == '*')
        {
          if (store_flag)
            IR_TOP_ADD_BYTE (input_char);
          input_char = getch (current_input_stream_state.input_file);
          current_position.column_number++;
          /* Here `current_position' corresponds to `input_char'. */
          if (input_char == '/')
            {
              if (store_flag)
                IR_TOP_ADD_BYTE (input_char);
              current_position.column_number++;
              return TRUE;
            }
          else
            {
              ungetch (input_char, current_input_stream_state.input_file);
              continue;
            }
        }
      else if (input_char == EOF)
        {
          ungetch (input_char, current_input_stream_state.input_file);
          return FALSE;
        }
      if (input_char == '\n')
        {
          current_position.column_number = 1;
          current_position.line_number++;
        }
      else if (input_char == '\t')
        current_position.column_number
          = ((current_position.column_number - 1) / TAB_STOP + 1)
            * TAB_STOP + 1;
      else
        current_position.column_number++;
      if (store_flag)
        IR_TOP_ADD_BYTE (input_char);
    }
}


/* The function skips C string code (e.g `a', `\n', or `\077').  The
   function also stores skipped characters onto top internal
   representation object.  Parameter `input_char' is the most recent
   read character.  The function returns (with the aid of parameter
   `correct_new_line') flag of that correct new line was read.  The
   function returns TRUE if no error (EOF or incorrect new line) was
   fixed, otherwise FALSE.
     The value of parameter `correct_new_line' can be used for fixing
   errors when character constant is processed.  It is supposed that
   the current character is not end marker (string or character
   constant).  Current position corresponds to parameter `input_char'
   before the function call.  Current_position corresponds to first
   character after skipped valid character code or EOF (or incorrect
   new line) after the function call. */

static int
skip_string_code (int input_char, int *correct_new_line)
{
  if (input_char == EOF || input_char == '\n')
    {
      ungetch (input_char, current_input_stream_state.input_file);
      return FALSE;
    }
  *correct_new_line = FALSE;
  IR_TOP_ADD_BYTE (input_char);
  if (input_char == '\\')
    {
      current_position.column_number++;
      input_char = getch (current_input_stream_state.input_file);
      IR_TOP_ADD_BYTE (input_char);
      if (input_char == 'n' || input_char == 't' || input_char == 'v'
          || input_char == 'b' || input_char == 'r' || input_char == 'f'
          || input_char == '\\' || input_char == '\'' || input_char == '\"')
        current_position.column_number++;
      else if (input_char == '\n')
        {
          current_position.column_number = 1;
          current_position.line_number++;
          *correct_new_line = TRUE;
        }
      else if (isdigit (input_char) && input_char != '8'
               && input_char != '9')
        {
          current_position.column_number++;
          input_char = getch (current_input_stream_state.input_file);
          if (!isdigit (input_char) || input_char == '8'
              || input_char == '9')
            ungetch (input_char, current_input_stream_state.input_file);
          else
            {
              current_position.column_number++;
              IR_TOP_ADD_BYTE (input_char);
              input_char = getch (current_input_stream_state.input_file);
              if (!isdigit (input_char) || input_char == '8'
                  || input_char == '9')
                ungetch (input_char, current_input_stream_state.input_file);
              else
                {
                  current_position.column_number++;
                  IR_TOP_ADD_BYTE (input_char);
                }
            }
        }
      else
        current_position.column_number++;
    }
  else if (input_char == '\t')
    current_position.column_number
      = ((current_position.column_number - 1) / TAB_STOP + 1) * TAB_STOP + 1;
  else
    current_position.column_number++;
  return TRUE;
}

/* The function skips C character constant by using function
   `skip_string_code'.  The function may fix error in the C character
   constant.  The function also stores skipped characters onto top
   internal representation object.  Current position corresponds to
   first character after '\'' (which was already read) before the
   function call.  Current_position corresponds to to first character
   after '\'' if error was not fixed after the function call. */

static void
skip_C_character (void)
{
  int input_char;
  int correct_new_line;
  int error_flag;

  error_flag = FALSE;
  input_char = getch (current_input_stream_state.input_file);
  /* Here `current_position' corresponds to `input_char'. */
  if (input_char == '\'')
    {
      IR_TOP_ADD_BYTE (input_char);
      error (FALSE, current_position, "invalid character constant");
      current_position.column_number++;
      error_flag = TRUE;
    }
  else
    {
      if (!skip_string_code (input_char, &correct_new_line)
          || correct_new_line)
        {
          error (FALSE, current_position, "invalid character constant");
          error_flag = TRUE;
        }
    }
  input_char = getch (current_input_stream_state.input_file);
  if (input_char != '\'')
    {
      ungetch (input_char, current_input_stream_state.input_file);
      if (!error_flag)
        error (FALSE, current_position, "invalid character constant");
    }
  else
    {
      IR_TOP_ADD_BYTE (input_char);
      current_position.column_number++;
    }
}

/* The function skips C string constant by using function
   `skip_string_code'.  The function may fix error in the C string
   constant.  The function also stores skipped characters onto top
   internal representation object.  Current position corresponds to
   first character after '\"' (which was already read) before the
   function call.  Current_position corresponds to to first character
   after '\"' if error was not fixed after the function call.  */

static void
skip_C_string (void)
{
  int input_char;
  int correct_new_line;
  
  for (;;)
    {
      /* Here `current_position' corresponds to next character. */
      input_char = getch (current_input_stream_state.input_file);
      if (input_char == '\"')
        {
          current_position.column_number++;
          IR_TOP_ADD_BYTE (input_char); 
          break;
        }
      if (!skip_string_code (input_char, &correct_new_line))
        {
          error (FALSE, current_position, "string end is absent");
          break;
        }
    }
}

/* This function is major scanner function.  The function recognizes
   next source lexema from the input file, returns its code, modifies
   variable current_position so that its value is equal to position of
   the current character in the current input file and sets up
   variable `current_lexema_position' so that its value is equal to
   start position of the returned lexema, creates corresponding
   internal representation node (if it is needed) and sets up yylval
   to the node.  The function skips all white spaces and commentaries
   and fixes the most of lexical errors. */

int
yylex (void)
{
  int input_char;
  int number_of_successive_error_characters;
  
  for (number_of_successive_error_characters = 0;;)
    {
      input_char = getch (current_input_stream_state.input_file);
      /* `current_position' corresponds `input_char' here */
      switch (input_char)
        {
        case '\f':
          /* flow-through */
        case ' ':
          current_position.column_number++;
          break;
        case '\t':
          current_position.column_number
            = ((current_position.column_number - 1) / TAB_STOP + 1)
              * TAB_STOP + 1;
          break;
        case '\n':
          current_position.column_number = 1;
          current_position.line_number++;
          break;
        case EOF:
          current_lexema_position = current_position;
          if (!current_input_stream_state.additional_c_code_was_returned)
            {
              char *additional_code_itself;

              ungetch (input_char, current_input_stream_state.input_file);
              current_input_stream_state.additional_c_code_was_returned = TRUE;
              IR_TOP_ADD_BYTE ('\0');
              additional_code_itself = IR_TOP_BEGIN ();
              IR_TOP_FINISH ();
              yylval.node = IR_new_additional_code (current_lexema_position,
                                                    additional_code_itself);
              return ADDITIONAL_C_CODE;
            }
          else
            return 0;
        case '/':
          current_lexema_position = current_position;
          current_position.column_number++;
          input_char = getch (current_input_stream_state.input_file);
          if (input_char != '*')
            {
              number_of_successive_error_characters++;
              if (number_of_successive_error_characters == 1)
                error (FALSE, current_lexema_position,
                       "invalid input character '/'");
              ungetch (input_char, current_input_stream_state.input_file);
            }
          else if (!skip_commentary (FALSE))
            error (FALSE, current_lexema_position, "commentary end is absent");
          break;
        case '(':
          current_lexema_position = current_position;
          current_position.column_number++;
          return LEFT_PARENTHESIS;
        case ')':
          current_lexema_position = current_position;
          current_position.column_number++;
          return RIGHT_PARENTHESIS;
        case '[':
          current_lexema_position = current_position;
          current_position.column_number++;
          return LEFT_BRACKET;
        case ']':
          current_lexema_position = current_position;
          current_position.column_number++;
          return RIGHT_BRACKET;
        case '<':
          current_lexema_position = current_position;
          current_position.column_number++;
          return LEFT_ANGLE_BRACKET;
        case '>':
          current_lexema_position = current_position;
          current_position.column_number++;
          return RIGHT_ANGLE_BRACKET;
        case ';':
          current_lexema_position = current_position;
          current_position.column_number++;
          return SEMICOLON;
        case ',':
          current_lexema_position = current_position;
          current_position.column_number++;
          return COMMA;
        case ':':
          current_lexema_position = current_position;
          current_position.column_number++;
          return COLON;
        case '+':
          current_lexema_position = current_position;
          current_position.column_number++;
          return PLUS;
        case '*':
          current_lexema_position = current_position;
          current_position.column_number++;
          return STAR;
        case '|':
          current_lexema_position = current_position;
          current_position.column_number++;
          return BAR;
        case '{':
          {
            int bracket_level = 1;
            int error_flag;
            char *code_itself;
            
            current_lexema_position = current_position;
            current_position.column_number++;
            for (error_flag = FALSE; !error_flag && bracket_level != 0;)
              {
                input_char = getch (current_input_stream_state.input_file);
                IR_TOP_ADD_BYTE (input_char);
                /* `current_position' corresponds to `input_char' here */
                switch (input_char)
                  {
                  case '/':
                    current_position.column_number++;
                    input_char = getch (current_input_stream_state.input_file);
                    if (input_char == '*')
                      {
                        current_position.column_number++;
                        error_flag = !skip_commentary (TRUE);
                      }
                    else
                      ungetch (input_char,
                               current_input_stream_state.input_file);
                    break;
                  case '{':
                    current_position.column_number++;
                    bracket_level++;
                    break;
                  case '}':
                    current_position.column_number++;
                    bracket_level--;
                    if (bracket_level == 0)
                      IR_TOP_SHORTEN (1);
                    break;
                  case '\'':
                    current_position.column_number++;
                    skip_C_character ();
                    break;
                  case '\"':
                    current_position.column_number++;
                    skip_C_string ();
                    break;
                  case '\t':
                    current_position.column_number
                      = ((current_position.column_number - 1) / TAB_STOP + 1)
                        * TAB_STOP + 1;
                    break;
                  case '\n':
                    current_position.column_number = 1;
                    current_position.line_number++;
                    break;
                  case EOF:
                    IR_TOP_SHORTEN (1);
                    ungetch (input_char,
                             current_input_stream_state.input_file);
                    error_flag = TRUE;
                    break;
                  default:
                    current_position.column_number++;
                    break;
                  }
              }
            if (error_flag)
              error (FALSE, current_lexema_position,
                     "C code insertion end is absent");
            IR_TOP_ADD_BYTE ('\0');
            code_itself = IR_TOP_BEGIN ();
            IR_TOP_FINISH ();
            yylval.node = IR_new_code_insertion (current_lexema_position,
                                                 code_itself);
            return CODE_INSERTION;
          }
        case '%':
          current_lexema_position = current_position;
          current_position.column_number++;
          input_char = getch (current_input_stream_state.input_file);
          if (input_char == '%')
            {
              current_position.column_number++;
              if (current_input_stream_state.it_is_first_percents)
                {
                  current_input_stream_state.it_is_first_percents = FALSE;
                  return PERCENTS;
                }
              else
                {
                  char *additional_code_itself;

                  current_position.column_number++;
                  input_char = getch (current_input_stream_state.input_file);
                  while (input_char != EOF)
                    {
                      current_position.column_number++;
                      IR_TOP_ADD_BYTE (input_char);
                      input_char
                        = getch (current_input_stream_state.input_file);
                    }
                  ungetch (input_char, current_input_stream_state.input_file);
                  IR_TOP_ADD_BYTE ('\0');
                  additional_code_itself = IR_TOP_BEGIN ();
                  IR_TOP_FINISH ();
                  current_input_stream_state.additional_c_code_was_returned
                    = TRUE;
                  yylval.node
                    = IR_new_additional_code (current_lexema_position,
                                              additional_code_itself);
                  return ADDITIONAL_C_CODE;
                }
            }
          else if (isalpha (input_char) || input_char == '_' )
            {
              int keyword;
              
              /* Identifier recognition. */
              do
                {
                  current_position.column_number++;
                  IR_TOP_ADD_BYTE (input_char);
                  input_char = getch (current_input_stream_state.input_file);
                }
              while (isalpha (input_char) || isdigit (input_char)
                     || input_char == '_');
              ungetch (input_char, current_input_stream_state.input_file);
              IR_TOP_ADD_BYTE ('\0');
              keyword = find_keyword (IR_TOP_BEGIN (), IR_TOP_LENGTH () - 1);
              if (keyword != 0)
                {
                  IR_TOP_NULLIFY ();
                  return keyword;
                }
              else
                {
                  error (FALSE, current_lexema_position,
                         "unknown keyword `%%%s'", IR_TOP_BEGIN ());
                  IR_TOP_NULLIFY ();
                }
            }
          else
            {
              number_of_successive_error_characters++;
              if (number_of_successive_error_characters == 1)
                error (FALSE, current_lexema_position,
                       "invalid input character '%%'");
              ungetch (input_char, current_input_stream_state.input_file);
            }
          break;
        default:
          if (isalpha (input_char) || input_char == '_' )
            {
              char *identifier_itself;

              current_lexema_position = current_position;
              /* Identifier recognition. */
              do
                {
                  current_position.column_number++;
                  IR_TOP_ADD_BYTE (input_char);
                  input_char = getch (current_input_stream_state.input_file);
                }
              while (isalpha (input_char) || isdigit (input_char)
                     || input_char == '_');
              ungetch (input_char, current_input_stream_state.input_file);
              IR_TOP_ADD_BYTE ('\0');
              identifier_itself = insert_identifier (IR_TOP_BEGIN ());
              if (identifier_itself == (char *) IR_TOP_BEGIN ())
                IR_TOP_FINISH ();
              else
                IR_TOP_NULLIFY ();
              yylval.node = IR_new_identifier (current_lexema_position,
                                               identifier_itself, NULL);
              return IDENTIFIER;
            }
          else if (isdigit (input_char))
            {
              int value;

              current_lexema_position = current_position;
              value = 0;
              /* Number recognition. */
              do
                {
                  current_position.column_number++;
                  value = value * 10 + (input_char - '0');
                  input_char = getch (current_input_stream_state.input_file);
                }
              while (isdigit (input_char));
              ungetch (input_char, current_input_stream_state.input_file);
              yylval.node = IR_new_number (current_lexema_position, value);
              return NUMBER;
            }
          else
            {
              number_of_successive_error_characters++;
              if (number_of_successive_error_characters == 1)
                {
                  if (isprint (input_char))
                    error (FALSE, current_position,
                           "invalid input character '%c'", input_char);
                  else
                    error (FALSE, current_position, "invalid input character");
                }
              current_position.column_number++;
            }
          break;
        }
    }
}

/* The function initiates current input stream and scanner state as
   undefined.  The function must be called only once before any work
   with the scanner.  */

static void
initiate_scanner (void)
{
  current_input_stream_state.input_file = NULL;
  current_input_stream_state.it_is_first_percents = TRUE;
  current_input_stream_state.additional_c_code_was_returned = FALSE;
}

/* The function opens file corresponding to new input stream, and sets
   up new current input stream and scanner state.  The function
   reports error if the new file is not opened.  */

static void
start_scanner_file (const char *new_file_name)
{
  current_input_stream_state.input_file = fopen (new_file_name, "rb");
  if (current_input_stream_state.input_file == NULL)
    system_error (TRUE, no_position, "fatal error -- `%s': ", new_file_name);
  start_file_position (new_file_name);
  previous_char = EMPTY_PREVIOUS_CHAR;
  current_input_stream_state.it_is_first_percents = TRUE;
  current_input_stream_state.additional_c_code_was_returned = FALSE;
}

/* The function closes current input file if current input stream
   state is defined.  Only call of function `initiate_scanner' or
   `finish_scanner' is possible immediately after this function call.  */

static void
finish_scanner (void)
{
  if (current_input_stream_state.input_file != NULL)
    {
      fclose (current_input_stream_state.input_file);
      current_input_stream_state.input_file = NULL;
      finish_file_position ();
    }
}



/*   This page contains abstract data `syntax_errors'.  This abstract
   data is used to error reporting.
     YACC has unsatisfactory syntactic error reporting.  Essentially
   YACC generates only one syntactic error "syntax error".  Abstract
   data `syntax_errors' for solution of this problem is suggested
   here.  This abstract data saves only one the most recent error
   message and its position.  Function `yyerror' used only by function
   `yyparse' flushes saved early error message and saves message given
   as parameter (usually this message is simply "syntax error" but may
   be "stack overflow" or others) and its position.  More detail
   diagnostic message is generated by the call of function
   `syntax_error' within action after special token `error'.  This
   function only rewrites saved early error message (not error
   position).  As consequence the message of last call `syntax_error'
   or the message of function `yyerror' (if such call of
   `syntax_error' does not exist) is fixed within one recover mode
   interval because function `yyerror' is called by `yyparse' only in
   non recover mode.
     This solution permits to create brief syntactic message which
   contain partially information about recovery location (error line
   and position is always in non-fatal errors and warnings -- see
   package for `errors').  */


/* This variable saves the most recent syntax error message or NULL (if
   such syntax error was not fixed). */

static const char *last_syntax_error_message;

/* This variable saves the first syntax error position. */

static position_t first_syntax_error_position;

/* The function initiates the abstract data as without saved syntax
   error message.  The function must be called only once before any
   work with the abstract data.  */

static void
initiate_syntax_errors (void)
{
  last_syntax_error_message = NULL;
}

/* The function flushes saved syntax error message, if any.  */

static void
flush_syntax_error (void)
{
  if (last_syntax_error_message != NULL)
    {
      error (FALSE, first_syntax_error_position,
             "%s", last_syntax_error_message);
      last_syntax_error_message = NULL;
    }
}

/* The function is called by `yyparse' on syntax error.  This function
   flushes saved syntax error message with the aid of
   `flush_syntax_error' and saves message given as parameter and
   `current_lexema position' as its position (see scanner). */

yyerror (const char *message)
{
  flush_syntax_error ();
  last_syntax_error_message = message;
  first_syntax_error_position = current_lexema_position;
  return 0;
}

/* The function rewrites saved syntax error message but does not
   change saved position.  */

static void
syntax_error (const char *s)
{
  last_syntax_error_message = s;
}

/* The function finishes work with the abstract data (flushes saved
   syntax error message with the aid of function
   `flush_syntax_error').  */

static void
finish_syntax_errors (void)
{
  flush_syntax_error ();
}



/* This page contains all external functions of parser. */

/* The function initiates all internal abstract data (the scanner and
   `syntax_errors').  The function must be called only once before any
   work with the abstract data. */

void
initiate_parser (void)
{
  initiate_scanner ();
  initiate_syntax_errors ();
}

/* The function forces to tune parser onto file with given name (calls
   function `start_scanner_file').  The function must be called only
   once after parser initiation.  */

void
start_parser_file (const char *file_name)
{
  start_scanner_file (file_name);
}

/* This function finishes all internal abstract data (the scanner and
   abstract data `syntax_errors').  Only call of function
   `initiate_parser' or `finish_parser' is possible immediately after
   this function call. */

void
finish_parser (void)
{
  finish_scanner ();
  finish_syntax_errors ();
}

/*
Local Variables:
mode:c
End:
*/
