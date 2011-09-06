/* cache vips operations
 */

/*

    This file is part of VIPS.
    
    VIPS is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

/*

    These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define VIPS_DEBUG
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /*HAVE_UNISTD_H*/
#include <ctype.h>

#include <vips/vips.h>
#include <vips/internal.h>
#include <vips/debug.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

/* An object in the cache. Keep a set of these in our object hash table.
 */
typedef struct _VipsCacheItem {
	VipsObject *object;

	/* Cache hash code here.
	 */
	unsigned int hash;
	gboolean found_hash;
} VipsCacheItem;

static GHashTable *vips_object_cache = NULL;

static unsigned int
vips_value_hash( GValue *value )
{
	switch( G_VALUE_TYPE( value ) ) { 
	case G_TYPE_BOOLEAN:
		return( (unsigned int) g_value_get_boolean( value ) );
	case G_TYPE_CHAR:
		return( (unsigned int) g_value_get_char( value ) );
	case G_TYPE_UCHAR:
		return( (unsigned int) g_value_get_uchar( value ) );
	case G_TYPE_INT:
		return( (unsigned int) g_value_get_int( value ) );
	case G_TYPE_UINT:
		return( (unsigned int) g_value_get_uint( value ) );
	case G_TYPE_LONG:
		return( (unsigned int) g_value_get_long( value ) );
	case G_TYPE_ULONG:
		return( (unsigned int) g_value_get_ulong( value ) );
	case G_TYPE_ENUM:
		return( (unsigned int) g_value_get_enum( value ) );
	case G_TYPE_FLAGS:
		return( (unsigned int) g_value_get_flags( value ) );
	case G_TYPE_UINT64:
		return( (unsigned int) g_value_get_uint64( value ) );

	case G_TYPE_INT64:
	{
		gint64 i = g_value_get_int64( value );

		return( g_int64_hash( &i ) );
	}
	case G_TYPE_FLOAT:
	{
		float f = g_value_get_float( value );

		return( *((unsigned int *) &f) );
	}
	case G_TYPE_DOUBLE:
	{
		double d = g_value_get_double( value );

		return( g_double_hash( &d ) );
	}
	case G_TYPE_STRING:
	{
		const char *s = g_value_get_string( value );

		return( g_str_hash( s ) );
	}
	case G_TYPE_BOXED:
	{
		void *p = g_value_get_boxed( value );

		return( g_direct_hash( p ) );
	}
	case G_TYPE_POINTER:
	{
		void *p = g_value_get_pointer( value );

		return( g_direct_hash( p ) );
	}
	case G_TYPE_OBJECT:
	{
		void *p = g_value_get_object( value );

		return( g_direct_hash( p ) );
	}

	default:
	{
		/* Fallback: convert to a string and hash that. This is very
		 * slow, print a warning if we use it so we can add another
		 * case.
		 */
		char *s;
		unsigned int hash;

		s = g_strdup_value_contents( value ); 
		hash = g_str_hash( s );
		printf( "vips_value_hash: no case for %s\n", s );
		g_free( s );

		return( hash );
	}
	}
}

static gboolean 
vips_value_equal( GValue *v1, GValue *v2 )
{
	GType t1 = G_VALUE_TYPE( v1 );
	GType t2 = G_VALUE_TYPE( v2 );

	if( t1 != t2 )
		return( FALSE );

	switch( t1 ) { 
	case G_TYPE_BOOLEAN:
		return( g_value_get_boolean( v1 ) == 
			g_value_get_boolean( v2 ) );
	case G_TYPE_CHAR:
		return( g_value_get_char( v1 ) ==
			g_value_get_char( v2 ) );
	case G_TYPE_UCHAR:
		return( g_value_get_uchar( v1 ) ==
			g_value_get_uchar( v2 ) );
	case G_TYPE_INT:
		return( g_value_get_int( v1 ) ==
			g_value_get_int( v2 ) );
	case G_TYPE_UINT:
		return( g_value_get_uint( v1 ) ==
			g_value_get_uint( v2 ) );
	case G_TYPE_LONG:
		return( g_value_get_long( v1 ) ==
			g_value_get_long( v2 ) );
	case G_TYPE_ULONG:
		return( g_value_get_ulong( v1 ) ==
			g_value_get_ulong( v2 ) );
	case G_TYPE_ENUM:
		return( g_value_get_enum( v1 ) ==
			g_value_get_enum( v2 ) );
	case G_TYPE_FLAGS:
		return( g_value_get_flags( v1 ) ==
			g_value_get_flags( v2 ) );
	case G_TYPE_UINT64:
		return( g_value_get_uint64( v1 ) ==
			g_value_get_uint64( v2 ) );
	case G_TYPE_INT64:
		return( g_value_get_int64( v1 ) ==
			g_value_get_int64( v2 ) );
	case G_TYPE_FLOAT:
		return( g_value_get_float( v1 ) ==
			g_value_get_float( v2 ) );
	case G_TYPE_DOUBLE:
		return( g_value_get_double( v1 ) ==
			g_value_get_double( v2 ) );
	case G_TYPE_STRING:
		return( strcmp( g_value_get_string( v1 ),
			g_value_get_string( v2 ) ) == 0 );
	case G_TYPE_BOXED:
		return( g_value_get_boxed( v1 ) ==
			g_value_get_boxed( v2 ) );
	case G_TYPE_POINTER:
		return( g_value_get_pointer( v1 ) ==
			g_value_get_pointer( v2 ) );
	case G_TYPE_OBJECT:
		return( g_value_get_object( v1 ) ==
			g_value_get_object( v2 ) );

	default:
	{
		/* Fallback: convert to a string and hash that. This is very
		 * slow, print a warning if we use it so we can add another
		 * case.
		 */
		char *s1;
		char *s2;
		gboolean equal;

		s1 = g_strdup_value_contents( v1 ); 
		s2 = g_strdup_value_contents( v2 ); 
		equal = strcmp( s1, s2 ) == 0;
		g_free( s1 );
		g_free( s2 );

		return( equal );
	}
	}
}

