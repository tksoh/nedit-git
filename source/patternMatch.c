static const char CVSID[] = "$Id: patternMatch.c,v 1.4 2003/10/27 21:59:14 uleh Exp $";
/*******************************************************************************
*                                                                              *
* patternMatch.c -- Nirvana Editor pattern matching functions                  *
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

#include "regularExp.h"
#include "textBuf.h"
#include "search.h"
#include "window.h"
#include "preferences.h"
#include "highlight.h"

#include "patternMatch.h"
#include "patternMatchData.h"

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#define MAX_NESTED_PATTERNS  100

#define IGNORE_HIGHLIGHT_CODE  -1

typedef struct _SearchRegionInfo {
  WindowInfo *sriWindow;
  char       *sriText;
  char        sriPrevChar;
  char        sriSuccChar;
  int         sriStartOfTextPos;
} SearchRegionInfo;

typedef struct _FoundStringInfo {
  char       *fsiStartPtr;
  char       *fsiEndPtr;
  int         fsiLength;
  char        fsiPrevChar;
  char        fsiSuccChar;
  const char *fsiDelimiters;
} FoundStringInfo;

typedef struct _BackRefInfo {
  int   briAvailable;
  int   briCaseInsensitive;
  char *briStartPtr[MAX_GLOBAL_BACK_REF_ID];
  char *briEndPtr[MAX_GLOBAL_BACK_REF_ID];
} BackRefInfo;

typedef struct _MatchingElementInfo {
  MatchPatternTableElement *meiElement;
  PatternReference          meiPatRef;
  int                       meiDirection;
  int                       meiHighLightCode;
  int                       meiAbsStartPos;
  int                       meiLength;
  BackRefInfo               meiBackRefInfo;
} MatchingElementInfo;

typedef struct _PatternStackElement {
  PatternReference psePatRef;
  int              pseHighLightCode;
  BackRefInfo      pseBackRefInfo;
} PatternStackElement;

/*
 * Prototypes of local functions
 */
void adaptPatternPositions(
  MatchingType  matchingType,
  int           direction,
  int          *charPos,
  int           startPatternLength,
  int          *matchPos,
  int           matchedPatternLength);

static int findMatchingStringElement(
  StringMatchTable         *smTable,
  SearchRegionInfo         *searchRegion,
  MatchingElementInfo      *matchInfo,
  int                      *charPos,
  const char               *delimiters);

static PatternElementMonoInfo determineMonoPatInfo(
  WindowInfo         *window,
  int                 patHighLightCode,
  int                 leftPos,
  int                 rightPos,
  PatternElementKind *patElementKind);

static void assignBackRefInfo(
  StringPattern *strPat,
  BackRefInfo   *backRefInfo);
static int doesBackRefInfoMatch(
  BackRefInfo *backRefInfo1,
  BackRefInfo *backRefInfo2);
static int compareBackRef(
  char *startPtr1,
  char *endPtr1,
  char *startPtr2,
  char *endPtr2,
  int   caseInsensitive);

static int doesPatternElementMatch(
  PatternElement  *patElement,
  FoundStringInfo *foundStringInfo,
  BackRefInfo     *backRefInfo);
static int doesMPTableElementMatch(
  MatchPatternTableElement *element,
  FoundStringInfo          *foundStringInfo,
  PatternElementKind       *patternElementKind,
  int                      *patternElementIdx,
  BackRefInfo              *backRefInfo);
static int getPatternInfo(
  MatchPatternTable *table,
  FoundStringInfo   *foundStringInfo,
  PatternReference  *patRef,
  BackRefInfo       *backRefInfo);

static int isPartOfPattern(
  MatchPatternTable *table,
  int                parentElementIdx,
  int                childElementIdx,
  PatternElementKind patElementKind);
static int isPartOfPatternElementSet(
  PatternElementSet *patElementSet,
  int                patternElementIdx);
static int isPartOfMiddlePatternElementSet(
  PatternElementSet *patElementSet,
  int                patternElementIdx);
static void considerNextPatternReference(
  MatchPatternTable *table,
  PatternReference  *startPatRef,
  PatternReference   nxtPatRef,
  int                groupIdx);

static int searchPatternForward(
  MatchPatternTable *table,
  regexp            *compiledRE,
  SearchRegionInfo  *searchRegion,
  const char        *delimiters,
  int                beginPos,
  int               *matchEndPos,
  PatternReference  *patRef,
  int               *highLightCode,
  BackRefInfo       *backRefInfo);
static int searchPatternBackward(
  MatchPatternTable *table,
  regexp            *compiledRE,
  SearchRegionInfo  *searchRegion,
  const char        *delimiters,
  int                beginPos,
  PatternReference  *patRef,
  int               *highLightCode,
  int               *matchedPatternLength,
  BackRefInfo       *backRefInfo);

static int parseStringElementForward(
  MatchingElementInfo *matchInfo,
  SearchRegionInfo    *searchRegion,
  int                  relCharPos,
  int                 *matchPos,
  int                 *matchedPatternLength,
  const char          *delimiters);
static int findRelatedForwardPattern(
  StringMatchTable *table,
  SearchRegionInfo *searchRegion,
  const char       *delimiters,
  PatternReference  beginPatRef,
  int               beginPatHighLightCode,
  BackRefInfo      *beginPatBackRefInfo,
  int               beginPos,
  int              *matchEndPos);

static int parseStringElementBackward(
  MatchingElementInfo *matchInfo,
  SearchRegionInfo    *searchRegion,
  int                  relCharPos,
  int                 *matchPos,
  int                 *matchedPatternLength,
  const char          *delimiters);
static int findRelatedStartPattern(
  StringMatchTable *table,
  SearchRegionInfo *searchRegion,
  const char       *delimiters,
  PatternReference  beginPatRef,
  int               beginPatHighLightCode,
  BackRefInfo      *beginPatBackRefInfo,
  int               beginPos,
  int              *matchedPatternLength);
static void considerStackPatReference(
  PatternElementSet *patSet,
  int                stackElementIdx,
  int               *foundElementIdx);

static int getPatternLocatedAtPos(
  regexp              *usedPatRE,
  MatchPatternTable   *table,
  SearchRegionInfo    *searchRegion,
  int                 *relBeginPos,
  MatchingElementInfo *matchInfo,
  const char          *delimiters);
static int getMatchedElementInfo(
  WindowInfo          *window,
  MatchPatternTable   *table,
  FoundStringInfo     *foundStringInfo,
  MatchingElementInfo *matchInfo);

static PatternElement *getPatternOfReference(
  MatchPatternTable *table,
  PatternReference   patRef);

#ifdef DEBUG_FIND
static char *getPatternForDebug(
  MatchPatternTable *table,
  PatternReference   patRef );
static char *patElemKindToString(
  PatternElementKind patElemKind);
static void printFoundStringForDebug(
  WindowInfo *window,
  int         absStartPos,
  int         length);
#endif

