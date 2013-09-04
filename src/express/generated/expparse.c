/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is included that follows the "include" declaration
** in the input grammar file. */
#include <stdio.h>
#line 2 "expparse.y"

#include <assert.h>
#include "token_type.h"
#include "parse_data.h"

int yyerrstatus = 0;
#define yyerrok (yyerrstatus = 0)

YYSTYPE yylval;

    /*
     * YACC grammar for Express parser.
     *
     * This software was developed by U.S. Government employees as part of
     * their official duties and is not subject to copyright.
     *
     * $Log: expparse.y,v $
     * Revision 1.23  1997/11/14 17:09:04  libes
     * allow multiple group references
     *
     * Revision 1.22  1997/01/21 19:35:43  dar
     * made C++ compatible
     *
     * Revision 1.21  1995/06/08  22:59:59  clark
     * bug fixes
     *
     * Revision 1.20  1995/04/08  21:06:07  clark
     * WHERE rule resolution bug fixes, take 2
     *
     * Revision 1.19  1995/04/08  20:54:18  clark
     * WHERE rule resolution bug fixes
     *
     * Revision 1.19  1995/04/08  20:49:08  clark
     * WHERE
     *
     * Revision 1.18  1995/04/05  13:55:40  clark
     * CADDETC preval
     *
     * Revision 1.17  1995/03/09  18:43:47  clark
     * various fixes for caddetc - before interface clause changes
     *
     * Revision 1.16  1994/11/29  20:55:58  clark
     * fix inline comment bug
     *
     * Revision 1.15  1994/11/22  18:32:39  clark
     * Part 11 IS; group reference
     *
     * Revision 1.14  1994/11/10  19:20:03  clark
     * Update to IS
     *
     * Revision 1.13  1994/05/11  19:50:00  libes
     * numerous fixes
     *
     * Revision 1.12  1993/10/15  18:47:26  libes
     * CADDETC certified
     *
     * Revision 1.10  1993/03/19  20:53:57  libes
     * one more, with feeling
     *
     * Revision 1.9  1993/03/19  20:39:51  libes
     * added unique to parameter types
     *
     * Revision 1.8  1993/02/16  03:17:22  libes
     * reorg'd alg bodies to not force artificial begin/ends
     * added flag to differentiate parameters in scopes
     * rewrote query to fix scope handling
     * added support for Where type
     *
     * Revision 1.7  1993/01/19  22:44:17  libes
     * *** empty log message ***
     *
     * Revision 1.6  1992/08/27  23:36:35  libes
     * created fifo for new schemas that are parsed
     * connected entity list to create of oneof
     *
     * Revision 1.5  1992/08/18  17:11:36  libes
     * rm'd extraneous error messages
     *
     * Revision 1.4  1992/06/08  18:05:20  libes
     * prettied up interface to print_objects_when_running
     *
     * Revision 1.3  1992/05/31  23:31:13  libes
     * implemented ALIAS resolution
     *
     * Revision 1.2  1992/05/31  08:30:54  libes
     * multiple files
     *
     * Revision 1.1  1992/05/28  03:52:25  libes
     * Initial revision
     */

#include "express/symbol.h"
#include "express/linklist.h"
#include "stack.h"
#include "express/express.h"
#include "express/schema.h"
#include "express/entity.h"
#include "express/resolve.h"
#include "expscan.h"

    extern int print_objects_while_running;

    int tag_count;	/* use this to count tagged GENERIC types in the formal */
    /* argument lists.  Gross, but much easier to do it this */
    /* way then with the 'help' of yacc. */
    /* Set it to -1 to indicate that tags cannot be defined, */
    /* only used (outside of formal parameter list, i.e. for */
    /* return types).  Hey, as long as */
    /* there's a gross hack sitting around, we might as well */
    /* milk it for all it's worth!  -snc */

    Express yyexpresult;	/* hook to everything built by parser */

    Symbol *interface_schema;	/* schema of interest in use/ref clauses */
    void (*interface_func)();	/* func to attach rename clauses */

    /* record schemas found in a single parse here, allowing them to be */
    /* differentiated from other schemas parsed earlier */
    Linked_List PARSEnew_schemas;

    void SCANskip_to_end_schema(perplex_t scanner);

    int	yylineno;


    static void	yyerror(const char*, char *string);

    bool yyeof = false;

#define MAX_SCOPE_DEPTH	20	/* max number of scopes that can be nested */

    static struct scope {
        struct Scope_ *this_;
        char type;	/* one of OBJ_XXX */
        struct scope *pscope;	/* pointer back to most recent scope */
        /* that has a printable name - for better */
        /* error messages */
    } scopes[MAX_SCOPE_DEPTH], *scope;
#define CURRENT_SCOPE (scope->this_)
#define PREVIOUS_SCOPE ((scope-1)->this_)
#define CURRENT_SCHEMA (scope->this_->u.schema)
#define CURRENT_SCOPE_NAME		(OBJget_symbol(scope->pscope->this_,scope->pscope->type)->name)
#define CURRENT_SCOPE_TYPE_PRINTABLE	(OBJget_type(scope->pscope->type))

    /* ths = new scope to enter */
    /* sym = name of scope to enter into parent.  Some scopes (i.e., increment) */
    /*       are not named, in which case sym should be 0 */
    /*	 This is useful for when a diagnostic is printed, an earlier named */
    /* 	 scoped can be used */
    /* typ = type of scope */
#define PUSH_SCOPE(ths,sym,typ) \
	if (sym) DICTdefine(scope->this_->symbol_table,(sym)->name,(Generic)ths,sym,typ);\
	ths->superscope = scope->this_; \
	scope++;		\
	scope->type = typ;	\
	scope->pscope = (sym?scope:(scope-1)->pscope); \
	scope->this_ = ths; \
	if (sym) { \
		ths->symbol = *(sym); \
	}
#define POP_SCOPE() scope--

    /* PUSH_SCOPE_DUMMY just pushes the scope stack with nothing actually on it */
    /* Necessary for situations when a POP_SCOPE is unnecessary but inevitable */
#define PUSH_SCOPE_DUMMY() scope++

    /* normally the superscope is added by PUSH_SCOPE, but some things (types) */
    /* bother to get pushed so fix them this way */
#define SCOPEadd_super(ths) ths->superscope = scope->this_;

#define ERROR(code)	ERRORreport(code, yylineno)

