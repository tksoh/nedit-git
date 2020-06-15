static const char CVSID[] = "$Id: patternMatchData.c,v 1.4 2004/10/27 21:57:12 uleh Exp $";
/*******************************************************************************
*                                                                              *
* patternMatchData.c -- Maintain and allow user to edit a matching pattern list*
*                       used for pattern matching                              *
*                                                                              *
* Copyright (C) 2003-2004, Uwe Lehnert                                         *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* October 27, 2004                                                             *
*                                                                              *
* Written by Uwe Lehnert                                                       *
*                                                                              *
*******************************************************************************/
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>

#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Text.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/SeparatoG.h>

#include "../util/misc.h"
#include "../util/DialogF.h"
#include "../util/managedList.h"

#include "regularExp.h"
#include "textBuf.h"
#include "nedit.h"
#include "window.h"
#include "preferences.h"
#include "help.h"
#include "file.h"
#include "textP.h"

#include "patternMatch.h"
#include "patternMatchData.h"

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

/*
 * local synonyms
 */
#define MAX_LOCAL_BACK_REF_ID  9
#define LOCAL_BACK_REF_ID_USED -2

#define MAX_NUMBER_MIDDLE_PATTERN       10
#define MAX_STRING_PATTERNS             30
#define MAX_NBR_MATCH_PATTERNS          50
#define MAX_NBR_MATCH_PATTERN_GROUPS    50
#define MAX_NBR_SEQ_ELEMENTS            MAX_NBR_MATCH_PATTERNS + MAX_NBR_MATCH_PATTERN_GROUPS

#define MATCH_PAT_NAME_LBL_TXT  "Matching Pattern Name"
#define STRING_PATTERNS_LBL_TXT "String Patterns"

#define BORDER 4
#define LIST_RIGHT 41
#define PLAIN_LM_STRING "PLAIN"

#define SPNM_NONE_SELECTED "none selected"

#define KEEP_LANGUAGE_MODE     True
#define DISCARD_LANGUAGE_MODE  False

#define STRING_PATTERN_DIALOG  True
#define CONTEXT_GROUP_DIALOG   False

/*
 * local data definitions
 */
typedef struct _ErrorInfo {
  char *eiDetail;
  char *eiLanguageMode;
  char *eiMPTabElementName;
  char *eiStringPatText;
  char *eiRegExpCompileMsg;
  int   eiBackRefNbr;
} ErrorInfo;

typedef struct _BackRefBracketInfo {
  int   brbiGlobalId;
  char *brbiContentStart;
  int   brbiNestingLevel;
} BackRefBracketInfo;

typedef struct _RegExpStringInfo {
  int                resiNbrOfAddedMultiPat;
  PatternReference **resiAddedMultiPat;
  char              *resiNoneWBRegExpString;
  char              *resiLeftWBRegExpString;
  char              *resiRightWBRegExpString;
  char              *resiBothWBRegExpString;
  int                resiLocalBackRefID;
} RegExpStringInfo;

typedef struct _ReadMatchPatternInfo {
  int                          rmpiNbrOfElements;
  MatchPatternTableElement    *rmpiElement[MAX_NBR_MATCH_PATTERNS];
  int                          rmpiNbrOfGroups;
  MatchPatternGroupElement    *rmpiGroup[MAX_NBR_MATCH_PATTERN_GROUPS];
  int                          rmpiNbrOfSeqElements;
  MatchPatternSequenceElement *rmpiSequence[MAX_NBR_SEQ_ELEMENTS];
  regexp                      *rmpiAllPatRE;
  regexp                      *rmpiFlashPatRE;
} ReadMatchPatternInfo;

typedef struct _DialogStringPatternElement {
  char                *dspeText;
  PatternElementKind   dspeKind;
  PatternWordBoundary  dspeWordBoundary;
  int                  dspeCaseInsensitive;
  int                  dspeRegularExpression;
} DialogStringPatternElement;

typedef struct _DialogStringPatterns {
  int                          dspNumberOfPatterns;
  DialogStringPatternElement  *dspElements[MAX_STRING_PATTERNS];
} DialogStringPatterns;

typedef struct _DialogMatchPatternTableElement {
  char                 *dmpteName;
  MatchPatternType      dmpteType;
  DialogStringPatterns  dmptePatterns;
  int                   dmpteSkipBtwnStartEnd;
  int                   dmpteIgnoreHighLightInfo;
  int                   dmpteFlash;
} DialogMatchPatternTableElement;

typedef struct _DialogMatchPatternGroupElement {
  char *dmpgeName;
  int   dmpgeNumberOfSubPatterns;
  char *dmpgeSubPatternIds[MAX_NBR_MATCH_PATTERNS];
} DialogMatchPatternGroupElement;

typedef struct _DialogMatchPatternSequenceElement {
  char             *dmpseName;
  MatchPatternType  dmpseType;
  int               dmpseValid;
  void             *dmpsePtr;
} DialogMatchPatternSequenceElement;

typedef struct _DialogMatchPatternInfo {
  int                                dmpiNbrOfSeqElements;
  DialogMatchPatternSequenceElement *dmpiSequence[MAX_NBR_SEQ_ELEMENTS];
} DialogMatchPatternInfo;

typedef enum {
  DMPTR_OK,
  DMPTR_EMPTY,
  DMPTR_INCOMPLETE
} DMPTranslationResult;

typedef struct _NameList {
  int   nlNumber;
  char *nlId[MAX_NBR_SEQ_ELEMENTS + 1];
} NameList;

/*
 * prototypes of local functions
 */
static void treatDuplicatedPattern(
  MatchPatternTable *table,
  PatternElement    *prevPattern,
  PatternReference   prevPatRef,
  PatternElement    *lastPattern,
  PatternReference   lastPatRef);
static void treatDuplicatedPatternElements(
  MatchPatternTable        *table,
  MatchPatternTableElement *prevElement,
  int                       prevElementIdx,
  PatternElement           *lastPattern,
  PatternReference          lastPatRef);
static void treatDuplicatedMTElements(
  MatchPatternTable        *table,
  MatchPatternTableElement *prevElement,
  int                       prevElementIdx,
  MatchPatternTableElement *lastElement,
  int                       lastElementIdx);
static void treatDuplicatedMTEntries(
  MatchPatternTableElement **element,
  int                        nbrOfElements);

static int createStrPatRegExpOfElement(
  ReadMatchPatternInfo     *readPatInfo,
  MatchPatternTableElement *element,
  ErrorInfo                *errInfo);
static int createStrPatRegExp(
  StringPattern *strPat,
  ErrorInfo     *errInfo);
static void adaptCompileMsg(
  char *compileMsg,
  int  *globalToLocalBackRef);
static int localToGlobalBackRef(
  int  *globalToLocalBackRef,
  int  localId);
static int createStartEndRegExp(
  ReadMatchPatternInfo     *readMatchPatternInfo,
  MatchPatternTableElement *element,
  ErrorInfo                *errInfo);
static int createGroupRegExp(
  ReadMatchPatternInfo     *readPatInfo,
  MatchPatternGroupElement *group,
  ErrorInfo                *errInfo);
static void setupRegExpStringBuffers(
  ReadMatchPatternInfo  *readMatchPatternInfo,
  RegExpStringInfo      *regExpStringInfo);
static void addSMTRegExpString(
  char       *result,
  char       *partToAdd,
  const char *postfix);
static void composeStartEndRegExpString(
  ReadMatchPatternInfo      *readMatchPatternInfo,
  MatchPatternTableElement  *element,
  char                     **regExpString);
static void addElementToRegExpString(
  MatchPatternTableElement *element,
  ReadMatchPatternInfo     *readMatchPatternInfo,
  RegExpStringInfo         *regExpStringInfo);
static void addUniquePatternToRegExpString(
  PatternElement           *patElement,
  PatternReference         *patElementReference,
  ReadMatchPatternInfo     *readMatchPatternInfo,
  RegExpStringInfo         *regExpStringInfo);
static void addPatternToRegExpString(
  StringPattern    *pattern,
  RegExpStringInfo *regExpStringInfo);
static char *adaptLocalBackRefs(
  char *regExpText,
  int  *commonLocalId);
static void scanForLocalBackRefs(
  char *regExpText,
  int  *localBackRefList);
static int isMultiPatternNotAdded(
  RegExpStringInfo         *regExpStringInfo,
  PatternReference         *toBeAddedPR);
static void catSMTRegExpStrings(
  RegExpStringInfo  *regExpStringInfo,
  char             **regExpString);
static void freeRegExpStringInfo(
  RegExpStringInfo *regExpStringInfo);
static int totalKeywordOfTableLen(
  ReadMatchPatternInfo *info,
  int                  *nbrOfMultiPatterns);
static int totalMatchPatternTableElementLen(
  ReadMatchPatternInfo     *info,
  MatchPatternTableElement *element,
  int                      *nbrOfMultiPatterns);
static int patternElementLen(
  ReadMatchPatternInfo *info,
  PatternElement       *patElement,
  int                  *nbrOfMultiPatterns);

static void parseMatchingPatternSetError(
  const char *stringStart,
  const char *stoppedAt,
  ErrorInfo  *errInfo);
static void dialogMatchingPatternSetError(
  char      *title,
  ErrorInfo *errInfo);

static char *createMatchPatternsString(
  StringMatchTable *table,
  char             *indentStr);
static char *createPatternElementString(
  MatchPatternTable *table,
  PatternElement    *pat);

static StringMatchTable *readDefaultStringMatchTable(const char *langModeName);
static int isDefaultMatchPatternTable(StringMatchTable *table);

static void freeReadMatchPatternInfo( ReadMatchPatternInfo *readPatInfo );
static void freeStringMatchTable( StringMatchTable *table );
static void freeMatchPatternTableElement( MatchPatternTableElement *element );
static void freePatternElement( PatternElement *element );
static void freeStringPattern( StringPattern *strPat );
static void freeMatchPatternGroupElement( MatchPatternGroupElement *group );
static void freeMatchPatternSequenceElement( MatchPatternSequenceElement *sequence );

static StringMatchTable *readMatchPatternSet(char **inPtr);
static StringMatchTable *readMatchPatternSetContent(
  char **inPtr,
  char  *stringStart,
  char  *languageMode);
static int createRegExpOfPatterns(
  ReadMatchPatternInfo *readPatInfo,
  ErrorInfo            *errInfo);
static int createRegExpOfAllPatterns(
  ReadMatchPatternInfo *readPatInfo,
  ErrorInfo            *errInfo);
static int createRegExpOfStrPatterns(
  ReadMatchPatternInfo *readPatInfo,
  ErrorInfo            *errInfo);
static StringMatchTable *createStringMatchTable(
  ReadMatchPatternInfo *readPatInfo,
  char                 *languageMode);
static int readMatchPatternEntry(
  char                 **inPtr,
  ErrorInfo             *errInfo,
  ReadMatchPatternInfo  *info);
static void recordPatternSequence(
  ReadMatchPatternInfo *info,
  char                 *name,
  MatchPatternType      type,
  int                   index);
static int assignIndividualGroup(
  ReadMatchPatternInfo      *info,
  char                     **errMsg,
  MatchPatternTableElement  *element);
static MatchPatternTableElement *getPatternOfName(
  ReadMatchPatternInfo *info,
  char                 *subPatToSearch);
static MatchPatternGroupElement *readMatchPatternGroup(
  char                 **inPtr,
  ErrorInfo             *errInfo,
  char                  *name,
  ReadMatchPatternInfo  *info);
static int readPatternElement(
  char           **inPtr,
  char           **errMsg,
  PatternElement **pattern);
static PatternElement *createPatternElement(
  char                *patternText,
  PatternElementKind   patternKind,
  PatternWordBoundary  wordBoundary,
  int                  caseInsensitive,
  int                  regularExpression);

static int createGlobalBackRefList(
  ReadMatchPatternInfo     *readPatInfo,
  MatchPatternTableElement *element,
  ErrorInfo                *errInfo);
StringPattern *getReadStringPattern(
  ReadMatchPatternInfo *readPatInfo,
  PatternElement       *pattern );
static PatternElement *getReadPatternOfReference(
  ReadMatchPatternInfo *readPatInfo,
  PatternReference     *patRef);
static char *replaceCapturingParentheses(
  const char *source);
static int parseGlobalBackRefs(
  StringPattern        *strPat,
  GlobalBackRefElement *backRefList,
  ErrorInfo            *errInfo);
static int updateGlobalBackRefs(
  StringPattern        *strPat,
  GlobalBackRefElement *backRefList,
  ErrorInfo            *errInfo);
static char *createBackRefRegExpText(
  const char *start,
  const char *end);
static int resolveGlobalBackRefs(
  ReadMatchPatternInfo     *readPatInfo,
  MatchPatternTableElement *element,
  ErrorInfo                *errInfo);
static int resolveGlobalBackRefsOfStrPat(
  StringPattern        *strPat,
  GlobalBackRefElement *backRefList,
  ErrorInfo            *errInfo);
static char *substituteGlobalBackRef(
  StringPattern        *strPat,
  char                 *subsPtr,
  int                   globalId,
  int                  *localId,
  GlobalBackRefElement *backRefList,
  ErrorInfo            *errInfo);
static char *replaceBackRefIdByRegExp(
  StringPattern  *strPat,
  char           *replaceStartPtr,
  char           *regExp);
static char *convertGlobalToLocalBackRef(
  StringPattern  *strPat,
  char           *convertPtr);

static MatchPatternTableElement *readMatchPatternTableElement(
  char             **inPtr,
  char             **errMsg,
  char              *name,
  MatchPatternType   type);
static int sortReadPatternElementSet(
  PatternElementSet         *allPat,
  char                     **errMsg,
  MatchPatternTableElement  *result);
static void countPatternElementKind(
  PatternElementSet         *allPat,
  MatchPatternTableElement  *result);
static void sortPatternElementSet(
  PatternElementSet         *allPat,
  MatchPatternTableElement  *result);
static void copyPatternSet(
  PatternElementSet *sourcePS,
  PatternElementSet *destPS);

static int getMPSName(
  char      **inPtr,
  ErrorInfo  *errInfo,
  char      **name );
static int getMPSTypeAttribute(
  char             **inPtr,
  ErrorInfo         *errInfo,
  MatchPatternType  *type);
static int getMPSGlobalAttribute(
  char **inPtr,
  char **errMsg,
  int   *isMonoPattern,
  int   *comment,
  int   *flash,
  int   *ignoreHighLightInfo);
static int getMPSPatternAttribute(
  char                **inPtr,
  char                **errMsg,
  PatternElementKind   *patternKind,
  PatternWordBoundary  *wordBoundary,
  int                  *caseInsensitive,
  int                  *regularExpression);

static void copyStringMatchTableForDialog(
  StringMatchTable       *sourceTable,
  DialogMatchPatternInfo *dialogTable );
static void *copyMatchPatternElementForDialog(
  MatchPatternTable *table,
  int                sourceElementIdx);
static void copyPatternForDialog(
  MatchPatternTable           *table,
  PatternElement              *sourcePattern,
  DialogStringPatternElement **dialogPattern );
static void *copyGroupElementForDialog(
  MatchPatternGroupElement *sourceGroup);
static void copySequenceElementForDialog(
  StringMatchTable                   *sourceTable,
  MatchPatternSequenceElement        *sourceSeqElement,
  DialogMatchPatternSequenceElement **dialogSeqElement );
static DialogMatchPatternSequenceElement *copyDialogSequenceElement(
  DialogMatchPatternSequenceElement *sourceSeq);
static void freeDialogMatchPatternElement(
  DialogMatchPatternTableElement *dialogElement );
static void freeDialogStringPatternElement(
  DialogStringPatternElement *element);
static void freeDialogGroupElement(
  DialogMatchPatternGroupElement *dialogGroup );
static void freeDialogSequenceElement(
  DialogMatchPatternSequenceElement *dialogSeq );

static void copyDialogStringPatternsFromTable(
  DialogMatchPatternTableElement  *tableElement,
  DialogStringPatterns            *destPatterns);
static void copyDialogStringPatterns(
  DialogStringPatterns *sourcePatterns,
  DialogStringPatterns *destPatterns);
static void freeDialogStringPatterns(
  DialogStringPatterns *patterns);

static DialogStringPatternElement *copyDialogStringPatternElement(
  DialogStringPatternElement  *sourceElement);

static void copyDialogPatternNamesFromGroup(
  DialogMatchPatternGroupElement  *group,
  DialogStringPatterns            *destPatterns);
static DialogStringPatternElement *copyDialogPatternName(
  char *sourcePatternId);
static void copyDialogPatternNamesToGroup(
  DialogStringPatterns           *sourceNames,
  DialogMatchPatternGroupElement *destGroup);

static void setDialogType(int dialogShowsStringPattern);
static void setSensitiveWordBoundaryBox(int enable);

static void *getStringPatternDisplayedCB(void *oldItem, int explicitRequest, int *abort,
        void *cbArg);
static void setStringPatternDisplayedCB(void *item, void *cbArg);
static void freeStringPatternItemCB(void *item);

static void *getMatchPatternDisplayedCB(void *oldItem, int explicitRequest, int *abort,
        void *cbArg);
static void setMatchPatternDisplayedCB(void *item, void *cbArg);
static void freeMatchPatternItemCB(void *item);
static int deleteMatchPatternItemCB(int itemIndex, void *cbArg);

static void matchPatternLangModeCB(Widget w, XtPointer clientData, XtPointer callData);
static void pmLanguageModeDialogCB(Widget w, XtPointer clientData, XtPointer callData);

static void destroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void okCB(Widget w, XtPointer clientData, XtPointer callData);
static void applyCB(Widget w, XtPointer clientData, XtPointer callData);
static void checkCB(Widget w, XtPointer clientData, XtPointer callData);
static void restoreCB(Widget w, XtPointer clientData, XtPointer callData);
static void deleteCB(Widget w, XtPointer clientData, XtPointer callData);
static void closeCB(Widget w, XtPointer clientData, XtPointer callData);
static void helpCB(Widget w, XtPointer clientData, XtPointer callData);

static void matchPatTypeCB(Widget w, XtPointer clientData, XtPointer callData);
static void strPatRegExpressionCB(Widget w, XtPointer clientData, XtPointer callData);
static void changeExistingSubPattern(char *warnTitle);
static void changeStringPatternToGroup(void);
static void changeGroupToStringPattern(char *warnTitle);

static Widget createSubPatternNameMenu(
  Widget  parent,
  char   *currentSubPatName,
  int     allSubPatterns);
static void setupSubPatternNameList(
  char     *currentSubPatName,
  int       allSubPatterns,
  NameList *nameList);
static void createSubPatNameMenuEntry(
  Widget  menu,
  char   *subPatName);
static void setSubPatternNameMenu(
  const char *subPatName);
static void updateSubPatternNameMenu(
  char *currentSubPatName,
  int   allSubPatterns);
static char *getSelectedSubPatternName(void);
static int isSubPatternNameInCurStrPat(
  char *subPatName);

static DialogMatchPatternSequenceElement *readMatchPatternFields(int silent);
static int isStartPatternElementAvailable(
  DialogStringPatterns *dialogPatterns);
static DialogStringPatternElement *readStringPatternFrameFields(int silent);

static int matchPatternDialogEmpty(void);
static int stringPatternFrameEmpty(void);
static int stringPatternFieldsEmpty(
    int strPatIsRelatedToGroup);

static int getAndUpdateStringMatchTable(void);
static void updateStringMatchTable(
  StringMatchTable *newTable);

static StringMatchTable *getDialogStringMatchTable(
  DMPTranslationResult *result);
static StringMatchTable *translateDialogStringMatchTable(
  DialogMatchPatternInfo *dialogTable,
  DMPTranslationResult   *result);
static MatchPatternTableElement *translateDialogMatchPatternTableElement(
  DialogMatchPatternTableElement *dialogElement);
static void translateDialogPatterns(
  DialogStringPatterns     *dialogPatterns,
  MatchPatternTableElement *newElement);
static MatchPatternGroupElement *translateDialogMatchPatternGroupElement(
  ReadMatchPatternInfo           *info,
  DialogMatchPatternGroupElement *dialogGroup);
static void sortDialogPatternElementSet(
  PatternElementSet         *allPat,
  MatchPatternTableElement  *result);

static int stringMatchTableDiffer(
  StringMatchTable *oldTable,
  StringMatchTable *newTable);

static int patternElementDiffer(
  PatternElement    *oldPE,
  MatchPatternTable *oldTab,
  PatternElement    *newPE,
  MatchPatternTable *newTab);

static DialogMatchPatternGroupElement *getDialogGroupUsingMatchPattern(
  char *matchPatternName);
static void removeMatchPatternFromGroup(
  char                           *matchPatternName,
  DialogMatchPatternGroupElement *group);
static void removeMatchPatternFromAllGroups(
  char *matchPatternName);
static void renameMatchPatternInGroup(
  char                           *oldMatchPatternName,
  char                           *newMatchPatternName,
  DialogMatchPatternGroupElement *group);
static void renameMatchPatternInAllGroups(
  char *oldMatchPatternName,
  char *newMatchPatternName);

static void freeVariableDialogData(
  int keepLanguageModeName);

static void initGlobalBackRefList(
  GlobalBackRefElement *list);
static void initStrPatBackRefList(
  StringPattern *strPat);

StringPattern *getUniqueStringPattern(
  PatternElement *pattern );

static void initErrorInfo(
  ErrorInfo *errInfo);
static void freeErrorInfo(
  ErrorInfo *errInfo);

static void freeXtPtr(void **ptr);
static void freePtr(void **ptr);

/*
 * matching pattern dialog information
 */
static struct {
    Widget                             mpdShell;
    Widget                             mpdLmOptMenu;
    Widget                             mpdLmPulldown;
    Widget                             mpdMatchPatternNamesListW;
    Widget                             mpdMatchPatternNameLbl;
    Widget                             mpdMatchPatternNameW;
    Widget                             mptbIndividualW;
    Widget                             mptbSubPatternW;
    Widget                             mptbContextGroupW;
    Widget                             mpdGlobalAttributesLbl;
    Widget                             gabSkipBtwStartEndW;
    Widget                             gabFlashW;
    Widget                             gabSyntaxBasedW;
    Widget                             mpdStringPatternsLbl;
    Widget                             mpdStringPatternsListW;
    Widget                             mpdStringPatternTypeLbl;
    Widget                             sptStartW;
    Widget                             sptMiddleW;
    Widget                             sptEndW;
    Widget                             mpdWordBoundaryLbl;
    Widget                             wbbBothW;
    Widget                             wbbLeftW;
    Widget                             wbbRightW;
    Widget                             wbbNoneW;
    Widget                             mpdStringAttributesLbl;
    Widget                             sabCaseSensitiveW;
    Widget                             sabRegularExpressionW;
    Widget                             mpdStringPatternLbl;
    Widget                             mpdStringPatternW;
    Widget                             mpdSubPatNamesLbl;
    Widget                             mpdSubPatNamesOptMenu;
    Widget                             mpdSubPatNamesPulldown;
    char                              *mpdLangModeName;
    DialogMatchPatternSequenceElement *currentDmptSeqElement;
    DialogMatchPatternTableElement    *currentDmptElement;
    DialogMatchPatternGroupElement    *currentDmptGroup;
    DialogStringPatterns               currentStringPatterns;
    DialogMatchPatternInfo             mpdTable;
    int                                mpdStringPatternIsDisplayed;
} MatchPatternDialog =
  {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   NULL, NULL, NULL, NULL,
   {0, {NULL}},
   {0, {NULL}},
   True
  };

/*
 * Matching pattern sources loaded from the nedit resources file or set
 * by the user
 */
static int NbrMatchTables = 0;
static StringMatchTable *MatchTables[MAX_LANGUAGE_MODES];

/*
 * Syntax:
 * LanguagePatternSet ::=
 *   LanguageName{PatternStatement..}
 *
 * PatternStatement ::=
 * name:[s]:[c][f][m][p][u]:([s|m|e][w|l|r][i]:"pattern":)..)\n)|
 * name:g:"sub-pattern name":..\n..
 *
 * TypeAttribute:
 *   s  : sub-pattern (pattern is only matched, if part of a pattern group).
 *   g  : pattern (context) group (i.e. a sequence of sub-patterns).
 *   default: individual pattern (pattern is matched individually).
 * GlobalAttribute:
 *   c  : the content between start and end pattern is skipped
 *        during parsing (e.g. pattern encloses a comment).
 *   f  : flash matching pattern (if not set, then only jump
 *        to matching pattern is supported).
 *   m  : mono pattern - set exist out of only one single pattern
 *        (start pattern = end pattern; e.g. quotes like ")
 *   p  : ignore highlight info code of single patterns of this set
 *        ("plain").
 * StringPatternKind:
 *   s  : start string pattern.
 *   m  : middle string pattern.
 *   e  : end string pattern.
 * WordBoundaryAttribute:
 *   w  : pattern is word (i.e. before and after pattern
 *        there must be a delimiter).
 *   l  : before pattern must be a delimiter (left side).
 *   r  : after pattern must be a delimiter (right side).
 *   default: neither before nor after pattern must be a delimiter.
 * StringAttribute:
 *   i  : pattern is case insensitive (if not set: pattern is
 *        case sensitive).
 *   x  : pattern is regular expression (if not set: pattern is
 *        literal string).
 *
 * \n : end of pattern
 */

