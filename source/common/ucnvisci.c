/*  
**********************************************************************
*   Copyright (C) 2000-2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   file name:  ucnvisci.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001JUN26
*   created by: Ram Viswanadha
*   
*/

#include "unicode/utypes.h"
#include "cmemory.h"
#include "unicode/ucnv_err.h"
#include "ucnv_bld.h"
#include "unicode/ucnv.h"
#include "ucnv_cnv.h"
#include "unicode/ustring.h"
#include "unicode/ucnv_cb.h"
#include  <stdio.h>

#define NUKTA 0x093c
#define HALANT 0x094d    
#define ZWNJ 0x200c
#define ZWJ 0x200d
#define INVALID_CHAR 0xffff   
#define UCNV_OPTIONS_VERSION_MASK 0xf
#define ATR 0xEF
#define EXT 0xF0
#define DANDA   0x0964
#define DOUBLE_DANDA 0x0965
#define ISCII_NUKTA  0xE9
#define ISCII_HALANT 0xE8
#define ISCII_DANDA  0xEA
#define ISCII_INV    0xD9
#define INDIC_BLOCK_BEGIN 0x0900
#define INDIC_BLOCK_END 0x0D70 
#define INDIC_RANGE 0x0470 /* INDIC_BLOCK_END - INDIC_BLOCK_BEGIN */
#define VOCALLIC_RR 0x0931
#define LF 0x0A

/* TODO: 
 * Add getName() function.
 */
/*********** ISCII Converter Protos ***********/
static void 
_ISCIIOpen(UConverter *cnv, const char *name, const char *locale, uint32_t options,UErrorCode *errorCode);

static void 
_ISCIIClose(UConverter *converter);

U_CFUNC void 
_ISCIIReset(UConverter *converter, UConverterResetChoice choice);

U_CFUNC UConverter * 
_ISCII_SafeClone(const UConverter *cnv, void *stackBuffer, int32_t *pBufferSize, UErrorCode *status);

U_CFUNC void 
UConverter_toUnicode_ISCII_OFFSETS_LOGIC (UConverterToUnicodeArgs *args,UErrorCode *err);

U_CFUNC void 
UConverter_fromUnicode_ISCII_OFFSETS_LOGIC (UConverterFromUnicodeArgs *args,UErrorCode *err);


typedef enum  {
    DEVANAGARI =0,
    BENGALI,
    GURMUKHI,
    GUJARATI,
    ORIYA,
    TAMIL,
    TELUGU,
    KANNADA,
    MALAYALAM,
    DELTA=0x80
}UniLang;

#define KANNADA_DELTA 0x380
/**
 * Enumeration for switching code pages if <ATX>+<one of below values>
 * is encountered
 */
typedef enum {
    DEF =0x40,
    RMN =0x41,
    DEV =0x42,
    BNG =0x43,
    TML =0x44,
    TLG =0x45,
    ASM =0x46,
    ORI =0x47,
    KND =0x48,
    MLM =0x49,
    GJR =0x4A,
    PNJ =0x4B,
    ARB =0x71,
    PES =0x72,
    URD =0x73,
    SND =0x74,
    KSM =0x75,
    PST =0x76
}ISCIILang;

typedef enum{
    DEV_MASK =0x80,
    PNJ_MASK =0x40,
    GJR_MASK =0x20,
    ORI_MASK =0x10,
    BNG_MASK =0x08,
    TLG_MASK =0x04,
    MLM_MASK =0x02,
    TML_MASK =0x01,
    ZERO     =0x00
}MaskEnum;

static UConverterImpl _ISCIIImpl={

    UCNV_ISCII,
    
    NULL,
    NULL,
    
    _ISCIIOpen,
    _ISCIIClose,
    _ISCIIReset,
    
    UConverter_toUnicode_ISCII_OFFSETS_LOGIC,
    UConverter_toUnicode_ISCII_OFFSETS_LOGIC,
    UConverter_fromUnicode_ISCII_OFFSETS_LOGIC,
    UConverter_fromUnicode_ISCII_OFFSETS_LOGIC,
    NULL,
    
    NULL,
    NULL,
    NULL,
    _ISCII_SafeClone
};

const UConverterStaticData _ISCIIStaticData={
    sizeof(UConverterStaticData),
        "ISCII",
         0, 
         UCNV_IBM, 
         UCNV_ISCII, 
         1, 
         4,
        { 0x1a, 0, 0, 0 },
        0x1,
        FALSE, 
        FALSE,
        0x0,
        0x0,
        { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }, /* reserved */

};
   
const UConverterSharedData _ISCIIData={
    sizeof(UConverterSharedData),
        ~((uint32_t) 0),
        NULL, 
        NULL, 
        &_ISCIIStaticData, 
        FALSE, 
        &_ISCIIImpl, 
        0
};

typedef struct{
    UChar contextCharToUnicode;     /* previous Unicode codepoint for contextual analysis */
    UChar contextCharFromUnicode;   /* previous Unicode codepoint for contextual analysis */
    int32_t defDeltaToUnicode;      /* delta for switching to default state when DEF is encountered  */ 
    int32_t currentDeltaFromUnicode;/* current delta in Indic block */
    int32_t currentDeltaToUnicode;  /* current delta in Indic block */  
    MaskEnum currentMaskFromUnicode;/* mask for current state in toUnicode */
    MaskEnum currentMaskToUnicode;  /* mask for current state in toUnicode */
    MaskEnum defMaskToUnicode;      /* mask for default state in toUnicode */
    UBool isFirstBuffer;            
}UConverterDataISCII; 

uint16_t lookupInitialData[][3]={
    DEVANAGARI,DEV_MASK,DEV,
    BENGALI,BNG_MASK,BNG,
    GURMUKHI,PNJ_MASK,PNJ,
    GUJARATI,GJR_MASK,GJR,
    ORIYA,ORI_MASK,ORI,
    TAMIL,TML_MASK,TML,
    TELUGU,TLG_MASK,TLG,
    KANNADA,TLG_MASK,KND,
    MALAYALAM,MLM_MASK,MLM,
};
    