void parserInitState()
{
    scope = scopes;
    /* no need to define scope->this */
    scope->this_ = yyexpresult;
    scope->pscope = scope;
    scope->type = OBJ_EXPRESS;
    yyexpresult->symbol.name = yyexpresult->u.express->filename;
    yyexpresult->symbol.filename = yyexpresult->u.express->filename;
    yyexpresult->symbol.line = 1;
}
#line 192 "expparse.c"
/* Next is all token values, in a form suitable for use by makeheaders.
** This section will be null unless lemon is run with the -m switch.
*/
/* 
** These constants (all generated automatically by the parser generator)
** specify the various kinds of tokens (terminals) that the parser
** understands. 
**
** Each symbol here is a terminal symbol in the grammar.
*/
/* Make sure the INTERFACE macro is defined.
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/* The next thing included is series of defines which control
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 terminals
**                       and nonterminals.  "int" is used otherwise.
**    YYNOCODE           is a number of type YYCODETYPE which corresponds
**                       to no legal terminal or nonterminal number.  This
**                       number is used to fill in empty slots of the hash 
**                       table.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       have fall-back values which should be used if the
**                       original value of the token will not parse.
**    YYACTIONTYPE       is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 rules and
**                       states combined.  "int" is used otherwise.
**    ParseTOKENTYPE     is the data type used for minor tokens given 
**                       directly to the parser from the tokenizer.
**    YYMINORTYPE        is the data type used for all minor tokens.
**                       This is typically a union of many types, one of
**                       which is ParseTOKENTYPE.  The entry in the union
**                       for base tokens is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    ParseARG_SDECL     A static variable declaration for the %extra_argument
**    ParseARG_PDECL     A parameter declaration for the %extra_argument
**    ParseARG_STORE     Code to store %extra_argument into yypParser
**    ParseARG_FETCH     Code to extract %extra_argument from yypParser
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
*/
#define YYCODETYPE unsigned short int
#define YYNOCODE 280
#define YYACTIONTYPE unsigned short int
#define ParseTOKENTYPE  YYSTYPE 
typedef union {
  int yyinit;
  ParseTOKENTYPE yy0;
  struct qualifier yy46;
  Variable yy91;
  Op_Code yy126;
  struct entity_body yy176;
  Where yy234;
  struct subsuper_decl yy242;
  struct type_flags yy252;
  struct upper_lower yy253;
  Symbol* yy275;
  Type yy297;
  Case_Item yy321;
  Statement yy332;
  Linked_List yy371;
  struct type_either yy378;
  struct subtypes yy385;
  Expression yy401;
  Qualified_Attr* yy457;
  TypeBody yy477;
  Integer yy507;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 0
#endif
#define ParseARG_SDECL  parse_data_t parseData ;
#define ParseARG_PDECL , parse_data_t parseData 
#define ParseARG_FETCH  parse_data_t parseData  = yypParser->parseData 
#define ParseARG_STORE yypParser->parseData  = parseData 
#define YYNSTATE 650
#define YYNRULE 333
#define YY_NO_ACTION      (YYNSTATE+YYNRULE+2)
#define YY_ACCEPT_ACTION  (YYNSTATE+YYNRULE+1)
#define YY_ERROR_ACTION   (YYNSTATE+YYNRULE)

/* The yyzerominor constant is used to initialize instances of
** YYMINORTYPE objects to zero. */
static const YYMINORTYPE yyzerominor = { 0 };

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N < YYNSTATE                  Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   YYNSTATE <= N < YYNSTATE+YYNRULE   Reduce by rule N-YYNSTATE.
**
**   N == YYNSTATE+YYNRULE              A syntax error has occurred.
**
**   N == YYNSTATE+YYNRULE+1            The parser accepts its input.
**
**   N == YYNSTATE+YYNRULE+2            No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as
**
**      yy_action[ yy_shift_ofst[S] + X ]
**
** If the index value yy_shift_ofst[S]+X is out of range or if the value
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X or if yy_shift_ofst[S]
** is equal to YY_SHIFT_USE_DFLT, it means that the action is not in the table
** and that yy_default[S] should be used instead.  
**
** The formula above is for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
*/
#define YY_ACTTAB_COUNT (2631)
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    77,   78,  530,   67,   68,  631,   23,   71,   69,   70,
 /*    10 */    72,  248,   79,   74,   73,   16,   41,  594,   58,   55,
 /*    20 */   232,   63,  592,  395,   59,   59,   67,   68,   77,   78,
 /*    30 */    71,   69,   70,   72,  247,  223,   74,   73,   66,  346,
 /*    40 */    79,  112,  111,   16,   41,  630,  629,  632,  337,  610,
 /*    50 */   527,  395,  102,  613,  623,  608,  112,  111,  607,  624,
 /*    60 */   605,  609,  627,  622,  604,   43,   66,  155,  547,  617,
 /*    70 */   403,  402,   75,  630,  629,  869,  619,  618,  616,  615,
 /*    80 */   614,  353,  623,  471,  458,  470,  473,  624,  469,  394,
 /*    90 */    89,  622,  472,  471,  474,  470,  473,  617,  469,   77,
 /*   100 */    78,  453,  472,  157,  619,  618,  616,  615,  614,  112,
 /*   110 */   111,   79,  236,  586,   16,   41,  587,  394,  522,  588,
 /*   120 */   637,  636,  395,  635,  634,  202,   77,   78,  118,  549,
 /*   130 */   783,  245,   36,  403,  402,   75,  168,   66,   79,  350,
 /*   140 */   409,   16,   41,  553,  630,  629,  113,  344,   35,  395,
 /*   150 */    67,   68,  649,  623,   71,   69,   70,   72,  624,  869,
 /*   160 */    74,   73,  622,  454,   66,  112,  111,   27,  617,  591,
 /*   170 */   529,  630,  629,  142,  520,  619,  618,  616,  615,  614,
 /*   180 */   623,  471,  468,  470,  473,  624,  469,  202,  394,  622,
 /*   190 */   472,  471,  467,  470,  473,  617,  469,   77,   78,  564,
 /*   200 */   472,  623,  619,  618,  616,  615,  614,  626,  123,   79,
 /*   210 */   622,  310,   16,   41,  221,  394,  514,  600,  598,  601,
 /*   220 */   312,  596,  595,  599,  602,  119,  597,   67,   68,  409,
 /*   230 */   382,   71,   69,   70,   72,   66,  152,   74,   73,  984,
 /*   240 */   117,  407,  630,  629,  514,  471,  466,  470,  473,   73,
 /*   250 */   469,  623,  533,  301,  472,  152,  624,  226,  574,  165,
 /*   260 */   622,  405,  142,  514,  166,  783,  617,  403,  348,   75,
 /*   270 */   367,  365,  361,  619,  618,  616,  615,  614,   67,   68,
 /*   280 */   169,  518,   71,   69,   70,   72,  394,  301,   74,   73,
 /*   290 */   589,  586,  114,   19,  587,   26,  167,  588,  637,  636,
 /*   300 */   356,  635,  634,  500,  499,  498,  497,  496,  495,  494,
 /*   310 */   493,  492,  491,    2,   61,  129,   57,  322,  127,  318,
 /*   320 */   346,   82,  500,  499,  498,  497,  496,  495,  494,  493,
 /*   330 */   492,  491,    2,  152,  393,  585,  586,  127,  103,  587,
 /*   340 */   335,  514,  588,  637,  636,    3,  635,  634,  249,  152,
 /*   350 */   120,  333,   15,   14,  203,   12,  131,  514,   13,  574,
 /*   360 */   647,  562,  737,  246,    3,  173,  584,  244,  243,  580,
 /*   370 */   579,  242,  240,  412,  182,  413,  178,  411,  355,  455,
 /*   380 */   625,   14,  203,   44,  120,  446,   13,   91,  327,  335,
 /*   390 */   471,  465,  470,  473,  304,  469,  172,  171,  451,  472,
 /*   400 */   500,  499,  498,  497,  496,  495,  494,  493,  492,  491,
 /*   410 */     2,  531,  555,  405,  222,  127,  500,  499,  498,  497,
 /*   420 */   496,  495,  494,  493,  492,  491,    2,  113,  477,  152,
 /*   430 */   392,  127,  384,   71,   69,   70,   72,  514,  645,   74,
 /*   440 */    73,  406,    3,  377,  646,  644,  643,  152,   42,  642,
 /*   450 */   641,  640,  639,  116,  521,  514,  548,  106,    3,  217,
 /*   460 */     5,  383,  647,  168,    9,   20,  482,  481,  480,  439,
 /*   470 */   553,  315,  335,  438,  354,   29,  222,  642,  641,  640,
 /*   480 */   639,  116,  331,   14,  203,  334,  184,   61,   13,  185,
 /*   490 */   332,   40,  352,  525,  237,  545,  500,  499,  498,  497,
 /*   500 */   496,  495,  494,  493,  492,  491,    2,  370,  539,  417,
 /*   510 */   331,  127,  181,  565,  500,  499,  498,  497,  496,  495,
 /*   520 */   494,  493,  492,  491,    2,  471,  463,  470,  473,  127,
 /*   530 */   469,  152,  564,   61,  472,   10,  349,   39,    3,  514,
 /*   540 */    84,  575,  586,  234,  638,  587,   90,  152,  588,  637,
 /*   550 */   636,   61,  635,  634,  316,  514,    3,   61,  231,  647,
 /*   560 */   568,  311,  368,  382,  228,  526,    9,   20,  482,  481,
 /*   570 */   480,  525,  556,  561,  132,   76,  490,   61,   61,  642,
 /*   580 */   641,  640,  639,  116,  162,  100,  556,  429,   14,  203,
 /*   590 */    61,  299,  489,   13,   21,  187,  109,  201,  500,  499,
 /*   600 */   498,  497,  496,  495,  494,  493,  492,  491,    2,  298,
 /*   610 */   419,  297,  331,  127,  500,  499,  498,  497,  496,  495,
 /*   620 */   494,  493,  492,  491,    2,  152,  487,  486,  362,  127,
 /*   630 */   610,   61,  533,  514,  613,  268,  608,  170,   98,  607,
 /*   640 */     3,  605,  609,  523,  553,  604,   43,  151,  156,  198,
 /*   650 */   215,  610,  208,  406,  425,  613,    3,  608,  378,  174,
 /*   660 */   607,   87,  605,  609,   24,  306,  604,   43,  279,  156,
 /*   670 */   351,    9,   20,  482,  481,  480,  483,  238,  647,   85,
 /*   680 */   567,  381,  380,  315,  642,  641,  640,  639,  116,  377,
 /*   690 */   379,  563,  500,  499,  498,  497,  496,  495,  494,  493,
 /*   700 */   492,  491,    2,  128,   67,   68,  362,  127,   71,   69,
 /*   710 */    70,   72,  245,  375,   74,   73,  376,  331,  374,   52,
 /*   720 */    50,   53,   46,   48,   47,   51,   54,   45,   49,  554,
 /*   730 */   507,   58,   55,  245,    3,  516,  647,   59,  124,  471,
 /*   740 */   462,  470,  473,  620,  469,  227,  448,   60,  472,   52,
 /*   750 */    50,   53,   46,   48,   47,   51,   54,   45,   49,  230,
 /*   760 */   214,   58,   55,  229,  573,  586,  224,   59,  587,  369,
 /*   770 */   366,  588,  637,  636,  137,  635,  634,  364,  633,  363,
 /*   780 */    52,   50,   53,   46,   48,   47,   51,   54,   45,   49,
 /*   790 */   108,  104,   58,   55,  360,  572,  586,  105,   59,  587,
 /*   800 */   519,  357,  588,  637,  636,  115,  635,  634,   56,  220,
 /*   810 */    52,   50,   53,   46,   48,   47,   51,   54,   45,   49,
 /*   820 */   219,  216,   58,   55,   32,  211,  206,   18,   59,  205,
 /*   830 */    52,   50,   53,   46,   48,   47,   51,   54,   45,   49,
 /*   840 */   347,   80,   58,   55,   25,  871,  204,  342,   59,  471,
 /*   850 */   461,  470,  473,  464,  469,   97,  196,  612,  472,  628,
 /*   860 */   200,   52,   50,   53,   46,   48,   47,   51,   54,   45,
 /*   870 */    49,   95,  158,   58,   55,  336,   93,   92,  457,   59,
 /*   880 */   191,   52,   50,   53,   46,   48,   47,   51,   54,   45,
 /*   890 */    49,  192,  197,   58,   55,  133,  430,  186,  188,   59,
 /*   900 */   471,  460,  470,  473,  324,  469,  422,  323,  606,  472,
 /*   910 */    52,   50,   53,   46,   48,   47,   51,   54,   45,   49,
 /*   920 */   321,  183,   58,   55,  471,  459,  470,  473,   59,  469,
 /*   930 */   420,  319,  179,  472,  317,  175,  176,  582,    8,   52,
 /*   940 */    50,   53,   46,   48,   47,   51,   54,   45,   49,  114,
 /*   950 */   121,   58,   55,  359,  445,  195,  325,   59,   11,   61,
 /*   960 */   449,  648,  611,  471,  199,  470,  473,  647,  469,  141,
 /*   970 */   388,  140,  472,   52,   50,   53,   46,   48,   47,   51,
 /*   980 */    54,   45,   49,  253,   21,   58,   55,  583,  593,  251,
 /*   990 */    38,   59,  581,   52,   50,   53,   46,   48,   47,   51,
 /*  1000 */    54,   45,   49,  241,  577,   58,   55,  239,  134,  578,
 /*  1010 */   576,   59,  235,   52,   50,   53,   46,   48,   47,   51,
 /*  1020 */    54,   45,   49,  571,   86,   58,   55,  471,  125,  470,
 /*  1030 */   473,   59,  469,   37,  570,  586,  472,   83,  587,  152,
 /*  1040 */   210,  588,  637,  636,  566,  635,  634,  514,    6,  139,
 /*  1050 */    52,   50,   53,   46,   48,   47,   51,   54,   45,   49,
 /*  1060 */   138,  110,   58,   55,  233,  546,   14,  203,   59,  373,
 /*  1070 */   544,   13,  574,   31,   81,   52,   50,   53,   46,   48,
 /*  1080 */    47,   51,   54,   45,   49,  543,  542,   58,   55,  302,
 /*  1090 */   541,  574,  540,   59,  537,  163,  536,  161,  584,  244,
 /*  1100 */   243,  580,  579,  242,  240,  534,  500,  499,  498,  497,
 /*  1110 */   496,  495,  494,  493,  492,  491,  513,  367,  535,  532,
 /*  1120 */    27,  127,   30,  254,  213,  517,  512,  511,  160,  159,
 /*  1130 */   343,  510,  574,  212,  509,   28,  246,  525,  173,  584,
 /*  1140 */   244,  243,  580,  579,  242,  240,  152,  286,  450,  479,
 /*  1150 */    20,  482,  481,  480,  514,  508,    4,  504,  502,  501,
 /*  1160 */   647,  330,  642,  641,  640,  639,  116,  610,  207,  172,
 /*  1170 */   171,  613,  149,  608,    1,  303,  607,  488,  605,  609,
 /*  1180 */   484,  476,  604,   43,  151,  156,  136,  456,   99,  569,
 /*  1190 */   586,   96,  452,  587,  442,  331,  588,  637,  636,  447,
 /*  1200 */   635,  634,  252,   22,  444,  193,  194,  443,  329,  524,
 /*  1210 */   122,  440,  441,  500,  499,  498,  497,  496,  495,  494,
 /*  1220 */   493,  492,  491,  485,  190,  389,  610,  431,  127,  300,
 /*  1230 */   613,  278,  608,  326,  428,  607,   17,  605,  609,  427,
 /*  1240 */   424,  604,   43,  151,  156,  426,  423,  180,  525,  245,
 /*  1250 */   610,  421,  130,  320,  613,  149,  608,  314,  309,  607,
 /*  1260 */   418,  605,  609,  416,  415,  604,   43,  151,  156,  414,
 /*  1270 */   177,  558,  362,   52,   50,   53,   46,   48,   47,   51,
 /*  1280 */    54,   45,   49,  410,  408,   58,   55,  390,  610,  308,
 /*  1290 */    88,   59,  613,  278,  608,  387,  386,  607,  385,  605,
 /*  1300 */   609,  551,  552,  604,   43,  151,  156,  610,  245,  305,
 /*  1310 */   358,  613,  276,  608,  550,  372,  607,  371,  605,  609,
 /*  1320 */   524,  538,  604,   43,  151,  156,  525,   34,  503,    7,
 /*  1330 */   338,  101,  245,  339,  340,  341,  285,  107,  525,  218,
 /*  1340 */   623,  135,   94,  603,  529,  525,  313,  610,  307,  622,
 /*  1350 */    33,  613,  266,  608,  557,  284,  607,  516,  605,  609,
 /*  1360 */    62,  621,  604,   43,  151,  156,  560,  559,  515,  505,
 /*  1370 */   245,   65,  478,   64,  506,  623,  475,  328,  126,  985,
 /*  1380 */   164,  985,  985,  362,  622,  985,  647,  209,  985,  245,
 /*  1390 */   610,  985,  985,  985,  613,  265,  608,  985,  524,  607,
 /*  1400 */   985,  605,  609,  985,  528,  604,   43,  151,  156,  610,
 /*  1410 */   524,  985,  985,  613,  590,  608,  985,  524,  607,  985,
 /*  1420 */   605,  609,  985,  985,  604,   43,  151,  156,  610,  245,
 /*  1430 */   985,  985,  613,  391,  608,  985,  985,  607,  985,  605,
 /*  1440 */   609,  985,  985,  604,   43,  151,  156,  610,  985,  985,
 /*  1450 */   985,  613,  264,  608,  985,  985,  607,  985,  605,  609,
 /*  1460 */   985,  362,  604,   43,  151,  156,  985,  225,  586,  985,
 /*  1470 */   985,  587,  245,  362,  588,  637,  636,  610,  635,  634,
 /*  1480 */   362,  613,  263,  608,  985,  985,  607,  985,  605,  609,
 /*  1490 */   985,  245,  604,   43,  151,  156,  985,  985,  985,  985,
 /*  1500 */   985,  985,  610,  985,  985,  985,  613,  404,  608,  985,
 /*  1510 */   245,  607,  985,  605,  609,  985,  985,  604,   43,  151,
 /*  1520 */   156,  985,  610,  985,  985,  985,  613,  296,  608,  245,
 /*  1530 */   985,  607,  985,  605,  609,  985,  985,  604,   43,  151,
 /*  1540 */   156,  985,  985,  985,  985,  985,  610,  985,  985,  985,
 /*  1550 */   613,  295,  608,  985,  985,  607,  985,  605,  609,  245,
 /*  1560 */   985,  604,   43,  151,  156,  610,  985,  985,  985,  613,
 /*  1570 */   294,  608,  985,  985,  607,  985,  605,  609,  985,  985,
 /*  1580 */   604,   43,  151,  156,  245,  985,  985,  985,  985,  985,
 /*  1590 */   610,  985,  985,  985,  613,  293,  608,  985,  985,  607,
 /*  1600 */   985,  605,  609,  985,  245,  604,   43,  151,  156,  610,
 /*  1610 */   985,  985,  985,  613,  292,  608,  985,  985,  607,  985,
 /*  1620 */   605,  609,  985,  985,  604,   43,  151,  156,  245,  985,
 /*  1630 */   985,  985,  985,  985,  610,  985,  985,  985,  613,  291,
 /*  1640 */   608,  985,  985,  607,  985,  605,  609,  245,  985,  604,
 /*  1650 */    43,  151,  156,  985,  985,  985,  985,  985,  985,  610,
 /*  1660 */   985,  985,  359,  613,  290,  608,  985,  985,  607,  449,
 /*  1670 */   605,  609,  245,  985,  604,   43,  151,  156,  610,  406,
 /*  1680 */   985,  985,  613,  289,  608,  985,  985,  607,  985,  605,
 /*  1690 */   609,  245,  985,  604,   43,  151,  156,  610,  251,  985,
 /*  1700 */   985,  613,  288,  608,  985,  985,  607,  250,  605,  609,
 /*  1710 */   985,  985,  604,   43,  151,  156,  245,  134,  610,  985,
 /*  1720 */   985,  985,  613,  287,  608,  985,  985,  607,  985,  605,
 /*  1730 */   609,  985,  985,  604,   43,  151,  156,  985,  985,  985,
 /*  1740 */   985,  245,  985,  985,  985,  985,  610,  985,  985,  985,
 /*  1750 */   613,  277,  608,  985,  985,  607,  985,  605,  609,  985,
 /*  1760 */   245,  604,   43,  151,  156,  985,  610,  985,  985,  985,
 /*  1770 */   613,  262,  608,  985,  985,  607,  985,  605,  609,  245,
 /*  1780 */   985,  604,   43,  151,  156,  985,  985,  610,  985,  985,
 /*  1790 */   985,  613,  261,  608,  985,  985,  607,  985,  605,  609,
 /*  1800 */   245,  985,  604,   43,  151,  156,  610,  985,  985,  985,
 /*  1810 */   613,  260,  608,  985,  985,  607,  985,  605,  609,  985,
 /*  1820 */   985,  604,   43,  151,  156,  610,  985,  985,  245,  613,
 /*  1830 */   275,  608,  985,  985,  607,  985,  605,  609,  985,  985,
 /*  1840 */   604,   43,  151,  156,  985,  985,  610,  985,  245,  985,
 /*  1850 */   613,  274,  608,  985,  985,  607,  985,  605,  609,  985,
 /*  1860 */   985,  604,   43,  151,  156,  985,  985,  985,  985,  245,
 /*  1870 */   985,  985,  985,  985,  610,  985,  985,  985,  613,  259,
 /*  1880 */   608,  985,  985,  607,  985,  605,  609,  985,  245,  604,
 /*  1890 */    43,  151,  156,  985,  610,  985,  985,  985,  613,  273,
 /*  1900 */   608,  985,  985,  607,  985,  605,  609,  245,  985,  604,
 /*  1910 */    43,  151,  156,  985,  985,  610,  985,  985,  985,  613,
 /*  1920 */   148,  608,  985,  985,  607,  985,  605,  609,  245,  985,
 /*  1930 */   604,   43,  151,  156,  610,  985,  985,  985,  613,  147,
 /*  1940 */   608,  985,  985,  607,  985,  605,  609,  985,  985,  604,
 /*  1950 */    43,  151,  156,  610,  985,  985,  245,  613,  258,  608,
 /*  1960 */   985,  985,  607,  985,  605,  609,  985,  985,  604,   43,
 /*  1970 */   151,  156,  985,  985,  610,  985,  245,  985,  613,  257,
 /*  1980 */   608,  985,  985,  607,  985,  605,  609,  985,  985,  604,
 /*  1990 */    43,  151,  156,  985,  985,  985,  985,  245,  985,  985,
 /*  2000 */   985,  985,  610,  985,  985,  985,  613,  256,  608,  985,
 /*  2010 */   985,  607,  985,  605,  609,  985,  245,  604,   43,  151,
 /*  2020 */   156,  985,  610,  985,  985,  985,  613,  146,  608,  985,
 /*  2030 */   985,  607,  985,  605,  609,  245,  985,  604,   43,  151,
 /*  2040 */   156,  985,  985,  610,  985,  985,  985,  613,  272,  608,
 /*  2050 */   985,  985,  607,  985,  605,  609,  245,  985,  604,   43,
 /*  2060 */   151,  156,  610,  985,  985,  985,  613,  255,  608,  985,
 /*  2070 */   985,  607,  985,  605,  609,  985,  985,  604,   43,  151,
 /*  2080 */   156,  610,  985,  985,  245,  613,  271,  608,  985,  985,
 /*  2090 */   607,  985,  605,  609,  985,  985,  604,   43,  151,  156,
 /*  2100 */   985,  985,  610,  985,  245,  985,  613,  270,  608,  985,
 /*  2110 */   985,  607,  985,  605,  609,  985,  985,  604,   43,  151,
 /*  2120 */   156,  985,  985,  985,  985,  245,  985,  985,  985,  985,
 /*  2130 */   610,  985,  985,  985,  613,  269,  608,  985,  985,  607,
 /*  2140 */   985,  605,  609,  985,  245,  604,   43,  151,  156,  985,
 /*  2150 */   610,  985,  985,  985,  613,  145,  608,  985,  985,  607,
 /*  2160 */   985,  605,  609,  245,  985,  604,   43,  151,  156,  985,
 /*  2170 */   985,  610,  985,  985,  985,  613,  267,  608,  985,  985,
 /*  2180 */   607,  985,  605,  609,  245,  985,  604,   43,  151,  156,
 /*  2190 */   305,  358,  345,  586,  985,  985,  587,  985,  985,  588,
 /*  2200 */   637,  636,  985,  635,  634,  985,  985,  610,   34,  985,
 /*  2210 */     7,  613,  245,  608,  985,  985,  607,  985,  605,  609,
 /*  2220 */   218,  623,  604,   43,  401,  156,  985,  985,  985,  985,
 /*  2230 */   622,   33,  245,  435,  985,  985,  436,  434,  985,  437,
 /*  2240 */   637,  636,  985,  635,  634,  985,  432,  985,  985,  985,
 /*  2250 */   985,  985,  985,  245,  985,  506,  985,  985,  610,  126,
 /*  2260 */   985,  164,  613,  985,  608,  985,  985,  607,  209,  605,
 /*  2270 */   609,  433,  985,  604,   43,  400,  156,  985,  985,  985,
 /*  2280 */   985,  985,  610,  985,  985,  985,  613,  985,  608,  245,
 /*  2290 */   985,  607,  985,  605,  609,  985,  189,  604,   43,  399,
 /*  2300 */   156,  985,  985,  610,  985,  985,  985,  613,  985,  608,
 /*  2310 */   985,  985,  607,  985,  605,  609,  985,  985,  604,   43,
 /*  2320 */   398,  156,  985,  985,  985,  985,  985,  985,  985,  985,
 /*  2330 */   985,  985,  985,  985,  985,  610,  985,  985,  985,  613,
 /*  2340 */   245,  608,  985,  985,  607,  985,  605,  609,  985,  985,
 /*  2350 */   604,   43,  397,  156,  985,  985,  985,  985,  985,  610,
 /*  2360 */   985,  985,  985,  613,  245,  608,  985,  985,  607,  985,
 /*  2370 */   605,  609,  985,  985,  604,   43,  396,  156,  985,  985,
 /*  2380 */   985,  985,  985,  610,  985,  245,  985,  613,  985,  608,
 /*  2390 */   985,  985,  607,  985,  605,  609,  985,  985,  604,   43,
 /*  2400 */   283,  156,  985,  610,  985,  985,  985,  613,  985,  608,
 /*  2410 */   985,  985,  607,  985,  605,  609,  985,  245,  604,   43,
 /*  2420 */   282,  156,  610,  985,  985,  985,  613,  985,  608,  985,
 /*  2430 */   985,  607,  985,  605,  609,  985,  985,  604,   43,  144,
 /*  2440 */   156,  245,  610,  985,  985,  985,  613,  985,  608,  985,
 /*  2450 */   985,  607,  985,  605,  609,  985,  985,  604,   43,  143,
 /*  2460 */   156,  985,  985,  610,  985,  245,  985,  613,  985,  608,
 /*  2470 */   985,  985,  607,  985,  605,  609,  985,  985,  604,   43,
 /*  2480 */   150,  156,  985,  985,  985,  245,  985,  985,  985,  985,
 /*  2490 */   985,  610,  985,  985,  985,  613,  985,  608,  985,  985,
 /*  2500 */   607,  985,  605,  609,  245,  985,  604,   43,  280,  156,
 /*  2510 */   985,  610,  985,  985,  985,  613,  985,  608,  985,  985,
 /*  2520 */   607,  985,  605,  609,  245,  985,  604,   43,  281,  156,
 /*  2530 */   985,  610,  985,  985,  985,  613,  985,  608,  985,  985,
 /*  2540 */   607,  985,  605,  609,  985,  245,  604,   43,  610,  154,
 /*  2550 */   985,  985,  613,  985,  608,  985,  985,  607,  985,  605,
 /*  2560 */   609,  985,  985,  604,   43,  985,  153,  985,  985,  985,
 /*  2570 */   985,  985,  985,  245,  985,  985,  985,  985,  985,  985,
 /*  2580 */   985,  985,  985,  985,  985,  985,  985,  985,  985,  985,
 /*  2590 */   985,  985,  985,  245,  985,  985,  985,  985,  985,  985,
 /*  2600 */   985,  985,  985,  985,  985,  985,  985,  985,  985,  985,
 /*  2610 */   985,  985,  985,  245,  985,  985,  985,  985,  985,  985,
 /*  2620 */   985,  985,  985,  985,  985,  985,  985,  985,  985,  985,
 /*  2630 */   245,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */    11,   12,  174,   11,   12,   30,   31,   15,   16,   17,
 /*    10 */    18,   80,   23,   21,   22,   26,   27,   28,   13,   14,
 /*    20 */   180,   29,   30,   34,   19,   19,   11,   12,   11,   12,
 /*    30 */    15,   16,   17,   18,  206,  207,   21,   22,   49,  136,
 /*    40 */    23,   19,   20,   26,   27,   56,   57,   30,   29,  127,
 /*    50 */    28,   34,   33,  131,   65,  133,   19,   20,  136,   70,
 /*    60 */   138,  139,   34,   74,  142,  143,   49,  145,  228,   80,
 /*    70 */    24,   25,   26,   56,   57,   27,   87,   88,   89,   90,
 /*    80 */    91,   34,   65,  212,  213,  214,  215,   70,  217,  100,
 /*    90 */    29,   74,  221,  212,  213,  214,  215,   80,  217,   11,
 /*   100 */    12,  167,  221,  169,   87,   88,   89,   90,   91,   19,
 /*   110 */    20,   23,  211,  212,   26,   27,  215,  100,   28,  218,
 /*   120 */   219,  220,   34,  222,  223,  191,   11,   12,  178,  179,
 /*   130 */    27,  209,   29,   24,   25,   26,  186,   49,   23,   51,
 /*   140 */   241,   26,   27,  193,   56,   57,  243,  244,   39,   34,
 /*   150 */    11,   12,  253,   65,   15,   16,   17,   18,   70,  111,
 /*   160 */    21,   22,   74,  167,   49,   19,   20,  120,   80,   30,
 /*   170 */    34,   56,   57,  274,   28,   87,   88,   89,   90,   91,
 /*   180 */    65,  212,  213,  214,  215,   70,  217,  191,  100,   74,
 /*   190 */   221,  212,  213,  214,  215,   80,  217,   11,   12,   34,
 /*   200 */   221,   65,   87,   88,   89,   90,   91,   34,  128,   23,
 /*   210 */    74,  171,   26,   27,  134,  100,  136,    1,    2,    3,
 /*   220 */    34,    5,    6,    7,    8,   60,   10,   11,   12,  241,
 /*   230 */    65,   15,   16,   17,   18,   49,  128,   21,   22,  251,
 /*   240 */   252,  253,   56,   57,  136,  212,  213,  214,  215,   22,
 /*   250 */   217,   65,  212,  170,  221,  128,   70,   29,   34,   31,
 /*   260 */    74,  129,  274,  136,   40,   27,   80,   24,   25,   26,
 /*   270 */   113,  114,  115,   87,   88,   89,   90,   91,   11,   12,
 /*   280 */    31,  173,   15,   16,   17,   18,  100,  170,   21,   22,
 /*   290 */   211,  212,   58,   29,  215,   31,   72,  218,  219,  220,
 /*   300 */   173,  222,  223,  195,  196,  197,  198,  199,  200,  201,
 /*   310 */   202,  203,  204,  205,   26,  183,   49,   83,  210,   85,
 /*   320 */   136,   33,  195,  196,  197,  198,  199,  200,  201,  202,
 /*   330 */   203,  204,  205,  128,   27,  211,  212,  210,   29,  215,
 /*   340 */    31,  136,  218,  219,  220,  237,  222,  223,  235,  128,
 /*   350 */   267,  268,  239,  149,  150,  151,  152,  136,  154,   34,
 /*   360 */   111,  229,    0,   38,  237,   40,   41,   42,   43,   44,
 /*   370 */    45,   46,   47,  260,  261,  262,  263,  264,  173,   28,
 /*   380 */    28,  149,  150,   31,  267,  268,  154,   29,  156,   31,
 /*   390 */   212,  213,  214,  215,  173,  217,   71,   72,  168,  221,
 /*   400 */   195,  196,  197,  198,  199,  200,  201,  202,  203,  204,
 /*   410 */   205,   28,   34,  129,   31,  210,  195,  196,  197,  198,
 /*   420 */   199,  200,  201,  202,  203,  204,  205,  243,  244,  128,
 /*   430 */    34,  210,   66,   15,   16,   17,   18,  136,  234,   21,
 /*   440 */    22,   79,  237,   65,  240,  241,  242,  128,  101,  245,
 /*   450 */   246,  247,  248,  249,   28,  136,  179,   31,  237,   77,
 /*   460 */    78,   95,  111,  186,  232,  233,  234,  235,  236,   28,
 /*   470 */   193,  109,   31,   28,  173,   27,   31,  245,  246,  247,
 /*   480 */   248,  249,  278,  149,  150,  255,   28,   26,  154,   31,
 /*   490 */   156,   29,  173,  136,   33,  212,  195,  196,  197,  198,
 /*   500 */   199,  200,  201,  202,  203,  204,  205,  224,  129,   28,
 /*   510 */   278,  210,   31,  229,  195,  196,  197,  198,  199,  200,
 /*   520 */   201,  202,  203,  204,  205,  212,  213,  214,  215,  210,
 /*   530 */   217,  128,   34,   26,  221,  160,  161,   29,  237,  136,
 /*   540 */    33,  211,  212,  164,  165,  215,  265,  128,  218,  219,
 /*   550 */   220,   26,  222,  223,  273,  136,  237,   26,   33,  111,
 /*   560 */    95,  182,  125,   65,   33,  208,  232,  233,  234,  235,
 /*   570 */   236,  136,  175,  176,  276,  277,  173,   26,   26,  245,
 /*   580 */   246,  247,  248,  249,   33,   33,  175,  176,  149,  150,
 /*   590 */    26,  171,  173,  154,   27,  156,  159,   33,  195,  196,
 /*   600 */   197,  198,  199,  200,  201,  202,  203,  204,  205,  185,
 /*   610 */   257,  258,  278,  210,  195,  196,  197,  198,  199,  200,
 /*   620 */   201,  202,  203,  204,  205,  128,  123,  124,  271,  210,
 /*   630 */   127,   26,  212,  136,  131,  132,  133,  186,   33,  136,
 /*   640 */   237,  138,  139,  208,  193,  142,  143,  144,  145,  140,
 /*   650 */   157,  127,  148,   79,  230,  131,  237,  133,   34,   33,
 /*   660 */   136,   33,  138,  139,   39,  162,  142,  143,  144,  145,
 /*   670 */   173,  232,  233,  234,  235,  236,  135,   33,  111,   33,
 /*   680 */    66,   25,   34,  109,  245,  246,  247,  248,  249,   65,
 /*   690 */    24,   34,  195,  196,  197,  198,  199,  200,  201,  202,
 /*   700 */   203,  204,  205,   29,   11,   12,  271,  210,   15,   16,
 /*   710 */    17,   18,  209,   34,   21,   22,   25,  278,   24,    1,
 /*   720 */     2,    3,    4,    5,    6,    7,    8,    9,   10,   34,
 /*   730 */   237,   13,   14,  209,  237,  194,  111,   19,   29,  212,
 /*   740 */   213,  214,  215,   50,  217,   34,  237,   29,  221,    1,
 /*   750 */     2,    3,    4,    5,    6,    7,    8,    9,   10,   33,
 /*   760 */   256,   13,   14,   33,  211,  212,   61,   19,  215,   36,
 /*   770 */    33,  218,  219,  220,   27,  222,  223,  115,   30,   33,
 /*   780 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*   790 */    27,   27,   13,   14,   33,  211,  212,   27,   19,  215,
 /*   800 */    34,   34,  218,  219,  220,   36,  222,  223,   29,   37,
 /*   810 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*   820 */    55,   77,   13,   14,   39,  104,  104,   29,   19,   53,
 /*   830 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*   840 */    34,   29,   13,   14,   39,  111,   59,   29,   19,  212,
 /*   850 */   213,  214,  215,   34,  217,   33,   97,   28,  221,   50,
 /*   860 */    33,    1,    2,    3,    4,    5,    6,    7,    8,    9,
 /*   870 */    10,   33,   33,   13,   14,   34,   33,   29,   34,   19,
 /*   880 */    33,    1,    2,    3,    4,    5,    6,    7,    8,    9,
 /*   890 */    10,  116,   93,   13,   14,   27,    1,  106,   68,   19,
 /*   900 */   212,  213,  214,  215,   34,  217,   27,   36,   28,  221,
 /*   910 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*   920 */    84,   34,   13,   14,  212,  213,  214,  215,   19,  217,
 /*   930 */    34,   82,   34,  221,   84,   34,  108,   28,  238,    1,
 /*   940 */     2,    3,    4,    5,    6,    7,    8,    9,   10,   58,
 /*   950 */   269,   13,   14,   62,  270,  155,  153,   19,  239,   26,
 /*   960 */    69,  237,  102,  212,  213,  214,  215,  111,  217,  237,
 /*   970 */   227,   33,  221,    1,    2,    3,    4,    5,    6,    7,
 /*   980 */     8,    9,   10,   92,   27,   13,   14,  141,  157,   98,
 /*   990 */    27,   19,  141,    1,    2,    3,    4,    5,    6,    7,
 /*  1000 */     8,    9,   10,  141,   96,   13,   14,  141,  117,  189,
 /*  1010 */   189,   19,  137,    1,    2,    3,    4,    5,    6,    7,
 /*  1020 */     8,    9,   10,   95,  192,   13,   14,  212,  213,  214,
 /*  1030 */   215,   19,  217,   39,  211,  212,  221,  192,  215,  128,
 /*  1040 */    28,  218,  219,  220,  237,  222,  223,  136,   76,  184,
 /*  1050 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*  1060 */    86,   95,   13,   14,  181,  228,  149,  150,   19,   34,
 /*  1070 */   212,  154,   34,   81,  190,    1,    2,    3,    4,    5,
 /*  1080 */     6,    7,    8,    9,   10,  212,  212,   13,   14,   32,
 /*  1090 */   212,   34,  237,   19,   66,   38,  237,   40,   41,   42,
 /*  1100 */    43,   44,   45,   46,   47,  174,  195,  196,  197,  198,
 /*  1110 */   199,  200,  201,  202,  203,  204,  205,  113,  237,  212,
 /*  1120 */   120,  210,   48,  237,  148,  237,  237,  237,   71,   72,
 /*  1130 */    73,  237,   34,  147,  237,  118,   38,  136,   40,   41,
 /*  1140 */    42,   43,   44,   45,   46,   47,  128,  146,   34,  232,
 /*  1150 */   233,  234,  235,  236,  136,  237,  237,  237,  237,  237,
 /*  1160 */   111,   63,  245,  246,  247,  248,  249,  127,  147,   71,
 /*  1170 */    72,  131,  132,  133,  237,  170,  136,  237,  138,  139,
 /*  1180 */   237,  237,  142,  143,  144,  145,  254,   34,  192,  211,
 /*  1190 */   212,  192,  237,  215,   34,  278,  218,  219,  220,  237,
 /*  1200 */   222,  223,  237,  163,  237,  168,  272,  237,  110,  208,
 /*  1210 */    27,  172,  237,  195,  196,  197,  198,  199,  200,  201,
 /*  1220 */   202,  203,  204,  205,   27,  126,  127,  237,  210,  170,
 /*  1230 */   131,  132,  133,  175,  237,  136,  119,  138,  139,  237,
 /*  1240 */   230,  142,  143,  144,  145,   34,  237,  259,  136,  209,
 /*  1250 */   127,  237,   27,   34,  131,  132,  133,  158,  146,  136,
 /*  1260 */   257,  138,  139,  237,  237,  142,  143,  144,  145,  237,
 /*  1270 */   259,  231,  271,    1,    2,    3,    4,    5,    6,    7,
 /*  1280 */     8,    9,   10,  237,  237,   13,   14,  126,  127,  177,
 /*  1290 */   188,   19,  131,  132,  133,  227,  227,  136,  227,  138,
 /*  1300 */   139,  237,  193,  142,  143,  144,  145,  127,  209,   34,
 /*  1310 */    35,  131,  132,  133,  237,  227,  136,  227,  138,  139,
 /*  1320 */   208,  129,  142,  143,  144,  145,  136,   52,  237,   54,
 /*  1330 */   227,  188,  209,  227,  227,  227,  146,   27,  136,   64,
 /*  1340 */    65,  237,  188,  194,   34,  136,  166,  127,  146,   74,
 /*  1350 */    75,  131,  132,  133,  231,  146,  136,  194,  138,  139,
 /*  1360 */   226,  266,  142,  143,  144,  145,  237,  237,  237,  130,
 /*  1370 */   209,  187,  237,  187,   99,   65,   67,   34,  103,  279,
 /*  1380 */   105,  279,  279,  271,   74,  279,  111,  112,  279,  209,
 /*  1390 */   127,  279,  279,  279,  131,  132,  133,  279,  208,  136,
 /*  1400 */   279,  138,  139,  279,   94,  142,  143,  144,  145,  127,
 /*  1410 */   208,  279,  279,  131,  132,  133,  279,  208,  136,  279,
 /*  1420 */   138,  139,  279,  279,  142,  143,  144,  145,  127,  209,
 /*  1430 */   279,  279,  131,  132,  133,  279,  279,  136,  279,  138,
 /*  1440 */   139,  279,  279,  142,  143,  144,  145,  127,  279,  279,
 /*  1450 */   279,  131,  132,  133,  279,  279,  136,  279,  138,  139,
 /*  1460 */   279,  271,  142,  143,  144,  145,  279,  211,  212,  279,
 /*  1470 */   279,  215,  209,  271,  218,  219,  220,  127,  222,  223,
 /*  1480 */   271,  131,  132,  133,  279,  279,  136,  279,  138,  139,
 /*  1490 */   279,  209,  142,  143,  144,  145,  279,  279,  279,  279,
 /*  1500 */   279,  279,  127,  279,  279,  279,  131,  132,  133,  279,
 /*  1510 */   209,  136,  279,  138,  139,  279,  279,  142,  143,  144,
 /*  1520 */   145,  279,  127,  279,  279,  279,  131,  132,  133,  209,
 /*  1530 */   279,  136,  279,  138,  139,  279,  279,  142,  143,  144,
 /*  1540 */   145,  279,  279,  279,  279,  279,  127,  279,  279,  279,
 /*  1550 */   131,  132,  133,  279,  279,  136,  279,  138,  139,  209,
 /*  1560 */   279,  142,  143,  144,  145,  127,  279,  279,  279,  131,
 /*  1570 */   132,  133,  279,  279,  136,  279,  138,  139,  279,  279,
 /*  1580 */   142,  143,  144,  145,  209,  279,  279,  279,  279,  279,
 /*  1590 */   127,  279,  279,  279,  131,  132,  133,  279,  279,  136,
 /*  1600 */   279,  138,  139,  279,  209,  142,  143,  144,  145,  127,
 /*  1610 */   279,  279,  279,  131,  132,  133,  279,  279,  136,  279,
 /*  1620 */   138,  139,  279,  279,  142,  143,  144,  145,  209,  279,
 /*  1630 */   279,  279,  279,  279,  127,  279,  279,  279,  131,  132,
 /*  1640 */   133,  279,  279,  136,  279,  138,  139,  209,  279,  142,
 /*  1650 */   143,  144,  145,  279,  279,  279,  279,  279,  279,  127,
 /*  1660 */   279,  279,   62,  131,  132,  133,  279,  279,  136,   69,
 /*  1670 */   138,  139,  209,  279,  142,  143,  144,  145,  127,   79,
 /*  1680 */   279,  279,  131,  132,  133,  279,  279,  136,  279,  138,
 /*  1690 */   139,  209,  279,  142,  143,  144,  145,  127,   98,  279,
 /*  1700 */   279,  131,  132,  133,  279,  279,  136,  107,  138,  139,
 /*  1710 */   279,  279,  142,  143,  144,  145,  209,  117,  127,  279,
 /*  1720 */   279,  279,  131,  132,  133,  279,  279,  136,  279,  138,
 /*  1730 */   139,  279,  279,  142,  143,  144,  145,  279,  279,  279,
 /*  1740 */   279,  209,  279,  279,  279,  279,  127,  279,  279,  279,
 /*  1750 */   131,  132,  133,  279,  279,  136,  279,  138,  139,  279,
 /*  1760 */   209,  142,  143,  144,  145,  279,  127,  279,  279,  279,
 /*  1770 */   131,  132,  133,  279,  279,  136,  279,  138,  139,  209,
 /*  1780 */   279,  142,  143,  144,  145,  279,  279,  127,  279,  279,
 /*  1790 */   279,  131,  132,  133,  279,  279,  136,  279,  138,  139,
 /*  1800 */   209,  279,  142,  143,  144,  145,  127,  279,  279,  279,
 /*  1810 */   131,  132,  133,  279,  279,  136,  279,  138,  139,  279,
 /*  1820 */   279,  142,  143,  144,  145,  127,  279,  279,  209,  131,
 /*  1830 */   132,  133,  279,  279,  136,  279,  138,  139,  279,  279,
 /*  1840 */   142,  143,  144,  145,  279,  279,  127,  279,  209,  279,
 /*  1850 */   131,  132,  133,  279,  279,  136,  279,  138,  139,  279,
 /*  1860 */   279,  142,  143,  144,  145,  279,  279,  279,  279,  209,
 /*  1870 */   279,  279,  279,  279,  127,  279,  279,  279,  131,  132,
 /*  1880 */   133,  279,  279,  136,  279,  138,  139,  279,  209,  142,
 /*  1890 */   143,  144,  145,  279,  127,  279,  279,  279,  131,  132,
 /*  1900 */   133,  279,  279,  136,  279,  138,  139,  209,  279,  142,
 /*  1910 */   143,  144,  145,  279,  279,  127,  279,  279,  279,  131,
 /*  1920 */   132,  133,  279,  279,  136,  279,  138,  139,  209,  279,
 /*  1930 */   142,  143,  144,  145,  127,  279,  279,  279,  131,  132,
 /*  1940 */   133,  279,  279,  136,  279,  138,  139,  279,  279,  142,
 /*  1950 */   143,  144,  145,  127,  279,  279,  209,  131,  132,  133,
 /*  1960 */   279,  279,  136,  279,  138,  139,  279,  279,  142,  143,
 /*  1970 */   144,  145,  279,  279,  127,  279,  209,  279,  131,  132,
 /*  1980 */   133,  279,  279,  136,  279,  138,  139,  279,  279,  142,
 /*  1990 */   143,  144,  145,  279,  279,  279,  279,  209,  279,  279,
 /*  2000 */   279,  279,  127,  279,  279,  279,  131,  132,  133,  279,
 /*  2010 */   279,  136,  279,  138,  139,  279,  209,  142,  143,  144,
 /*  2020 */   145,  279,  127,  279,  279,  279,  131,  132,  133,  279,
 /*  2030 */   279,  136,  279,  138,  139,  209,  279,  142,  143,  144,
 /*  2040 */   145,  279,  279,  127,  279,  279,  279,  131,  132,  133,
 /*  2050 */   279,  279,  136,  279,  138,  139,  209,  279,  142,  143,
 /*  2060 */   144,  145,  127,  279,  279,  279,  131,  132,  133,  279,
 /*  2070 */   279,  136,  279,  138,  139,  279,  279,  142,  143,  144,
 /*  2080 */   145,  127,  279,  279,  209,  131,  132,  133,  279,  279,
 /*  2090 */   136,  279,  138,  139,  279,  279,  142,  143,  144,  145,
 /*  2100 */   279,  279,  127,  279,  209,  279,  131,  132,  133,  279,
 /*  2110 */   279,  136,  279,  138,  139,  279,  279,  142,  143,  144,
 /*  2120 */   145,  279,  279,  279,  279,  209,  279,  279,  279,  279,
 /*  2130 */   127,  279,  279,  279,  131,  132,  133,  279,  279,  136,
 /*  2140 */   279,  138,  139,  279,  209,  142,  143,  144,  145,  279,
 /*  2150 */   127,  279,  279,  279,  131,  132,  133,  279,  279,  136,
 /*  2160 */   279,  138,  139,  209,  279,  142,  143,  144,  145,  279,
 /*  2170 */   279,  127,  279,  279,  279,  131,  132,  133,  279,  279,
 /*  2180 */   136,  279,  138,  139,  209,  279,  142,  143,  144,  145,
 /*  2190 */    34,   35,  211,  212,  279,  279,  215,  279,  279,  218,
 /*  2200 */   219,  220,  279,  222,  223,  279,  279,  127,   52,  279,
 /*  2210 */    54,  131,  209,  133,  279,  279,  136,  279,  138,  139,
 /*  2220 */    64,   65,  142,  143,  144,  145,  279,  279,  279,  279,
 /*  2230 */    74,   75,  209,  212,  279,  279,  215,  216,  279,  218,
 /*  2240 */   219,  220,  279,  222,  223,  279,  225,  279,  279,  279,
 /*  2250 */   279,  279,  279,  209,  279,   99,  279,  279,  127,  103,
 /*  2260 */   279,  105,  131,  279,  133,  279,  279,  136,  112,  138,
 /*  2270 */   139,  250,  279,  142,  143,  144,  145,  279,  279,  279,
 /*  2280 */   279,  279,  127,  279,  279,  279,  131,  279,  133,  209,
 /*  2290 */   279,  136,  279,  138,  139,  279,  275,  142,  143,  144,
 /*  2300 */   145,  279,  279,  127,  279,  279,  279,  131,  279,  133,
 /*  2310 */   279,  279,  136,  279,  138,  139,  279,  279,  142,  143,
 /*  2320 */   144,  145,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2330 */   279,  279,  279,  279,  279,  127,  279,  279,  279,  131,
 /*  2340 */   209,  133,  279,  279,  136,  279,  138,  139,  279,  279,
 /*  2350 */   142,  143,  144,  145,  279,  279,  279,  279,  279,  127,
 /*  2360 */   279,  279,  279,  131,  209,  133,  279,  279,  136,  279,
 /*  2370 */   138,  139,  279,  279,  142,  143,  144,  145,  279,  279,
 /*  2380 */   279,  279,  279,  127,  279,  209,  279,  131,  279,  133,
 /*  2390 */   279,  279,  136,  279,  138,  139,  279,  279,  142,  143,
 /*  2400 */   144,  145,  279,  127,  279,  279,  279,  131,  279,  133,
 /*  2410 */   279,  279,  136,  279,  138,  139,  279,  209,  142,  143,
 /*  2420 */   144,  145,  127,  279,  279,  279,  131,  279,  133,  279,
 /*  2430 */   279,  136,  279,  138,  139,  279,  279,  142,  143,  144,
 /*  2440 */   145,  209,  127,  279,  279,  279,  131,  279,  133,  279,
 /*  2450 */   279,  136,  279,  138,  139,  279,  279,  142,  143,  144,
 /*  2460 */   145,  279,  279,  127,  279,  209,  279,  131,  279,  133,
 /*  2470 */   279,  279,  136,  279,  138,  139,  279,  279,  142,  143,
 /*  2480 */   144,  145,  279,  279,  279,  209,  279,  279,  279,  279,
 /*  2490 */   279,  127,  279,  279,  279,  131,  279,  133,  279,  279,
 /*  2500 */   136,  279,  138,  139,  209,  279,  142,  143,  144,  145,
 /*  2510 */   279,  127,  279,  279,  279,  131,  279,  133,  279,  279,
 /*  2520 */   136,  279,  138,  139,  209,  279,  142,  143,  144,  145,
 /*  2530 */   279,  127,  279,  279,  279,  131,  279,  133,  279,  279,
 /*  2540 */   136,  279,  138,  139,  279,  209,  142,  143,  127,  145,
 /*  2550 */   279,  279,  131,  279,  133,  279,  279,  136,  279,  138,
 /*  2560 */   139,  279,  279,  142,  143,  279,  145,  279,  279,  279,
 /*  2570 */   279,  279,  279,  209,  279,  279,  279,  279,  279,  279,
 /*  2580 */   279,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2590 */   279,  279,  279,  209,  279,  279,  279,  279,  279,  279,
 /*  2600 */   279,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2610 */   279,  279,  279,  209,  279,  279,  279,  279,  279,  279,
 /*  2620 */   279,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2630 */   209,
};
#define YY_SHIFT_USE_DFLT (-70)
#define YY_SHIFT_COUNT (406)
#define YY_SHIFT_MIN   (-69)
#define YY_SHIFT_MAX   (2156)
static const short yy_shift_ofst[] = {
 /*     0 */   574, 1275, 1275, 1275, 1275, 1275, 1275, 1275, 1275, 1275,
 /*    10 */    88, 1600,  891,  891,  891, 1600,   17,  186, 2156, 2156,
 /*    20 */   891,  -11,  186,  115,  115,  115,  115,  115,  115,  115,
 /*    30 */   115,  115,  115,  115,  115,  115,  115,  115,  115,  115,
 /*    40 */   115,  115,  115,  115,  115,  115,  115,  115,  115,  115,
 /*    50 */   115,  115,  115,  115,  115,  115,  115,  115,  115,  115,
 /*    60 */   115,  115,  115,  115,  115,  115,  115,  115,  115,  115,
 /*    70 */   115,  115,  115,  115,  115,  115, 1098,  115,  115,  115,
 /*    80 */   325,  325,  325,  325,  325,  325,  325,  325,  325,  325,
 /*    90 */   234, 1057, 1057, 1057, 1057, 1057, 1057, 1057, 1057, 1057,
 /*   100 */  1057, 1057, 1057, 1057, 1310, 1310, 1310, 1310, 1310,  165,
 /*   110 */   624, 1310, 1310,  136,  136,  136,  157,  362,  624,  498,
 /*   120 */  1153, 1153, 1309,  243,  224,  625,   47,  567,  378,  498,
 /*   130 */  1219, 1211, 1117, 1038, 1343, 1309, 1183, 1038, 1035, 1117,
 /*   140 */   -70,  -70,  -70,  216,  216, 1049, 1074, 1049, 1049, 1049,
 /*   150 */   693,  267,  109,   46,   46,   46,   46,  351,  366,  605,
 /*   160 */   564,  552,  366,  551,  448,  498,  531,  525,  249,  378,
 /*   170 */   249,  507,  461,  288,  366,  856,  856,  856, 1225,  856,
 /*   180 */   856, 1219, 1225,  856,  856, 1211,  856, 1117,  856,  856,
 /*   190 */  1153, 1197,  856,  856, 1183, 1160,  856,  856,  856,  856,
 /*   200 */   928,  928, 1153, 1114,  856,  856,  856,  856, 1017,  856,
 /*   210 */   856,  856,  856, 1017, 1000,  856,  856,  856,  856,  856,
 /*   220 */   856,  856, 1038, 1004,  856,  856, 1028,  856, 1038, 1038,
 /*   230 */  1038, 1038, 1035,  966,  974,  856,  994,  928,  928,  908,
 /*   240 */   963,  908,  963,  963,  963,  957,  933,  856,  856,  -70,
 /*   250 */   -70,  -70,  -70,  -70,  -70, 1012,  992,  972,  938,  909,
 /*   260 */   880,  860,  829,  809,  779,  748,  718, 1272, 1272, 1272,
 /*   270 */  1272, 1272, 1272, 1272, 1272, 1272, 1272, 1272, 1272,   -8,
 /*   280 */   139,   15,  418,  418,  146,   90,   22,    5,    5,    5,
 /*   290 */     5,    5,    5,    5,    5,    5,    5,  481,  458,  445,
 /*   300 */   441,  358,   19,  309,  382,   48,  264,   37,  426,   37,
 /*   310 */   383,  228,  103,  352,  -25,  901,  828,  898,  850,  896,
 /*   320 */   849,  887,  836,  879,  871,  870,  791,  830,  895,  868,
 /*   330 */   847,  775,  759,  799,  848,  844,  843,  841,  839,  838,
 /*   340 */   827,  822,  819,  818,  787,  805,  812,  734,  806,  776,
 /*   350 */   798,  722,  721,  785,  744,  765,  772,  769,  767,  766,
 /*   360 */   770,  761,  764,  763,  746,  662,  747,  737,  705,  711,
 /*   370 */   733,  730,  726,  709,  695,  694,  679,  691,  674,  657,
 /*   380 */   666,  648,  656,  614,  465,  646,  644,  628,  626,  508,
 /*   390 */   462,    6,  347,  396,  307,  238,  227,  227,  227,  227,
 /*   400 */   227,  227,  173,   28,    6,   61,  -69,
};
#define YY_REDUCE_USE_DFLT (-173)
#define YY_REDUCE_COUNT (254)
#define YY_REDUCE_MIN   (-172)
#define YY_REDUCE_MAX   (2421)
static const short yy_reduce_ofst[] = {
 /*     0 */   -12,  497,  419,  403,  319,  301,  221,  205,  127,  108,
 /*    10 */   503,  204,  439,  334,  232,  204, 1099, 1040, 1018,  911,
 /*    20 */   917, 1180, 1123, 1161, 2044, 2023, 2003, 1975, 1954, 1935,
 /*    30 */  1916, 1895, 1875, 1847, 1826, 1807, 1788, 1767, 1747, 1719,
 /*    40 */  1698, 1679, 1660, 1639, 1619, 1591, 1570, 1551, 1532, 1507,
 /*    50 */  1482, 1463, 1438, 1419, 1395, 1375, 1350, 1320, 1301, 1282,
 /*    60 */  1263, 1220, 2384, 2364, 2336, 2315, 2295, 2276, 2256, 2232,
 /*    70 */  2208, 2176, 2155, 2131, 2080,  524, 2021, 2421, 2404,  -78,
 /*    80 */  1981, 1256,  978,  823,  584,  553,  330,  124,   79,  -99,
 /*    90 */   113,  815,  751,  712,  688,  637,  527,  313,  178,   33,
 /*   100 */   -21,  -31, -119, -129, 1112, 1209, 1202, 1190, 1001,  379,
 /*   110 */   -50,  435,  357,  184,  -97,   80, -172, -101,  277,  132,
 /*   120 */   117,   83,  -66,  541,  283,  509,  504,  493,  451,  284,
 /*   130 */   353,  424,  411,  420,  298,   -4,  230,   40, -160,  397,
 /*   140 */   375,  437,  281, 1186, 1184, 1135, 1239, 1131, 1130, 1129,
 /*   150 */  1095, 1134, 1163, 1149, 1149, 1149, 1149, 1104, 1154, 1108,
 /*   160 */  1107, 1106, 1143, 1103, 1091, 1192, 1090, 1088, 1077, 1109,
 /*   170 */  1064, 1071, 1069, 1068, 1102, 1047, 1046, 1032, 1011, 1027,
 /*   180 */  1026, 1003,  988, 1014, 1009, 1010, 1002, 1058,  997,  990,
 /*   190 */  1059, 1039,  975,  970, 1037,  934,  967,  965,  962,  955,
 /*   200 */   999,  996, 1005,  932,  944,  943,  940,  937, 1021,  922,
 /*   210 */   921,  920,  919,  986,  976,  918,  897,  894,  890,  889,
 /*   220 */   888,  886,  907,  931,  881,  859,  884,  855,  878,  874,
 /*   230 */   873,  858,  837,  865,  883,  807,  875,  845,  832,  821,
 /*   240 */   866,  820,  862,  851,  846,  831,  743,  732,  724,  719,
 /*   250 */   803,  800,  684,  681,  700,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */   983,  918,  918,  918,  918,  918,  918,  918,  918,  918,
 /*    10 */   706,  899,  654,  654,  654,  898,  983,  983,  983,  983,
 /*    20 */   654,  983,  978,  983,  983,  983,  983,  983,  983,  983,
 /*    30 */   983,  983,  983,  983,  983,  983,  983,  983,  983,  983,
 /*    40 */   983,  983,  983,  983,  983,  983,  983,  983,  983,  983,
 /*    50 */   983,  983,  983,  983,  983,  983,  983,  983,  983,  983,
 /*    60 */   983,  983,  983,  983,  983,  983,  983,  983,  983,  983,
 /*    70 */   983,  983,  983,  983,  983,  983,  983,  983,  983,  983,
 /*    80 */   983,  983,  983,  983,  983,  983,  983,  983,  983,  983,
 /*    90 */   692,  983,  983,  983,  983,  983,  983,  983,  983,  983,
 /*   100 */   983,  983,  983,  983,  983,  983,  983,  983,  983,  720,
 /*   110 */   983,  983,  983,  713,  713,  983,  921,  983,  971,  983,
 /*   120 */   844,  844,  766,  948,  983,  983,  981,  983,  983,  721,
 /*   130 */   983,  983,  979,  983,  983,  766,  769,  983,  983,  979,
 /*   140 */   701,  681,  819,  983,  983,  983,  697,  983,  983,  983,
 /*   150 */   983,  740,  983,  959,  958,  957,  755,  983,  854,  983,
 /*   160 */   983,  983,  854,  983,  983,  983,  983,  983,  983,  983,
 /*   170 */   983,  983,  983,  983,  854,  983,  983,  983,  983,  816,
 /*   180 */   983,  983,  983,  813,  983,  983,  983,  983,  983,  983,
 /*   190 */   983,  983,  983,  983,  769,  983,  983,  983,  983,  983,
 /*   200 */   960,  960,  983,  983,  983,  983,  983,  983,  972,  983,
 /*   210 */   983,  983,  983,  972,  981,  983,  983,  983,  983,  983,
 /*   220 */   983,  983,  983,  922,  983,  983,  734,  983,  983,  983,
 /*   230 */   983,  983,  831,  970,  830,  983,  983,  960,  960,  859,
 /*   240 */   861,  859,  861,  861,  861,  983,  983,  983,  983,  692,
 /*   250 */   897,  868,  848,  847,  673,  983,  983,  983,  983,  983,
 /*   260 */   983,  983,  983,  983,  983,  983,  983,  841,  704,  705,
 /*   270 */   982,  973,  698,  805,  663,  665,  764,  765,  661,  983,
 /*   280 */   983,  754,  763,  762,  983,  983,  983,  753,  752,  751,
 /*   290 */   750,  749,  748,  747,  746,  745,  744,  983,  983,  983,
 /*   300 */   983,  983,  983,  983,  983,  800,  983,  933,  983,  932,
 /*   310 */   983,  983,  800,  983,  983,  983,  983,  983,  983,  983,
 /*   320 */   806,  983,  983,  983,  983,  983,  983,  983,  983,  983,
 /*   330 */   983,  983,  983,  983,  983,  983,  983,  983,  983,  983,
 /*   340 */   983,  983,  983,  794,  983,  983,  983,  873,  983,  983,
 /*   350 */   983,  983,  983,  983,  983,  983,  983,  983,  983,  983,
 /*   360 */   983,  983,  983,  983,  926,  983,  983,  983,  983,  983,
 /*   370 */   983,  983,  983,  983,  983,  983,  983,  983,  962,  983,
 /*   380 */   983,  983,  983,  856,  855,  983,  983,  983,  983,  662,
 /*   390 */   664,  742,  983,  983,  983,  800,  761,  760,  759,  758,
 /*   400 */   757,  756,  983,  983,  743,  983,  983,  738,  902,  901,
 /*   410 */   900,  820,  818,  817,  815,  814,  812,  810,  809,  808,
 /*   420 */   807,  811,  896,  895,  894,  893,  892,  891,  778,  946,
 /*   430 */   944,  943,  942,  941,  940,  939,  938,  937,  903,  852,
 /*   440 */   728,  945,  867,  866,  865,  846,  845,  843,  842,  780,
 /*   450 */   781,  782,  779,  771,  772,  770,  796,  797,  768,  667,
 /*   460 */   787,  789,  791,  793,  795,  792,  790,  788,  786,  785,
 /*   470 */   776,  775,  774,  773,  666,  767,  715,  714,  712,  655,
 /*   480 */   653,  652,  651,  947,  708,  707,  703,  702,  887,  920,
 /*   490 */   919,  917,  916,  915,  914,  913,  912,  911,  910,  909,
 /*   500 */   908,  907,  889,  888,  886,  804,  870,  864,  863,  802,
 /*   510 */   801,  729,  709,  700,  676,  677,  675,  672,  650,  727,
 /*   520 */   927,  935,  936,  931,  929,  934,  930,  928,  853,  800,
 /*   530 */   923,  925,  851,  850,  924,  726,  736,  735,  733,  732,
 /*   540 */   829,  826,  825,  824,  823,  822,  828,  827,  969,  968,
 /*   550 */   966,  967,  965,  964,  963,  962,  980,  977,  976,  975,
 /*   560 */   974,  725,  723,  731,  730,  724,  722,  858,  857,  684,
 /*   570 */   833,  961,  906,  905,  849,  832,  691,  860,  690,  689,
 /*   580 */   688,  687,  862,  686,  685,  683,  680,  679,  678,  674,
 /*   590 */   741,  875,  874,  777,  657,  885,  884,  883,  882,  881,
 /*   600 */   880,  879,  878,  950,  956,  955,  954,  953,  952,  951,
 /*   610 */   949,  877,  876,  840,  839,  838,  837,  836,  835,  834,
 /*   620 */   890,  821,  799,  798,  784,  656,  873,  872,  699,  711,
 /*   630 */   710,  660,  659,  658,  671,  670,  669,  668,  682,  719,
 /*   640 */   718,  717,  716,  696,  695,  694,  693,  904,  803,  739,
};