static char *DefaultStringMatchTable[] = {
  "PLAIN{"
    "Round braces::fp:s:\"(\":e:\")\":\n"
    "Curly braces::fp:s:\"{\":e:\"}\":\n"
    "Squared braces::fp:s:\"[\":e:\"]\":\n"
    "Sharp braces::fp:s:\"<\":e:\">\":\n}",
  "C++{"
    "Comment:s:cf:s:\"/*\":e:\"*/\":\n"
    "Double Quotes:s:cmf:s:\"\"\"\":\n"
    "Single Quotes:s:cmf:s:\"'\":\n"
    "Round braces:s:f:s:\"(\":e:\")\":\n"
    "Curly braces:s:f:s:\"{\":e:\"}\":\n"
    "Squared braces:s:f:s:\"[\":e:\"]\":\n"
    "#ifdef statement:s:f:sx:\"#\\s*ifdef\":sx:\"#\\s*ifndef\":sx:\"#\\s*if\":mx:\"#\\s*elif\":mx:\"#\\s*else\":ex:\"#\\s*endif\":\n"
    "#if group:g:Comment:Double Quotes:Single Quotes:#ifdef statement:\n"
    "Braces:g:Comment:Double Quotes:Single Quotes:Curly braces:Round braces:Squared braces:\n}",
  "C{"
    "Comment:s:cf:s:\"/*\":e:\"*/\":\n"
    "Double Quotes:s:cmf:s:\"\"\"\":\n"
    "Single Quotes:s:cmf:s:\"'\":\n"
    "Round braces:s:f:s:\"(\":e:\")\":\n"
    "Curly braces:s:f:s:\"{\":e:\"}\":\n"
    "Squared braces:s:f:s:\"[\":e:\"]\":\n"
    "#ifdef statement:s:f:sx:\"#\\s*ifdef\":sx:\"#\\s*ifndef\":sx:\"#\\s*if\":mx:\"#\\s*elif\":mx:\"#\\s*else\":ex:\"#\\s*endif\":\n"
    "#if group:g:Comment:Double Quotes:Single Quotes:#ifdef statement:\n"
    "Braces:g:Comment:Double Quotes:Single Quotes:Curly braces:Round braces:Squared braces:\n}",
  "CSS{"
    "comment:s:cf:s:\"/*\":e:\"*/\":\n"
    "curly braces:s:f:s:\"{\":e:\"}\":\n"
    "round braces:s:f:s:\"(\":e:\")\":\n"
    "double quotes:s:cfm:s:\"\"\"\":\n"
    "single quotes:s:cfm:s:\"'\":\n"
    "braces:g:comment:single quotes:double quotes:curly braces:round braces:\n}",
  "Csh{"
    "Double Quotes:s:cmf:s:\"\"\"\":\n"
    "Single Quotes:s:cmf:s:\"'\":\n"
    "Squared braces:s:f:s:\"[\":e:\"]\":\n"
    "Round braces:s:f:s:\"(\":e:\")\":\n"
    "Curly braces:s:f:s:\"{\":e:\"}\":\n"
    "if statement:s:f:sw:\"if\":mw:\"else\":ew:\"endif\":\n"
    "switch statement:s:f:sw:\"switch\":mw:\"case\":mw:\"default\":ew:\"endsw\":\n"
    "foreach statement:s:f:sw:\"for\":ew:\"end\":\n"
    "while statement:s:f:sw:\"while\":ew:\"end\":\n"
    "statement group:g:Double Quotes:Single Quotes:if statement:switch statement:foreach statement:while statement:\n"
    "Braces:g:Double Quotes:Single Quotes:Squared braces:Round braces:Curly braces:\n}",
  "Java{"
    "Comment:s:cf:s:\"/*\":e:\"*/\":\n"
    "Double Quotes:s:cmf:s:\"\"\"\":\n"
    "Single Quotes:s:cmf:s:\"'\":\n"
    "Round braces:s:f:s:\"(\":e:\")\":\n"
    "Curly braces:s:f:s:\"{\":e:\"}\":\n"
    "Squared braces:s:f:s:\"[\":e:\"]\":\n"
    "Braces:g:Comment:Double Quotes:Single Quotes:Curly braces:Round braces:Squared braces:\n}",
  "JavaScript{"
    "Comment:s:cf:s:\"/*\":e:\"*/\":\n"
    "Double Quotes:s:cmf:s:\"\"\"\":\n"
    "Single Quotes:s:cmf:s:\"'\":\n"
    "Round braces:s:f:s:\"(\":e:\")\":\n"
    "Curly braces:s:f:s:\"{\":e:\"}\":\n"
    "Squared braces:s:f:s:\"[\":e:\"]\":\n"
    "Braces:g:Comment:Double Quotes:Single Quotes:Curly braces:Round braces:Squared braces:\n}",
  "Makefile{"
    "Double Quotes:s:cmf:s:\"\"\"\":\n"
    "Single Quotes:s:cmf:s:\"'\":\n"
    "Round braces:s:f:s:\"(\":e:\")\":\n"
    "Curly braces:s:f:s:\"{\":e:\"}\":\n"
    "Braces:g:Double Quotes:Single Quotes:Curly braces:Round braces:\n}",
  "NEdit Macro{"
    "Double Quotes:s:cmf:s:\"\"\"\":\n"
    "Round braces:s:f:s:\"(\":e:\")\":\n"
    "Curly braces:s:f:s:\"{\":e:\"}\":\n"
    "Squared braces:s:f:s:\"[\":e:\"]\":\n"
    "Braces:g:Double Quotes:Curly braces:Round braces:Squared braces:\n}",
  "Pascal{"
    "Single Quotes:s:cmf:s:\"'\":\n"
    "Comment1:s:cf:s:\"(*\":e:\"*)\":\n"
    "Comment2:s:cf:s:\"{\":e:\"}\":\n"
    "Round braces:s:f:s:\"(\":e:\")\":\n"
    "Squared braces:s:f:s:\"[\":e:\"]\":\n"
    "Block:s:f:swi:\"begin\":ewi:\"end\":\n"
    "Case:s:fp:swi:\"case\":ewi:\"end\":\n"
    "Record:s:f:swi:\"record\":ewi:\"end\":\n"
    "Statement:g:Comment1:Comment2:Single Quotes:Block:Case:Record:\n"
    "Braces:g:Comment1:Comment2:Single Quotes:Round braces:Squared braces:\n}",
  "Perl{"
    "Comment:s:cf:s:\"/*\":e:\"*/\":\n"
    "Double Quotes:s:cmf:s:\"\"\"\":\n"
    "Single Quotes:s:cmf:s:\"'\":\n"
    "Round braces:s:f:s:\"(\":e:\")\":\n"
    "Curly braces:s:f:s:\"{\":e:\"}\":\n"
    "Squared braces:s:f:s:\"[\":e:\"]\":\n"
    "Braces:g:Comment:Double Quotes:Single Quotes:Curly braces:Round braces:Squared braces:\n}",
  "SGML HTML{"
    "Double Quotes:s:cmf:s:\"\"\"\":\n"
    "Single Quotes:s:cmf:s:\"'\":\n"
    "Sharp braces:s:f:s:\"<\":e:\">\":\n"
    "Round braces:s:f:s:\"(\":e:\")\":\n"
    "Curly braces:s:f:s:\"{\":e:\"}\":\n"
    "Squared braces:s:f:s:\"[\":e:\"]\":\n"
    "Braces:g:Double Quotes:Single Quotes:Sharp braces:Curly braces:Round braces:Squared braces:\n}",
  "Sh Ksh Bash{"
    "Double Quotes:s:cmf:s:\"\"\"\":\n"
    "Single Quotes:s:cmf:s:\"'\":\n"
    "Back Quotes:s:cfm:s:\"`\":\n"
    "Round braces:s:f:s:\"(\":e:\")\":\n"
    "Curly braces:s:f:s:\"{\":e:\"}\":\n"
    "Squared braces:s:f:s:\"[\":e:\"]\":\n"
    "if statement:s:f:sw:\"if\":mw:\"elif\":mw:\"else\":ew:\"fi\":\n"
    "case statement:s:f:sw:\"case\":ew:\"esac\":\n"
    "for statement:s:f:sw:\"for\":mw:\"do\":ew:\"done\":\n"
    "while statement:s:f:sw:\"while\":mw:\"do\":ew:\"done\":\n"
    "statement group:g:Double Quotes:Single Quotes:if statement:case statement:for statement:while statement:\n"
    "Braces:g:Double Quotes:Single Quotes:Back Quotes:Squared braces:Round braces:Curly braces:\n}",
  "XML{"
    "round braces:s:f:s:\"(\":e:\")\":\n"
    "comment:s:cf:s:\"<!--\":e:\"-->\":\n"
    "cdata + ignore:s:cf:sx:\"\\<!\\[((?icdata)|(\\s*IGNORE\\s*))\\[\":ex:\"\\]\\]\\>\":\n"
    "short element:s:cf:sx:\"(?n\\<[\\l_][^@$%/\"\"';!>\\s]*(?=[^>]*/\\>))\":ex:\"/\\>\":\n"
    "element pair:s:f:sx:\"(?n\\<(*1[\\l|_][^@$%/\"\"';!>\\s]*)\\>)\":sx:\"(?n\\<(*1[\\l|_][^@$%/\"\"';!>\\s]*)(?=[^>]*[^/]\\>))\":eix:\"\\</\\1\\>\":\n"
    "processing instruction:s:f:sx:\"\\<\\?\\S+\":ex:\"\\?\\>\":\n"
    "double quotes:s:cmf:s:\"\"\"\":\n"
    "single quotes:s:cmf:s:\"'\":\n"
    "tags:g:comment:double quotes:single quotes:round braces:cdata + ignore:element pair:short element:processing instruction:\n}",
   };

static char *StandardStringMatchTable =
    "{Round braces::fp:s:\"(\":e:\")\":\n"
    "Curly braces::fp:s:\"{\":e:\"}\":\n"
    "Squared braces::fp:s:\"[\":e:\"]\":\n}";

/*
** Return string match table related to given language mode name.
** Return NULL, if no table is found.
*/
void *FindStringMatchTable(const char *langModeName)
{
    const char *nameToSearch;
    int i;

    if (langModeName == NULL)
        nameToSearch = PLAIN_LM_STRING;
    else
        nameToSearch = langModeName;

    for (i=0; i<NbrMatchTables; i++)
        if (!strcmp(nameToSearch, MatchTables[i]->smtLanguageMode))
            return (void *)MatchTables[i];
    return NULL;
}

/*
** Change the language mode name of string match tables for language
** "oldName" to "newName" in both the stored tables, and the table
** currently being edited in the dialog.
*/
void RenameStringMatchTable(const char *oldName, const char *newName)
{
    int i;

    for (i=0; i<NbrMatchTables; i++)
    {
        if (!strcmp(oldName, MatchTables[i]->smtLanguageMode))
        {
            XtFree(MatchTables[i]->smtLanguageMode);
            MatchTables[i]->smtLanguageMode = XtNewString(newName);
        }
    }
    if (MatchPatternDialog.mpdShell != NULL)
    {
        if (!strcmp(MatchPatternDialog.mpdLangModeName, oldName))
        {
            XtFree(MatchPatternDialog.mpdLangModeName);
            MatchPatternDialog.mpdLangModeName = XtNewString(newName);
        }
    }
}

/*
** Delete string match table related to given language mode name.
*/
void DeleteStringMatchTable(const char *langModeName)
{
    int i;

    for (i=0; i<NbrMatchTables; i++)
    {
        if (!strcmp(langModeName, MatchTables[i]->smtLanguageMode))
        {
            /*
             * free (delete) existing matching pattern
             */
            freeStringMatchTable(MatchTables[i]);
            memmove(
              &MatchTables[i],
              &MatchTables[i+1],
              (NbrMatchTables-1 - i) * sizeof(StringMatchTable *));
            NbrMatchTables--;
            break;
        }
    }
}

/*
** Assign a standard string match table to a given new language mode.
*/
void AssignStandardStringMatchTable(const char *langModeName)
{
    char *list;
    StringMatchTable *newTable;

    /*
     * assign standard table for new language mode
     * add table to end
     */
    list = StandardStringMatchTable;
    newTable =
      readMatchPatternSetContent(&list, list, XtNewString(langModeName));

    /*
     * add table to end
     */
    MatchTables[NbrMatchTables++] = newTable;
}

/*
** Returns True if there is a string match table, or potential table
** not yet committed in the match pattern dialog for a language mode,
*/
int LMHasStringMatchTable(const char *languageMode)
{
    StringMatchTable *table = FindStringMatchTable(languageMode);

    if (table != NULL && table->smtNumberOfSeqElements != 0)
        return True;
    return MatchPatternDialog.mpdShell != NULL &&
           !strcmp(MatchPatternDialog.mpdLangModeName, languageMode) &&
           MatchPatternDialog.mpdTable.dmpiNbrOfSeqElements != 0;
}

/*
** Read a string representing string matching pattern sets and add them
** to the StringMatchTable list of loaded string match patterns.
** Returns true, if read of string was successful.
*/
int LoadMatchPatternString(char *inString)
{
    char *inPtr = inString;
    StringMatchTable *table;
    int i;

    for (;;)
    {
        /*
         * read each matching pattern set
         */
        table = readMatchPatternSet(&inPtr);

        if (table != NULL)
        {
            /*
             * add/change the pattern set in the list
             */
            for (i=0; i<NbrMatchTables; i++)
            {
                if (!strcmp(MatchTables[i]->smtLanguageMode, table->smtLanguageMode))
                {
                    freeStringMatchTable(MatchTables[i]);
                    MatchTables[i] = table;
                    break;
                }
            }
            if (i == NbrMatchTables)
            {
                MatchTables[NbrMatchTables++] = table;
                if (NbrMatchTables > MAX_LANGUAGE_MODES)
                {
                    return False;
                }
            }
        }

        /*
         * find end of this pattern. if the string ends here, we're done
         */
        inPtr = strstr(inPtr, "\n");
        if (inPtr == NULL)
        {
            return True;
        }

        /*
         * skip newline, tabs & spaces in front of next pattern.
         * if the string ends here, we're done
         */
        inPtr += strspn(inPtr, " \t\n");
        if (*inPtr == '\0')
        {
            return True;
        }
    }
}

/*
** Create a string in the correct format for the matchPatterns resource,
** containing all of the matching pattern information from the stored
** matching pattern sets for this NEdit session.
*/
char *WriteMatchPatternString(void)
{
    char *outStr, *str, *escapedStr;
    textBuffer *outBuf;
    int i, written = False;
    StringMatchTable *table;

    outBuf = BufCreate();

    for (i=0; i<NbrMatchTables; i++)
    {
        table = MatchTables[i];

        written = True;

        BufInsert(outBuf, outBuf->length, table->smtLanguageMode);
        BufInsert(outBuf, outBuf->length, ":");

        if (isDefaultMatchPatternTable(table))
        {
            BufInsert(outBuf, outBuf->length, "Default\n\t");
        }
        else
        {
            BufInsert(outBuf, outBuf->length, "{\n");
            BufInsert(outBuf, outBuf->length,
                    str = createMatchPatternsString(table, "\t\t"));
            XtFree(str);
            BufInsert(outBuf, outBuf->length, "\t}\n\t");
        }
    }

    /*
     * Get the output string, and lop off the trailing newline and tab
     */
    outStr = BufGetRange(outBuf, 0, outBuf->length - (written?2:0));
    BufFree(outBuf);

    /*
     * Protect newlines and backslashes from translation by the resource
     * reader
     */
    escapedStr = EscapeSensitiveChars(outStr);

    XtFree(outStr);

    return escapedStr;
}

/*
** Check, if last pattern is a duplicate of a previous pattern.
** Convert last pattern to a reference, if so.
*/
static void treatDuplicatedPattern(
  MatchPatternTable *table,
  PatternElement    *prevPattern,
  PatternReference   prevPatRef,
  PatternElement    *lastPattern,
  PatternReference   lastPatRef)
{
    StringPattern *prevStringPat;
    StringPattern *lastStringPat;
    StringPattern *stringPat;
    PatternReference *oldList;
    int nbrOfRef;

    /*
     * No duplicate check needed, if previous pattern is a reference,
     * due to the related multi pattern element is checked before.
     */
    if (prevPattern->peType == PET_REFERENCE)
      return;

    prevStringPat = GetStringPattern(table, prevPattern);
    lastStringPat = GetStringPattern(table, lastPattern);

    if (!AllocatedStringsDiffer(prevStringPat->spText, lastStringPat->spText) &&
        !AllocatedStringsDiffer(prevStringPat->spOrigText, lastStringPat->spOrigText) &&
        prevStringPat->spWordBoundary == lastStringPat->spWordBoundary &&
        prevStringPat->spCaseInsensitive == lastStringPat->spCaseInsensitive &&
        prevStringPat->spRegularExpression == lastStringPat->spRegularExpression)
    {
        /*
         * Patterns are identical: Is prevPattern already a multi pattern ?
         */
        if (prevPattern->peType == PET_MULTIPLE)
        {
            /*
             * just add ref. to "lastPattern" to the ref. list
             */
            (prevPattern->peVal.peuMulti.mpNumberOfReferences) ++;
            nbrOfRef = prevPattern->peVal.peuMulti.mpNumberOfReferences;
            oldList = prevPattern->peVal.peuMulti.mpRefList;
            prevPattern->peVal.peuMulti.mpRefList =
              (PatternReference *)XtMalloc( nbrOfRef * sizeof(PatternReference) );
            memcpy(
              prevPattern->peVal.peuMulti.mpRefList,
              oldList,
              (nbrOfRef-1) * sizeof(PatternReference) );
            prevPattern->peVal.peuMulti.mpRefList[nbrOfRef-1] = lastPatRef;
            XtFree( (char *)oldList );
        }
        else
        {
            /*
             * convert prev. single pattern to multi pattern
             */
            stringPat = &prevPattern->peVal.peuSingle;
            prevPattern->peType = PET_MULTIPLE;
            prevPattern->peVal.peuMulti.mpStringPattern = *stringPat;
            prevPattern->peVal.peuMulti.mpNumberOfReferences = 1;
            prevPattern->peVal.peuMulti.mpRefList =
              (PatternReference *)XtMalloc( sizeof(PatternReference) );
            prevPattern->peVal.peuMulti.mpRefList[0] = lastPatRef;
        }

        /*
         * convert last single pattern to reference
         */
        freeStringPattern( &(lastPattern->peVal.peuSingle) );
        lastPattern->peType = PET_REFERENCE;
        lastPattern->peVal.peuRef = prevPatRef;
    }
}

/*
** Check, if last pattern is a duplicate of a pattern stored within a
** previous match pattern table element.
** Convert last pattern to a reference, if so.
*/
static void treatDuplicatedPatternElements(
  MatchPatternTable        *table,
  MatchPatternTableElement *prevElement,
  int                       prevElementIdx,
  PatternElement           *lastPattern,
  PatternReference          lastPatRef)
{
    int i;
    PatternReference prevPatRef;

    prevPatRef.prElementIdx = prevElementIdx;

    for (i=0; i<prevElement->mpteAll.pesNumberOfPattern; i++)
    {
        prevPatRef.prPatternIdx = i;
        treatDuplicatedPattern(
          table,
          prevElement->mpteAll.pesPattern[i],
          prevPatRef,
          lastPattern,
          lastPatRef);
    }
}

/*
** Check, if a pattern of last match pattern table element is a
** duplicate of a pattern stored within a previous match pattern table
** element.
** Convert duplicated last patterns to references, if so.
*/
static void treatDuplicatedMTElements(
  MatchPatternTable        *table,
  MatchPatternTableElement *prevElement,
  int                       prevElementIdx,
  MatchPatternTableElement *lastElement,
  int                       lastElementIdx)
{
    int i;
    PatternReference  lastPatRef;

    lastPatRef.prElementIdx = lastElementIdx;

    for (i=0; i<lastElement->mpteAll.pesNumberOfPattern; i++)
    {
        lastPatRef.prPatternIdx = i;
        treatDuplicatedPatternElements(
          table,
          prevElement,
          prevElementIdx,
          lastElement->mpteAll.pesPattern[i],
          lastPatRef);
    }
}

/*
** Convert all duplicated patterns of given match pattern table to
** references.
*/
static void treatDuplicatedMTEntries(
  MatchPatternTableElement **element,
  int                        nbrOfElements)
{
    int i;
    MatchPatternTableElement *lastElement;
    int lastElementIdx;
    MatchPatternTable table;

    if (nbrOfElements < 2)
        return;

    lastElementIdx = nbrOfElements - 1;
    lastElement    = element[lastElementIdx];

    table.mptElements         = element;
    table.mptNumberOfElements = nbrOfElements;

    for (i=0; i<nbrOfElements-1; i ++)
    {
        treatDuplicatedMTElements( &table, element[i], i, lastElement, lastElementIdx );
    }
}

/*
** Compile regular expressions of all string patterns of given
** match pattern table element.
** Returns true, if compilation fails.
*/
static int createStrPatRegExpOfElement(
  ReadMatchPatternInfo     *readPatInfo,
  MatchPatternTableElement *element,
  ErrorInfo                *errInfo)
{
    int i;
    StringPattern *strPat;

    for (i=0; i<element->mpteAll.pesNumberOfPattern; i++)
    {
        strPat = getReadStringPattern(readPatInfo, element->mpteAll.pesPattern[i]);

        /*
         * if current string pattern holds a regular expression, then
         * compile it
         */
        if (strPat->spRegularExpression)
        {
            if (createStrPatRegExp(strPat, errInfo))
            {
                /*
                 * compilation was not successful
                 */
                errInfo->eiMPTabElementName = XtNewString(element->mpteName);
                return True;
            }
        }
    }

    return False;
}

/*
** Compile regular expressions of given string pattern.
** Returns true, if compilation fails.
*/
static int createStrPatRegExp(
  StringPattern *strPat,
  ErrorInfo     *errInfo)
{
    char *regExpString;
    char *compileMsg;

    /*
     * compose regular expression for start string pattern.
     */
    if( strPat->spCaseInsensitive)
    {
        /*
         * Add '(?i .. )' to given text for case insensitive search.
         * Allocate buffer to hold 5 more char than text length
         * (4 char '(?i)' + \0 char.
         */
        regExpString = XtMalloc(strPat->spLength + 5);
        strcpy(regExpString, "(?i");
        strcat(regExpString, strPat->spText);
        strcat(regExpString, ")");
    }
    else
    {
        regExpString = strPat->spText;
    }

    /*
     * compile regular expression & free allocated string buffer,
     * if applicable.
     */
    strPat->spTextRE =
      CompileRE(regExpString, &compileMsg, REDFLT_STANDARD);

    if (strPat->spTextRE == NULL)
    {
        /*
         * compilation was not successful: adapt error reason by
         * converting local backrefs to global ones.
         */
        adaptCompileMsg(compileMsg, strPat->spGlobalToLocalBackRef);

        errInfo->eiRegExpCompileMsg = compileMsg;
        errInfo->eiStringPatText    = XtNewString(strPat->spOrigText);
    }

    if (strPat->spCaseInsensitive)
        XtFree( regExpString );

    return (strPat->spTextRE == NULL);
}

/*
** adapt regular expression compilation message by converting local
** backrefs to global ones.
*/
static void adaptCompileMsg(
  char *compileMsg,
  int  *globalToLocalBackRef)
{
    int localId;
    int globalId;
    char *s = compileMsg;

    while (*s != '\0')
    {
        if (*s == '\\')
        {
            if (isdigit((unsigned char)*(s+1)))
            {
                /*
                 * \n (n=1..9) found: substitute local by global back ref.
                 */
                s ++;

                localId =
                  (int)((unsigned char)*s - (unsigned char)'0');

                globalId = localToGlobalBackRef(globalToLocalBackRef, localId);

                *s = (char)((int)('0') + globalId);
            }
            else if (*(s+1) != '\0')
                s ++;
        }
        s ++;
    }
}

/*
** translate given local backref to global backref by using
** given globalToLocalBackRef list.
*/
static int localToGlobalBackRef(
  int  *globalToLocalBackRef,
  int  localId)
{
    int i;

    for (i=0; i < MAX_GLOBAL_BACK_REF_ID; i++)
    {
        if (globalToLocalBackRef[i] == localId)
            return i+1;
    }

    return 0;
}

/*
** Create a regular expression holding keywords of given start & end
** pattern set.
** Returns true, if creation of regular expression has failed.
*/
static int createStartEndRegExp(
  ReadMatchPatternInfo     *readMatchPatternInfo,
  MatchPatternTableElement *element,
  ErrorInfo                *errInfo)
{
    char *regExpString;
    char *compileMsg;

    /*
     * compose regular expression for start / end pattern.
     */
    composeStartEndRegExpString(
      readMatchPatternInfo,
      element,
      &regExpString);

    /*
     * compile regular expression & free allocated string buffer.
     */
    element->mpteStartEndRE =
      CompileRE(regExpString, &compileMsg, REDFLT_STANDARD);

    XtFree( regExpString );

    if( element->mpteStartEndRE == NULL)
    {
        errInfo->eiRegExpCompileMsg = compileMsg;
        errInfo->eiDetail           = "Error compiling start / end reg. exp.";
    }

    return (element->mpteStartEndRE == NULL);
}

/*
** Create a regular expression holding keywords of given group element.
** Returns true, if creation of regular expression has failed.
*/
static int createGroupRegExp(
  ReadMatchPatternInfo     *readPatInfo,
  MatchPatternGroupElement *group,
  ErrorInfo                *errInfo)
{
    int i;
    MatchPatternTableElement *element;
    RegExpStringInfo regExpStringInfo;
    char *regExpString;
    regexp *keywordRE;
    char *compileMsg;

    /*
     * Allocate buffers for keyword regular expression of this group.
     */
    setupRegExpStringBuffers(
      readPatInfo,
      &regExpStringInfo);

    for (i=0; i<group->mpgeNumberOfSubPatterns; i++)
    {
        element = getPatternOfName(readPatInfo, group->mpgeSubPatternIds[i]);
        /*
         * Add the keywords of the sub pattern to the keyword regular
         * expression string buffer of new group.
         */
        addElementToRegExpString(
          element,
          readPatInfo,
          &regExpStringInfo);
    }

    /*
     * Assemble and compile the resulting keyword regular expression string.
     */
    catSMTRegExpStrings(
      &regExpStringInfo,
      &regExpString);

    keywordRE = CompileRE(regExpString, &compileMsg, REDFLT_STANDARD);

    XtFree( regExpString );

    if (keywordRE == NULL)
    {
        errInfo->eiMPTabElementName = XtNewString(group->mpgeName);
        errInfo->eiRegExpCompileMsg = compileMsg;
        errInfo->eiDetail           = "Group: Compile reg. exp. error";
    }

    group->mpgeKeywordRE = keywordRE;

    return (keywordRE == NULL);
}

/*
** Allocate memory for regular expression strings to be
** created out of read match pattern info.
*/
static void setupRegExpStringBuffers(
  ReadMatchPatternInfo  *readMatchPatternInfo,
  RegExpStringInfo      *regExpStringInfo)
{
    int totalLen;
    int nbrOfMultiPatterns;
    int sizeOfPatRefs;

    /*
     * determine total length of pattern characters to determine
     * the size of a string buffer for the regular expression to
     * compose. Count number of total multi patterns, too.
     */
    totalLen =
      totalKeywordOfTableLen( readMatchPatternInfo, &nbrOfMultiPatterns );

    /*
     * allocate memory to store added multi pattern references (to avoid
     * duplicated keywords strings later on).
     */
    sizeOfPatRefs = sizeof(PatternReference *) * nbrOfMultiPatterns;

    regExpStringInfo->resiAddedMultiPat =
      (PatternReference **)XtMalloc( sizeOfPatRefs );

    regExpStringInfo->resiNbrOfAddedMultiPat = 0;

    /*
     * init. ID of capturing local backrefs
     */
    regExpStringInfo->resiLocalBackRefID = 1;

    /*
     * allocate & init. string buffers for regular expression:
     * 3 times the size + x of all pattern characters (due to
     * a.) each char may need to be escaped
     * b.) '<(?:', ')>', ')' and '|' need to be added.
     */
    regExpStringInfo->resiNoneWBRegExpString  = XtMalloc( 3 * totalLen );
    regExpStringInfo->resiLeftWBRegExpString  = XtMalloc( 3 * totalLen + 5 );
    regExpStringInfo->resiRightWBRegExpString = XtMalloc( 3 * totalLen + 5 );
    regExpStringInfo->resiBothWBRegExpString  = XtMalloc( 3 * totalLen + 6 );

    strcpy( regExpStringInfo->resiNoneWBRegExpString,  "" );
    strcpy( regExpStringInfo->resiLeftWBRegExpString,  "" );
    strcpy( regExpStringInfo->resiRightWBRegExpString, "" );
    strcpy( regExpStringInfo->resiBothWBRegExpString,  "" );
}

/*
** Concatenate given 'partToAdd' string to result string, separated
** by an OR ('|'). Add 'postfix' at end of result string.
*/
static void addSMTRegExpString(
  char       *result,
  char       *partToAdd,
  const char *postfix)
{
    if (strlen(partToAdd) != 0)
    {
        if (strlen(result) != 0)
        {
            strcat( result, "|" );
        }

        strcat( result, partToAdd );

        strcat( result, postfix );
    }

}

/*
** Return a string representing given string match table.
*/
static char *createMatchPatternsString(
  StringMatchTable *table,
  char             *indentStr)
{
    char *outStr, *str;
    textBuffer *outBuf;
    int i, j;
    MatchPatternSequenceElement *seq;
    MatchPatternTableElement *element;
    MatchPatternGroupElement *group;

    outBuf = BufCreate();

    for (i=0; i<table->smtNumberOfSeqElements; i++)
    {
        seq = table->smtSequence[i];

        BufInsert(outBuf, outBuf->length, indentStr);
        BufInsert(outBuf, outBuf->length, seq->mpseName);
        BufInsert(outBuf, outBuf->length, ":");

        if (seq->mpseType == MPT_GROUP)
        {
            BufInsert(outBuf, outBuf->length, "g:");

            group = table->smtGroups[seq->mpseIndex];

            for (j=0; j < group->mpgeNumberOfSubPatterns; j ++)
            {
                BufInsert(outBuf, outBuf->length, group->mpgeSubPatternIds[j]);
                BufInsert(outBuf, outBuf->length, ":");
            }
        }
        else
        {
            if (seq->mpseType == MPT_SUB)
            {
                BufInsert(outBuf, outBuf->length, "s");
            }
            BufInsert(outBuf, outBuf->length, ":");

            element = table->smtAllPatterns->mptElements[seq->mpseIndex];

            /*
             * write global attributes
             */
            if (element->mpteSkipBtwnStartEnd)
                BufInsert(outBuf, outBuf->length, "c");
            if (element->mpteFlash)
                BufInsert(outBuf, outBuf->length, "f");
            if (element->mpteIsMonoPattern)
                BufInsert(outBuf, outBuf->length, "m");
            if (element->mpteIgnoreHighLightInfo)
                BufInsert(outBuf, outBuf->length, "p");
            BufInsert(outBuf, outBuf->length, ":");

            /*
             * write string patterns
             */
            for (j=0; j < element->mpteAll.pesNumberOfPattern; j ++)
            {
                BufInsert(
                  outBuf,
                  outBuf->length,
                  str =
                    createPatternElementString(
                      table->smtAllPatterns,
                      element->mpteAll.pesPattern[j]));
                XtFree(str);
            }
        }

        BufInsert(outBuf, outBuf->length, "\n");
    }

    outStr = BufGetAll(outBuf);
    BufFree(outBuf);

    return outStr;
}

/*
** Return a string representing given pattern element.
*/
static char *createPatternElementString(
  MatchPatternTable *table,
  PatternElement    *pat)
{
    char *outStr, *str;
    textBuffer *outBuf;
    StringPattern *strPat;

    outBuf = BufCreate();

    strPat = GetStringPattern(table, pat);

    /*
     * write string pattern kind
     */
    if (pat->peKind == PEK_START)
        BufInsert(outBuf, outBuf->length, "s");
    else if (pat->peKind == PEK_MIDDLE)
        BufInsert(outBuf, outBuf->length, "m");
    else if (pat->peKind == PEK_END)
        BufInsert(outBuf, outBuf->length, "e");

    /*
     * write word boundary
     */
    if (strPat->spWordBoundary == PWB_BOTH)
        BufInsert(outBuf, outBuf->length, "w");
    else if (strPat->spWordBoundary == PWB_LEFT)
        BufInsert(outBuf, outBuf->length, "l");
    else if (strPat->spWordBoundary == PWB_RIGHT)
        BufInsert(outBuf, outBuf->length, "r");

    /*
     * write case insensitive flag
     */
    if (strPat->spCaseInsensitive)
        BufInsert(outBuf, outBuf->length, "i");

    /*
     * write regular expression flag
     */
    if (strPat->spRegularExpression)
        BufInsert(outBuf, outBuf->length, "x");

    BufInsert(outBuf, outBuf->length, ":");

    /*
     * write pattern string
     */
    if( strPat->spOrigText != NULL)
        BufInsert(
          outBuf,
          outBuf->length,
          str = MakeQuotedString(strPat->spOrigText));
    else
        BufInsert(
          outBuf,
          outBuf->length,
          str = MakeQuotedString(strPat->spText));
    XtFree(str);

    BufInsert(outBuf, outBuf->length, ":");

    outStr = BufGetAll(outBuf);
    BufFree(outBuf);

    return outStr;
}