static void 
_ISCIIOpen(UConverter *cnv, const char *name,const char *locale,uint32_t options, UErrorCode *errorCode){
    cnv->extraInfo = uprv_malloc (sizeof (UConverterDataISCII));
    if(cnv->extraInfo != NULL) {
        UConverterDataISCII *converterData=(UConverterDataISCII *) cnv->extraInfo;
        converterData->contextCharToUnicode=0x0000;
        converterData->contextCharFromUnicode=0x0000;

        if((options & UCNV_OPTIONS_VERSION_MASK) < 9){
            converterData->defDeltaToUnicode=
                    lookupInitialData[options & UCNV_OPTIONS_VERSION_MASK][0] * DELTA;
            converterData->currentDeltaFromUnicode=converterData->currentDeltaToUnicode=
                 lookupInitialData[options & UCNV_OPTIONS_VERSION_MASK][0] * DELTA;
            converterData->currentMaskFromUnicode = converterData->currentMaskToUnicode = 
            converterData->defMaskToUnicode=lookupInitialData[options & UCNV_OPTIONS_VERSION_MASK][1];
            
            converterData->isFirstBuffer=TRUE;
        }else{
            *errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        }

    }else{
        *errorCode =U_MEMORY_ALLOCATION_ERROR;
    }
}
static void 
_ISCIIClose(UConverter *cnv){
     uprv_free(cnv->extraInfo);
}

U_CFUNC void 
_ISCIIReset(UConverter *cnv, UConverterResetChoice choice){
    UConverterDataISCII* data =(UConverterDataISCII *) (cnv->extraInfo);
    if(choice<=UCNV_RESET_TO_UNICODE) {
        cnv->toUnicodeStatus = 0;
        cnv->mode=0;
        data->currentDeltaToUnicode=data->defDeltaToUnicode;
        data->currentMaskToUnicode = data->defMaskToUnicode;
        data->contextCharToUnicode=0x00;
    }
    if(choice!=UCNV_RESET_TO_UNICODE) {
        cnv->fromUSurrogateLead=0x0000; 
        data->contextCharFromUnicode=0x00;
        data->currentMaskFromUnicode=data->defDeltaToUnicode;
        data->currentDeltaFromUnicode=data->defDeltaToUnicode;
    }
    data->isFirstBuffer=TRUE;

}

/** 
 * The values in validity table are indexed by the lower bits of Unicode 
 * range 0x0900 - 0x09ff. The values have a structure like: 
 *       ---------------------------------------------------------------
 *      | DEV   | PNJ   | GJR   | ORI   | BNG   | TLG   | MLM   | TML   |
 *      |       |       |       |       | ASM   | KND   |       |       |  
 *       ---------------------------------------------------------------
 * If a code point is valid in a particular script 
 * then that bit is turned on
 */
uint8_t validityTable[256] = {
/* This state table is tool generated please donot edit unless you know exactly what you are doing */

/*0xa0 : 0x00: 0x900  */ ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0xa1 : 0xb8: 0x901  */ DEV_MASK + ZERO     + GJR_MASK + ORI_MASK + BNG_MASK + ZERO     + ZERO     + ZERO     ,
/*0xa2 : 0xfe: 0x902  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xa3 : 0xbf: 0x903  */ DEV_MASK + ZERO     + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0x00 : 0x00: 0x904  */ ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0xa4 : 0xff: 0x905  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xa5 : 0xff: 0x906  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xa6 : 0xff: 0x907  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xa7 : 0xff: 0x908  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xa8 : 0xff: 0x909  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xa9 : 0xff: 0x90a  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xaa : 0xfe: 0x90b  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0x00 : 0x00: 0x90c  */ DEV_MASK + ZERO     + ZERO     + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xae : 0x80: 0x90d  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0xab : 0x87: 0x90e  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xac : 0xff: 0x90f  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xad : 0xff: 0x910  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xb2 : 0x80: 0x911  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0xaf : 0x87: 0x912  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xb0 : 0xff: 0x913  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xb1 : 0xff: 0x914  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xb3 : 0xff: 0x915  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xb4 : 0xfe: 0x916  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xb5 : 0xfe: 0x917  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xb6 : 0xfe: 0x918  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xb7 : 0xff: 0x919  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xb8 : 0xff: 0x91a  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xb9 : 0xfe: 0x91b  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xba : 0xff: 0x91c  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xbb : 0xfe: 0x91d  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xbc : 0xff: 0x91e  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xbd : 0xff: 0x91f  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xbe : 0xfe: 0x920  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xbf : 0xfe: 0x921  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xc0 : 0xfe: 0x922  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xc1 : 0xff: 0x923  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xc2 : 0xff: 0x924  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xc3 : 0xfe: 0x925  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xc4 : 0xfe: 0x926  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xc5 : 0xfe: 0x927  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xc6 : 0xff: 0x928  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xc7 : 0x81: 0x929  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + TML_MASK ,
/*0xc8 : 0xff: 0x92a  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xc9 : 0xfe: 0x92b  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xca : 0xfe: 0x92c  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xcb : 0xfe: 0x92d  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xcc : 0xfe: 0x92e  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xcd : 0xff: 0x92f  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xcf : 0xff: 0x930  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xd0 : 0x87: 0x931  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xd1 : 0xff: 0x932  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xd2 : 0xb7: 0x933  */ DEV_MASK + ZERO     + GJR_MASK + ORI_MASK + ZERO     + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xd3 : 0x83: 0x934  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + MLM_MASK + TML_MASK ,
/*0xd4 : 0xff: 0x935  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xd5 : 0xfe: 0x936  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0xd6 : 0xbf: 0x937  */ DEV_MASK + ZERO     + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xd7 : 0xff: 0x938  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xd8 : 0xff: 0x939  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0x00 : 0x00: 0x93A  */ ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x93B  */ ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0xe9 : 0xda: 0x93c  */ DEV_MASK + PNJ_MASK + ZERO     + ORI_MASK + BNG_MASK + ZERO     + MLM_MASK + ZERO     ,
/*0x00 : 0x00: 0x93d  */ ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0xda : 0xff: 0x93e  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xdb : 0xff: 0x93f  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xdc : 0xff: 0x940  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xdd : 0xff: 0x941  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xde : 0xff: 0x942  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xdf : 0xbe: 0x943  */ DEV_MASK + ZERO     + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0x00 : 0x00: 0x944  */ ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0xe3 : 0x80: 0x945  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0xe0 : 0x87: 0x946  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xe1 : 0xff: 0x947  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xe2 : 0xff: 0x948  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xe7 : 0x80: 0x949  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0xe4 : 0x87: 0x94a  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xe5 : 0xff: 0x94b  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xe6 : 0xff: 0x94c  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xe8 : 0xff: 0x94d  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xec : 0x00: 0x94e  */ ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0xed : 0x00: 0x94f  */ ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x950  */ DEV_MASK + ZERO     + GJR_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x951  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x952  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x953  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x954  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x955  */ ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + TLG_MASK + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x956  */ ZERO     + ZERO     + ZERO     + ORI_MASK + ZERO     + TLG_MASK + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x957  */ ZERO     + ZERO     + ZERO     + ORI_MASK + ZERO     + ZERO     + MLM_MASK + ZERO     ,
/*0x00 : 0x00: 0x958  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x959  */ DEV_MASK + PNJ_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x95a  */ DEV_MASK + PNJ_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x95b  */ DEV_MASK + PNJ_MASK + ZERO     + ORI_MASK + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x95c  */ DEV_MASK + PNJ_MASK + ZERO     + ZERO     + BNG_MASK + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x95d  */ DEV_MASK + ZERO     + ZERO     + ORI_MASK + BNG_MASK + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x95e  */ DEV_MASK + PNJ_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
/*0xce : 0x98: 0x95f  */ DEV_MASK + ZERO     + ZERO     + ORI_MASK + BNG_MASK + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x960  */ DEV_MASK + ZERO     + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0x00 : 0x00: 0x961  */ DEV_MASK + ZERO     + ZERO     + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + ZERO     ,
/*0x00 : 0x00: 0x962  */ DEV_MASK + ZERO     + ZERO     + ZERO     + BNG_MASK + ZERO     + ZERO     + ZERO     ,
/*0x00 : 0x00: 0x963  */ DEV_MASK + ZERO     + ZERO     + ZERO     + BNG_MASK + ZERO     + ZERO     + ZERO     ,
/*0xea : 0xf8: 0x964  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + ZERO     + ZERO     + ZERO     ,
/*0xeaea : 0x00: 0x965*/ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + ZERO     + ZERO     + ZERO     ,
/*0xf1 : 0xff: 0x966  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xf2 : 0xff: 0x967  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xf3 : 0xff: 0x968  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xf4 : 0xff: 0x969  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xf5 : 0xff: 0x96a  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xf6 : 0xff: 0x96b  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xf7 : 0xff: 0x96c  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xf8 : 0xff: 0x96d  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xf9 : 0xff: 0x96e  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0xfa : 0xff: 0x96f  */ DEV_MASK + PNJ_MASK + GJR_MASK + ORI_MASK + BNG_MASK + TLG_MASK + MLM_MASK + TML_MASK ,
/*0x00 : 0x00: 0x970  */ DEV_MASK + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     + ZERO     ,
};

