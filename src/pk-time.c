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
#include <time.h>
#include <errno.h>
#include <signal.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <sys/wait.h>
#include <fcntl.h>

#include <glib/gi18n.h>

#include "egg-debug.h"
#include "pk-time.h"
#include "pk-marshal.h"

static void     pk_time_class_init	(PkTimeClass *klass);
static void     pk_time_init		(PkTime      *time);
static void     pk_time_finalize	(GObject     *object);

#define PK_TIME_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PK_TYPE_TIME, PkTimePrivate))
#define PK_TIME_AVERAGE_DEFAULT_MIN		4     /* samples */
#define PK_TIME_AVERAGE_DEFAULT_MAX		10    /* samples */
#define PK_TIME_VALUE_DEFAULT_MIN		5     /* s */
#define PK_TIME_VALUE_DEFAULT_MAX		60*60 /* s */

struct PkTimePrivate
{
	guint			 time_offset; /* ms */
	guint			 last_percentage;
	guint			 average_min;
	guint			 average_max;
	guint			 value_min;
	guint			 value_max;
	GPtrArray		*array;
	GTimer			*timer;
};

typedef struct {
	guint			 percentage;
	guint			 time;
} PkTimeItem;

G_DEFINE_TYPE (PkTime, pk_time, G_TYPE_OBJECT)

/**
 * pk_time_set_average_limits:
 * @time: This class instance
 * @average_min: the smallest number of samples we will try to average
 * @average_max: the largest number of past samples we will try to average
 *
 * Return value: if we set the average limits correctly
 **/
gboolean
pk_time_set_average_limits (PkTime *time, guint average_min, guint average_max)
{
	g_return_val_if_fail (PK_IS_TIME (time), FALSE);
	time->priv->average_min = average_min;
	time->priv->average_max = average_max;
	return TRUE;
}

/**
 * pk_time_set_value_limits:
 * @time: This class instance
 * @average_min: the smallest value that is acceptable for time (in seconds)
 * @average_max: the largest value that is acceptable for time (in seconds)
 *
 * Return value: if we set the value limits correctly
 **/
gboolean
pk_time_set_value_limits (PkTime *time, guint value_min, guint value_max)
{
	g_return_val_if_fail (PK_IS_TIME (time), FALSE);
	time->priv->value_min = value_min;
	time->priv->value_max = value_max;
	return TRUE;
}

/**
 * pk_time_get_elapsed:
 *
 * Returns time running in ms
 **/
guint
pk_time_get_elapsed (PkTime *time)
{
	gdouble elapsed;

	g_return_val_if_fail (PK_IS_TIME (time), 0);

	elapsed = g_timer_elapsed (time->priv->timer, NULL);
	elapsed *= 1000;
	elapsed += time->priv->time_offset;

	return (guint) elapsed;
}

/**
 * pk_time_get_gradient:
 **/
static gfloat
pk_time_get_gradient (PkTimeItem *item1, PkTimeItem *item2)
{
	gfloat dy;
	gfloat dx;
	dy = (gfloat) (item1->percentage - item2->percentage);
	dx = (gfloat) (item1->time - item2->time);
	return dy/dx;
}

/**
 * pk_time_get_remaining:
 *
 * Returns time in seconds
 **/
guint
pk_time_get_remaining (PkTime *time)
{
	guint i;
	guint averaged = 0;
	guint length;
	gfloat grad;
	gfloat grad_ave = 0.0f;
	gfloat estimated;
	guint percentage_left;
	guint elapsed;
	PkTimeItem *item;
	PkTimeItem *item_prev;

	g_return_val_if_fail (PK_IS_TIME (time), 0);

	length = time->priv->array->len;
	if (length < 2) {
		egg_debug ("array too small");
		return 0;
	}

	/* get as many as we can */
	for (i=length-1; i>0; i--) {
		item_prev = g_ptr_array_index (time->priv->array, i-1);
		item = g_ptr_array_index (time->priv->array, i);
		grad = pk_time_get_gradient (item, item_prev);
		egg_debug ("gradient between %i/%i=%f", i-1, i, grad);
		if (grad < 0.00001 || grad > 100) {
			egg_debug ("ignoring gradient: %f", grad);
		} else {
			grad_ave += grad;
			averaged++;
			if (averaged > time->priv->average_max) {
				break;
			}
		}
	}

	egg_debug ("averaged %i points", averaged);
	if (averaged < time->priv->average_min) {
		egg_debug ("not enough samples for accurate time: %i", averaged);
		return 0;
	}

	/* normalise to the number of samples */
	grad_ave /= averaged;
	egg_debug ("grad_ave=%f", grad_ave);

	/* just for debugging */
	elapsed = pk_time_get_elapsed (time);
	egg_debug ("elapsed=%i", elapsed);

	/* 100 percent to be complete */
	item = g_ptr_array_index (time->priv->array, length - 1);
	percentage_left = 100 - item->percentage;
	egg_debug ("percentage_left=%i", percentage_left);
	estimated = (gfloat) percentage_left / grad_ave;

	/* turn to ms */
	estimated /= 1000;
	egg_debug ("estimated=%f seconds", estimated);

	if (estimated < time->priv->value_min) {
		estimated = 0;
	} else if (estimated > time->priv->value_max) {
		estimated = 0;
	}
	return (guint) estimated;
}

/**
 * pk_time_add_data:
 **/
