#include "AutoRunManager.h"
#include "strutils.h"
#include "sourceparser.h"


#define AUTORUN_WILDCARD_PREFIX "AUTO_RUN_"

AutoRunManager::AutoRunManager()
{
	m_bSomethingChanged = false;
	m_iNumAutoRuns = 0;
	m_AutoRunFileName[0] = 0;
	m_ShortAutoRunFileName[0] = 0;	

	memset(m_AutoRuns, 0, sizeof(m_AutoRuns));
}

AutoRunManager::~AutoRunManager()
{
}

char *AutoRunManager::GetMagicWord(int iWhichMagicWord)
{
	switch (iWhichMagicWord)
	{
	case AUTORUN_ORDER_FIRST:
		return "AUTO_RUN_FIRST";
	case AUTORUN_ORDER_EARLY:
		return "AUTO_RUN_EARLY";
	case AUTORUN_ORDER_NORMAL:
		return "AUTO_RUN";
	case AUTORUN_ORDER_LATE:
		return "AUTO_RUN_LATE";
	case AUTORUN_WILDCARD:
		return AUTORUN_WILDCARD_PREFIX WILDCARD_STRING;
	case AUTORUN_ANON:
		return "AUTO_RUN_ANON";

	}

	return "x x";
}


typedef enum
{
	RW_PARSABLE = RW_COUNT,
};

static char *sAutoRunReservedWords[] =
{
	"PARSABLE",
	NULL
};

void AutoRunManager::SetProjectPathAndName(char *pProjectPath, char *pProjectName)
{
	strcpy(m_ProjectName, pProjectName);

	sprintf(m_ShortAutoRunFileName, "%s_autorun_autogen", pProjectName);
	sprintf(m_AutoRunFileName, "%s\\AutoGen\\%s.cpp", pProjectPath, m_ShortAutoRunFileName);

	sprintf(m_AutoRunExtraFuncFileName, "%s\\AutoGen\\%s_autorun_extrafunc_autogen.c", pProjectPath, pProjectName);

}

bool AutoRunManager::DoesFileNeedUpdating(char *pFileName)
{
	return false;
}


bool AutoRunManager::LoadStoredData(bool bForceReset)
{
	if (bForceReset)
	{
		m_bSomethingChanged = true;
		return false;
	}

	Tokenizer tokenizer;

	if (!tokenizer.LoadFromFile(m_AutoRunFileName))
	{
		m_bSomethingChanged = true;
		return false;
	}

	if (!tokenizer.IsStringAtVeryEndOfBuffer("#endif"))
	{
		m_bSomethingChanged = true;
		return false;
	}

	tokenizer.SetExtraReservedWords(sAutoRunReservedWords);

	Token token;
	enumTokenType eType;

	do
	{
		eType = tokenizer.GetNextToken(&token);
		Tokenizer::StaticAssert(eType != TOKEN_NONE, "AUTORUN data corruption");
	} while (!(eType == TOKEN_RESERVEDWORD && token.iVal == RW_PARSABLE));

	tokenizer.SetCSourceStyleStrings(true);

	tokenizer.AssertNextTokenTypeAndGet(&token, TOKEN_INT, 0, "Didn't find number of autruns");

	m_iNumAutoRuns = token.iVal;

	int iAutoRunNum;

	for (iAutoRunNum = 0; iAutoRunNum < m_iNumAutoRuns; iAutoRunNum++)
	{
		AUTO_RUN_STRUCT *pAutoRun = &m_AutoRuns[iAutoRunNum];

		tokenizer.AssertNextTokenTypeAndGet(&token, TOKEN_STRING, MAX_AUTORUN_COMMAND_LENGTH, "Didn't find autorun function name");
		strcpy(pAutoRun->functionName, token.sVal);

		tokenizer.AssertNextTokenTypeAndGet(&token, TOKEN_STRING, MAX_PATH, "Didn't find autorun source file");
		strcpy(pAutoRun->sourceFileName, token.sVal);

		tokenizer.AssertNextTokenTypeAndGet(&token, TOKEN_INT, 0, "Didn't find autorun order");
		pAutoRun->iOrder = token.iVal;

		tokenizer.AssertNextTokenTypeAndGet(&token, TOKEN_STRING, 0, "Didn't find autorun declarations");

		if (token.sVal[0])
		{
			char temp[TOKENIZER_MAX_STRING_LENGTH];
			RemoveCStyleEscaping(temp, token.sVal);
			pAutoRun->pDeclarations = STRDUP(temp);
		}
		else
		{
			pAutoRun->pDeclarations = NULL;
		}
	
		
		tokenizer.AssertNextTokenTypeAndGet(&token, TOKEN_STRING, 0, "Didn't find autorun code");

		if (token.sVal[0])
		{
			char temp[TOKENIZER_MAX_STRING_LENGTH];
			RemoveCStyleEscaping(temp, token.sVal);
			pAutoRun->pCode = STRDUP(temp);
		}
		else
		{
			pAutoRun->pCode = NULL;
		}
	}

	m_bSomethingChanged = false;

	return true;
}