/*
** Given a language mode name, determine if there is a default (built-in)
** string match table available for that language mode, and if so, read it and
** return a new allocated copy of it. The returned pattern set should be
** freed by the caller with freeStringMatchTable().
*/
static StringMatchTable *readDefaultStringMatchTable(const char *langModeName)
{
    int i, modeNameLen;
    char *list;

    modeNameLen = strlen(langModeName);

    for (i=0; i<(int)XtNumber(DefaultStringMatchTable); i++)
    {
        if (!strncmp(langModeName, DefaultStringMatchTable[i], modeNameLen) &&
                DefaultStringMatchTable[i][modeNameLen] == '{')
        {
            list = DefaultStringMatchTable[i];
            return readMatchPatternSet(&list);
        }
    }

    list = StandardStringMatchTable;
    return readMatchPatternSetContent(&list, list, XtNewString(langModeName));
}

/*
** Return true, if table exactly matches one of the default matching
** pattern tables.
*/
static int isDefaultMatchPatternTable(StringMatchTable *table)
{
    StringMatchTable *defaultTable;
    int retVal;

    defaultTable = readDefaultStringMatchTable(table->smtLanguageMode);

    if (defaultTable == NULL)
        return False;

    retVal = !stringMatchTableDiffer(table, defaultTable);

    freeStringMatchTable(defaultTable);

    return retVal;
}

/*
** Read in a string match pattern table character string,
** and advance *inPtr beyond it.
** Returns NULL and outputs an error to stderr on failure.
*/
static StringMatchTable *readMatchPatternSet(char **inPtr)
{
    char *languageMode;
    StringMatchTable *table = NULL;
    char *stringStart = *inPtr;
    ErrorInfo errInfo;

    initErrorInfo(&errInfo);

    /*
     * remove leading whitespace
     */
    *inPtr += strspn(*inPtr, " \t\n");

    /*
     * read language mode field
     */
    languageMode = ReadSymbolicField(inPtr);

    /*
     * look for initial brace
     */
    if (**inPtr == ':')
    {
        (*inPtr) ++;
         /*
          * look for "Default" keyword, and if it's there, return the default
          * pattern set
          */
        if (!strncmp(*inPtr, "Default", 7))
        {
            *inPtr += 7;
            table = readDefaultStringMatchTable(languageMode);
            XtFree(languageMode);

            return table;
        }
    }

    table = readMatchPatternSetContent(inPtr, stringStart, languageMode);

    if (table == NULL)
        XtFree(languageMode);

    return table;
}

/*
** Read in a content string ("{..}") of match pattern table,
** and advance *inPtr beyond it.
** Returns NULL and outputs an error to stderr on failure.
*/
static StringMatchTable *readMatchPatternSetContent(
  char **inPtr,
  char  *stringStart,
  char  *languageMode)
{
    ReadMatchPatternInfo readPatInfo;
    StringMatchTable *table = NULL;
    ErrorInfo errInfo;
    int successful = True;
    int endOfPatternSet = False;

    initErrorInfo(&errInfo);

    /*
     * look for initial brace
     */
    if (**inPtr != '{')
    {
        errInfo.eiLanguageMode = XtNewString(languageMode);
        errInfo.eiDetail       = "pattern list must begin with \"{\"";
        parseMatchingPatternSetError(stringStart, *inPtr, &errInfo );

        return NULL;
    }

    (*inPtr)++;

    readPatInfo.rmpiNbrOfElements    = 0;
    readPatInfo.rmpiNbrOfGroups      = 0;
    readPatInfo.rmpiNbrOfSeqElements = 0;
    readPatInfo.rmpiAllPatRE         = NULL;
    readPatInfo.rmpiFlashPatRE       = NULL;

    /*
     * parse each pattern in the list
     */
    while (successful && !endOfPatternSet)
    {
        *inPtr += strspn(*inPtr, " \t\n");
        if (**inPtr == '\0')
        {
            errInfo.eiLanguageMode = XtNewString(languageMode);
            errInfo.eiDetail       = "end of pattern list not found";
            parseMatchingPatternSetError(stringStart, *inPtr, &errInfo);
            successful = False;
        }
        else if (**inPtr == '}')
        {
            (*inPtr)++;
            endOfPatternSet = True;
        }
        else
        {
            if (!readMatchPatternEntry(inPtr, &errInfo, &readPatInfo))
            {
                errInfo.eiLanguageMode = XtNewString(languageMode);
                parseMatchingPatternSetError(stringStart, *inPtr, &errInfo);
                successful = False;
            }
        }
    }

    if (successful)
    {
        /*
         * compile regular expressions of read patterns
         */
        if (createRegExpOfPatterns(&readPatInfo, &errInfo))
        {
            parseMatchingPatternSetError(stringStart, *inPtr, &errInfo);
            successful = False;
        }
    }

    if (successful)
    {
        return createStringMatchTable(&readPatInfo, languageMode);
    }
    else
    {
        /*
         * free memory of already read patterns
         */
        freeReadMatchPatternInfo(&readPatInfo);

        return NULL;
    }

    return table;
}

/*
** Create a reg. exp. of all patterns contained
** in given read match pattern info.
*/
static int createRegExpOfPatterns(
  ReadMatchPatternInfo *readPatInfo,
  ErrorInfo            *errInfo)
{
    if (createRegExpOfStrPatterns(readPatInfo, errInfo))
        return True;

    if (createRegExpOfAllPatterns(readPatInfo, errInfo))
        return True;

    return False;
}

/*
** Create a "total pattern reg. exp." of all patterns / flash patterns
** contained in given read match pattern info.
** Returns true, if create of reg. exp. has failed.
*/
static int createRegExpOfAllPatterns(
  ReadMatchPatternInfo *readPatInfo,
  ErrorInfo            *errInfo)
{
    int i;
    RegExpStringInfo allPatRegExpSI;
    RegExpStringInfo flashPatRegExpSI;
    MatchPatternTableElement *element;
    char *regExpString;
    char *compileMsg;

    /*
     * Allocate buffers for keyword regular expressions.
     */
    setupRegExpStringBuffers(readPatInfo, &allPatRegExpSI);
    setupRegExpStringBuffers(readPatInfo, &flashPatRegExpSI);

    for (i=0; i < readPatInfo->rmpiNbrOfElements; i ++)
    {
        element = readPatInfo->rmpiElement[i];

        /*
         * Add the keywords of the matching pattern to the keyword
         * regular expression string buffer of all patterns.
         */
        addElementToRegExpString(
          element,
          readPatInfo,
          &allPatRegExpSI);

        /*
         * If flash attribute is set, then add the keywords of the
         * matching pattern also to the keyword regular expression
         * string buffer of flash patterns.
         */
        if (element->mpteFlash)
        {
            addElementToRegExpString(
              element,
              readPatInfo,
              &flashPatRegExpSI);
        }
    }

    /*
     * Assemble and compile the resulting all keywords reg. exp. string.
     */
    catSMTRegExpStrings(
      &allPatRegExpSI,
      &regExpString);

    readPatInfo->rmpiAllPatRE =
      CompileRE(regExpString, &compileMsg, REDFLT_STANDARD);

    XtFree( regExpString );

    if (readPatInfo->rmpiAllPatRE == NULL)
    {
        errInfo->eiRegExpCompileMsg = compileMsg;
        errInfo->eiDetail           = "All patterns: compile reg. exp. error";
        return True;
    }

    /*
     * Assemble and compile the resulting flash keywords reg. exp. string.
     */
    catSMTRegExpStrings(
      &flashPatRegExpSI,
      &regExpString);

    readPatInfo->rmpiFlashPatRE =
      CompileRE(regExpString, &compileMsg, REDFLT_STANDARD);

    XtFree( regExpString );

    if (readPatInfo->rmpiFlashPatRE == NULL)
    {
        errInfo->eiRegExpCompileMsg = compileMsg;
        errInfo->eiDetail           = "Flash patterns: compile reg. exp. error";
        return True;
    }

    /*
     * Pattern reg. exp. successful created:
     */
    return False;
}

/*
** Create reg. exp. of single patterns contained in given
** read match pattern info.
** Returns true, if create of reg. exp. has failed.
*/
static int createRegExpOfStrPatterns(
  ReadMatchPatternInfo *readPatInfo,
  ErrorInfo            *errInfo)
{
    int i;
    MatchPatternTableElement *element;
    MatchPatternGroupElement *group;

    /*
     * create global backref list of all elements of read info
     */
    for (i=0; i < readPatInfo->rmpiNbrOfElements; i ++)
    {
        element = readPatInfo->rmpiElement[i];

        if (!createGlobalBackRefList(readPatInfo, element, errInfo))
            return True;
    }

    /*
     * resolve global backrefs of all elements of read info
     */
    for (i=0; i < readPatInfo->rmpiNbrOfElements; i ++)
    {
        element = readPatInfo->rmpiElement[i];

        if (!resolveGlobalBackRefs(readPatInfo, element, errInfo))
            return True;
    }

    /*
     * compile reg. exp. of all elements of read info
     */
    for (i=0; i < readPatInfo->rmpiNbrOfElements; i ++)
    {
        element = readPatInfo->rmpiElement[i];

        if (createStrPatRegExpOfElement(readPatInfo, element, errInfo))
            return True;

        /*
         * create start / end reg. exp. if applicable.
         */
        if (element->mpteSkipBtwnStartEnd)
        {
            if (createStartEndRegExp(readPatInfo, element, errInfo))
                return True;
        }
    }

    /*
     * compile reg. exp. of all groups of read info
     */
    for (i=0; i < readPatInfo->rmpiNbrOfGroups; i ++)
    {
        group = readPatInfo->rmpiGroup[i];

        if (createGroupRegExp(readPatInfo, group, errInfo))
        {
            return True;
        }
    }

    return False;
}

/*
** Create a string match table out of read match pattern info.
*/
static StringMatchTable *createStringMatchTable(
  ReadMatchPatternInfo *readPatInfo,
  char                 *languageMode)
{
    StringMatchTable *table;
    MatchPatternTable *patTable;
    int sizeOfElements;

    table = (StringMatchTable *)XtMalloc(sizeof(StringMatchTable));
    table->smtLanguageMode = languageMode;

    /*
     * allocate a more appropriately sized list to return matching patterns
     */
    patTable = (MatchPatternTable *)XtMalloc(sizeof(MatchPatternTable));
    patTable->mptNumberOfElements = readPatInfo->rmpiNbrOfElements;

    if (readPatInfo->rmpiNbrOfElements > 0)
    {
        sizeOfElements =
          sizeof(MatchPatternTableElement *) * readPatInfo->rmpiNbrOfElements;
        patTable->mptElements =
          (MatchPatternTableElement **)XtMalloc(sizeOfElements);
        memcpy(patTable->mptElements, readPatInfo->rmpiElement, sizeOfElements);
    }
    else
    {
        patTable->mptElements = NULL;
    }

    table->smtAllPatterns = patTable;

    table->smtAllPatRE   = readPatInfo->rmpiAllPatRE;
    table->smtFlashPatRE = readPatInfo->rmpiFlashPatRE;
    table->smtUsedPatRE  = NULL;

    /*
     * allocate a more appropriately sized list to return matching pattern groups
     */
    table->smtNumberOfGroups = readPatInfo->rmpiNbrOfGroups;
    if (readPatInfo->rmpiNbrOfGroups > 0)
    {
        sizeOfElements =
          sizeof(MatchPatternGroupElement *) * readPatInfo->rmpiNbrOfGroups;
        table->smtGroups =
          (MatchPatternGroupElement **)XtMalloc(sizeOfElements);
        memcpy(table->smtGroups, readPatInfo->rmpiGroup, sizeOfElements);
    }
    else
    {
        table->smtGroups = NULL;
    }
    /*
     * allocate a more appropriately sized list to return matching pattern sequence
     */
    table->smtNumberOfSeqElements = readPatInfo->rmpiNbrOfSeqElements;
    if (readPatInfo->rmpiNbrOfSeqElements > 0)
    {
        sizeOfElements =
          sizeof(MatchPatternSequenceElement *) * readPatInfo->rmpiNbrOfSeqElements;
        table->smtSequence =
          (MatchPatternSequenceElement **)XtMalloc(sizeOfElements);
        memcpy(table->smtSequence, readPatInfo->rmpiSequence, sizeOfElements);
    }
    else
    {
        table->smtSequence = NULL;
    }

    return table;
}

/*
** Read one match pattern entry of a match pattern string.
** Returns true, if read was successful.
*/
static int readMatchPatternEntry(
  char                 **inPtr,
  ErrorInfo             *errInfo,
  ReadMatchPatternInfo  *info)
{
    char *name;
    MatchPatternType type;
    MatchPatternGroupElement *readGroup;
    MatchPatternTableElement *readElement;

    if (!getMPSName( inPtr, errInfo, &name ))
    {
        return False;
    }

    if (!getMPSTypeAttribute( inPtr, errInfo, &type ))
    {
        errInfo->eiMPTabElementName = XtNewString(name);
        return False;
    }

    if (type == MPT_GROUP)
    {
        if (info->rmpiNbrOfGroups >= MAX_NBR_MATCH_PATTERN_GROUPS)
        {
            errInfo->eiMPTabElementName = XtNewString(name);
            errInfo->eiDetail           = "max number of match pattern groups exceeded";
            return False;
        }

        readGroup = readMatchPatternGroup( inPtr, errInfo, name, info );

        if (readGroup == NULL)
        {
            errInfo->eiMPTabElementName = XtNewString(name);
            XtFree( name );
        }
        else
        {
            info->rmpiGroup[info->rmpiNbrOfGroups ++] = readGroup;

            recordPatternSequence( info, name, type, info->rmpiNbrOfGroups-1 );
        }

        return (readGroup != NULL);
    }
    else
    {
        if (info->rmpiNbrOfElements >= MAX_NBR_MATCH_PATTERNS)
        {
            errInfo->eiMPTabElementName = XtNewString(name);
            errInfo->eiDetail           = "max number of match patterns exceeded";
            XtFree( name );
            return False;
        }

        readElement =
          readMatchPatternTableElement( inPtr, &errInfo->eiDetail, name, type );

        if (readElement == NULL)
        {
            errInfo->eiMPTabElementName = XtNewString(name);
            XtFree( name );
        }
        else
        {
            readElement->mpteIndex = info->rmpiNbrOfElements;

            info->rmpiElement[info->rmpiNbrOfElements ++] = readElement;

            if (type == MPT_INDIVIDUAL)
            {
                if (!assignIndividualGroup( info, &errInfo->eiDetail, readElement ))
                {
                    errInfo->eiMPTabElementName = XtNewString(name);
                    return False;
                }
            }

            treatDuplicatedMTEntries(
              info->rmpiElement, info->rmpiNbrOfElements );

            recordPatternSequence( info, name, type, info->rmpiNbrOfElements-1 );
        }

        return (readElement != NULL);
    }
}

/*
** Record match pattern sequence for display of match pattern dialog.
*/
static void recordPatternSequence(
  ReadMatchPatternInfo *info,
  char                 *name,
  MatchPatternType      type,
  int                   index)
{
    MatchPatternSequenceElement *sequence;

    sequence =
      (MatchPatternSequenceElement *)XtMalloc( sizeof(MatchPatternSequenceElement) );

    sequence->mpseName  = XtNewString(name);
    sequence->mpseType  = type;
    sequence->mpseIndex = index;

    info->rmpiSequence[info->rmpiNbrOfSeqElements ++] = sequence;
}

/*
** Assign a new group to an individual match pattern.
** Returns true, if assignment was successful.
*/
static int assignIndividualGroup(
  ReadMatchPatternInfo      *info,
  char                     **errMsg,
  MatchPatternTableElement  *element)
{
    MatchPatternGroupElement *group = NULL;

    if (info->rmpiNbrOfGroups >= MAX_NBR_MATCH_PATTERN_GROUPS)
    {
        *errMsg = "max. number of matching pattern groups exceeded\n";
        return False;
    }

    /*
     * Assign the index of new group to the individual matching pattern
     */
    element->mpteGroup = info->rmpiNbrOfGroups;

    /*
     * Allocate memory for the matching pattern group and copy the
     * info into this group element.
     */
    group =
      (MatchPatternGroupElement *)XtMalloc( sizeof(MatchPatternGroupElement) );

    group->mpgeName      = NULL;
    group->mpgeKeywordRE = NULL;

    /*
     * remember name of match pattern table element, which is
     * represented by this group.
     */
    group->mpgeNumberOfSubPatterns = 1;
    group->mpgeSubPatternIds       = (char **)XtMalloc( sizeof(char *) );
    group->mpgeSubPatternIds[0]    = XtNewString(element->mpteName);

    info->rmpiGroup[info->rmpiNbrOfGroups ++] = group;

    return True;
}

/*
** Get the match pattern table element of given 'patToSearch'
** name.
** Returns NULL, if no element was found.
*/
static MatchPatternTableElement *getPatternOfName(
  ReadMatchPatternInfo *info,
  char                 *patToSearch)
{
    int i;
    MatchPatternTableElement *element;

    for (i=0; i<info->rmpiNbrOfElements; i ++)
    {
        element = info->rmpiElement[i];

        if (strcmp( element->mpteName, patToSearch ) == 0)
        {
            /*
             * Related sub-pattern found:
             */
            return element;
        }
    }

    /*
     * No sub-pattern found:
     */
    return NULL;
}

/*
** Read match pattern group of given match pattern string.
** Returns NULL, if read fails.
*/
static MatchPatternGroupElement *readMatchPatternGroup(
  char                 **inPtr,
  ErrorInfo             *errInfo,
  char                  *name,
  ReadMatchPatternInfo  *info)
{
    int i;
    int error = False;
    char *patNameInPtr;
    char *subPatName;
    MatchPatternTableElement *subPatElement;
    int  numberOfRelatedSubPattern = 0;
    char *relatedSubPatternId[MAX_NBR_MATCH_PATTERNS];
    int sizeOfIds;
    MatchPatternGroupElement *group = NULL;

    /*
     * Read sub-matching patterns of this group.
     */
    while (**inPtr != '\n' && !error)
    {
        /*
         * Read next pattern name from inPtr.
         */
        patNameInPtr = *inPtr;
        subPatName = ReadSymbolicField(inPtr);

        if (subPatName == NULL)
        {
            errInfo->eiDetail = "Sub-Matching Pattern Name expected";
            error = True;
        }
        else
        {
            /*
             * Get matching pattern related to sub-matching pattern name.
             */
            subPatElement =
              getPatternOfName( info, subPatName );

            if (subPatElement == NULL)
            {
                errInfo->eiStringPatText = XtNewString(subPatName);
                errInfo->eiDetail        = "Sub-Matching Pattern not defined before";
                error = True;
            }
            else if (numberOfRelatedSubPattern >= MAX_NBR_MATCH_PATTERNS)
            {
                errInfo->eiDetail = "Group holds too many Sub-Matching Patterns";
                error = True;
            }
            else if (subPatElement->mpteType != MPT_SUB)
            {
                errInfo->eiStringPatText = XtNewString(subPatName);
                errInfo->eiDetail        = "Not a Sub-Matching Pattern";
                error = True;
            }
            else
            {
                /*
                 * Remember sub-matching pattern ID
                 */
                relatedSubPatternId[numberOfRelatedSubPattern ++] = subPatName;

                /*
                 * Assign the index of this group to the sub-matching pattern
                 * if no group index was assigned before.
                 */
                if (subPatElement->mpteGroup == NO_GROUP_IDX)
                {
                    subPatElement->mpteGroup = info->rmpiNbrOfGroups;
                }
            }

            /*
             * Skip to the start of the next matching pattern name.
             */
            if (!error && !SkipDelimiter(inPtr, &errInfo->eiDetail))
            {
                error = True;
            }
        }
    }

    if (error)
    {
        for (i=0; i < numberOfRelatedSubPattern; i++)
        {
          XtFree( relatedSubPatternId[i] );
        }

        return NULL;
    }
    else
    {
        /*
         * Allocate memory for the matching pattern group and copy the
         * info into this group element.
         */
        group =
          (MatchPatternGroupElement *)XtMalloc( sizeof(MatchPatternGroupElement) );

        group->mpgeName      = name;
        group->mpgeKeywordRE = NULL;

        /*
         * Allocate memory for the sub-matching pattern IDs & copy
         * related sub-matching pattern into the group element.
         */
        sizeOfIds = sizeof(char *) * numberOfRelatedSubPattern;
        group->mpgeSubPatternIds = (char **)XtMalloc( sizeOfIds );

        memcpy(group->mpgeSubPatternIds, relatedSubPatternId, sizeOfIds);

        group->mpgeNumberOfSubPatterns = numberOfRelatedSubPattern;

        return group;
    }
}

/*
** Read one match pattern element from given match pattern string.
** Returns true, if read was successful.
*/
static int readPatternElement(
  char           **inPtr,
  char           **errMsg,
  PatternElement **pattern)
{
    PatternElementKind  patternKind;
    PatternWordBoundary wordBoundary;
    int caseInsensitive;
    int regularExpression;
    char *string;

    if (!getMPSPatternAttribute(
           inPtr,
           errMsg,
           &patternKind,
           &wordBoundary,
           &caseInsensitive,
           &regularExpression ))
    {
        return False;
    }

    if (!ReadQuotedString(inPtr, errMsg, &string))
    {
        return False;
    }

    if (!SkipDelimiter(inPtr, errMsg))
    {
        XtFree( string );
        return False;
    }

    *pattern =
      createPatternElement(
        string,
        patternKind,
        wordBoundary,
        caseInsensitive,
        regularExpression);

    return True;
}

/*
** Create a pattern element.
*/
static PatternElement *createPatternElement(
  char                *patternText,
  PatternElementKind   patternKind,
  PatternWordBoundary  wordBoundary,
  int                  caseInsensitive,
  int                  regularExpression)
{
    PatternElement *pattern;
    char *s;

    /*
     * Convert pattern text to lower case, if case insensitive
     * attribute is set.
     */
    if (caseInsensitive)
    {
        for (s = patternText; *s != '\0'; s ++)
        {
            *s = tolower(*s);
        }
    }

    /*
     * Allocate memory for the new pattern element and init. / copy
     * related info into this pattern element.
     */
    pattern = (PatternElement *)XtMalloc( sizeof(PatternElement) );

    initStrPatBackRefList(&pattern->peVal.peuSingle);

    pattern->peKind  = patternKind;
    pattern->peIndex = NO_PATTERN_IDX;
    pattern->peType  = PET_SINGLE;

    pattern->peVal.peuSingle.spLength            = strlen(patternText);
    pattern->peVal.peuSingle.spBackRefParsed     = False;
    pattern->peVal.peuSingle.spBackRefResolved   = False;

    pattern->peVal.peuSingle.spCaseInsensitive   = caseInsensitive;
    pattern->peVal.peuSingle.spRegularExpression = regularExpression;
    pattern->peVal.peuSingle.spTextRE            = NULL;

    /*
     * Store original string of regular expression patterns due to
     * it may be later adapted (e.g. due to global backrefs etc.).
     */
    if (regularExpression)
    {
        pattern->peVal.peuSingle.spOrigText     = patternText;
        pattern->peVal.peuSingle.spText         = NULL;
        pattern->peVal.peuSingle.spWordBoundary = PWB_NONE;
    }
    else
    {
        pattern->peVal.peuSingle.spOrigText     = NULL;
        pattern->peVal.peuSingle.spText         = patternText;
        pattern->peVal.peuSingle.spWordBoundary = wordBoundary;
    }

    return pattern;
}

/*
** Create a list holding all global backref definitions of given
** match pattern table element. The list is stored in this given
** element.
** Returns true, if list was successfully created.
*/
static int createGlobalBackRefList(
  ReadMatchPatternInfo     *readPatInfo,
  MatchPatternTableElement *element,
  ErrorInfo                *errInfo)
{
    int i;
    StringPattern *strPat;

    for (i=0; i<element->mpteAll.pesNumberOfPattern; i++)
    {
        strPat = getReadStringPattern(readPatInfo, element->mpteAll.pesPattern[i]);

        if (strPat->spRegularExpression)
        {
            if (strPat->spBackRefParsed)
            {
                /*
                 * Global backrefs or this string pattern already parsed:
                 * just merge string pattern list with elements one.
                 */
                if (!updateGlobalBackRefs(
                       strPat,
                       element->mpteGlobalBackRef,
                       errInfo))
                {
                    return False;
                }
            }
            else
            {
                /*
                 * parse string pattern for global backrefs and
                 * merge string pattern list with elements one.
                 */
                if (!parseGlobalBackRefs(
                       strPat,
                       element->mpteGlobalBackRef,
                       errInfo))
                {
                    return False;
                }
            }
        }
    }

    return True;
}

/*
** Returns read string pattern of given pattern element.
*/
StringPattern *getReadStringPattern(
  ReadMatchPatternInfo *readPatInfo,
  PatternElement       *pattern )
{
    switch (pattern->peType)
    {
      case PET_SINGLE:
          return &pattern->peVal.peuSingle;
          break;

      case PET_MULTIPLE:
          return &pattern->peVal.peuMulti.mpStringPattern;
          break;

      case PET_REFERENCE:
          return getReadStringPattern(
                   readPatInfo,
                   getReadPatternOfReference(readPatInfo, &pattern->peVal.peuRef));
          break;
    }

    /*
     * never reached; just to make compiler happy
     */
    return NULL;
}

/*
** Returns read pattern element of given pattern reference.
*/
static PatternElement *getReadPatternOfReference(
  ReadMatchPatternInfo *readPatInfo,
  PatternReference     *patRef)
{
    MatchPatternTableElement **element = readPatInfo->rmpiElement;

    return element[ patRef->prElementIdx ]->mpteAll.pesPattern[patRef->prPatternIdx];
}

/*
** Allocate a new copy of given string and substitute each capturing
** parentheses inside given string by a non-capturing one.
** Returns resulting string.
*/
static char *replaceCapturingParentheses(
  const char *source)
{
    char *destination;
    const char *s;
    char *d;
    int nbrOfOpenBraces = 0;

    s = source;

    /*
     * count number of open braces
     */
    while (*s != '\0')
    {
        if (*s++ == '(')
            nbrOfOpenBraces ++;
    }

    /*
     * allocate memory for substitued reg. exp. text
     */
    destination = XtMalloc(strlen(source) + 2*nbrOfOpenBraces);

    /*
     * substitute each capturing open brace by a non-capturing one
     */
    s = source;
    d = destination;

    while (*s != '\0')
    {
        if (*s == '\\')
        {
            *d++ = *s++;

            if (*s != '\0')
              *d++ = *s++;
        }
        else if (*s == '(')
        {
            *d++ = *s++;

            if (*s != '?' && *s != '*')
            {
                *d++ = '?';
                *d++ = ':';
            }
        }
        else
        {
            *d++ = *s++;
        }
    }

    *d = '\0';

    return destination;
}

/*
** Parse given string pattern for global backrefs definitions
** (syntax: "(*n", where n=1..9). Add found global backrefs to
** given backRefList.
** Returns false, if parse fails.
*/
static int parseGlobalBackRefs(
  StringPattern        *strPat,
  GlobalBackRefElement *backRefList,
  ErrorInfo            *errInfo)
{
    char *s;
    char *backRefContent;
    int  nestingLevel = 0;
    int  stackIdx = -1;
    BackRefBracketInfo backRefInfo[MAX_GLOBAL_BACK_REF_ID+1];
    StrPatBackRefElement *curStrPatBRE;

    strPat->spText          = replaceCapturingParentheses(strPat->spOrigText);
    strPat->spLength        = strlen(strPat->spText);
    strPat->spBackRefParsed = True;

    s = strPat->spText;

    while (*s != '\0')
    {
        if (*s == '\\')
        {
            /*
             * Ignore escaped characters
             */
            if (*(s+1) != '\0')
                s ++;
        }
        else if (*s == '(')
        {
            if (*(s+1) == '*')
            {
                if (isdigit((unsigned char)*(s+2)))
                {
                    /*
                     * Global backref. definition start found:
                     */
                    stackIdx ++;

                    backRefInfo[stackIdx].brbiGlobalId =
                      (int)((unsigned char)*(s+2) - (unsigned char)'0') - 1;

                    if(backRefInfo[stackIdx].brbiGlobalId < 0)
                    {
                        errInfo->eiStringPatText = XtNewString(strPat->spOrigText);
                        errInfo->eiDetail        = "Backref '0' not allowed";
                        return False;
                    }

                    backRefInfo[stackIdx].brbiContentStart = s+3;
                    backRefInfo[stackIdx].brbiNestingLevel = nestingLevel;
                    s ++;
                }
                s ++;
            }
            nestingLevel ++;
        }
        else if (*s == ')')
        {
            nestingLevel --;
            if (stackIdx != -1 &&
                backRefInfo[stackIdx].brbiNestingLevel == nestingLevel)
            {
                /*
                 * Global backref. definition end found: add it to
                 * backref. list of string pattern.
                 */
                curStrPatBRE =
                  &strPat->spOwnGlobalBackRef[backRefInfo[stackIdx].brbiGlobalId];

                backRefContent =
                  createBackRefRegExpText(
                    backRefInfo[stackIdx].brbiContentStart,
                    s);

                if (curStrPatBRE->spbreRegExpText != NULL)
                {
                    errInfo->eiStringPatText = XtNewString(strPat->spOrigText);
                    errInfo->eiBackRefNbr    = backRefInfo[stackIdx].brbiGlobalId + 1;
                    errInfo->eiDetail        = "already defined before";
                    XtFree(backRefContent);
                    return False;
                }
                else
                {
                    curStrPatBRE->spbreRegExpText = backRefContent;
                }

                stackIdx --;
            }
        }
        s ++;
    }

    /*
     * Merge global backref. list of string pattern with given backRefList.
     */
    return updateGlobalBackRefs(
             strPat,
             backRefList,
             errInfo);
}

