/**
 * @file sha1.h Purple SHA1 Cipher
 * @ingroup core
 */

/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef PURPLE_SHA1_CIPHER_H
#define PURPLE_SHA1_CIPHER_H

#include "cipher.h"

#define PURPLE_TYPE_SHA1_CIPHER				(purple_sha1_cipher_get_gtype())
#define PURPLE_SHA1_CIPHER(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_SHA1_CIPHER, PurpleSHA1Cipher))
#define PURPLE_SHA1_CIPHER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_SHA1_CIPHER, PurpleSHA1CipherClass))
#define PURPLE_IS_SHA1_CIPHER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_SHA1_CIPHER))
#define PURPLE_IS_SHA1_CIPHER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((obj), PURPLE_TYPE_SHA1_CIPHER))
#define PURPLE_SHA1_CIPHER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_SHA1_CIPHER, PurpleSHA1CipherClass))

typedef struct _PurpleSHA1Cipher				PurpleSHA1Cipher;
typedef struct _PurpleSHA1CipherClass		PurpleSHA1CipherClass;

struct _PurpleSHA1Cipher {
	PurpleCipher gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

struct _PurpleSHA1CipherClass {
	PurpleCipherClass gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

GType purple_sha1_cipher_get_gtype(void);

PurpleCipher *purple_sha1_cipher_new(void);

G_END_DECLS

#endif /* PURPLE_SHA1_CIPHER_H */