/* The next table maps tokens into fallback tokens.  If a construct
** like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number */
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level */
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  int yyidx;                    /* Index of top element in stack */
#ifdef YYTRACKMAXSTACKDEPTH
  int yyidxMax;                 /* Maximum value of yyidx */
#endif
  int yyerrcnt;                 /* Shifts left before out of the error */
  ParseARG_SDECL                /* A place to hold %extra_argument */
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
#endif
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void ParseTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "TOK_EQUAL",     "TOK_GREATER_EQUAL",  "TOK_GREATER_THAN",
  "TOK_IN",        "TOK_INST_EQUAL",  "TOK_INST_NOT_EQUAL",  "TOK_LESS_EQUAL",
  "TOK_LESS_THAN",  "TOK_LIKE",      "TOK_NOT_EQUAL",  "TOK_MINUS",   
  "TOK_PLUS",      "TOK_OR",        "TOK_XOR",       "TOK_DIV",     
  "TOK_MOD",       "TOK_REAL_DIV",  "TOK_TIMES",     "TOK_AND",     
  "TOK_ANDOR",     "TOK_CONCAT_OP",  "TOK_EXP",       "TOK_NOT",     
  "TOK_DOT",       "TOK_BACKSLASH",  "TOK_LEFT_BRACKET",  "TOK_LEFT_PAREN",
  "TOK_RIGHT_PAREN",  "TOK_COLON",     "TOK_RIGHT_BRACKET",  "TOK_COMMA",   
  "TOK_AGGREGATE",  "TOK_OF",        "TOK_IDENTIFIER",  "TOK_ALIAS",   
  "TOK_FOR",       "TOK_END_ALIAS",  "TOK_ARRAY",     "TOK_ASSIGNMENT",
  "TOK_BAG",       "TOK_BOOLEAN",   "TOK_INTEGER",   "TOK_REAL",    
  "TOK_NUMBER",    "TOK_LOGICAL",   "TOK_BINARY",    "TOK_STRING",  
  "TOK_BY",        "TOK_LEFT_CURL",  "TOK_RIGHT_CURL",  "TOK_OTHERWISE",
  "TOK_CASE",      "TOK_END_CASE",  "TOK_BEGIN",     "TOK_END",     
  "TOK_PI",        "TOK_E",         "TOK_CONSTANT",  "TOK_END_CONSTANT",
  "TOK_DERIVE",    "TOK_END_ENTITY",  "TOK_ENTITY",    "TOK_ENUMERATION",
  "TOK_ESCAPE",    "TOK_SELF",      "TOK_OPTIONAL",  "TOK_VAR",     
  "TOK_END_FUNCTION",  "TOK_FUNCTION",  "TOK_BUILTIN_FUNCTION",  "TOK_LIST",    
  "TOK_SET",       "TOK_GENERIC",   "TOK_QUESTION_MARK",  "TOK_IF",      
  "TOK_THEN",      "TOK_END_IF",    "TOK_ELSE",      "TOK_INCLUDE", 
  "TOK_STRING_LITERAL",  "TOK_TO",        "TOK_AS",        "TOK_REFERENCE",
  "TOK_FROM",      "TOK_USE",       "TOK_INVERSE",   "TOK_INTEGER_LITERAL",
  "TOK_REAL_LITERAL",  "TOK_STRING_LITERAL_ENCODED",  "TOK_LOGICAL_LITERAL",  "TOK_BINARY_LITERAL",
  "TOK_LOCAL",     "TOK_END_LOCAL",  "TOK_ONEOF",     "TOK_UNIQUE",  
  "TOK_FIXED",     "TOK_END_PROCEDURE",  "TOK_PROCEDURE",  "TOK_BUILTIN_PROCEDURE",
  "TOK_QUERY",     "TOK_ALL_IN",    "TOK_SUCH_THAT",  "TOK_REPEAT",  
  "TOK_END_REPEAT",  "TOK_RETURN",    "TOK_END_RULE",  "TOK_RULE",    
  "TOK_END_SCHEMA",  "TOK_SCHEMA",    "TOK_SELECT",    "TOK_SEMICOLON",
  "TOK_SKIP",      "TOK_SUBTYPE",   "TOK_ABSTRACT",  "TOK_SUPERTYPE",
  "TOK_END_TYPE",  "TOK_TYPE",      "TOK_UNTIL",     "TOK_WHERE",   
  "TOK_WHILE",     "error",         "statement_list",  "case_action", 
  "case_otherwise",  "entity_body",   "aggregate_init_element",  "aggregate_initializer",
  "assignable",    "attribute_decl",  "by_expression",  "constant",    
  "expression",    "function_call",  "general_ref",   "group_ref",   
  "identifier",    "initializer",   "interval",      "literal",     
  "local_initializer",  "precision_spec",  "query_expression",  "query_start", 
  "simple_expression",  "unary_expression",  "supertype_expression",  "until_control",
  "while_control",  "function_header",  "fh_lineno",     "rule_header", 
  "rh_start",      "rh_get_line",   "procedure_header",  "ph_get_line", 
  "action_body",   "actual_parameters",  "aggregate_init_body",  "explicit_attr_list",
  "case_action_list",  "case_block",    "case_labels",   "where_clause_list",
  "derive_decl",   "explicit_attribute",  "expression_list",  "formal_parameter",
  "formal_parameter_list",  "formal_parameter_rep",  "id_list",       "defined_type_list",
  "nested_id_list",  "statement_rep",  "subtype_decl",  "where_rule",  
  "where_rule_OPT",  "supertype_expression_list",  "labelled_attrib_list_list",  "labelled_attrib_list",
  "inverse_attr_list",  "inverse_clause",  "attribute_decl_list",  "derived_attribute_rep",
  "unique_clause",  "rule_formal_parameter_list",  "qualified_attr_list",  "rel_op",      
  "optional_or_unique",  "optional_fixed",  "optional",      "var",         
  "unique",        "qualified_attr",  "qualifier",     "alias_statement",
  "assignment_statement",  "case_statement",  "compound_statement",  "escape_statement",
  "if_statement",  "proc_call_statement",  "repeat_statement",  "return_statement",
  "skip_statement",  "statement",     "subsuper_decl",  "supertype_decl",
  "supertype_factor",  "function_id",   "procedure_id",  "attribute_type",
  "defined_type",  "parameter_type",  "generic_type",  "basic_type",  
  "select_type",   "aggregate_type",  "aggregation_type",  "array_type",  
  "bag_type",      "conformant_aggregation",  "list_type",     "set_type",    
  "set_or_bag_of_entity",  "type",          "cardinality_op",  "bound_spec",  
  "inverse_attr",  "derived_attribute",  "rule_formal_parameter",  "where_clause",
  "action_body_item_rep",  "action_body_item",  "declaration",   "constant_decl",
  "local_decl",    "semicolon",     "alias_push_scope",  "block_list",  
  "block_member",  "include_directive",  "rule_decl",     "constant_body",
  "constant_body_list",  "entity_decl",   "function_decl",  "procedure_decl",
  "type_decl",     "entity_header",  "enumeration_type",  "express_file",
  "schema_decl_list",  "schema_decl",   "fh_push_scope",  "fh_plist",    
  "increment_control",  "rename",        "rename_list",   "parened_rename_list",
  "reference_clause",  "reference_head",  "use_clause",    "use_head",    
  "interface_specification",  "interface_specification_list",  "right_curl",    "local_variable",
  "local_body",    "allow_generic_types",  "disallow_generic_types",  "oneof_op",    
  "ph_push_scope",  "schema_body",   "schema_header",  "type_item_body",
  "type_item",     "ti_start",      "td_start",    
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "action_body ::= action_body_item_rep statement_rep",
 /*   1 */ "action_body_item ::= declaration",
 /*   2 */ "action_body_item ::= constant_decl",
 /*   3 */ "action_body_item ::= local_decl",
 /*   4 */ "action_body_item_rep ::=",
 /*   5 */ "action_body_item_rep ::= action_body_item action_body_item_rep",
 /*   6 */ "actual_parameters ::= TOK_LEFT_PAREN expression_list TOK_RIGHT_PAREN",
 /*   7 */ "actual_parameters ::= TOK_LEFT_PAREN TOK_RIGHT_PAREN",
 /*   8 */ "bound_spec ::= TOK_LEFT_BRACKET expression TOK_COLON expression TOK_RIGHT_BRACKET",
 /*   9 */ "aggregate_initializer ::= TOK_LEFT_BRACKET TOK_RIGHT_BRACKET",
 /*  10 */ "aggregate_initializer ::= TOK_LEFT_BRACKET aggregate_init_body TOK_RIGHT_BRACKET",
 /*  11 */ "aggregate_init_element ::= expression",
 /*  12 */ "aggregate_init_body ::= aggregate_init_element",
 /*  13 */ "aggregate_init_body ::= aggregate_init_element TOK_COLON expression",
 /*  14 */ "aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element",
 /*  15 */ "aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element TOK_COLON expression",
 /*  16 */ "aggregate_type ::= TOK_AGGREGATE TOK_OF parameter_type",
 /*  17 */ "aggregate_type ::= TOK_AGGREGATE TOK_COLON TOK_IDENTIFIER TOK_OF parameter_type",
 /*  18 */ "aggregation_type ::= array_type",
 /*  19 */ "aggregation_type ::= bag_type",
 /*  20 */ "aggregation_type ::= list_type",
 /*  21 */ "aggregation_type ::= set_type",
 /*  22 */ "alias_statement ::= TOK_ALIAS TOK_IDENTIFIER TOK_FOR general_ref semicolon alias_push_scope statement_rep TOK_END_ALIAS semicolon",
 /*  23 */ "alias_push_scope ::=",
 /*  24 */ "array_type ::= TOK_ARRAY bound_spec TOK_OF optional_or_unique attribute_type",
 /*  25 */ "assignable ::= assignable qualifier",
 /*  26 */ "assignable ::= identifier",
 /*  27 */ "assignment_statement ::= assignable TOK_ASSIGNMENT expression semicolon",
 /*  28 */ "attribute_type ::= aggregation_type",
 /*  29 */ "attribute_type ::= basic_type",
 /*  30 */ "attribute_type ::= defined_type",
 /*  31 */ "explicit_attr_list ::=",
 /*  32 */ "explicit_attr_list ::= explicit_attr_list explicit_attribute",
 /*  33 */ "bag_type ::= TOK_BAG bound_spec TOK_OF attribute_type",
 /*  34 */ "bag_type ::= TOK_BAG TOK_OF attribute_type",
 /*  35 */ "basic_type ::= TOK_BOOLEAN",
 /*  36 */ "basic_type ::= TOK_INTEGER precision_spec",
 /*  37 */ "basic_type ::= TOK_REAL precision_spec",
 /*  38 */ "basic_type ::= TOK_NUMBER",
 /*  39 */ "basic_type ::= TOK_LOGICAL",
 /*  40 */ "basic_type ::= TOK_BINARY precision_spec optional_fixed",
 /*  41 */ "basic_type ::= TOK_STRING precision_spec optional_fixed",
 /*  42 */ "block_list ::=",
 /*  43 */ "block_list ::= block_list block_member",
 /*  44 */ "block_member ::= declaration",
 /*  45 */ "block_member ::= include_directive",
 /*  46 */ "block_member ::= rule_decl",
 /*  47 */ "by_expression ::=",
 /*  48 */ "by_expression ::= TOK_BY expression",
 /*  49 */ "cardinality_op ::= TOK_LEFT_CURL expression TOK_COLON expression TOK_RIGHT_CURL",
 /*  50 */ "case_action ::= case_labels TOK_COLON statement",
 /*  51 */ "case_action_list ::=",
 /*  52 */ "case_action_list ::= case_action_list case_action",
 /*  53 */ "case_block ::= case_action_list case_otherwise",
 /*  54 */ "case_labels ::= expression",
 /*  55 */ "case_labels ::= case_labels TOK_COMMA expression",
 /*  56 */ "case_otherwise ::=",
 /*  57 */ "case_otherwise ::= TOK_OTHERWISE TOK_COLON statement",
 /*  58 */ "case_statement ::= TOK_CASE expression TOK_OF case_block TOK_END_CASE semicolon",
 /*  59 */ "compound_statement ::= TOK_BEGIN statement_rep TOK_END semicolon",
 /*  60 */ "constant ::= TOK_PI",
 /*  61 */ "constant ::= TOK_E",
 /*  62 */ "constant_body ::= identifier TOK_COLON attribute_type TOK_ASSIGNMENT expression semicolon",
 /*  63 */ "constant_body_list ::=",
 /*  64 */ "constant_body_list ::= constant_body constant_body_list",
 /*  65 */ "constant_decl ::= TOK_CONSTANT constant_body_list TOK_END_CONSTANT semicolon",
 /*  66 */ "declaration ::= entity_decl",
 /*  67 */ "declaration ::= function_decl",
 /*  68 */ "declaration ::= procedure_decl",
 /*  69 */ "declaration ::= type_decl",
 /*  70 */ "derive_decl ::=",
 /*  71 */ "derive_decl ::= TOK_DERIVE derived_attribute_rep",
 /*  72 */ "derived_attribute ::= attribute_decl TOK_COLON attribute_type initializer semicolon",
 /*  73 */ "derived_attribute_rep ::= derived_attribute",
 /*  74 */ "derived_attribute_rep ::= derived_attribute_rep derived_attribute",
 /*  75 */ "entity_body ::= explicit_attr_list derive_decl inverse_clause unique_clause where_rule_OPT",
 /*  76 */ "entity_decl ::= entity_header subsuper_decl semicolon entity_body TOK_END_ENTITY semicolon",
 /*  77 */ "entity_header ::= TOK_ENTITY TOK_IDENTIFIER",
 /*  78 */ "enumeration_type ::= TOK_ENUMERATION TOK_OF nested_id_list",
 /*  79 */ "escape_statement ::= TOK_ESCAPE semicolon",
 /*  80 */ "attribute_decl ::= TOK_IDENTIFIER",
 /*  81 */ "attribute_decl ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER",
 /*  82 */ "attribute_decl_list ::= attribute_decl",
 /*  83 */ "attribute_decl_list ::= attribute_decl_list TOK_COMMA attribute_decl",
 /*  84 */ "optional ::=",
 /*  85 */ "optional ::= TOK_OPTIONAL",
 /*  86 */ "explicit_attribute ::= attribute_decl_list TOK_COLON optional attribute_type semicolon",
 /*  87 */ "express_file ::= schema_decl_list",
 /*  88 */ "schema_decl_list ::= schema_decl",
 /*  89 */ "schema_decl_list ::= schema_decl_list schema_decl",
 /*  90 */ "expression ::= simple_expression",
 /*  91 */ "expression ::= expression TOK_AND expression",
 /*  92 */ "expression ::= expression TOK_OR expression",
 /*  93 */ "expression ::= expression TOK_XOR expression",
 /*  94 */ "expression ::= expression TOK_LESS_THAN expression",
 /*  95 */ "expression ::= expression TOK_GREATER_THAN expression",
 /*  96 */ "expression ::= expression TOK_EQUAL expression",
 /*  97 */ "expression ::= expression TOK_LESS_EQUAL expression",
 /*  98 */ "expression ::= expression TOK_GREATER_EQUAL expression",
 /*  99 */ "expression ::= expression TOK_NOT_EQUAL expression",
 /* 100 */ "expression ::= expression TOK_INST_EQUAL expression",
 /* 101 */ "expression ::= expression TOK_INST_NOT_EQUAL expression",
 /* 102 */ "expression ::= expression TOK_IN expression",
 /* 103 */ "expression ::= expression TOK_LIKE expression",
 /* 104 */ "expression ::= simple_expression cardinality_op simple_expression",
 /* 105 */ "simple_expression ::= unary_expression",
 /* 106 */ "simple_expression ::= simple_expression TOK_CONCAT_OP simple_expression",
 /* 107 */ "simple_expression ::= simple_expression TOK_EXP simple_expression",
 /* 108 */ "simple_expression ::= simple_expression TOK_TIMES simple_expression",
 /* 109 */ "simple_expression ::= simple_expression TOK_DIV simple_expression",
 /* 110 */ "simple_expression ::= simple_expression TOK_REAL_DIV simple_expression",
 /* 111 */ "simple_expression ::= simple_expression TOK_MOD simple_expression",
 /* 112 */ "simple_expression ::= simple_expression TOK_PLUS simple_expression",
 /* 113 */ "simple_expression ::= simple_expression TOK_MINUS simple_expression",
 /* 114 */ "expression_list ::= expression",
 /* 115 */ "expression_list ::= expression_list TOK_COMMA expression",
 /* 116 */ "var ::=",
 /* 117 */ "var ::= TOK_VAR",
 /* 118 */ "formal_parameter ::= var id_list TOK_COLON parameter_type",
 /* 119 */ "formal_parameter_list ::=",
 /* 120 */ "formal_parameter_list ::= TOK_LEFT_PAREN formal_parameter_rep TOK_RIGHT_PAREN",
 /* 121 */ "formal_parameter_rep ::= formal_parameter",
 /* 122 */ "formal_parameter_rep ::= formal_parameter_rep semicolon formal_parameter",
 /* 123 */ "parameter_type ::= basic_type",
 /* 124 */ "parameter_type ::= conformant_aggregation",
 /* 125 */ "parameter_type ::= defined_type",
 /* 126 */ "parameter_type ::= generic_type",
 /* 127 */ "function_call ::= function_id actual_parameters",
 /* 128 */ "function_decl ::= function_header action_body TOK_END_FUNCTION semicolon",
 /* 129 */ "function_header ::= fh_lineno fh_push_scope fh_plist TOK_COLON parameter_type semicolon",
 /* 130 */ "fh_lineno ::= TOK_FUNCTION",
 /* 131 */ "fh_push_scope ::= TOK_IDENTIFIER",
 /* 132 */ "fh_plist ::= formal_parameter_list",
 /* 133 */ "function_id ::= TOK_IDENTIFIER",
 /* 134 */ "function_id ::= TOK_BUILTIN_FUNCTION",
 /* 135 */ "conformant_aggregation ::= aggregate_type",
 /* 136 */ "conformant_aggregation ::= TOK_ARRAY TOK_OF optional_or_unique parameter_type",
 /* 137 */ "conformant_aggregation ::= TOK_ARRAY bound_spec TOK_OF optional_or_unique parameter_type",
 /* 138 */ "conformant_aggregation ::= TOK_BAG TOK_OF parameter_type",
 /* 139 */ "conformant_aggregation ::= TOK_BAG bound_spec TOK_OF parameter_type",
 /* 140 */ "conformant_aggregation ::= TOK_LIST TOK_OF unique parameter_type",
 /* 141 */ "conformant_aggregation ::= TOK_LIST bound_spec TOK_OF unique parameter_type",
 /* 142 */ "conformant_aggregation ::= TOK_SET TOK_OF parameter_type",
 /* 143 */ "conformant_aggregation ::= TOK_SET bound_spec TOK_OF parameter_type",
 /* 144 */ "generic_type ::= TOK_GENERIC",
 /* 145 */ "generic_type ::= TOK_GENERIC TOK_COLON TOK_IDENTIFIER",
 /* 146 */ "id_list ::= TOK_IDENTIFIER",
 /* 147 */ "id_list ::= id_list TOK_COMMA TOK_IDENTIFIER",
 /* 148 */ "identifier ::= TOK_SELF",
 /* 149 */ "identifier ::= TOK_QUESTION_MARK",
 /* 150 */ "identifier ::= TOK_IDENTIFIER",
 /* 151 */ "if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_END_IF semicolon",
 /* 152 */ "if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_ELSE statement_rep TOK_END_IF semicolon",
 /* 153 */ "include_directive ::= TOK_INCLUDE TOK_STRING_LITERAL semicolon",
 /* 154 */ "increment_control ::= TOK_IDENTIFIER TOK_ASSIGNMENT expression TOK_TO expression by_expression",
 /* 155 */ "initializer ::= TOK_ASSIGNMENT expression",
 /* 156 */ "rename ::= TOK_IDENTIFIER",
 /* 157 */ "rename ::= TOK_IDENTIFIER TOK_AS TOK_IDENTIFIER",
 /* 158 */ "rename_list ::= rename",
 /* 159 */ "rename_list ::= rename_list TOK_COMMA rename",
 /* 160 */ "parened_rename_list ::= TOK_LEFT_PAREN rename_list TOK_RIGHT_PAREN",
 /* 161 */ "reference_clause ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER semicolon",
 /* 162 */ "reference_clause ::= reference_head parened_rename_list semicolon",
 /* 163 */ "reference_head ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER",
 /* 164 */ "use_clause ::= TOK_USE TOK_FROM TOK_IDENTIFIER semicolon",
 /* 165 */ "use_clause ::= use_head parened_rename_list semicolon",
 /* 166 */ "use_head ::= TOK_USE TOK_FROM TOK_IDENTIFIER",
 /* 167 */ "interface_specification ::= use_clause",
 /* 168 */ "interface_specification ::= reference_clause",
 /* 169 */ "interface_specification_list ::=",
 /* 170 */ "interface_specification_list ::= interface_specification_list interface_specification",
 /* 171 */ "interval ::= TOK_LEFT_CURL simple_expression rel_op simple_expression rel_op simple_expression right_curl",
 /* 172 */ "set_or_bag_of_entity ::= defined_type",
 /* 173 */ "set_or_bag_of_entity ::= TOK_SET TOK_OF defined_type",
 /* 174 */ "set_or_bag_of_entity ::= TOK_SET bound_spec TOK_OF defined_type",
 /* 175 */ "set_or_bag_of_entity ::= TOK_BAG bound_spec TOK_OF defined_type",
 /* 176 */ "set_or_bag_of_entity ::= TOK_BAG TOK_OF defined_type",
 /* 177 */ "inverse_attr_list ::= inverse_attr",
 /* 178 */ "inverse_attr_list ::= inverse_attr_list inverse_attr",
 /* 179 */ "inverse_attr ::= TOK_IDENTIFIER TOK_COLON set_or_bag_of_entity TOK_FOR TOK_IDENTIFIER semicolon",
 /* 180 */ "inverse_clause ::=",
 /* 181 */ "inverse_clause ::= TOK_INVERSE inverse_attr_list",
 /* 182 */ "list_type ::= TOK_LIST bound_spec TOK_OF unique attribute_type",
 /* 183 */ "list_type ::= TOK_LIST TOK_OF unique attribute_type",
 /* 184 */ "literal ::= TOK_INTEGER_LITERAL",
 /* 185 */ "literal ::= TOK_REAL_LITERAL",
 /* 186 */ "literal ::= TOK_STRING_LITERAL",
 /* 187 */ "literal ::= TOK_STRING_LITERAL_ENCODED",
 /* 188 */ "literal ::= TOK_LOGICAL_LITERAL",
 /* 189 */ "literal ::= TOK_BINARY_LITERAL",
 /* 190 */ "literal ::= constant",
 /* 191 */ "local_initializer ::= TOK_ASSIGNMENT expression",
 /* 192 */ "local_variable ::= id_list TOK_COLON parameter_type semicolon",
 /* 193 */ "local_variable ::= id_list TOK_COLON parameter_type local_initializer semicolon",
 /* 194 */ "local_body ::=",
 /* 195 */ "local_body ::= local_variable local_body",
 /* 196 */ "local_decl ::= TOK_LOCAL allow_generic_types local_body TOK_END_LOCAL semicolon disallow_generic_types",
 /* 197 */ "allow_generic_types ::=",
 /* 198 */ "disallow_generic_types ::=",
 /* 199 */ "defined_type ::= TOK_IDENTIFIER",
 /* 200 */ "defined_type_list ::= defined_type",
 /* 201 */ "defined_type_list ::= defined_type_list TOK_COMMA defined_type",
 /* 202 */ "nested_id_list ::= TOK_LEFT_PAREN id_list TOK_RIGHT_PAREN",
 /* 203 */ "oneof_op ::= TOK_ONEOF",
 /* 204 */ "optional_or_unique ::=",
 /* 205 */ "optional_or_unique ::= TOK_OPTIONAL",
 /* 206 */ "optional_or_unique ::= TOK_UNIQUE",
 /* 207 */ "optional_or_unique ::= TOK_OPTIONAL TOK_UNIQUE",
 /* 208 */ "optional_or_unique ::= TOK_UNIQUE TOK_OPTIONAL",
 /* 209 */ "optional_fixed ::=",
 /* 210 */ "optional_fixed ::= TOK_FIXED",
 /* 211 */ "precision_spec ::=",
 /* 212 */ "precision_spec ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN",
 /* 213 */ "proc_call_statement ::= procedure_id actual_parameters semicolon",
 /* 214 */ "proc_call_statement ::= procedure_id semicolon",
 /* 215 */ "procedure_decl ::= procedure_header action_body TOK_END_PROCEDURE semicolon",
 /* 216 */ "procedure_header ::= TOK_PROCEDURE ph_get_line ph_push_scope formal_parameter_list semicolon",
 /* 217 */ "ph_push_scope ::= TOK_IDENTIFIER",
 /* 218 */ "ph_get_line ::=",
 /* 219 */ "procedure_id ::= TOK_IDENTIFIER",
 /* 220 */ "procedure_id ::= TOK_BUILTIN_PROCEDURE",
 /* 221 */ "group_ref ::= TOK_BACKSLASH TOK_IDENTIFIER",
 /* 222 */ "qualifier ::= TOK_DOT TOK_IDENTIFIER",
 /* 223 */ "qualifier ::= TOK_BACKSLASH TOK_IDENTIFIER",
 /* 224 */ "qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_RIGHT_BRACKET",
 /* 225 */ "qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_COLON simple_expression TOK_RIGHT_BRACKET",
 /* 226 */ "query_expression ::= query_start expression TOK_RIGHT_PAREN",
 /* 227 */ "query_start ::= TOK_QUERY TOK_LEFT_PAREN TOK_IDENTIFIER TOK_ALL_IN expression TOK_SUCH_THAT",
 /* 228 */ "rel_op ::= TOK_LESS_THAN",
 /* 229 */ "rel_op ::= TOK_GREATER_THAN",
 /* 230 */ "rel_op ::= TOK_EQUAL",
 /* 231 */ "rel_op ::= TOK_LESS_EQUAL",
 /* 232 */ "rel_op ::= TOK_GREATER_EQUAL",
 /* 233 */ "rel_op ::= TOK_NOT_EQUAL",
 /* 234 */ "rel_op ::= TOK_INST_EQUAL",
 /* 235 */ "rel_op ::= TOK_INST_NOT_EQUAL",
 /* 236 */ "repeat_statement ::= TOK_REPEAT increment_control while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon",
 /* 237 */ "repeat_statement ::= TOK_REPEAT while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon",
 /* 238 */ "return_statement ::= TOK_RETURN semicolon",
 /* 239 */ "return_statement ::= TOK_RETURN TOK_LEFT_PAREN expression TOK_RIGHT_PAREN semicolon",
 /* 240 */ "right_curl ::= TOK_RIGHT_CURL",
 /* 241 */ "rule_decl ::= rule_header action_body where_rule TOK_END_RULE semicolon",
 /* 242 */ "rule_formal_parameter ::= TOK_IDENTIFIER",
 /* 243 */ "rule_formal_parameter_list ::= rule_formal_parameter",
 /* 244 */ "rule_formal_parameter_list ::= rule_formal_parameter_list TOK_COMMA rule_formal_parameter",
 /* 245 */ "rule_header ::= rh_start rule_formal_parameter_list TOK_RIGHT_PAREN semicolon",
 /* 246 */ "rh_start ::= TOK_RULE rh_get_line TOK_IDENTIFIER TOK_FOR TOK_LEFT_PAREN",
 /* 247 */ "rh_get_line ::=",
 /* 248 */ "schema_body ::= interface_specification_list block_list",
 /* 249 */ "schema_body ::= interface_specification_list constant_decl block_list",
 /* 250 */ "schema_decl ::= schema_header schema_body TOK_END_SCHEMA semicolon",
 /* 251 */ "schema_decl ::= include_directive",
 /* 252 */ "schema_header ::= TOK_SCHEMA TOK_IDENTIFIER semicolon",
 /* 253 */ "select_type ::= TOK_SELECT TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN",
 /* 254 */ "semicolon ::= TOK_SEMICOLON",
 /* 255 */ "set_type ::= TOK_SET bound_spec TOK_OF attribute_type",
 /* 256 */ "set_type ::= TOK_SET TOK_OF attribute_type",
 /* 257 */ "skip_statement ::= TOK_SKIP semicolon",
 /* 258 */ "statement ::= alias_statement",
 /* 259 */ "statement ::= assignment_statement",
 /* 260 */ "statement ::= case_statement",
 /* 261 */ "statement ::= compound_statement",
 /* 262 */ "statement ::= escape_statement",
 /* 263 */ "statement ::= if_statement",
 /* 264 */ "statement ::= proc_call_statement",
 /* 265 */ "statement ::= repeat_statement",
 /* 266 */ "statement ::= return_statement",
 /* 267 */ "statement ::= skip_statement",
 /* 268 */ "statement_rep ::=",
 /* 269 */ "statement_rep ::= semicolon statement_rep",
 /* 270 */ "statement_rep ::= statement statement_rep",
 /* 271 */ "subsuper_decl ::=",
 /* 272 */ "subsuper_decl ::= supertype_decl",
 /* 273 */ "subsuper_decl ::= subtype_decl",
 /* 274 */ "subsuper_decl ::= supertype_decl subtype_decl",
 /* 275 */ "subtype_decl ::= TOK_SUBTYPE TOK_OF TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN",
 /* 276 */ "supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE",
 /* 277 */ "supertype_decl ::= TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN",
 /* 278 */ "supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN",
 /* 279 */ "supertype_expression ::= supertype_factor",
 /* 280 */ "supertype_expression ::= supertype_expression TOK_AND supertype_factor",
 /* 281 */ "supertype_expression ::= supertype_expression TOK_ANDOR supertype_factor",
 /* 282 */ "supertype_expression_list ::= supertype_expression",
 /* 283 */ "supertype_expression_list ::= supertype_expression_list TOK_COMMA supertype_expression",
 /* 284 */ "supertype_factor ::= identifier",
 /* 285 */ "supertype_factor ::= oneof_op TOK_LEFT_PAREN supertype_expression_list TOK_RIGHT_PAREN",
 /* 286 */ "supertype_factor ::= TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN",
 /* 287 */ "type ::= aggregation_type",
 /* 288 */ "type ::= basic_type",
 /* 289 */ "type ::= defined_type",
 /* 290 */ "type ::= select_type",
 /* 291 */ "type_item_body ::= enumeration_type",
 /* 292 */ "type_item_body ::= type",
 /* 293 */ "type_item ::= ti_start type_item_body semicolon",
 /* 294 */ "ti_start ::= TOK_IDENTIFIER TOK_EQUAL",
 /* 295 */ "type_decl ::= td_start TOK_END_TYPE semicolon",
 /* 296 */ "td_start ::= TOK_TYPE type_item where_rule_OPT",
 /* 297 */ "general_ref ::= assignable group_ref",
 /* 298 */ "general_ref ::= assignable",
 /* 299 */ "unary_expression ::= aggregate_initializer",
 /* 300 */ "unary_expression ::= unary_expression qualifier",
 /* 301 */ "unary_expression ::= literal",
 /* 302 */ "unary_expression ::= function_call",
 /* 303 */ "unary_expression ::= identifier",
 /* 304 */ "unary_expression ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN",
 /* 305 */ "unary_expression ::= interval",
 /* 306 */ "unary_expression ::= query_expression",
 /* 307 */ "unary_expression ::= TOK_NOT unary_expression",
 /* 308 */ "unary_expression ::= TOK_PLUS unary_expression",
 /* 309 */ "unary_expression ::= TOK_MINUS unary_expression",
 /* 310 */ "unique ::=",
 /* 311 */ "unique ::= TOK_UNIQUE",
 /* 312 */ "qualified_attr ::= TOK_IDENTIFIER",
 /* 313 */ "qualified_attr ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER",
 /* 314 */ "qualified_attr_list ::= qualified_attr",
 /* 315 */ "qualified_attr_list ::= qualified_attr_list TOK_COMMA qualified_attr",
 /* 316 */ "labelled_attrib_list ::= qualified_attr_list semicolon",
 /* 317 */ "labelled_attrib_list ::= TOK_IDENTIFIER TOK_COLON qualified_attr_list semicolon",
 /* 318 */ "labelled_attrib_list_list ::= labelled_attrib_list",
 /* 319 */ "labelled_attrib_list_list ::= labelled_attrib_list_list labelled_attrib_list",
 /* 320 */ "unique_clause ::=",
 /* 321 */ "unique_clause ::= TOK_UNIQUE labelled_attrib_list_list",
 /* 322 */ "until_control ::=",
 /* 323 */ "until_control ::= TOK_UNTIL expression",
 /* 324 */ "where_clause ::= expression semicolon",
 /* 325 */ "where_clause ::= TOK_IDENTIFIER TOK_COLON expression semicolon",
 /* 326 */ "where_clause_list ::= where_clause",
 /* 327 */ "where_clause_list ::= where_clause_list where_clause",
 /* 328 */ "where_rule ::= TOK_WHERE where_clause_list",
 /* 329 */ "where_rule_OPT ::=",
 /* 330 */ "where_rule_OPT ::= where_rule",
 /* 331 */ "while_control ::=",
 /* 332 */ "while_control ::= TOK_WHILE expression",
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.
*/
static void yyGrowStack(yyParser *p){
  int newSize;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 256;
  pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  if( pNew ){
    p->yystack = pNew;
    p->yystksz = newSize;
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows to %d entries!\n",
              yyTracePrompt, p->yystksz);
    }
