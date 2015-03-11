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
#define PRINT_HELP 	1
#define PRINT_FILES 2
#define SRCH_FOR	3
#define EXIT		127
#define UNKNOWN		-1

#define SRCH_NUM	1
#define SRCH_OFFS	2
#define SRCH_NAME	3

/* User commands */
#define HELP_CMD			"help"
#define PRINT_FILES_CMD		"print files"
#define SRCH_MFTN_CMD		"search for"
#define EXIT_CMD			"exit"

/* User search commands */
#define SRCH_NUM_CMD 		"record number"
#define SRCH_OFFS_CMD 		"offset"
#define SRCH_NAME_CMD		"name"

#define HELP \
"From here you can issue the following commands:\n\
\t" KWHT "%s" KRESET " - Display this menu.\n\
\t" KWHT "%s" KRESET " - Print out a list of all file names found on volume.\n\
\t" KWHT "%s" KRESET " - Search for a specific file, using either it's record number or disk offset value.\n\
\t" KWHT "%s" KRESET " - Close this program.\n", \
HELP_CMD, \
PRINT_FILES_CMD, \
SRCH_MFTN_CMD, \
EXIT_CMD

#define SEARCH \
"How would you like to search? choose from:\n\
" KWHT "%s" KRESET " - Search by specifying an MFT record number.\n\
" KWHT "%s" KRESET " - Search by specifying a disk offset to the file, in bytes.\n\
" KWHT "%s" KRESET " - Search by specifying a file name, must be exact.\n\
(or " KWHT "%s" KRESET ").\n", \
SRCH_NUM_CMD, \
SRCH_OFFS_CMD, \
SRCH_NAME_CMD, \
EXIT_CMD

#define SEARCHTERM "Enter the search term: "

int8_t parseUserInput(char * userInput) {

	userInput[ strcspn(userInput, "\r\n") ] = 0; /*Replaces LF with \0 */

	if		( ENTERED(HELP_CMD) ) 		 { return PRINT_HELP; }
	else if ( ENTERED(PRINT_FILES_CMD) ) { return PRINT_FILES; }
	else if ( ENTERED(SRCH_MFTN_CMD) )	 { return SRCH_FOR; }
	else if ( ENTERED(EXIT_CMD) ) 		 { return EXIT;	}
	else 								 { return UNKNOWN; }

}

int8_t searchForMenu() {
	printf(SEARCH);
	char userInput[CMD_BUFF];
	fgets(userInput, CMD_BUFF-1, stdin);
	userInput[ strcspn(userInput, "\r\n") ] = 0; /*Replaces LF with \0 */
	if		( ENTERED(SRCH_NUM_CMD) )    { return SRCH_NUM; }
	else if ( ENTERED(SRCH_OFFS_CMD) )   { return SRCH_OFFS; }
	else if ( ENTERED(SRCH_NAME_CMD) )	 { return SRCH_NAME; }
	else if ( ENTERED(EXIT_CMD) )		 { return EXIT;		}
	else								 { return UNKNOWN;	}
}

char *getSearchTerm() {
	printf(SEARCHTERM);
	char *userInput = malloc( sizeof(CMD_BUFF) );
	fgets(userInput, CMD_BUFF-1, stdin);
	userInput[ strcspn(userInput, "\r\n") ] = 0; /*Replaces LF with \0 */
	return userInput;
}

//char * getSearchNum() {}

#endif /* USERINTERFACE_H_ */