static void *
vips_cache_hash_arg( VipsObject *object,
	GParamSpec *pspec,
	VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance,
	void *a, void *b )
{
	unsigned int *hash = (unsigned int *) a;

	if( (argument_class->flags & VIPS_ARGUMENT_CONSTRUCT) &&
		argument_instance->assigned ) {
		GType type = G_PARAM_SPEC_VALUE_TYPE( pspec );
		GValue value = { 0, };

		g_value_init( &value, type );
		g_object_get_property( G_OBJECT( object ), 
			g_param_spec_get_name( pspec ), &value ); 
		*hash = (*hash << 1) ^ vips_value_hash( &value );
		g_value_unset( &value );
	}

	return( NULL );
}

/* Find a hash from the input arguments.
 */
static unsigned int
vips_cache_hash( VipsCacheItem *item )
{
	if( !item->found_hash ) {
		unsigned int hash;

		hash = 0;
		(void) vips_argument_map( item->object,
			vips_cache_hash_arg, &hash, NULL );
		item->hash = hash;

		item->found_hash = TRUE;
	}

	return( item->hash );
}

static void *
vips_cache_equal_arg( VipsObject *object,
	GParamSpec *pspec,
	VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance,
	void *a, void *b )
{
	VipsObject *other = (VipsObject *) a;

	if( (argument_class->flags & VIPS_ARGUMENT_CONSTRUCT) &&
		argument_instance->assigned ) {
		const char *name = g_param_spec_get_name( pspec );
		GType type = G_PARAM_SPEC_VALUE_TYPE( pspec );
		GValue v1 = { 0, };
		GValue v2 = { 0, };

		gboolean equal;

		g_value_init( &v1, type );
		g_value_init( &v2, type );
		g_object_get_property( G_OBJECT( object ), name, &v1 ); 
		g_object_get_property( G_OBJECT( other ), name, &v2 ); 
		equal = vips_value_equal( &v1, &v2 );
		g_value_unset( &v1 );
		g_value_unset( &v2 );

		if( !equal )
			return( object );
	}

	return( NULL );
}

/* Are two cache items equal.
 */
static gboolean 
vips_cache_equal( VipsCacheItem *a, VipsCacheItem *b )
{
	if( G_OBJECT_TYPE( a->object ) == G_OBJECT_TYPE( b->object ) &&
		vips_cache_hash( a ) == vips_cache_hash( b ) &&
		!vips_argument_map( a->object, 
			vips_cache_equal_arg, b->object, NULL ) )
		return( TRUE );

	return( FALSE );
}

static void
vips_cache_remove( void *data, GObject *object )
{
	VIPS_DEBUG_MSG( "vips_cache_remove: removing %p\n", object );

	if( !g_hash_table_remove( vips_object_cache, object ) )
		g_assert( 0 );
}

/* Look up an object in the cache. If we get a hit, unref the new one, ref the
 * old one and return that. If we miss, add this object and a weakref so we
 * will drop it when it's unreffed.
 */
VipsObject *
vips_cache_lookup( VipsObject *object )
{
	VipsObject *hit;

	if( !vips_object_cache ) 
		vips_object_cache = g_hash_table_new( 
			(GHashFunc) vips_cache_hash, 
			(GEqualFunc) vips_cache_equal );

	if( (hit = g_hash_table_lookup( vips_object_cache, object )) ) {
		VIPS_DEBUG_MSG( "vips_cache_lookup: hit for %p\n", hit );

		g_object_unref( object );
		g_object_ref( hit );

		return( hit );
	} 
	else {
		VIPS_DEBUG_MSG( "vips_cache_lookup: adding %p\n", object );

		g_hash_table_insert( vips_object_cache, object, object );
		g_object_weak_ref( G_OBJECT( object ), 
			vips_cache_remove, NULL );

		return( object );
	}
}