uint16_t fromUnicodeTable[128]={   
    0x00a0 ,/*  0x0900  */ 
    0x00a1 ,/*  0x0901  */ 
    0x00a2 ,/*  0x0902  */ 
    0x00a3 ,/*  0x0903  */ 
    0xFFFF ,/*  0x0904  */ 
    0x00a4 ,/*  0x0905  */ 
    0x00a5 ,/*  0x0906  */ 
    0x00a6 ,/*  0x0907  */ 
    0x00a7 ,/*  0x0908  */ 
    0x00a8 ,/*  0x0909  */ 
    0x00a9 ,/*  0x090a  */ 
    0x00aa ,/*  0x090b  */ 
    0xA6E9 ,/*  0x090c  */ 
    0x00ae ,/*  0x090d  */ 
    0x00ab ,/*  0x090e  */ 
    0x00ac ,/*  0x090f  */ 
    0x00ad ,/*  0x0910  */ 
    0x00b2 ,/*  0x0911  */ 
    0x00af ,/*  0x0912  */ 
    0x00b0 ,/*  0x0913  */ 
    0x00b1 ,/*  0x0914  */ 
    0x00b3 ,/*  0x0915  */ 
    0x00b4 ,/*  0x0916  */ 
    0x00b5 ,/*  0x0917  */ 
    0x00b6 ,/*  0x0918  */ 
    0x00b7 ,/*  0x0919  */ 
    0x00b8 ,/*  0x091a  */ 
    0x00b9 ,/*  0x091b  */ 
    0x00ba ,/*  0x091c  */ 
    0x00bb ,/*  0x091d  */ 
    0x00bc ,/*  0x091e  */ 
    0x00bd ,/*  0x091f  */ 
    0x00be ,/*  0x0920  */ 
    0x00bf ,/*  0x0921  */ 
    0x00c0 ,/*  0x0922  */ 
    0x00c1 ,/*  0x0923  */ 
    0x00c2 ,/*  0x0924  */ 
    0x00c3 ,/*  0x0925  */ 
    0x00c4 ,/*  0x0926  */ 
    0x00c5 ,/*  0x0927  */ 
    0x00c6 ,/*  0x0928  */ 
    0x00c7 ,/*  0x0929  */ 
    0x00c8 ,/*  0x092a  */ 
    0x00c9 ,/*  0x092b  */ 
    0x00ca ,/*  0x092c  */ 
    0x00cb ,/*  0x092d  */ 
    0x00cc ,/*  0x092e  */ 
    0x00cd ,/*  0x092f  */ 
    0x00cf ,/*  0x0930  */ 
    0x00d0 ,/*  0x0931  */ 
    0x00d1 ,/*  0x0932  */ 
    0x00d2 ,/*  0x0933  */ 
    0x00d3 ,/*  0x0934  */ 
    0x00d4 ,/*  0x0935  */ 
    0x00d5 ,/*  0x0936  */ 
    0x00d6 ,/*  0x0937  */ 
    0x00d7 ,/*  0x0938  */ 
    0x00d8 ,/*  0x0939  */ 
    0xFFFF ,/*  0x093A  */ 
    0xFFFF ,/*  0x093B  */ 
    0x00e9 ,/*  0x093c  */ 
    0xFFFF ,/*  0x093d  */ 
    0x00da ,/*  0x093e  */ 
    0x00db ,/*  0x093f  */ 
    0x00dc ,/*  0x0940  */ 
    0x00dd ,/*  0x0941  */ 
    0x00de ,/*  0x0942  */ 
    0x00df ,/*  0x0943  */ 
    0xFFFF ,/*  0x0944  */ 
    0x00e3 ,/*  0x0945  */ 
    0x00e0 ,/*  0x0946  */ 
    0x00e1 ,/*  0x0947  */ 
    0x00e2 ,/*  0x0948  */ 
    0x00e7 ,/*  0x0949  */ 
    0x00e4 ,/*  0x094a  */ 
    0x00e5 ,/*  0x094b  */ 
    0x00e6 ,/*  0x094c  */ 
    0x00e8 ,/*  0x094d  */ 
    0x00ec ,/*  0x094e  */ 
    0x00ed ,/*  0x094f  */ 
    0xA1E9 ,/*  0x0950  */ /* OM Symbol */
    0xF0B5 ,/*  0x0951  */ 
    0xF0B8 ,/*  0x0952  */ 
    0xFFFF ,/*  0x0953  */ 
    0xFFFF ,/*  0x0954  */ 
    0xFFFF ,/*  0x0955  */ 
    0xFFFF ,/*  0x0956  */ 
    0xFFFF ,/*  0x0957  */ 
    0xb3e9 ,/*  0x0958  */ 
    0xb4e9 ,/*  0x0959  */ 
    0xb5e9 ,/*  0x095a  */ 
    0xbae9 ,/*  0x095b  */ 
    0xbfe9 ,/*  0x095c  */ 
    0xC0E9 ,/*  0x095d  */ 
    0xc9e9 ,/*  0x095e  */ 
    0x00ce ,/*  0x095f  */ 
    0xAAe9 ,/*  0x0960  */ 
    0xA7E9 ,/*  0x0961  */ 
    0x1DE9 ,/*  0x0962  */ 
    0xDCE9 ,/*  0x0963  */ 
    0x00ea ,/*  0x0964  */ 
    0xeaea ,/*  0x0965  */ 
    0x00f1 ,/*  0x0966  */ 
    0x00f2 ,/*  0x0967  */ 
    0x00f3 ,/*  0x0968  */ 
    0x00f4 ,/*  0x0969  */ 
    0x00f5 ,/*  0x096a  */ 
    0x00f6 ,/*  0x096b  */ 
    0x00f7 ,/*  0x096c  */ 
    0x00f8 ,/*  0x096d  */ 
    0x00f9 ,/*  0x096e  */ 
    0x00fa ,/*  0x096f  */ 
    0xFFFF ,/*  0x0970  */ 
  
};
uint16_t toUnicodeTable[256]={
    0x0000,
    0x0001,
    0x0002,
    0x0003,
    0x0004,
    0x0005,
    0x0006,
    0x0007,
    0x0008,
    0x0009,
    0x000a,
    0x000b,
    0x000c,
    0x000d,
    0x000e,
    0x000f,
    0x0010,
    0x0011,
    0x0012,
    0x0013,
    0x0014,
    0x0015,
    0x0016,
    0x0017,
    0x0018,
    0x0019,
    0x001a,
    0x001b,
    0x001c,
    0x001d,
    0x001e,
    0x001f,
    0x0020,
    0x0021,
    0x0022,
    0x0023,
    0x0024,
    0x0025,
    0x0026,
    0x0027,
    0x0028,
    0x0029,
    0x002a,
    0x002b,
    0x002c,
    0x002d,
    0x002e,
    0x002f,
    0x0030,
    0x0031,
    0x0032,
    0x0033,
    0x0034,
    0x0035,
    0x0036,
    0x0037,
    0x0038,
    0x0039,
    0x003A,
    0x003B,
    0x003c,
    0x003d,
    0x003e,
    0x003f,
    0x0040,
    0x0041,
    0x0042,
    0x0043,
    0x0044,
    0x0045,
    0x0046,
    0x0047,
    0x0048,
    0x0049,
    0x004a,
    0x004b,
    0x004c,
    0x004d,
    0x004e,
    0x004f,
    0x0050,
    0x0051,
    0x0052,
    0x0053,
    0x0054,
    0x0055,
    0x0056,
    0x0057,
    0x0058,
    0x0059,
    0x005a,
    0x005b,
    0x005c,
    0x005d,
    0x005e,
    0x005f,
    0x0060,
    0x0061,
    0x0062,
    0x0063,
    0x0064,
    0x0065,
    0x0066,
    0x0067,
    0x0068,
    0x0069,
    0x006a,
    0x006b,
    0x006c,
    0x006d,
    0x006e,
    0x006f,
    0x0070,
    0x0071,
    0x0072,
    0x0073,
    0x0074,
    0x0075,
    0x0076,
    0x0077,
    0x0078,
    0x0079,
    0x007a,
    0x007b,
    0x007c,
    0x007d,
    0x007e,
    0x007f,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0x0900,/*0xa0*/
    0x0901,/*0xa1*/
    0x0902,/*0xa2*/
    0x0903,/*0xa3*/
    0x0905,/*0xa4*/
    0x0906,/*0xa5*/
    0x0907,/*0xa6*/
    0x0908,/*0xa7*/
    0x0909,/*0xa8*/
    0x090a,/*0xa9*/
    0x090b,/*0xaa*/
    0x090e,/*0xab*/
    0x090f,/*0xac*/
    0x0910,/*0xad*/
    0x090d,/*0xae*/
    0x0912,/*0xaf*/
    0x0913,/*0xb0*/
    0x0914,/*0xb1*/
    0x0911,/*0xb2*/
    0x0915,/*0xb3*/
    0x0916,/*0xb4*/
    0x0917,/*0xb5*/
    0x0918,/*0xb6*/
    0x0919,/*0xb7*/
    0x091a,/*0xb8*/
    0x091b,/*0xb9*/
    0x091c,/*0xba*/
    0x091d,/*0xbb*/
    0x091e,/*0xbc*/
    0x091f,/*0xbd*/
    0x0920,/*0xbe*/
    0x0921,/*0xbf*/
    0x0922,/*0xc0*/
    0x0923,/*0xc1*/
    0x0924,/*0xc2*/
    0x0925,/*0xc3*/
    0x0926,/*0xc4*/
    0x0927,/*0xc5*/
    0x0928,/*0xc6*/
    0x0929,/*0xc7*/
    0x092a,/*0xc8*/
    0x092b,/*0xc9*/
    0x092c,/*0xca*/
    0x092d,/*0xcb*/
    0x092e,/*0xcc*/
    0x092f,/*0xcd*/
    0x095f,/*0xce*/
    0x0930,/*0xcf*/
    0x0931,/*0xd0*/
    0x0932,/*0xd1*/
    0x0933,/*0xd2*/
    0x0934,/*0xd3*/
    0x0935,/*0xd4*/
    0x0936,/*0xd5*/
    0x0937,/*0xd6*/
    0x0938,/*0xd7*/
    0x0939,/*0xd8*/
    0x200D,/*0xd9*/
    0x093e,/*0xda*/
    0x093f,/*0xdb*/
    0x0940,/*0xdc*/
    0x0941,/*0xdd*/
    0x0942,/*0xde*/
    0x0943,/*0xdf*/
    0x0946,/*0xe0*/
    0x0947,/*0xe1*/
    0x0948,/*0xe2*/
    0x0945,/*0xe3*/
    0x094a,/*0xe4*/
    0x094b,/*0xe5*/
    0x094c,/*0xe6*/
    0x0949,/*0xe7*/
    0x094d,/*0xe8*/
    0x093c,/*0xe9*/
    0x0964,/*0xea*/
    0xFFFF,/*0xeb*/
    0xFFFF,/*0xec*/
    0xFFFF,/*0xed*/
    0xFFFF,/*0xee*/
    0xFFFF,/*0xef*/
    0xFFFF,/*0xf0*/
    0x0966,/*0xf1*/
    0x0967,/*0xf2*/
    0x0968,/*0xf3*/
    0x0969,/*0xf4*/
    0x096a,/*0xf5*/
    0x096b,/*0xf6*/
    0x096c,/*0xf7*/
    0x096d,/*0xf8*/
    0x096e,/*0xf9*/
    0x096f,/*0xfa*/
    0xFFFF,/*0xfb*/
    0xFFFF,/*0xfc*/
    0xFFFF,/*0xfd*/
    0xFFFF,/*0xfe*/
    0xFFFF,/*0xff*/
};