#endif
  }
}
#endif

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to Parse and ParseFree.
*/
void *ParseAlloc(void *(*mallocProc)(size_t)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (size_t)sizeof(yyParser) );
  if( pParser ){
    pParser->yyidx = -1;
#ifdef YYTRACKMAXSTACKDEPTH
    pParser->yyidxMax = 0;
#endif
#if YYSTACKDEPTH<=0
    pParser->yystack = NULL;
    pParser->yystksz = 0;
    yyGrowStack(pParser);
#endif
  }
  return pParser;
}

/* The following function deletes the value associated with a
** symbol.  The symbol can be either a terminal or nonterminal.
** "yymajor" is the symbol code, and "yypminor" is a pointer to
** the value.
*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  ParseARG_FETCH;
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are not used
    ** inside the C code.
    */
    case 122: /* statement_list */
{
#line 189 "expparse.y"

    if (parseData.scanner == NULL) {
	(yypminor->yy0).string = (char*)NULL;
    }

#line 1612 "expparse.c"
}
      break;
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
**
** Return the major token number for the symbol popped.
*/
static int yy_pop_parser_stack(yyParser *pParser){
  YYCODETYPE yymajor;
  yyStackEntry *yytos;

  if( pParser->yyidx<0 ) return 0;

  yytos = &pParser->yystack[pParser->yyidx];

#ifndef NDEBUG
  if( yyTraceFILE && pParser->yyidx>=0 ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yymajor = yytos->major;
  yy_destructor(pParser, yymajor, &yytos->minor);
  pParser->yyidx--;
  return yymajor;
}

/* 
** Deallocate and destroy a parser.  Destructors are all called for
** all stack elements before shutting the parser down.
**
** Inputs:
** <ul>
** <li>  A pointer to the parser.  This should be a pointer
**       obtained from ParseAlloc.
** <li>  A pointer to a function used to reclaim memory obtained
**       from malloc.
** </ul>
*/
void ParseFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
  if( pParser==0 ) return;
  while( pParser->yyidx>=0 ) yy_pop_parser_stack(pParser);
#if YYSTACKDEPTH<=0
  free(pParser->yystack);
#endif
  (*freeProc)((void*)pParser);
}

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
int ParseStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyidxMax;
}
#endif

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  int stateno = pParser->yystack[pParser->yyidx].stateno;
 
  if( stateno>YY_SHIFT_COUNT
   || (i = yy_shift_ofst[stateno])==YY_SHIFT_USE_DFLT ){
    return yy_default[stateno];
  }
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    if( iLookAhead>0 ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback;            /* Fallback token */
      if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
             && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        return yy_find_shift_action(pParser, iFallback);
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        if( 
#if YY_SHIFT_MIN+YYWILDCARD<0
          j>=0 &&
#endif
#if YY_SHIFT_MAX+YYWILDCARD>=YY_ACTTAB_COUNT
          j<YY_ACTTAB_COUNT &&
#endif
          yy_lookahead[j]==YYWILDCARD
        ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
    }
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_COUNT );
#endif
  i = yy_reduce_ofst[stateno];
  assert( i!=YY_REDUCE_USE_DFLT );
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_ACTTAB_COUNT );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser, YYMINORTYPE *yypMinor){
   ParseARG_FETCH;
   yypParser->yyidx--;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
#line 2474 "expparse.y"

    fprintf(stderr, "Express parser experienced stack overflow.\n");
    fprintf(stderr, "Last token had value %x\n", yypMinor->yy0.val);
#line 1801 "expparse.c"
   ParseARG_STORE; /* Suppress warning about unused %extra_argument var */
}

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  YYMINORTYPE *yypMinor         /* Pointer to the minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yyidx++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( yypParser->yyidx>yypParser->yyidxMax ){
    yypParser->yyidxMax = yypParser->yyidx;
  }
#endif
#if YYSTACKDEPTH>0 
  if( yypParser->yyidx>=YYSTACKDEPTH ){
    yyStackOverflow(yypParser, yypMinor);
    return;
  }
#else
  if( yypParser->yyidx>=yypParser->yystksz ){
    yyGrowStack(yypParser);
    if( yypParser->yyidx>=yypParser->yystksz ){
      yyStackOverflow(yypParser, yypMinor);
      return;
    }
  }
#endif
  yytos = &yypParser->yystack[yypParser->yyidx];
  yytos->stateno = (YYACTIONTYPE)yyNewState;
  yytos->major = (YYCODETYPE)yyMajor;
  yytos->minor = *yypMinor;
#ifndef NDEBUG
  if( yyTraceFILE && yypParser->yyidx>0 ){
    int i;
    fprintf(yyTraceFILE,"%sShift %d\n",yyTracePrompt,yyNewState);
    fprintf(yyTraceFILE,"%sStack:",yyTracePrompt);
    for(i=1; i<=yypParser->yyidx; i++)
      fprintf(yyTraceFILE," %s",yyTokenName[yypParser->yystack[i].major]);
    fprintf(yyTraceFILE,"\n");
  }
#endif
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 156, 2 },
  { 233, 1 },
  { 233, 1 },
  { 233, 1 },
  { 232, 0 },
  { 232, 2 },
  { 157, 3 },
  { 157, 2 },
  { 227, 5 },
  { 127, 2 },
  { 127, 3 },
  { 126, 1 },
  { 158, 1 },
  { 158, 3 },
  { 158, 3 },
  { 158, 5 },
  { 217, 3 },
  { 217, 5 },
  { 218, 1 },
  { 218, 1 },
  { 218, 1 },
  { 218, 1 },
  { 195, 9 },
  { 238, 0 },
  { 219, 5 },
  { 128, 2 },
  { 128, 1 },
  { 196, 4 },
  { 211, 1 },
  { 211, 1 },
  { 211, 1 },
  { 159, 0 },
  { 159, 2 },
  { 220, 4 },
  { 220, 3 },
  { 215, 1 },
  { 215, 2 },
  { 215, 2 },
  { 215, 1 },
  { 215, 1 },
  { 215, 3 },
  { 215, 3 },
  { 239, 0 },
  { 239, 2 },
  { 240, 1 },
  { 240, 1 },
  { 240, 1 },
  { 130, 0 },
  { 130, 2 },
  { 226, 5 },
  { 123, 3 },
  { 160, 0 },
  { 160, 2 },
  { 161, 2 },
  { 162, 1 },
  { 162, 3 },
  { 124, 0 },
  { 124, 3 },
  { 197, 6 },
  { 198, 4 },
  { 131, 1 },
  { 131, 1 },
  { 243, 6 },
  { 244, 0 },
  { 244, 2 },
  { 235, 4 },
  { 234, 1 },
  { 234, 1 },
  { 234, 1 },
  { 234, 1 },
  { 164, 0 },
  { 164, 2 },
  { 229, 5 },
  { 183, 1 },
  { 183, 2 },
  { 125, 5 },
  { 245, 6 },
  { 249, 2 },
  { 250, 3 },
  { 199, 2 },
  { 129, 1 },
  { 129, 5 },
  { 182, 1 },
  { 182, 3 },
  { 190, 0 },
  { 190, 1 },
  { 165, 5 },
  { 251, 1 },
  { 252, 1 },
  { 252, 2 },
  { 132, 1 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 144, 1 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 166, 1 },
  { 166, 3 },
  { 191, 0 },
  { 191, 1 },
  { 167, 4 },
  { 168, 0 },
  { 168, 3 },
  { 169, 1 },
  { 169, 3 },
  { 213, 1 },
  { 213, 1 },
  { 213, 1 },
  { 213, 1 },
  { 133, 2 },
  { 246, 4 },
  { 149, 6 },
  { 150, 1 },
  { 254, 1 },
  { 255, 1 },
  { 209, 1 },
  { 209, 1 },
  { 221, 1 },
  { 221, 4 },
  { 221, 5 },
  { 221, 3 },
  { 221, 4 },
  { 221, 4 },
  { 221, 5 },
  { 221, 3 },
  { 221, 4 },
  { 214, 1 },
  { 214, 3 },
  { 170, 1 },
  { 170, 3 },
  { 136, 1 },
  { 136, 1 },
  { 136, 1 },
  { 200, 6 },
  { 200, 8 },
  { 241, 3 },
  { 256, 6 },
  { 137, 2 },
  { 257, 1 },
  { 257, 3 },
  { 258, 1 },
  { 258, 3 },
  { 259, 3 },
  { 260, 4 },
  { 260, 3 },
  { 261, 3 },
  { 262, 4 },
  { 262, 3 },
  { 263, 3 },
  { 264, 1 },
  { 264, 1 },
  { 265, 0 },
  { 265, 2 },
  { 138, 7 },
  { 224, 1 },
  { 224, 3 },
  { 224, 4 },
  { 224, 4 },
  { 224, 3 },
  { 180, 1 },
  { 180, 2 },
  { 228, 6 },
  { 181, 0 },
  { 181, 2 },
  { 222, 5 },
  { 222, 4 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 140, 2 },
  { 267, 4 },
  { 267, 5 },
  { 268, 0 },
  { 268, 2 },
  { 236, 6 },
  { 269, 0 },
  { 270, 0 },
  { 212, 1 },
  { 171, 1 },
  { 171, 3 },
  { 172, 3 },
  { 271, 1 },
  { 188, 0 },
  { 188, 1 },
  { 188, 1 },
  { 188, 2 },
  { 188, 2 },
  { 189, 0 },
  { 189, 1 },
  { 141, 0 },
  { 141, 3 },
  { 201, 3 },
  { 201, 2 },
  { 247, 4 },
  { 154, 5 },
  { 272, 1 },
  { 155, 0 },
  { 210, 1 },
  { 210, 1 },
  { 135, 2 },
  { 194, 2 },
  { 194, 2 },
  { 194, 3 },
  { 194, 5 },
  { 142, 3 },
  { 143, 6 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 202, 8 },
  { 202, 7 },
  { 203, 2 },
  { 203, 5 },
  { 266, 1 },
  { 242, 5 },
  { 230, 1 },
  { 185, 1 },
  { 185, 3 },
  { 151, 4 },
  { 152, 5 },
  { 153, 0 },
  { 273, 2 },
  { 273, 3 },
  { 253, 4 },
  { 253, 1 },
  { 274, 3 },
  { 216, 4 },
  { 237, 1 },
  { 223, 4 },
  { 223, 3 },
  { 204, 2 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 173, 0 },
  { 173, 2 },
  { 173, 2 },
  { 206, 0 },
  { 206, 1 },
  { 206, 1 },
  { 206, 2 },
  { 174, 5 },
  { 207, 2 },
  { 207, 5 },
  { 207, 6 },
  { 146, 1 },
  { 146, 3 },
  { 146, 3 },
  { 177, 1 },
  { 177, 3 },
  { 208, 1 },
  { 208, 4 },
  { 208, 3 },
  { 225, 1 },
  { 225, 1 },
  { 225, 1 },
  { 225, 1 },
  { 275, 1 },
  { 275, 1 },
  { 276, 3 },
  { 277, 2 },
  { 248, 3 },
  { 278, 3 },
  { 134, 2 },
  { 134, 1 },
  { 145, 1 },
  { 145, 2 },
  { 145, 1 },
  { 145, 1 },
  { 145, 1 },
  { 145, 3 },
  { 145, 1 },
  { 145, 1 },
  { 145, 2 },
  { 145, 2 },
  { 145, 2 },
  { 192, 0 },
  { 192, 1 },
  { 193, 1 },
  { 193, 5 },
  { 186, 1 },
  { 186, 3 },
  { 179, 2 },
  { 179, 4 },
  { 178, 1 },
  { 178, 2 },
  { 184, 0 },
  { 184, 2 },
  { 147, 0 },
  { 147, 2 },
  { 231, 2 },
  { 231, 4 },
  { 163, 1 },
  { 163, 2 },
  { 175, 2 },
  { 176, 0 },
  { 176, 1 },
  { 148, 0 },
  { 148, 2 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  int yyruleno                 /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  YYMINORTYPE yygotominor;        /* The LHS of the rule reduced */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  ParseARG_FETCH;

  yymsp = &yypParser->yystack[yypParser->yyidx];

  if( yyruleno>=0 ) {
#ifndef NDEBUG
      if ( yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0]))) {
         if (yyTraceFILE) {
      fprintf(yyTraceFILE, "%sReduce [%s].\n", yyTracePrompt,
              yyRuleName[yyruleno]);
    }
   }