void AutoRunManager::DeleteAutoRun(AUTO_RUN_STRUCT *pAutoRun)
{
	if (pAutoRun->pDeclarations)
	{
		delete(pAutoRun->pDeclarations);
	}

	if (pAutoRun->pCode)
	{
		delete(pAutoRun->pCode);
	}
}


void AutoRunManager::ResetSourceFile(char *pSourceFileName)
{
	int i = 0;

	while (i < m_iNumAutoRuns)
	{
		if (AreFilenamesEqual(m_AutoRuns[i].sourceFileName, pSourceFileName))
		{
			DeleteAutoRun(&m_AutoRuns[i]);
			memcpy(&m_AutoRuns[i], &m_AutoRuns[m_iNumAutoRuns - 1], sizeof(AUTO_RUN_STRUCT));
			m_iNumAutoRuns--;

			m_bSomethingChanged = true;
		}
		else
		{
			i++;
		}
	}
}

int AutoRunComparator(const void *pAutoRun1, const void *pAutoRun2)
{
	return strcmp(((AUTO_RUN_STRUCT*)pAutoRun1)->functionName, ((AUTO_RUN_STRUCT*)pAutoRun2)->functionName);
}

void AutoRunManager::WriteOutAnonAutoRunIncludes(FILE *pOutFile)
{
	//check if *_AnonAutoRunIncludes.h is included in the parent file
	int i;
	int iNumFiles = m_pParent->GetNumProjectFiles();

	for (i=0; i < iNumFiles; i++)
	{
		char *pFileName = m_pParent->GetNthProjectFile(i);

		if (strstri(pFileName, "AnonAutoRunIncludes.h"))
		{
			fprintf(pOutFile, "\n\n//including %s, so that ANON_AUTORUNs will get the header files they need\n#include \"%s\"\n", pFileName, pFileName);
		}
	}

}