/*
** Merge global backref. list of given string pattern with given backRefList.
** Returns false, if merge fails.
*/
static int updateGlobalBackRefs(
  StringPattern        *strPat,
  GlobalBackRefElement *backRefList,
  ErrorInfo            *errInfo)
{
    int i;
    StrPatBackRefElement *curStrPatBRE;
    GlobalBackRefElement *curGlobalBRE;

    for (i=0;i < MAX_GLOBAL_BACK_REF_ID; i ++)
    {
        curStrPatBRE = &strPat->spOwnGlobalBackRef[i];

        if (curStrPatBRE->spbreRegExpText != NULL)
        {
            curGlobalBRE = &backRefList[i];

            if (curGlobalBRE->gbreDefByStrPat != NULL)
            {
                if (strcmp(curGlobalBRE->gbreRegExpText, curStrPatBRE->spbreRegExpText) != 0)
                {
                    errInfo->eiStringPatText = XtNewString(strPat->spOrigText);
                    errInfo->eiBackRefNbr = i+1;
                    errInfo->eiDetail     = "already defined before";

                    return False;
                }
            }
            else
            {
                curGlobalBRE->gbreDefByStrPat = strPat;
                curGlobalBRE->gbreRegExpText  = curStrPatBRE->spbreRegExpText;
            }
        }
    }

    return True;
}

/*
** Allocate and return a new string holding content of
** global backref. definition.
*/
static char *createBackRefRegExpText(
  const char *start,
  const char *end)
{
    int len = end - start;
    char *regExpText = XtMalloc( len+1 );

    memcpy( regExpText, start, len );

    regExpText[len] = '\0';

    return regExpText;
}

/*
** Resolve all global backrefs of given match pattern table element.
** Returns false, if resolve fails.
*/
static int resolveGlobalBackRefs(
  ReadMatchPatternInfo     *readPatInfo,
  MatchPatternTableElement *element,
  ErrorInfo                *errInfo)
{
    int i;
    StringPattern *strPat;

    for (i=0; i<element->mpteAll.pesNumberOfPattern; i++)
    {
        strPat = getReadStringPattern(readPatInfo, element->mpteAll.pesPattern[i]);

        if (strPat->spRegularExpression && !strPat->spBackRefResolved)
        {
            if (!resolveGlobalBackRefsOfStrPat(strPat, element->mpteGlobalBackRef, errInfo))
                return False;

            strPat->spBackRefResolved = True;
        }
    }

    return True;
}

/*
** Resolve global backrefs of given string pattern.
** Returns false, if resolve fails.
*/
static int resolveGlobalBackRefsOfStrPat(
  StringPattern        *strPat,
  GlobalBackRefElement *backRefList,
  ErrorInfo            *errInfo)
{
    char *s;
    int globalId;
    int localId = 1;

    s = strPat->spText;

    while (*s != '\0')
    {
        if (*s == '\\')
        {
            if (isdigit((unsigned char)*(s+1)))
            {
                /*
                 * \n (n=1..9) found: substitute global backref.
                 */
                globalId =
                  (int)((unsigned char)*(s+1) - (unsigned char)'0') - 1;

                if(globalId < 0)
                {
                    errInfo->eiStringPatText = XtNewString(strPat->spOrigText);
                    errInfo->eiDetail = "backref '\\0' not allowed";
                    return False;
                }

                s = substituteGlobalBackRef(strPat, s, globalId, &localId, backRefList, errInfo);

                if (s == NULL)
                    return False;
            }
            else if (*(s+1) != '\0')
                s ++;
        }
        else if (*s == '(')
        {
            if (*(s+1) == '*')
            {
                if (isdigit((unsigned char)*(s+2)))
                {
                    /*
                     * "(*n" (n=1..9) found: substitute global backref. definition.
                     */
                    globalId =
                      (int)((unsigned char)*(s+2) - (unsigned char)'0') - 1;

                    strPat->spOwnGlobalBackRef[globalId].spbreLocalBackRefID = localId;
                    strPat->spGlobalToLocalBackRef[globalId]                 = localId;

                    localId ++;

                    s = convertGlobalToLocalBackRef(strPat, s);
                }
                else
                {
                    s ++;
                }
            }
        }
        s ++;
    }

    return True;
}

/*
** Substitute global backref (\n, n=1..9) located at given "subsPtr"
** by its definition or by a local backref.
** Returns
** - NULL, if substitute fails or
** - substituted string pointer, where scan shall continue with.
*/
static char *substituteGlobalBackRef(
  StringPattern        *strPat,
  char                 *subsPtr,
  int                   globalId,
  int                  *localId,
  GlobalBackRefElement *backRefList,
  ErrorInfo            *errInfo)
{
    StrPatBackRefElement *strPatBackRef = &strPat->spOwnGlobalBackRef[globalId];
    char *s;

    if (strPatBackRef->spbreRegExpText == NULL)
    {
        /*
         * given global backref definition is not located in given
         * string pattern -> replace backref ID by backref reg. exp.
         */
        if (backRefList[globalId].gbreRegExpText == NULL)
        {
            errInfo->eiStringPatText = XtNewString(strPat->spOrigText);
            errInfo->eiBackRefNbr    = globalId + 1;
            errInfo->eiDetail        = "not defined within any string pattern";
            return NULL;
        }

        if (strPat->spGlobalToLocalBackRef[globalId] == NO_LOCAL_BACK_REF_ID)
        {
            /*
             * 1st occurence of global backref ID in this pattern ->
             * replace global backref ID by backref reg. exp.
             */
            s = replaceBackRefIdByRegExp(strPat, subsPtr, backRefList[globalId].gbreRegExpText);

            strPat->spGlobalToLocalBackRef[globalId] = *localId;
            (*localId) ++;
        }
        else
        {
            /*
             * next occurence of global backref ID in this pattern ->
             * replace global backref ID by local one
             */
            s = subsPtr + 1;
            *s = (char)((int)('0') + strPat->spGlobalToLocalBackRef[globalId]);
        }
    }
    else
    {
        /*
         * given global backref definition is located in given string pattern
         */
        if (strPatBackRef->spbreLocalBackRefID == NO_LOCAL_BACK_REF_ID)
        {
            errInfo->eiStringPatText = XtNewString(strPat->spOrigText);
            errInfo->eiBackRefNbr    = globalId + 1;
            errInfo->eiDetail        = "not defined before";
            return NULL;
        }

        /*
         * replace global backref ID by local one
         */
        s = subsPtr + 1;
        *s = (char)((int)('0') + strPatBackRef->spbreLocalBackRefID);
    }

    return s;
}

/*
** Replace global backref ID ("\n", n=1..9), located at given
** replaceStartPtr, by its definition (given by regExp parameter).
** Returns string pointer, where scan shall continue with
*/
static char *replaceBackRefIdByRegExp(
  StringPattern  *strPat,
  char           *replaceStartPtr,
  char           *regExp)
{
    char *oldText = strPat->spText;
    char *newText;
    char *n;
    char *continueScanPtr;
    int oldLen = strlen(oldText);
    int regExpLen = strlen(regExp);
    int replacePos = replaceStartPtr - oldText;
    int remainingLen = oldLen-replacePos-2;

    /*
     * replace "\n" - located at replaceStartPtr - by "(regExp)"
     */
    newText = XtMalloc(oldLen + regExpLen + 3);

    memcpy(newText, oldText, replacePos);
    n = newText + replacePos;
    *n = '(';
    continueScanPtr = n;
    n ++;
    memcpy(n, regExp, regExpLen);
    n += regExpLen;
    *n = ')';
    n ++;
    memcpy(n, replaceStartPtr+2, remainingLen);
    *(n + remainingLen) = '\0';

    XtFree(oldText);

    strPat->spText   = newText;
    strPat->spLength = strlen(newText);

    return continueScanPtr;
}

/*
** Convert global backref definition ("(*n", n=1..9), located at given
** convertPtr, by capturing parentheses "(".
** Returns string pointer, where scan shall continue with
*/
static char *convertGlobalToLocalBackRef(
  StringPattern  *strPat,
  char           *convertPtr)
{
    char *oldText = strPat->spText;
    char *newText;
    int oldLen = strlen(oldText);
    int convertPos = convertPtr - oldText;

    /*
     * replace "(*n" - located at convertPtr - by "("
     */
    newText = XtMalloc(oldLen - 1);

    memcpy(newText, oldText, convertPos+1);
    memcpy(newText+convertPos+1, convertPtr+3, oldLen-convertPos-3);

    *(newText + oldLen - 2) = '\0';

    XtFree(oldText);

    strPat->spText   = newText;
    strPat->spLength = strlen(newText);

    return newText + convertPos;
}

/*
** Read a match pattern table element from given input string.
** Return NULL, if read fails.
*/
static MatchPatternTableElement *readMatchPatternTableElement(
  char             **inPtr,
  char             **errMsg,
  char              *name,
  MatchPatternType   type)
{
    int error = False;
    PatternElement *pattern;
    PatternElement *allPat[MAX_STRING_PATTERNS];
    int nbrOfPat = 0;
    int sizeOfPat;
    MatchPatternTableElement *result;
    int isMonoPattern;
    int skipBtwStartEnd;
    int flash;
    int ignoreHighLightInfo;
    int i;

    if (!getMPSGlobalAttribute(
          inPtr,
          errMsg,
          &isMonoPattern,
          &skipBtwStartEnd,
          &flash,
          &ignoreHighLightInfo ))
    {
        return NULL;
    }

    /*
     * read all patterns
     */
    while (**inPtr != '\n' && !error)
    {
        if (!readPatternElement( inPtr, errMsg, &pattern ))
        {
            error = True;
        }
        else if (nbrOfPat >= MAX_STRING_PATTERNS)
        {
            *errMsg = "max. number of string patterns exceeded";
            error = True;
        }
        else
        {
            pattern->peIndex = nbrOfPat;

            allPat[nbrOfPat ++] = pattern;
        }
    }

    if (error)
    {
        for (i=0; i < nbrOfPat; i ++)
          freePatternElement( allPat[i] );

        return NULL;
    }

    if (nbrOfPat == 0)
    {
        *errMsg = "min. one string pattern needed";
        return NULL;
    }

    /*
     * allocate & init. MatchPatternTableElement
     */
    result =
      (MatchPatternTableElement *)XtMalloc(sizeof(MatchPatternTableElement));

    result->mpteName  = name;
    result->mpteIndex = NO_ELEMENT_IDX;
    result->mpteType  = type;
    result->mpteGroup = NO_GROUP_IDX;

    sizeOfPat = sizeof(PatternElement *) * nbrOfPat;
    result->mpteAll.pesPattern = (PatternElement **)XtMalloc( sizeOfPat );

    memcpy(result->mpteAll.pesPattern, allPat, sizeOfPat);

    result->mpteAll.pesNumberOfPattern = nbrOfPat;

    result->mpteIsMonoPattern       = isMonoPattern;
    result->mpteSkipBtwnStartEnd    = skipBtwStartEnd;
    result->mpteFlash               = flash;
    result->mpteIgnoreHighLightInfo = ignoreHighLightInfo;

    result->mpteStartEndRE = NULL;

    initGlobalBackRefList( result->mpteGlobalBackRef );

    /*
     * sort start / end / middle pattern
     */
    error = !sortReadPatternElementSet( &result->mpteAll, errMsg, result );

    if (error)
    {
        freeMatchPatternTableElement( result );
        return NULL;
    }
    else
    {
        return result;
    }
}

/*
 * Sort read pattern element set into start, middle & end arrays.
 * Validate "monopattern" attribute.
 * Returns true, if validation was successful.
 */
static int sortReadPatternElementSet(
  PatternElementSet         *allPat,
  char                     **errMsg,
  MatchPatternTableElement  *result)
{
    int sizeOfPat;
    int isMonoPattern = result->mpteIsMonoPattern;

    /*
     * count number of start, middle & end pattern elements.
     */
    countPatternElementKind( allPat, result );

    /*
     * validate and allocate pattern elements.
     */
    if (result->mpteStart.pesNumberOfPattern != 0)
    {
        sizeOfPat = sizeof(PatternElement *) * result->mpteStart.pesNumberOfPattern;
        result->mpteStart.pesPattern = (PatternElement **)XtMalloc( sizeOfPat );
    }
    else
    {
        *errMsg = "min. one start pattern needed";
         return False;
    }

    if (isMonoPattern &&
        (result->mpteMiddle.pesNumberOfPattern != 0 ||
         result->mpteEnd.pesNumberOfPattern !=0))
    {
        *errMsg = "mono pattern: only start pattern(s) allowed due to attribute [m]";
         return False;
    }

    if (result->mpteMiddle.pesNumberOfPattern != 0)
    {
        sizeOfPat = sizeof(PatternElement *) * result->mpteMiddle.pesNumberOfPattern;
        result->mpteMiddle.pesPattern = (PatternElement **)XtMalloc( sizeOfPat );
    }

    if (result->mpteEnd.pesNumberOfPattern != 0)
    {
        sizeOfPat = sizeof(PatternElement *) * result->mpteEnd.pesNumberOfPattern;
        result->mpteEnd.pesPattern = (PatternElement **)XtMalloc( sizeOfPat );
    }
    else
    {
        if (!isMonoPattern)
        {
            *errMsg = "min. one end pattern needed";
            return False;
        }
    }

    /*
     * sort pattern elements into start, middle & end arrays.
     */
    sortPatternElementSet( allPat, result );

    if (isMonoPattern)
    {
        copyPatternSet( &result->mpteStart, &result->mpteEnd );
    }

    return True;
}

/*
 * Count number of start, middle & end patterns stored in "allPat".
 */
static void countPatternElementKind(
  PatternElementSet         *allPat,
  MatchPatternTableElement  *result)
{
    int i;

    result->mpteStart.pesNumberOfPattern  = 0;
    result->mpteEnd.pesNumberOfPattern    = 0;
    result->mpteMiddle.pesNumberOfPattern = 0;

    result->mpteStart.pesPattern  = NULL;
    result->mpteEnd.pesPattern    = NULL;
    result->mpteMiddle.pesPattern = NULL;

    for (i=0; i < allPat->pesNumberOfPattern; i ++)
    {
        switch (allPat->pesPattern[i]->peKind)
        {
            case PEK_START:
                result->mpteStart.pesNumberOfPattern ++;
                break;
            case PEK_MIDDLE:
                result->mpteMiddle.pesNumberOfPattern ++;
                break;
            case PEK_END:
                result->mpteEnd.pesNumberOfPattern ++;
                break;
            default:;
        }
    }
}

/*
 * Sort start, middle & end pattern elements into related arrays.
 */
static void sortPatternElementSet(
  PatternElementSet         *allPat,
  MatchPatternTableElement  *result)
{
    int i;
    int s,m,e;

    for (i=0, s=0, m=0, e=0; i < allPat->pesNumberOfPattern; i ++)
    {
        switch (allPat->pesPattern[i]->peKind)
        {
            case PEK_START:
                result->mpteStart.pesPattern[s ++] = allPat->pesPattern[i];
                break;
            case PEK_MIDDLE:
                result->mpteMiddle.pesPattern[m ++] = allPat->pesPattern[i];
                break;
            case PEK_END:
                result->mpteEnd.pesPattern[e ++] = allPat->pesPattern[i];
                break;
            default:;
        }
    }
}

static void copyPatternSet(
  PatternElementSet *sourcePS,
  PatternElementSet *destPS)
{
    int sizeOfPat;

    destPS->pesNumberOfPattern = sourcePS->pesNumberOfPattern;

    sizeOfPat = sizeof(PatternElement *) * destPS->pesNumberOfPattern;
    destPS->pesPattern = (PatternElement **)XtMalloc( sizeOfPat );

    memcpy(destPS->pesPattern, sourcePS->pesPattern, sizeOfPat);
}

/*
** Free the allocated memory contained in a ReadMatchPatternInfo data structure
*/
static void freeReadMatchPatternInfo( ReadMatchPatternInfo *readPatInfo )
{
    int i;

    for (i=0; i<readPatInfo->rmpiNbrOfElements; i++)
        freeMatchPatternTableElement(readPatInfo->rmpiElement[i]);

    for (i=0; i<readPatInfo->rmpiNbrOfGroups; i++)
        freeMatchPatternGroupElement(readPatInfo->rmpiGroup[i]);

    for (i=0; i<readPatInfo->rmpiNbrOfSeqElements; i++)
        freeMatchPatternSequenceElement(readPatInfo->rmpiSequence[i]);

    freePtr((void **)&readPatInfo->rmpiAllPatRE);

    freePtr((void **)&readPatInfo->rmpiFlashPatRE);
}

/*
** Free the allocated memory contained in a StringMatchTable data structure
*/
static void freeStringMatchTable( StringMatchTable *table )
{
    MatchPatternTable *patTable;
    int i;

    if (table == NULL)
        return;

    XtFree(table->smtLanguageMode);

    /*
     * Free all matching patterns
     */
    patTable = table->smtAllPatterns;

    for (i=0; i<patTable->mptNumberOfElements; i++)
        freeMatchPatternTableElement(patTable->mptElements[i]);

    XtFree((char *)patTable);

    /*
     * Free matching pattern group elements
     */
    for (i=0; i<table->smtNumberOfGroups; i++)
        freeMatchPatternGroupElement(table->smtGroups[i]);

    /*
     * Free matching pattern sequence elements
     */
    for (i=0; i<table->smtNumberOfSeqElements; i++)
        freeMatchPatternSequenceElement(table->smtSequence[i]);

    /*
     * Free keyword reg. expressions
     */
    freePtr((void **)&table->smtAllPatRE);

    freePtr((void **)&table->smtFlashPatRE);

    XtFree((char *)table);
}

/*
** Free the allocated memory contained in a MatchPatternTableElement data structure
*/
static void freeMatchPatternTableElement( MatchPatternTableElement *element )
{
    int i;

    XtFree(element->mpteName);

    for (i=0; i<element->mpteAll.pesNumberOfPattern; i++)
    {
        freePatternElement(element->mpteAll.pesPattern[i]);
    }

    freePtr((void **)&element->mpteStartEndRE);

    freeXtPtr((void **)&element->mpteStart.pesPattern);

    freeXtPtr((void **)&element->mpteMiddle.pesPattern);

    freeXtPtr((void **)&element->mpteEnd.pesPattern);

    XtFree((char *)element);
}

/*
** Free the allocated memory contained in a PatternElement data structure
*/
static void freePatternElement( PatternElement *element )
{
    if (element->peType == PET_SINGLE)
        freeStringPattern( &(element->peVal.peuSingle) );
    else if (element->peType == PET_MULTIPLE)
    {
        freeStringPattern( &(element->peVal.peuMulti.mpStringPattern) );
        XtFree( (char *)element->peVal.peuMulti.mpRefList );
    }

    XtFree( (char *)element );
}

/*
** Free the allocated memory contained in a StringPattern data structure
*/
static void freeStringPattern( StringPattern *strPat )
{
    int i;

    freeXtPtr((void **)&strPat->spText);
    freeXtPtr((void **)&strPat->spOrigText);

    freePtr((void **)&strPat->spTextRE);

    for (i=0; i < MAX_GLOBAL_BACK_REF_ID; i++)
        freeXtPtr((void **)&strPat->spOwnGlobalBackRef[i].spbreRegExpText);
}

/*
** Free the allocated memory contained in a MatchPatternGroupElement data structure
*/
static void freeMatchPatternGroupElement( MatchPatternGroupElement *group )
{
    int i;

    freeXtPtr((void **)&group->mpgeName);

    freePtr((void **)&group->mpgeKeywordRE);

    if (group->mpgeSubPatternIds != NULL)
    {
        for (i=0; i < group->mpgeNumberOfSubPatterns; i++)
        {
            XtFree(group->mpgeSubPatternIds[i]);
        }
        XtFree((char *)group->mpgeSubPatternIds);
    }

    XtFree((char *)group);
}

/*
** Free the allocated memory contained in a MatchPatternSequenceElement data structure
*/
static void freeMatchPatternSequenceElement( MatchPatternSequenceElement *sequence )
{
    XtFree( sequence->mpseName );

    XtFree( (char *)sequence );
}


/*
** Format a matching pattern set parse error.
*/
static void parseMatchingPatternSetError(
  const char *stringStart,
  const char *stoppedAt,
  ErrorInfo  *errInfo)
{
    char *message = "";
    int   msgLen;
    char *errorInForm = "matching pattern of \"%s\"";
    char *errorIn;

    if (errInfo->eiLanguageMode == NULL)
    {
        errorIn = "matching pattern";
    }
    else
    {
        errorIn = XtMalloc(strlen(errorInForm) + strlen(errInfo->eiLanguageMode)+1);
        sprintf(errorIn, "matching pattern of \"%s\"", errInfo->eiLanguageMode);
    }

    if (errInfo->eiRegExpCompileMsg != NULL)
    {
        /*
         * Error message of form:
         * "MP \"eiMPTabElementName\", SP \"eiStringPatText\": eiRegExpCompileMsg" or
         * "MP \"eiMPTabElementName\" - eiDetail: eiRegExpCompileMsg"
         */

        msgLen = strlen(errInfo->eiRegExpCompileMsg) + 1;

        if (errInfo->eiMPTabElementName != NULL)
            msgLen += strlen(errInfo->eiMPTabElementName) + 10;

        if (errInfo->eiDetail != NULL)
        {
            msgLen += strlen(errInfo->eiDetail + 2);
        }
        else
        {
            if (errInfo->eiStringPatText != NULL)
                msgLen += strlen(errInfo->eiStringPatText) + 9;
        }

        message = XtMalloc(msgLen);

        strcpy(message, "");

        if (errInfo->eiMPTabElementName != NULL)
            sprintf( message, "MP \"%s\"", errInfo->eiMPTabElementName);

        if (errInfo->eiDetail == NULL)
        {
            if (errInfo->eiStringPatText != NULL)
                sprintf( message, "%s, SP \"%s\"", message, errInfo->eiStringPatText);
        }
        else
        {
            if (strlen(message) != 0)
                strcat(message, " - ");

            strcat(message, errInfo->eiDetail);
        }

        if (strlen(message) != 0)
            strcat(message, ": ");

        strcat(message, errInfo->eiRegExpCompileMsg);
    }
    else if (errInfo->eiDetail != NULL)
    {
        /*
         * Error message of form:
         * "MP \"eiMPTabElementName\", SP \"eiStringPatText\": Backref %d eiDetail
         */
        msgLen = strlen(errInfo->eiDetail) + 1;

        if (errInfo->eiMPTabElementName != NULL)
            msgLen += strlen(errInfo->eiMPTabElementName) + 7;
        if (errInfo->eiStringPatText != NULL)
            msgLen += strlen(errInfo->eiStringPatText) + 9;
        if (errInfo->eiBackRefNbr != 0)
            msgLen += 15;

        message = XtMalloc(msgLen);

        strcpy(message, "");

        if (errInfo->eiMPTabElementName != NULL)
            sprintf( message, "MP \"%s\"", errInfo->eiMPTabElementName);
        if (errInfo->eiStringPatText != NULL)
            sprintf( message, "%s, SP \"%s\"", message, errInfo->eiStringPatText);

        if (strlen(message) != 0)
            strcat(message, ": ");

        if (errInfo->eiBackRefNbr != 0)
            sprintf( message, "%s Backref %d ", message, errInfo->eiBackRefNbr);

        strcat(message, errInfo->eiDetail);
    }

    ParseError(NULL, stringStart, stoppedAt, errorIn, message);

    if (errInfo->eiRegExpCompileMsg != NULL || errInfo->eiDetail != NULL)
    {
        XtFree(message);
    }

    if (errInfo->eiLanguageMode != NULL)
    {
        XtFree(errorIn);
    }

    freeErrorInfo(errInfo);
    initErrorInfo(errInfo);
}

/*
 * Pop-up a warning dialog showing a matching pattern set error.
 */
static void dialogMatchingPatternSetError(
  char       *title,
  ErrorInfo  *errInfo)
{
    char *message;
    int   msgLen = 1;

    /*
     * Error message of form:
     * "Name  : \"eiMPTabElementName\"\n
     * "String: \"eiStringPatText\"\n
     * eiDetail\n
     * eiRegExpCompileMsg\n"
     */

    if (errInfo->eiMPTabElementName != NULL)
        msgLen += strlen(errInfo->eiMPTabElementName) + 15;
    if (errInfo->eiStringPatText != NULL)
        msgLen += strlen(errInfo->eiStringPatText) + 15;
    if (errInfo->eiDetail != NULL)
        msgLen += strlen(errInfo->eiDetail) + 15;
    if (errInfo->eiBackRefNbr != 0)
        msgLen += 15;
    if (errInfo->eiRegExpCompileMsg != NULL)
        msgLen += strlen(errInfo->eiRegExpCompileMsg) + 15;

    message = XtMalloc(msgLen);

    strcpy(message, "");

    if (errInfo->eiMPTabElementName != NULL)
        sprintf( message, "%sName   : \"%s\"\n", message, errInfo->eiMPTabElementName);
    if (errInfo->eiStringPatText != NULL)
        sprintf( message, "%sPattern: \"%s\"\n", message, errInfo->eiStringPatText);
    if (errInfo->eiBackRefNbr != 0)
        sprintf( message, "%sBackref %d ", message, errInfo->eiBackRefNbr);
    if (errInfo->eiDetail != NULL)
        sprintf( message, "%s%s\n", message, errInfo->eiDetail);
    if (errInfo->eiRegExpCompileMsg != NULL)
        sprintf( message, "%s%s\n", message, errInfo->eiRegExpCompileMsg);

    DialogF(
      DF_WARN, MatchPatternDialog.mpdShell, 1,
      title,
      "%s(language mode '%s')",
      "OK",
      message,
      errInfo->eiLanguageMode);

    XtFree(message);

    freeErrorInfo(errInfo);
    initErrorInfo(errInfo);
}

/*
** Get matching pattern set name.
** Syntax:
** patternName ::= "name:"
** Returns true, if get was successful.
*/
static int getMPSName(
  char      **inPtr,
  ErrorInfo  *errInfo,
  char      **name )
{
    char *dummy;
    char *field = ReadSymbolicField(inPtr);

    if (field == NULL)
    {
        errInfo->eiDetail = "matching pattern name missing";
        return False;
    }

    if (!SkipDelimiter(inPtr, &dummy))
    {
        errInfo->eiMPTabElementName = XtNewString(field);
        errInfo->eiDetail = "':' missing after matching pattern name";
        XtFree( field );
        return False;
    }

    *name = field;

    return True;
}

/*
** Get matching pattern set type attribute.
** TypeAttribute ::=
** [s|g]:
**
**   s  : sub-pattern (pattern is only matched, if part of a pattern group).
**   g  : pattern (context) group (i.e. a sequence of sub-patterns).
**   default: individual pattern (pattern is not part of a group and is
**            matched individually.
** Returns true, if get was successful.
*/
static int getMPSTypeAttribute(
  char             **inPtr,
  ErrorInfo         *errInfo,
  MatchPatternType  *type)
{
    char *field = ReadSymbolicField(inPtr);
    int successful = True;

    *type = MPT_INDIVIDUAL;

    if (field != NULL)
    {
        switch (*field)
        {
            case 'g':
                *type = MPT_GROUP;
                break;
            case 's':
                *type = MPT_SUB;
                break;
            default:
                errInfo->eiDetail = "unknown matching pattern type attribute";
                successful = False;
        }
    }

    if (successful)
    {
        if (!SkipDelimiter(inPtr, &errInfo->eiDetail))
        {
            successful = False;
        }
    }

    freeXtPtr((void **)&field);

    return successful;
}

/*
** Syntax:
**
** GlobalAttribute ::=
** [c][f][m][p][u]:
**
** c  : the content between start and end pattern is skipped
**      during parsing (e.g. pattern encloses a comment).
** f  : flash matching pattern (if not set, then only jump
**      to matching pattern is supported).
** m  : mono pattern - set exist out of only one single pattern
**      (start pattern = end pattern; e.g. quotes like ")
** p  : ignore highlight info code of single patterns of this set
**      ("plain").
**
** Returns TRUE, if global attribute was successful read.
*/
static int getMPSGlobalAttribute(
  char **inPtr,
  char **errMsg,
  int   *isMonoPattern,
  int   *comment,
  int   *flash,
  int   *ignoreHighLightInfo)
{
    char *field = ReadSymbolicField(inPtr);
    char *attribute;
    int successful = True;

    *isMonoPattern       = False;
    *comment             = False;
    *flash               = False;
    *ignoreHighLightInfo = False;

    if (field != NULL)
    {
        attribute = field;
        while (*attribute != '\0' && successful)
        {
            switch (*attribute)
            {
                case 'c':
                    *comment = True;
                    break;
                case 'f':
                    *flash = True;
                    break;
                case 'm':
                    *isMonoPattern = True;
                    break;
                case 'p':
                    *ignoreHighLightInfo = True;
                    break;
                default:
                    *errMsg = "unknown global attribute";
                    successful = False;
            }
            attribute ++;
        }
    }

    if (successful)
    {
        if (!SkipDelimiter(inPtr, errMsg))
        {
            successful = False;
        }
    }

    freeXtPtr((void **)&field);

    return successful;
}

