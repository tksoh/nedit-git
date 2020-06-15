/* $Id: patternMatchData.h,v 1.3 2003/12/12 16:45:25 uleh Exp $ */

void *FindStringMatchTable(const char *langModeName);

void RenameStringMatchTable(const char *oldName, const char *newName);

void DeleteStringMatchTable(const char *langModeName);

void AssignStandardStringMatchTable(const char *langModeName);

int LMHasStringMatchTable(const char *languageMode);

int LoadMatchPatternString(char *inString);

char *WriteMatchPatternString(void);

void EditMatchPatterns(WindowInfo *window);

void UpdateLanguageModeMenuMatchPattern(void);
