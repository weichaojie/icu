// 
//   Copyright (C) 2002 International Business Machines Corporation 
//   and others. All rights reserved.  
//
//   file:  regeximp.h
//
//           ICU Regular Expressions,
//               Definitions of constant values used in the compiled form of
//               a regular expression pattern.
//

#ifndef _REGEXIMP_H
#define _REGEXIMP_H

U_NAMESPACE_BEGIN

//
//  debugging support.  Enable one or more of the #defines immediately following
//
#ifdef _DEBUG
//#define REGEX_SCAN_DEBUG
#define REGEX_DUMP_DEBUG
#define REGEX_RUN_DEBUG
#endif
//  End of #defines inteded to be directly set.

#if defined(REGEX_SCAN_DEBUG) || defined(REGEX_RUN_DEBUG) || defined(REGEX_DUMP_DEBUG)
#include <stdio.h>
#endif

#ifdef REGEX_SCAN_DEBUG
#define REGEX_SCAN_DEBUG_PRINTF printf
#else
#define REGEX_SCAN_DEBUG_PRINTF
#endif

#ifdef REGEX_DUMP_DEBUG
#define REGEX_DUMP_DEBUG_PRINTF printf
#else
#define REGEX_DUMP_DEBUG_PRINTF
#endif

#ifdef REGEX_RUN_DEBUG
#define REGEX_RUN_DEBUG_PRINTF printf
#define REGEX_DUMP_DEBUG_PRINTF printf
#else
#define REGEX_RUN_DEBUG_PRINTF
#endif


//
//  Opcode types     In the compiled form of the regexp, these are the type, or opcodes,
//                   of the entries.
//
enum {
     URX_RESERVED_OP   = 0,    // For multi-operand ops, most non-first words.
     URX_RESERVED_OP_N = 255,  // For multi-operand ops, negative operand values.
     URX_BACKTRACK     = 1,
     URX_END           = 2,
     URX_ONECHAR       = 3,    // Value field is the 21 bit unicode char to match
     URX_STRING        = 4,    // Value field is index of string start
     URX_STRING_LEN    = 5,    // Value field is string length (code units)
     URX_STATE_SAVE    = 6,    // Value field is pattern position to push
     URX_NOP           = 7,
     URX_START_CAPTURE = 8,    // Value field is capture group number.
     URX_END_CAPTURE   = 9,    // Value field is capture group number
     URX_STATIC_SETREF = 10,   // Value field is index of set in array of sets.   
     URX_SETREF        = 11,   // Value field is index of set in array of sets.
     URX_DOTANY        = 12, 
     URX_JMP           = 13,   // Value field is destination position in
                                                    //   the pattern.
     URX_FAIL          = 14,   // Stop match operation,  No match.

     URX_JMP_SAV       = 15,   // Operand:  JMP destination location
     URX_BACKSLASH_B   = 16,   // Value field:  0:  \b    1:  \B
     URX_BACKSLASH_G   = 17, 
     URX_UNUSED_1      = 18,   // Value field:  0:  \w    1:  \W
     URX_BACKSLASH_X   = 19,
     URX_BACKSLASH_Z   = 20,   // \z   Unconditional end of line.

     URX_DOTANY_ALL    = 21,   // ., in the . matches any mode.
     URX_BACKSLASH_D   = 22,   // Value field:  0:  \d    1:  \D
     URX_CARET         = 23,   // Value field:  1:  multi-line mode.
     URX_DOLLAR        = 24,  // Also for \Z

     URX_CTR_INIT      = 25,   // Counter Inits for {Interval} loops.
     URX_CTR_INIT_NG   = 26,   //   3 kinds, normal, non-greedy, and possesive.
     URX_UNUSED_2      = 27,   //   These are 4 word opcodes.  See description.
                               //    First Operand:  Data loc of counter variable
                               //    2nd   Operand:  Pat loc of the URX_CTR_LOOPx 
                               //                    at the end of the loop.
                               //    3rd   Operand:  Minimum count.
                               //    4th   Operand:  Max count, -1 for unbounded.
     URX_CTR_LOOP      = 28,   // Loop Ops for {interval} loops.
     URX_CTR_LOOP_NG   = 29,   //   Also in three flavors.
     URX_UNUSED_3      = 30,   //   Operand is loc of corresponding CTR_INIT.

     URX_RELOC_OPRND   = 31,   // Operand value in multi-operand ops that refers
                               //   back into compiled pattern code, and thus must
                               //   be relocated when inserting/deleting ops in code.