#endif /* NDEBUG */
  } else {
    /* invalid rule number range */
    return;
  }


  /* Silence complaints from purify about yygotominor being uninitialized
  ** in some cases when it is copied into the stack after the following
  ** switch.  yygotominor is uninitialized when a rule reduces that does
  ** not set the value of its left-hand side nonterminal.  Leaving the
  ** value of the nonterminal uninitialized is utterly harmless as long
  ** as the value is never used.  So really the only thing this code
  ** accomplishes is to quieten purify.  
  **
  ** 2007-01-16:  The wireshark project (www.wireshark.org) reports that
  ** without this code, their parser segfaults.  I'm not sure what there
  ** parser is doing to make this happen.  This is the second bug report
  ** from wireshark this week.  Clearly they are stressing Lemon in ways
  ** that it has not been previously stressed...  (SQLite ticket #2172)
  */
  /*memset(&yygotominor, 0, sizeof(yygotominor));*/
  yygotominor = yyzerominor;


  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
      case 0: /* action_body ::= action_body_item_rep statement_rep */
      case 71: /* derive_decl ::= TOK_DERIVE derived_attribute_rep */ yytestcase(yyruleno==71);
      case 181: /* inverse_clause ::= TOK_INVERSE inverse_attr_list */ yytestcase(yyruleno==181);
      case 269: /* statement_rep ::= semicolon statement_rep */ yytestcase(yyruleno==269);
      case 321: /* unique_clause ::= TOK_UNIQUE labelled_attrib_list_list */ yytestcase(yyruleno==321);
      case 328: /* where_rule ::= TOK_WHERE where_clause_list */ yytestcase(yyruleno==328);
      case 330: /* where_rule_OPT ::= where_rule */ yytestcase(yyruleno==330);
#line 362 "expparse.y"
{
    yygotominor.yy371 = yymsp[0].minor.yy371;
}
#line 2265 "expparse.c"
        break;
      case 1: /* action_body_item ::= declaration */
      case 2: /* action_body_item ::= constant_decl */ yytestcase(yyruleno==2);
      case 3: /* action_body_item ::= local_decl */ yytestcase(yyruleno==3);
      case 44: /* block_member ::= declaration */ yytestcase(yyruleno==44);
      case 45: /* block_member ::= include_directive */ yytestcase(yyruleno==45);
      case 46: /* block_member ::= rule_decl */ yytestcase(yyruleno==46);
      case 66: /* declaration ::= entity_decl */ yytestcase(yyruleno==66);
      case 67: /* declaration ::= function_decl */ yytestcase(yyruleno==67);
      case 68: /* declaration ::= procedure_decl */ yytestcase(yyruleno==68);
      case 69: /* declaration ::= type_decl */ yytestcase(yyruleno==69);
      case 88: /* schema_decl_list ::= schema_decl */ yytestcase(yyruleno==88);
      case 158: /* rename_list ::= rename */ yytestcase(yyruleno==158);
      case 167: /* interface_specification ::= use_clause */ yytestcase(yyruleno==167);
      case 168: /* interface_specification ::= reference_clause */ yytestcase(yyruleno==168);
      case 203: /* oneof_op ::= TOK_ONEOF */ yytestcase(yyruleno==203);
      case 251: /* schema_decl ::= include_directive */ yytestcase(yyruleno==251);
      case 291: /* type_item_body ::= enumeration_type */ yytestcase(yyruleno==291);
#line 368 "expparse.y"
{
    yygotominor.yy0 = yymsp[0].minor.yy0;
}
#line 2288 "expparse.c"
        break;
      case 5: /* action_body_item_rep ::= action_body_item action_body_item_rep */
      case 43: /* block_list ::= block_list block_member */ yytestcase(yyruleno==43);
      case 64: /* constant_body_list ::= constant_body constant_body_list */ yytestcase(yyruleno==64);
      case 89: /* schema_decl_list ::= schema_decl_list schema_decl */ yytestcase(yyruleno==89);
      case 170: /* interface_specification_list ::= interface_specification_list interface_specification */ yytestcase(yyruleno==170);
      case 195: /* local_body ::= local_variable local_body */ yytestcase(yyruleno==195);
      case 248: /* schema_body ::= interface_specification_list block_list */ yytestcase(yyruleno==248);
#line 385 "expparse.y"
{
    yygotominor.yy0 = yymsp[-1].minor.yy0;
}
#line 2301 "expparse.c"
        break;
      case 6: /* actual_parameters ::= TOK_LEFT_PAREN expression_list TOK_RIGHT_PAREN */
      case 202: /* nested_id_list ::= TOK_LEFT_PAREN id_list TOK_RIGHT_PAREN */ yytestcase(yyruleno==202);
      case 275: /* subtype_decl ::= TOK_SUBTYPE TOK_OF TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN */ yytestcase(yyruleno==275);
#line 402 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 2310 "expparse.c"
        break;
      case 7: /* actual_parameters ::= TOK_LEFT_PAREN TOK_RIGHT_PAREN */
      case 320: /* unique_clause ::= */ yytestcase(yyruleno==320);
#line 406 "expparse.y"
{
    yygotominor.yy371 = 0;
}
#line 2318 "expparse.c"
        break;
      case 8: /* bound_spec ::= TOK_LEFT_BRACKET expression TOK_COLON expression TOK_RIGHT_BRACKET */
      case 49: /* cardinality_op ::= TOK_LEFT_CURL expression TOK_COLON expression TOK_RIGHT_CURL */ yytestcase(yyruleno==49);
#line 413 "expparse.y"
{
    yygotominor.yy253.lower_limit = yymsp[-3].minor.yy401;
    yygotominor.yy253.upper_limit = yymsp[-1].minor.yy401;
}
#line 2327 "expparse.c"
        break;
      case 9: /* aggregate_initializer ::= TOK_LEFT_BRACKET TOK_RIGHT_BRACKET */
#line 420 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Aggregate);
    yygotominor.yy401->u.list = LISTcreate();
}
#line 2335 "expparse.c"
        break;
      case 10: /* aggregate_initializer ::= TOK_LEFT_BRACKET aggregate_init_body TOK_RIGHT_BRACKET */
