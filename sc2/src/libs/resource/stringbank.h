/*  stringbank.h, Copyright (c) 2005 Michael C. Martin */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope thta it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  Se the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef LIBS_RESOURCE_STRINGBANK_H_
#define LIBS_RESOURCE_STRINGBANK_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stringbank stringbank;

/* Owns immutable, deduplicated strings. Returned pointers remain valid through
 * further insertions, until StringBank_Free; callers must not modify/free them.
 * Callers must synchronize access. Allocation failure terminates the program;
 * C++ exceptions never cross this interface. */

/* Create an empty bank. The caller owns it and must eventually free it. */
stringbank *StringBank_Create(void);

/* Destroy the bank and all its strings, invalidating returned pointers.
 * NULL is allowed. Menu widgets must stop using their labels before this call. */
void StringBank_Free(stringbank *bank);

/* Copy a NUL-terminated string into bank, or return the existing copy.
 * bank/str must be non-NULL. The source may be changed/freed after return.
 * Empty strings are supported; there is no fixed maximum string length. */
const char *StringBank_AddOrFindString(stringbank *bank, const char *str);

/* Split NUL-terminated s on splitchar into at most capacity strings owned by bank.
 * s/bank must be non-NULL; capacity must be nonnegative. result must have space
 * for capacity pointers (NULL is allowed for capacity == 0). Return the number
 * written, leaving unused slots untouched; zero capacity writes nothing.
 * Preserve empty fields. The final slot holds the entire remaining suffix,
 * including any delimiters. A NUL delimiter means no splitting. */
int SplitString(const char *s, char splitchar, int capacity,
        const char **result, stringbank *bank);

#ifdef __cplusplus
}
#endif
#endif /* LIBS_RESOURCE_STRINGBANK_H_ */
