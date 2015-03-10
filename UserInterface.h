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
#define KWHT  "\x1B[37m"
#define KRESET "\033[0m"

/* Clean up command checking */
#define ENTERED(cmd) strcmp(cmd, userInput) == 0

#define	CMD_BUFF 128			/*Buffer for user string input */

#define PRINT_HELP 	1
#define PRINT_FILES 2
#define EXIT		3
#define UNKNOWN		-1

#define HELP_CMD			"help"
#define PRINT_FILES_CMD		"print files"
#define EXIT_CMD			"exit"

#define HELP \
"From here you can issue the following commands:\n\
\t" KWHT "%s" KRESET " - Display this menu.\n\
\t" KWHT "%s" KRESET " - Print out a list of all file names found on volume.\n\
\t" KWHT "%s" KRESET " - Close this program.\n", \
HELP_CMD, \
PRINT_FILES_CMD, \
EXIT_CMD

int8_t parseUserInput(char * userInput) {

	userInput[ strcspn(userInput, "\r\n") ] = 0; /*Replaces LF with \0 */

	if		( ENTERED(HELP_CMD) ) 		 { return PRINT_HELP; }
	else if ( ENTERED(PRINT_FILES_CMD) ) { return PRINT_FILES; }
	else if ( ENTERED(EXIT_CMD) ) 		 { return EXIT;	}
	else 								 { return UNKNOWN; }

}

#endif /* USERINTERFACE_H_ */
