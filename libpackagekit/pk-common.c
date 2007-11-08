/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib/gi18n.h>

#include "pk-debug.h"
#include "pk-common.h"
#include "pk-enum.h"

/**
 * pk_filter_check:
 **/
gboolean
pk_filter_check (const gchar *filter)
{
	gchar **sections;
	guint i;
	guint length;
	gboolean ret;

	if (filter == NULL) {
		pk_warning ("filter null");
		return FALSE;
	}
	if (pk_strzero (filter) == TRUE) {
		pk_warning ("filter zero length");
		return FALSE;
	}

	/* split by delimeter ';' */
	sections = g_strsplit (filter, ";", 0);
	length = g_strv_length (sections);
	ret = FALSE;
	for (i=0; i<length; i++) {
		/* only one wrong part is enough to fail the filter */
		if (pk_strzero (sections[i]) == TRUE) {
			goto out;
		}
		if (pk_filter_enum_from_text (sections[i]) == PK_FILTER_ENUM_UNKNOWN) {
			goto out;
		}
	}
	ret = TRUE;
out:
	g_strfreev (sections);
	return ret;
}

/**
 * pk_strvalidate_char:
 **/
static gboolean
pk_strvalidate_char (gchar item)
{
	switch (item) {
	case ' ':
	case '$':
	case '`':
	case '\'':
	case '"':
	case '^':
	case '[':
	case ']':
	case '{':
	case '}':
	case '#':
	case '\\':
	case '<':
	case '>':
	case '|':
		return FALSE;
	}
	return TRUE;
}

/**
 * pk_strsafe:
 **/
gchar *
pk_strsafe (const gchar *text)
{
	gchar *text_safe;
	const gchar *delimiters;

	/* rip out any insane characters */
	delimiters = "\\\f\n\r\t\"'";
	text_safe = g_strdup (text);
	g_strdelimit (text_safe, delimiters, ' ');
	return text_safe;
}

/**
 * pk_strnumber:
 **/