/*
** Get matching pattern set attribute.
**
** Syntax:
**
** patternAttribute ::=
** [s|m|e][w|l|r][i]:
**
** StringPatternKind:
**   s  : start string pattern.
**   m  : middle string pattern.
**   e  : end string pattern.
** WordBoundaryAttribute:
**   w  : pattern is word (i.e. before and after pattern
**        there must be a delimiter).
**   l  : before pattern must be a delimiter (left side).
**   r  : after pattern must be a delimiter (right side).
**   default: neither before nor after pattern must be a delimiter.
** StringAttribute:
**   i  : pattern is case insensitive (if not set: pattern is
**        case sensitive).
**   x  : pattern is regular expression (if not set: pattern is
**        literal string).
**
** Returns TRUE, if pattern attribute was successful read.
*/
static int getMPSPatternAttribute(
  char                **inPtr,
  char                **errMsg,
  PatternElementKind   *patternKind,
  PatternWordBoundary  *wordBoundary,
  int                  *caseInsensitive,
  int                  *regularExpression)
{
    char *field = ReadSymbolicField(inPtr);
    char *attribute;
    int successful = True;

    *patternKind       = PEK_UNKNOWN;
    *wordBoundary      = PWB_NONE;
    *caseInsensitive   = False;
    *regularExpression = False;

    if (field != NULL)
    {
        attribute = field;
        while (*attribute != '\0' && successful)
        {
            switch (*attribute)
            {
                case 'e':
                    *patternKind = PEK_END;
                    break;
                case 'i':
                    *caseInsensitive = True;
                    break;
                case 'l':
                    *wordBoundary = PWB_LEFT;
                    break;
                case 'm':
                    *patternKind = PEK_MIDDLE;
                    break;
                case 'r':
                    *wordBoundary = PWB_RIGHT;
                    break;
                case 's':
                    *patternKind = PEK_START;
                    break;
                case 'w':
                    *wordBoundary = PWB_BOTH;
                    break;
                case 'x':
                    *regularExpression = True;
                    break;
                default:
                    *errMsg = "unknown string pattern attribute";
                    successful = False;
                }
                attribute ++;
        }
    }

    if (successful)
    {
        if (!SkipDelimiter(inPtr, errMsg))
        {
            successful = False;
        }
    }

    freeXtPtr((void **)&field);

    return successful;
}

/*
** Returns the (to be reserved) reg. ex. length of an pattern element.
** Update total number of multi patterns, too.
*/
static int patternElementLen(
  ReadMatchPatternInfo *info,
  PatternElement       *patElement,
  int                  *nbrOfMultiPatterns)
{
    PatternElement *referredElement;
    StringPattern *strPat = NULL;
    int patElementLen;

    switch (patElement->peType)
    {
        case PET_SINGLE:
            strPat = &patElement->peVal.peuSingle;
            break;

        case PET_MULTIPLE:
            strPat = &patElement->peVal.peuMulti.mpStringPattern;

            (*nbrOfMultiPatterns) ++;

            break;

        case PET_REFERENCE:
            referredElement =
              info->rmpiElement[patElement->peVal.peuRef.prElementIdx]->
                mpteAll.pesPattern[patElement->peVal.peuRef.prPatternIdx];

            strPat = &referredElement->peVal.peuMulti.mpStringPattern;
            break;
    }

    /*
     * reserve additional 4 characters ("(?i)") for case insensitive search
     */
    if (strPat->spCaseInsensitive)
        patElementLen = strPat->spLength + 4;
    else
        patElementLen = strPat->spLength;

    /*
     * reserve additional 4 characters ("(?:)") for regular expression
     */
    if (strPat->spRegularExpression)
        patElementLen += 4;

    return patElementLen;
}

/*
** Returns the (to be reserved) total reg. ex. length of given
** MatchPatternTableElement. Update total number of multi patterns, too.
*/
static int totalMatchPatternTableElementLen(
  ReadMatchPatternInfo     *info,
  MatchPatternTableElement *element,
  int                      *nbrOfMultiPatterns)
{
    int i;
    int totalLen = 0;

    for (i=0; i<element->mpteAll.pesNumberOfPattern; i ++)
    {
        totalLen +=
          patternElementLen(
            info,
            element->mpteAll.pesPattern[i],
            nbrOfMultiPatterns );
    }

    return totalLen;
}

/*
** Returns the (to be reserved) total reg. ex. length of given
** read match pattern info. Counts total number of multi patterns, too.
*/
static int totalKeywordOfTableLen(
  ReadMatchPatternInfo *info,
  int                  *nbrOfMultiPatterns)
{
    int i;
    int totalLen = 0;

    *nbrOfMultiPatterns = 0;

    for (i=0; i<info->rmpiNbrOfElements; i ++)
    {
        totalLen +=
          totalMatchPatternTableElementLen(
            info,
            info->rmpiElement[i],
            nbrOfMultiPatterns);
    }

    return totalLen;
}

/*
** Add given StringPattern to given reg. ex. strings
*/
static void addPatternToRegExpString(
  StringPattern    *pattern,
  RegExpStringInfo *regExpStringInfo)
{
    char *r = NULL, *s;
    char *adaptedRegExpText = NULL;
    int prevLen = 0;

    /*
     * Select the buffer, where the pattern needs to be added
     * to (depending on its word boundary). Prepare the buffer
     * by evtl. adding a prefix, if related buffer is empty.
     */
    switch (pattern->spWordBoundary)
    {
        case PWB_NONE:
            prevLen = strlen( regExpStringInfo->resiNoneWBRegExpString );

            r = regExpStringInfo->resiNoneWBRegExpString + strlen( regExpStringInfo->resiNoneWBRegExpString );
            break;
        case PWB_LEFT:
            prevLen = strlen( regExpStringInfo->resiLeftWBRegExpString );
            if (prevLen == 0)
            {
                strcpy( regExpStringInfo->resiLeftWBRegExpString, "<(?:" );
            }
            r = regExpStringInfo->resiLeftWBRegExpString + strlen( regExpStringInfo->resiLeftWBRegExpString );
            break;
        case PWB_RIGHT:
            prevLen = strlen( regExpStringInfo->resiRightWBRegExpString );
            if (prevLen == 0)
            {
                strcpy( regExpStringInfo->resiRightWBRegExpString, "(?:" );
            }
            r = regExpStringInfo->resiRightWBRegExpString + strlen( regExpStringInfo->resiRightWBRegExpString );
            break;
        case PWB_BOTH:
            prevLen = strlen( regExpStringInfo->resiBothWBRegExpString );
            if (prevLen == 0)
            {
                strcpy( regExpStringInfo->resiBothWBRegExpString, "<(?:" );
            }
            r = regExpStringInfo->resiBothWBRegExpString + strlen( regExpStringInfo->resiBothWBRegExpString );
            break;
    }

    /*
     * add an "or" if there is already a pattern in the buffer
     */
    if (prevLen != 0)
    {
        *r++ = '|';
    }

    /*
     * add "(?i" to order case insensitive search
     */
    if (pattern->spCaseInsensitive)
    {
        *r++ = '(';
        *r++ = '?';
        *r++ = 'i';
    }

    /*
     * add "(?:" to group this regular expression
     */
    if (pattern->spRegularExpression)
    {
        *r++ = '(';
        *r++ = '?';
        *r++ = ':';

        adaptedRegExpText =
          adaptLocalBackRefs(
            pattern->spText,
            &regExpStringInfo->resiLocalBackRefID);

        s = adaptedRegExpText;
    }
    else
    {
        s = pattern->spText;
    }

    /*
     * add the pattern characters - evtl. escaped, if special
     * regular expression symbols & pattern is no reg. exp. -
     * to the buffer:
     */

    while (*s != '\0')
    {
        if (!pattern->spRegularExpression)
        {
            switch (*s)
            {
                case '(':
                case ')':
                case '-':
                case '[':
                case ']':
                case '<':
                case '>':
                case '{':
                case '}':
                case '.':
                case '\\':
                case '|':
                case '^':
                case '$':
                case '*':
                case '+':
                case '?':
                case '&':
                    *r++ = '\\';
                    break;
            };
        }

        *r++ = *s++;
    }

    /*
     * close "reg. exp. group" bracket
     */
    if (pattern->spRegularExpression)
    {
      *r++ = ')';

      XtFree(adaptedRegExpText);
    }

    /*
     * close case insensitive search bracket
     */
    if (pattern->spCaseInsensitive)
    {
      *r++ = ')';
    }

    /*
     * terminate added string
     */
    *r = '\0';
}

/*
** Adapt local backrefs inside given regExpText by common local IDs.
** Returns new allocated reg. exp. string holding common local backrefs.
*/
static char *adaptLocalBackRefs(
  char *regExpText,
  int  *commonLocalId)
{
    int id;
    int ownLocalId = 0;
    char *s = regExpText;
    char *newRegExpText = XtMalloc(strlen(regExpText)*3);
    char *n = newRegExpText;
    int  localBackRefList[MAX_LOCAL_BACK_REF_ID];

    /*
     * put all local backrefs into "localBackRefList".
     */
    scanForLocalBackRefs(regExpText, localBackRefList);

    while (*s != '\0')
    {
        *n++ = *s;

        if (*s == '\\')
        {
            s++;

            if (isdigit((unsigned char)*s))
            {
                /*
                 * \n (n=1..9) found: replace local backref.
                 * by "common local ID"
                 */
                id =
                  (int)((unsigned char)*s - (unsigned char)'0') - 1;

                if (localBackRefList[id] != NO_LOCAL_BACK_REF_ID &&
                    localBackRefList[id] != LOCAL_BACK_REF_ID_USED)
                {
                    *n++ = (char)((int)('0') + localBackRefList[id]);
                }
                else
                {
                    *n++ = '0';
                }

                s ++;
            }
            else if (*s != '\0')
            {
                /*
                 * copy escaped character
                 */
                *n++ = *s++;
            }
        }
        else if (*s == '(')
        {
            s ++;

            if (*s == '?')
            {
                /*
                 * non capturing parentheses found -> just copy it
                 */
                *n++ = *s++;
            }
            else
            {
                /*
                 * capturing parentheses found:
                 */
                if (localBackRefList[ownLocalId] == LOCAL_BACK_REF_ID_USED)
                {
                    /*
                     * backref used within given reg. exp. text:
                     * remember common local id for replacement later on
                     */
                    localBackRefList[ownLocalId] = *commonLocalId;
                    (*commonLocalId) ++;
                    ownLocalId ++;
                }
                else
                {
                    /*
                     * backref not used within given reg. exp. text:
                     * replace capturing parentheses by non capturing one
                     */
                    *n++ = '?';
                    *n++ = ':';
                }
            }
        }
        else
        {
          s ++;
        }
    }

    /*
     * terminate adapted string
     */
    *n = '\0';

    return newRegExpText;
}

/*
** Get all local backrefs of given regExpText and put them into
** given localBackRefList.
*/
static void scanForLocalBackRefs(
  char *regExpText,
  int  *localBackRefList)
{
    int i;
    int localId;
    char *s = regExpText;

    for (i=0; i<MAX_LOCAL_BACK_REF_ID; i++)
    {
        localBackRefList[i] = NO_LOCAL_BACK_REF_ID;
    }

    while (*s != '\0')
    {
        if (*s == '\\')
        {
            if (isdigit((unsigned char)*(s+1)))
            {
                /*
                 * \n (n=1..9) found: flag usage in local backref list
                 */
                localId =
                  (int)((unsigned char)*(s+1) - (unsigned char)'0') - 1;

                localBackRefList[localId] = LOCAL_BACK_REF_ID_USED;

                s ++;
            }
            else if (*(s+1) != '\0')
                s ++;
        }
        s ++;
    }
}

/*
** Returns true, if string of given multi pattern reference was
** not added to regExpStringInfo.
*/
static int isMultiPatternNotAdded(
  RegExpStringInfo         *regExpStringInfo,
  PatternReference         *toBeAddedPR)
{
    int i;
    PatternReference *addedPR;
    PatternReference *newPR;

    for (i=0; i < regExpStringInfo->resiNbrOfAddedMultiPat; i++)
    {
        addedPR = regExpStringInfo->resiAddedMultiPat[i];

        if (addedPR->prElementIdx == toBeAddedPR->prElementIdx &&
            addedPR->prPatternIdx == toBeAddedPR->prPatternIdx)
        {
            return False;
        }
    }

    newPR = (PatternReference *)XtMalloc(sizeof(PatternReference));

    *newPR = *toBeAddedPR;

    regExpStringInfo->resiAddedMultiPat[regExpStringInfo->resiNbrOfAddedMultiPat ++] = newPR;

    return True;
}

/*
** add given PatternElement to given reg. exp. strings
*/
static void addUniquePatternToRegExpString(
  PatternElement           *patElement,
  PatternReference         *patElementReference,
  ReadMatchPatternInfo     *readMatchPatternInfo,
  RegExpStringInfo         *regExpStringInfo)
{
    PatternElement   *referredElement;
    PatternReference  referredPatReference;

    switch (patElement->peType)
    {
        case PET_SINGLE:
            addPatternToRegExpString(
              &(patElement->peVal.peuSingle),
              regExpStringInfo);
            break;

        case PET_MULTIPLE:
            /*
             * add element to reg. exp. string only, if it was
             * not added before.
             */
            if (isMultiPatternNotAdded(regExpStringInfo, patElementReference))
            {
                addPatternToRegExpString(
                  &(patElement->peVal.peuMulti.mpStringPattern),
                  regExpStringInfo);
            }
            break;

      case PET_REFERENCE:
            /*
             * add referred element to reg. exp. string only, if related
             * multi pattern was not added before.
             */
            referredPatReference = patElement->peVal.peuRef;

            referredElement =
              readMatchPatternInfo->rmpiElement[referredPatReference.prElementIdx]->
                mpteAll.pesPattern[referredPatReference.prPatternIdx];

            if (isMultiPatternNotAdded(regExpStringInfo, &referredPatReference))
            {
                addPatternToRegExpString(
                  &(referredElement->peVal.peuMulti.mpStringPattern),
                  regExpStringInfo);
            }
            break;
    }
}

/*
** add given MatchPatternTableElement to given reg. ex. strings
*/
static void addElementToRegExpString(
  MatchPatternTableElement *element,
  ReadMatchPatternInfo     *readMatchPatternInfo,
  RegExpStringInfo         *regExpStringInfo)
{
    int i;
    PatternReference elementRef;

    elementRef.prElementIdx = element->mpteIndex;

    for (i=0; i<element->mpteAll.pesNumberOfPattern; i ++)
    {
        elementRef.prPatternIdx = i;

        addUniquePatternToRegExpString(
          element->mpteAll.pesPattern[i],
          &elementRef,
          readMatchPatternInfo,
          regExpStringInfo);
    }
}

/*
** Concatenate strings stored by regExpStringInfo.
** Free given regExpStringInfo afterwards.
** Returns resulting string.
*/
static void catSMTRegExpStrings(
  RegExpStringInfo  *regExpStringInfo,
  char             **regExpString)
{
    int resultingLen;

    /*
     * allocate & init. a buffer for the resulting regular expression
     */
    resultingLen =
      strlen( regExpStringInfo->resiNoneWBRegExpString ) +
      strlen( regExpStringInfo->resiLeftWBRegExpString ) +
      strlen( regExpStringInfo->resiRightWBRegExpString ) +
      strlen( regExpStringInfo->resiBothWBRegExpString ) + 5;

    *regExpString = XtMalloc( resultingLen );

    strcpy( *regExpString, "" );

    /*
     * add the single parts to the resulting regular expression
     * (= cat of parts separated by an "or")
     */
    addSMTRegExpString( *regExpString, regExpStringInfo->resiNoneWBRegExpString,  "" );
    addSMTRegExpString( *regExpString, regExpStringInfo->resiLeftWBRegExpString,  ")" );
    addSMTRegExpString( *regExpString, regExpStringInfo->resiRightWBRegExpString, ")>" );
    addSMTRegExpString( *regExpString, regExpStringInfo->resiBothWBRegExpString,  ")>" );

    /*
     * free buffers
     */
    freeRegExpStringInfo( regExpStringInfo );
}

/*
** Free the allocated memory contained in a RegExpStringInfo data structure
*/
static void freeRegExpStringInfo(
  RegExpStringInfo *regExpStringInfo)
{
    int i;

    XtFree( regExpStringInfo->resiNoneWBRegExpString  );
    XtFree( regExpStringInfo->resiLeftWBRegExpString  );
    XtFree( regExpStringInfo->resiRightWBRegExpString );
    XtFree( regExpStringInfo->resiBothWBRegExpString  );

    for (i=0; i < regExpStringInfo->resiNbrOfAddedMultiPat; i ++)
        XtFree( (char *)regExpStringInfo->resiAddedMultiPat[i] );

    XtFree( (char *)regExpStringInfo->resiAddedMultiPat);
}

/*
** Compose regular expression for start / end pattern.
*/
static void composeStartEndRegExpString(
  ReadMatchPatternInfo      *readMatchPatternInfo,
  MatchPatternTableElement  *element,
  char                     **regExpString)
{
    int i;
    RegExpStringInfo regExpStringInfo;
    PatternReference elementRef;
    PatternElementSet startPat = element->mpteStart;
    PatternElementSet endPat   = element->mpteEnd;

    /*
     * Allocate buffers for keyword regular expression.
     */
    setupRegExpStringBuffers(
      readMatchPatternInfo,
      &regExpStringInfo);

    /*
     * Treat start / end element of MatchPatternTableElement
     */

    elementRef.prElementIdx = element->mpteIndex;

    for (i=0; i < startPat.pesNumberOfPattern; i ++)
    {
        elementRef.prPatternIdx = startPat.pesPattern[i]->peIndex;

        addUniquePatternToRegExpString(
          startPat.pesPattern[i],
          &elementRef,
          readMatchPatternInfo,
          &regExpStringInfo);
    }

    for (i=0; i < endPat.pesNumberOfPattern; i ++)
    {
        elementRef.prPatternIdx = endPat.pesPattern[i]->peIndex;

        addUniquePatternToRegExpString(
          endPat.pesPattern[i],
          &elementRef,
          readMatchPatternInfo,
          &regExpStringInfo);
    }

    /*
     * Assemble the resulting regular expression
     */
    catSMTRegExpStrings(
      &regExpStringInfo,
      regExpString);
}

static void copyStringMatchTableForDialog(
  StringMatchTable       *sourceTable,
  DialogMatchPatternInfo *dialogTable )
{
    int i;

    /*
     * if no source table exist (yet), then set nbr. of elements / groups to 0
     */
    if (sourceTable == NULL)
    {
        dialogTable->dmpiNbrOfSeqElements = 0;

        return;
    }

    /*
     * copy matching pattern sequence
     */
    dialogTable->dmpiNbrOfSeqElements = sourceTable->smtNumberOfSeqElements;

    for (i=0; i < sourceTable->smtNumberOfSeqElements; i ++)
    {
        copySequenceElementForDialog(
          sourceTable,
          sourceTable->smtSequence[i],
          &dialogTable->dmpiSequence[i] );
    }
}

static void *copyMatchPatternElementForDialog(
  MatchPatternTable *table,
  int                sourceElementIdx)
{
    int i;
    int patIdx = 0;
    MatchPatternTableElement       *sourceElement;
    DialogMatchPatternTableElement *destination;

    sourceElement = table->mptElements[sourceElementIdx];

    destination =
      (DialogMatchPatternTableElement *)XtMalloc( sizeof(DialogMatchPatternTableElement) );

    destination->dmpteName = XtNewString(sourceElement->mpteName);
    destination->dmpteType = sourceElement->mpteType;
    destination->dmpteSkipBtwnStartEnd = sourceElement->mpteSkipBtwnStartEnd;
    destination->dmpteIgnoreHighLightInfo = sourceElement->mpteIgnoreHighLightInfo;
    destination->dmpteFlash = sourceElement->mpteFlash;

    for (i=0; i<sourceElement->mpteAll.pesNumberOfPattern; i++)
    {
        copyPatternForDialog(
          table,
          sourceElement->mpteAll.pesPattern[i],
          &destination->dmptePatterns.dspElements[patIdx ++]);
    }

    destination->dmptePatterns.dspNumberOfPatterns = patIdx;

    return (void *)destination;
}

static void copyPatternForDialog(
  MatchPatternTable            *table,
  PatternElement               *sourcePattern,
  DialogStringPatternElement  **dialogPattern )
{
    DialogStringPatternElement *newPat;
    StringPattern *strSourcePat = GetStringPattern( table, sourcePattern );

    newPat = (DialogStringPatternElement *)XtMalloc(sizeof(DialogStringPatternElement));
    *dialogPattern = newPat;

    if( strSourcePat->spOrigText != NULL)
        newPat->dspeText = XtNewString(strSourcePat->spOrigText);
    else
        newPat->dspeText = XtNewString(strSourcePat->spText);

    newPat->dspeKind              = sourcePattern->peKind;
    newPat->dspeWordBoundary      = strSourcePat->spWordBoundary;
    newPat->dspeCaseInsensitive   = strSourcePat->spCaseInsensitive;
    newPat->dspeRegularExpression = strSourcePat->spRegularExpression;
}

static void *copyGroupElementForDialog(
  MatchPatternGroupElement *sourceGroup)
{
    int i;
    DialogMatchPatternGroupElement *destination;

    destination =
      (DialogMatchPatternGroupElement *)XtMalloc( sizeof(DialogMatchPatternGroupElement) );

    destination->dmpgeName = XtNewString(sourceGroup->mpgeName);
    destination->dmpgeNumberOfSubPatterns = sourceGroup->mpgeNumberOfSubPatterns;

    for ( i=0; i<destination->dmpgeNumberOfSubPatterns; i ++)
    {
        destination->dmpgeSubPatternIds[i] =
          XtNewString(sourceGroup->mpgeSubPatternIds[i]);
    }

    return destination;
}

static void copySequenceElementForDialog(
  StringMatchTable                   *sourceTable,
  MatchPatternSequenceElement        *sourceSeqElement,
  DialogMatchPatternSequenceElement **dialogSeqElement )
{
    DialogMatchPatternSequenceElement *destSeqElement;

    destSeqElement =
      (DialogMatchPatternSequenceElement *)XtMalloc( sizeof(DialogMatchPatternSequenceElement) );

    *dialogSeqElement = destSeqElement;

    destSeqElement->dmpseName  = XtNewString(sourceSeqElement->mpseName);
    destSeqElement->dmpseType  = sourceSeqElement->mpseType;
    destSeqElement->dmpseValid = True;

    if (destSeqElement->dmpseType == MPT_GROUP)
    {
        destSeqElement->dmpsePtr =
          copyGroupElementForDialog(
            sourceTable->smtGroups[sourceSeqElement->mpseIndex]);
    }
    else
    {
        destSeqElement->dmpsePtr =
          copyMatchPatternElementForDialog(
            sourceTable->smtAllPatterns,
            sourceSeqElement->mpseIndex);
    }
}

static DialogMatchPatternSequenceElement *copyDialogSequenceElement(
  DialogMatchPatternSequenceElement *sourceSeq)
{
    DialogMatchPatternSequenceElement *destSeq;

    destSeq =
      (DialogMatchPatternSequenceElement *)XtMalloc(sizeof(DialogMatchPatternSequenceElement));

    destSeq->dmpseName  = XtNewString(sourceSeq->dmpseName);
    destSeq->dmpseType  = sourceSeq->dmpseType;
    destSeq->dmpseValid = True;

    destSeq->dmpsePtr = sourceSeq->dmpsePtr;

    return destSeq;
}

static void freeDialogMatchPatternElement(
  DialogMatchPatternTableElement *dialogElement )
{
    int i;

    for (i=0; i<dialogElement->dmptePatterns.dspNumberOfPatterns; i ++)
    {
        freeDialogStringPatternElement(
          dialogElement->dmptePatterns.dspElements[i]);
    }

    freeXtPtr((void **)&dialogElement->dmpteName);

    freeXtPtr((void **)&dialogElement);
}

static void freeDialogStringPatternElement(
  DialogStringPatternElement *element)
{
    freeXtPtr((void **)&element->dspeText);

    freeXtPtr((void **)&element);
}

static void freeDialogGroupElement(
  DialogMatchPatternGroupElement *dialogGroup )
{
    int i;

    for (i=0; i<dialogGroup->dmpgeNumberOfSubPatterns; i ++)
    {
        freeXtPtr((void **)&dialogGroup->dmpgeSubPatternIds[i]);
    }

    freeXtPtr((void **)&dialogGroup->dmpgeName);

    freeXtPtr((void **)&dialogGroup);
}

static void freeDialogSequenceElement(
  DialogMatchPatternSequenceElement *dialogSeq )
{
    freeXtPtr((void **)&dialogSeq->dmpseName);

    if (dialogSeq->dmpseType == MPT_GROUP)
    {
        freeDialogGroupElement(
          (DialogMatchPatternGroupElement *)dialogSeq->dmpsePtr );
    }
    else
    {
        freeDialogMatchPatternElement(
          (DialogMatchPatternTableElement *)dialogSeq->dmpsePtr );
    }

    freeXtPtr((void **)&dialogSeq);
}

static void copyDialogStringPatternsFromTable(
  DialogMatchPatternTableElement  *tableElement,
  DialogStringPatterns            *destPatterns)
{
    int i;

    destPatterns->dspNumberOfPatterns =
      tableElement->dmptePatterns.dspNumberOfPatterns;

    for (i=0; i<destPatterns->dspNumberOfPatterns; i++)
    {
        destPatterns->dspElements[i] =
          copyDialogStringPatternElement(
            tableElement->dmptePatterns.dspElements[i] );
    }
}

static void copyDialogStringPatterns(
  DialogStringPatterns *sourcePatterns,
  DialogStringPatterns *destPatterns)
{
    int i;

    destPatterns->dspNumberOfPatterns =
      sourcePatterns->dspNumberOfPatterns;

    for (i=0; i<destPatterns->dspNumberOfPatterns; i++)
    {
        destPatterns->dspElements[i] =
          copyDialogStringPatternElement(
            sourcePatterns->dspElements[i] );
    }
}

static void freeDialogStringPatterns(
  DialogStringPatterns *patterns)
{
    int i;

    for (i=0; i<patterns->dspNumberOfPatterns; i++)
    {
        freeDialogStringPatternElement(patterns->dspElements[i]);
    }

    patterns->dspNumberOfPatterns = 0;
}

static DialogStringPatternElement *copyDialogStringPatternElement(
  DialogStringPatternElement  *sourceElement)
{
    DialogStringPatternElement *newPatElement;

    newPatElement = (DialogStringPatternElement *)XtMalloc(sizeof(DialogStringPatternElement));

    newPatElement->dspeText              = XtNewString(sourceElement->dspeText);
    newPatElement->dspeKind              = sourceElement->dspeKind;
    newPatElement->dspeWordBoundary      = sourceElement->dspeWordBoundary;
    newPatElement->dspeCaseInsensitive   = sourceElement->dspeCaseInsensitive;
    newPatElement->dspeRegularExpression = sourceElement->dspeRegularExpression;

    return newPatElement;
}

static void copyDialogPatternNamesFromGroup(
  DialogMatchPatternGroupElement  *group,
  DialogStringPatterns            *destPatterns)
{
    int i;

    destPatterns->dspNumberOfPatterns =
      group->dmpgeNumberOfSubPatterns;

    for (i=0; i<destPatterns->dspNumberOfPatterns; i++)
    {
        destPatterns->dspElements[i] =
          copyDialogPatternName(
            group->dmpgeSubPatternIds[i] );
    }
}

static DialogStringPatternElement *copyDialogPatternName(
  char *sourcePatternId)
{
    DialogStringPatternElement *newPatElement;

    newPatElement = (DialogStringPatternElement *)XtMalloc(sizeof(DialogStringPatternElement));

    newPatElement->dspeText              = XtNewString(sourcePatternId);
    newPatElement->dspeKind              = PEK_START;
    newPatElement->dspeWordBoundary      = PWB_NONE;
    newPatElement->dspeCaseInsensitive   = False;
    newPatElement->dspeRegularExpression = False;

    return newPatElement;
}

static void copyDialogPatternNamesToGroup(
  DialogStringPatterns           *sourceNames,
  DialogMatchPatternGroupElement *destGroup)
{
    int i;

    destGroup->dmpgeNumberOfSubPatterns =
      sourceNames->dspNumberOfPatterns;

    for (i=0; i<destGroup->dmpgeNumberOfSubPatterns; i++)
    {
        destGroup->dmpgeSubPatternIds[i] =
          XtNewString(
            sourceNames->dspElements[i]->dspeText);
    }
}