#line 426 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Aggregate);
    yygotominor.yy401->u.list = yymsp[-1].minor.yy371;
}
#line 2343 "expparse.c"
        break;
      case 11: /* aggregate_init_element ::= expression */
      case 26: /* assignable ::= identifier */ yytestcase(yyruleno==26);
      case 48: /* by_expression ::= TOK_BY expression */ yytestcase(yyruleno==48);
      case 90: /* expression ::= simple_expression */ yytestcase(yyruleno==90);
      case 105: /* simple_expression ::= unary_expression */ yytestcase(yyruleno==105);
      case 155: /* initializer ::= TOK_ASSIGNMENT expression */ yytestcase(yyruleno==155);
      case 190: /* literal ::= constant */ yytestcase(yyruleno==190);
      case 191: /* local_initializer ::= TOK_ASSIGNMENT expression */ yytestcase(yyruleno==191);
      case 298: /* general_ref ::= assignable */ yytestcase(yyruleno==298);
      case 299: /* unary_expression ::= aggregate_initializer */ yytestcase(yyruleno==299);
      case 301: /* unary_expression ::= literal */ yytestcase(yyruleno==301);
      case 302: /* unary_expression ::= function_call */ yytestcase(yyruleno==302);
      case 303: /* unary_expression ::= identifier */ yytestcase(yyruleno==303);
      case 305: /* unary_expression ::= interval */ yytestcase(yyruleno==305);
      case 306: /* unary_expression ::= query_expression */ yytestcase(yyruleno==306);
      case 308: /* unary_expression ::= TOK_PLUS unary_expression */ yytestcase(yyruleno==308);
      case 323: /* until_control ::= TOK_UNTIL expression */ yytestcase(yyruleno==323);
      case 332: /* while_control ::= TOK_WHILE expression */ yytestcase(yyruleno==332);
#line 432 "expparse.y"
{
    yygotominor.yy401 = yymsp[0].minor.yy401;
}
#line 2367 "expparse.c"
        break;
      case 12: /* aggregate_init_body ::= aggregate_init_element */
      case 114: /* expression_list ::= expression */ yytestcase(yyruleno==114);
      case 282: /* supertype_expression_list ::= supertype_expression */ yytestcase(yyruleno==282);
#line 437 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2377 "expparse.c"
        break;
      case 13: /* aggregate_init_body ::= aggregate_init_element TOK_COLON expression */
#line 442 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[-2].minor.yy401);

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

    yymsp[-2].minor.yy401->type->u.type->body->flags.repeat = 1;
}
#line 2389 "expparse.c"
        break;
      case 14: /* aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element */
#line 452 "expparse.y"
{ 
    yygotominor.yy371 = yymsp[-2].minor.yy371;

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

}
#line 2399 "expparse.c"
        break;
      case 15: /* aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element TOK_COLON expression */
#line 460 "expparse.y"
{
    yygotominor.yy371 = yymsp[-4].minor.yy371;

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[-2].minor.yy401);
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

    yymsp[-2].minor.yy401->type->u.type->body->flags.repeat = 1;
}
#line 2411 "expparse.c"
        break;
      case 16: /* aggregate_type ::= TOK_AGGREGATE TOK_OF parameter_type */
#line 470 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(aggregate_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;

    if (tag_count < 0) {
        Symbol sym;
        sym.line = yylineno;
        sym.filename = current_filename;
        ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym,
	    CURRENT_SCOPE_NAME);
    }
}
#line 2427 "expparse.c"
        break;
      case 17: /* aggregate_type ::= TOK_AGGREGATE TOK_COLON TOK_IDENTIFIER TOK_OF parameter_type */
#line 484 "expparse.y"
{
    Type t = TYPEcreate_user_defined_tag(yymsp[0].minor.yy297, CURRENT_SCOPE, yymsp[-2].minor.yy0.symbol);

    if (t) {
        SCOPEadd_super(t);
        yygotominor.yy477 = TYPEBODYcreate(aggregate_);
        yygotominor.yy477->tag = t;
        yygotominor.yy477->base = yymsp[0].minor.yy297;
    }
}
#line 2441 "expparse.c"
        break;
      case 18: /* aggregation_type ::= array_type */
      case 19: /* aggregation_type ::= bag_type */ yytestcase(yyruleno==19);
      case 20: /* aggregation_type ::= list_type */ yytestcase(yyruleno==20);
      case 21: /* aggregation_type ::= set_type */ yytestcase(yyruleno==21);
#line 496 "expparse.y"
{
    yygotominor.yy477 = yymsp[0].minor.yy477;
}
#line 2451 "expparse.c"
        break;
      case 22: /* alias_statement ::= TOK_ALIAS TOK_IDENTIFIER TOK_FOR general_ref semicolon alias_push_scope statement_rep TOK_END_ALIAS semicolon */
#line 515 "expparse.y"
{
    Expression e = EXPcreate_from_symbol(Type_Attribute, yymsp[-7].minor.yy0.symbol);
    Variable v = VARcreate(e, Type_Unknown);

    v->initializer = yymsp[-5].minor.yy401; 

    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-7].minor.yy0.symbol->name, (Generic)v,
	    yymsp[-7].minor.yy0.symbol, OBJ_VARIABLE);
    yygotominor.yy332 = ALIAScreate(CURRENT_SCOPE, v, yymsp[-2].minor.yy371);

    POP_SCOPE();
}
#line 2467 "expparse.c"
        break;
      case 23: /* alias_push_scope ::= */
#line 529 "expparse.y"
{
    struct Scope_ *s = SCOPEcreate_tiny(OBJ_ALIAS);
    PUSH_SCOPE(s, (Symbol *)0, OBJ_ALIAS);
}
#line 2475 "expparse.c"
        break;
      case 24: /* array_type ::= TOK_ARRAY bound_spec TOK_OF optional_or_unique attribute_type */
#line 536 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(array_);

    yygotominor.yy477->flags.optional = yymsp[-1].minor.yy252.optional;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 2488 "expparse.c"
        break;
      case 25: /* assignable ::= assignable qualifier */
      case 300: /* unary_expression ::= unary_expression qualifier */ yytestcase(yyruleno==300);
#line 548 "expparse.y"
{
    yymsp[0].minor.yy46.first->e.op1 = yymsp[-1].minor.yy401;
    yygotominor.yy401 = yymsp[0].minor.yy46.expr;
}
#line 2497 "expparse.c"
        break;
      case 27: /* assignment_statement ::= assignable TOK_ASSIGNMENT expression semicolon */
#line 559 "expparse.y"
{ 
    yygotominor.yy332 = ASSIGNcreate(yymsp[-3].minor.yy401, yymsp[-1].minor.yy401);
}
#line 2504 "expparse.c"
        break;
      case 28: /* attribute_type ::= aggregation_type */
      case 29: /* attribute_type ::= basic_type */ yytestcase(yyruleno==29);
      case 123: /* parameter_type ::= basic_type */ yytestcase(yyruleno==123);
      case 124: /* parameter_type ::= conformant_aggregation */ yytestcase(yyruleno==124);
#line 564 "expparse.y"
{
    yygotominor.yy297 = TYPEcreate_from_body_anonymously(yymsp[0].minor.yy477);
    SCOPEadd_super(yygotominor.yy297);
}
#line 2515 "expparse.c"
        break;
      case 30: /* attribute_type ::= defined_type */
      case 125: /* parameter_type ::= defined_type */ yytestcase(yyruleno==125);
      case 126: /* parameter_type ::= generic_type */ yytestcase(yyruleno==126);
#line 574 "expparse.y"
{
    yygotominor.yy297 = yymsp[0].minor.yy297;
}
#line 2524 "expparse.c"
        break;
      case 31: /* explicit_attr_list ::= */
      case 51: /* case_action_list ::= */ yytestcase(yyruleno==51);
      case 70: /* derive_decl ::= */ yytestcase(yyruleno==70);
      case 268: /* statement_rep ::= */ yytestcase(yyruleno==268);
#line 579 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
}
#line 2534 "expparse.c"
        break;
      case 32: /* explicit_attr_list ::= explicit_attr_list explicit_attribute */
#line 583 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy371); 
}
#line 2542 "expparse.c"
        break;
      case 33: /* bag_type ::= TOK_BAG bound_spec TOK_OF attribute_type */
      case 139: /* conformant_aggregation ::= TOK_BAG bound_spec TOK_OF parameter_type */ yytestcase(yyruleno==139);
#line 589 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(bag_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 2553 "expparse.c"
        break;
      case 34: /* bag_type ::= TOK_BAG TOK_OF attribute_type */
#line 596 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(bag_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 2561 "expparse.c"
        break;
      case 35: /* basic_type ::= TOK_BOOLEAN */
#line 602 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(boolean_);
}
#line 2568 "expparse.c"
        break;
      case 36: /* basic_type ::= TOK_INTEGER precision_spec */
#line 606 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(integer_);
    yygotominor.yy477->precision = yymsp[0].minor.yy401;
}
#line 2576 "expparse.c"
        break;
      case 37: /* basic_type ::= TOK_REAL precision_spec */
#line 611 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(real_);
    yygotominor.yy477->precision = yymsp[0].minor.yy401;
}
#line 2584 "expparse.c"
        break;
      case 38: /* basic_type ::= TOK_NUMBER */
#line 616 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(number_);
}
#line 2591 "expparse.c"
        break;
      case 39: /* basic_type ::= TOK_LOGICAL */
#line 620 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(logical_);
}
#line 2598 "expparse.c"
        break;
      case 40: /* basic_type ::= TOK_BINARY precision_spec optional_fixed */
#line 624 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(binary_);
    yygotominor.yy477->precision = yymsp[-1].minor.yy401;
    yygotominor.yy477->flags.fixed = yymsp[0].minor.yy252.fixed;
}
#line 2607 "expparse.c"
        break;
      case 41: /* basic_type ::= TOK_STRING precision_spec optional_fixed */
#line 630 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(string_);
    yygotominor.yy477->precision = yymsp[-1].minor.yy401;
    yygotominor.yy477->flags.fixed = yymsp[0].minor.yy252.fixed;
}
#line 2616 "expparse.c"
        break;
      case 47: /* by_expression ::= */
#line 656 "expparse.y"
{
    yygotominor.yy401 = LITERAL_ONE;
}
#line 2623 "expparse.c"
        break;
      case 50: /* case_action ::= case_labels TOK_COLON statement */
#line 672 "expparse.y"
{
    yygotominor.yy321 = CASE_ITcreate(yymsp[-2].minor.yy371, yymsp[0].minor.yy332);
    SYMBOLset(yygotominor.yy321);
}
#line 2631 "expparse.c"
        break;
      case 52: /* case_action_list ::= case_action_list case_action */
#line 682 "expparse.y"
{
    yyerrok;

    yygotominor.yy371 = yymsp[-1].minor.yy371;

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy321);
}
#line 2642 "expparse.c"
        break;
      case 53: /* case_block ::= case_action_list case_otherwise */
#line 691 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;

    if (yymsp[0].minor.yy321) {
        LISTadd_last(yygotominor.yy371,
        (Generic)yymsp[0].minor.yy321);
    }
}
#line 2654 "expparse.c"
        break;
      case 54: /* case_labels ::= expression */
#line 701 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2663 "expparse.c"
        break;
      case 55: /* case_labels ::= case_labels TOK_COMMA expression */
#line 707 "expparse.y"
{
    yyerrok;

    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2673 "expparse.c"
        break;
      case 56: /* case_otherwise ::= */
#line 715 "expparse.y"
{
    yygotominor.yy321 = (Case_Item)0;
}
#line 2680 "expparse.c"
        break;
      case 57: /* case_otherwise ::= TOK_OTHERWISE TOK_COLON statement */
#line 719 "expparse.y"
{
    yygotominor.yy321 = CASE_ITcreate(LIST_NULL, yymsp[0].minor.yy332);
    SYMBOLset(yygotominor.yy321);
}
#line 2688 "expparse.c"
        break;
      case 58: /* case_statement ::= TOK_CASE expression TOK_OF case_block TOK_END_CASE semicolon */
#line 726 "expparse.y"
{
    yygotominor.yy332 = CASEcreate(yymsp[-4].minor.yy401, yymsp[-2].minor.yy371);
}
#line 2695 "expparse.c"
        break;
      case 59: /* compound_statement ::= TOK_BEGIN statement_rep TOK_END semicolon */
#line 731 "expparse.y"
{
    yygotominor.yy332 = COMP_STMTcreate(yymsp[-2].minor.yy371);
}
#line 2702 "expparse.c"
        break;
      case 60: /* constant ::= TOK_PI */
#line 736 "expparse.y"
{ 
    yygotominor.yy401 = LITERAL_PI;
}
#line 2709 "expparse.c"
        break;
      case 61: /* constant ::= TOK_E */
#line 741 "expparse.y"
{ 
    yygotominor.yy401 = LITERAL_E;
}
#line 2716 "expparse.c"
        break;
      case 62: /* constant_body ::= identifier TOK_COLON attribute_type TOK_ASSIGNMENT expression semicolon */
#line 748 "expparse.y"
{
    Variable v;

    yymsp[-5].minor.yy401->type = yymsp[-3].minor.yy297;
    v = VARcreate(yymsp[-5].minor.yy401, yymsp[-3].minor.yy297);
    v->initializer = yymsp[-1].minor.yy401;
    v->flags.constant = 1;
    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-5].minor.yy401->symbol.name, (Generic)v,
	&yymsp[-5].minor.yy401->symbol, OBJ_VARIABLE);
}
#line 2730 "expparse.c"
        break;
      case 65: /* constant_decl ::= TOK_CONSTANT constant_body_list TOK_END_CONSTANT semicolon */
#line 767 "expparse.y"
{
    yygotominor.yy0 = yymsp[-3].minor.yy0;
}
#line 2737 "expparse.c"
        break;
      case 72: /* derived_attribute ::= attribute_decl TOK_COLON attribute_type initializer semicolon */
#line 799 "expparse.y"
{
    yygotominor.yy91 = VARcreate(yymsp[-4].minor.yy401, yymsp[-2].minor.yy297);
    yygotominor.yy91->initializer = yymsp[-1].minor.yy401;
    yygotominor.yy91->flags.attribute = true;
}
#line 2746 "expparse.c"
        break;
      case 73: /* derived_attribute_rep ::= derived_attribute */
      case 177: /* inverse_attr_list ::= inverse_attr */ yytestcase(yyruleno==177);
#line 806 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy91);
}
#line 2755 "expparse.c"
        break;
      case 74: /* derived_attribute_rep ::= derived_attribute_rep derived_attribute */
      case 178: /* inverse_attr_list ::= inverse_attr_list inverse_attr */ yytestcase(yyruleno==178);
#line 811 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy91);
}
#line 2764 "expparse.c"
        break;
      case 75: /* entity_body ::= explicit_attr_list derive_decl inverse_clause unique_clause where_rule_OPT */
#line 818 "expparse.y"
{
    yygotominor.yy176.attributes = yymsp[-4].minor.yy371;
    /* this is flattened out in entity_decl - DEL */
    LISTadd_last(yygotominor.yy176.attributes, (Generic)yymsp[-3].minor.yy371);

    if (yymsp[-2].minor.yy371 != LIST_NULL) {
	LISTadd_last(yygotominor.yy176.attributes, (Generic)yymsp[-2].minor.yy371);
    }

    yygotominor.yy176.unique = yymsp[-1].minor.yy371;
    yygotominor.yy176.where = yymsp[0].minor.yy371;
}
#line 2780 "expparse.c"
        break;
      case 76: /* entity_decl ::= entity_header subsuper_decl semicolon entity_body TOK_END_ENTITY semicolon */
#line 833 "expparse.y"
{
    CURRENT_SCOPE->u.entity->subtype_expression = yymsp[-4].minor.yy242.subtypes;
    CURRENT_SCOPE->u.entity->supertype_symbols = yymsp[-4].minor.yy242.supertypes;
    LISTdo (yymsp[-2].minor.yy176.attributes, l, Linked_List) {
        LISTdo (l, a, Variable) {
            ENTITYadd_attribute(CURRENT_SCOPE, a);
        } LISTod;
    } LISTod;
    CURRENT_SCOPE->u.entity->abstract = yymsp[-4].minor.yy242.abstract;
    CURRENT_SCOPE->u.entity->unique = yymsp[-2].minor.yy176.unique;
    CURRENT_SCOPE->where = yymsp[-2].minor.yy176.where;
    POP_SCOPE();
}
#line 2797 "expparse.c"
        break;
      case 77: /* entity_header ::= TOK_ENTITY TOK_IDENTIFIER */
#line 848 "expparse.y"
{
    Entity e = ENTITYcreate(yymsp[0].minor.yy0.symbol);

    if (print_objects_while_running & OBJ_ENTITY_BITS) {
	fprintf(stdout, "parse: %s (entity)\n", yymsp[0].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(e, yymsp[0].minor.yy0.symbol, OBJ_ENTITY);
}
#line 2810 "expparse.c"
        break;
      case 78: /* enumeration_type ::= TOK_ENUMERATION TOK_OF nested_id_list */
#line 859 "expparse.y"
{
    int value = 0;
    Expression x;
    Symbol *tmp;
    TypeBody tb;
    tb = TYPEBODYcreate(enumeration_);
    CURRENT_SCOPE->u.type->head = 0;
    CURRENT_SCOPE->u.type->body = tb;
    tb->list = yymsp[0].minor.yy371;

    if (!CURRENT_SCOPE->symbol_table) {
        CURRENT_SCOPE->symbol_table = DICTcreate(25);
    }
    if (!PREVIOUS_SCOPE->enum_table) {
        PREVIOUS_SCOPE->enum_table = DICTcreate(25);
    }
    LISTdo_links(yymsp[0].minor.yy371, id) {
        tmp = (Symbol *)id->data;
        id->data = (Generic)(x = EXPcreate(CURRENT_SCOPE));
        x->symbol = *(tmp);
        x->u.integer = ++value;

        /* define both in enum scope and scope of */
        /* 1st visibility */
        DICT_define(CURRENT_SCOPE->symbol_table, x->symbol.name,
            (Generic)x, &x->symbol, OBJ_EXPRESSION);
        DICTdefine(PREVIOUS_SCOPE->enum_table, x->symbol.name,
            (Generic)x, &x->symbol, OBJ_EXPRESSION);
        SYMBOL_destroy(tmp);
    } LISTod;
}
#line 2845 "expparse.c"
        break;
      case 79: /* escape_statement ::= TOK_ESCAPE semicolon */
#line 892 "expparse.y"
{
    yygotominor.yy332 = STATEMENT_ESCAPE;
}
#line 2852 "expparse.c"
        break;
      case 80: /* attribute_decl ::= TOK_IDENTIFIER */
#line 897 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Attribute);
    yygotominor.yy401->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 2861 "expparse.c"
        break;
      case 81: /* attribute_decl ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER */
#line 904 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Expression);
    yygotominor.yy401->e.op1 = EXPcreate(Type_Expression);
    yygotominor.yy401->e.op1->e.op_code = OP_GROUP;
    yygotominor.yy401->e.op1->e.op1 = EXPcreate(Type_Self);
    yygotominor.yy401->e.op1->e.op2 = EXPcreate_from_symbol(Type_Entity, yymsp[-2].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[-2].minor.yy0.symbol);

    yygotominor.yy401->e.op_code = OP_DOT;
    yygotominor.yy401->e.op2 = EXPcreate_from_symbol(Type_Attribute, yymsp[0].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 2877 "expparse.c"
        break;
      case 82: /* attribute_decl_list ::= attribute_decl */
#line 918 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

}
#line 2886 "expparse.c"
        break;
      case 83: /* attribute_decl_list ::= attribute_decl_list TOK_COMMA attribute_decl */
      case 115: /* expression_list ::= expression_list TOK_COMMA expression */ yytestcase(yyruleno==115);
#line 925 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2895 "expparse.c"
        break;
      case 84: /* optional ::= */
#line 931 "expparse.y"
{
    yygotominor.yy252.optional = 0;
}
#line 2902 "expparse.c"
        break;
      case 85: /* optional ::= TOK_OPTIONAL */
#line 935 "expparse.y"
{
    yygotominor.yy252.optional = 1;
}
#line 2909 "expparse.c"
        break;
      case 86: /* explicit_attribute ::= attribute_decl_list TOK_COLON optional attribute_type semicolon */
#line 941 "expparse.y"
{
    Variable v;

    LISTdo_links (yymsp[-4].minor.yy371, attr)
	v = VARcreate((Expression)attr->data, yymsp[-1].minor.yy297);
	v->flags.optional = yymsp[-2].minor.yy252.optional;
	v->flags.attribute = true;
	attr->data = (Generic)v;
    LISTod;

    yygotominor.yy371 = yymsp[-4].minor.yy371;
}
#line 2925 "expparse.c"
        break;
      case 91: /* expression ::= expression TOK_AND expression */
#line 970 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_AND, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2934 "expparse.c"
        break;
      case 92: /* expression ::= expression TOK_OR expression */
#line 976 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_OR, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2943 "expparse.c"
        break;
      case 93: /* expression ::= expression TOK_XOR expression */
#line 982 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_XOR, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2952 "expparse.c"
        break;
      case 94: /* expression ::= expression TOK_LESS_THAN expression */
#line 988 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_LESS_THAN, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2961 "expparse.c"
        break;
      case 95: /* expression ::= expression TOK_GREATER_THAN expression */
#line 994 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_GREATER_THAN, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2970 "expparse.c"
        break;
      case 96: /* expression ::= expression TOK_EQUAL expression */
#line 1000 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2979 "expparse.c"
        break;
      case 97: /* expression ::= expression TOK_LESS_EQUAL expression */
#line 1006 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_LESS_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2988 "expparse.c"
        break;
      case 98: /* expression ::= expression TOK_GREATER_EQUAL expression */
#line 1012 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_GREATER_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2997 "expparse.c"
        break;
      case 99: /* expression ::= expression TOK_NOT_EQUAL expression */
#line 1018 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_NOT_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3006 "expparse.c"
        break;
      case 100: /* expression ::= expression TOK_INST_EQUAL expression */
#line 1024 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_INST_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3015 "expparse.c"
        break;
      case 101: /* expression ::= expression TOK_INST_NOT_EQUAL expression */
#line 1030 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_INST_NOT_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3024 "expparse.c"
        break;
      case 102: /* expression ::= expression TOK_IN expression */
#line 1036 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_IN, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3033 "expparse.c"
        break;
      case 103: /* expression ::= expression TOK_LIKE expression */
#line 1042 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_LIKE, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3042 "expparse.c"
        break;
      case 104: /* expression ::= simple_expression cardinality_op simple_expression */
      case 240: /* right_curl ::= TOK_RIGHT_CURL */ yytestcase(yyruleno==240);
      case 254: /* semicolon ::= TOK_SEMICOLON */ yytestcase(yyruleno==254);
#line 1048 "expparse.y"
{
    yyerrok;
}
#line 3051 "expparse.c"
        break;
      case 106: /* simple_expression ::= simple_expression TOK_CONCAT_OP simple_expression */
#line 1058 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_CONCAT, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3060 "expparse.c"
        break;
      case 107: /* simple_expression ::= simple_expression TOK_EXP simple_expression */
#line 1064 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_EXP, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3069 "expparse.c"
        break;
      case 108: /* simple_expression ::= simple_expression TOK_TIMES simple_expression */
#line 1070 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_TIMES, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3078 "expparse.c"
        break;
      case 109: /* simple_expression ::= simple_expression TOK_DIV simple_expression */
#line 1076 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_DIV, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3087 "expparse.c"
        break;
      case 110: /* simple_expression ::= simple_expression TOK_REAL_DIV simple_expression */
#line 1082 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_REAL_DIV, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3096 "expparse.c"
        break;
      case 111: /* simple_expression ::= simple_expression TOK_MOD simple_expression */
#line 1088 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_MOD, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3105 "expparse.c"
        break;
      case 112: /* simple_expression ::= simple_expression TOK_PLUS simple_expression */
#line 1094 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_PLUS, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3114 "expparse.c"
        break;
      case 113: /* simple_expression ::= simple_expression TOK_MINUS simple_expression */
#line 1100 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_MINUS, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3123 "expparse.c"
        break;
      case 116: /* var ::= */
#line 1118 "expparse.y"
{
    yygotominor.yy252.var = 1;
}
#line 3130 "expparse.c"
        break;
      case 117: /* var ::= TOK_VAR */
#line 1122 "expparse.y"
{
    yygotominor.yy252.var = 0;
}
#line 3137 "expparse.c"
        break;
      case 118: /* formal_parameter ::= var id_list TOK_COLON parameter_type */
#line 1127 "expparse.y"
{
    Symbol *tmp;
    Expression e;
    Variable v;

    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTdo_links(yygotominor.yy371, param)
    tmp = (Symbol*)param->data;

    e = EXPcreate_from_symbol(Type_Attribute, tmp);
    v = VARcreate(e, yymsp[0].minor.yy297);
    v->flags.optional = yymsp[-3].minor.yy252.var;
    v->flags.parameter = true;
    param->data = (Generic)v;

    /* link it in to the current scope's dict */
    DICTdefine(CURRENT_SCOPE->symbol_table,
    tmp->name, (Generic)v, tmp, OBJ_VARIABLE);

    LISTod;
}
#line 3162 "expparse.c"
        break;
      case 119: /* formal_parameter_list ::= */
      case 180: /* inverse_clause ::= */ yytestcase(yyruleno==180);
      case 329: /* where_rule_OPT ::= */ yytestcase(yyruleno==329);
#line 1150 "expparse.y"
{
    yygotominor.yy371 = LIST_NULL;
}
#line 3171 "expparse.c"
        break;
      case 120: /* formal_parameter_list ::= TOK_LEFT_PAREN formal_parameter_rep TOK_RIGHT_PAREN */
#line 1155 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;

}
#line 3179 "expparse.c"
        break;
      case 121: /* formal_parameter_rep ::= formal_parameter */
#line 1161 "expparse.y"
{
    yygotominor.yy371 = yymsp[0].minor.yy371;

}
#line 3187 "expparse.c"
        break;
      case 122: /* formal_parameter_rep ::= formal_parameter_rep semicolon formal_parameter */
#line 1167 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_all(yygotominor.yy371, yymsp[0].minor.yy371);
}
#line 3195 "expparse.c"
        break;
      case 127: /* function_call ::= function_id actual_parameters */
