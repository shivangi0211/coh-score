/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Using locations.  */
#define YYLSP_NEEDED 1



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     TYPEDEF = 258,
     EXTERN = 259,
     STATIC = 260,
     CONST_TOK = 261,
     VOID_TOK = 262,
     META_TOK = 263,
     TOK = 264,
     CONSTANT = 265,
     COMMANDLINE = 266,
     CONSOLE = 267,
     REMOTE = 268,
     ACCESSLEVEL = 269,
     CONSOLE_DESC = 270,
     CONSOLE_REQPARAMS = 271,
     STR = 272
   };
#endif
/* Tokens.  */
#define TYPEDEF 258
#define EXTERN 259
#define STATIC 260
#define CONST_TOK 261
#define VOID_TOK 262
#define META_TOK 263
#define TOK 264
#define CONSTANT 265
#define COMMANDLINE 266
#define CONSOLE 267
#define REMOTE 268
#define ACCESSLEVEL 269
#define CONSOLE_DESC 270
#define CONSOLE_REQPARAMS 271
#define STR 272




/* Copy the first part of user declarations.  */
#line 1 "c.y"

// PROLOGUE
#include "ncstd.h"
#include "Array.h"
#include <stdio.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "parse.h"
#include "str.h"

#pragma warning(push)
#pragma warning(disable:4053)
#pragma warning(disable:4244)
#pragma warning(disable:4702)
#pragma warning(disable:4706)
#pragma warning(disable:6262)
#pragma warning(disable:6385)

// #define YYLTYPE int
#define YYLLOC_DEFAULT(Current, Rhs, N) do { \
        Current = YYRHSLOC(Rhs,N);                \
} while(0)



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 29 "c.y"
typedef union YYSTYPE {
	char tok[MAX_IDENTIFIER_LEN];
	MetaToken mtok;
	char *str;							// Str

	Arg arg;
	Arg *args;							// aarg
	char **strs;						// ap of Str
	char **callnames[METATOKEN_COUNT];	// array of ap of Str
} YYSTYPE;
/* Line 196 of yacc.c.  */
#line 156 "c.tab.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

#if ! defined (YYLTYPE) && ! defined (YYLTYPE_IS_DECLARED)
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


/* Copy the second part of user declarations.  */
#line 41 "c.y"

#define __STDC__ // ab: do this last, only needed by output of this
#define YYMALLOC malloc
#define YYFREE free

int yylex (YYSTYPE *lvalp, YYLTYPE *line, ParseRun *run);
void yyerror (YYLTYPE *line, ParseRun *run, char const *s);
char *tok_append(char*a,char *b);

static void print_token_value (FILE *, int, YYSTYPE);
#define YYPRINT(file, type, value) print_token_value (file, type, value)



/* Line 219 of yacc.c.  */
#line 193 "c.tab.c"

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T) && (defined (__STDC__) || defined (__cplusplus))
# include <stddef.h> /* INFRINGES ON USER NAME SPACE */
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

