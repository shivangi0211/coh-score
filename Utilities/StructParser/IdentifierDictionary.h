#ifndef _IDENTIFIERDICTIONARY_H_
#define _IDENTIFIERDICTIONARY_H_

#include "tokenizer.h"
#include "windows.h"


typedef enum
{
	IDENTIFIER_NONE,
	IDENTIFIER_ENUM,
	IDENTIFIER_STRUCT,

	//structs that are used as containers are treated as a different identifier type, but are structs
	//for many purposes, so don't forget to check this type if appropriate
	IDENTIFIER_STRUCT_CONTAINER,
} enumIdentifierType;

class IdentifierDictionary
{
public:
	IdentifierDictionary();
	~IdentifierDictionary();

	bool SetFileNameAndLoad(char *pProjPath, char *pProjFileName);

	void DeleteAllFromFile(char *pSourceFileName);

	void AddIdentifier(char *pIdentifierName, char *pSourceFileName, enumIdentifierType eType);

	void WriteOutFile();

	enumIdentifierType FindIdentifier(char *pIdentifierName);
	enumIdentifierType FindIdentifierAndGetSourceFile(char *pIdentifierName, char *pOutSourceFileName);
	enumIdentifierType FindIdentifierAndGetSourceFilePointer(char *pIdentifierName, char **ppOutSourceFileName);

	char *GetSourceFileForIdentifier(char *pIdentifier);

	void BeginIdentifierIterating(enumIdentifierType eType);
	char *GetNextIdentifier();

private:
	typedef struct IdentifierDictionaryNode
	{
		char *pIdentifierName;
		char *pSourceFileName;
		enumIdentifierType eType;
		struct IdentifierDictionaryNode *pNext;
	} IdentifierDictionaryNode;

	char m_DictionaryFileName[MAX_PATH];

	IdentifierDictionaryNode *m_pFirst;

	bool m_bSomethingHasChanged;

	enumIdentifierType m_eCurIteratorType;
	IdentifierDictionaryNode *m_pNextIteratorNode;


private:
	void DeleteNode(IdentifierDictionaryNode **ppNode);
};



#endif