#line 1192 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Funcall);
    yygotominor.yy401->symbol = *yymsp[-1].minor.yy275;
    SYMBOL_destroy(yymsp[-1].minor.yy275);
    yygotominor.yy401->u.funcall.list = yymsp[0].minor.yy371;
}
#line 3205 "expparse.c"
        break;
      case 128: /* function_decl ::= function_header action_body TOK_END_FUNCTION semicolon */
#line 1201 "expparse.y"
{
    FUNCput_body(CURRENT_SCOPE, yymsp[-2].minor.yy371);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-3].minor.yy507, SCANtell());
    POP_SCOPE();
}
#line 3214 "expparse.c"
        break;
      case 129: /* function_header ::= fh_lineno fh_push_scope fh_plist TOK_COLON parameter_type semicolon */
#line 1209 "expparse.y"
{ 
    Function f = CURRENT_SCOPE;

    f->u.func->return_type = yymsp[-1].minor.yy297;
    yygotominor.yy507 = yymsp[-5].minor.yy507;
}
#line 3224 "expparse.c"
        break;
      case 130: /* fh_lineno ::= TOK_FUNCTION */
      case 218: /* ph_get_line ::= */ yytestcase(yyruleno==218);
      case 247: /* rh_get_line ::= */ yytestcase(yyruleno==247);
#line 1217 "expparse.y"
{
    yygotominor.yy507 = SCANtell();
}
#line 3233 "expparse.c"
        break;
      case 131: /* fh_push_scope ::= TOK_IDENTIFIER */
#line 1222 "expparse.y"
{
    Function f = ALGcreate(OBJ_FUNCTION);
    tag_count = 0;
    if (print_objects_while_running & OBJ_FUNCTION_BITS) {
        fprintf(stdout, "parse: %s (function)\n", yymsp[0].minor.yy0.symbol->name);
    }
    PUSH_SCOPE(f, yymsp[0].minor.yy0.symbol, OBJ_FUNCTION);
}
#line 3245 "expparse.c"
        break;
      case 132: /* fh_plist ::= formal_parameter_list */
#line 1232 "expparse.y"
{
    Function f = CURRENT_SCOPE;
    f->u.func->parameters = yymsp[0].minor.yy371;
    f->u.func->pcount = LISTget_length(yymsp[0].minor.yy371);
    f->u.func->tag_count = tag_count;
    tag_count = -1;	/* done with parameters, no new tags can be defined */
}
#line 3256 "expparse.c"
        break;
      case 133: /* function_id ::= TOK_IDENTIFIER */
      case 219: /* procedure_id ::= TOK_IDENTIFIER */ yytestcase(yyruleno==219);
      case 220: /* procedure_id ::= TOK_BUILTIN_PROCEDURE */ yytestcase(yyruleno==220);
#line 1241 "expparse.y"
{
    yygotominor.yy275 = yymsp[0].minor.yy0.symbol;
}
#line 3265 "expparse.c"
        break;
      case 134: /* function_id ::= TOK_BUILTIN_FUNCTION */
#line 1245 "expparse.y"
{
    yygotominor.yy275 = yymsp[0].minor.yy0.symbol;

}
#line 3273 "expparse.c"
        break;
      case 135: /* conformant_aggregation ::= aggregate_type */
#line 1251 "expparse.y"
{
    yygotominor.yy477 = yymsp[0].minor.yy477;

}
#line 3281 "expparse.c"
        break;
      case 136: /* conformant_aggregation ::= TOK_ARRAY TOK_OF optional_or_unique parameter_type */
#line 1257 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(array_);
    yygotominor.yy477->flags.optional = yymsp[-1].minor.yy252.optional;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 3291 "expparse.c"
        break;
      case 137: /* conformant_aggregation ::= TOK_ARRAY bound_spec TOK_OF optional_or_unique parameter_type */
#line 1265 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(array_);
    yygotominor.yy477->flags.optional = yymsp[-1].minor.yy252.optional;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
}
#line 3303 "expparse.c"
        break;
      case 138: /* conformant_aggregation ::= TOK_BAG TOK_OF parameter_type */
#line 1274 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(bag_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;

}
#line 3312 "expparse.c"
        break;
      case 140: /* conformant_aggregation ::= TOK_LIST TOK_OF unique parameter_type */
#line 1287 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->base = yymsp[0].minor.yy297;

}
#line 3322 "expparse.c"
        break;
      case 141: /* conformant_aggregation ::= TOK_LIST bound_spec TOK_OF unique parameter_type */
#line 1295 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
}
#line 3333 "expparse.c"
        break;
      case 142: /* conformant_aggregation ::= TOK_SET TOK_OF parameter_type */
      case 256: /* set_type ::= TOK_SET TOK_OF attribute_type */ yytestcase(yyruleno==256);
#line 1303 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(set_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 3342 "expparse.c"
        break;
      case 143: /* conformant_aggregation ::= TOK_SET bound_spec TOK_OF parameter_type */
#line 1308 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(set_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 3352 "expparse.c"
        break;
      case 144: /* generic_type ::= TOK_GENERIC */
#line 1316 "expparse.y"
{
    yygotominor.yy297 = Type_Generic;

    if (tag_count < 0) {
        Symbol sym;
        sym.line = yylineno;
        sym.filename = current_filename;
        ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym,
	    CURRENT_SCOPE_NAME);
    }
}
#line 3367 "expparse.c"
        break;
      case 145: /* generic_type ::= TOK_GENERIC TOK_COLON TOK_IDENTIFIER */
#line 1328 "expparse.y"
{
    TypeBody g = TYPEBODYcreate(generic_);
    yygotominor.yy297 = TYPEcreate_from_body_anonymously(g);

    SCOPEadd_super(yygotominor.yy297);

    g->tag = TYPEcreate_user_defined_tag(yygotominor.yy297, CURRENT_SCOPE, yymsp[0].minor.yy0.symbol);
    if (g->tag) {
        SCOPEadd_super(g->tag);
    }
}
#line 3382 "expparse.c"
        break;
      case 146: /* id_list ::= TOK_IDENTIFIER */
#line 1341 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy0.symbol);

}
#line 3391 "expparse.c"
        break;
      case 147: /* id_list ::= id_list TOK_COMMA TOK_IDENTIFIER */
#line 1347 "expparse.y"
{
    yyerrok;

    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy0.symbol);
}
#line 3401 "expparse.c"
        break;
      case 148: /* identifier ::= TOK_SELF */
#line 1355 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Self);
}
#line 3408 "expparse.c"
        break;
      case 149: /* identifier ::= TOK_QUESTION_MARK */
#line 1359 "expparse.y"
{
    yygotominor.yy401 = LITERAL_INFINITY;
}
#line 3415 "expparse.c"
        break;
      case 150: /* identifier ::= TOK_IDENTIFIER */
#line 1363 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Identifier);
    yygotominor.yy401->symbol = *(yymsp[0].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3424 "expparse.c"
        break;
      case 151: /* if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_END_IF semicolon */
#line 1371 "expparse.y"
{
    yygotominor.yy332 = CONDcreate(yymsp[-4].minor.yy401, yymsp[-2].minor.yy371, STATEMENT_LIST_NULL);
}
#line 3431 "expparse.c"
        break;
      case 152: /* if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_ELSE statement_rep TOK_END_IF semicolon */
#line 1376 "expparse.y"
{
    yygotominor.yy332 = CONDcreate(yymsp[-6].minor.yy401, yymsp[-4].minor.yy371, yymsp[-2].minor.yy371);
}
#line 3438 "expparse.c"
        break;
      case 153: /* include_directive ::= TOK_INCLUDE TOK_STRING_LITERAL semicolon */
#line 1381 "expparse.y"
{
    SCANinclude_file(yymsp[-1].minor.yy0.string);
}
#line 3445 "expparse.c"
        break;
      case 154: /* increment_control ::= TOK_IDENTIFIER TOK_ASSIGNMENT expression TOK_TO expression by_expression */
#line 1387 "expparse.y"
{
    Increment i = INCR_CTLcreate(yymsp[-5].minor.yy0.symbol, yymsp[-3].minor.yy401, yymsp[-1].minor.yy401, yymsp[0].minor.yy401);

    /* scope doesn't really have/need a name, I suppose */
    /* naming it by the iterator variable is fine */

    PUSH_SCOPE(i, (Symbol *)0, OBJ_INCREMENT);
}
#line 3457 "expparse.c"
        break;
      case 156: /* rename ::= TOK_IDENTIFIER */
#line 1402 "expparse.y"
{
    (*interface_func)(CURRENT_SCOPE, interface_schema, yymsp[0].minor.yy0, yymsp[0].minor.yy0);
}
#line 3464 "expparse.c"
        break;
      case 157: /* rename ::= TOK_IDENTIFIER TOK_AS TOK_IDENTIFIER */
#line 1406 "expparse.y"
{
    (*interface_func)(CURRENT_SCOPE, interface_schema, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
#line 3471 "expparse.c"
        break;
      case 159: /* rename_list ::= rename_list TOK_COMMA rename */
      case 162: /* reference_clause ::= reference_head parened_rename_list semicolon */ yytestcase(yyruleno==162);
      case 165: /* use_clause ::= use_head parened_rename_list semicolon */ yytestcase(yyruleno==165);
      case 249: /* schema_body ::= interface_specification_list constant_decl block_list */ yytestcase(yyruleno==249);
      case 295: /* type_decl ::= td_start TOK_END_TYPE semicolon */ yytestcase(yyruleno==295);
#line 1415 "expparse.y"
{
    yygotominor.yy0 = yymsp[-2].minor.yy0;
}
#line 3482 "expparse.c"
        break;
      case 161: /* reference_clause ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER semicolon */
#line 1422 "expparse.y"
{
    if (!CURRENT_SCHEMA->ref_schemas) {
        CURRENT_SCHEMA->ref_schemas = LISTcreate();
    }

    LISTadd_last(CURRENT_SCHEMA->ref_schemas, (Generic)yymsp[-1].minor.yy0.symbol);
}
#line 3493 "expparse.c"
        break;
      case 163: /* reference_head ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER */
#line 1435 "expparse.y"
{
    interface_schema = yymsp[0].minor.yy0.symbol;
    interface_func = SCHEMAadd_reference;
}
#line 3501 "expparse.c"
        break;
      case 164: /* use_clause ::= TOK_USE TOK_FROM TOK_IDENTIFIER semicolon */
#line 1441 "expparse.y"
{
    if (!CURRENT_SCHEMA->use_schemas) {
        CURRENT_SCHEMA->use_schemas = LISTcreate();
    }

    LISTadd_last(CURRENT_SCHEMA->use_schemas, (Generic)yymsp[-1].minor.yy0.symbol);
}
#line 3512 "expparse.c"
        break;
      case 166: /* use_head ::= TOK_USE TOK_FROM TOK_IDENTIFIER */
#line 1454 "expparse.y"
{
    interface_schema = yymsp[0].minor.yy0.symbol;
    interface_func = SCHEMAadd_use;
}
#line 3520 "expparse.c"
        break;
      case 171: /* interval ::= TOK_LEFT_CURL simple_expression rel_op simple_expression rel_op simple_expression right_curl */
#line 1477 "expparse.y"
{
    Expression	tmp1, tmp2;

    yygotominor.yy401 = (Expression)0;
    tmp1 = BIN_EXPcreate(yymsp[-4].minor.yy126, yymsp[-5].minor.yy401, yymsp[-3].minor.yy401);
    tmp2 = BIN_EXPcreate(yymsp[-2].minor.yy126, yymsp[-3].minor.yy401, yymsp[-1].minor.yy401);
    yygotominor.yy401 = BIN_EXPcreate(OP_AND, tmp1, tmp2);
}
#line 3532 "expparse.c"
        break;
      case 172: /* set_or_bag_of_entity ::= defined_type */
      case 289: /* type ::= defined_type */ yytestcase(yyruleno==289);
#line 1489 "expparse.y"
{
    yygotominor.yy378.type = yymsp[0].minor.yy297;
    yygotominor.yy378.body = 0;
}
#line 3541 "expparse.c"
        break;
      case 173: /* set_or_bag_of_entity ::= TOK_SET TOK_OF defined_type */
#line 1494 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = TYPEBODYcreate(set_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;

}
#line 3551 "expparse.c"
        break;
      case 174: /* set_or_bag_of_entity ::= TOK_SET bound_spec TOK_OF defined_type */
#line 1501 "expparse.y"
{
    yygotominor.yy378.type = 0; 
    yygotominor.yy378.body = TYPEBODYcreate(set_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;
    yygotominor.yy378.body->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy378.body->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 3562 "expparse.c"
        break;
      case 175: /* set_or_bag_of_entity ::= TOK_BAG bound_spec TOK_OF defined_type */
#line 1509 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = TYPEBODYcreate(bag_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;
    yygotominor.yy378.body->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy378.body->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 3573 "expparse.c"
        break;
      case 176: /* set_or_bag_of_entity ::= TOK_BAG TOK_OF defined_type */
#line 1517 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = TYPEBODYcreate(bag_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;
}
#line 3582 "expparse.c"
        break;
      case 179: /* inverse_attr ::= TOK_IDENTIFIER TOK_COLON set_or_bag_of_entity TOK_FOR TOK_IDENTIFIER semicolon */
#line 1536 "expparse.y"
{
    Expression e = EXPcreate(Type_Attribute);

    e->symbol = *yymsp[-5].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[-5].minor.yy0.symbol);

    if (yymsp[-3].minor.yy378.type) {
        yygotominor.yy91 = VARcreate(e, yymsp[-3].minor.yy378.type);
    } else {
        Type t = TYPEcreate_from_body_anonymously(yymsp[-3].minor.yy378.body);
        SCOPEadd_super(t);
        yygotominor.yy91 = VARcreate(e, t);
    }

    yygotominor.yy91->flags.attribute = true;
    yygotominor.yy91->inverse_symbol = yymsp[-1].minor.yy0.symbol;
}
#line 3603 "expparse.c"
        break;
      case 182: /* list_type ::= TOK_LIST bound_spec TOK_OF unique attribute_type */
#line 1564 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
}
#line 3614 "expparse.c"
        break;
      case 183: /* list_type ::= TOK_LIST TOK_OF unique attribute_type */
#line 1572 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
}
#line 3623 "expparse.c"
        break;
      case 184: /* literal ::= TOK_INTEGER_LITERAL */
#line 1579 "expparse.y"
{
    if (yymsp[0].minor.yy0.iVal == 0) {
        yygotominor.yy401 = LITERAL_ZERO;
    } else if (yymsp[0].minor.yy0.iVal == 1) {
	yygotominor.yy401 = LITERAL_ONE;
    } else {
	yygotominor.yy401 = EXPcreate_simple(Type_Integer);
	yygotominor.yy401->u.integer = (int)yymsp[0].minor.yy0.iVal;
	resolved_all(yygotominor.yy401);
    }
}
#line 3638 "expparse.c"
        break;
      case 185: /* literal ::= TOK_REAL_LITERAL */
#line 1591 "expparse.y"
{
    if (yymsp[0].minor.yy0.rVal == 0.0) {
	yygotominor.yy401 = LITERAL_ZERO;
    } else {
	yygotominor.yy401 = EXPcreate_simple(Type_Real);
	yygotominor.yy401->u.real = yymsp[0].minor.yy0.rVal;
	resolved_all(yygotominor.yy401);
    }
}
#line 3651 "expparse.c"
        break;
      case 186: /* literal ::= TOK_STRING_LITERAL */
#line 1601 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_String);
    yygotominor.yy401->symbol.name = yymsp[0].minor.yy0.string;
    resolved_all(yygotominor.yy401);
}
#line 3660 "expparse.c"
        break;
      case 187: /* literal ::= TOK_STRING_LITERAL_ENCODED */
#line 1607 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_String_Encoded);
    yygotominor.yy401->symbol.name = yymsp[0].minor.yy0.string;
    resolved_all(yygotominor.yy401);
}
#line 3669 "expparse.c"
        break;
      case 188: /* literal ::= TOK_LOGICAL_LITERAL */
#line 1613 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_Logical);
    yygotominor.yy401->u.logical = yymsp[0].minor.yy0.logical;
    resolved_all(yygotominor.yy401);
}
#line 3678 "expparse.c"
        break;
      case 189: /* literal ::= TOK_BINARY_LITERAL */
#line 1619 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_Binary);
    yygotominor.yy401->symbol.name = yymsp[0].minor.yy0.binary;
    resolved_all(yygotominor.yy401);
}
#line 3687 "expparse.c"
        break;
      case 192: /* local_variable ::= id_list TOK_COLON parameter_type semicolon */
#line 1635 "expparse.y"
{
    Expression e;
    Variable v;
    LISTdo(yymsp[-3].minor.yy371, sym, Symbol *)

    /* convert symbol to name-expression */

    e = EXPcreate(Type_Attribute);
    e->symbol = *sym; SYMBOL_destroy(sym);
    v = VARcreate(e, yymsp[-1].minor.yy297);
    DICTdefine(CURRENT_SCOPE->symbol_table, e->symbol.name, (Generic)v,
	&e->symbol, OBJ_VARIABLE);
    LISTod;
    LISTfree(yymsp[-3].minor.yy371);
}
#line 3706 "expparse.c"
        break;
      case 193: /* local_variable ::= id_list TOK_COLON parameter_type local_initializer semicolon */
#line 1652 "expparse.y"
{
    Expression e;
    Variable v;
    LISTdo(yymsp[-4].minor.yy371, sym, Symbol *)
    e = EXPcreate(Type_Attribute);
    e->symbol = *sym; SYMBOL_destroy(sym);
    v = VARcreate(e, yymsp[-2].minor.yy297);
    v->initializer = yymsp[-1].minor.yy401;
    DICTdefine(CURRENT_SCOPE->symbol_table, e->symbol.name, (Generic)v,
	&e->symbol, OBJ_VARIABLE);
    LISTod;
    LISTfree(yymsp[-4].minor.yy371);
}
#line 3723 "expparse.c"
        break;
      case 197: /* allow_generic_types ::= */
#line 1675 "expparse.y"
{
    tag_count = 0; /* don't signal an error if we find a generic_type */
}
#line 3730 "expparse.c"
        break;
      case 198: /* disallow_generic_types ::= */
#line 1680 "expparse.y"
{
    tag_count = -1; /* signal an error if we find a generic_type */
}
#line 3737 "expparse.c"
        break;
      case 199: /* defined_type ::= TOK_IDENTIFIER */
#line 1685 "expparse.y"
{
    yygotominor.yy297 = TYPEcreate_name(yymsp[0].minor.yy0.symbol);
    SCOPEadd_super(yygotominor.yy297);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3746 "expparse.c"
        break;
      case 200: /* defined_type_list ::= defined_type */
#line 1692 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy297);

}
#line 3755 "expparse.c"
        break;
      case 201: /* defined_type_list ::= defined_type_list TOK_COMMA defined_type */
#line 1698 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371,
    (Generic)yymsp[0].minor.yy297);
}
#line 3764 "expparse.c"
        break;
      case 204: /* optional_or_unique ::= */
#line 1715 "expparse.y"
{
    yygotominor.yy252.unique = 0;
    yygotominor.yy252.optional = 0;
}
#line 3772 "expparse.c"
        break;
      case 205: /* optional_or_unique ::= TOK_OPTIONAL */
#line 1720 "expparse.y"
{
    yygotominor.yy252.unique = 0;
    yygotominor.yy252.optional = 1;
}
#line 3780 "expparse.c"
        break;
      case 206: /* optional_or_unique ::= TOK_UNIQUE */
#line 1725 "expparse.y"
{
    yygotominor.yy252.unique = 1;
    yygotominor.yy252.optional = 0;
}
#line 3788 "expparse.c"
        break;
      case 207: /* optional_or_unique ::= TOK_OPTIONAL TOK_UNIQUE */
      case 208: /* optional_or_unique ::= TOK_UNIQUE TOK_OPTIONAL */ yytestcase(yyruleno==208);
#line 1730 "expparse.y"
{
    yygotominor.yy252.unique = 1;
    yygotominor.yy252.optional = 1;
}
#line 3797 "expparse.c"
        break;
      case 209: /* optional_fixed ::= */
#line 1741 "expparse.y"
{
    yygotominor.yy252.fixed = 0;
}
#line 3804 "expparse.c"
        break;
      case 210: /* optional_fixed ::= TOK_FIXED */
#line 1745 "expparse.y"
{
    yygotominor.yy252.fixed = 1;
}
#line 3811 "expparse.c"
        break;
      case 211: /* precision_spec ::= */
#line 1750 "expparse.y"
{
    yygotominor.yy401 = (Expression)0;
}
#line 3818 "expparse.c"
        break;
      case 212: /* precision_spec ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN */
      case 304: /* unary_expression ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN */ yytestcase(yyruleno==304);
#line 1754 "expparse.y"
{
    yygotominor.yy401 = yymsp[-1].minor.yy401;
}
#line 3826 "expparse.c"
        break;
      case 213: /* proc_call_statement ::= procedure_id actual_parameters semicolon */
#line 1764 "expparse.y"
{
    yygotominor.yy332 = PCALLcreate(yymsp[-1].minor.yy371);
    yygotominor.yy332->symbol = *(yymsp[-2].minor.yy275);
}
#line 3834 "expparse.c"
        break;
      case 214: /* proc_call_statement ::= procedure_id semicolon */
#line 1769 "expparse.y"
{
    yygotominor.yy332 = PCALLcreate((Linked_List)0);
    yygotominor.yy332->symbol = *(yymsp[-1].minor.yy275);
}
#line 3842 "expparse.c"
        break;
      case 215: /* procedure_decl ::= procedure_header action_body TOK_END_PROCEDURE semicolon */
#line 1776 "expparse.y"
{
    PROCput_body(CURRENT_SCOPE, yymsp[-2].minor.yy371);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-3].minor.yy507, SCANtell());
    POP_SCOPE();
}
#line 3851 "expparse.c"
        break;
      case 216: /* procedure_header ::= TOK_PROCEDURE ph_get_line ph_push_scope formal_parameter_list semicolon */
#line 1784 "expparse.y"
{
    Procedure p = CURRENT_SCOPE;
    p->u.proc->parameters = yymsp[-1].minor.yy371;
    p->u.proc->pcount = LISTget_length(yymsp[-1].minor.yy371);
    p->u.proc->tag_count = tag_count;
    tag_count = -1;	/* done with parameters, no new tags can be defined */
    yygotominor.yy507 = yymsp[-3].minor.yy507;
}
#line 3863 "expparse.c"
        break;
      case 217: /* ph_push_scope ::= TOK_IDENTIFIER */
#line 1794 "expparse.y"
{
    Procedure p = ALGcreate(OBJ_PROCEDURE);
    tag_count = 0;

    if (print_objects_while_running & OBJ_PROCEDURE_BITS) {
	fprintf(stdout, "parse: %s (procedure)\n", yymsp[0].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(p, yymsp[0].minor.yy0.symbol, OBJ_PROCEDURE);
}
#line 3877 "expparse.c"
        break;
      case 221: /* group_ref ::= TOK_BACKSLASH TOK_IDENTIFIER */
#line 1820 "expparse.y"
{
    yygotominor.yy401 = BIN_EXPcreate(OP_GROUP, (Expression)0, (Expression)0);
    yygotominor.yy401->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy401->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3887 "expparse.c"
        break;
      case 222: /* qualifier ::= TOK_DOT TOK_IDENTIFIER */
#line 1828 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = BIN_EXPcreate(OP_DOT, (Expression)0, (Expression)0);
    yygotominor.yy46.expr->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy46.expr->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3897 "expparse.c"
        break;
      case 223: /* qualifier ::= TOK_BACKSLASH TOK_IDENTIFIER */
#line 1835 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = BIN_EXPcreate(OP_GROUP, (Expression)0, (Expression)0);
    yygotominor.yy46.expr->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy46.expr->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3907 "expparse.c"
        break;
      case 224: /* qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_RIGHT_BRACKET */
#line 1844 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = BIN_EXPcreate(OP_ARRAY_ELEMENT, (Expression)0,
	(Expression)0);
    yygotominor.yy46.expr->e.op2 = yymsp[-1].minor.yy401;
}
#line 3916 "expparse.c"
        break;
      case 225: /* qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_COLON simple_expression TOK_RIGHT_BRACKET */
#line 1853 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = TERN_EXPcreate(OP_SUBCOMPONENT, (Expression)0,
	(Expression)0, (Expression)0);
    yygotominor.yy46.expr->e.op2 = yymsp[-3].minor.yy401;
    yygotominor.yy46.expr->e.op3 = yymsp[-1].minor.yy401;
}
#line 3926 "expparse.c"
        break;
      case 226: /* query_expression ::= query_start expression TOK_RIGHT_PAREN */