gboolean
pk_strnumber (const gchar *text)
{
	guint i;
	guint length;

	/* check explicitly */
	if (pk_strzero (text) == TRUE) {
		return FALSE;
	}

	/* ITS4: ignore, not used for allocation and checked for oversize */
	length = strlen (text);
	for (i=0; i<length; i++) {
		if (i > 10) {
			pk_debug ("input too long!");
			return FALSE;
		}
		if (i == 0 && text[i] == '-') {
			/* negative sign */
		} else if (g_ascii_isdigit (text[i]) == FALSE) {
			pk_debug ("not a number '%c' in text!", text[i]);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * pk_strtoint:
 **/
gboolean
pk_strtoint (const gchar *text, gint *value)
{
	gboolean ret;
	ret = pk_strnumber (text);
	if (ret == FALSE) {
		*value = 0;
		return FALSE;
	}
	/* ITS4: ignore, we've already checked for validity */
	*value = atoi (text);
	return TRUE;
}

/**
 * pk_strtouint:
 **/
gboolean
pk_strtouint (const gchar *text, guint *value)
{
	gboolean ret;
	gint temp;
	ret = pk_strtoint (text, &temp);
	if (ret == FALSE || temp < 0) {
		*value = 0;
		return FALSE;
	}
	*value = (guint) temp;
	return TRUE;
}

/**
 * pk_strzero:
 *
 * This function is a much safer way of doing "if (strlen (text) == 0))"
 * as it does not rely on text being NULL terminated. It's also much
 * quicker as it only checks the first byte rather than scanning the whole
 * string just to verify it's not zero length.
 **/
gboolean
pk_strzero (const gchar *text)
{
	if (text == NULL) {
		return TRUE;
	}
	if (text[0] == '\0') {
		return TRUE;
	}
	return FALSE;
}

/**
 * pk_strlen:
 *
 * This function is a much safer way of doing strlen as it checks for NULL and
 * a stupidly long string.
 * This also modifies the string in place if it is over-range by inserting
 * a NULL at the max_length.
 **/
guint
pk_strlen (gchar *text, guint max_length)
{
	guint length;
	/* ITS4: ignore, not used for allocation and checked */
	length = strlen (text);
	if (length > max_length) {
		text[max_length] = '\0';
		return max_length;
	}
	return length;
}

/**
 * pk_strvalidate:
 **/
gboolean
pk_strvalidate (const gchar *text)
{
	guint i;
	guint length;

	/* ITS4: ignore, not used for allocation and checked for oversize */
	length = strlen (text);
	for (i=0; i<length; i++) {
		if (i > 1024) {
			pk_debug ("input too long!");
			return FALSE;
		}
		if (pk_strvalidate_char (text[i]) == FALSE) {
			pk_debug ("invalid char '%c' in text!", text[i]);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * pk_strsplit:
 *
 * You need to use g_strfreev on the returned value
 **/
gchar **
pk_strsplit (const gchar *id, guint parts)
{
	gchar **sections = NULL;

	if (id == NULL) {
		pk_warning ("ident is null!");
		goto out;
	}

	/* split by delimeter ';' */
	sections = g_strsplit (id, ";", 0);
	if (g_strv_length (sections) != parts) {
		pk_warning ("ident '%s' is invalid (sections=%d)", id, g_strv_length (sections));
		goto out;
	}

	/* ITS4: ignore, not used for allocation */
	if (pk_strzero (sections[0]) == TRUE) {
		/* name has to be valid */
		pk_warning ("ident first section is empty");
		goto out;
	}

	/* all okay, phew.. */
	return sections;

out:
	/* free sections and return NULL */
	if (sections != NULL) {
		g_strfreev (sections);
	}
	return NULL;
}

/**
 * pk_strequal:
 *
 * This function is a much safer way of doing strcmp as it checks for
 * stupidly long strings, and returns boolean TRUE, not zero for success
 **/
gboolean
pk_strequal (const gchar *id1, const gchar *id2)
{
	if (id1 == NULL || id2 == NULL) {
		pk_debug ("string compare invalid '%s' and '%s'", id1, id2);
		return FALSE;
	}
	return (strcmp (id1, id2) == 0);
}

/**
 * pk_strcmp_sections:
 * only compare first sections, not all the data
 **/
gboolean
pk_strcmp_sections (const gchar *id1, const gchar *id2, guint parts, guint compare)
{
	gchar **sections1;
	gchar **sections2;
	gboolean ret = FALSE;
	guint i;

	if (id1 == NULL || id2 == NULL) {
		pk_warning ("package id compare invalid '%s' and '%s'", id1, id2);
		return FALSE;
	}
	if (compare > parts) {
		pk_warning ("compare %i > parts %i", compare, parts);
		return FALSE;
	}
	if (compare == parts) {
		pk_debug ("optimise to strcmp");
		return pk_strequal (id1, id2);
	}

	/* split, NULL will be returned if error */
	sections1 = pk_strsplit (id1, parts);
	sections2 = pk_strsplit (id2, parts);

	/* check we split okay */
	if (sections1 == NULL) {
		pk_warning ("string id compare sections1 invalid '%s'", id1);
		goto out;
	}
	if (sections2 == NULL) {
		pk_warning ("string id compare sections2 invalid '%s'", id2);
		goto out;
	}

	/* only compare preceeding sections */
	for (i=0; i<compare; i++) {
		if (pk_strequal (sections1[i], sections2[i]) == FALSE) {
			goto out;
		}
	}
	ret = TRUE;

out:
	g_strfreev (sections1);
	g_strfreev (sections2);
	return ret;
}

/**
 * pk_strpad:
 * @data: the input string
 * @length: the desired length of the output string, with padding
 **/
gchar *
pk_strpad (const gchar *data, guint length)
{
	gint size;
	gchar *text;
	gchar *padding;

	if (data == NULL) {
		return g_strnfill (length, ' ');
	}

	/* ITS4: ignore, only used for formatting */
	size = (length - strlen(data));
	if (size <= 0) {
		return g_strdup (data);
	}

	padding = g_strnfill (size, ' ');
	text = g_strdup_printf ("%s%s", data, padding);
	g_free (padding);
	return text;
}

/**
 * pk_strpad_extra:
 * @data: the input string
 * @length: the desired length of the output string, with padding
 * @extra: if we are running with a deficit, we might have a positive offset
 **/
gchar *
pk_strpad_extra (const gchar *data, guint length, guint *extra)
{
	gint size;
	gchar *text;

	/* can we just do the simple version? */
	if (data == NULL || extra == NULL) {
		return pk_strpad (data, length);
	}

	/* work out what we want to do */
	size = length - *extra;

	if (size < 0) {
		size = 0;
	}

	/* do the padding */
	text = pk_strpad (data, size);

	/* ITS4: ignore, we know pk_strpad is null terminated */
	*extra = strlen (text) - size;

	return text;
}

/**
 * pk_strbuild_va:
 **/
gchar *
pk_strbuild_va (const gchar *first_element, va_list *args)
{
	const gchar *element;
	GString *string;

	/* shortcut */
	if (pk_strzero (first_element) == TRUE) {
		return NULL;
	}

	/* set the first entry and a space */
	string = g_string_new (first_element);
	g_string_append_c (string, ' ');

	/* do all elements */
	while (TRUE) {
		element = va_arg (*args, const gchar *);

		/* are we at the end? Is this safe? */
		if (element == NULL) {
			break;
		}

		/* Ignore empty elements */
		if (*element == '\0') {
			continue;
		}

		g_string_append (string, element);
		g_string_append_c (string, ' ');
	}

	/* remove last char */
	g_string_set_size (string, string->len - 1);

	return g_string_free (string, FALSE);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef PK_BUILD_TESTS
#include <libselftest.h>

static gchar *
pk_strbuild_test (const gchar *first_element, ...)
{
	va_list args;
	gchar *text;

	/* get the argument list */
	va_start (args, first_element);
	text = pk_strbuild_va (first_element, &args);
	va_end (args);

	return text;
}

void
libst_common (LibSelfTest *test)
{
	gboolean ret;
	gchar **array;
	gchar *text_safe;
	const gchar *temp;
	guint length;
	gint value;
	guint uvalue;

	if (libst_start (test, "PkCommon", CLASS_AUTO) == FALSE) {
		return;
	}

	/************************************************************
	 ****************        build var args        **************
	 ************************************************************/
	libst_title (test, "build_va NULL");
	text_safe = pk_strbuild_test (NULL);
	if (text_safe == NULL) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "build_va blank");
	text_safe = pk_strbuild_test ("", NULL);
	if (text_safe == NULL) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "incorrect ret '%s'", text_safe);
	}

	/************************************************************/
	libst_title (test, "build_va single");
	text_safe = pk_strbuild_test ("richard", NULL);
	if (pk_strequal (text_safe, "richard") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "incorrect ret '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "build_va double");
	text_safe = pk_strbuild_test ("richard", "hughes", NULL);
	if (pk_strequal (text_safe, "richard hughes") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "incorrect ret '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "build_va double with space");
	text_safe = pk_strbuild_test ("richard", "", "hughes", NULL);
	if (pk_strequal (text_safe, "richard hughes") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "incorrect ret '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "build_va triple");
	text_safe = pk_strbuild_test ("richard", "phillip", "hughes", NULL);
	if (pk_strequal (text_safe, "richard phillip hughes") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "incorrect ret '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************
	 ****************        validate text         **************
	 ************************************************************/
	libst_title (test, "validate correct char 1");
	ret = pk_strvalidate_char ('a');
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "validate correct char 2");
	ret = pk_strvalidate_char ('~');
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "validate incorrect char");
	ret = pk_strvalidate_char ('$');
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "validate incorrect text");
	ret = pk_strvalidate ("richard$hughes");
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "validate correct text");
	ret = pk_strvalidate ("richardhughes");
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************
	 ****************          Zero            ******************
	 ************************************************************/
	temp = NULL;
	libst_title (test, "test strzero (null)");
	ret = pk_strzero (NULL);
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed null");
	}

	/************************************************************/
	libst_title (test, "test strzero (null first char)");
	ret = pk_strzero ("");
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed null");
	}

	/************************************************************/
	libst_title (test, "test strzero (long string)");
	ret = pk_strzero ("Richard");
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "zero length word!");
	}

	/************************************************************
	 ****************          splitting         ****************
	 ************************************************************/
	libst_title (test, "test pass 1");
	array = pk_strsplit ("foo", 1);
	if (array != NULL &&
	    pk_strequal (array[0], "foo") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "got %s", array[0]);
	}
	g_strfreev (array);

	/************************************************************/
	libst_title (test, "test pass 2");
	array = pk_strsplit ("foo;moo", 2);
	if (array != NULL &&
	    pk_strequal (array[0], "foo") == TRUE &&
	    pk_strequal (array[1], "moo") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "got %s, %s", array[0], array[1]);
	}
	g_strfreev (array);

	/************************************************************/
	libst_title (test, "test pass 3");
	array = pk_strsplit ("foo;moo;bar", 3);
	if (array != NULL &&
	    pk_strequal (array[0], "foo") == TRUE &&
	    pk_strequal (array[1], "moo") == TRUE &&
	    pk_strequal (array[2], "bar") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "got %s, %s, %s, %s", array[0], array[1], array[2], array[3]);
	}
	g_strfreev (array);

	/************************************************************/
	libst_title (test, "test on real packageid");
	array = pk_strsplit ("kde-i18n-csb;4:3.5.8~pre20071001-0ubuntu1;all;", 4);
	if (array != NULL &&
	    pk_strequal (array[0], "kde-i18n-csb") == TRUE &&
	    pk_strequal (array[1], "4:3.5.8~pre20071001-0ubuntu1") == TRUE &&
	    pk_strequal (array[2], "all") == TRUE &&
	    pk_strequal (array[3], "") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "got %s, %s, %s, %s", array[0], array[1], array[2], array[3]);
	}
	g_strfreev (array);

	/************************************************************/
	libst_title (test, "test on short packageid");
	array = pk_strsplit ("kde-i18n-csb;4:3.5.8~pre20071001-0ubuntu1;;", 4);
	if (array != NULL &&
	    pk_strequal (array[0], "kde-i18n-csb") == TRUE &&
	    pk_strequal (array[1], "4:3.5.8~pre20071001-0ubuntu1") == TRUE &&
	    pk_strequal (array[2], "") == TRUE &&
	    pk_strequal (array[3], "") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "got %s, %s, %s, %s", array[0], array[1], array[2], array[3]);
	}
	g_strfreev (array);

	/************************************************************/
	libst_title (test, "test fail under");
	array = pk_strsplit ("foo;moo", 1);
	if (array == NULL) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "test fail over");
	array = pk_strsplit ("foo;moo", 3);
	if (array == NULL) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "test fail missing first");
	array = pk_strsplit (";moo", 2);
	if (array == NULL) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "id strcmp pass");
	ret = pk_strequal ("moo;0.0.1;i386;fedora", "moo;0.0.1;i386;fedora");
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "id strcmp fail");
	ret = pk_strequal ("moo;0.0.1;i386;fedora", "moo;0.0.2;i386;fedora");
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	libst_title (test, "id equal pass (same)");
	ret = pk_strcmp_sections ("moo;0.0.1;i386;fedora", "moo;0.0.1;i386;fedora", 4, 3);
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	libst_title (test, "id equal pass (parts==match)");
	ret = pk_strcmp_sections ("moo;0.0.1;i386;fedora", "moo;0.0.1;i386;fedora", 4, 4);
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	libst_title (test, "id equal pass (different)");
	ret = pk_strcmp_sections ("moo;0.0.1;i386;fedora", "moo;0.0.1;i386;data", 4, 3);
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	libst_title (test, "id equal fail1");
	ret = pk_strcmp_sections ("moo;0.0.1;i386;fedora", "moo;0.0.2;x64;fedora", 4, 3);
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	libst_title (test, "id equal fail2");
	ret = pk_strcmp_sections ("moo;0.0.1;i386;fedora", "gnome;0.0.2;i386;fedora", 4, 3);
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	libst_title (test, "id equal fail3");
	ret = pk_strcmp_sections ("moo;0.0.1;i386;fedora", "moo;0.0.3;i386;fedora", 4, 3);
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	libst_title (test, "id equal fail (match too high)");
	ret = pk_strcmp_sections ("moo;0.0.1;i386;fedora", "moo;0.0.3;i386;fedora", 4, 5);
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************
	 ****************          FILTERS         ******************
	 ************************************************************/
	temp = NULL;
	libst_title (test, "test a fail filter (null)");
	ret = pk_filter_check (temp);
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "passed the filter '%s'", temp);
	}

	/************************************************************/
	temp = "";
	libst_title (test, "test a fail filter ()");
	ret = pk_filter_check (temp);
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "passed the filter '%s'", temp);
	}

	/************************************************************/
	temp = ";";
	libst_title (test, "test a fail filter (;)");
	ret = pk_filter_check (temp);
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "passed the filter '%s'", temp);
	}

	/************************************************************/
	temp = "moo";
	libst_title (test, "test a fail filter (invalid)");
	ret = pk_filter_check (temp);
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "passed the filter '%s'", temp);
	}

	/************************************************************/
	temp = "moo;foo";
	libst_title (test, "test a fail filter (invalid, multiple)");
	ret = pk_filter_check (temp);
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "passed the filter '%s'", temp);
	}

	/************************************************************/
	temp = "gui;;";
	libst_title (test, "test a fail filter (valid then zero length)");
	ret = pk_filter_check (temp);
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "passed the filter '%s'", temp);
	}

	/************************************************************/
	temp = "none";
	libst_title (test, "test a pass filter (none)");
	ret = pk_filter_check (temp);
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the filter '%s'", temp);
	}

	/************************************************************/
	temp = "gui";
	libst_title (test, "test a pass filter (single)");
	ret = pk_filter_check (temp);
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the filter '%s'", temp);
	}

	/************************************************************/
	temp = "devel;~gui";
	libst_title (test, "test a pass filter (multiple)");
	ret = pk_filter_check (temp);
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the filter '%s'", temp);
	}

	/************************************************************/
	temp = "~gui;~installed";
	libst_title (test, "test a pass filter (multiple2)");
	ret = pk_filter_check (temp);
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the filter '%s'", temp);
	}

	/************************************************************
	 ****************          strlen          ******************
	 ************************************************************/
	libst_title (test, "strlen bigger");
	text_safe = g_strdup ("123456789");
	length = pk_strlen (text_safe, 20);
	if (length == 9 && pk_strequal (text_safe, "123456789") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the strlen %i,'%s'", length, text_safe);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "strlen smaller");
	text_safe = g_strdup ("123456789");
	length = pk_strlen (text_safe, 5);
	if (length == 5 && pk_strequal (text_safe, "12345") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the strlen %i,'%s'", length, text_safe);
	}
	g_free (text_safe);

	/************************************************************
	 ****************         Padding          ******************
	 ************************************************************/
	libst_title (test, "pad smaller");
	text_safe = pk_strpad ("richard", 10);
	if (pk_strequal (text_safe, "richard   ") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the padd '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "pad NULL");
	text_safe = pk_strpad (NULL, 10);
	if (pk_strequal (text_safe, "          ") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the padd '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "pad nothing");
	text_safe = pk_strpad ("", 10);
	if (pk_strequal (text_safe, "          ") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the padd '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "pad over");
	text_safe = pk_strpad ("richardhughes", 10);
	if (pk_strequal (text_safe, "richardhughes") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the padd '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "pad zero");
	text_safe = pk_strpad ("rich", 0);
	if (pk_strequal (text_safe, "rich") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the padd '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************
	 ****************         Padding          ******************
	 ************************************************************/
	libst_title (test, "pad smaller, no extra");
	length = 0;
	text_safe = pk_strpad_extra ("richard", 10, &length);
	if (length == 0 && pk_strequal (text_safe, "richard   ") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the padd '%s', extra %i", text_safe, length);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "pad over, no extra");
	length = 0;
	text_safe = pk_strpad_extra ("richardhughes", 10, &length);
	if (length == 3 && pk_strequal (text_safe, "richardhughes") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the padd '%s', extra %i", text_safe, length);
	}

	/************************************************************/
	g_free (text_safe);
	libst_title (test, "pad smaller, 1 extra");
	length = 1;
	text_safe = pk_strpad_extra ("richard", 10, &length);
	if (length == 0 && pk_strequal (text_safe, "richard  ") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the padd '%s', extra %i", text_safe, length);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "pad over, 1 extra");
	length = 1;
	text_safe = pk_strpad_extra ("richardhughes", 10, &length);
	if (length == 4 && pk_strequal (text_safe, "richardhughes") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the padd '%s', extra %i", text_safe, length);
	}

	/************************************************************
	 ****************       REPLACE CHARS      ******************
	 ************************************************************/
	libst_title (test, "test replace unsafe (okay)");
	text_safe = pk_strsafe ("Richard Hughes");
	if (pk_strequal (text_safe, "Richard Hughes") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the replace unsafe '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "test replace unsafe (one invalid)");
	text_safe = pk_strsafe ("Richard\tHughes");
	if (pk_strequal (text_safe, "Richard Hughes") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the replace unsafe '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "test replace unsafe (one invalid 2)");
	text_safe = pk_strsafe ("Richard\"Hughes\"");
	if (pk_strequal (text_safe, "Richard Hughes ") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the replace unsafe '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************/
	libst_title (test, "test replace unsafe (multiple invalid)");
	text_safe = pk_strsafe ("'Richard\"Hughes\"");
	if (pk_strequal (text_safe, " Richard Hughes ") == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "failed the replace unsafe '%s'", text_safe);
	}
	g_free (text_safe);

	/************************************************************
	 **************       Check for numbers      ****************
	 ************************************************************/
	libst_title (test, "check number valid");
	ret = pk_strnumber ("123");
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "check number valid");
	ret = pk_strnumber ("-123");
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "check number zero");
	ret = pk_strnumber ("0");
	if (ret == TRUE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "check number oversize");
	ret = pk_strnumber ("123456891234");
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "check number NULL");
	ret = pk_strnumber (NULL);
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "check number blank");
	ret = pk_strnumber ("");
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "check number not negative");
	ret = pk_strnumber ("503-");
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "check number positive");
	ret = pk_strnumber ("+503");
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "check number random chars");
	ret = pk_strnumber ("dave");
	if (ret == FALSE) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************
	 **************        Convert numbers       ****************
	 ************************************************************/
	libst_title (test, "convert valid number");
	ret = pk_strtoint ("234", &value);
	if (ret == TRUE && value == 234) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "value is %i", value);
	}

	/************************************************************/
	libst_title (test, "convert negative valid number");
	ret = pk_strtoint ("-234", &value);
	if (ret == TRUE && value == -234) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "value is %i", value);
	}

	/************************************************************/
	libst_title (test, "don't convert invalid number");
	ret = pk_strtoint ("dave", &value);
	if (ret == FALSE && value == 0) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "value is %i", value);
	}

	/************************************************************/
	libst_title (test, "convert valid uint number");
	ret = pk_strtouint ("234", &uvalue);
	if (ret == TRUE && uvalue == 234) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "value is %i", uvalue);
	}

	/************************************************************/
	libst_title (test, "convert invalid uint number");
	ret = pk_strtouint ("-234", &uvalue);
	if (ret == FALSE && uvalue == 0) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, "value is %i", uvalue);
	}

	libst_end (test);
}
#endif

