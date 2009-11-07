// Dialogs.h

//////////////////////////////////////////////////////////////////////


void ShowAboutBox();

// Input octal value
//   strTitle - dialog caption
//   strPrompt - label text
BOOL InputBoxOctal(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strPrompt, WORD* pValue);

BOOL ShowSaveDialog(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strFilter, TCHAR* bufFileName);

void ShowCreateDiskDialog();


//////////////////////////////////////////////////////////////////////
