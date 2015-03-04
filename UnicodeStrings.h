#include "Debug.h"

int utf8cmpuni(char * utfString, uint16_t * uniString, uint8_t length);

/**
 * Compares a UTF-8 formatted string with a 16-bit UNICODE one.
 * Compares the second byte of each Unicode character with the only byte
 * in each UTF-8 character.
 *
 * utfString = pointer to UTF-8 encoded char array.
 * uniString = pointer to Unicode encoded uint16_t array.
 * length = length of both strings
 *
 * Returns 0 if equal, greater than 0 for each difference.
 */
int utf8cmpuni(char * utfString, uint16_t * uniString, uint8_t length) {
	uint8_t ret = 0;
	int  k = 0;
	for(; k<length; k++) {
		if(!((utfString[k]) == (uniString[k]&0xFF))) {
			ret++;
		}
	}
	if(DEBUG && VERBOSE) printf("utf8cmpuni returns: %d", ret);
	return ret;
}