/*
** Try to find a matching pattern string related to the given "charPos"
** inside the given range (defined by startLimit & endLimit).
** Determine the matching position & the match pattern length (depending
** on given matchingType), if a matching pattern was found.
** Returns true, if a matching pattern string was found.
*/
int FindMatchingString(
  WindowInfo   *window,
  MatchingType  matchingType,
  int          *charPos,
  int           startLimit,
  int           endLimit,
  int          *matchPos,
  int          *matchedPatternLength,
  int          *direction)
{
    StringMatchTable *smTable = (StringMatchTable *)window->stringMatchTable;
    const char *delimiters;
    SearchRegionInfo searchRegion;
    MatchingElementInfo matchInfo;
    int matchingPatternFound = FALSE;
    int relCharPos;

    if (smTable == NULL || smTable->smtAllPatRE == NULL)
    {
        /*
         * No match pattern table available:
         */
        return FALSE;
    }

    /*
     * Get delimiters related to window
     */
    delimiters = GetWindowDelimiters(window);
    if (delimiters == NULL)
        delimiters = GetPrefDelimiters();

    /*
     * Select the start pattern reg. exp. to use
     */
    if (matchingType == MT_FLASH_RANGE ||
        matchingType == MT_FLASH_DELIMIT)
    {
        smTable->smtUsedPatRE = smTable->smtFlashPatRE;
    }
    else
    {
        smTable->smtUsedPatRE = smTable->smtAllPatRE;
    }

    /*
     * Get a copy of the text buffer area to parse
     */
    searchRegion.sriWindow         = window;
    searchRegion.sriText           = BufGetRange(window->buffer, startLimit, endLimit);
    searchRegion.sriPrevChar       = BufGetCharacter(window->buffer, startLimit - 1);
    searchRegion.sriSuccChar       = BufGetCharacter(window->buffer, endLimit);
    searchRegion.sriStartOfTextPos = startLimit;

    relCharPos = *charPos - startLimit;

    /*
     * Try to find a matching pattern string using string match table
     * of window
     */
    if (findMatchingStringElement(
           smTable,
           &searchRegion,
           &matchInfo,
           &relCharPos,
           delimiters ))
    {
#ifdef DEBUG_FIND
        printf("--- Start at : ");
        printFoundStringForDebug(
          window,
          matchInfo.meiAbsStartPos,
          matchInfo.meiLength);
        printf(" ---\n");
#endif
        if (matchInfo.meiDirection == SEARCH_FORWARD)
        {
            matchingPatternFound =
              parseStringElementForward(
                &matchInfo,
                &searchRegion,
                relCharPos,
                matchPos,
                matchedPatternLength,
                delimiters );
        }
        else
        {
            matchingPatternFound =
              parseStringElementBackward(
                &matchInfo,
                &searchRegion,
                relCharPos,
                matchPos,
                matchedPatternLength,
                delimiters );
        }

        if (matchingPatternFound)
        {
            /*
             * Calc. abs. start char pos. (may have been changed if
             * cursor was located inside a string pattern). Adapt
             * pattern positions depending on matchingType.
             */
            *charPos   = relCharPos + startLimit;
            *direction = matchInfo.meiDirection;

            adaptPatternPositions(
              matchingType,
              matchInfo.meiDirection,
              charPos,
              matchInfo.meiLength,
              matchPos,
              *matchedPatternLength);
        }
    }

    XtFree(searchRegion.sriText);

    return matchingPatternFound;
}

/*
** Adapt match pattern position / start position depending on the
** given matching type.
*/
void adaptPatternPositions(
  MatchingType  matchingType,
  int           direction,
  int          *charPos,
  int           startPatternLength,
  int          *matchPos,
  int           matchedPatternLength)
{
    switch (matchingType)
    {
        case MT_FLASH_DELIMIT:
        case MT_MACRO:
            if (direction == SEARCH_FORWARD)
            {
                (*matchPos) -= matchedPatternLength - 1;
            }
            break;

        case MT_FLASH_RANGE:
        case MT_SELECT:
            if (direction == SEARCH_FORWARD)
            {
                (*charPos) -= startPatternLength;
            }
            else
            {
                (*charPos) --;
            }
            break;

        case MT_GOTO:
            if (direction == SEARCH_FORWARD)
            {
                (*matchPos) ++;
            }
            else
            {
                (*matchPos) += matchedPatternLength;
            }
            break;
    }
}

/*
** Try to find a string pattern at given buffer position 'charPos'.
** A string pattern is found, if pattern is located before 'charPos' or
** 'charPos' is located within a pattern.
** If a string pattern is found, then search direction and string pattern
** reference / properties are determined.
** Returns true, if a string pattern was found.
*/
static int findMatchingStringElement(
  StringMatchTable         *smTable,
  SearchRegionInfo         *searchRegion,
  MatchingElementInfo      *matchInfo,
  int                      *charPos,
  const char               *delimiters)
{
    if (getPatternLocatedAtPos(
          smTable->smtUsedPatRE,
          smTable->smtAllPatterns,
          searchRegion,
          charPos,
          matchInfo,
          delimiters))
    {
        /*
         * Pattern found -> define search direction:
         * - START & MIDDLE pattern: matching pattern is searched in
         *   forward direction
         * - END pattern: matching pattern is searched in backward
         *   direction
         */
        if (matchInfo->meiPatRef.prKind == PEK_END)
            matchInfo->meiDirection = SEARCH_BACKWARD;
        else
            matchInfo->meiDirection = SEARCH_FORWARD;

        return TRUE;
    }

    return FALSE;
}

/*
** Return mono pattern info depending on highlight codes
** of left / right side of string pattern. Update pattern
** kind if applicable.
*/
static PatternElementMonoInfo determineMonoPatInfo(
  WindowInfo         *window,
  int                 patHighLightCode,
  int                 leftPos,
  int                 rightPos,
  PatternElementKind *patElementKind)
{
    int leftSideHasSameHC;
    int rightSideHasSameHC;

    /*
     * Determine, if left side holds same highlight code than
     * found string pattern
     */
    if (leftPos >= 0)
        leftSideHasSameHC =
          (HighlightCodeOfPos(window, leftPos) == patHighLightCode);
    else
        leftSideHasSameHC = FALSE;

    /*
     * Determine, if right side holds same highlight code than
     * found string pattern
     */
    if (rightPos < window->buffer->length)
        rightSideHasSameHC =
          (HighlightCodeOfPos(window, rightPos) == patHighLightCode);
    else
        rightSideHasSameHC = FALSE;

    if ((rightSideHasSameHC && leftSideHasSameHC) ||
        (!rightSideHasSameHC && !leftSideHasSameHC))
    {
        return PEMI_MONO_AMBIGUOUS_SYNTAX;
    }
    else if (leftSideHasSameHC)
    {
        *patElementKind = PEK_END;
        return PEMI_MONO_DEFINITE_SYNTAX;
    }
    else
    {
        *patElementKind = PEK_START;
        return PEMI_MONO_DEFINITE_SYNTAX;
    }
}

/*
** Get backref info out of found string pattern and
** put it into given backRefInfo.
*/
static void assignBackRefInfo(
  StringPattern *strPat,
  BackRefInfo   *backRefInfo)
{
    int i;
    int localId;
    regexp *patRE = strPat->spTextRE;

    backRefInfo->briAvailable       = FALSE;
    backRefInfo->briCaseInsensitive = strPat->spCaseInsensitive;

    for (i=0; i<MAX_GLOBAL_BACK_REF_ID; i++)
    {
        localId = strPat->spGlobalToLocalBackRef[i];

        if (localId != NO_LOCAL_BACK_REF_ID)
        {
            backRefInfo->briAvailable = TRUE;

            backRefInfo->briStartPtr[i] = patRE->startp[localId];
            backRefInfo->briEndPtr[i]   = patRE->endp[localId];
        }
        else
        {
            backRefInfo->briStartPtr[i] = NULL;
            backRefInfo->briEndPtr[i]   = NULL;
        }
    }
}

