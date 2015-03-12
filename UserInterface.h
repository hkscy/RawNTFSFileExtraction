/*
 * UserInterface.h
 *
 *  Created on: 10 Mar 2015
 *      Author: Christopher Hicks
 */

#include <string.h>

#ifndef USERINTERFACE_H_
#define USERINTERFACE_H_

/* Colour codes for enhancing readability*/
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define KRESET "\033[0m"

/* Clean up command checking */
#define ENTERED(cmd) strcmp(cmd, userInput) == 0

/*Buffer for user string input */
#define	CMD_BUFF 128

/* parseUserInput Return codes */
#define PRINT_HELP 		1
#define PRINT_FILES 	2
#define SRCH_FOR_MFTN	3
#define SRCH_FOR_MFTC	4
#define SRCH_FOR_MFTO	5
#define EXT_MFTN		6
#define EXIT			127
#define UNKNOWN			-1

#define SRCH_NUM		1
#define SRCH_OFFS		2
#define SRCH_NAME		3

/* User commands */
#define HELP_CMD			"help"
#define PRINT_FILES_CMD		"print files"
#define SRCH_MFTN_CMD		"search using record number"
#define SRCH_MFTC_CMD		"search using record name"
#define SRCH_MFTO_CMD		"search using record offset"
#define EXT_MFTN_CMD		"extract using record number"
#define EXIT_CMD			"exit"


#define HELP \
"From here you can issue the following commands:\n\
\t" KWHT "%s" KRESET " - Display this menu.\n\
\t" KWHT "%s" KRESET " - Print out a list of all file names found on volume.\n\
\t" KWHT "%s" KRESET " - Search for a specific file, using it's MFT record number.\n\
\t" KWHT "%s" KRESET " - Search for a specific file, using it's file name.\n\
\t" KWHT "%s" KRESET " - Search for a specific file, using it's absolute offset in bytes.\n\
\t" KWHT "%s" KRESET " - Extract a specific file, using it's MFT record number.\n\
\t" KWHT "%s" KRESET " - Close this program.\n", \
HELP_CMD, \
PRINT_FILES_CMD, \
SRCH_MFTN_CMD, \
SRCH_MFTC_CMD, \
SRCH_MFTO_CMD, \
EXT_MFTN_CMD, \
EXIT_CMD

#define SEARCHTERM "Enter the search term: "

int8_t parseUserInput(char * userInput) {

	userInput[ strcspn(userInput, "\r\n") ] = 0; /*Replaces LF with \0 */

	if		( ENTERED(HELP_CMD) ) 		 { return PRINT_HELP; }
	else if ( ENTERED(PRINT_FILES_CMD) ) { return PRINT_FILES; }
	else if ( ENTERED(SRCH_MFTN_CMD) )	 { return SRCH_FOR_MFTN; }
	else if ( ENTERED(SRCH_MFTC_CMD) )	 { return SRCH_FOR_MFTC; }
	else if ( ENTERED(SRCH_MFTO_CMD) )	 { return SRCH_FOR_MFTO; }
	else if ( ENTERED(EXT_MFTN_CMD) )	 { return EXT_MFTN; }
	else if ( ENTERED(EXIT_CMD) ) 		 { return EXIT;	}
	else 								 { return UNKNOWN; }

}

char *getSearchTerm() {
	printf(SEARCHTERM);
	char *userInput = malloc( sizeof(CMD_BUFF) );
	fgets(userInput, CMD_BUFF-1, stdin);
	userInput[ strcspn(userInput, "\r\n") ] = 0; /*Replaces LF with \0 */
	return userInput;
}

#endif /* USERINTERFACE_H_ */