/*
** Present a dialog for editing matching pattern information
*/
void EditMatchPatterns(WindowInfo *window)
{
    Widget form, lmOptMenu;
    Widget lmForm;
    Widget okBtn, applyBtn, checkBtn, deleteBtn, closeBtn, helpBtn;
    Widget restoreBtn, lmBtn;
    Widget matchPatternsForm, matchPatternsFrame, matchPatternsLbl;
    Widget matchPatternTypeBox, matchPatternTypeLbl;
    Widget globalAttributesBox;
    Widget stringPatternsFrame, stringPatternsForm;
    Widget stringPatternTypeBox;
    Widget wordBoundaryBox;
    Widget stringAttributesBox;
    StringMatchTable *table;
    XmString s1;
    int n;
    Arg args[20];

    /*
     * if the dialog is already displayed, just pop it to the top and return
     */
    if (MatchPatternDialog.mpdShell != NULL)
    {
        RaiseDialogWindow(MatchPatternDialog.mpdShell);
        return;
    }

    /*
     * decide on an initial language mode
     */
    MatchPatternDialog.mpdLangModeName =
      XtNewString(
        window->languageMode == PLAIN_LANGUAGE_MODE ?
          PLAIN_LM_STRING : LanguageModeName(window->languageMode));

    /*
     * find the associated matching pattern table to edit
     */
    table = (StringMatchTable *)FindStringMatchTable(MatchPatternDialog.mpdLangModeName);

    /*
     * copy the list of patterns to one that the user can freely edit
     */
    copyStringMatchTableForDialog( table, &MatchPatternDialog.mpdTable );

    /*
     * init. status information of dialog
     */
    MatchPatternDialog.currentDmptSeqElement = NULL;
    MatchPatternDialog.currentDmptElement    = NULL;
    MatchPatternDialog.currentDmptGroup      = NULL;

    MatchPatternDialog.currentStringPatterns.dspNumberOfPatterns = 0;
    MatchPatternDialog.mpdStringPatternIsDisplayed = True;

    /*
     * Create a form widget in an application shell
     */
    n = 0;
    XtSetArg(args[n], XmNdeleteResponse, XmDO_NOTHING); n++;
    XtSetArg(args[n], XmNiconName, "Matching Patterns"); n++;
    XtSetArg(args[n], XmNtitle, "Matching (Parenthesis) Patterns"); n++;
    MatchPatternDialog.mpdShell = CreateShellWithBestVis(APP_NAME, APP_CLASS,
            applicationShellWidgetClass, TheDisplay, args, n);
    AddSmallIcon(MatchPatternDialog.mpdShell);
    form = XtVaCreateManagedWidget("editMatchPatterns", xmFormWidgetClass,
            MatchPatternDialog.mpdShell, XmNautoUnmanage, False,
            XmNresizePolicy, XmRESIZE_NONE, NULL);
    XtAddCallback(form, XmNdestroyCallback, destroyCB, NULL);
    AddMotifCloseCallback(MatchPatternDialog.mpdShell, closeCB, NULL);

    lmForm = XtVaCreateManagedWidget("lmForm", xmFormWidgetClass,
            form,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 1,
            XmNtopAttachment, XmATTACH_POSITION,
            XmNtopPosition, 1,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 99, NULL);

    MatchPatternDialog.mpdLmPulldown =
      CreateLanguageModeMenu(lmForm, matchPatternLangModeCB, NULL, True);

    n = 0;
    XtSetArg(args[n], XmNspacing, 0); n++;
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 50); n++;
    XtSetArg(args[n], XmNsubMenuId, MatchPatternDialog.mpdLmPulldown); n++;
    lmOptMenu = XmCreateOptionMenu(lmForm, "langModeOptMenu", args, n);
    XtManageChild(lmOptMenu);
    MatchPatternDialog.mpdLmOptMenu = lmOptMenu;

    XtVaCreateManagedWidget("lmLbl", xmLabelGadgetClass, lmForm,
            XmNlabelString, s1=XmStringCreateSimple("Language Mode:"),
            XmNmnemonic, 'M',
            XmNuserData, XtParent(MatchPatternDialog.mpdLmOptMenu),
            XmNalignment, XmALIGNMENT_END,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 50,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
            XmNbottomWidget, lmOptMenu, NULL);
    XmStringFree(s1);

    lmBtn = XtVaCreateManagedWidget("lmBtn", xmPushButtonWidgetClass, lmForm,
            XmNlabelString, s1=MKSTRING("Add / Modify\nLanguage Mode..."),
            XmNmnemonic, 'A',
            XmNrightAttachment, XmATTACH_FORM,
            XmNtopAttachment, XmATTACH_FORM, NULL);
    XtAddCallback(lmBtn, XmNactivateCallback, pmLanguageModeDialogCB, NULL);
    XmStringFree(s1);

    okBtn = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("OK"),
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 1,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 13,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, BORDER, NULL);
    XtAddCallback(okBtn, XmNactivateCallback, okCB, NULL);
    XmStringFree(s1);

    applyBtn = XtVaCreateManagedWidget("apply", xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("Apply"),
            XmNmnemonic, 'y',
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 13,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 26,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, BORDER, NULL);
    XtAddCallback(applyBtn, XmNactivateCallback, applyCB, NULL);
    XmStringFree(s1);

    checkBtn = XtVaCreateManagedWidget("check", xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("Check"),
            XmNmnemonic, 'k',
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 26,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 39,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, BORDER, NULL);
    XtAddCallback(checkBtn, XmNactivateCallback, checkCB, NULL);
    XmStringFree(s1);

    deleteBtn = XtVaCreateManagedWidget("delete", xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("Delete"),
            XmNmnemonic, 'D',
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 39,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 52,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, BORDER, NULL);
    XtAddCallback(deleteBtn, XmNactivateCallback, deleteCB, NULL);
    XmStringFree(s1);

    restoreBtn = XtVaCreateManagedWidget("restore", xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("Restore Defaults"),
            XmNmnemonic, 'f',
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 52,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 73,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, BORDER, NULL);
    XtAddCallback(restoreBtn, XmNactivateCallback, restoreCB, NULL);
    XmStringFree(s1);

    closeBtn = XtVaCreateManagedWidget("close", xmPushButtonWidgetClass,
            form,
            XmNlabelString, s1=XmStringCreateSimple("Close"),
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 73,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 86,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, BORDER, NULL);
    XtAddCallback(closeBtn, XmNactivateCallback, closeCB, NULL);
    XmStringFree(s1);

    helpBtn = XtVaCreateManagedWidget("help", xmPushButtonWidgetClass,
            form,
            XmNlabelString, s1=XmStringCreateSimple("Help"),
            XmNmnemonic, 'H',
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 86,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 99,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, BORDER, NULL);
    XtAddCallback(helpBtn, XmNactivateCallback, helpCB, NULL);
    XmStringFree(s1);

    stringPatternsFrame = XtVaCreateManagedWidget("stringPatternsFrame", xmFrameWidgetClass,
            form,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 1,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 99,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, okBtn,
            XmNbottomOffset, BORDER, NULL);
    stringPatternsForm = XtVaCreateManagedWidget("stringPatternsForm", xmFormWidgetClass,
            stringPatternsFrame, NULL);
    MatchPatternDialog.mpdStringPatternsLbl = XtVaCreateManagedWidget("mpdStringPatternsLbl", xmLabelGadgetClass,
            stringPatternsFrame,
            XmNlabelString, s1=XmStringCreateSimple(STRING_PATTERNS_LBL_TXT),
            XmNmarginHeight, 0,
            XmNchildType, XmFRAME_TITLE_CHILD, NULL);
    XmStringFree(s1);

    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 1); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, LIST_RIGHT-1); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomOffset, BORDER); n++;
    MatchPatternDialog.mpdStringPatternsListW =
      CreateManagedList(stringPatternsForm, "stringPatternsList", args,
            n, (void **)MatchPatternDialog.currentStringPatterns.dspElements,
            &MatchPatternDialog.currentStringPatterns.dspNumberOfPatterns,
            MAX_STRING_PATTERNS, 18,
            getStringPatternDisplayedCB, NULL,
            setStringPatternDisplayedCB, NULL,
            freeStringPatternItemCB);
    XtVaSetValues(MatchPatternDialog.mpdStringPatternsLbl, XmNuserData, MatchPatternDialog.mpdStringPatternsListW, NULL);

    MatchPatternDialog.mpdStringPatternTypeLbl = XtVaCreateManagedWidget("mpdStringPatternTypeLbl", xmLabelGadgetClass,
            stringPatternsForm,
            XmNlabelString, s1=XmStringCreateSimple("String Pattern Type:"),
            XmNmarginHeight, 0,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_FORM, NULL);
    XmStringFree(s1);

    stringPatternTypeBox = XtVaCreateManagedWidget("stringPatternTypeBox", xmRowColumnWidgetClass,
            stringPatternsForm,
            XmNorientation, XmHORIZONTAL,
            XmNpacking, XmPACK_TIGHT,
            XmNradioBehavior, True,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, MatchPatternDialog.mpdStringPatternTypeLbl, NULL);
    MatchPatternDialog.sptStartW = XtVaCreateManagedWidget("sptStartW",
            xmToggleButtonWidgetClass, stringPatternTypeBox,
            XmNset, True,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Start"),
            NULL);
    XmStringFree(s1);
    MatchPatternDialog.sptMiddleW = XtVaCreateManagedWidget("sptMiddleW",
            xmToggleButtonWidgetClass, stringPatternTypeBox,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Middle"),
            NULL);
    XmStringFree(s1);
    MatchPatternDialog.sptEndW = XtVaCreateManagedWidget("sptEndW",
            xmToggleButtonWidgetClass, stringPatternTypeBox,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "End"),
            NULL);
    XmStringFree(s1);

    MatchPatternDialog.mpdWordBoundaryLbl = XtVaCreateManagedWidget("mpdWordBoundaryLbl", xmLabelGadgetClass,
            stringPatternsForm,
            XmNlabelString, s1=XmStringCreateSimple("Word boundary:"),
            XmNmarginHeight, 0,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, stringPatternTypeBox,
            NULL);
    XmStringFree(s1);

    wordBoundaryBox = XtVaCreateManagedWidget("wordBoundaryBox", xmRowColumnWidgetClass,
            stringPatternsForm,
            XmNorientation, XmHORIZONTAL,
            XmNpacking, XmPACK_TIGHT,
            XmNradioBehavior, True,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, MatchPatternDialog.mpdWordBoundaryLbl, NULL);
    MatchPatternDialog.wbbBothW = XtVaCreateManagedWidget("wbbBothW",
            xmToggleButtonWidgetClass, wordBoundaryBox,
            XmNset, True,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Both (pattern is word)"),
            NULL);
    XmStringFree(s1);
    MatchPatternDialog.wbbLeftW = XtVaCreateManagedWidget("wbbLeftW",
            xmToggleButtonWidgetClass, wordBoundaryBox,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Left"),
            NULL);
    XmStringFree(s1);
    MatchPatternDialog.wbbRightW = XtVaCreateManagedWidget("wbbRightW",
            xmToggleButtonWidgetClass, wordBoundaryBox,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Right"),
            NULL);
    XmStringFree(s1);
    MatchPatternDialog.wbbNoneW = XtVaCreateManagedWidget("wbbNoneW",
            xmToggleButtonWidgetClass, wordBoundaryBox,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "None"),
            NULL);
    XmStringFree(s1);

    MatchPatternDialog.mpdStringAttributesLbl = XtVaCreateManagedWidget("mpdStringAttributesLbl", xmLabelGadgetClass,
            stringPatternsForm,
            XmNlabelString, s1=XmStringCreateSimple("String Attributes:"),
            XmNmarginHeight, 0,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, wordBoundaryBox, NULL);
    XmStringFree(s1);

    stringAttributesBox = XtVaCreateManagedWidget("stringAttributesBox", xmRowColumnWidgetClass,
            stringPatternsForm,
            XmNorientation, XmHORIZONTAL,
            XmNpacking, XmPACK_TIGHT,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, MatchPatternDialog.mpdStringAttributesLbl, NULL);
    MatchPatternDialog.sabRegularExpressionW = XtVaCreateManagedWidget("sabRegularExpressionW",
            xmToggleButtonWidgetClass, stringAttributesBox,
            XmNset, False,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Regular Expression"),
            NULL);
    XmStringFree(s1);
    XtAddCallback(MatchPatternDialog.sabRegularExpressionW, XmNvalueChangedCallback,
            strPatRegExpressionCB, NULL);
    MatchPatternDialog.sabCaseSensitiveW = XtVaCreateManagedWidget("sabCaseSensitiveW",
            xmToggleButtonWidgetClass, stringAttributesBox,
            XmNset, True,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Case Sensitive"),
            NULL);
    XmStringFree(s1);

    MatchPatternDialog.mpdStringPatternLbl = XtVaCreateManagedWidget("mpdStringPatternLbl", xmLabelGadgetClass,
            stringPatternsForm,
            XmNlabelString, s1=XmStringCreateSimple("String Pattern"),
            XmNmnemonic, 'S',
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, stringAttributesBox,
            XmNtopOffset, BORDER,
            NULL);
    XmStringFree(s1);

    MatchPatternDialog.mpdStringPatternW = XtVaCreateManagedWidget("mpdStringPatternW", xmTextWidgetClass,
            stringPatternsForm,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, MatchPatternDialog.mpdStringPatternLbl,
            XmNrightAttachment, XmATTACH_FORM,
            XmNrightOffset, BORDER,
            NULL);
    RemapDeleteKey(MatchPatternDialog.mpdStringPatternW);
    XtVaSetValues(MatchPatternDialog.mpdStringPatternLbl, XmNuserData, MatchPatternDialog.mpdStringPatternW, NULL);

    MatchPatternDialog.mpdSubPatNamesLbl = XtVaCreateManagedWidget("mpdSubPatNamesLbl", xmLabelGadgetClass,
            stringPatternsForm,
            XmNlabelString, s1=XmStringCreateSimple("Sub-Pattern Name"),
            XmNmnemonic, 't',
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, MatchPatternDialog.mpdStringPatternW,
            XmNtopOffset, BORDER,
            NULL);
    XmStringFree(s1);

    MatchPatternDialog.mpdSubPatNamesPulldown =
      createSubPatternNameMenu(stringPatternsForm, NULL, False);

    n = 0;
    XtSetArg(args[n], XmNspacing, 0); n++;
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    XtSetArg(args[n], XmNresizeWidth, True); n++;
    XtSetArg(args[n], XmNresizeHeight, True); n++;
    XtSetArg(args[n], XmNnavigationType, XmTAB_GROUP); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, MatchPatternDialog.mpdSubPatNamesLbl); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, LIST_RIGHT); n++;
    XtSetArg(args[n], XmNsubMenuId, MatchPatternDialog.mpdSubPatNamesPulldown); n++;
    MatchPatternDialog.mpdSubPatNamesOptMenu =
      XmCreateOptionMenu(stringPatternsForm, "subPatNamesOptMenu", args, n);
    XtManageChild(MatchPatternDialog.mpdSubPatNamesOptMenu);

    XtVaSetValues(
      MatchPatternDialog.mpdSubPatNamesLbl,
      XmNuserData, XtParent(MatchPatternDialog.mpdSubPatNamesOptMenu),
      NULL);

    XtSetSensitive(MatchPatternDialog.mpdSubPatNamesLbl, False);
    XtSetSensitive(MatchPatternDialog.mpdSubPatNamesOptMenu, False);

    matchPatternsFrame = XtVaCreateManagedWidget("matchPatternsFrame", xmFrameWidgetClass,
            form,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, 1,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, lmForm,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 99,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, stringPatternsFrame,
            XmNbottomOffset, BORDER, NULL);
    matchPatternsForm = XtVaCreateManagedWidget("matchPatternsForm", xmFormWidgetClass,
            matchPatternsFrame, NULL);
    matchPatternsLbl = XtVaCreateManagedWidget("matchPatternsLbl", xmLabelGadgetClass,
            matchPatternsFrame,
            XmNlabelString, s1=XmStringCreateSimple("Matching Patterns"),
            XmNmnemonic, 'P',
            XmNmarginHeight, 0,
            XmNchildType, XmFRAME_TITLE_CHILD, NULL);
    XmStringFree(s1);

    matchPatternTypeLbl = XtVaCreateManagedWidget("matchPatternTypeLbl", xmLabelGadgetClass,
            matchPatternsForm,
            XmNlabelString, s1=XmStringCreateSimple("Matching Pattern Type:"),
            XmNmarginHeight, 0,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_FORM, NULL);
    XmStringFree(s1);

    matchPatternTypeBox = XtVaCreateManagedWidget("matchPatternTypeBox", xmRowColumnWidgetClass,
            matchPatternsForm,
            XmNpacking, XmPACK_COLUMN,
            XmNradioBehavior, True,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, matchPatternTypeLbl, NULL);
    MatchPatternDialog.mptbIndividualW = XtVaCreateManagedWidget("mptbIndividualW",
            xmToggleButtonWidgetClass, matchPatternTypeBox,
            XmNset, True,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Individual"),
            XmNmnemonic, 'I', NULL);
    XmStringFree(s1);
    XtAddCallback(MatchPatternDialog.mptbIndividualW, XmNvalueChangedCallback,
            matchPatTypeCB, NULL);
    MatchPatternDialog.mptbSubPatternW = XtVaCreateManagedWidget("mptbSubPatternW",
            xmToggleButtonWidgetClass, matchPatternTypeBox,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Sub-pattern (belongs to context group)"),
            XmNmnemonic, 'u', NULL);
    XmStringFree(s1);
    XtAddCallback(MatchPatternDialog.mptbSubPatternW, XmNvalueChangedCallback,
            matchPatTypeCB, NULL);
    MatchPatternDialog.mptbContextGroupW = XtVaCreateManagedWidget("mptbContextGroupW",
            xmToggleButtonWidgetClass, matchPatternTypeBox,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Context group"),
            XmNmnemonic, 'g', NULL);
    XmStringFree(s1);
    XtAddCallback(MatchPatternDialog.mptbContextGroupW, XmNvalueChangedCallback,
            matchPatTypeCB, NULL);

    MatchPatternDialog.mpdGlobalAttributesLbl = XtVaCreateManagedWidget("mpdGlobalAttributesLbl",
            xmLabelGadgetClass, matchPatternsForm,
            XmNlabelString, s1=XmStringCreateSimple("Global Attributes:"),
            XmNmarginHeight, 0,
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopOffset, BORDER,
            XmNtopWidget, matchPatternTypeBox, NULL);
    XmStringFree(s1);

    globalAttributesBox = XtVaCreateManagedWidget("globalAttributesBox", xmRowColumnWidgetClass,
            matchPatternsForm,
            XmNpacking, XmPACK_COLUMN,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, MatchPatternDialog.mpdGlobalAttributesLbl, NULL);
    MatchPatternDialog.gabSkipBtwStartEndW = XtVaCreateManagedWidget("gabSkipBtwStartEndW",
            xmToggleButtonWidgetClass, globalAttributesBox,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Skip content between start / end pattern"),
            XmNmnemonic, 'c', NULL);
    XmStringFree(s1);
    MatchPatternDialog.gabFlashW = XtVaCreateManagedWidget("gabFlashW",
            xmToggleButtonWidgetClass, globalAttributesBox,
            XmNset, True,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Flash matching pattern"),
            XmNmnemonic, 'l', NULL);
    XmStringFree(s1);
    MatchPatternDialog.gabSyntaxBasedW = XtVaCreateManagedWidget("gabSyntaxBasedW",
            xmToggleButtonWidgetClass, globalAttributesBox,
            XmNset, True,
            XmNmarginHeight, 0,
            XmNlabelString, s1=XmStringCreateSimple(
                "Syntax based"),
            XmNmnemonic, 'b', NULL);
    XmStringFree(s1);

    MatchPatternDialog.mpdMatchPatternNameLbl = XtVaCreateManagedWidget("mpdMatchPatternNameLbl", xmLabelGadgetClass,
            matchPatternsForm,
            XmNlabelString, s1=XmStringCreateSimple(MATCH_PAT_NAME_LBL_TXT),
            XmNmnemonic, 'N',
            XmNalignment, XmALIGNMENT_BEGINNING,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, globalAttributesBox,
            XmNtopOffset, BORDER, NULL);
    XmStringFree(s1);

    MatchPatternDialog.mpdMatchPatternNameW = XtVaCreateManagedWidget("mpdMatchPatternNameW", xmTextWidgetClass,
            matchPatternsForm,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, LIST_RIGHT,
            XmNtopAttachment, XmATTACH_WIDGET,
            XmNtopWidget, MatchPatternDialog.mpdMatchPatternNameLbl,
            XmNrightAttachment, XmATTACH_FORM,
            XmNrightOffset, BORDER,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNbottomOffset, BORDER,
            NULL);
    RemapDeleteKey(MatchPatternDialog.mpdMatchPatternNameW);
    XtVaSetValues(MatchPatternDialog.mpdMatchPatternNameLbl, XmNuserData, MatchPatternDialog.mpdMatchPatternNameW, NULL);

    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 1); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, LIST_RIGHT-1); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;

    XtSetArg(args[n], XmNbottomOffset, BORDER); n++;
    MatchPatternDialog.mpdMatchPatternNamesListW =
      CreateManagedList(
        matchPatternsForm, "mpdMatchPatternNamesListW",
        args, n,
        (void **)MatchPatternDialog.mpdTable.dmpiSequence, &MatchPatternDialog.mpdTable.dmpiNbrOfSeqElements,
        MAX_NBR_MATCH_PATTERNS, 18,
        getMatchPatternDisplayedCB, NULL,
        setMatchPatternDisplayedCB, NULL,
        freeMatchPatternItemCB);
    AddDeleteConfirmCB(MatchPatternDialog.mpdMatchPatternNamesListW, deleteMatchPatternItemCB, NULL);

    XtVaSetValues(matchPatternsLbl, XmNuserData, MatchPatternDialog.mpdMatchPatternNamesListW, NULL);

    /*
     * set initial default button
     */
    XtVaSetValues(form, XmNdefaultButton, okBtn, NULL);
    XtVaSetValues(form, XmNcancelButton, closeBtn, NULL);

    /*
     * handle mnemonic selection of buttons and focus to dialog
     */
    AddDialogMnemonicHandler(form, False);

    /*
     * fill in the dialog information for the selected language mode
     */
    SetLangModeMenu(MatchPatternDialog.mpdLmOptMenu, MatchPatternDialog.mpdLangModeName);

    /*
     * realize all of the widgets in the new dialog
     */
    RealizeWithoutForcingPosition(MatchPatternDialog.mpdShell);
}

/*
** Modify match pattern dialog depending on showing a string pattern
** or a context group.
*/
static void setDialogType(int dialogShowsStringPattern)
{
    char *matchPatternNameText;
    char *strPatCxtGrpListText;
    XmString s1;
    int regularExpression =
          XmToggleButtonGetState(MatchPatternDialog.sabRegularExpressionW);

    /*
     * check, if dialog mode needs to be switched
     */
    if (MatchPatternDialog.mpdStringPatternIsDisplayed == dialogShowsStringPattern)
    {
        return;
    }

    if (dialogShowsStringPattern)
    {
        matchPatternNameText = MATCH_PAT_NAME_LBL_TXT;
        strPatCxtGrpListText = STRING_PATTERNS_LBL_TXT;
    }
    else
    {
        matchPatternNameText = "Context Group Name";
        strPatCxtGrpListText = "Related Sub-Patterns";
    }

    XtSetSensitive(MatchPatternDialog.mpdGlobalAttributesLbl, dialogShowsStringPattern);
    XtSetSensitive(MatchPatternDialog.gabSkipBtwStartEndW, dialogShowsStringPattern);
    XtSetSensitive(MatchPatternDialog.gabFlashW, dialogShowsStringPattern);
    XtSetSensitive(MatchPatternDialog.gabSyntaxBasedW, dialogShowsStringPattern);

    XtSetSensitive(MatchPatternDialog.mpdStringPatternTypeLbl, dialogShowsStringPattern);
    XtSetSensitive(MatchPatternDialog.sptStartW, dialogShowsStringPattern);
    XtSetSensitive(MatchPatternDialog.sptMiddleW, dialogShowsStringPattern);
    XtSetSensitive(MatchPatternDialog.sptEndW, dialogShowsStringPattern);

    setSensitiveWordBoundaryBox( dialogShowsStringPattern && !regularExpression );

    XtSetSensitive(MatchPatternDialog.mpdStringAttributesLbl, dialogShowsStringPattern);
    XtSetSensitive(MatchPatternDialog.sabCaseSensitiveW, dialogShowsStringPattern);
    XtSetSensitive(MatchPatternDialog.sabRegularExpressionW, dialogShowsStringPattern);

    XtSetSensitive(MatchPatternDialog.mpdStringPatternLbl, dialogShowsStringPattern);
    XtSetSensitive(MatchPatternDialog.mpdStringPatternW, dialogShowsStringPattern);

    XtSetSensitive(MatchPatternDialog.mpdSubPatNamesLbl, !dialogShowsStringPattern);
    XtSetSensitive(MatchPatternDialog.mpdSubPatNamesOptMenu, !dialogShowsStringPattern);

    XtVaSetValues(
      MatchPatternDialog.mpdMatchPatternNameLbl,
      XmNlabelString, s1=XmStringCreateSimple(matchPatternNameText),
      NULL);
    XmStringFree(s1);

    XtVaSetValues(
      MatchPatternDialog.mpdStringPatternsLbl,
      XmNlabelString, s1=XmStringCreateSimple(strPatCxtGrpListText),
      NULL);
    XmStringFree(s1);

    MatchPatternDialog.mpdStringPatternIsDisplayed = dialogShowsStringPattern;
}

static void setSensitiveWordBoundaryBox(int enable)
{
    XtSetSensitive(MatchPatternDialog.mpdWordBoundaryLbl, enable);
    XtSetSensitive(MatchPatternDialog.wbbBothW , enable);
    XtSetSensitive(MatchPatternDialog.wbbLeftW , enable);
    XtSetSensitive(MatchPatternDialog.wbbRightW, enable);
    XtSetSensitive(MatchPatternDialog.wbbNoneW , enable);
}

static void matchPatternLangModeCB(Widget w, XtPointer clientData, XtPointer callData)
{
    char *modeName;
    StringMatchTable *oldTable, *newTable;
    StringMatchTable emptyTable = {"", NULL, NULL, NULL, NULL, 0, NULL, 0, NULL};
    StringMatchTable *table;
    DMPTranslationResult translResult;
    int resp;

    /*
     * Get the newly selected mode name.  If it's the same, do nothing
     */
    XtVaGetValues(w, XmNuserData, &modeName, NULL);
    if (!strcmp(modeName, MatchPatternDialog.mpdLangModeName))
        return;

    /*
     * Look up the original version of the patterns being edited
     */
    oldTable = (StringMatchTable *)FindStringMatchTable(MatchPatternDialog.mpdLangModeName);
    if (oldTable == NULL)
        oldTable = &emptyTable;

    /*
     * Get the current information displayed by the dialog.  If it's bad,
     * give the user the chance to throw it out or go back and fix it. If
     * it has changed, give the user the chance to apply discard or cancel.
     */
    newTable = getDialogStringMatchTable(&translResult);

    if (translResult == DMPTR_EMPTY)
    {
        newTable = &emptyTable;
    }

    if (newTable == NULL)
    {
        if (DialogF(
              DF_WARN, MatchPatternDialog.mpdShell, 2,
              "Incomplete Matching Patterns for Language Mode",
              "Discard incomplete entry for language mode '%s'?",
              "Keep", "Discard",
              MatchPatternDialog.mpdLangModeName) == 1)
        {
            SetLangModeMenu(
              MatchPatternDialog.mpdLmOptMenu,
              MatchPatternDialog.mpdLangModeName);

            return;
        }
    }
    else if (stringMatchTableDiffer(oldTable, newTable))
    {
        if (newTable == &emptyTable)
            newTable = NULL;

        resp =
          DialogF(
            DF_WARN, MatchPatternDialog.mpdShell, 3,
            "Change Language Mode",
            "Apply changes for language mode '%s'?",
            "Apply Changes", "Discard Changes", "Cancel",
            MatchPatternDialog.mpdLangModeName);

        if (resp == 3)
        {
            SetLangModeMenu(
              MatchPatternDialog.mpdLmOptMenu,
              MatchPatternDialog.mpdLangModeName);

            freeStringMatchTable( newTable );

            return;
        }
        else if (resp == 1)
        {
            updateStringMatchTable( newTable );

            /*
             * Don't free the new table due to it's stored in MatchTables now
             */
            newTable = NULL;
        }
    }

    if (newTable != NULL && newTable != &emptyTable)
        freeStringMatchTable(newTable);

    /*
     * Free the old dialog information
     */
    freeVariableDialogData(DISCARD_LANGUAGE_MODE);

    /*
     * Fill the dialog with the new language mode information
     */
    MatchPatternDialog.mpdLangModeName = XtNewString(modeName);

    /*
     * Find the associated matching pattern table to edit
     */
    table = (StringMatchTable *)FindStringMatchTable(MatchPatternDialog.mpdLangModeName);

    /*
     * Copy the list of patterns to one that the user can freely edit
     */
    copyStringMatchTableForDialog( table, &MatchPatternDialog.mpdTable );

    /*
     * Update dialog fields
     */
    ChangeManagedListData(MatchPatternDialog.mpdMatchPatternNamesListW);
    ChangeManagedListData(MatchPatternDialog.mpdStringPatternsListW);
}

static void pmLanguageModeDialogCB(Widget w, XtPointer clientData, XtPointer callData)
{
    EditLanguageModes();
}