uint16_t nuktaSpecialCases[][2]={
    { 14 /*length of array*/   , 0      },
    { 0xA6 , 0x090c },
    { 0xA1 , 0x0950 },
    { 0xb3 , 0x0958 },
    { 0xb4 , 0x0959 },
    { 0xb5 , 0x095a },
    { 0xba , 0x095b },
    { 0xbf , 0x095c },
    { 0xC0 , 0x095d },
    { 0xc9 , 0x095e },
    { 0xAA , 0x0960 },
    { 0xA7 , 0x0961 },
    { 0x1D , 0x0962 },
    { 0xDC , 0x0963 }, 
};

/* Rules:
 *    Explicit Halant :  
 *                      <HALANT> + <ZWNJ>
 *    Soft Halant :
 *                      <HALANT> + <ZWJ>
 */
#define U_WRITE_TO_TARGET(args,offsets,source,target,targetLimit,targetByteUnit,err){       \
      /* write the targetUniChar  to target */                                              \
    if(target <targetLimit){                                                                \
        if(targetByteUnit & 0xFF00){                                                        \
            *(target)++ = (uint8_t)(targetByteUnit>>8);                                     \
            if(offsets){                                                                    \
                *(offsets++) = source - args->source-1;                                     \
            }                                                                               \
        }                                                                                   \
        if(target < targetLimit){                                                           \
            *(target)++ = (uint8_t)  targetByteUnit;                                        \
            if(offsets){                                                                    \
                *(offsets++) = source - args->source-1;                                     \
            }                                                                               \
        }else{                                                                              \
            args->converter->charErrorBuffer[args->converter->charErrorBufferLength++] =    \
                        (uint8_t) (targetByteUnit);                                         \
            *err = U_BUFFER_OVERFLOW_ERROR;                                                 \
        }                                                                                   \
    }else{                                                                                  \
        if(targetByteUnit & 0xFF00){                                                        \
            args->converter->charErrorBuffer[args->converter->charErrorBufferLength++] =    \
                        (uint8_t) (targetByteUnit >>8);                                     \
        }                                                                                   \
        args->converter->charErrorBuffer[args->converter->charErrorBufferLength++] =        \
                        (uint8_t) (targetByteUnit);                                         \
        *err = U_BUFFER_OVERFLOW_ERROR;                                                     \
    }                                                                                       \
 }          

