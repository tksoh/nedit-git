/* $Id: patternMatch.h,v 1.2 2003/10/11 16:45:25 uleh Exp $ */

#define NO_GROUP_IDX        -1
#define NO_ELEMENT_IDX      -1
#define NO_PATTERN_IDX      -1

#define MAX_GLOBAL_BACK_REF_ID  9
#define NO_LOCAL_BACK_REF_ID   -1

typedef enum {
  PEK_UNKNOWN,
  PEK_START,
  PEK_END,
  PEK_MIDDLE
} PatternElementKind;

typedef enum {
  PEMI_NOT_MONO,
  PEMI_MONO_NOT_SYNTAX_BASED,
  PEMI_MONO_AMBIGUOUS_SYNTAX,
  PEMI_MONO_DEFINITE_SYNTAX
} PatternElementMonoInfo;

typedef struct _PatternReference {
  PatternElementKind     prKind;
  PatternElementMonoInfo prMonoInfo;
  int                    prElementIdx;
  int                    prPatternIdx;
} PatternReference;

typedef enum {
  MPT_INDIVIDUAL,
  MPT_SUB,
  MPT_GROUP
} MatchPatternType;

typedef struct _MatchPatternSequenceElement {
  char             *mpseName;
  MatchPatternType  mpseType;
  int               mpseIndex;
} MatchPatternSequenceElement;

typedef struct _MatchPatternGroupElement {
  char    *mpgeName;
  int      mpgeNumberOfSubPatterns;
  char   **mpgeSubPatternIds;
  regexp  *mpgeKeywordRE;
} MatchPatternGroupElement;

typedef enum {
  PWB_NONE,
  PWB_LEFT,
  PWB_RIGHT,
  PWB_BOTH
} PatternWordBoundary;

typedef struct _StrPatBackRefElement{
  int   spbreLocalBackRefID;
  char *spbreRegExpText;
} StrPatBackRefElement;

typedef struct _StringPattern {
  char                 *spText;
  int                   spLength;
  PatternWordBoundary   spWordBoundary;
  int                   spCaseInsensitive;
  int                   spRegularExpression;
  regexp               *spTextRE;
  char                 *spOrigText;
  int                   spBackRefParsed;
  int                   spBackRefResolved;
  StrPatBackRefElement  spOwnGlobalBackRef[MAX_GLOBAL_BACK_REF_ID];
  int                   spGlobalToLocalBackRef[MAX_GLOBAL_BACK_REF_ID];
} StringPattern;

typedef struct _MultiPattern {
  StringPattern      mpStringPattern;
  int                mpNumberOfReferences;
  PatternReference  *mpRefList;
} MultiPattern;

typedef enum {
  PET_SINGLE,
  PET_MULTIPLE,
  PET_REFERENCE
} PatternElementType;

typedef struct _PatternElement {
  int                peIndex;
  PatternElementKind peKind;
  PatternElementType peType;
  union {
    StringPattern    peuSingle;
    MultiPattern     peuMulti;
    PatternReference peuRef;
  } peVal;
} PatternElement;

typedef struct _PatternElementSet {
  int                pesNumberOfPattern;
  PatternElement   **pesPattern;
} PatternElementSet;

typedef struct _GlobalBackRefElement{
  StringPattern *gbreDefByStrPat;
  char          *gbreRegExpText;
} GlobalBackRefElement;

typedef struct _MatchPatternTableElement {
  char                 *mpteName;
  int                   mpteIndex;
  MatchPatternType      mpteType;
  int                   mpteGroup;
  PatternElementSet     mpteAll;
  PatternElementSet     mpteStart;
  PatternElementSet     mpteMiddle;
  PatternElementSet     mpteEnd;
  int                   mpteFlash;
  int                   mpteIsMonoPattern;
  int                   mpteSkipBtwnStartEnd;
  int                   mpteIgnoreHighLightInfo;
  regexp               *mpteStartEndRE;
  GlobalBackRefElement  mpteGlobalBackRef[MAX_GLOBAL_BACK_REF_ID];
} MatchPatternTableElement;

typedef struct _MatchPatternTable {
  int                        mptNumberOfElements;
  MatchPatternTableElement **mptElements;
} MatchPatternTable;

typedef struct _StringMatchTable {
  char                         *smtLanguageMode;
  MatchPatternTable            *smtAllPatterns;
  regexp                       *smtAllPatRE;
  regexp                       *smtFlashPatRE;
  regexp                       *smtUsedPatRE;
  int                           smtNumberOfGroups;
  MatchPatternGroupElement    **smtGroups;
  int                           smtNumberOfSeqElements;
  MatchPatternSequenceElement **smtSequence;
} StringMatchTable;

typedef enum {
  MT_FLASH_DELIMIT,
  MT_FLASH_RANGE,
  MT_SELECT,
  MT_GOTO,
  MT_MACRO
} MatchingType;

int FindMatchingString(
  WindowInfo   *window,
  MatchingType  matchingType,
  int          *charPos,
  int           startLimit,
  int           endLimit,
  int          *matchPos,
  int          *matchLength,
  int          *direction);

StringPattern *GetStringPattern(
  MatchPatternTable *table,
  PatternElement *pattern);