/*
** If a matching pattern dialog is up, ask to have the option menu for
** chosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void UpdateLanguageModeMenuMatchPattern(void)
{
    Widget oldMenu;

    if (MatchPatternDialog.mpdShell == NULL)
        return;

    oldMenu = MatchPatternDialog.mpdLmPulldown;
    /*
     * don't include "PLAIN" (4th parameter) in LM menu
     */
    MatchPatternDialog.mpdLmPulldown = CreateLanguageModeMenu(
            XtParent(XtParent(oldMenu)), matchPatternLangModeCB, NULL, False);
    XtVaSetValues(XmOptionButtonGadget(MatchPatternDialog.mpdLmOptMenu),
            XmNsubMenuId, MatchPatternDialog.mpdLmPulldown, NULL);
    SetLangModeMenu(MatchPatternDialog.mpdLmOptMenu, MatchPatternDialog.mpdLangModeName);

    XtDestroyWidget(oldMenu);
}

static void *getMatchPatternDisplayedCB(void *oldItem, int explicitRequest, int *abort,
        void *cbArg)
{
    DialogMatchPatternSequenceElement *newSeq;

    /*
     * If the dialog is currently displaying the "new" entry and the
     * fields are empty, that's just fine
     */
    if (oldItem == NULL && matchPatternDialogEmpty())
        return NULL;

    /*
     * Read string patterns / sub-pattern names area first
     */
    UpdateManagedList(MatchPatternDialog.mpdStringPatternsListW, True);

    /*
     * If there are no problems reading the data, just return it
     */
    newSeq = readMatchPatternFields(True);
    if (newSeq != NULL)
        return (void *)newSeq;

    /*
     * If there are problems, and the user didn't ask for the fields to be
     * read, give more warning
     */
    if (!explicitRequest)
    {
        if (DialogF(
              DF_WARN, MatchPatternDialog.mpdShell, 2,
              "Discard Matching Pattern Entry",
              "Discard incomplete entry\nfor current matching pattern?",
              "Keep", "Discard") == 2)
        {
            return oldItem == NULL
                    ? NULL
                    : (void *)copyDialogSequenceElement(
                                (DialogMatchPatternSequenceElement *)oldItem);
        }
    }

    /*
     * read data again without "silent" mode to display warning
     */
    newSeq = readMatchPatternFields(False);
    *abort = True;

    return NULL;
}

static void setMatchPatternDisplayedCB(void *item, void *cbArg)
{
    DialogMatchPatternSequenceElement *seqElement;
    DialogMatchPatternTableElement    *element;
    DialogMatchPatternGroupElement    *group;
    int isGroup;

    seqElement = (DialogMatchPatternSequenceElement *)item;

    MatchPatternDialog.currentDmptSeqElement = seqElement;

    if (item == NULL)
    {
        MatchPatternDialog.currentDmptElement = NULL;
        MatchPatternDialog.currentDmptGroup   = NULL;

        XmTextSetString(MatchPatternDialog.mpdMatchPatternNameW, "");
        RadioButtonChangeState(MatchPatternDialog.mptbIndividualW, True, True);
        RadioButtonChangeState(MatchPatternDialog.gabSkipBtwStartEndW, False, False);
        RadioButtonChangeState(MatchPatternDialog.gabFlashW, True, False);
        RadioButtonChangeState(MatchPatternDialog.gabSyntaxBasedW, True, False);

        freeDialogStringPatterns(
          &MatchPatternDialog.currentStringPatterns);

        ChangeManagedListData(MatchPatternDialog.mpdStringPatternsListW);

        setDialogType( STRING_PATTERN_DIALOG );
    }
    else
    {
        isGroup      = (seqElement->dmpseType == MPT_GROUP);

        XmTextSetString(MatchPatternDialog.mpdMatchPatternNameW, seqElement->dmpseName);

        if (seqElement->dmpseType == MPT_INDIVIDUAL)
            RadioButtonChangeState(MatchPatternDialog.mptbIndividualW, True, True);
        else if (seqElement->dmpseType == MPT_SUB)
            RadioButtonChangeState(MatchPatternDialog.mptbSubPatternW, True, True);
        else
            RadioButtonChangeState(MatchPatternDialog.mptbContextGroupW, True, True);

        freeDialogStringPatterns(
          &MatchPatternDialog.currentStringPatterns);

        if (isGroup)
        {
            group = (DialogMatchPatternGroupElement *)seqElement->dmpsePtr;
            MatchPatternDialog.currentDmptElement = NULL;
            MatchPatternDialog.currentDmptGroup   = group;

            copyDialogPatternNamesFromGroup(
              group,
              &MatchPatternDialog.currentStringPatterns);
        }
        else
        {
            element = (DialogMatchPatternTableElement *)seqElement->dmpsePtr;
            MatchPatternDialog.currentDmptElement = element;
            MatchPatternDialog.currentDmptGroup   = NULL;

            RadioButtonChangeState(
              MatchPatternDialog.gabSkipBtwStartEndW,
              element->dmpteSkipBtwnStartEnd,
              False);
            RadioButtonChangeState(
              MatchPatternDialog.gabFlashW,
              element->dmpteFlash,
              False);
            RadioButtonChangeState(
              MatchPatternDialog.gabSyntaxBasedW,
              !element->dmpteIgnoreHighLightInfo,
              False);

            copyDialogStringPatternsFromTable(
              element,
              &MatchPatternDialog.currentStringPatterns);
        }

        setDialogType( !isGroup );

        ChangeManagedListData(MatchPatternDialog.mpdStringPatternsListW);
    }
}

static void freeMatchPatternItemCB(void *item)
{
    freeDialogSequenceElement((DialogMatchPatternSequenceElement *)item);
}

/*
** Use "delete confirm" to delete matching pattern name from
** any group of this matching pattern set, in case of sub-pattern.
** Always confirm the delete.
*/
static int deleteMatchPatternItemCB(int itemIndex, void *cbArg)
{
    DialogMatchPatternSequenceElement *seqElement;

    seqElement = MatchPatternDialog.mpdTable.dmpiSequence[itemIndex];

    if (seqElement->dmpseType == MPT_SUB)
    {
        removeMatchPatternFromAllGroups( seqElement->dmpseName );
    }

    return True;
}

static void *getStringPatternDisplayedCB(void *oldItem, int explicitRequest, int *abort,
        void *cbArg)
{
    DialogStringPatternElement *newPat;
    int isRelatedToGroup = !MatchPatternDialog.mpdStringPatternIsDisplayed;

    /*
     * If the string pattern frame is currently displaying the "new" entry and the
     * fields are empty, that's just fine
     */
    if (oldItem == NULL && stringPatternFieldsEmpty(isRelatedToGroup))
        return NULL;

    /*
     * If there are no problems reading the data, just return it
     */
    newPat = readStringPatternFrameFields(True);
    if (newPat != NULL)
        return (void *)newPat;

    /*
     * If there are problems, and the user didn't ask for the fields to be
     * read, give more warning
     */

    if (!explicitRequest)
    {
        if (DialogF(
              DF_WARN, MatchPatternDialog.mpdShell, 2,
              "Discard String Pattern Entry",
              "Discard incomplete entry\nfor current string pattern?",
              "Keep", "Discard") == 2)
        {
            return oldItem == NULL
                    ? NULL
                    : (void *)copyDialogStringPatternElement(
                                (DialogStringPatternElement *)oldItem);
        }
    }

    /*
     * read data again without "silent" mode to display warning
     */
    newPat = readStringPatternFrameFields(False);
    *abort = True;

    return NULL;
}

static void setStringPatternDisplayedCB(void *item, void *cbArg)
{
    DialogStringPatternElement *element = (DialogStringPatternElement *)item;
    PatternElementKind  peKind;
    PatternWordBoundary wordBoundary;
    int isRelatedToGroup = !MatchPatternDialog.mpdStringPatternIsDisplayed;

    if (item == NULL)
    {
        if (isRelatedToGroup)
        {
            updateSubPatternNameMenu(NULL, False);
        }
        else
        {
            XmTextSetString(MatchPatternDialog.mpdStringPatternW, "");

            setSensitiveWordBoundaryBox( True );

            RadioButtonChangeState(MatchPatternDialog.wbbBothW, True, True);

            /*
             * type of "new" string pattern:
             * preset "start", if no string pattern exists at all;
             * else select "end"
             */
            if (MatchPatternDialog.currentStringPatterns.dspNumberOfPatterns == 0)
              RadioButtonChangeState(MatchPatternDialog.sptStartW, True, True);
            else
              RadioButtonChangeState(MatchPatternDialog.sptEndW, True, True);

            RadioButtonChangeState(MatchPatternDialog.sabCaseSensitiveW, True,  False);
            RadioButtonChangeState(MatchPatternDialog.sabRegularExpressionW, False,  False);
        }
    }
    else
    {
        if (isRelatedToGroup)
        {
            updateSubPatternNameMenu(element->dspeText, False);
        }
        else
        {
            XmTextSetString(MatchPatternDialog.mpdStringPatternW, element->dspeText);

            peKind       = element->dspeKind;
            wordBoundary = element->dspeWordBoundary;

            if (peKind == PEK_START)
                RadioButtonChangeState(MatchPatternDialog.sptStartW, True, True);
            else if (peKind == PEK_MIDDLE)
                RadioButtonChangeState(MatchPatternDialog.sptMiddleW, True, True);
            else
                RadioButtonChangeState(MatchPatternDialog.sptEndW, True, True);

            if (wordBoundary == PWB_BOTH)
                RadioButtonChangeState(MatchPatternDialog.wbbBothW, True, True);
            else if (wordBoundary == PWB_LEFT)
                RadioButtonChangeState(MatchPatternDialog.wbbLeftW, True, True);
            else if (wordBoundary == PWB_RIGHT)
                RadioButtonChangeState(MatchPatternDialog.wbbRightW, True, True);
            else
                RadioButtonChangeState(MatchPatternDialog.wbbNoneW, True, True);

            RadioButtonChangeState(
              MatchPatternDialog.sabCaseSensitiveW,
              !element->dspeCaseInsensitive,
              False);

            RadioButtonChangeState(
              MatchPatternDialog.sabRegularExpressionW,
              element->dspeRegularExpression,
              False);

            setSensitiveWordBoundaryBox( !element->dspeRegularExpression );
        }
    }
}

static void freeStringPatternItemCB(void *item)
{
    DialogStringPatternElement *patElement;

    patElement = (DialogStringPatternElement *)item;

    freeDialogStringPatternElement(patElement);
}

static void destroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    freeVariableDialogData(DISCARD_LANGUAGE_MODE);

    MatchPatternDialog.mpdShell = NULL;
}

static void okCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /*
     * change the matching pattern
     */
    if (!getAndUpdateStringMatchTable())
        return;

    /*
     * pop down and destroy the dialog
     */
    XtDestroyWidget(MatchPatternDialog.mpdShell);
}

static void applyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /*
     * change the matching pattern
     */
    getAndUpdateStringMatchTable();
}

static void checkCB(Widget w, XtPointer clientData, XtPointer callData)
{
    StringMatchTable *newTable;
    DMPTranslationResult translResult;

    /*
     * Get the current information displayed by the dialog.
     * If a new table is available, then the test is passed successfully.
     */
    newTable = getDialogStringMatchTable(&translResult);

    if (newTable != NULL)
    {
        DialogF(
          DF_INF, MatchPatternDialog.mpdShell, 1,
          "Matching Patterns Checked",
          "Matching Patterns checked without error",
          "OK");

        freeStringMatchTable(newTable);
    }
}

static void restoreCB(Widget w, XtPointer clientData, XtPointer callData)
{
    StringMatchTable *defaultTable;

    defaultTable = readDefaultStringMatchTable(MatchPatternDialog.mpdLangModeName);

    if (defaultTable == NULL)
    {
        DialogF(
          DF_WARN, MatchPatternDialog.mpdShell, 1,
          "No Default Matching Pattern",
          "There is no default matching pattern set\nfor language mode %s",
          "OK",
          MatchPatternDialog.mpdLangModeName);

        return;
    }

    if (DialogF(
          DF_WARN, MatchPatternDialog.mpdShell, 2,
          "Discard Changes",
          "Are you sure you want to discard\n"
          "all changes to matching patterns\n"
          "for language mode %s?",
          "Discard", "Cancel",
          MatchPatternDialog.mpdLangModeName) == 2)
    {
        freeStringMatchTable(defaultTable);

        return;
    }

    /*
     * if a stored version of the matching pattern set exists, replace it.
     * if it doesn't, add a new one.
     */
    updateStringMatchTable( defaultTable );

    /*
     * free the old dialog information
     */
    freeVariableDialogData(KEEP_LANGUAGE_MODE);

    /*
     * update the dialog
     */
    copyStringMatchTableForDialog( defaultTable, &MatchPatternDialog.mpdTable );

    ChangeManagedListData(MatchPatternDialog.mpdMatchPatternNamesListW);
    ChangeManagedListData(MatchPatternDialog.mpdStringPatternsListW);
}

static void deleteCB(Widget w, XtPointer clientData, XtPointer callData)
{
    if (DialogF(
          DF_WARN, MatchPatternDialog.mpdShell, 2,
            "Delete Matching Patterns",
            "Are you sure you want to delete\n"
            "all matching patterns for\n"
            "language mode %s?",
            "Yes, Delete", "Cancel",
            MatchPatternDialog.mpdLangModeName) == 2)
    {
        return;
    }

    /*
     * if a stored version of the matching pattern exists, delete it from the list
     */
    DeleteStringMatchTable(MatchPatternDialog.mpdLangModeName);

    /*
     * free the old dialog information
     */
    freeVariableDialogData(KEEP_LANGUAGE_MODE);

    /*
     * clear out the dialog
     */
    ChangeManagedListData(MatchPatternDialog.mpdMatchPatternNamesListW);
    ChangeManagedListData(MatchPatternDialog.mpdStringPatternsListW);
}

static void closeCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /*
     * pop down and destroy the dialog
     */
    XtDestroyWidget(MatchPatternDialog.mpdShell);
}

static void helpCB(Widget w, XtPointer clientData, XtPointer callData)
{

   Help(HELP_MATCHING_PATTERNS);

}

static void strPatRegExpressionCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int regularExpression =
          XmToggleButtonGetState(MatchPatternDialog.sabRegularExpressionW);

    setSensitiveWordBoundaryBox( !regularExpression );

    if (regularExpression)
        RadioButtonChangeState(MatchPatternDialog.sabCaseSensitiveW, True,  False);
}

static void matchPatTypeCB(Widget w, XtPointer clientData, XtPointer callData)
{
    if (MatchPatternDialog.currentDmptSeqElement != NULL)
    {
        if (XmToggleButtonGetState(MatchPatternDialog.mptbContextGroupW))
        {
            if (MatchPatternDialog.currentDmptSeqElement->dmpseType == MPT_SUB)
            {
                changeExistingSubPattern("Change to Context Group");
            }

            if (XmToggleButtonGetState(MatchPatternDialog.mptbContextGroupW) &&
                MatchPatternDialog.currentDmptSeqElement->dmpseType != MPT_GROUP)
            {
                changeStringPatternToGroup();
            }
        }
        else if (XmToggleButtonGetState(MatchPatternDialog.mptbIndividualW))
        {
            if (MatchPatternDialog.currentDmptSeqElement->dmpseType == MPT_SUB)
            {
                changeExistingSubPattern("Change to Individual Matching Pattern");
            }
            else if (MatchPatternDialog.currentDmptSeqElement->dmpseType == MPT_GROUP)
            {
                changeGroupToStringPattern("Change to Individual Matching Pattern");
            }
        }
        else if (XmToggleButtonGetState(MatchPatternDialog.mptbSubPatternW))
        {
            if (MatchPatternDialog.currentDmptSeqElement->dmpseType == MPT_GROUP)
            {
                changeGroupToStringPattern("Change to Sub-Matching Pattern");
            }
        }
    }

    /*
     * if context group button is (still) selected, then update labels etc.
     */
    if (XmToggleButtonGetState(MatchPatternDialog.mptbContextGroupW))
    {
        setDialogType(CONTEXT_GROUP_DIALOG);
    }
    else
    {
        setDialogType(STRING_PATTERN_DIALOG);
    }

    /*
     * if a "new" entry is selected in matching patterns names list, then provide a
     * list of all sub-pattern names
     */
    if (MatchPatternDialog.currentDmptSeqElement == NULL)
    {
        updateSubPatternNameMenu(NULL, True);
    }
}

static void changeExistingSubPattern(
  char *warnTitle)
{
    DialogMatchPatternGroupElement *group;
    int resp;

    group =
      getDialogGroupUsingMatchPattern(
        MatchPatternDialog.currentDmptElement->dmpteName );

    while ( group != NULL )
    {
        resp =
          DialogF(
            DF_WARN, MatchPatternDialog.mpdShell, 3,
            warnTitle,
            "Sub-pattern '%s' is used at least\n"
            "by context group '%s'.\n\n"
            "Remove this sub-pattern from this resp. all context group(s) ?",
            "No, Keep", "Yes, Remove", "Yes, Remove All",
            MatchPatternDialog.currentDmptElement->dmpteName,
            group->dmpgeName);

        if (resp == 1)
        {
            RadioButtonChangeState(MatchPatternDialog.mptbSubPatternW, True, True);

            return;
        }
        else if (resp == 2)
        {
            removeMatchPatternFromGroup(
              MatchPatternDialog.currentDmptElement->dmpteName,
              group);

            /*
             * look for evtl. next context group holding this matching pattern
             */
            group =
              getDialogGroupUsingMatchPattern(
                MatchPatternDialog.currentDmptElement->dmpteName );
        }
        else
        {
            /*
             * remove this matching pattern form all context groups
             */
            removeMatchPatternFromAllGroups(
              MatchPatternDialog.currentDmptElement->dmpteName);

            return;
        }
    }
}

static void changeStringPatternToGroup(void)
{
    int resp;
    int isSubPattern;

    if (MatchPatternDialog.currentStringPatterns.dspNumberOfPatterns != 0)
    {
        resp =
          DialogF(
            DF_WARN, MatchPatternDialog.mpdShell, 2,
            "Change to Context Group",
            "All string patterns of '%s'\n"
            "need to be discarded.\n\n"
            "Discard related string patterns ?",
            "No, Keep", "Yes, Discard",
            MatchPatternDialog.currentDmptElement->dmpteName);

        if (resp == 1)
        {
            isSubPattern = (MatchPatternDialog.currentDmptElement->dmpteType == MPT_SUB);

            if (isSubPattern)
                RadioButtonChangeState(MatchPatternDialog.mptbSubPatternW, True, True);
            else
                RadioButtonChangeState(MatchPatternDialog.mptbIndividualW, True, True);

            return;
        }

        /*
         * remove string patterns & update dialog fields
         */
        freeDialogStringPatterns(&MatchPatternDialog.currentStringPatterns);

        ChangeManagedListData(MatchPatternDialog.mpdStringPatternsListW);
    }

    /*
     * invalidate this sub-/individual pattern
     */
    MatchPatternDialog.currentDmptSeqElement->dmpseValid = False;

    /*
     * update sub-pattern menu due to change to context group
     */
    updateSubPatternNameMenu(NULL, True);
}

static void changeGroupToStringPattern(
  char *warnTitle)
{
    int resp;

    if (MatchPatternDialog.currentStringPatterns.dspNumberOfPatterns == 0)
        return;

    resp =
      DialogF(
        DF_WARN, MatchPatternDialog.mpdShell, 2,
        warnTitle,
        "Content of context group '%s'\n"
        "needs to be discarded.\n\n"
        "Discard content ?",
        "No, Keep", "Yes, Discard",
        MatchPatternDialog.currentDmptGroup->dmpgeName);

    if (resp == 1)
    {
        RadioButtonChangeState(MatchPatternDialog.mptbContextGroupW, True, True);
    }
    else
    {
        /*
         * remove string patterns & update dialog fields
         */
        freeDialogStringPatterns(&MatchPatternDialog.currentStringPatterns);

        ChangeManagedListData(MatchPatternDialog.mpdStringPatternsListW);
    }
}

/*
** Create a pulldown menu pane with the names of the sub-patterns of
** the current matching pattern set.
*/
static Widget createSubPatternNameMenu(
  Widget  parent,
  char   *currentSubPatName,
  int     allSubPatterns)
{
    NameList nameList;
    Widget menu;
    int i;

    setupSubPatternNameList(currentSubPatName, allSubPatterns, &nameList);

    menu = CreatePulldownMenu(parent, "subPatternNames", NULL, 0);

    for (i=0; i<nameList.nlNumber; i++)
    {
        createSubPatNameMenuEntry(menu, nameList.nlId[i]);
    }

    return menu;
}

static void setupSubPatternNameList(
  char     *currentSubPatName,
  int       allSubPatterns,
  NameList *nameList)
{
    int n = 0;
    int i;
    DialogMatchPatternSequenceElement *seq;
    int isRelatedToGroup = !MatchPatternDialog.mpdStringPatternIsDisplayed;

    if (isRelatedToGroup || allSubPatterns)
    {
        /*
         * add "none selected" (default) item
         */
        nameList->nlId[n ++] = SPNM_NONE_SELECTED;

        /*
         * add one item for each (not assigned) sub-pattern name
         */
        for (i=0; i<MatchPatternDialog.mpdTable.dmpiNbrOfSeqElements; i++)
        {
            seq = MatchPatternDialog.mpdTable.dmpiSequence[i];

            if (seq->dmpseType == MPT_SUB &&
                seq->dmpseValid &&
                (allSubPatterns ||
                 (!isSubPatternNameInCurStrPat(seq->dmpseName) ||
                   (currentSubPatName != NULL &&
                     (strcmp(seq->dmpseName, currentSubPatName) == 0))) ))
            {
                nameList->nlId[n ++] = seq->dmpseName;
            }
        }
    }
    else
    {
        nameList->nlId[n ++] = "none available              ";
    }

    nameList->nlNumber = n;
}

/*
** Create a menu entry with the names of one sub-pattern.
** XmNuserData of this item contains the sub-pattern name.
*/
static void createSubPatNameMenuEntry(
  Widget  menu,
  char   *subPatName)
{
    Widget btn;
    XmString s1;

    btn = XtVaCreateManagedWidget("subPattern", xmPushButtonGadgetClass,
            menu,
            XmNlabelString, s1=XmStringCreateSimple(subPatName),
            XmNmarginHeight, 0,
            XmNuserData, (void *)subPatName, NULL);
    XmStringFree(s1);
}

/*
** Set the sub-patterns menu to show a particular sub-pattern name
*/
static void setSubPatternNameMenu(
  const char *subPatName)
{
    int i;
    Cardinal nItems;
    WidgetList items;
    Widget pulldown, selectedItem;
    char *itemName;

    XtVaGetValues(MatchPatternDialog.mpdSubPatNamesOptMenu, XmNsubMenuId, &pulldown, NULL);
    XtVaGetValues(pulldown, XmNchildren, &items, XmNnumChildren, &nItems, NULL);

    if (nItems == 0)
        return;

    selectedItem = items[0];

    /*
     * if no subPatName is given, then select first item of option menu
     */
    if (subPatName != NULL)
    {
        for (i=0; i<(int)nItems; i++)
        {
            if (items[i] != NULL && !items[i]->core.being_destroyed)
            {
                XtVaGetValues(items[i], XmNuserData, &itemName, NULL);
                if (!strcmp(itemName, subPatName))
                {
                    selectedItem = items[i];
                    break;
                }
            }
        }
    }

    XtVaSetValues(MatchPatternDialog.mpdSubPatNamesOptMenu, XmNmenuHistory, selectedItem, NULL);
}

/*
** Update sub-pattern names menu, e.g. when a new sub-pattern is defined
*/
static void updateSubPatternNameMenu(
  char *currentSubPatName,
  int   allSubPatterns)
{
    NameList nameList;
    WidgetList items;
    Cardinal nItems;
    int n;
    XmString st1;

    setupSubPatternNameList(currentSubPatName, allSubPatterns, &nameList);

    /*
     * Go thru all of the items in the sub-pattern names menu
     * and rename them to match the current sub-patterns.
     * Delete any extras.
     */
    XtVaGetValues(
      MatchPatternDialog.mpdSubPatNamesPulldown,
      XmNchildren, &items,
      XmNnumChildren, &nItems,
      NULL);

    for (n=0; n<(int)nItems; n++)
    {
        if (n >= nameList.nlNumber)
        {
            /*
             * unmanaging before destroying stops parent from displaying
             */
            XtUnmanageChild(items[n]);
            XtDestroyWidget(items[n]);
        }
        else
        {
            if (items[n] == NULL || items[n]->core.being_destroyed)
            {
                /*
                 * create a new entry (widget) if this one is not existing or
                 * if it is marked as to be destroyed
                 */
                createSubPatNameMenuEntry(
                  MatchPatternDialog.mpdSubPatNamesPulldown, nameList.nlId[n]);
            }
            else
            {
                XtVaSetValues(
                  items[n],
                  XmNlabelString, st1=XmStringCreateSimple(nameList.nlId[n]),
                  XmNuserData, (void *)nameList.nlId[n],
                  NULL);

                XmStringFree(st1);
            }
        }
    }

    /*
     * add new items for remaining sub-patterns names
     */
    for (n=(int)nItems; n<nameList.nlNumber; n++)
    {
        createSubPatNameMenuEntry(
          MatchPatternDialog.mpdSubPatNamesPulldown, nameList.nlId[n]);
    }

    /*
     * select entry shown in sub-pattern name option menu
     */
    setSubPatternNameMenu(currentSubPatName);
}

static char *getSelectedSubPatternName(void)
{
    Widget selectedItem;
    char *itemName;

    XtVaGetValues(MatchPatternDialog.mpdSubPatNamesOptMenu, XmNmenuHistory, &selectedItem, NULL);
    XtVaGetValues(selectedItem, XmNuserData, &itemName, NULL);

    return itemName;
}

static int isSubPatternNameInCurStrPat(
  char *subPatName)
{
    int i;
    DialogStringPatterns *curPatNames = &MatchPatternDialog.currentStringPatterns;

    for (i=0; i<curPatNames->dspNumberOfPatterns; i++)
    {
        if (strcmp(curPatNames->dspElements[i]->dspeText, subPatName) == 0)
          return True;
    }

    return False;
}

/*
** Read the matching pattern fields of the matching pattern dialog and produce an
** allocated DialogMatchPatternSequenceElement structure reflecting the contents.
** Pop up dialogs telling the user what's wrong (Passing "silent" as True,
** suppresses these dialogs).
** Returns NULL on error.
*/
static DialogMatchPatternSequenceElement *readMatchPatternFields(int silent)
{
    int isGroup;
    char *name;
    char *nameLabel;
    char *nameTitle;
    char *contentTitle;
    char *contentWarningText;
    DialogMatchPatternSequenceElement *newSeq;
    DialogMatchPatternTableElement    *newElement;
    DialogMatchPatternGroupElement    *newGroup;

    if (XmToggleButtonGetState(MatchPatternDialog.mptbContextGroupW))
    {
        nameLabel          = "context group name";
        nameTitle          = "Context Group Name";
        contentTitle       = "Context Group Content";
        contentWarningText = "Please assign min. 1\nsub-pattern";
        isGroup            = True;
    }
    else
    {
        nameLabel          = "matching pattern name";
        nameTitle          = "Matching Pattern Name";
        contentTitle       = "Matching Pattern Content";
        contentWarningText = "Please specify min. 1\nstring pattern";
        isGroup            = False;
    }

    name =
      ReadSymbolicFieldTextWidget(
        MatchPatternDialog.mpdMatchPatternNameW,
        nameLabel,
        silent);

    if (name == NULL)
    {
        return NULL;
    }
    else if (*name == '\0')
    {
        if (!silent)
        {
            DialogF(
              DF_WARN, MatchPatternDialog.mpdShell, 1,
              nameTitle,
              "Please specify a valid name",
              "OK");
            XmProcessTraversal(MatchPatternDialog.mpdMatchPatternNameW, XmTRAVERSE_CURRENT);
        }
        XtFree(name);
        return NULL;
    }

    if (MatchPatternDialog.currentStringPatterns.dspNumberOfPatterns == 0)
    {
        if (!silent)
        {
            DialogF(
              DF_WARN, MatchPatternDialog.mpdShell, 1,
              contentTitle,
              contentWarningText,
              "OK");
        }
        XtFree(name);
        return NULL;
    }
    else
    {
        if (!isGroup &&
            !isStartPatternElementAvailable(&MatchPatternDialog.currentStringPatterns))
        {
            if (!silent)
            {
                DialogF(
                  DF_WARN, MatchPatternDialog.mpdShell, 1,
                  contentTitle,
                  "Please specify min. 1 string pattern\nof type 'start'",
                  "OK");
            }
            XtFree(name);
            return NULL;
        }
    }

    if (MatchPatternDialog.currentDmptElement != NULL &&
        MatchPatternDialog.currentDmptElement->dmpteType == MPT_SUB &&
        strcmp(MatchPatternDialog.currentDmptElement->dmpteName, name) != 0)
    {
        renameMatchPatternInAllGroups(
          MatchPatternDialog.currentDmptElement->dmpteName,
          name);
    }

    newSeq =
      (DialogMatchPatternSequenceElement *)XtMalloc(sizeof(DialogMatchPatternSequenceElement));

    newSeq->dmpseName = name;
    newSeq->dmpseValid = True;

    if (isGroup)
    {
        newSeq->dmpseType = MPT_GROUP;

        newGroup =
          (DialogMatchPatternGroupElement *)XtMalloc(sizeof(DialogMatchPatternGroupElement));

        newGroup->dmpgeName = XtNewString(name);

        copyDialogPatternNamesToGroup(
          &MatchPatternDialog.currentStringPatterns,
          newGroup);

        newSeq->dmpsePtr = (void *)newGroup;
    }
    else
    {
        if (XmToggleButtonGetState(MatchPatternDialog.mptbIndividualW))
            newSeq->dmpseType = MPT_INDIVIDUAL;
        else
            newSeq->dmpseType = MPT_SUB;

        newElement =
          (DialogMatchPatternTableElement *)XtMalloc(sizeof(DialogMatchPatternTableElement));

        newElement->dmpteName = XtNewString(name);
        newElement->dmpteType = newSeq->dmpseType;

        newElement->dmpteSkipBtwnStartEnd =
          XmToggleButtonGetState(MatchPatternDialog.gabSkipBtwStartEndW);

        newElement->dmpteFlash =
          XmToggleButtonGetState(MatchPatternDialog.gabFlashW);

        newElement->dmpteIgnoreHighLightInfo =
          !XmToggleButtonGetState(MatchPatternDialog.gabSyntaxBasedW);

        copyDialogStringPatterns(
          &MatchPatternDialog.currentStringPatterns,
          &newElement->dmptePatterns);

        newSeq->dmpsePtr = (void *)newElement;
    }

    return newSeq;
}