bool AutoRunManager::WriteOutData(void)
{
	if (!m_bSomethingChanged)
	{
		return false;
	}

	qsort(m_AutoRuns, m_iNumAutoRuns, sizeof(AUTO_RUN_STRUCT), AutoRunComparator);

	FILE *pOutFile = fopen_nofail(m_AutoRunFileName, "wt");

	fprintf(pOutFile, "//This file contains data and prototypes for autoruns. It is autogenerated by StructParser\n\n\n// autogenerated" "nocheckin\n\n");

	fprintf(pOutFile, "extern \"C\"\n{\n");


	int iAutoRunNum;

	for (iAutoRunNum = 0; iAutoRunNum < m_iNumAutoRuns; iAutoRunNum++)
	{
		AUTO_RUN_STRUCT *pAutoRun = &m_AutoRuns[iAutoRunNum];

		fprintf(pOutFile, "extern void %s(void);\n", pAutoRun->functionName);
	}

	int iLibNum, iOrderNum;

	for (iLibNum = 0; iLibNum < m_pParent->GetNumLibraries(); iLibNum++)
	{
		for (iOrderNum = 0; iOrderNum < AUTORUN_ORDER_COUNT; iOrderNum++)
		{
			fprintf(pOutFile, "void doAutoRuns_%s_%d(void);\n", m_pParent->GetNthLibraryName(iLibNum), iOrderNum);
		}
	}

	for (iOrderNum = 0; iOrderNum < AUTORUN_ORDER_COUNT; iOrderNum++)
	{
		fprintf(pOutFile, "\n\nvoid doAutoRuns_%s_%d(void)\n{\n", 
			m_pParent->GetShortProjectName(), iOrderNum);

		fprintf(pOutFile, "\tstatic int once = 0;\n\tif (once) return;\n\tonce = 1;\n");


		for (iLibNum = 0; iLibNum < m_pParent->GetNumLibraries(); iLibNum++)
		{	
			if (m_pParent->IsNthLibraryXBoxExcluded(iLibNum))
			{
				fprintf(pOutFile, "\t#ifndef _XBOX\n");
			}

			fprintf(pOutFile, "\tdoAutoRuns_%s_%d();\n", m_pParent->GetNthLibraryName(iLibNum), iOrderNum);		
	
			if (m_pParent->IsNthLibraryXBoxExcluded(iLibNum))
			{
				fprintf(pOutFile, "\t#endif\n");
			}
		}

		for (iAutoRunNum = 0; iAutoRunNum < m_iNumAutoRuns; iAutoRunNum++)
		{
			AUTO_RUN_STRUCT *pAutoRun = &m_AutoRuns[iAutoRunNum];

			if (pAutoRun->iOrder == iOrderNum)
			{
				fprintf(pOutFile, "\t%s();\n", pAutoRun->functionName);
			}
		}

		fprintf(pOutFile, "}\n\n");
	}

	fprintf(pOutFile, "extern void utilitiesLibPreAutoRunStuff(void);\n");

	fprintf(pOutFile, "int MagicAutoRunFunc_%s(void)\n{\n", m_pParent->GetShortProjectName());


	fprintf(pOutFile, "\tutilitiesLibPreAutoRunStuff();\n");

	for (iOrderNum = 0; iOrderNum < AUTORUN_ORDER_COUNT; iOrderNum++)
	{
		fprintf(pOutFile, "\tdoAutoRuns_%s_%d();\n", 
			m_pParent->GetShortProjectName(), iOrderNum);
	}

	fprintf(pOutFile, "\treturn 0;\n}\n\n");

	if (m_pParent->ProjectIsExecutable())
	{
		fprintf(pOutFile, "void do_auto_runs(void)\n{\n\tMagicAutoRunFunc_%s();\n}\n\n",
			m_pParent->GetShortProjectName());
	}

	fprintf(pOutFile, "};\n\n\n#if 0\nPARSABLE\n");
	
	fprintf(pOutFile, "%d\n", m_iNumAutoRuns);

	for (iAutoRunNum = 0; iAutoRunNum < m_iNumAutoRuns; iAutoRunNum++)
	{
		AUTO_RUN_STRUCT *pAutoRun = &m_AutoRuns[iAutoRunNum];
		
		fprintf(pOutFile, "\"%s\" \"%s\" %d ", pAutoRun->functionName, pAutoRun->sourceFileName, pAutoRun->iOrder);

		if (pAutoRun->pDeclarations)
		{
			char temp[TOKENIZER_MAX_STRING_LENGTH + 1000];

			AddCStyleEscaping(temp, pAutoRun->pDeclarations, TOKENIZER_MAX_STRING_LENGTH + 999);

			fprintf(pOutFile, "\"%s\" ", temp);
		}
		else
		{
			fprintf(pOutFile, "\"\" ");
		}

		if (pAutoRun->pCode)
		{
			char temp[TOKENIZER_MAX_STRING_LENGTH + 1000];

			AddCStyleEscaping(temp, pAutoRun->pCode, TOKENIZER_MAX_STRING_LENGTH + 999);

			fprintf(pOutFile, "\"%s\" ", temp);
		}
		else
		{
			fprintf(pOutFile, "\"\" ");
		}

		fprintf(pOutFile, "\n");


	}

	fprintf(pOutFile, "#endif\n");

	fclose(pOutFile);

	pOutFile = fopen_nofail(m_AutoRunExtraFuncFileName, "wt");

	fprintf(pOutFile, "//This file contains data and prototypes for autoruns. It is autogenerated by StructParser\n\n\n// autogenerated" "nocheckin\n\n");

	WriteOutAnonAutoRunIncludes(pOutFile);

	//print out function bodies for any autoruns that have internal bodies/declarations

	for (iAutoRunNum = 0; iAutoRunNum < m_iNumAutoRuns; iAutoRunNum++)
	{
		AUTO_RUN_STRUCT *pAutoRun = &m_AutoRuns[iAutoRunNum];
		
		if (pAutoRun->pCode)
		{
			fprintf(pOutFile, "//declarations/code for internal autorun %s\n", pAutoRun->functionName);

			if (pAutoRun->pDeclarations)
			{
				fprintf(pOutFile, "%s\n", pAutoRun->pDeclarations);
			}

			fprintf(pOutFile, "void %s(void)\n{\n%s\n}\n", pAutoRun->functionName, pAutoRun->pCode);
		}
	}

	fclose(pOutFile);

	return true;
}
	