U_CFUNC void 
UConverter_fromUnicode_ISCII_OFFSETS_LOGIC (UConverterFromUnicodeArgs * args,
                                                      UErrorCode * err){
    const UChar *source = args->source;
    const UChar *sourceLimit = args->sourceLimit;
    unsigned char *target = (unsigned char *) args->target;
    unsigned char *targetLimit = (unsigned char *) args->targetLimit;
    int32_t* offsets = args->offsets;
    uint32_t targetByteUnit = 0x0000;
    UChar32 sourceChar = 0x0000;
    UConverterCallbackReason reason;
    UBool useFallback;
    UConverterDataISCII *converterData;
    int range = 0;
    int newDelta=0;
    UBool deltaChanged = FALSE;
    /*Arguments Check*/
    if (U_FAILURE(*err)){ 
        return;
    }
    if ((args->converter == NULL) || (args->targetLimit < args->target) || (args->sourceLimit < args->source)){
        *err = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    converterData=(UConverterDataISCII*)args->converter->extraInfo;
    useFallback = args->converter->useFallback;

    if(args->converter->fromUSurrogateLead!=0 && target <targetLimit) {
        goto getTrail;
    }

    /*writing the char to the output stream */
    while(source < sourceLimit){    

        targetByteUnit = missingCharMarker;

        sourceChar = *source++;

        /*check if input is in ASCII RANGE*/
        if (sourceChar <= 0x007f) {
            U_WRITE_TO_TARGET(args,offsets,source,target,targetLimit,sourceChar,err);
            if(U_FAILURE(*err)){
                break;
            }
            if(sourceChar == LF){                         
                targetByteUnit = ATR<<8;
                targetByteUnit += (uint8_t) lookupInitialData[range][2];
                args->converter->toUnicodeStatus=sourceChar;
                /* now append ATR and language code */
                U_WRITE_TO_TARGET(args,offsets,source,target,targetLimit,targetByteUnit,err);
                if(U_FAILURE(*err)){
                    break;
                }
            }
            continue;
        }
        switch(sourceChar){
        case ZWNJ:
            if(converterData->contextCharFromUnicode){
                converterData->contextCharFromUnicode = 0x00;
                targetByteUnit = ISCII_HALANT;
            }else{
                converterData->contextCharFromUnicode = 0x00;
                continue;
            }
            break;
        case ZWJ:
            if(converterData->contextCharFromUnicode){
                targetByteUnit = ISCII_NUKTA;               
            }else{
                targetByteUnit =ISCII_INV;
            }
            converterData->contextCharFromUnicode = 0x00;
            break;
       default:
            /* is the sourceChar in the INDIC_RANGE? */
            if(INDIC_BLOCK_END-sourceChar <= INDIC_RANGE){
                /* find out to which block the souceChar belongs*/ 
                range = (sourceChar-INDIC_BLOCK_BEGIN)/DELTA;
                newDelta =range*DELTA;

                /* Now are we in the same block as the previous? */
                if(newDelta!= converterData->currentDeltaFromUnicode || converterData->isFirstBuffer){
                    converterData->currentDeltaFromUnicode = newDelta;
                    converterData->currentMaskFromUnicode = lookupInitialData[range][1];
                    deltaChanged =TRUE;
                    converterData->isFirstBuffer=FALSE;
                }
                /* now subtract the new delta from sourceChar*/
                if(sourceChar!= DANDA && sourceChar != DOUBLE_DANDA){
                    sourceChar -= converterData->currentDeltaFromUnicode ;
                }
                /* get the target byte unit */                  
                targetByteUnit=fromUnicodeTable[(uint8_t)sourceChar];
                            
                /* is the code point valid in current script? */
                /*valid = validityTable[(uint8_t)sourceChar] & converterData->maskFromUnicode;*/
                if((validityTable[(uint8_t)sourceChar] & converterData->currentMaskFromUnicode)==0){
                    targetByteUnit = missingCharMarker;
                }

                /* Vocallic RR is not available in ISCII Kannada */
                if(converterData->currentDeltaFromUnicode==(KANNADA_DELTA) && sourceChar==VOCALLIC_RR){
                    targetByteUnit=missingCharMarker;
                }
            
                if(deltaChanged){
                    /* we are in a script block which is different than 
                     * previous sourceChar's script block write ATR and language codes 
                     */
                    uint16_t temp=0;              
                    temp = ATR<<8;
                    temp += (uint8_t) lookupInitialData[range][2];
                    /* reset */
                    deltaChanged=FALSE;
                    /* now append ATR and language code */
                    U_WRITE_TO_TARGET(args,offsets,source,target,targetLimit,temp,err);
                    if(U_FAILURE(*err)){
                        break;
                    }
                }
            }

            break;
        }


        if(targetByteUnit != missingCharMarker){
            if(targetByteUnit==ISCII_HALANT){
                converterData->contextCharFromUnicode = (UChar)targetByteUnit;
            }
             /* write targetByteUnit to target*/
             U_WRITE_TO_TARGET(args,offsets,source,target,targetLimit,targetByteUnit,err);
             if(U_FAILURE(*err)){
                  break;
             }
        }
        else{
            /* oops.. the code point is unassingned
             * set the error and reason
             */
            reason =UCNV_UNASSIGNED;
            *err =U_INVALID_CHAR_FOUND;

            /*check if the char is a First surrogate*/
            if(UTF_IS_SURROGATE(sourceChar)) {
                if(UTF_IS_SURROGATE_FIRST(sourceChar)) {
                    args->converter->fromUSurrogateLead=(UChar)sourceChar;
getTrail:
                    /*look ahead to find the trail surrogate*/
                    if(source <  sourceLimit) {
                        /* test the following code unit */
                        UChar trail= (*source);
                        if(UTF_IS_SECOND_SURROGATE(trail)) {
                            source++;
                            sourceChar=UTF16_GET_PAIR_VALUE(args->converter->fromUSurrogateLead, trail);
                            args->converter->fromUSurrogateLead=0x00;
                            reason =UCNV_UNASSIGNED;
                            /* convert this surrogate code point */
                            /* exit this condition tree */
                        } else {
                            /* this is an unmatched lead code unit (1st surrogate) */
                            /* callback(illegal) */
                            sourceChar =  args->converter->fromUSurrogateLead;
                            reason=UCNV_ILLEGAL;
                            *err=U_ILLEGAL_CHAR_FOUND;
                        }
                    } else {
                        /* no more input */
                        *err = U_ZERO_ERROR;
                        break;
                    }
                } else {
                    /* this is an unmatched trail code unit (2nd surrogate) */
                    /* callback(illegal) */
                    reason=UCNV_ILLEGAL;
                    *err=U_ILLEGAL_CHAR_FOUND;
                }
            }
            {
                /*variables for callback */
                const UChar* saveSource =NULL;
                char* saveTarget =NULL;
                int32_t* saveOffsets =NULL;
                int currentOffset =0;
                int saveIndex =0;
                if(sourceChar>0xffff){
                    args->converter->invalidUCharBuffer[args->converter->invalidUCharLength++] =(uint16_t)(((sourceChar)>>10)+0xd7c0);
                    args->converter->invalidUCharBuffer[args->converter->invalidUCharLength++] =(uint16_t)(((sourceChar)&0x3ff)|0xdc00);
                }
                else{
                    args->converter->invalidUCharBuffer[args->converter->invalidUCharLength++] =(UChar)sourceChar;
                }
                if(offsets)
                    currentOffset = *(offsets-1)+1;

                saveSource = args->source;
                saveTarget = args->target;
                saveOffsets = args->offsets;
                args->target = (char*)target;
                args->source = source;
                args->offsets = offsets;

                /*copies current values for the ErrorFunctor to update */
                /*Calls the ErrorFunctor */
                args->converter->fromUCharErrorBehaviour ( args->converter->fromUContext, 
                              args, 
                              args->converter->invalidUCharBuffer, 
                              args->converter->invalidUCharLength, 
                             (UChar32) (sourceChar), 
                              reason, 
                              err);

                saveIndex = args->target - (char*)target;
                if(args->offsets){
                    args->offsets = saveOffsets;
                    while(saveIndex-->0){
                         *offsets = currentOffset;
                          offsets++;
                    }
                }
                target = (unsigned char*)args->target;
                args->source=saveSource;
                args->target=saveTarget;
                args->offsets=saveOffsets;
                args->converter->invalidUCharLength = 0;
                args->converter->fromUSurrogateLead=0x00;

                if (U_FAILURE (*err)){
                    break;
                }
            }
        }


    }/* end while(mySourceIndex<mySourceLength) */


    /*If at the end of conversion we are still carrying state information
     *flush is TRUE, we can deduce that the input stream is truncated
     */
    if (args->converter->fromUSurrogateLead !=0 && (source == sourceLimit) && args->flush){
        *err = U_TRUNCATED_CHAR_FOUND;
    }
    /* Reset the state of converter if we consumed 
     * the source and flush is true
     */
    if( (source == sourceLimit) && args->flush){
       /*reset converter*/
        _ISCIIReset(args->converter,UCNV_RESET_FROM_UNICODE);
    }

    /*save the state and return */
    args->source = source;
    args->target = (char*)target;
}

int32_t lookupTable[][2]={
    ZERO,ZERO,      /*DEFALT*/
    ZERO, ZERO,     /*ROMAN*/
    DEVANAGARI, DEV_MASK,
    BENGALI, BNG_MASK,
    TAMIL, TML_MASK,
    TELUGU, TLG_MASK,
    BENGALI, BNG_MASK,
    ORIYA, ORI_MASK,
    KANNADA, TLG_MASK,
    GUJARATI, GJR,
    GURMUKHI, PNJ,
};
#define WRITE_TO_TARGET_ToU(args,source,target,offsets,offset,targetUniChar,err){       \
    /* now write the targetUniChar */                                                   \
    if(target<args->targetLimit){                                                       \
        *(target)++ = targetUniChar;                                                    \
        if(offsets){                                                                    \
            *(offsets)++ = offset;                                                      \
        }                                                                               \
    }else{                                                                              \
        args->converter->UCharErrorBuffer[args->converter->UCharErrorBufferLength++] =  \
            data->contextCharToUnicode;                                                 \
        *err = U_BUFFER_OVERFLOW_ERROR;                                                 \
    }                                                                                   \
}

