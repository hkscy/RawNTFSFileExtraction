#include "Debug.h"

#ifndef _UTILITY
#define _UTILITY

int aSCIIcmpuni(char * utfString, uint16_t * uniString, uint8_t length);

/**
 * Compares an 8-bit formatted string with a 16-bit UNICODE one.
 * Compares the second byte of each Unicode character with the only byte
 * in each ASCII character.
 *
 * utfString = pointer to ASCI encoded char array.
 * uniString = pointer to UNICODE encoded uint16_t array.
 * length = length of both strings
 *
 * Returns 0 if equal, greater than 0 for each difference.
 */
int aSCIIcmpuni(char * utfString, uint16_t * uniString, uint8_t length) {
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

#endif
