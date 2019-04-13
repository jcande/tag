#pragma once

#include <stdint.h>

typedef struct _TagBinPureRuleHeader
{
    uint16_t AppendantSize;   // The length, in bytes, of the data
} TagBinPureRuleHeader;

typedef struct _TagBinInputRuleHeader
{
    uint16_t Appendant0Size;    // The length, in bytes, of the data corresponding to a 0 bit
    uint16_t Appendant1Size;    // The length, in bytes, of the data corresponding to a 1 bit
} TagBinInputRuleHeader;

typedef struct _TagBinOutputRuleHeader
{
    uint16_t AppendantSize; // The length, in bytes, of the data
} TagBinOutputRuleHeader;

typedef struct _TagBinRuleHeader
{
    uint8_t Style;
    union {
        TagBinPureRuleHeader Pure;

        TagBinInputRuleHeader In;

        TagBinOutputRuleHeader Out;
    };
} TagBinRuleHeader;

typedef struct _TagBinPureRule
{
    uint8_t *RawAppendant;
} TagBinPureRule;

typedef struct _TabBinInputRule
{
    uint8_t *RawAppendant0;
    uint8_t *RawAppendant1;
} TagBinInputRule;

typedef struct _TabBinOutputRule
{
    uint8_t *RawAppendant;
    uint8_t Bit;
} TagBinOutputRule;

typedef struct _TagBinRule
{
    TagBinRuleHeader Header;

    uint8_t *RawSymbol;

    union {
        TagBinPureRule Pure;

        TagBinInputRule In;

        TagBinOutputRule Out;
    };
} TagBinRule;


#pragma pack(push)
// force alignment to byte boundaries
#pragma pack(1)
typedef struct _TagBinHeader
{
    uint64_t RuleCount;         // How many rules are in the program
    uint32_t SymbolSize;        // The length, in bytes, of each symbol in the program
    uint32_t QueueSize;         // The length, in bytes, of the initial data in the program
    uint32_t DeletionNumber;    // The number of symbols to delete each step
} TagBinHeader;
#pragma pack(pop)

typedef struct _TagBin
{
    TagBinHeader Header;
    TagBinRule *Rules;
    uint8_t *Queue;
} TagBin;