gboolean
pk_time_add_data (PkTime *time, guint percentage)
{
	PkTimeItem *item;
	guint elapsed;

	g_return_val_if_fail (PK_IS_TIME (time), FALSE);

	/* check we are going up */
	if (percentage < time->priv->last_percentage) {
		egg_warning ("percentage cannot go down!");
		return FALSE;
	}
	time->priv->last_percentage = percentage;

	/* get runtime in ms */
	elapsed = pk_time_get_elapsed (time);

	egg_debug ("adding %i at %i (ms)", percentage, elapsed);

	/* create a new object and add to the array */
	item = g_new0 (PkTimeItem, 1);
	item->time = elapsed;
	item->percentage = percentage;
	g_ptr_array_add (time->priv->array, item);

	return TRUE;
}

/**
 * pk_time_free_data:
 **/
static gboolean
pk_time_free_data (PkTime *time)
{
	guint i;
	guint length;
	gpointer mem;

	g_return_val_if_fail (PK_IS_TIME (time), FALSE);

	length = time->priv->array->len;
	for (i=0; i<length; i++) {
		mem = g_ptr_array_index (time->priv->array, 0);
		g_ptr_array_remove_index (time->priv->array, 0);
		g_free (mem);
	}
	return TRUE;
}

/**
 * pk_time_reset:
 **/
gboolean
pk_time_reset (PkTime *time)
{
	g_return_val_if_fail (PK_IS_TIME (time), FALSE);

	time->priv->time_offset = 0;
	time->priv->last_percentage = 0;
	time->priv->average_min = PK_TIME_AVERAGE_DEFAULT_MIN;
	time->priv->average_max = PK_TIME_AVERAGE_DEFAULT_MAX;
	time->priv->value_min = PK_TIME_VALUE_DEFAULT_MIN;
	time->priv->value_max = PK_TIME_VALUE_DEFAULT_MAX;
	g_timer_reset (time->priv->timer);
	pk_time_free_data (time);

	return TRUE;
}

/**
 * pk_time_class_init:
 * @klass: The PkTimeClass
 **/
static void
pk_time_class_init (PkTimeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = pk_time_finalize;
	g_type_class_add_private (klass, sizeof (PkTimePrivate));
}

/**
 * pk_time_init:
 * @time: This class instance
 **/
static void
pk_time_init (PkTime *time)
{
	time->priv = PK_TIME_GET_PRIVATE (time);
	time->priv->array = g_ptr_array_new ();
	time->priv->timer = g_timer_new ();
	pk_time_reset (time);
}

/**
 * pk_time_finalize:
 * @object: The object to finalize
 **/
static void
pk_time_finalize (GObject *object)
{
	PkTime *time;

	g_return_if_fail (PK_IS_TIME (object));

	time = PK_TIME (object);
	g_return_if_fail (time->priv != NULL);
	g_ptr_array_foreach (time->priv->array, (GFunc) g_free, NULL);
	g_ptr_array_free (time->priv->array, TRUE);
	g_timer_destroy (time->priv->timer);

	G_OBJECT_CLASS (pk_time_parent_class)->finalize (object);
}

/**
 * pk_time_new:
 *
 * Return value: a new PkTime object.
 **/
PkTime *
pk_time_new (void)
{
	PkTime *time;
	time = g_object_new (PK_TYPE_TIME, NULL);
	return PK_TIME (time);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
pk_time_test (EggTest *test)
{
	PkTime *time = NULL;
	gboolean ret;
	guint value;

	if (!egg_test_start (test, "PkTime"))
		return;

	/************************************************************/
	egg_test_title (test, "get PkTime object");
	time = pk_time_new ();
	if (time != NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, NULL);

	/************************************************************/
	egg_test_title (test, "get elapsed correctly at startup");
	value = pk_time_get_elapsed (time);
	if (value < 10) {
		egg_test_success (test, "elapsed at startup %i", value);
	} else {
		egg_test_failed (test, "elapsed at startup %i", value);
	}

	/************************************************************/
	egg_test_title (test, "ignore remaining correctly");
	value = pk_time_get_remaining (time);
	if (value == 0)
		egg_test_success (test, NULL);
	else {
		egg_test_failed (test, "got %i, not zero!", value);
	}

	/************************************************************/
	g_usleep (1000*1000);

	/************************************************************/
	egg_test_title (test, "get elapsed correctly");
	value = pk_time_get_elapsed (time);
	if (value > 900 && value < 1100) {
		egg_test_success (test, "elapsed ~1000ms: %i", value);
	} else {
		egg_test_failed (test, "elapsed not ~1000ms: %i", value);
	}

	/************************************************************/
	egg_test_title (test, "ignore remaining correctly when not enough entries");
	value = pk_time_get_remaining (time);
	if (value == 0)
		egg_test_success (test, NULL);
	else {
		egg_test_failed (test, "got %i, not zero!", value);
	}

	/************************************************************/
	egg_test_title (test, "make sure we can add data");
	ret = pk_time_add_data (time, 10);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, NULL);

	/************************************************************/
	egg_test_title (test, "make sure we can get remaining correctly");
	value = 20;
	while (value < 60) {
		time->priv->time_offset += 2000;
		pk_time_add_data (time, value);
		value += 10;
	}
	value = pk_time_get_remaining (time);
	if (value > 9 && value < 11)
		egg_test_success (test, NULL);
	else {
		egg_test_failed (test, "got %i", value);
	}

	/* reset */
	g_object_unref (time);
	time = pk_time_new ();

	/************************************************************/
	egg_test_title (test, "make sure we can do long times");
	value = 10;
	pk_time_add_data (time, 0);
	while (value < 60) {
		time->priv->time_offset += 4*60*1000;
		pk_time_add_data (time, value);
		value += 10;
	}
	value = pk_time_get_remaining (time);
	if (value > 1199 && value < 1201)
		egg_test_success (test, NULL);
	else {
		egg_test_failed (test, "got %i", value);
	}


	g_object_unref (time);

	egg_test_end (test);
}
#endif