/*
** Check, if given backref infos match.
** Returns true, if backref infos match.
*/
static int doesBackRefInfoMatch(
  BackRefInfo *backRefInfo1,
  BackRefInfo *backRefInfo2)
{
    int i;

    /*
     * if min. one string pattern doesn't hold backref info, then nothing
     * could be compared -> both string pattern are matching.
     */
    if (!backRefInfo1->briAvailable || !backRefInfo2->briAvailable)
        return TRUE;

    for (i=0; i < MAX_GLOBAL_BACK_REF_ID; i++)
    {
        if (!compareBackRef(
               backRefInfo1->briStartPtr[i],
               backRefInfo1->briEndPtr[i],
               backRefInfo2->briStartPtr[i],
               backRefInfo2->briEndPtr[i],
               backRefInfo1->briCaseInsensitive || backRefInfo2->briCaseInsensitive))
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*
** Compares two backref content.
** Returns true, if backref contents match.
*/
static int compareBackRef(
  char *startPtr1,
  char *endPtr1,
  char *startPtr2,
  char *endPtr2,
  int   caseInsensitive)
{
    char *s1;
    char *s2;

    if (startPtr1 == NULL && startPtr2 == NULL)
        return TRUE;

    if (startPtr1 == NULL || startPtr2 == NULL)
        return FALSE;

    if ((endPtr1 - startPtr1) != (endPtr2 - startPtr2))
        return FALSE;

    s1 = startPtr1;
    s2 = startPtr2;

    if (caseInsensitive)
    {
        while (s1 != endPtr1)
        {
            if (tolower((unsigned char)*s1++) != tolower((unsigned char)*s2++))
                return FALSE;
        }
    }
    else
    {
        while (s1 != endPtr1)
        {
            if (*s1++ != *s2++)
                return FALSE;
        }
    }
    return TRUE;
}

/*
** Verify if given pattern element is located between given
** start / end pointer of "foundStringInfo". Assign backreference
** information, if pattern element matches.
** Returns true, if given pattern element matches.
*/
static int doesPatternElementMatch(
  PatternElement  *patElement,
  FoundStringInfo *foundStringInfo,
  BackRefInfo     *backRefInfo)
{
    char *s;
    char *p;
    StringPattern *strPat;
    int elementMatch;

    switch (patElement->peType)
    {
      case PET_SINGLE:
        strPat = &patElement->peVal.peuSingle;
        break;
      case PET_MULTIPLE:
        strPat = &patElement->peVal.peuMulti.mpStringPattern;
        break;
      default:
        return FALSE;
    }

    if (strPat->spRegularExpression)
    {
        /*
         * check reg. expression:
         */
        elementMatch =
          ExecRE(
            strPat->spTextRE,
            foundStringInfo->fsiStartPtr,
            foundStringInfo->fsiEndPtr,
            FALSE,
            foundStringInfo->fsiPrevChar,
            foundStringInfo->fsiSuccChar,
            foundStringInfo->fsiDelimiters,
            NULL,
            NULL);

        if (elementMatch)
            assignBackRefInfo(strPat, backRefInfo);

        return elementMatch;
    }
    else
    {
        backRefInfo->briAvailable = FALSE;

        /*
         * check literal string:
         */
        p = strPat->spText;

        /*
         * if length of found string is different from length of
         * given string pattern, then there is no match.
         */
        if (strPat->spLength != foundStringInfo->fsiLength)
            return FALSE;

        s = foundStringInfo->fsiStartPtr;

        if (strPat->spCaseInsensitive)
        {
            while (s != foundStringInfo->fsiEndPtr)
            {
                if (tolower((unsigned char)*s++) != *p++)
                  return FALSE;
            }
        }
        else
        {
            while (s != foundStringInfo->fsiEndPtr)
            {
                if (*s++ != *p++)
                  return FALSE;
            }
        }
    }

    return TRUE;
}

/*
** Verify if a pattern element of given MatchPatternTableElement is
** located between given start / end pointer of "foundStringInfo".
** Returns true, if so.
*/
static int doesMPTableElementMatch(
  MatchPatternTableElement *element,
  FoundStringInfo          *foundStringInfo,
  PatternElementKind       *patternElementKind,
  int                      *patternElementIdx,
  BackRefInfo              *backRefInfo)
{
    int i;

    for (i=0; i<element->mpteAll.pesNumberOfPattern; i ++)
    {
        if (doesPatternElementMatch(
              element->mpteAll.pesPattern[i],
              foundStringInfo,
              backRefInfo))
        {
            *patternElementKind = element->mpteAll.pesPattern[i]->peKind;
            *patternElementIdx  = i;

            return TRUE;
        }
    }

    return FALSE;
}

/*
** Determine the pattern element of given MatchPatternTable, which is
** located between given start / end pointer of "foundStringInfo".
** Setup related pattern element reference.
** Returns true, if a pattern element was found.
*/
static int getPatternInfo(
  MatchPatternTable *table,
  FoundStringInfo   *foundStringInfo,
  PatternReference  *patRef,
  BackRefInfo       *backRefInfo)
{
    int i;

    for (i=0; i<table->mptNumberOfElements; i++)
    {
        if (doesMPTableElementMatch(
              table->mptElements[i],
              foundStringInfo,
              &patRef->prKind,
              &patRef->prPatternIdx,
              backRefInfo))
        {
            patRef->prElementIdx = i;

            return TRUE;
        }
    }

    /*
     * Should never been reached !
     */
    patRef->prElementIdx = NO_ELEMENT_IDX;
    patRef->prKind       = PEK_UNKNOWN;
    patRef->prPatternIdx = NO_PATTERN_IDX;

    return FALSE;
}

/*
** Check, if given child pattern element is part of given
** parent pattern element.
** Returns true, if child is part of parent.
*/
static int isPartOfPattern(
  MatchPatternTable *table,
  int                parentElementIdx,
  int                childElementIdx,
  PatternElementKind patElementKind)
{
    MatchPatternTableElement *parent = table->mptElements[parentElementIdx];
    MatchPatternTableElement *child  = table->mptElements[childElementIdx];

    if (childElementIdx == parentElementIdx)
        return TRUE;

    if (patElementKind == PEK_START)
    {
        if (isPartOfPatternElementSet(&parent->mpteStart, childElementIdx))
            return TRUE;

        return( isPartOfPatternElementSet(&child->mpteStart, parentElementIdx) );
    }
    else if (patElementKind == PEK_END)
    {
        if (isPartOfPatternElementSet(&parent->mpteEnd, childElementIdx))
            return TRUE;

        return( isPartOfPatternElementSet(&child->mpteEnd, parentElementIdx) );
    }
    else
    {
        /*
         * Given child pattern element is middle pattern: the given pattern element
         * is part of parent pattern, if it's a reference of a middle pattern
         */
        if (isPartOfMiddlePatternElementSet(&parent->mpteMiddle, childElementIdx))
            return TRUE;

        return( isPartOfMiddlePatternElementSet(&child->mpteMiddle, parentElementIdx) );
    }
}

/*
** Check, if given pattern element is part of given pattern element set.
** Returns true, if so.
*/
static int isPartOfPatternElementSet(
  PatternElementSet *patElementSet,
  int                patternElementIdx)
{
    PatternElement *patElement;
    int i;

    for (i=0; i<patElementSet->pesNumberOfPattern; i++)
    {
        if (patElementSet->pesPattern[i]->peType == PET_REFERENCE)
        {
            patElement = patElementSet->pesPattern[i];

            if (patElement->peVal.peuRef.prElementIdx == patternElementIdx)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

/*
** Verify, if given pattern element is part of middle pattern set.
** That's the case, if an element of the pattern set is reference
** of a middle pattern, which fits to given pattern element.
** Returns true, if pattern element is part of middle pattern set.
 */
static int isPartOfMiddlePatternElementSet(
  PatternElementSet *patElementSet,
  int                patternElementIdx)
{
    PatternElement *patElement;
    int i;

    for (i=0; i<patElementSet->pesNumberOfPattern; i++)
    {
        if (patElementSet->pesPattern[i]->peType == PET_REFERENCE)
        {
            patElement = patElementSet->pesPattern[i];

            if (patElement->peVal.peuRef.prElementIdx == patternElementIdx &&
                patElement->peKind == PEK_MIDDLE)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

/*
** Update start pattern reference depending on next pattern located
** after start pattern.
*/
static void considerNextPatternReference(
  MatchPatternTable *table,
  PatternReference  *startPatRef,
  PatternReference   nxtPatRef,
  int                groupIdx)
{
    MatchPatternTableElement *tabElement;
    PatternElement           *startPat;
    MultiPattern             *multiStartPat;
    PatternReference          patRef;
    int i;

    /*
     * startPatRef needs no adaption, if element index of start pattern
     * and next pattern are equal (i.e. start and next pattern belong
     * to same pattern element).
     */
    if (startPatRef->prElementIdx == nxtPatRef.prElementIdx)
        return;

    /*
     * Verify, if start pattern belongs to multiple pattern elements
     * (like "ELSE .. FI" & "ELSE .. ESAC").
     */
    startPat = getPatternOfReference( table, *startPatRef );

    if (startPat->peType == PET_MULTIPLE)
    {
        /*
         * Check, if next pattern fits to one of the references of
         * the start multi pattern. If so: adapt start pattern reference.
         */
        multiStartPat = &startPat->peVal.peuMulti;

        for (i=0; i<multiStartPat->mpNumberOfReferences; i ++)
        {
            patRef     = multiStartPat->mpRefList[i];
            tabElement = table->mptElements[patRef.prElementIdx];

            if (nxtPatRef.prElementIdx == patRef.prElementIdx &&
                tabElement->mpteGroup == groupIdx)
            {
                *startPatRef = patRef;
                return;
            }
        }
    }
}

/*
** Search for a string pattern in forward direction, starting at
** given beginPos. Determine related pattern reference of a found
** string pattern.
** Returns true, if a next string pattern was found.
*/
static int searchPatternForward(
  MatchPatternTable *table,
  regexp            *compiledRE,
  SearchRegionInfo  *searchRegion,
  const char        *delimiters,
  int                beginPos,
  int               *matchEndPos,
  PatternReference  *patRef,
  int               *highLightCode,
  BackRefInfo       *backRefInfo)
{
    FoundStringInfo foundStringInfo;
    int matchStartPos;
    int absMatchStartPos;
    MatchPatternTableElement *matchedElement;

    *matchEndPos = -1;

    patRef->prElementIdx = NO_ELEMENT_IDX;
    patRef->prKind       = PEK_UNKNOWN;
    patRef->prPatternIdx = NO_PATTERN_IDX;

    *highLightCode = IGNORE_HIGHLIGHT_CODE;

    if (ExecRE(
          compiledRE,
          searchRegion->sriText + beginPos,
          NULL,
          FALSE,
          beginPos==0 ? searchRegion->sriPrevChar : searchRegion->sriText[beginPos-1],
          searchRegion->sriSuccChar,
          delimiters,
          searchRegion->sriText,
          NULL))
    {
        foundStringInfo.fsiStartPtr   = compiledRE->startp[0];
        foundStringInfo.fsiEndPtr     = compiledRE->endp[0];
        foundStringInfo.fsiLength     =
          foundStringInfo.fsiEndPtr - foundStringInfo.fsiStartPtr;

        foundStringInfo.fsiPrevChar   =
          foundStringInfo.fsiStartPtr == searchRegion->sriText ?
            searchRegion->sriPrevChar : *(foundStringInfo.fsiStartPtr - 1);

        foundStringInfo.fsiSuccChar   =
          *(foundStringInfo.fsiEndPtr) == '\0' ?
            searchRegion->sriSuccChar : *(foundStringInfo.fsiEndPtr);

        foundStringInfo.fsiDelimiters = delimiters;

        if (getPatternInfo(
              table,
              &foundStringInfo,
              patRef,
              backRefInfo))
        {
            /*
             * Next string pattern was found in forward direction and
             * a pattern reference could be assigned to: calculate
             * relative & absolute match positions.
             */
            matchStartPos = foundStringInfo.fsiStartPtr - searchRegion->sriText;
            *matchEndPos  = foundStringInfo.fsiEndPtr   - searchRegion->sriText;

            absMatchStartPos = matchStartPos + searchRegion->sriStartOfTextPos;

            matchedElement = table->mptElements[patRef->prElementIdx];

            /*
             * get highlight code of found string pattern, if applicable
             */
            if (!matchedElement->mpteIgnoreHighLightInfo)
            {
                *highLightCode =
                  HighlightCodeOfPos(searchRegion->sriWindow, absMatchStartPos);
            }

            /*
             * setup mono pattern info of found string pattern
             */
            if (matchedElement->mpteIsMonoPattern)
            {
                if (matchedElement->mpteIgnoreHighLightInfo)
                {
                    patRef->prMonoInfo = PEMI_MONO_NOT_SYNTAX_BASED;
                }
                else
                {
                    /*
                     * determine mono pattern info depending on highLightCodes
                     * before / after found string pattern.
                     */
                    patRef->prMonoInfo =
                      determineMonoPatInfo(
                        searchRegion->sriWindow,
                        *highLightCode,
                        absMatchStartPos - 1,
                        *matchEndPos + searchRegion->sriStartOfTextPos,
                        &patRef->prKind);
                }
            }
            else
            {
                patRef->prMonoInfo = PEMI_NOT_MONO;
            }

            return matchStartPos;
        }
        else
        {
           /*
            *  Found string can't be assigned to a pattern element
            *  (should never occur).
            */
            return -1;
        }
    }
    else
    {
        /*
         * No next string pattern is found in forward direction.
         */
        return -1;
    }
}

/*
** Search for a string pattern in backward direction, starting at
** given beginPos. Determine related pattern reference of a found
** string pattern.
** Returns true, if a next string pattern was found.
*/
static int searchPatternBackward(
  MatchPatternTable *table,
  regexp            *compiledRE,
  SearchRegionInfo  *searchRegion,
  const char        *delimiters,
  int                beginPos,
  PatternReference  *patRef,
  int               *highLightCode,
  int               *matchedPatternLength,
  BackRefInfo       *backRefInfo)
{
    FoundStringInfo foundStringInfo;
    int matchStartPos;
    int absMatchStartPos;
    MatchPatternTableElement *matchedElement;

    patRef->prElementIdx = NO_ELEMENT_IDX;
    patRef->prKind       = PEK_UNKNOWN;
    patRef->prPatternIdx = NO_PATTERN_IDX;

    *highLightCode = IGNORE_HIGHLIGHT_CODE;

    if (ExecRE(
          compiledRE,
          searchRegion->sriText,
          searchRegion->sriText + beginPos,
          TRUE,
          searchRegion->sriPrevChar,
          searchRegion->sriText[beginPos] == '\0' ?
            searchRegion->sriSuccChar : searchRegion->sriText[beginPos + 1],
          delimiters,
          searchRegion->sriText,
          NULL))
    {
        foundStringInfo.fsiStartPtr   = compiledRE->startp[0];
        foundStringInfo.fsiEndPtr     = compiledRE->endp[0];
        foundStringInfo.fsiLength     =
          foundStringInfo.fsiEndPtr - foundStringInfo.fsiStartPtr;

        foundStringInfo.fsiPrevChar   =
          foundStringInfo.fsiStartPtr == searchRegion->sriText ?
            searchRegion->sriPrevChar : *(foundStringInfo.fsiStartPtr - 1);

        foundStringInfo.fsiSuccChar   =
          *(foundStringInfo.fsiEndPtr) == '\0' ?
            searchRegion->sriSuccChar : *(foundStringInfo.fsiEndPtr);

        foundStringInfo.fsiDelimiters = delimiters;

        if (getPatternInfo(
              table,
              &foundStringInfo,
              patRef,
              backRefInfo))
        {
            /*
             * Next string pattern was found in backward direction and
             * a pattern reference could be assigned to: calculate
             * relative & absolute match positions.
             */
            matchStartPos = foundStringInfo.fsiStartPtr - searchRegion->sriText;
            *matchedPatternLength = foundStringInfo.fsiLength;

            absMatchStartPos = matchStartPos + searchRegion->sriStartOfTextPos;

            matchedElement = table->mptElements[patRef->prElementIdx];

            /*
             * get highlight code of found string pattern, if applicable
             */
            if (!matchedElement->mpteIgnoreHighLightInfo)
            {
                *highLightCode =
                  HighlightCodeOfPos(searchRegion->sriWindow, absMatchStartPos);
            }

            /*
             * setup mono pattern info of found string pattern
             */
            if (matchedElement->mpteIsMonoPattern)
            {
                if (matchedElement->mpteIgnoreHighLightInfo)
                {
                    patRef->prMonoInfo = PEMI_MONO_NOT_SYNTAX_BASED;
                }
                else
                {
                    /*
                     * determine mono pattern info depending on highLightCodes
                     * before / after found string pattern.
                     */
                    patRef->prMonoInfo =
                      determineMonoPatInfo(
                        searchRegion->sriWindow,
                        *highLightCode,
                        absMatchStartPos - 1,
                        absMatchStartPos + *matchedPatternLength,
                        &patRef->prKind);
                }
            }
            else
            {
                patRef->prMonoInfo = PEMI_NOT_MONO;
            }

            return matchStartPos;
        }
        else
        {
           /*
            *  Found string can't be assigned to a pattern element
            *  (should never occur).
            */
            return -1;
        }
    }
    else
    {
        /*
         * No next string pattern is found in backward direction.
         */
        return -1;
    }
}

/*
** Find matching pattern related to given pattern (stored in
** 'matchInfo') in forward direction by considering the rules stored in
** string match table of given window. Determine match position (= abs.
** pos. of last character of matching string) and length of matching
** string. If a mono matching pattern couldn't be found in forward
** direction, then try finding it in backward direction (if found in
** backward direction, then match position indicates the 1st char. of
** matching string).
** Returns true, if a matching pattern was found.
*/
static int parseStringElementForward(
  MatchingElementInfo *matchInfo,
  SearchRegionInfo    *searchRegion,
  int                  relCharPos,
  int                 *matchPos,
  int                 *matchedPatternLength,
  const char          *delimiters)
{
    StringMatchTable *smTable =
      (StringMatchTable *)searchRegion->sriWindow->stringMatchTable;
    MatchPatternTableElement *matchElement = matchInfo->meiElement;
    int beginPos = relCharPos;
    int endStartPos;
    int startStartPos;
    int endEndPos;
    int matchingPatternFound = FALSE;

    /*
     * Find matching pattern within text buffer area to parse in
     * forward direction.
     */
    endStartPos =
      findRelatedForwardPattern(
        smTable,
        searchRegion,
        delimiters,
        matchInfo->meiPatRef,
        matchInfo->meiHighLightCode,
        &matchInfo->meiBackRefInfo,
        beginPos,
        &endEndPos);

    if (endEndPos != -1)
    {
        *matchPos             = endEndPos - 1 + searchRegion->sriStartOfTextPos;
        *matchedPatternLength = endEndPos - endStartPos;

        matchingPatternFound = TRUE;
    }
    else if (matchElement->mpteIsMonoPattern)
    {
        /*
         * mono pattern: forward find fails -> try backward direction.
         * Calc. relative position of 1st char. before found string pattern.
         */
        beginPos = beginPos - matchInfo->meiLength - 1;

        if (beginPos > 0)
        {
            startStartPos =
              findRelatedStartPattern(
                smTable,
                searchRegion,
                delimiters,
                matchInfo->meiPatRef,
                matchInfo->meiHighLightCode,
                &matchInfo->meiBackRefInfo,
                beginPos,
                matchedPatternLength);

            if (startStartPos != -1)
            {
                matchInfo->meiDirection = SEARCH_BACKWARD;

                *matchPos = startStartPos + searchRegion->sriStartOfTextPos;

                matchingPatternFound = TRUE;
            }
        }
    }

    return matchingPatternFound;
}

/*
** Find matching pattern related to given begin pattern reference
** in forward direction by considering the rules stored in given
** string match table. Determine match position (= relative
** pos. of last character of matching string).
** Returns -1, if no matching pattern was found. Else the relative
** position of 1st char. of matching string is returned.
*/
static int findRelatedForwardPattern(
  StringMatchTable *table,
  SearchRegionInfo *searchRegion,
  const char       *delimiters,
  PatternReference  beginPatRef,
  int               beginPatHighLightCode,
  BackRefInfo      *beginPatBackRefInfo,
  int               beginPos,
  int              *matchEndPos)
{
    MatchPatternTable *patTable = table->smtAllPatterns;
    int startPos = beginPos;
    int nxtPatStartPos = 0;
    int nxtPatEndPos = 0;
    PatternReference nxtPatRef;
    int nxtPatHighLightCode;
    BackRefInfo nxtPatBackRefInfo;
    PatternStackElement stack[MAX_NESTED_PATTERNS];
    int stackIdx = 0;
    PatternReference startPatRef;
    MatchPatternTableElement *currentElement;
    int groupIdx;
    regexp *groupPatRE;
    regexp *currentPatRE;
    int skipToEnd = FALSE;
    int beginPatternIsMono;

#ifdef DEBUG_FIND
    printf("Forward Start Pos: %d K: %s EI: %d PI: %d HC: %d <%s>\n",
      startPos,
      patElemKindToString(beginPatRef.prKind),
      beginPatRef.prElementIdx,
      beginPatRef.prPatternIdx,
      beginPatHighLightCode,
      getPatternForDebug(patTable, beginPatRef) );
#endif

    /*
     * put begin pattern info on stack
     */
    stack[stackIdx].psePatRef        = beginPatRef;
    stack[stackIdx].pseHighLightCode = beginPatHighLightCode;
    stack[stackIdx].pseBackRefInfo   = *beginPatBackRefInfo;
    stackIdx ++;

    currentElement = patTable->mptElements[beginPatRef.prElementIdx];

    beginPatternIsMono = currentElement->mpteIsMonoPattern;

    groupIdx = currentElement->mpteGroup;

    /*
     * no next pattern can be found, if there is no group assigned
     * to begin pattern (should never occur)
     */
    if (groupIdx == NO_GROUP_IDX)
    {
        *matchEndPos = -1;

        return -1;
    }

    /*
     * Remember pattern (= keywords) regular expression of context
     * group related to begin pattern. Use it for forward search.
     */
    groupPatRE   = table->smtGroups[groupIdx]->mpgeKeywordRE;
    currentPatRE = groupPatRE;

    /*
     * Use start / end pattern regular expression if skip to end is
     * set for begin pattern.
     */
    if (currentElement->mpteSkipBtwnStartEnd)
    {
        currentPatRE = currentElement->mpteStartEndRE;
        skipToEnd = TRUE;
    }

    while (stackIdx > 0 && nxtPatStartPos != -1)
    {
        /*
         * Search for next string pattern in forward direction.
         */
        nxtPatStartPos =
          searchPatternForward(
            patTable,
            currentPatRE,
            searchRegion,
            delimiters,
            startPos,
            &nxtPatEndPos,
            &nxtPatRef,
            &nxtPatHighLightCode,
            &nxtPatBackRefInfo );

        startPatRef = stack[stackIdx-1].psePatRef;

        if (nxtPatStartPos == -1)
        {
            /*
             * no next pattern found -> leave loop
             */
#ifdef DEBUG_FIND
            printf("  SI: %d [start K: %s EI: %d PI %d] Pos: %d --> no next pat. found\n",
              stackIdx,
              patElemKindToString(startPatRef.prKind),
              startPatRef.prElementIdx,
              startPatRef.prPatternIdx,
              startPos);
#endif
            break;
        }

        /*
         * Update start pattern reference depending on next pattern
         * located after start pattern.
         */
        considerNextPatternReference(
          patTable,
          &startPatRef,
          nxtPatRef,
          groupIdx );

        /*
         * If current found match pattern table element is a mono pattern and
         * skip to start pattern is active, then the found pattern string is
         * a END one in case of ambiguous or no syntax was detected.
         */
        if (skipToEnd &&
            (nxtPatRef.prMonoInfo == PEMI_MONO_AMBIGUOUS_SYNTAX ||
             nxtPatRef.prMonoInfo == PEMI_MONO_NOT_SYNTAX_BASED))
        {
            nxtPatRef.prKind = PEK_END;
        }

#ifdef DEBUG_FIND
        printf("  SI: %d [start K: %s EI: %d PI %d] Pos: %d-%d K: %s EI: %d PI: %d HC: %d f=<%s> ",
          stackIdx,
          patElemKindToString(startPatRef.prKind),
          startPatRef.prElementIdx,
          startPatRef.prPatternIdx,
          nxtPatStartPos,
          nxtPatEndPos,
          patElemKindToString(nxtPatRef.prKind),
          nxtPatRef.prElementIdx,
          nxtPatRef.prPatternIdx,
          nxtPatHighLightCode,
          getPatternForDebug(patTable, nxtPatRef) );
        printFoundStringForDebug(
          searchRegion->sriWindow,
          nxtPatStartPos + searchRegion->sriStartOfTextPos,
          nxtPatEndPos - nxtPatStartPos);
        printf("\n");
#endif

        if (nxtPatRef.prKind == PEK_START)
        {
            if (stackIdx >= MAX_NESTED_PATTERNS)
            {
#ifdef DEBUG_FIND
                printf("FORWARD: MAX. NESTED PATTERN DEPTH REACHED !\n");
#endif
                nxtPatStartPos = -1;
                nxtPatEndPos   = -1;
            }
            else if (!skipToEnd)
            {
                /*
                 * Put next pattern on stack, if contents between start /
                 * end shouldn't be skipped (if "skipToEnd" is set,
                 * a (usually illegal) start pattern to skip inside the
                 * skipped one is found (e.g. \* \* ..)
                 */
                stack[stackIdx].psePatRef        = nxtPatRef;
                stack[stackIdx].pseHighLightCode = nxtPatHighLightCode;
                stack[stackIdx].pseBackRefInfo   = nxtPatBackRefInfo;
                stackIdx ++;

                currentElement = patTable->mptElements[nxtPatRef.prElementIdx];

                /*
                 * Use start / end pattern regular expression if skip to
                 * end is set for found start pattern.
                 */
                if (currentElement->mpteSkipBtwnStartEnd)
                {
                    currentPatRE = currentElement->mpteStartEndRE;
                    skipToEnd = TRUE;
                }
            }
            else if (beginPatternIsMono)
            {
                /*
                 * skip to end is set and a mono pattern start is reached:
                 * trigger backward search by returning "not found"
                 */
#ifdef DEBUG_FIND
                printf("  ---> mono pattern (re-)start -> trigger backward search\n");
#endif
                nxtPatStartPos = -1;
                nxtPatEndPos   = -1;
            }
#ifdef DEBUG_FIND
            else
            {
                printf("  ---> skip to end: illegal (re-)start pattern !\n");
            }
#endif
        }
        else if (nxtPatRef.prKind == PEK_END)
        {
            /*
             * ignore current found pattern, if it doesn't fit to the prev.
             * opened one.
             */
            if (isPartOfPattern(
                  patTable,
                  nxtPatRef.prElementIdx,
                  startPatRef.prElementIdx,
                  PEK_END) &&
                (stack[stackIdx - 1].pseHighLightCode == nxtPatHighLightCode ||
                 stack[stackIdx - 1].pseHighLightCode == IGNORE_HIGHLIGHT_CODE ||
                 nxtPatHighLightCode == IGNORE_HIGHLIGHT_CODE) &&
                doesBackRefInfoMatch(
                  &stack[stackIdx - 1].pseBackRefInfo,
                  &nxtPatBackRefInfo))
            {
                /*
                 * use context group pattern again, if end pattern to skip
                 * to was found.
                 */
                if (skipToEnd)
                {
                    currentPatRE = groupPatRE;
                    skipToEnd    = FALSE;
                }

                /*
                 * pop. related start pattern from stack.
                 */
                stackIdx --;
            }
        }
        else if (!skipToEnd)
        {
            /*
             * middle pattern was found: ignore it, if found middle pattern
             * doesn't belong to begin pattern.
             */
            if (stackIdx == 1 &&
                isPartOfPattern(
                  patTable,
                  startPatRef.prElementIdx,
                  nxtPatRef.prElementIdx,
                  nxtPatRef.prKind) &&
                (beginPatHighLightCode == nxtPatHighLightCode ||
                 beginPatHighLightCode == IGNORE_HIGHLIGHT_CODE ||
                 nxtPatHighLightCode == IGNORE_HIGHLIGHT_CODE) &&
                doesBackRefInfoMatch(
                  beginPatBackRefInfo,
                  &nxtPatBackRefInfo))

            {
                stackIdx --;
            }
        }

        startPos = nxtPatEndPos;
    }

    *matchEndPos = nxtPatEndPos;

    return nxtPatStartPos;
}

/*
** Find matching pattern related to given pattern (stored in
** 'matchInfo') in backward direction by considering the rules stored in
** string match table of given window. Determine match position (= abs.
** pos. of 1st character of matching string) and length of matching
** string.
** Returns true, if a matching pattern was found.
*/
static int parseStringElementBackward(
  MatchingElementInfo *matchInfo,
  SearchRegionInfo    *searchRegion,
  int                  relCharPos,
  int                 *matchPos,
  int                 *matchedPatternLength,
  const char          *delimiters)
{
    StringMatchTable *smTable =
      (StringMatchTable *)searchRegion->sriWindow->stringMatchTable;
    int beginPos;
    int startStartPos;
    int matchingPatternFound = FALSE;

    /*
     * determine begin of search in string buffer (= relative position
     * of 1st char. before found string pattern.)
     */
    beginPos = relCharPos - matchInfo->meiLength - 1;

    if (beginPos < 0)
      return FALSE;

    /*
     * Find matching pattern within text buffer area to parse in
     * backward direction.
     */
    startStartPos =
      findRelatedStartPattern(
        smTable,
        searchRegion,
        delimiters,
        matchInfo->meiPatRef,
        matchInfo->meiHighLightCode,
        &matchInfo->meiBackRefInfo,
        beginPos,
        matchedPatternLength);

    if (startStartPos != -1)
    {
        *matchPos = startStartPos + searchRegion->sriStartOfTextPos;
        matchingPatternFound = TRUE;
    }

    return matchingPatternFound;
}

/*
** Find matching pattern related to given begin pattern reference
** in backward direction by considering the rules stored in given
** string match table. Determine match position (= relative
** pos. of 1st character of matching string).
** Returns -1, if no matching pattern was found. Else the relative
** position of 1st char. of matching string is returned.
*/
static int findRelatedStartPattern(
  StringMatchTable *table,
  SearchRegionInfo *searchRegion,
  const char       *delimiters,
  PatternReference  beginPatRef,
  int               beginPatHighLightCode,
  BackRefInfo      *beginPatBackRefInfo,
  int               beginPos,
  int              *matchedPatternLength)
{
    MatchPatternTable *patTable = table->smtAllPatterns;
    int startPos = beginPos;
    int prevStartPos = 0;
    PatternReference prevPatRef;
    int prevPatHighLightCode;
    BackRefInfo prevPatBackRefInfo;
    PatternStackElement stack[MAX_NESTED_PATTERNS];
    int stackIdx = 0;
    MatchPatternTableElement *currentElement;
    int groupIdx;
    regexp *groupPatRE;
    regexp *currentPatRE;
    int skipToStart = FALSE;

    /*
     * put begin pattern info on stack
     */
    stack[stackIdx].psePatRef        = beginPatRef;
    stack[stackIdx].pseHighLightCode = beginPatHighLightCode;
    stack[stackIdx].pseBackRefInfo   = *beginPatBackRefInfo;
    stackIdx ++;

    currentElement = patTable->mptElements[beginPatRef.prElementIdx];

#ifdef DEBUG_FIND
    printf("Backward Start Pos: %d K: %s EI: %d PI: %d HC: %d <%s>\n",
      startPos,
      patElemKindToString(beginPatRef.prKind),
      beginPatRef.prElementIdx,
      beginPatRef.prPatternIdx,
      beginPatHighLightCode,
      getPatternForDebug(patTable, beginPatRef) );
#endif

    groupIdx = currentElement->mpteGroup;

    /*
     * no start pattern can be found, if there is no group assigned
     * to begin pattern (should never occur)
     */
    if (groupIdx == NO_GROUP_IDX)
    {
        return -1;
    }

    /*
     * Remember pattern (= keywords) regular expression of context
     * group related to begin pattern. Use it for backward search.
     */
    groupPatRE   = table->smtGroups[groupIdx]->mpgeKeywordRE;
    currentPatRE = groupPatRE;

    /*
     * Use start / end pattern regular expression if skip to start is
     * set for begin pattern.
     */
    if (currentElement->mpteSkipBtwnStartEnd)
    {
        currentPatRE = currentElement->mpteStartEndRE;
        skipToStart  = TRUE;
    }

    while (stackIdx > 0 && prevStartPos != -1)
    {
        /*
         * Search for previous string pattern in backward direction.
         */
        prevStartPos =
          searchPatternBackward(
            patTable,
            currentPatRE,
            searchRegion,
            delimiters,
            startPos,
            &prevPatRef,
            &prevPatHighLightCode,
            matchedPatternLength,
            &prevPatBackRefInfo );

        if (prevStartPos == -1)
        {
            /*
             * no previous pattern found -> leave loop
             */
#ifdef DEBUG_FIND
            printf("  SI: %d [K: %s start EI: %d PI %d] Pos: %d --> no next pat. found\n",
              stackIdx,
              patElemKindToString(stack[stackIdx -1].psePatRef.prKind),
              stack[stackIdx -1].psePatRef.prElementIdx,
              stack[stackIdx -1].psePatRef.prPatternIdx,
              startPos);
#endif
            break;
        }

        /*
         * Update previous pattern reference depending on last stack
         * pattern, which is located in text puffer after previous
         * start pattern.
         */
        considerNextPatternReference(
          patTable,
          &prevPatRef,
          stack[stackIdx - 1].psePatRef,
          groupIdx);

        /*
         * If current found match pattern table element is a mono pattern and
         * skip to start pattern is active, then the found pattern string is
         * a START one in case of ambiguous or no syntax was detected.
         */
        if (skipToStart &&
            (prevPatRef.prMonoInfo == PEMI_MONO_AMBIGUOUS_SYNTAX ||
             prevPatRef.prMonoInfo == PEMI_MONO_NOT_SYNTAX_BASED))
        {
            prevPatRef.prKind = PEK_START;
        }

#ifdef DEBUG_FIND
        printf("  SI: %d [K: %s start EI: %d PI %d] Pos: %d K: %s EI: %d PI: %d HC: %d f=<%s> ",
          stackIdx,
          patElemKindToString(stack[stackIdx -1].psePatRef.prKind),
          stack[stackIdx -1].psePatRef.prElementIdx,
          stack[stackIdx -1].psePatRef.prPatternIdx,
          prevStartPos,
          patElemKindToString(prevPatRef.prKind),
          prevPatRef.prElementIdx,
          prevPatRef.prPatternIdx,
          prevPatHighLightCode,
          getPatternForDebug(patTable, prevPatRef) );
        printFoundStringForDebug(
          searchRegion->sriWindow,
          prevStartPos + searchRegion->sriStartOfTextPos,
          *matchedPatternLength);
        printf("\n");
#endif

        if (prevPatRef.prKind == PEK_START)
        {
            /*
             * If the end pattern of the previous pattern set is a reference,
             * then the prev. element index is the one of the ref. (due to this
             * string was found before and was stored on stack)
             */
            if (patTable->mptElements[prevPatRef.prElementIdx]->mpteGroup == groupIdx)
            {
                considerStackPatReference(
                  &patTable->mptElements[prevPatRef.prElementIdx]->mpteEnd,
                  stack[stackIdx - 1].psePatRef.prElementIdx,
                  &prevPatRef.prElementIdx);
            }

            /*
             * Ignore current found pattern, if it doesn't fit to the prev.
             * opened one.
             */
            if (stack[stackIdx - 1].psePatRef.prElementIdx == prevPatRef.prElementIdx &&
                (stack[stackIdx - 1].pseHighLightCode == prevPatHighLightCode ||
                 stack[stackIdx - 1].pseHighLightCode == IGNORE_HIGHLIGHT_CODE ||
                 prevPatHighLightCode == IGNORE_HIGHLIGHT_CODE) &&
                doesBackRefInfoMatch(
                  &stack[stackIdx - 1].pseBackRefInfo,
                  &prevPatBackRefInfo))
            {
                /*
                 * use context group pattern again, if start pattern
                 * to skip to was found.
                 */
                if (skipToStart)
                {
                    currentPatRE = groupPatRE;
                    skipToStart  = FALSE;
                }

                /*
                 * pop. related end pattern from stack.
                 */
                stackIdx --;
            }
        }
        else if (prevPatRef.prKind == PEK_END)
        {
            if (stackIdx >= MAX_NESTED_PATTERNS)
            {
#ifdef DEBUG_FIND
                printf("BACKWARD: MAX. NESTED PATTERN DEPTH REACHED !\n");
#endif
                prevStartPos = -1;
            }
            else if (!skipToStart)
            {
                /*
                 * Put prev. pattern on stack, if contents between start /
                 * end shouldn't be skipped (if "skipToStart" is set,
                 * a (usually illegal) end pattern to skip inside the
                 * skipped one is found (e.g. *\ *\ ..)
                 */
                stack[stackIdx].psePatRef        = prevPatRef;
                stack[stackIdx].pseHighLightCode = prevPatHighLightCode;
                stack[stackIdx].pseBackRefInfo   = prevPatBackRefInfo;
                stackIdx ++;

                currentElement =
                  patTable->mptElements[prevPatRef.prElementIdx];

                /*
                 * Use start / end pattern regular expression if skip to
                 * end is set for found end pattern.
                 */
                if (currentElement->mpteSkipBtwnStartEnd)
                {
                    currentPatRE = currentElement->mpteStartEndRE;
                    skipToStart  = TRUE;
                }
            }
        }
        startPos = prevStartPos - 1;
    }

    return prevStartPos;
}

/*
** Adapt found pattern element index depending on
** the info stored on (last) stack element and
** a given pattern set (belonging to the found pattern).
*/
static void considerStackPatReference(
  PatternElementSet  *patSet,
  int                 stackElementIdx,
  int                *foundElementIdx)
{
    PatternElement *patElement;
    int i;

    /*
     * If found pattern index already indicates, that found pattern
     * belongs to pattern set stored on stack, then no adaption is needed
     */
    if (*foundElementIdx == stackElementIdx)
        return;

    /*
     * Check all elements of given pattern element set:
     */
    for (i=0; i < patSet->pesNumberOfPattern; i++)
    {
        patElement = patSet->pesPattern[i];

        /*
         * If this set element is a reference and this reference fits
         * to the element stored on stack, then adapt found element index:
         * indicate, that found pattern belongs to pattern set stored on stack
         */
        if (patElement->peType == PET_REFERENCE &&
            patElement->peVal.peuRef.prElementIdx == stackElementIdx)
        {
            *foundElementIdx = stackElementIdx;

            return;
        }
    }
}

/*
** Determines, if a string pattern is located at the given position
** "relBeginPos" in the given "searchRegion". A string pattern is
** found, if the pattern is located just before given position
** "relBeginPos" OR if "relBeginPos" is located within a string pattern.
**
** Returns true, if the given "pattern" is located at
** "relBeginPos". "matchInfo" holds all info needed about matched
** "start" string pattern.
*/
static int getPatternLocatedAtPos(
  regexp              *usedPatRE,
  MatchPatternTable   *table,
  SearchRegionInfo    *searchRegion,
  int                 *relBeginPos,
  MatchingElementInfo *matchInfo,
  const char          *delimiters)
{
    int searchStartPos = *relBeginPos;
    PatternReference *patRef = &matchInfo->meiPatRef;
    FoundStringInfo foundStringInfo;
    int relMatchStartPos;
    int relMatchEndPos;

    patRef->prElementIdx = NO_ELEMENT_IDX;
    patRef->prKind       = PEK_UNKNOWN;
    patRef->prPatternIdx = NO_PATTERN_IDX;

    matchInfo->meiHighLightCode = IGNORE_HIGHLIGHT_CODE;
    matchInfo->meiAbsStartPos   = -1;
    matchInfo->meiLength        = 0;

    /*
     * No backward search possible, if we are at beginning of
     * search region
     */
    if (searchStartPos == 0)
        return FALSE;

    /*
     * Search in backward direction for 1st occurance of a string pattern
     * starting one char before "searchStartPos".
     */
    if (ExecRE(
          usedPatRE,
          searchRegion->sriText,
          searchRegion->sriText + searchStartPos - 1,
          TRUE,
          searchRegion->sriPrevChar,
          searchRegion->sriText[searchStartPos],
          delimiters,
          searchRegion->sriText,
          NULL))
    {
        /*
         * String pattern was found:
         */
        foundStringInfo.fsiStartPtr = usedPatRE->startp[0];
        foundStringInfo.fsiEndPtr   = usedPatRE->endp[0];
        foundStringInfo.fsiLength   =
          foundStringInfo.fsiEndPtr - foundStringInfo.fsiStartPtr;

        relMatchEndPos   = foundStringInfo.fsiEndPtr   - searchRegion->sriText;
        relMatchStartPos = foundStringInfo.fsiStartPtr - searchRegion->sriText;

        /*
         * Is found pattern located exactly one char before "relBeginPos" OR
         * is "relBeginPos" located within found string pattern ?
         * Note: "relMatchEndPos" indicates 1st pos. in "sriText"
         * which does *not* belong to found string anymore.
         */
        if ((*relBeginPos == relMatchEndPos) ||
            (*relBeginPos >= relMatchStartPos &&
             *relBeginPos < relMatchEndPos))
        {
            *relBeginPos = relMatchEndPos;

            /*
             * Determine match element info related to found string.
             */
            matchInfo->meiAbsStartPos =
              foundStringInfo.fsiStartPtr - searchRegion->sriText +
                searchRegion->sriStartOfTextPos;
            matchInfo->meiLength      = foundStringInfo.fsiLength;

            foundStringInfo.fsiPrevChar   =
              foundStringInfo.fsiStartPtr == searchRegion->sriText ?
                searchRegion->sriPrevChar : *(foundStringInfo.fsiStartPtr - 1);

            foundStringInfo.fsiSuccChar   =
              *(foundStringInfo.fsiEndPtr) == '\0' ?
                searchRegion->sriSuccChar : *(foundStringInfo.fsiEndPtr);

            foundStringInfo.fsiDelimiters = delimiters;

            return(
              getMatchedElementInfo(
                searchRegion->sriWindow,
                table,
                &foundStringInfo,
                matchInfo));
        }
    }

    return FALSE;
}

/*
** Get all needed info related to matched "start" string pattern
** (given by parameter "foundStringInfo").
**
** Returns true, if info was determined successfully.
*/
static int getMatchedElementInfo(
  WindowInfo          *window,
  MatchPatternTable   *table,
  FoundStringInfo     *foundStringInfo,
  MatchingElementInfo *matchInfo)
{
    PatternReference *patRef = &matchInfo->meiPatRef;
    int absMatchStartPos = matchInfo->meiAbsStartPos;
    MatchPatternTableElement *matchedElement;

    if (getPatternInfo(
          table,
          foundStringInfo,
          patRef,
          &matchInfo->meiBackRefInfo))
    {
        /*
         * A pattern reference could be assigned to found string:
         */
        matchedElement = table->mptElements[patRef->prElementIdx];

        matchInfo->meiElement = matchedElement;

        /*
         * get highlight code of found string pattern, if applicable
         */
        if (!matchedElement->mpteIgnoreHighLightInfo)
        {
            matchInfo->meiHighLightCode =
              HighlightCodeOfPos(window, absMatchStartPos);
        }

        /*
         * setup mono pattern info of found string pattern
         */
        if (matchedElement->mpteIsMonoPattern)
        {
            if (matchedElement->mpteIgnoreHighLightInfo)
            {
                patRef->prMonoInfo = PEMI_MONO_NOT_SYNTAX_BASED;
            }
            else
            {
                /*
                 * determine mono pattern info depending on highLightCodes
                 * before / after found string pattern.
                 */
                patRef->prMonoInfo =
                  determineMonoPatInfo(
                    window,
                    matchInfo->meiHighLightCode,
                    absMatchStartPos - 1,
                    absMatchStartPos + matchInfo->meiLength,
                    &patRef->prKind);
            }
        }
        else
        {
            patRef->prMonoInfo = PEMI_NOT_MONO;
        }

        return TRUE;
    }
    else
    {
        /*
         *  Found string can't be assigned to a pattern element
         *  (should never occur).
         */
        return FALSE;
    }
}

/*
** Returns string pattern of given pattern element.
*/
StringPattern *GetStringPattern(
  MatchPatternTable *table,
  PatternElement    *pattern )
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
          return GetStringPattern(
                   table,
                   getPatternOfReference(table, pattern->peVal.peuRef));
          break;
    }

    /*
     * never reached; just to make compiler happy
     */
    return NULL;
}

/*
** Returns pattern element of given pattern reference.
*/
static PatternElement *getPatternOfReference(
  MatchPatternTable *table,
  PatternReference   patRef)
{
    MatchPatternTableElement **element = table->mptElements;

    return element[ patRef.prElementIdx ]->mpteAll.pesPattern[patRef.prPatternIdx];
}

#ifdef DEBUG_FIND
static char *getPatternForDebug(
  MatchPatternTable *table,
  PatternReference   patRef )
{
  if (patRef.prElementIdx < 0)
  {
    return "---";
  }

  return
    GetStringPattern(
      table,
      getPatternOfReference(table, patRef))->spText;
}

static char *patElemKindToString(
  PatternElementKind patElemKind)
{
    if (patElemKind == PEK_START)
        return "START";
    else if (patElemKind == PEK_MIDDLE)
        return "MIDDLE";
    else if (patElemKind == PEK_END)
        return "END";
    else
        return "UNKNOWN";
}

static void printFoundStringForDebug(
  WindowInfo *window,
  int         absStartPos,
  int         length)
{
    char *foundStr =
      BufGetRange( window->buffer, absStartPos, absStartPos + length);

    printf("%d (abs.) <%s>",
      absStartPos,
      foundStr);

    XtFree(foundStr);
}
#endif