     URX_STO_SP        = 32,   // Store the stack ptr.  Operand is location within
                               //   matcher data (not stack data) to store it.
     URX_LD_SP         = 33,   // Load the stack pointer.  Operand is location
                               //   to load from.
     URX_BACKREF       = 34,   // Back Reference.  Parameter is the index of the
                               //   capture group variables in the state stack frame.
     URX_STO_INP_LOC   = 35,   // Store the input location.  Operand is location
                               //   within the matcher data (not stack).
     URX_JMPX          = 36,  // Conditional JMP.
                               //   First Operand:  JMP target location.
                               //   Second Operand:  Data location containing an 
                               //     input position.  If current input position ==
                               //     saved input position, FAIL rather than taking
                               //     the JMP
     URX_LA_START      = 37,   // Starting a LookAround expression.
                               //   Save InputPos and SP in static data.
                               //   Operand:  Static data offset for the save
     URX_LA_END        = 38,   // Ending a Lookaround expression.
                               //   Restore InputPos and Stack to saved values.
                               //   Operand:  Static data offset for saved data.
     URX_ONECHAR_I     = 39,   // Test for case-insensitive match of a literal character.
                               //   Operand:  the literal char.
     URX_STRING_I      = 40,   // Case insensitive string compare.
                               //   First Operand:  Index of start of string in string literals
                               //   Second Operand (next word in compiled code):
                               //     the length of the string.
     URX_BACKREF_I     = 41,   // Case insensitive back reference.
                               //   Parameter is the index of the
                               //   capture group variables in the state stack frame.
     URX_DOLLAR_M      = 42,   // $ in multi-line mode.
     URX_CARET_M       = 43,   // ^ in multi-line mode.
     URX_LB_START      = 44,   // LookBehind Start.
                               //   Paramater is data location
     URX_LB_CONT       = 45,   // LookBehind Continue.
                               //   Param 0:  the data location
                               //   Param 1:  The minimum length of the look-behind match
                               //   Param 2:  The max length of the look-behind match
     URX_LB_END        = 46,   // LookBehind End.
                               //   Parameter is the data location.
                               //     Check that match ended at the right spot,
                               //     Restore original input string len.
     URX_LBN_CONT      = 47,   // Negative LookBehind Continue
                               //   Param 0:  the data location
                               //   Param 1:  The minimum length of the look-behind match
                               //   Param 2:  The max     length of the look-behind match
                               //   Param 3:  The pattern loc following the look-behind block.
     URX_LBN_END       = 48    // Negative LookBehind end
                               //   Parameter is the data location.
                               //   Check that the match ended at the right spot.

};           

// Keep this list of opcode names in sync with the above enum
//   Used for debug printing only.
#define URX_OPCODE_NAMES       \
        "               ",     \
        "URX_BACKTRACK",       \
        "END",                 \
        "ONECHAR",             \
        "STRING",              \
        "STRING_LEN",          \
        "STATE_SAVE",          \
        "NOP",                 \
        "START_CAPTURE",       \
        "END_CAPTURE",         \
        "URX_STATIC_SETREF",   \
        "SETREF",              \
        "DOTANY",              \
        "JMP",                 \
        "FAIL",                \
        "URX_JMP_SAV",         \
        "URX_BACKSLASH_B",     \
        "URX_BACKSLASH_G",     \
        "URX_UNUSED_1",        \
        "URX_BACKSLASH_X",     \
        "URX_BACKSLASH_Z",     \
        "URX_DOTANY_ALL",      \
        "URX_BACKSLASH_D",     \
        "URX_CARET",           \
        "URX_DOLLAR",          \
        "CTR_INIT",            \
        "CTR_INIT_NG",         \
        "CTR_UNUSED_2",        \
        "CTR_LOOP",            \
        "CTR_LOOP_NG",         \
        "CTR_UNUSED_3",        \
        "RELOC_OPRND",         \
        "STO_SP",              \
        "LD_SP",               \
        "BACKREF",             \
        "STO_INP_LOC",         \
        "JMPX",                \
        "LA_START",            \
        "LA_END",              \
        "ONECHAR_I",           \
        "STRING_I",            \
        "BACKREF_I",           \
        "DOLLAR_M",            \
        "CARET_M",             \
        "LB_START",            \
        "LB_CONT",             \
        "LB_END",              \
        "LBN_CONT",            \
        "LBN_END"

//
//  Convenience macros for assembling and disassembling a compiled operation.
//
#define URX_BUILD(type, val) (int32_t)((type << 24) | (val))
#define URX_TYPE(x)          ((uint32_t)(x) >> 24) 
#define URX_VAL(x)           ((x) & 0xffffff)

                
//
//  Access to Unicode Sets composite character properties
//     The sets are accessed by the match engine for things like \w (word boundary)
//     
enum {
     URX_ISWORD_SET  = 1,
     URX_ISALNUM_SET = 2,
     URX_ISALPHA_SET = 3,
     URX_ISSPACE_SET = 4,

     URX_GC_NORMAL,          // Sets for finding grapheme cluster boundaries.
     URX_GC_EXTEND,
     URX_GC_CONTROL,
     URX_GC_L,
     URX_GC_LV,
     URX_GC_LVT,
     URX_GC_V,
     URX_GC_T,

     URX_LAST_SET,

     URX_NEG_SET     = 0x800000          // Flag bit to reverse sense of set
                                         //   membership test.
};


//
//  Match Engine State Stack Frame Layout.
//
struct REStackFrame {
    int32_t            fInputIdx;        // Position of next character in the input string
    int32_t            fPatIdx;          // Position of next Op in the compiled pattern
    int32_t            fExtra[2];        // Extra state, for capture group start/ends
                                         //   atomic parentheses, repeat counts, etc.
                                         //   Locations assigned at pattern compile time.
};

//
//  Start-Of-Match type.  Used by find() to quickly scan to positions where a
//                        match might start before firing up the full match engine.
//
enum StartOfMatch {
    START_NO_INFO,             // No hint available.
    START_CHAR,                // Match starts with a literal code point.
    START_SET,                 // Match starts with something matching a set.
    START_START,               // Match starts at start of buffer only (^ or \A)
    START_LINE,                // Match starts with ^ in multi-line mode.
    START_STRING               // Match starts with a literal string.
};

#define START_OF_MATCH_STR(v) ((v)==START_NO_INFO? "START_NO_INFO" : \
                               (v)==START_CHAR?    "START_CHAR"    : \
                               (v)==START_SET?     "START_SET"     : \
                               (v)==START_START?   "START_START"   : \
                               (v)==START_LINE?    "START_LINE"    : \
                               (v)==START_STRING?  "START_STRING"  : \
                                                   "ILLEGAL")
    
U_NAMESPACE_END
#endif