#line 1861 "expparse.y"
{
    yygotominor.yy401 = yymsp[-2].minor.yy401;
    yygotominor.yy401->u.query->expression = yymsp[-1].minor.yy401;
    POP_SCOPE();
}
#line 3935 "expparse.c"
        break;
      case 227: /* query_start ::= TOK_QUERY TOK_LEFT_PAREN TOK_IDENTIFIER TOK_ALL_IN expression TOK_SUCH_THAT */
#line 1869 "expparse.y"
{
    yygotominor.yy401 = QUERYcreate(yymsp[-3].minor.yy0.symbol, yymsp[-1].minor.yy401);
    SYMBOL_destroy(yymsp[-3].minor.yy0.symbol);
    PUSH_SCOPE(yygotominor.yy401->u.query->scope, (Symbol *)0, OBJ_QUERY);
}
#line 3944 "expparse.c"
        break;
      case 228: /* rel_op ::= TOK_LESS_THAN */
#line 1876 "expparse.y"
{
    yygotominor.yy126 = OP_LESS_THAN;
}
#line 3951 "expparse.c"
        break;
      case 229: /* rel_op ::= TOK_GREATER_THAN */
#line 1880 "expparse.y"
{
    yygotominor.yy126 = OP_GREATER_THAN;
}
#line 3958 "expparse.c"
        break;
      case 230: /* rel_op ::= TOK_EQUAL */
#line 1884 "expparse.y"
{
    yygotominor.yy126 = OP_EQUAL;
}
#line 3965 "expparse.c"
        break;
      case 231: /* rel_op ::= TOK_LESS_EQUAL */
#line 1888 "expparse.y"
{
    yygotominor.yy126 = OP_LESS_EQUAL;
}
#line 3972 "expparse.c"
        break;
      case 232: /* rel_op ::= TOK_GREATER_EQUAL */
#line 1892 "expparse.y"
{
    yygotominor.yy126 = OP_GREATER_EQUAL;
}
#line 3979 "expparse.c"
        break;
      case 233: /* rel_op ::= TOK_NOT_EQUAL */
#line 1896 "expparse.y"
{
    yygotominor.yy126 = OP_NOT_EQUAL;
}
#line 3986 "expparse.c"
        break;
      case 234: /* rel_op ::= TOK_INST_EQUAL */
#line 1900 "expparse.y"
{
    yygotominor.yy126 = OP_INST_EQUAL;
}
#line 3993 "expparse.c"
        break;
      case 235: /* rel_op ::= TOK_INST_NOT_EQUAL */
#line 1904 "expparse.y"
{
    yygotominor.yy126 = OP_INST_NOT_EQUAL;
}
#line 4000 "expparse.c"
        break;
      case 236: /* repeat_statement ::= TOK_REPEAT increment_control while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon */
#line 1912 "expparse.y"
{
    yygotominor.yy332 = LOOPcreate(CURRENT_SCOPE, yymsp[-5].minor.yy401, yymsp[-4].minor.yy401, yymsp[-2].minor.yy371);

    /* matching PUSH_SCOPE is in increment_control */
    POP_SCOPE();
}
#line 4010 "expparse.c"
        break;
      case 237: /* repeat_statement ::= TOK_REPEAT while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon */
#line 1920 "expparse.y"
{
    yygotominor.yy332 = LOOPcreate((struct Scope_ *)0, yymsp[-5].minor.yy401, yymsp[-4].minor.yy401, yymsp[-2].minor.yy371);
}
#line 4017 "expparse.c"
        break;
      case 238: /* return_statement ::= TOK_RETURN semicolon */
#line 1925 "expparse.y"
{
    yygotominor.yy332 = RETcreate((Expression)0);
}
#line 4024 "expparse.c"
        break;
      case 239: /* return_statement ::= TOK_RETURN TOK_LEFT_PAREN expression TOK_RIGHT_PAREN semicolon */
#line 1930 "expparse.y"
{
    yygotominor.yy332 = RETcreate(yymsp[-2].minor.yy401);
}
#line 4031 "expparse.c"
        break;
      case 241: /* rule_decl ::= rule_header action_body where_rule TOK_END_RULE semicolon */
#line 1941 "expparse.y"
{
    RULEput_body(CURRENT_SCOPE, yymsp[-3].minor.yy371);
    RULEput_where(CURRENT_SCOPE, yymsp[-2].minor.yy371);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-4].minor.yy507, SCANtell());
    POP_SCOPE();
}
#line 4041 "expparse.c"
        break;
      case 242: /* rule_formal_parameter ::= TOK_IDENTIFIER */
#line 1949 "expparse.y"
{
    Expression e;
    Type t;

    /* it's true that we know it will be an entity_ type later */
    TypeBody tb = TYPEBODYcreate(set_);
    tb->base = TYPEcreate_name(yymsp[0].minor.yy0.symbol);
    SCOPEadd_super(tb->base);
    t = TYPEcreate_from_body_anonymously(tb);
    SCOPEadd_super(t);
    e = EXPcreate_from_symbol(t, yymsp[0].minor.yy0.symbol);
    yygotominor.yy91 = VARcreate(e, t);
    yygotominor.yy91->flags.attribute = true;
    yygotominor.yy91->flags.parameter = true;

    /* link it in to the current scope's dict */
    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[0].minor.yy0.symbol->name, (Generic)yygotominor.yy91,
	yymsp[0].minor.yy0.symbol, OBJ_VARIABLE);
}
#line 4064 "expparse.c"
        break;
      case 243: /* rule_formal_parameter_list ::= rule_formal_parameter */
#line 1970 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy91); 
}
#line 4072 "expparse.c"
        break;
      case 244: /* rule_formal_parameter_list ::= rule_formal_parameter_list TOK_COMMA rule_formal_parameter */
#line 1976 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy91);
}
#line 4080 "expparse.c"
        break;
      case 245: /* rule_header ::= rh_start rule_formal_parameter_list TOK_RIGHT_PAREN semicolon */
#line 1983 "expparse.y"
{
    CURRENT_SCOPE->u.rule->parameters = yymsp[-2].minor.yy371;

    yygotominor.yy507 = yymsp[-3].minor.yy507;
}
#line 4089 "expparse.c"
        break;
      case 246: /* rh_start ::= TOK_RULE rh_get_line TOK_IDENTIFIER TOK_FOR TOK_LEFT_PAREN */
#line 1991 "expparse.y"
{
    Rule r = ALGcreate(OBJ_RULE);

    if (print_objects_while_running & OBJ_RULE_BITS) {
	fprintf(stdout, "parse: %s (rule)\n", yymsp[-2].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(r, yymsp[-2].minor.yy0.symbol, OBJ_RULE);

    yygotominor.yy507 = yymsp[-3].minor.yy507;
}
#line 4104 "expparse.c"
        break;
      case 250: /* schema_decl ::= schema_header schema_body TOK_END_SCHEMA semicolon */
#line 2018 "expparse.y"
{
    POP_SCOPE();
}
#line 4111 "expparse.c"
        break;
      case 252: /* schema_header ::= TOK_SCHEMA TOK_IDENTIFIER semicolon */
#line 2027 "expparse.y"
{
    Schema schema = ( Schema ) DICTlookup(CURRENT_SCOPE->symbol_table, yymsp[-1].minor.yy0.symbol->name);

    if (print_objects_while_running & OBJ_SCHEMA_BITS) {
	fprintf(stdout, "parse: %s (schema)\n", yymsp[-1].minor.yy0.symbol->name);
    }

    if (EXPRESSignore_duplicate_schemas && schema) {
	SCANskip_to_end_schema(parseData.scanner);
	PUSH_SCOPE_DUMMY();
    } else {
	schema = SCHEMAcreate();
	LISTadd_last(PARSEnew_schemas, (Generic)schema);
	PUSH_SCOPE(schema, yymsp[-1].minor.yy0.symbol, OBJ_SCHEMA);
    }
}
#line 4131 "expparse.c"
        break;
      case 253: /* select_type ::= TOK_SELECT TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN */
#line 2046 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(select_);
    yygotominor.yy477->list = yymsp[-1].minor.yy371;
}
#line 4139 "expparse.c"
        break;
      case 255: /* set_type ::= TOK_SET bound_spec TOK_OF attribute_type */
#line 2057 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(set_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->lower = yymsp[-2].minor.yy253.lower_limit;
    yygotominor.yy477->upper = yymsp[-2].minor.yy253.upper_limit;
}
#line 4149 "expparse.c"
        break;
      case 257: /* skip_statement ::= TOK_SKIP semicolon */
#line 2070 "expparse.y"
{
    yygotominor.yy332 = STATEMENT_SKIP;
}
#line 4156 "expparse.c"
        break;
      case 258: /* statement ::= alias_statement */
      case 259: /* statement ::= assignment_statement */ yytestcase(yyruleno==259);
      case 260: /* statement ::= case_statement */ yytestcase(yyruleno==260);
      case 261: /* statement ::= compound_statement */ yytestcase(yyruleno==261);
      case 262: /* statement ::= escape_statement */ yytestcase(yyruleno==262);
      case 263: /* statement ::= if_statement */ yytestcase(yyruleno==263);
      case 264: /* statement ::= proc_call_statement */ yytestcase(yyruleno==264);
      case 265: /* statement ::= repeat_statement */ yytestcase(yyruleno==265);
      case 266: /* statement ::= return_statement */ yytestcase(yyruleno==266);
      case 267: /* statement ::= skip_statement */ yytestcase(yyruleno==267);
#line 2075 "expparse.y"
{
    yygotominor.yy332 = yymsp[0].minor.yy332;
}
#line 4172 "expparse.c"
        break;
      case 270: /* statement_rep ::= statement statement_rep */
#line 2124 "expparse.y"
{
    yygotominor.yy371 = yymsp[0].minor.yy371;
    LISTadd_first(yygotominor.yy371, (Generic)yymsp[-1].minor.yy332); 
}
#line 4180 "expparse.c"
        break;
      case 271: /* subsuper_decl ::= */
#line 2134 "expparse.y"
{
    yygotominor.yy242.subtypes = EXPRESSION_NULL;
    yygotominor.yy242.abstract = false;
    yygotominor.yy242.supertypes = LIST_NULL;
}
#line 4189 "expparse.c"
        break;
      case 272: /* subsuper_decl ::= supertype_decl */
#line 2140 "expparse.y"
{
    yygotominor.yy242.subtypes = yymsp[0].minor.yy385.subtypes;
    yygotominor.yy242.abstract = yymsp[0].minor.yy385.abstract;
    yygotominor.yy242.supertypes = LIST_NULL;
}
#line 4198 "expparse.c"
        break;
      case 273: /* subsuper_decl ::= subtype_decl */
#line 2146 "expparse.y"
{
    yygotominor.yy242.supertypes = yymsp[0].minor.yy371;
    yygotominor.yy242.abstract = false;
    yygotominor.yy242.subtypes = EXPRESSION_NULL;
}
#line 4207 "expparse.c"
        break;
      case 274: /* subsuper_decl ::= supertype_decl subtype_decl */
#line 2152 "expparse.y"
{
    yygotominor.yy242.subtypes = yymsp[-1].minor.yy385.subtypes;
    yygotominor.yy242.abstract = yymsp[-1].minor.yy385.abstract;
    yygotominor.yy242.supertypes = yymsp[0].minor.yy371;
}
#line 4216 "expparse.c"
        break;
      case 276: /* supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE */
#line 2165 "expparse.y"
{
    yygotominor.yy385.subtypes = (Expression)0;
    yygotominor.yy385.abstract = true;
}
#line 4224 "expparse.c"
        break;
      case 277: /* supertype_decl ::= TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2171 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[-1].minor.yy401;
    yygotominor.yy385.abstract = false;
}
#line 4232 "expparse.c"
        break;
      case 278: /* supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2177 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[-1].minor.yy401;
    yygotominor.yy385.abstract = true;
}
#line 4240 "expparse.c"
        break;
      case 279: /* supertype_expression ::= supertype_factor */
#line 2183 "expparse.y"
{
    yygotominor.yy401 = yymsp[0].minor.yy385.subtypes;
}
#line 4247 "expparse.c"
        break;
      case 280: /* supertype_expression ::= supertype_expression TOK_AND supertype_factor */
#line 2187 "expparse.y"
{
    yygotominor.yy401 = BIN_EXPcreate(OP_AND, yymsp[-2].minor.yy401, yymsp[0].minor.yy385.subtypes);
}
#line 4254 "expparse.c"
        break;
      case 281: /* supertype_expression ::= supertype_expression TOK_ANDOR supertype_factor */
#line 2192 "expparse.y"
{
    yygotominor.yy401 = BIN_EXPcreate(OP_ANDOR, yymsp[-2].minor.yy401, yymsp[0].minor.yy385.subtypes);
}
#line 4261 "expparse.c"
        break;
      case 283: /* supertype_expression_list ::= supertype_expression_list TOK_COMMA supertype_expression */
#line 2203 "expparse.y"
{
    LISTadd_last(yymsp[-2].minor.yy371, (Generic)yymsp[0].minor.yy401);
    yygotominor.yy371 = yymsp[-2].minor.yy371;
}
#line 4269 "expparse.c"
        break;
      case 284: /* supertype_factor ::= identifier */
#line 2209 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[0].minor.yy401;
}
#line 4276 "expparse.c"
        break;
      case 285: /* supertype_factor ::= oneof_op TOK_LEFT_PAREN supertype_expression_list TOK_RIGHT_PAREN */
#line 2214 "expparse.y"
{
    yygotominor.yy385.subtypes = EXPcreate(Type_Oneof);
    yygotominor.yy385.subtypes->u.list = yymsp[-1].minor.yy371;
}
#line 4284 "expparse.c"
        break;
      case 286: /* supertype_factor ::= TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2219 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[-1].minor.yy401;
}
#line 4291 "expparse.c"
        break;
      case 287: /* type ::= aggregation_type */
      case 288: /* type ::= basic_type */ yytestcase(yyruleno==288);
      case 290: /* type ::= select_type */ yytestcase(yyruleno==290);
#line 2224 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = yymsp[0].minor.yy477;
}
#line 4301 "expparse.c"
        break;
      case 292: /* type_item_body ::= type */
#line 2249 "expparse.y"
{
    CURRENT_SCOPE->u.type->head = yymsp[0].minor.yy378.type;
    CURRENT_SCOPE->u.type->body = yymsp[0].minor.yy378.body;
}
#line 4309 "expparse.c"
        break;
      case 294: /* ti_start ::= TOK_IDENTIFIER TOK_EQUAL */
#line 2257 "expparse.y"
{
    Type t = TYPEcreate_name(yymsp[-1].minor.yy0.symbol);
    PUSH_SCOPE(t, yymsp[-1].minor.yy0.symbol, OBJ_TYPE);
}
#line 4317 "expparse.c"
        break;
      case 296: /* td_start ::= TOK_TYPE type_item where_rule_OPT */
#line 2268 "expparse.y"
{
    CURRENT_SCOPE->where = yymsp[0].minor.yy371;
    POP_SCOPE();
    yygotominor.yy0 = yymsp[-2].minor.yy0;
}
#line 4326 "expparse.c"
        break;
      case 297: /* general_ref ::= assignable group_ref */
#line 2275 "expparse.y"
{
    yymsp[0].minor.yy401->e.op1 = yymsp[-1].minor.yy401;
    yygotominor.yy401 = yymsp[0].minor.yy401;
}
#line 4334 "expparse.c"
        break;
      case 307: /* unary_expression ::= TOK_NOT unary_expression */
#line 2318 "expparse.y"
{
    yygotominor.yy401 = UN_EXPcreate(OP_NOT, yymsp[0].minor.yy401);
}
#line 4341 "expparse.c"
        break;
      case 309: /* unary_expression ::= TOK_MINUS unary_expression */
#line 2326 "expparse.y"
{
    yygotominor.yy401 = UN_EXPcreate(OP_NEGATE, yymsp[0].minor.yy401);
}
#line 4348 "expparse.c"
        break;
      case 310: /* unique ::= */
#line 2331 "expparse.y"
{
    yygotominor.yy252.unique = 0;
}
#line 4355 "expparse.c"
        break;
      case 311: /* unique ::= TOK_UNIQUE */
#line 2335 "expparse.y"
{
    yygotominor.yy252.unique = 1;
}
#line 4362 "expparse.c"
        break;
      case 312: /* qualified_attr ::= TOK_IDENTIFIER */
#line 2340 "expparse.y"
{
    yygotominor.yy457 = QUAL_ATTR_new();
    yygotominor.yy457->attribute = yymsp[0].minor.yy0.symbol;
}
#line 4370 "expparse.c"
        break;
      case 313: /* qualified_attr ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER */
#line 2346 "expparse.y"
{
    yygotominor.yy457 = QUAL_ATTR_new();
    yygotominor.yy457->entity = yymsp[-2].minor.yy0.symbol;
    yygotominor.yy457->attribute = yymsp[0].minor.yy0.symbol;
}
#line 4379 "expparse.c"
        break;
      case 314: /* qualified_attr_list ::= qualified_attr */
#line 2353 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy457);
}
#line 4387 "expparse.c"
        break;
      case 315: /* qualified_attr_list ::= qualified_attr_list TOK_COMMA qualified_attr */
#line 2358 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy457);
}
#line 4395 "expparse.c"
        break;
      case 316: /* labelled_attrib_list ::= qualified_attr_list semicolon */
#line 2364 "expparse.y"
{
    LISTadd_first(yymsp[-1].minor.yy371, (Generic)EXPRESSION_NULL);
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 4403 "expparse.c"
        break;
      case 317: /* labelled_attrib_list ::= TOK_IDENTIFIER TOK_COLON qualified_attr_list semicolon */
#line 2370 "expparse.y"
{
    LISTadd_first(yymsp[-1].minor.yy371, (Generic)yymsp[-3].minor.yy0.symbol); 
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 4411 "expparse.c"
        break;
      case 318: /* labelled_attrib_list_list ::= labelled_attrib_list */
#line 2377 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy371);
}
#line 4419 "expparse.c"
        break;
      case 319: /* labelled_attrib_list_list ::= labelled_attrib_list_list labelled_attrib_list */
#line 2383 "expparse.y"
{
    LISTadd_last(yymsp[-1].minor.yy371, (Generic)yymsp[0].minor.yy371);
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 4427 "expparse.c"
        break;
      case 322: /* until_control ::= */
      case 331: /* while_control ::= */ yytestcase(yyruleno==331);
#line 2398 "expparse.y"
{
    yygotominor.yy401 = 0;
}
#line 4435 "expparse.c"
        break;
      case 324: /* where_clause ::= expression semicolon */
#line 2407 "expparse.y"
{
    yygotominor.yy234 = WHERE_new();
    yygotominor.yy234->label = SYMBOLcreate("<unnamed>", yylineno, current_filename);
    yygotominor.yy234->expr = yymsp[-1].minor.yy401;
}
#line 4444 "expparse.c"
        break;
      case 325: /* where_clause ::= TOK_IDENTIFIER TOK_COLON expression semicolon */
#line 2413 "expparse.y"
{
    yygotominor.yy234 = WHERE_new();
    yygotominor.yy234->label = yymsp[-3].minor.yy0.symbol;
    yygotominor.yy234->expr = yymsp[-1].minor.yy401;

    if (!CURRENT_SCOPE->symbol_table) {
	CURRENT_SCOPE->symbol_table = DICTcreate(25);
    }

    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-3].minor.yy0.symbol->name, (Generic)yygotominor.yy234,
	yymsp[-3].minor.yy0.symbol, OBJ_WHERE);
}
#line 4460 "expparse.c"
        break;
      case 326: /* where_clause_list ::= where_clause */
#line 2427 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy234);
}
#line 4468 "expparse.c"
        break;
      case 327: /* where_clause_list ::= where_clause_list where_clause */
#line 2432 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy234);
}
#line 4476 "expparse.c"
        break;
      default:
      /* (4) action_body_item_rep ::= */ yytestcase(yyruleno==4);
      /* (42) block_list ::= */ yytestcase(yyruleno==42);
      /* (63) constant_body_list ::= */ yytestcase(yyruleno==63);
      /* (87) express_file ::= schema_decl_list */ yytestcase(yyruleno==87);
      /* (160) parened_rename_list ::= TOK_LEFT_PAREN rename_list TOK_RIGHT_PAREN */ yytestcase(yyruleno==160);
      /* (169) interface_specification_list ::= */ yytestcase(yyruleno==169);
      /* (194) local_body ::= */ yytestcase(yyruleno==194);
      /* (196) local_decl ::= TOK_LOCAL allow_generic_types local_body TOK_END_LOCAL semicolon disallow_generic_types */ yytestcase(yyruleno==196);
      /* (293) type_item ::= ti_start type_item_body semicolon */ yytestcase(yyruleno==293);
        break;
  };
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yypParser->yyidx -= yysize;
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,(YYCODETYPE)yygoto);
  if( yyact < YYNSTATE ){
#ifdef NDEBUG
    /* If we are not debugging and the reduce action popped at least
    ** one element off the stack, then we can push the new element back
    ** onto the stack here, and skip the stack overflow test in yy_shift().
    ** That gives a significant speed improvement. */
    if( yysize ){
      yypParser->yyidx++;
      yymsp -= yysize-1;
      yymsp->stateno = (YYACTIONTYPE)yyact;
      yymsp->major = (YYCODETYPE)yygoto;
      yymsp->minor = yygotominor;
    }else
#endif
    {
      yy_shift(yypParser,yyact,yygoto,&yygotominor);
    }
  }else{
    assert( yyact == YYNSTATE + YYNRULE + 1 );
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  YYMINORTYPE yyminor            /* The minor type of the error token */
){
  ParseARG_FETCH;
#define TOKEN (yyminor.yy0)
#line 2460 "expparse.y"

    Symbol sym;

    yyerrstatus++;

    sym.line = yylineno;
    sym.filename = current_filename;

    ERRORreport_with_symbol(ERROR_syntax, &sym, "",
	CURRENT_SCOPE_TYPE_PRINTABLE, CURRENT_SCOPE_NAME);
#line 4558 "expparse.c"
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "ParseAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void Parse(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  ParseTOKENTYPE yyminor       /* The value for the token */
  ParseARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  int yyact;            /* The parser action. */
  int yyendofinput;     /* True if we are at the end of input */
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser;  /* The parser */

  /* (re)initialize the parser, if necessary */
  yypParser = (yyParser*)yyp;
  if( yypParser->yyidx<0 ){
#if YYSTACKDEPTH<=0
    if( yypParser->yystksz <=0 ){
      /*memset(&yyminorunion, 0, sizeof(yyminorunion));*/
      yyminorunion = yyzerominor;
      yyStackOverflow(yypParser, &yyminorunion);
      return;
    }
#endif
    yypParser->yyidx = 0;
    yypParser->yyerrcnt = -1;
    yypParser->yystack[0].stateno = 0;
    yypParser->yystack[0].major = 0;
  }
  yyminorunion.yy0 = yyminor;
  yyendofinput = (yymajor==0);
  ParseARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput %s\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,(YYCODETYPE)yymajor);
    if( yyact<YYNSTATE ){
      assert( !yyendofinput );  /* Impossible to shift the $ token */
      yy_shift(yypParser,yyact,yymajor,&yyminorunion);
      yypParser->yyerrcnt--;
      yymajor = YYNOCODE;
    }else if( yyact < YYNSTATE + YYNRULE ){
      yy_reduce(yypParser,yyact-YYNSTATE);
    }else{
      assert( yyact == YY_ERROR_ACTION );
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yymx = yypParser->yystack[yypParser->yyidx].major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor,&yyminorunion);
        yymajor = YYNOCODE;
      }else{
         while(
          yypParser->yyidx >= 0 &&
          yymx != YYERRORSYMBOL &&
          (yyact = yy_find_reduce_action(
                        yypParser->yystack[yypParser->yyidx].stateno,
                        YYERRORSYMBOL)) >= YYNSTATE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yyidx < 0 || yymajor==0 ){
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          YYMINORTYPE u2;
          u2.YYERRSYMDT = 0;
          yy_shift(yypParser,yyact,YYERRORSYMBOL,&u2);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor,yyminorunion);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      yymajor = YYNOCODE;
      
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
      }
      yymajor = YYNOCODE;
#endif
    }
  }while( yymajor!=YYNOCODE && yypParser->yyidx>=0 );
  return;
}
