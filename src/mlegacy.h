
#ifndef MLEGACY_H
#define MLEGACY_H

#ifdef ANCIENT_C
#define  UPRINTF_ANCIENT_C
#endif /* ANCIENT_C */

#if defined( PLATFORM_PALM )
#  define MAUG_OS_PALM
#elif defined( PLATFORM_WIN ) || defined( RETROFLAT_OS_WIN )
#  define MAUG_OS_WIN
#endif /* PLATFORM_* */

#if defined( DEBUG )
#include <assert.h>
#elif !defined( DOCUMENTATION )
#define assert( x )
#endif /* DEBUG */

#endif /* !MLEGACY_H */

