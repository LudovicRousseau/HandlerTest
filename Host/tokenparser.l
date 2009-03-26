/*
 * Reads lexical config files and updates database.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2003
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief provides LTPBundleFindValueWithKey() function on non-MacOS X
 * platforms
 */

%{

#include "parser.h"

void tpevalToken(char *pcToken, int tokType);

static const char *pcDesiredKey = NULL;
static char pcKey[TOKEN_MAX_KEY_SIZE];
static char pcValue[TOKEN_MAX_VALUE_SIZE];
static char pcFinValue[TOKEN_MAX_VALUE_SIZE];
static int valueIndex = 0;
static int desiredIndex = 0;

void tperrorCheck (char *pcToken_error);

%}

%option nounput
%option noyywrap

%%

#.*                                             {}
"\n"                                            {}
\<key\>([A-Z]|[a-z]|[0-9]|[ \t])+\<\/key\>      { valueIndex = 0; tpevalToken(yytext, TOKEN_TYPE_KEY); }
[ \t]                                           {}
\<string\>([A-Z]|[a-z]|[0-9]|[ \t]|[!@#$%^&*()\-+/_\:?.,=~'"])+\<\/string\> {tpevalToken(yytext, TOKEN_TYPE_STRING); valueIndex += 1;}
.                                               { tperrorCheck(yytext); }
%%

#include <stdio.h>
#include <string.h>
#include "debug.h"


void tpevalToken(char *pcToken, int tokType)
{
	unsigned int len;
	len = 0;

	if (tokType == TOKEN_TYPE_KEY)
	{
		/* <key>foobar</key>
		 * 012345 : 5 is the first key character index */

		/* calculate the argument length */
		for (len=0; pcToken[len+5] != '<'; len++)
			;
		len++;	/* final NULL byte */

		if (len > sizeof(pcKey))
		{
			strncpy(pcKey, &pcToken[5], sizeof(pcKey));
			pcKey[sizeof(pcKey) - 1] = '\0';
		}
		else
		{
			strncpy(pcKey, &pcToken[5], len - 5);
			pcKey[len-5] = 0;
		}
	}

	if (tokType == TOKEN_TYPE_STRING)
	{
		/* <string>foobar</string>
		 * 012345678 : 8 is the first string character index */

		/* calculate the argument length */
		for (len=0; pcToken[len+8] != '<'; len++)
			;
		len++;	/* final NULL byte */

		if (len > sizeof(pcValue))
		{
			strncpy(pcValue, &pcToken[8], sizeof(pcValue));
			pcValue[sizeof(pcValue) - 1] = '\0';
		}
		else
		{
			strncpy(pcValue, &pcToken[8], len);
			pcValue[len - 1] = 0;
		}
		if (strcmp(pcKey, pcDesiredKey) == 0)
			if (desiredIndex == valueIndex)
			{
				strncpy(pcFinValue, pcValue, sizeof(pcValue));
				pcFinValue[sizeof(pcValue) - 1] = '\0';
			}
	}
}

void tperrorCheck (char *token_error)
{
	(void)token_error;
}

/**
 * Find a key in a configuration file
 *
 * @param fileName file name
 * @param tokenKey key value
 * @param[out] tokenValue token value (if key found)
 * @param tokenIndice indice of the desired key
 * @retval -1 configuration file not found
 * @retval 0 OK
 * @retval 1 key not found
 */
int LTPBundleFindValueWithKey(const char *fileName, const char *tokenKey,
                              char *tokenValue, int tokenIndice)
{
	FILE *file = NULL;
	int ret = 0;

	desiredIndex  = tokenIndice;
	pcDesiredKey  = tokenKey;
	pcFinValue[0] = '\0';

	file = fopen(fileName, "r");

	if (!file)
	{
		DEBUG3("Could not open bundle file %s: %s",
			fileName, strerror(errno));
		return 1;
	}

	yyin = file;

	do
	{
		(void)yylex();
	} while (!feof(file));

	if (pcFinValue[0] == 0)
	{
		if (tokenIndice == 0)
		{
			/* Not defined at all */
			DEBUG3("Value/Key not defined for: %s, indice: %d",
				tokenKey, tokenIndice);
		}
		ret = -1;
	}
	else
	{
		strncpy(tokenValue, pcFinValue, TOKEN_MAX_VALUE_SIZE);
		tokenValue[TOKEN_MAX_VALUE_SIZE - 1] = '\0';
	}

	(void)fclose(file);
	return ret;
}