void AutoRunManager::FoundMagicWord(char *pSourceFileName, Tokenizer *pTokenizer, int iWhichMagicWord, char *pMagicWordString)
{
	Tokenizer tokenizer;

	Token token;
	enumTokenType eType;

	if (iWhichMagicWord ==  MAGICWORD_BEGINNING_OF_FILE || iWhichMagicWord == MAGICWORD_END_OF_FILE)
	{
		return;
	}

	if (iWhichMagicWord == AUTORUN_ANON)
	{
		Token token;
		char funcName[256];
		int iLineNum;

		//all this gibberish is to support replacing __FILE__ in the AUTO_RUN_ANON with the actual filename, properly
		//turned into a C-source-style escaped string
		char fileNameReplaceString1[MAX_PATH * 2 + 10];
		char fileNameReplaceString2[MAX_PATH * 2 + 10];

		char *macros[][2] =
		{
			{
				"__FILE__",
				fileNameReplaceString2,
			},
			{
				NULL,
				NULL
			}
		};

		AddCStyleEscaping(fileNameReplaceString1, pSourceFileName, MAX_PATH * 2 + 5);
		sprintf(fileNameReplaceString2, "\"%s\"", fileNameReplaceString1);


		sprintf(funcName, "%s_ANON_AUTORUN_%d", 
			GetFileNameWithoutDirectories(pSourceFileName), pTokenizer->GetOffset(&iLineNum));
		MakeStringAllAlphaNum(funcName);

		pTokenizer->AssertNextTokenTypeAndGet(&token, TOKEN_RESERVEDWORD, RW_LEFTPARENS, "Didn't find ( after AUTO_RUN_WILDCARD");
		pTokenizer->GetSpecialStringTokenWithParenthesesMatching(&token);
		
		ReplaceMacrosInPlace(token.sVal, macros);

		
		AddAutoRunWithBody(funcName, pSourceFileName, "", token.sVal, AUTORUN_ORDER_NORMAL);
		return;
	}



	pTokenizer->Assert(m_iNumAutoRuns < MAX_AUTORUNS, "Too many autoruns");

	AUTO_RUN_STRUCT *pAutoRun = &m_AutoRuns[m_iNumAutoRuns++];

	m_bSomethingChanged = true;

	strcpy(pAutoRun->sourceFileName, pSourceFileName);

	pAutoRun->pCode = pAutoRun->pDeclarations = NULL;

	if (iWhichMagicWord == AUTORUN_WILDCARD)
	{
		pTokenizer->AssertNextTokenTypeAndGet(&token, TOKEN_RESERVEDWORD, RW_LEFTPARENS, "Didn't find ( after AUTO_RUN_WILDCARD");
		pTokenizer->AssertNextTokenTypeAndGet(&token, TOKEN_IDENTIFIER, 0, "Didn't find identifier after AUTO_RUN_WILDCARD");

		char autoGenFuncName[4096];

		sprintf(autoGenFuncName, "_AUTOGEN_%s%s", pMagicWordString + strlen(AUTORUN_WILDCARD_PREFIX), token.sVal);

		pTokenizer->Assertf(strlen(autoGenFuncName) < MAX_AUTORUN_COMMAND_LENGTH, "assumed AUTO_RUN function name %s too long", 
			autoGenFuncName);

		strcpy(pAutoRun->functionName, autoGenFuncName);

		pAutoRun->iOrder = AUTORUN_ORDER_NORMAL;
		return;

	}







	pTokenizer->AssertNextTokenTypeAndGet(&token, TOKEN_RESERVEDWORD, RW_SEMICOLON, "Didn't find ; after AUTO_RUN");

	eType = pTokenizer->GetNextToken(&token);

	pTokenizer->Assert(eType == TOKEN_RESERVEDWORD && token.iVal == RW_VOID 
		|| eType == TOKEN_IDENTIFIER && strcmp(token.sVal, "int") == 0, "Expected int or void after AUTO_RUN;");

	pTokenizer->AssertNextTokenTypeAndGet(&token, TOKEN_IDENTIFIER, MAX_AUTORUN_COMMAND_LENGTH, "Didn't find auto run name after int");

	strcpy(pAutoRun->functionName, token.sVal);
	pAutoRun->iOrder = iWhichMagicWord;

	pTokenizer->AssertNextTokenTypeAndGet(&token, TOKEN_RESERVEDWORD, RW_LEFTPARENS, "Didn't find ( after auto run name");
	pTokenizer->AssertNextTokenTypeAndGet(&token, TOKEN_RESERVEDWORD, RW_VOID, "Didn't find void after (");
	pTokenizer->AssertNextTokenTypeAndGet(&token, TOKEN_RESERVEDWORD, RW_RIGHTPARENS, "Didn't find ) after void");
}