#define NO_CHAR_MARKER 0xFFFE

#define GET_MAPPING(sourceChar,targetUniChar,data){                                     \
    int valid=0;                                                                        \
    targetUniChar = toUnicodeTable[(sourceChar)] ;                                      \
    /* is the code point valid in current script? */                                    \
    valid = validityTable[(uint8_t)targetUniChar] & data->currentMaskToUnicode;         \
    if(valid==0){                                                                       \
        targetUniChar= missingCharMarker;                                               \
    }                                                                                   \
}

U_CFUNC void 
UConverter_toUnicode_ISCII_OFFSETS_LOGIC(UConverterToUnicodeArgs *args,
                                                            UErrorCode* err){
    const char *source = ( char *) args->source;
    UChar *target = args->target;
    const char *sourceLimit = args->sourceLimit;
    uint32_t targetUniChar = 0x0000;
    uint32_t sourceChar = 0x0000;
    UConverterDataISCII* data;
    int32_t* offsets = args->offsets;
    /*Arguments Check*/
    if (U_FAILURE(*err)){ 
        return;
    }

    if ((args->converter == NULL) || (target < args->target) || (source < args->source)){
        *err = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    
    data = (UConverterDataISCII*)(args->converter->extraInfo);

    while(source< args->sourceLimit){

        targetUniChar = missingCharMarker;
        
        if(target < args->targetLimit){
            sourceChar = (unsigned char)*(source)++;

            if(sourceChar!=ATR && data->contextCharToUnicode  == 0x0000){
                /* get the mapping */
                GET_MAPPING(sourceChar, targetUniChar,data);
                if(targetUniChar==missingCharMarker){
                    *err = U_INVALID_CHAR_FOUND;
                    goto CALLBACK;
                }else{
                    /* save the mapping in contextCharToUnicode */
                    data->contextCharToUnicode = (UChar)targetUniChar;
                }
                continue;               
            }
            /* If we have ATR in contextCharToUnicode then we need to change our
             * state to the Indic Script specified by sourceChar
             */
            if(data->contextCharToUnicode==ATR){
                if(PNJ-sourceChar<=PNJ-DEV){
                    data->currentDeltaToUnicode = lookupTable[sourceChar & 0x0F][0] * DELTA;
                    data->currentMaskToUnicode = lookupTable[sourceChar & 0x0F][1] ;
                }
                else if( sourceChar==RMN || sourceChar==DEF){
                    /* switch back to default */
                    data->currentDeltaToUnicode = data->defDeltaToUnicode;
                    data->currentMaskToUnicode = data->defMaskToUnicode;
                }else{
                    /* these are display codes consume and continue */
                }
                /* reset */
                data->contextCharToUnicode=0x0000;              
                continue;
            }

            switch(sourceChar){
            case ATR:
                /* if there is a mapping in contextCharToUnicode write it
                 * to target 
                 */
                if(data->contextCharToUnicode !=0x000){
                    /* add the delta of Indic blocks */
                    if(data->contextCharToUnicode>0x7f &&
                       data->contextCharToUnicode != ZWNJ &&
                       data->contextCharToUnicode != ZWNJ &&
                       data->contextCharToUnicode != DANDA &&
                       data->contextCharToUnicode != DOUBLE_DANDA
                       ){ 
                        
                        data->contextCharToUnicode+= (UChar)data->currentDeltaToUnicode; 
                    }
                    WRITE_TO_TARGET_ToU(args,source,target,args->offsets,(source-args->source -2),
                                data->contextCharToUnicode,err);

                }
                data->contextCharToUnicode = (UChar)sourceChar;
                continue;
            case ISCII_DANDA:
                if(data->contextCharToUnicode== DANDA){
                    targetUniChar = DOUBLE_DANDA;
                    data->contextCharToUnicode= NO_CHAR_MARKER;
                }else{
                    GET_MAPPING(sourceChar,targetUniChar,data);
                }
                break;
            case ISCII_HALANT:
                /* handle explicit halant */
                if(data->contextCharToUnicode == HALANT){
                    targetUniChar = ZWNJ;
                }else{
                    GET_MAPPING(sourceChar,targetUniChar,data);
                }
                
                break;
            case ISCII_NUKTA:
                /* handle soft halant */
                if(data->contextCharToUnicode == HALANT){
                    GET_MAPPING(data->contextCharToUnicode,targetUniChar,data);
                    targetUniChar = ZWJ;
                    break;
                }else{
                    /* try to handle <CHAR> + ISCII_NUKTA special mappings */
                    int i=1;
                    /* we have stored Unicode codepoint in contextCharToUnicode
                     * we need to get ISCII code to map byte unit back to Unicode
                     */
                    UChar temp = fromUnicodeTable[(uint8_t)data->contextCharToUnicode];
                    UBool found =FALSE;
                    for( ;i<nuktaSpecialCases[0][0];i++){
                        if(nuktaSpecialCases[i][0]==temp){
                            targetUniChar=nuktaSpecialCases[i][1];
                            found =TRUE;
                            break;
                        }
                    }
                    if(found){
                        /* find out if the mapping is valid in this state */
                        int valid = validityTable[(uint8_t)targetUniChar] & data->currentMaskToUnicode;                                            
                        if(valid){       
                            targetUniChar += data->currentDeltaToUnicode ;
                            data->contextCharToUnicode= NO_CHAR_MARKER;
                            break;
                        }
                        /* else fall through to default */
                    }
                    /* else fall through to default */
                }                
            default:
                GET_MAPPING(sourceChar,targetUniChar,data);
                break;
            }
            
            /* here contextCharToUnicode is never missingCharMarker 
             * since that case has been handled at the begining.
             * so write the char to target
             */
            if(data->contextCharToUnicode !=NO_CHAR_MARKER ){
                /* add offset to current Indic Block */
                if(data->contextCharToUnicode>0x7f &&
                       data->contextCharToUnicode != ZWNJ &&
                       data->contextCharToUnicode != ZWNJ &&
                       data->contextCharToUnicode != DANDA &&
                       data->contextCharToUnicode != DOUBLE_DANDA){ 
                        
                        data->contextCharToUnicode+=(UChar)data->currentDeltaToUnicode; 
                }
                WRITE_TO_TARGET_ToU(args,source,target,args->offsets,(source-args->source -2),
                                data->contextCharToUnicode,err);
            }
            /* reset contextCharToUnicode */
            data->contextCharToUnicode = 0x0000;
            if(targetUniChar != missingCharMarker ){
                /* now write the targetUniChar */
                data->contextCharToUnicode = (UChar) targetUniChar;
            }else{  
CALLBACK:
                 {
                    const char *saveSource = args->source;
                    UChar *saveTarget = args->target;
                    int32_t *saveOffsets = NULL;
                    UConverterCallbackReason reason;
                    int32_t currentOffset;
                    int32_t saveIndex = target - args->target;

                    currentOffset= source - args->source -1;
                    if(data->contextCharToUnicode != 0x0000){
                        args->converter->invalidCharBuffer[args->converter->invalidCharLength++] = 
                            (char) data->contextCharToUnicode;
                    }
                    args->converter->invalidCharBuffer[args->converter->invalidCharLength++] =(char) sourceChar;
                
                    if(targetUniChar == missingCharMarker){
                        reason = UCNV_UNASSIGNED;
                        *err = U_INVALID_CHAR_FOUND;
                    }

                    if(args->offsets){
                        saveOffsets=args->offsets;
                        args->offsets = args->offsets+(target - args->target);
                    }

                    args->target =target;
                    target =saveTarget;
                    args->source = source;

                    args->converter->fromCharErrorBehaviour ( 
                         args->converter->toUContext, 
                         args, 
                         args->converter->invalidCharBuffer, 
                         args->converter->invalidCharLength, 
                         reason, 
                         err);

                    if(args->offsets){
                        args->offsets = saveOffsets;

                        for (;saveIndex < (args->target - target);saveIndex++) {
                          *(args->offsets)++ = currentOffset;
                        }
                    }
                    args->converter->invalidCharLength=0;
                    target=args->target;
                    args->source  = saveSource;
                    args->target  = saveTarget;
                }
            }

        }
        else{
            *err =U_BUFFER_OVERFLOW_ERROR;
            break;
        }
    }
    if((args->flush==TRUE)
        && (source == sourceLimit) 
        && data->contextCharToUnicode !=0){
        /* add offset to current Indic block */
        if(data->contextCharToUnicode>0x7f &&
           data->contextCharToUnicode != ZWNJ &&
           data->contextCharToUnicode != ZWNJ &&
           data->contextCharToUnicode != DANDA &&
           data->contextCharToUnicode != DOUBLE_DANDA){ 
            
            data->contextCharToUnicode+= (UChar) data->currentDeltaToUnicode; 
        }
        WRITE_TO_TARGET_ToU(args,source,target,args->offsets,(source-args->source -1),
                                data->contextCharToUnicode,err);


    }
    /* Reset the state of converter if we consumed 
     * the source and flush is true
     */
    if( (source == sourceLimit) && args->flush){
        /*reset converter*/
        _ISCIIReset(args->converter,UCNV_RESET_TO_UNICODE);
    }
    args->target = target;
    args->source = source;
}

/* structure for SafeClone calculations */
struct cloneStruct
{
    UConverter cnv;
    UConverterDataISCII mydata;
};


U_CFUNC UConverter * 
_ISCII_SafeClone(const UConverter *cnv, 
              void *stackBuffer, 
              int32_t *pBufferSize, 
              UErrorCode *status)
{
    struct cloneStruct * localClone;
    int32_t bufferSizeNeeded = sizeof(struct cloneStruct);

    if (U_FAILURE(*status)){
        return 0;
    }

    if (*pBufferSize == 0){ /* 'preflighting' request - set needed size into *pBufferSize */
        *pBufferSize = bufferSizeNeeded;
        return 0;
    }

    localClone = (struct cloneStruct *)stackBuffer;
    memcpy(&localClone->cnv, cnv, sizeof(UConverter));
    localClone->cnv.isCopyLocal = TRUE;

    memcpy(&localClone->mydata, cnv->extraInfo, sizeof(UConverterDataISCII));
    localClone->cnv.extraInfo = &localClone->mydata;

    return &localClone->cnv;
}