/*
** Check, if min. 1 string pattern of type 'start' is
** available within dialog.
** Returns True, if min. 1 start string pattern is
** populated in dialog.
*/
static int isStartPatternElementAvailable(
  DialogStringPatterns *dialogPatterns)
{
    int i;

    for (i=0; i < dialogPatterns->dspNumberOfPatterns; i++)
    {
        if (dialogPatterns->dspElements[i]->dspeKind == PEK_START)
            return True;
    }

    return False;
}

/*
** Read the string pattern fields of the string pattern frame and produce an
** allocated DialogStringPatternElement structure reflecting the contents.
** Pop up dialogs telling the user what's wrong (Passing "silent" as True,
** suppresses these dialogs).
** Returns NULL on error.
*/
static DialogStringPatternElement *readStringPatternFrameFields(int silent)
{
    char *stringPatText;
    DialogStringPatternElement *newPatElement;
    int isRelatedToGroup = !MatchPatternDialog.mpdStringPatternIsDisplayed;

    if (isRelatedToGroup)
    {
        stringPatText = getSelectedSubPatternName();
        if (strcmp(stringPatText, SPNM_NONE_SELECTED) == 0)
        {
            return NULL;
        }

        stringPatText = XtNewString(stringPatText);
    }
    else
    {
        stringPatText =
          XmTextGetString(MatchPatternDialog.mpdStringPatternW);

        if (stringPatText == NULL)
        {
            return NULL;
        }
        else if (*stringPatText == '\0')
        {
            if (!silent)
            {
                DialogF(
                  DF_WARN, MatchPatternDialog.mpdShell, 1,
                  "String Pattern",
                  "Please specify string\npattern content",
                  "OK");
                XmProcessTraversal(MatchPatternDialog.mpdStringPatternW, XmTRAVERSE_CURRENT);
            }
            XtFree(stringPatText);
            return NULL;
        }
    }

    newPatElement = (DialogStringPatternElement *)XtMalloc(sizeof(DialogStringPatternElement));

    newPatElement->dspeText = stringPatText;

    if (isRelatedToGroup)
    {
        newPatElement->dspeKind              = PEK_START;
        newPatElement->dspeWordBoundary      = PWB_NONE;
        newPatElement->dspeCaseInsensitive   = False;
        newPatElement->dspeRegularExpression = False;
    }
    else
    {
        newPatElement->dspeRegularExpression =
          XmToggleButtonGetState(MatchPatternDialog.sabRegularExpressionW);

        if (XmToggleButtonGetState(MatchPatternDialog.sptStartW))
            newPatElement->dspeKind = PEK_START;
        else if (XmToggleButtonGetState(MatchPatternDialog.sptMiddleW))
            newPatElement->dspeKind = PEK_MIDDLE;
        else
            newPatElement->dspeKind = PEK_END;

        if (newPatElement->dspeRegularExpression)
            newPatElement->dspeWordBoundary = PWB_NONE;
        else if (XmToggleButtonGetState(MatchPatternDialog.wbbBothW))
            newPatElement->dspeWordBoundary = PWB_BOTH;
        else if (XmToggleButtonGetState(MatchPatternDialog.wbbLeftW))
            newPatElement->dspeWordBoundary = PWB_LEFT;
        else if (XmToggleButtonGetState(MatchPatternDialog.wbbRightW))
            newPatElement->dspeWordBoundary = PWB_RIGHT;
        else
            newPatElement->dspeWordBoundary = PWB_NONE;

        newPatElement->dspeCaseInsensitive =
          !XmToggleButtonGetState(MatchPatternDialog.sabCaseSensitiveW);
    }

    return newPatElement;
}

/*
** Returns true if the pattern fields of the matching pattern dialog are set to
** the default ("New" pattern) state.
*/
static int matchPatternDialogEmpty(void)
{
    return
      TextWidgetIsBlank(MatchPatternDialog.mpdMatchPatternNameW) &&
      XmToggleButtonGetState(MatchPatternDialog.mptbIndividualW) &&
      XmToggleButtonGetState(MatchPatternDialog.gabFlashW) &&
      XmToggleButtonGetState(MatchPatternDialog.gabSyntaxBasedW) &&
      stringPatternFrameEmpty();
}

/*
** Returns true if the string pattern frame of the matching pattern dialog is set to
** the default state.
*/
static int stringPatternFrameEmpty(void)
{
    return
      stringPatternFieldsEmpty(False) &&
      MatchPatternDialog.currentStringPatterns.dspNumberOfPatterns == 0;
}

/*
** Returns true if the string pattern fields of the string pattern frame are set to
** the default state.
*/
static int stringPatternFieldsEmpty(
  int strPatIsRelatedToGroup)
{
    if (strPatIsRelatedToGroup)
    {
        return(
          strcmp( getSelectedSubPatternName(), SPNM_NONE_SELECTED ) == 0);
    }
    else
    {
        return
          TextWidgetIsBlank(MatchPatternDialog.mpdStringPatternW) &&
          XmToggleButtonGetState(MatchPatternDialog.wbbBothW) &&
          XmToggleButtonGetState(MatchPatternDialog.sabCaseSensitiveW) &&
          !XmToggleButtonGetState(MatchPatternDialog.sabRegularExpressionW);
    }
}

/*
** Get the current content of the matching pattern dialog.
** If the matching pattern is o.k., then update & apply it
** to any window which is currently using the matching pattern.
** If it's bad, then only report it.
*/
static int getAndUpdateStringMatchTable(void)
{
    StringMatchTable *newTable;
    DMPTranslationResult translResult;

    /*
     * Get the current information displayed by the dialog.  If it's bad,
     * report it to the user & return.
     */
    newTable = getDialogStringMatchTable(&translResult);

    if (newTable == NULL && translResult != DMPTR_EMPTY)
    {
        DialogF(
          DF_WARN, MatchPatternDialog.mpdShell, 1,
          "Incomplete Matching Patterns for Language Mode",
          "Incomplete matching patterns for language mode '%s'.\n"
          "Please complete them first",
          "OK",
          MatchPatternDialog.mpdLangModeName);

        return False;
    }

    /*
     * change the matching pattern
     */
    updateStringMatchTable( newTable );

    return True;
}

/*
** Update the matching pattern set being edited in the matching pattern dialog
** with the information that the dialog is currently displaying, and
** apply changes to any window which is currently using the matching pattern.
*/
static void updateStringMatchTable(
  StringMatchTable *newTable)
{
    WindowInfo *window;
    int i;

    /*
     * Find the matching pattern being modified
     */
    for (i=0; i<NbrMatchTables; i++)
    {
        if (!strcmp(MatchPatternDialog.mpdLangModeName, MatchTables[i]->smtLanguageMode))
        {
            /*
             * replace existing matching pattern
             */
            freeStringMatchTable(MatchTables[i]);
            MatchTables[i] = newTable;
            break;
        }
    }

    if (i == NbrMatchTables)
    {
        /*
         * new match table for language mode -> add it to end
         */
        MatchTables[NbrMatchTables++] = newTable;
    }

    /*
     * Find windows that are currently using this matching pattern set and
     * update this windows
     */
    for (window=WindowList; window!=NULL; window=window->next)
    {
        if ((window->languageMode == PLAIN_LANGUAGE_MODE &&
             !strcmp(PLAIN_LM_STRING, newTable->smtLanguageMode)) ||
            (window->languageMode != PLAIN_LANGUAGE_MODE &&
             !strcmp(LanguageModeName(window->languageMode), newTable->smtLanguageMode)))
        {
            window->stringMatchTable = newTable;
        }
    }

    /*
     * Note that preferences have been changed
     */
    MarkPrefsChanged();
}

static StringMatchTable *getDialogStringMatchTable(
  DMPTranslationResult *result)
{
    int matchPatListIdx =
          ManagedListSelectedIndex(MatchPatternDialog.mpdMatchPatternNamesListW);
    int stringPatListIdx =
          ManagedListSelectedIndex(MatchPatternDialog.mpdStringPatternsListW);

    /*
     * Get the current content of the matching pattern dialog fields
     */
    if (!UpdateManagedList(MatchPatternDialog.mpdStringPatternsListW, True))
    {
        *result = DMPTR_INCOMPLETE;

        return NULL;
    }

    if (!UpdateManagedList(MatchPatternDialog.mpdMatchPatternNamesListW, True))
    {
        *result = DMPTR_INCOMPLETE;

        return NULL;
    }

    SelectManagedListItem(MatchPatternDialog.mpdMatchPatternNamesListW, matchPatListIdx);
    SelectManagedListItem(MatchPatternDialog.mpdStringPatternsListW, stringPatListIdx);

    /*
     * Translate dialog match table to string match table
     */
    return translateDialogStringMatchTable(&MatchPatternDialog.mpdTable, result);
}

static StringMatchTable *translateDialogStringMatchTable(
  DialogMatchPatternInfo *dialogTable,
  DMPTranslationResult   *result)
{
    ReadMatchPatternInfo readPatInfo;
    DialogMatchPatternSequenceElement *seq;
    MatchPatternTableElement *newPatElement;
    MatchPatternGroupElement *newGroupElement;
    char *errMsg;
    ErrorInfo errInfo;
    int i;

    initErrorInfo(&errInfo);

    readPatInfo.rmpiNbrOfElements    = 0;
    readPatInfo.rmpiNbrOfGroups      = 0;
    readPatInfo.rmpiNbrOfSeqElements = 0;
    readPatInfo.rmpiAllPatRE         = NULL;
    readPatInfo.rmpiFlashPatRE       = NULL;

    /*
     * if no dialog patterns are defined, return "empty" table
     */
    if (dialogTable->dmpiNbrOfSeqElements == 0)
    {
        *result = DMPTR_EMPTY;

        return createStringMatchTable(
                 &readPatInfo,
                 XtNewString(MatchPatternDialog.mpdLangModeName));
    }

    /*
     * translate dialog matching pattern elements
     */
    for (i=0; i < dialogTable->dmpiNbrOfSeqElements; i++)
    {
        seq = dialogTable->dmpiSequence[i];

        if (seq->dmpseType == MPT_GROUP)
        {
            newGroupElement =
              translateDialogMatchPatternGroupElement(
                &readPatInfo,
                (DialogMatchPatternGroupElement *)seq->dmpsePtr);

            if (newGroupElement == NULL)
            {
                freeReadMatchPatternInfo(&readPatInfo);

                *result = DMPTR_INCOMPLETE;

                return NULL;
            }

            readPatInfo.rmpiGroup[readPatInfo.rmpiNbrOfGroups ++] =
              newGroupElement;

            recordPatternSequence(
              &readPatInfo,
              seq->dmpseName,
              seq->dmpseType,
              readPatInfo.rmpiNbrOfGroups-1 );
        }
        else
        {
            newPatElement =
              translateDialogMatchPatternTableElement(
              (DialogMatchPatternTableElement *)seq->dmpsePtr);

            newPatElement->mpteIndex = readPatInfo.rmpiNbrOfElements;

            readPatInfo.rmpiElement[readPatInfo.rmpiNbrOfElements ++] =
              newPatElement;

            if (newPatElement->mpteType == MPT_INDIVIDUAL)
            {
                if (!assignIndividualGroup(&readPatInfo, &errMsg, newPatElement))
                {
                    DialogF(
                      DF_WARN, MatchPatternDialog.mpdShell, 1,
                      "Assign reg. exp.",
                      "%s\n(Pattern:  '%s')",
                      "OK",
                      errMsg,
                      newPatElement->mpteName);

                    freeReadMatchPatternInfo(&readPatInfo);

                    *result = DMPTR_INCOMPLETE;

                    return NULL;
                }
            }

            treatDuplicatedMTEntries(readPatInfo.rmpiElement, readPatInfo.rmpiNbrOfElements);

            recordPatternSequence(
              &readPatInfo,
              seq->dmpseName,
              seq->dmpseType,
              newPatElement->mpteIndex );
        }
    }

    /*
     * compile reg. expressions of "read" patterns
     */
    if (createRegExpOfPatterns( &readPatInfo, &errInfo ))
    {
        errInfo.eiLanguageMode = XtNewString(MatchPatternDialog.mpdLangModeName);
        dialogMatchingPatternSetError(
          "Assign all patterns reg. exp.",
          &errInfo);

        freeReadMatchPatternInfo(&readPatInfo);

        *result = DMPTR_INCOMPLETE;

        return NULL;
    }

    *result = DMPTR_OK;

    return createStringMatchTable(
             &readPatInfo,
             XtNewString(MatchPatternDialog.mpdLangModeName));
}

static MatchPatternTableElement *translateDialogMatchPatternTableElement(
  DialogMatchPatternTableElement *dialogElement)
{
    MatchPatternTableElement *newElement;

    newElement =
      (MatchPatternTableElement *)XtMalloc(sizeof(MatchPatternTableElement));

    newElement->mpteName  = XtNewString(dialogElement->dmpteName);
    newElement->mpteIndex = NO_ELEMENT_IDX;
    newElement->mpteType  = dialogElement->dmpteType;
    newElement->mpteGroup = NO_GROUP_IDX;

    translateDialogPatterns(&dialogElement->dmptePatterns, newElement);

    newElement->mpteSkipBtwnStartEnd    = dialogElement->dmpteSkipBtwnStartEnd;
    newElement->mpteFlash               = dialogElement->dmpteFlash;
    newElement->mpteIgnoreHighLightInfo = dialogElement->dmpteIgnoreHighLightInfo;
    newElement->mpteStartEndRE = NULL;

    initGlobalBackRefList( newElement->mpteGlobalBackRef );

    return newElement;
}

static void translateDialogPatterns(
  DialogStringPatterns     *dialogPatterns,
  MatchPatternTableElement *newElement)
{
    int sizeOfPat;
    int i;

    /*
     * allocate memory for patterns
     */
    newElement->mpteAll.pesNumberOfPattern = dialogPatterns->dspNumberOfPatterns;

    sizeOfPat =
      sizeof(PatternElement *) * dialogPatterns->dspNumberOfPatterns;

    newElement->mpteAll.pesPattern = (PatternElement **)XtMalloc(sizeOfPat);

    /*
     * assign dialog patterns to patterns of MatchPatternTableElement
     */
    for (i=0; i < dialogPatterns->dspNumberOfPatterns; i++)
    {
        newElement->mpteAll.pesPattern[i] =
          createPatternElement(
            XtNewString(dialogPatterns->dspElements[i]->dspeText),
            dialogPatterns->dspElements[i]->dspeKind,
            dialogPatterns->dspElements[i]->dspeWordBoundary,
            dialogPatterns->dspElements[i]->dspeCaseInsensitive,
            dialogPatterns->dspElements[i]->dspeRegularExpression);

        newElement->mpteAll.pesPattern[i]->peIndex = i;
    }

    /*
     * sort pattern elements into start, middle & end arrays
     */
    sortDialogPatternElementSet( &newElement->mpteAll, newElement );

    /*
     * determine mono pattern
     */
    if (newElement->mpteEnd.pesNumberOfPattern == 0)
    {
        newElement->mpteIsMonoPattern = True;

        copyPatternSet( &newElement->mpteStart, &newElement->mpteEnd );
    }
    else
    {
        newElement->mpteIsMonoPattern = False;
    }
}

/*
 * Sort dialog pattern element set into start, middle & end arrays.
 */
static void sortDialogPatternElementSet(
  PatternElementSet         *allPat,
  MatchPatternTableElement  *result)
{
    int sizeOfPat;

    /*
     * count number of start, middle & end pattern elements
     */
    countPatternElementKind( allPat, result );

    /*
     * allocate pattern elements
     */
    sizeOfPat = sizeof(PatternElement *) * result->mpteStart.pesNumberOfPattern;
    result->mpteStart.pesPattern = (PatternElement **)XtMalloc( sizeOfPat );

    if (result->mpteMiddle.pesNumberOfPattern != 0)
    {
        sizeOfPat = sizeof(PatternElement *) * result->mpteMiddle.pesNumberOfPattern;
        result->mpteMiddle.pesPattern = (PatternElement **)XtMalloc( sizeOfPat );
    }

    if (result->mpteEnd.pesNumberOfPattern != 0)
    {
        sizeOfPat = sizeof(PatternElement *) * result->mpteEnd.pesNumberOfPattern;
        result->mpteEnd.pesPattern = (PatternElement **)XtMalloc( sizeOfPat );
    }

    /*
     * sort pattern elements into start, middle & end arrays
     */
    sortPatternElementSet( allPat, result );
}

static MatchPatternGroupElement *translateDialogMatchPatternGroupElement(
  ReadMatchPatternInfo           *info,
  DialogMatchPatternGroupElement *dialogGroup)
{
    int i;
    int error = False;
    MatchPatternTableElement *subPatElement;
    int sizeOfIds;
    MatchPatternGroupElement *group;

    /*
     * Allocate memory for the matching pattern group. Copy name & number of
     * sub patterns.
     */
    group =
      (MatchPatternGroupElement *)XtMalloc( sizeof(MatchPatternGroupElement) );

    group->mpgeName = XtNewString(dialogGroup->dmpgeName);
    group->mpgeNumberOfSubPatterns = dialogGroup->dmpgeNumberOfSubPatterns;
    group->mpgeKeywordRE = NULL;

    /*
     * Allocate memory for the sub-matching pattern IDs
     */
    sizeOfIds = sizeof(char *) * group->mpgeNumberOfSubPatterns;
    group->mpgeSubPatternIds = (char **)XtMalloc( sizeOfIds );

    for (i=0; i < group->mpgeNumberOfSubPatterns; i ++)
    {
        /*
         * Remember sub-matching pattern ID
         */
        group->mpgeSubPatternIds[i] = XtNewString(dialogGroup->dmpgeSubPatternIds[i]);

        /*
         * Assign the index of this group to the sub-matching pattern
         * if no group index was assigned before.
         */
        subPatElement =
          getPatternOfName( info, dialogGroup->dmpgeSubPatternIds[i]);

        if (subPatElement == NULL)
        {
            DialogF(
              DF_WARN, MatchPatternDialog.mpdShell, 1,
              "Group Compilation",
              "Group '%s':\nsub-matching pattern '%s' not defined before",
              "OK",
              group->mpgeName,
              dialogGroup->dmpgeSubPatternIds[i]);

            error = True;
        }
        else
        {
            if (subPatElement->mpteGroup == NO_GROUP_IDX)
            {
                subPatElement->mpteGroup = info->rmpiNbrOfGroups;
            }
        }
    }

    if (error)
    {
        freeMatchPatternGroupElement(group);

        return NULL;
    }

    return group;
}

static int stringMatchTableDiffer(
  StringMatchTable *oldTable,
  StringMatchTable *newTable)
{
    int i, j;
    MatchPatternTable *oldPatTab = oldTable->smtAllPatterns;
    MatchPatternTable *newPatTab = newTable->smtAllPatterns;
    MatchPatternTableElement *oldPat;
    MatchPatternTableElement *newPat;
    MatchPatternGroupElement *oldGroup;
    MatchPatternGroupElement *newGroup;
    MatchPatternSequenceElement *oldSeq;
    MatchPatternSequenceElement *newSeq;

    if (oldTable->smtNumberOfSeqElements != newTable->smtNumberOfSeqElements)
        return True;

    for (i=0; i < oldTable->smtNumberOfSeqElements; i++)
    {
        oldSeq = oldTable->smtSequence[i];
        newSeq = newTable->smtSequence[i];

        if (AllocatedStringsDiffer(oldSeq->mpseName, newSeq->mpseName))
            return True;
        if (oldSeq->mpseType != newSeq->mpseType)
            return True;

        if (oldSeq->mpseType == MPT_GROUP)
        {
            oldGroup = oldTable->smtGroups[oldSeq->mpseIndex];
            newGroup = newTable->smtGroups[newSeq->mpseIndex];

            if (AllocatedStringsDiffer(oldGroup->mpgeName, newGroup->mpgeName))
                return True;

            if (oldGroup->mpgeNumberOfSubPatterns != newGroup->mpgeNumberOfSubPatterns)
                return True;

            for (j=0; j < oldGroup->mpgeNumberOfSubPatterns; j++)
            {
                if (AllocatedStringsDiffer(
                      oldGroup->mpgeSubPatternIds[j],
                      newGroup->mpgeSubPatternIds[j]))
                    return True;
            }
        }
        else
        {
            oldPat = oldPatTab->mptElements[oldSeq->mpseIndex];
            newPat = newPatTab->mptElements[newSeq->mpseIndex];

            if (AllocatedStringsDiffer(oldPat->mpteName, newPat->mpteName))
                return True;

            if (oldPat->mpteType != newPat->mpteType)
                return True;

            if (oldPat->mpteGroup != newPat->mpteGroup)
                return True;

            if (oldPat->mpteAll.pesNumberOfPattern != newPat->mpteAll.pesNumberOfPattern)
                return True;

            for (j=0; j < oldPat->mpteAll.pesNumberOfPattern; j ++)
            {
                if (patternElementDiffer(
                      oldPat->mpteAll.pesPattern[j], oldPatTab,
                      newPat->mpteAll.pesPattern[j], newPatTab ) )
                    return True;
            }

            if (oldPat->mpteIsMonoPattern != newPat->mpteIsMonoPattern)
                return True;

            if (oldPat->mpteSkipBtwnStartEnd != newPat->mpteSkipBtwnStartEnd)
                return True;

            if (oldPat->mpteIgnoreHighLightInfo != newPat->mpteIgnoreHighLightInfo)
                return True;

            if (oldPat->mpteFlash != newPat->mpteFlash)
                return True;
        }
    }

    return False;
}

static int patternElementDiffer(
  PatternElement    *oldPE,
  MatchPatternTable *oldTab,
  PatternElement    *newPE,
  MatchPatternTable *newTab)
{
    StringPattern *oldSP;
    StringPattern *newSP;

    oldSP = GetStringPattern(oldTab, oldPE);
    newSP = GetStringPattern(newTab, newPE);

    if (AllocatedStringsDiffer(oldSP->spText, newSP->spText))
        return True;
    if (AllocatedStringsDiffer(oldSP->spOrigText, newSP->spOrigText))
        return True;
    if (oldPE->peKind != newPE->peKind)
        return True;
    if (oldSP->spWordBoundary != newSP->spWordBoundary)
        return True;
    if (oldSP->spCaseInsensitive != newSP->spCaseInsensitive)
        return True;
    if (oldSP->spRegularExpression != newSP->spRegularExpression)
        return True;

    return False;
}

static DialogMatchPatternGroupElement *getDialogGroupUsingMatchPattern(
  char *matchPatternName)
{
    DialogMatchPatternSequenceElement *seq;
    DialogMatchPatternGroupElement *group;
    int i, j;

    for (i=0; i < MatchPatternDialog.mpdTable.dmpiNbrOfSeqElements; i ++)
    {
        seq = MatchPatternDialog.mpdTable.dmpiSequence[i];

        if (seq->dmpseType == MPT_GROUP)
        {
            group = (DialogMatchPatternGroupElement *)seq->dmpsePtr;

            for (j=0; j < group->dmpgeNumberOfSubPatterns; j ++)
            {
                if (strcmp(group->dmpgeSubPatternIds[j], matchPatternName) == 0)
                    return group;
            }
        }
    }

    return NULL;
}

static void removeMatchPatternFromGroup(
  char                           *matchPatternName,
  DialogMatchPatternGroupElement *group)
{
    int i;

    for (i=0; i < group->dmpgeNumberOfSubPatterns; i ++)
    {
        if (strcmp(group->dmpgeSubPatternIds[i], matchPatternName) == 0)
        {
            /*
             * remove existing matching pattern name from sub-pattern list
             */
            freeXtPtr((void **)&group->dmpgeSubPatternIds[i]);
            memmove(
              &group->dmpgeSubPatternIds[i],
              &group->dmpgeSubPatternIds[i+1],
              (group->dmpgeNumberOfSubPatterns-1 - i) * sizeof(char *));
            group->dmpgeNumberOfSubPatterns --;

            return;
        }
    }
}

static void removeMatchPatternFromAllGroups(
  char *matchPatternName)
{
    DialogMatchPatternSequenceElement *seq;
    DialogMatchPatternGroupElement *group;
    int i;

    for (i=0; i < MatchPatternDialog.mpdTable.dmpiNbrOfSeqElements; i ++)
    {
        seq = MatchPatternDialog.mpdTable.dmpiSequence[i];

        if (seq->dmpseType == MPT_GROUP)
        {
            group = (DialogMatchPatternGroupElement *)seq->dmpsePtr;

            removeMatchPatternFromGroup(matchPatternName, group);
        }
    }
}

static void renameMatchPatternInGroup(
  char                           *oldMatchPatternName,
  char                           *newMatchPatternName,
  DialogMatchPatternGroupElement *group)
{
    int i;

    for (i=0; i < group->dmpgeNumberOfSubPatterns; i ++)
    {
        if (strcmp(group->dmpgeSubPatternIds[i], oldMatchPatternName) == 0)
        {
            /*
             * rename existing matching pattern name in sub-pattern list
             */
            freeXtPtr((void **)&group->dmpgeSubPatternIds[i]);

            group->dmpgeSubPatternIds[i] =
              XtNewString(newMatchPatternName);

            return;
        }
    }
}

static void renameMatchPatternInAllGroups(
  char *oldMatchPatternName,
  char *newMatchPatternName)
{
    DialogMatchPatternSequenceElement *seq;
    DialogMatchPatternGroupElement *group;
    int i;

    for (i=0; i < MatchPatternDialog.mpdTable.dmpiNbrOfSeqElements; i ++)
    {
        seq = MatchPatternDialog.mpdTable.dmpiSequence[i];

        if (seq->dmpseType == MPT_GROUP)
        {
            group = (DialogMatchPatternGroupElement *)seq->dmpsePtr;

            renameMatchPatternInGroup(
              oldMatchPatternName,
              newMatchPatternName,
              group);
        }
    }
}

static void freeVariableDialogData(
  int keepLanguageModeName)
{
    int i;

    if (!keepLanguageModeName)
        freeXtPtr((void **)&MatchPatternDialog.mpdLangModeName);

    for (i=0; i < MatchPatternDialog.mpdTable.dmpiNbrOfSeqElements; i ++)
    {
        freeDialogSequenceElement(
          MatchPatternDialog.mpdTable.dmpiSequence[i] );
    }

    MatchPatternDialog.mpdTable.dmpiNbrOfSeqElements = 0;

    freeDialogStringPatterns(
      &MatchPatternDialog.currentStringPatterns);
}

static void initGlobalBackRefList(
  GlobalBackRefElement *list)
{
    int i;

    for (i=0; i < MAX_GLOBAL_BACK_REF_ID; i++)
    {
        list[i].gbreDefByStrPat = NULL;
        list[i].gbreRegExpText  = NULL;
    }
}

static void initStrPatBackRefList(
  StringPattern *strPat)
{
    int i;

    for (i=0; i < MAX_GLOBAL_BACK_REF_ID; i++)
    {
        strPat->spOwnGlobalBackRef[i].spbreRegExpText     = NULL;
        strPat->spOwnGlobalBackRef[i].spbreLocalBackRefID = NO_LOCAL_BACK_REF_ID;

        strPat->spGlobalToLocalBackRef[i] = NO_LOCAL_BACK_REF_ID;
    }
}

static void initErrorInfo(
  ErrorInfo *errInfo)
{
    errInfo->eiDetail           = NULL;
    errInfo->eiLanguageMode     = NULL;
    errInfo->eiMPTabElementName = NULL;
    errInfo->eiStringPatText    = NULL;
    errInfo->eiRegExpCompileMsg = NULL;
    errInfo->eiBackRefNbr       = 0;
}

static void freeErrorInfo(
  ErrorInfo *errInfo)
{
    freeXtPtr((void **)&errInfo->eiLanguageMode);
    freeXtPtr((void **)&errInfo->eiMPTabElementName);
    freeXtPtr((void **)&errInfo->eiStringPatText);
}

static void freeXtPtr(void **ptr)
{
    if (*ptr != NULL)
    {
        XtFree((char *)*ptr);
        *ptr = NULL;
    }
}

static void freePtr(void **ptr)
{
    if (*ptr != NULL)
    {
        free((char *)*ptr);
        *ptr = NULL;
    }
}