void AutoRunManager::AddAutoRun(char *pFuncName, char *pSourceFileName)
{
	AddAutoRunWithBody(pFuncName, pSourceFileName, NULL, NULL, AUTORUN_ORDER_INTERNAL);
}

void AutoRunManager::AddAutoRunWithBody(char *pFuncName, char *pSourceFileName, char *pDeclarations, char *pCode, int iOrder)
{
	if (m_iNumAutoRuns >= MAX_AUTORUNS)
	{
		printf("Too many autoruns");
	
		fflush(stdout);

		Sleep(100);
		exit(1);
	}

	AUTO_RUN_STRUCT *pAutoRun = &m_AutoRuns[m_iNumAutoRuns++];

	Tokenizer::StaticAssertf(strlen(pFuncName) < MAX_AUTORUN_COMMAND_LENGTH, "AUTORUN name %s too long", pFuncName);

	strcpy(pAutoRun->functionName, pFuncName);
	strcpy(pAutoRun->sourceFileName, pSourceFileName);
	pAutoRun->iOrder = iOrder;

	if (pCode)
	{
		pAutoRun->pCode = STRDUP(pCode);
		if (pDeclarations)
		{
			pAutoRun->pDeclarations = STRDUP(pDeclarations);
		}
		else
		{
			pAutoRun->pDeclarations = NULL;
		}
	}
	else
	{
		assert(!pDeclarations);
		pAutoRun->pDeclarations = pAutoRun->pCode = NULL;
	}

	m_bSomethingChanged = true;

}


void AutoRunManager::AddAutoRunSpecial(char *pFuncName, char *pSourceFileName, bool bCheckIfAlreadyExists, int iOrder)
{
	if (bCheckIfAlreadyExists)
	{
		int i;

		for (i=0; i < m_iNumAutoRuns; i++)
		{
			if (strcmp(pFuncName, m_AutoRuns[i].functionName) == 0)
			{
				return;
			}
		}
	}

	Tokenizer::StaticAssertf(strlen(pFuncName) < MAX_AUTORUN_COMMAND_LENGTH, "AUTORUN name %s too long", pFuncName);


	if (m_iNumAutoRuns >= MAX_AUTORUNS)
	{
		printf("Too many autoruns");
	
		fflush(stdout);

		Sleep(100);
		exit(1);
	}

	AUTO_RUN_STRUCT *pAutoRun = &m_AutoRuns[m_iNumAutoRuns++];

	strcpy(pAutoRun->functionName, pFuncName);
	strcpy(pAutoRun->sourceFileName, pSourceFileName);
	pAutoRun->iOrder = iOrder;
	pAutoRun->pDeclarations = pAutoRun->pCode = NULL;

	m_bSomethingChanged = true;



}




//returns number of dependencies found
int AutoRunManager::ProcessDataSingleFile(char *pSourceFileName, char *pDependencies[MAX_DEPENDENCIES_SINGLE_FILE])
{
	return 0;
}