#if ! defined (yyoverflow) || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if defined (__STDC__) || defined (__cplusplus)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     define YYINCLUDED_STDLIB_H
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2005 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM ((YYSIZE_T) -1)
#  endif
#  ifdef __cplusplus
extern "C" {
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if (! defined (malloc) && ! defined (YYINCLUDED_STDLIB_H) \
	&& (defined (__STDC__) || defined (__cplusplus)))
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if (! defined (free) && ! defined (YYINCLUDED_STDLIB_H) \
	&& (defined (__STDC__) || defined (__cplusplus)))
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifdef __cplusplus
}
#  endif
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (defined (YYLTYPE_IS_TRIVIAL) && YYLTYPE_IS_TRIVIAL \
             && defined (YYSTYPE_IS_TRIVIAL) && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short int yyss;
  YYSTYPE yyvs;
    YYLTYPE yyls;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short int) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined (__GNUC__) && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short int yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   48

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  24
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  11
/* YYNRULES -- Number of rules. */
#define YYNRULES  40
/* YYNRULES -- Number of states. */
#define YYNSTATES  53

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   272

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      18,    19,    22,     2,    23,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    21,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    20,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned char yyprhs[] =
{
       0,     0,     3,     4,     7,    10,    22,    28,    30,    32,
      35,    38,    41,    43,    45,    49,    50,    52,    54,    56,
      58,    60,    62,    64,    66,    68,    69,    72,    73,    79,
      82,    84,    86,    88,    90,    92,    94,    96,    98,   100,
     102
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      25,     0,    -1,    -1,    25,    26,    -1,    25,    34,    -1,
       8,    18,    33,    19,    30,    27,     9,    18,    29,    19,
      20,    -1,     8,    18,    33,    19,    21,    -1,     9,    -1,
       7,    -1,     6,     9,    -1,    27,    22,    -1,    27,     9,
      -1,     7,    -1,    28,    -1,    29,    23,    28,    -1,    -1,
       3,    -1,     4,    -1,     5,    -1,    11,    -1,    12,    -1,
      13,    -1,    14,    -1,    15,    -1,    16,    -1,    -1,    32,
      17,    -1,    -1,    33,    31,    18,    32,    19,    -1,    33,
      31,    -1,    20,    -1,    18,    -1,    19,    -1,    22,    -1,
      23,    -1,    21,    -1,     9,    -1,    17,    -1,    10,    -1,
       6,    -1,     7,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned char yyrline[] =
{
       0,    83,    83,    84,    85,    88,    96,   108,   109,   110,
     111,   114,   117,   118,   119,   123,   124,   125,   126,   130,
     131,   132,   133,   134,   135,   138,   139,   142,   143,   144,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "TYPEDEF", "EXTERN", "STATIC",
  "CONST_TOK", "VOID_TOK", "META_TOK", "TOK", "CONSTANT", "COMMANDLINE",
  "CONSOLE", "REMOTE", "ACCESSLEVEL", "CONSOLE_DESC", "CONSOLE_REQPARAMS",
  "STR", "'('", "')'", "'{'", "';'", "'*'", "','", "$accept",
  "translation_unit", "meta_function_definition", "type_decl",
  "function_arg", "function_args", "static_opt", "meta_token",
  "meta_names", "meta_args", "ignored_opt", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short int yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,    40,    41,
     123,    59,    42,    44
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    24,    25,    25,    25,    26,    26,    27,    27,    27,
      27,    28,    29,    29,    29,    30,    30,    30,    30,    31,
      31,    31,    31,    31,    31,    32,    32,    33,    33,    33,
      34,    34,    34,    34,    34,    34,    34,    34,    34,    34,
      34
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     0,     2,     2,    11,     5,     1,     1,     2,
       2,     2,     1,     1,     3,     0,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     0,     2,     0,     5,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       2,     0,     1,    39,    40,     0,    36,    38,    37,    31,
      32,    30,    35,    33,    34,     3,     4,    27,     0,    19,
      20,    21,    22,    23,    24,    15,    29,    16,    17,    18,
       6,     0,    25,     0,     8,     7,     0,     0,     9,     0,
      10,    26,    28,     0,     8,     0,    13,     0,    11,     0,
       0,     5,    14
};

/* YYDEFGOTO[NTERM-NUM]. */
static const yysigned_char yydefgoto[] =
{
      -1,     1,    15,    45,    46,    47,    31,    26,    37,    18,
      16
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -15
static const yysigned_char yypact[] =
{
     -15,     0,   -15,   -15,   -15,    -2,   -15,   -15,   -15,   -15,
     -15,   -15,   -15,   -15,   -15,   -15,   -15,   -15,    19,   -15,
     -15,   -15,   -15,   -15,   -15,     8,     7,   -15,   -15,   -15,
     -15,    35,   -15,    -5,   -15,   -15,    -8,   -14,   -15,     9,
     -15,   -15,   -15,    39,     5,    -7,   -15,    17,   -15,     6,
      35,   -15,   -15
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
     -15,   -15,   -15,    12,   -13,   -15,   -15,   -15,   -15,   -15,
     -15
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -13
static const yysigned_char yytable[] =
{
       2,    39,    48,    41,    38,    42,     3,     4,     5,     6,
       7,    27,    28,    29,    40,    40,    17,     8,     9,    10,
      11,    12,    13,    14,   -12,    32,    51,    43,   -12,    30,
      19,    20,    21,    22,    23,    24,    49,    52,    25,     0,
      50,    33,    34,    36,    35,    33,    44,     0,    35
};

static const yysigned_char yycheck[] =
{
       0,     9,     9,    17,     9,    19,     6,     7,     8,     9,
      10,     3,     4,     5,    22,    22,    18,    17,    18,    19,
      20,    21,    22,    23,    19,    18,    20,    18,    23,    21,
      11,    12,    13,    14,    15,    16,    19,    50,    19,    -1,
      23,     6,     7,    31,     9,     6,     7,    -1,     9
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    25,     0,     6,     7,     8,     9,    10,    17,    18,
      19,    20,    21,    22,    23,    26,    34,    18,    33,    11,
      12,    13,    14,    15,    16,    19,    31,     3,     4,     5,
      21,    30,    18,     6,     7,     9,    27,    32,     9,     9,
      22,    17,    19,    18,     7,    27,    28,    29,     9,    19,
      23,    20,    28
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (&yylloc, run, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (0)


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (N)								\
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (0)
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
              (Loc).first_line, (Loc).first_column,	\
              (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, &yylloc, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, &yylloc, run)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr,					\
                  Type, Value, Location);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short int *bottom, short int *top)
#else
static void
yy_stack_print (bottom, top)
    short int *bottom;
    short int *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu), ",
             yyrule - 1, yylno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname[yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      size_t yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

#endif /* YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep, yylocationp)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;
  (void) yylocationp;

  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");

# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;
  (void) yylocationp;

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (ParseRun *run);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */






/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (ParseRun *run)
#else
int
yyparse (run)
    ParseRun *run;
#endif
#endif
{
  /* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;
/* Location data for the look-ahead symbol.  */
YYLTYPE yylloc;

  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short int yyssa[YYINITDEPTH];
  short int *yyss = yyssa;
  short int *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;

  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
  /* The locations where the error started and ended. */
  YYLTYPE yyerror_range[2];

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;
  yylsp = yyls;
#if YYLTYPE_IS_TRIVIAL
  /* Initialize the default location before parsing starts.  */
  yylloc.first_line   = yylloc.last_line   = 1;
  yylloc.first_column = yylloc.last_column = 0;
#endif

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short int *yyss1 = yyss;
	YYLTYPE *yyls1 = yyls;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short int *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
	YYSTACK_RELOCATE (yyls);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a look-ahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to look-ahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
  *++yylsp = yylloc;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, yylsp - yylen, yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 5:
#line 89 "c.y"
    {
								parse_addfunc(	run,
												(yyvsp[-8].callnames), // acall
												&(yyvsp[-5].arg), // return type
												(yyvsp[-4].tok), // func name
												(yyvsp[-2].args)); // func args
							;}
    break;

  case 6:
#line 97 "c.y"
    {
								parse_addfunc(	run,
												(yyvsp[-2].callnames),
												NULL,
												NULL,
												NULL);
							;}
    break;

  case 7:
#line 108 "c.y"
    { ClearStruct(&(yyval.arg)); strcpy((yyval.arg).type_name, (yyvsp[0].tok)); ;}
    break;

  case 8:
#line 109 "c.y"
    { ClearStruct(&(yyval.arg)); strcpy((yyval.arg).type_name, (yyvsp[0].tok)); ;}
    break;

  case 9:
#line 110 "c.y"
    { ClearStruct(&(yyval.arg)); strcpy((yyval.arg).type_name, (yyvsp[0].tok)); (yyval.arg).is_const = 1; ;}
    break;

  case 10:
#line 111 "c.y"
    { (yyval.arg) = (yyvsp[-1].arg); (yyval.arg).n_indirections++; ;}
    break;

  case 11:
#line 114 "c.y"
    { (yyval.arg) = (yyvsp[-1].arg); strcpy((yyval.arg).name,(yyvsp[0].tok)); ;}
    break;

  case 12:
#line 117 "c.y"
    { (yyval.args) = NULL; ;}
    break;

  case 13:
#line 118 "c.y"
    { (yyval.args) = NULL;	*aarg_push(&(yyval.args)) = (yyvsp[0].arg); ;}
    break;

  case 14:
#line 119 "c.y"
    { (yyval.args) = (yyvsp[-2].args);		*aarg_push(&(yyval.args)) = (yyvsp[0].arg); ;}
    break;

  case 25:
#line 138 "c.y"
    { (yyval.strs) = NULL; ;}
    break;

  case 26:
#line 139 "c.y"
    { ap_push(&(yyvsp[-1].strs), Str_dup((yyvsp[0].str))); (yyval.strs) = (yyvsp[-1].strs); ;}
    break;

  case 27:
#line 142 "c.y"
    { memset(&(yyval.callnames), 0, sizeof((yyval.callnames))); ;}
    break;

  case 28:
#line 143 "c.y"
    { ap_size(&(yyvsp[-1].strs)) ? ap_append(&(yyvsp[-4].callnames)[(yyvsp[-3].mtok)], (yyvsp[-1].strs), ap_size(&(yyvsp[-1].strs))) : ap_push(&(yyvsp[-4].callnames)[(yyvsp[-3].mtok)], NULL); memcpy((yyval.callnames), (yyvsp[-4].callnames), sizeof((yyval.callnames))); ;}
    break;

  case 29:
#line 144 "c.y"
    { ap_push(&(yyvsp[-1].callnames)[(yyvsp[0].mtok)], NULL); memcpy((yyval.callnames), (yyvsp[-1].callnames), sizeof((yyval.callnames))); ;}
    break;

  case 37:
#line 155 "c.y"
    { Str_destroy(&(yyvsp[0].str)); ;}
    break;


      default: break;
    }

/* Line 1126 of yacc.c.  */
#line 1340 "c.tab.c"

  yyvsp -= yylen;
  yyssp -= yylen;
  yylsp -= yylen;

  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  int yytype = YYTRANSLATE (yychar);
	  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
	  YYSIZE_T yysize = yysize0;
	  YYSIZE_T yysize1;
	  int yysize_overflow = 0;
	  char *yymsg = 0;
#	  define YYERROR_VERBOSE_ARGS_MAXIMUM 5
	  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
	  int yyx;

#if 0
	  /* This is so xgettext sees the translatable formats that are
	     constructed on the fly.  */
	  YY_("syntax error, unexpected %s");
	  YY_("syntax error, unexpected %s, expecting %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s or %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
#endif
	  char *yyfmt;
	  char const *yyf;
	  static char const yyunexpected[] = "syntax error, unexpected %s";
	  static char const yyexpecting[] = ", expecting %s";
	  static char const yyor[] = " or %s";
	  char yyformat[sizeof yyunexpected
			+ sizeof yyexpecting - 1
			+ ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
			   * (sizeof yyor - 1))];
	  char const *yyprefix = yyexpecting;

	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  int yyxbegin = yyn < 0 ? -yyn : 0;

	  /* Stay within bounds of both yycheck and yytname.  */
	  int yychecklim = YYLAST - yyn;
	  int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
	  int yycount = 1;

	  yyarg[0] = yytname[yytype];
	  yyfmt = yystpcpy (yyformat, yyunexpected);

	  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      {
		if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
		  {
		    yycount = 1;
		    yysize = yysize0;
		    yyformat[sizeof yyunexpected - 1] = '\0';
		    break;
		  }
		yyarg[yycount++] = yytname[yyx];
		yysize1 = yysize + yytnamerr (0, yytname[yyx]);
		yysize_overflow |= yysize1 < yysize;
		yysize = yysize1;
		yyfmt = yystpcpy (yyfmt, yyprefix);
		yyprefix = yyor;
	      }

	  yyf = YY_(yyformat);
	  yysize1 = yysize + yystrlen (yyf);
	  yysize_overflow |= yysize1 < yysize;
	  yysize = yysize1;

	  if (!yysize_overflow && yysize <= YYSTACK_ALLOC_MAXIMUM)
	    yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg)
	    {
	      /* Avoid sprintf, as that infringes on the user's name space.
		 Don't have undefined behavior even if the translation
		 produced a string with the wrong number of "%s"s.  */
	      char *yyp = yymsg;
	      int yyi = 0;
	      while ((*yyp = *yyf))
		{
		  if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		    {
		      yyp += yytnamerr (yyp, yyarg[yyi++]);
		      yyf += 2;
		    }
		  else
		    {
		      yyp++;
		      yyf++;
		    }
		}
	      yyerror (&yylloc, run, yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    {
	      yyerror (&yylloc, run, YY_("syntax error"));
	      goto yyexhaustedlab;
	    }
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror (&yylloc, run, YY_("syntax error"));
    }

  yyerror_range[0] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
        {
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
        }
      else
	{
	  yydestruct ("Error: discarding", yytoken, &yylval, &yylloc);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (0)
     goto yyerrorlab;

  yyerror_range[0] = yylsp[1-yylen];
  yylsp -= yylen;
  yyvsp -= yylen;
  yyssp -= yylen;
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      yyerror_range[0] = *yylsp;
      yydestruct ("Error: popping", yystos[yystate], yyvsp, yylsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;

  yyerror_range[1] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the look-ahead.  YYLOC is available though. */
  YYLLOC_DEFAULT (yyloc, yyerror_range - 1, 2);
  *++yylsp = yyloc;

  /* Shift the error token. */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, run, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, &yylloc);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yylsp);
      YYPOPSTACK;
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 160 "c.y"


#pragma warning(pop)

static char non_tok_chars[] = ",*(){;"; // if here, return as is, otherwise a TOK

static struct StrKws { char *kw; int tok; } tok_kws[] =
{
	{ "META",		META_TOK	},
	{ "const",		CONST_TOK	},
	{ "void",		VOID_TOK	},
//	{ "typedef",	TYPEDEF		},
//	{ "extern",		EXTERN		},
//	{ "static",		STATIC		},
};

static struct MetaKws { char *kw; int tok; MetaToken mtok; } meta_kws[] =
{
#define METAKWS(word) { #word, word, METATOKEN_##word }
	METAKWS(COMMANDLINE),
	METAKWS(CONSOLE),
	METAKWS(REMOTE),

	METAKWS(ACCESSLEVEL),
	METAKWS(CONSOLE_DESC),
	METAKWS(CONSOLE_REQPARAMS),
};
assert_static(ARRAY_SIZE(meta_kws) == METATOKEN_COUNT);

int yylex_stdin(YYSTYPE *lval, YYLTYPE *line, ParseRun *run)
{
//    static struct TypeKws
    int c;
    char *tok = Str_temp();
    int j;

    yydebug = 1;

    while ((c = getchar ()) == ' ' || c == '\t' || c == '\n' || c == '\r')
    {
        if(c == '\n') { 
        }
    }
    ungetc (c, stdin);

	achr_pop(&tok);
    while(isalnum(c = getchar()) || c == '_')
		achr_push(&tok, (U8)c);
	achr_push(&tok, (U8)c);
    if(!*tok) // no characters grabbed. return
        return c == EOF?0:c;
    ungetc(c,stdin);

	for(j = 0; j < ARRAY_SIZE(tok_kws); ++j)
    {
        if(0 == strcmp(tok_kws[j].kw,tok))
        {
            strcpy(lval->tok,tok);
            return tok_kws[j].tok;
        }
    }
    for(j = 0; j < ARRAY_SIZE(meta_kws); ++j)
    {
        if(0 == strcmp(meta_kws[j].kw,tok))
        {
            lval->mtok = meta_kws[j].mtok;
            return meta_kws[j].tok;
        }
    }

	strcpy(lval->tok,tok);
	Str_destroy(&tok);

	return TOK;
}

int yylex(YYSTYPE *lval, YYLTYPE *line, ParseRun *run)
{
    BOOL beginning_of_line = FALSE;
    char prev;
    int res = 0;
//    static struct TypeKws
//    char tok_scratch[MAX_IDENTIFIER_LEN]; */
   char *i = lval->tok;
   char **ht;
   int j;

   if(run->debug)
       yydebug = 1;
   else
       yydebug = 0;

   if(!run->lex_ctxt) {
      run->lex_ctxt = run->input;
      beginning_of_line = TRUE;
   }
   if(!run->lex_ctxt)
   {
	  WARN("yylex: no input to lexer %s:%i",run->caller_fname,run->line);
      return 0; // 
   }
   else if(!INRANGE(run->lex_ctxt,run->input,run->input+Str_len(&run->input)+1))
   { 
       WARN("yylex: run->lex_ctxt(%p) is not within run->input and its length (%p,%i)", run->lex_ctxt, run->input, Str_len(&run->input));
       return 0;
   }
   ht = &run->lex_ctxt;

   if(!run->loc.first_line)
       run->loc.first_line = 1;

   while (*(*ht) && strchr(" \t\v\r\n",*(*ht)))
   {
       if(*(*ht) == '\n') {
           run->loc.first_line++;
           run->lex_line_start = *ht;
           beginning_of_line = TRUE;
       }
       prev = *(*ht);
      (*ht)++;
   }

   if(!INRANGE(run->lex_line_start,run->input,*ht))
       run->lex_line_start = run->input;

   run->loc.last_line = run->loc.first_line;
   run->loc.first_column = *ht - run->lex_line_start;

#define ADVANCE_TO_NEWLINE        while(*(*ht) && *(*ht) != '\n') (*ht)++


	if(beginning_of_line && *(*ht) == '#') // #include, etc.
	{
		ADVANCE_TO_NEWLINE;
		return yylex(lval, line, run); // recurse
	}
	else if(*(*ht) == '/' && (*ht)[1] == '/') // line comment
	{
		ADVANCE_TO_NEWLINE;
		return yylex(lval, line, run); // recurse
	}
	else if(*(*ht) == '/' && (*ht)[1] == '*') // block comment
	{
		prev = 0;
		(*ht) +=2;
		while(*(*ht) && *(*ht) != '/' && prev != '*')
			prev = *(*ht)++;
		(*ht)++;
		return yylex(lval, line, run); // recurse
	}
	else if(*(*ht) == '"') // string literal
	{
		lval->str = NULL;
		(*ht)++;
		for(;*(*ht);(*ht)++)
		{
			if(*(*ht) == '"' && (*ht)[-1] != '\\')
				break;

			if(achr_size(&lval->str) > MAX_STRING_LITERAL_LEN)
			{
				WARN("Warning: %s(%d): string literal too long", run->fn, run->loc.first_line);
				return -1;
			}
			achr_push(&lval->str,*(*ht));
		}
		achr_push(&lval->str, '\0');
		if(*(*ht))
			(*ht)++;
		res = STR;
	}
	else if(*(*ht) == '\'') // character constant
	{
		*i = *(*ht)++;
		while(*(*ht))
		{
			if(*(*ht) == '\'' && (*ht)[-1] != '\\')
				break;
			*i = *(*ht)++;
		}
		if(*(*ht))
			(*ht)++;
		*i = 0;
		res = CONSTANT;
	}
	else if(*(*ht) == '0' && strchr("xX",(*ht)[1])) // hex
	{
		*i = *(*ht)++;
		*i = *(*ht)++;
		while(*(*ht) && ch_isxdigit(*(*ht)))
			*i = *(*ht)++;
		// apparently floats and ints and hex ar constants
//		if(1 != sscanf(tok,"%h",&lval->num)) */
//			WARN("couldn't scan %s to hex",tok); */
		*i = 0;
		res = CONSTANT;
	}
	else if(ch_isdigit(*(*ht))) // numeric constant
	{
		*i = *(*ht)++;
		while(*(*ht) && ch_isdigit(*(*ht)))
			*i = *(*ht)++;
		*i = 0;
		res = CONSTANT;
	}
	else // TOK
	{
		while(*ht && (ch_isalnum(*(*ht)) || *(*ht) == '_'))
		{
			*i++ = *(*ht)++;
			if(i - lval->tok >= ARRAY_SIZE(lval->tok))
			{
				*i = 0;
				WARN("token %s too long",lval->tok);
				return -1;
			}
		}
		*i = 0;
	}
	run->loc.last_column = *ht - run->lex_line_start;

	line->first_line = run->loc.first_line;
	line->first_column = run->loc.first_column;
	line->last_line = run->loc.last_line;
	line->last_column = run->loc.last_column;

	// already set, e.g. string literal
	if(res)
		return res;

	if(!*lval->tok) // no characters grabbed. '+','=',';' etc.
	{
		char c = *(*ht)++;
		if(strchr(non_tok_chars,c))
			return c;
		// we don't do anything with this yet, return as TOK
		// otherwise we get a parse error when a non TOK is
		// returned in ignored_tok rule
		lval->tok[0] = c;
		lval->tok[1] = 0;
		return TOK;
	}

	for(j = 0; j < ARRAY_SIZE(tok_kws); ++j)
		if(0 == strcmp(tok_kws[j].kw,lval->tok))
			return tok_kws[j].tok;

	for(j = 0; j < ARRAY_SIZE(meta_kws); ++j)
	{
		if(0 == strcmp(meta_kws[j].kw,lval->tok))
		{
			lval->mtok = meta_kws[j].mtok;
			return meta_kws[j].tok;
		}
	}

	return TOK;
}

void yyerror (YYLTYPE *line, ParseRun *run, char const *s)
{
    fprintf (stderr, "%s(%i):%s\n", run->fn,line->first_line,s);
}

static void print_token_value (FILE *strm, int tok, YYSTYPE lval)
{
    int j;
    for(j = 0; j < sizeof(tok_kws)/sizeof(*tok_kws); ++j) {
        if(tok_kws[j].tok == tok) {
            fprintf(strm,"%s",lval.tok);
            return;
        }
    }
    for(j = 0; j < sizeof(meta_kws)/sizeof(*meta_kws); ++j) {
        if(meta_kws[j].tok == tok) {
            fprintf(strm,"%s",meta_kws[j].kw);
            return;
        }
    }
    fprintf(strm,"%s",lval.tok);
}

char *tok_append(char *a,char *b)
{
    char *res;
    int na = strlen(a);
    int nb = strlen(b);
    res = malloc(na+nb+1);
    sprintf_s(res,na+nb+1,"%s%s",a,b);
    return res;
}

