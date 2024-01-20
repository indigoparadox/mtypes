
#ifndef MPLUG_H
#define MPLUG_H

typedef MERROR_RETVAL (*mplug_proc_t)( void* data, size_t data_sz );

#ifdef RETROFLAT_OS_WIN
typedef HMODULE mplug_mod_t;
#  define MPLUG_EXPORT extern __declspec( dllexport )
#else
typedef void* mplug_mod_t;
#  define MPLUG_EXPORT
#endif /* RETROFLAT_OS_WIN */

MERROR_RETVAL mplug_load(
   const char* plugin_path, mplug_mod_t* p_mod_exe );

MERROR_RETVAL mplug_call(
   mplug_mod_t mod_exe, const char* proc_name, void* data, size_t data_sz );

void mplug_free( mplug_mod_t mod_exe );

#ifdef MPLUG_C

#ifdef RETROFLAT_OS_WIN
#else
#  include <dlfcn.h>
#endif /* RETROFLAT_OS_WIN */

MERROR_RETVAL mplug_load(
   const char* plugin_basename, mplug_mod_t* p_mod_exe
) {
   MERROR_RETVAL retval = MERROR_OK;
   char plugin_path[MAUG_PATH_SZ_MAX + 1];
#ifdef RETROFLAT_OS_WIN
   size_t i = 0;
#endif /* RETROFLAT_OS_WIN */

   memset( plugin_path, '\0', MAUG_PATH_SZ_MAX + 1 );

#if defined( RETROFLAT_OS_UNIX )
   maug_snprintf( plugin_path, MAUG_PATH_SZ_MAX, "%s.so", plugin_basename );
   *p_mod_exe = dlopen( plugin_path, RTLD_LAZY );
#elif defined( RETROFLAT_OS_WIN )
   maug_snprintf(
      plugin_path, MAUG_PATH_SZ_MAX, "%s.dll", plugin_basename );
   for( i = 0 ; MAUG_PATH_SZ_MAX > i ; i++ ) {
      if( '/' == plugin_path[i] ) {
         plugin_path[i] = '\\';
      }
   }
   *p_mod_exe = LoadLibrary( plugin_path );
#else
#  error "dlopen undefined!"
#endif /* RETROFLAT_OS_UNIX */

   if( NULL == *p_mod_exe ) {
      error_printf( "unable to load module: %s", plugin_path );
      retval = MERROR_FILE;
   }

   return retval;
}

MERROR_RETVAL mplug_call(
   mplug_mod_t mod_exe, const char* proc_name, void* data, size_t data_sz
) {
   MERROR_RETVAL retval = MERROR_OK;
   mplug_proc_t plugin_proc = (mplug_proc_t)NULL;
#ifdef RETROFLAT_OS_WIN
   char proc_name_ex[MAUG_PATH_SZ_MAX + 1];
#endif /* RETROFLAT_OS_WIN */

#ifdef RETROFLAT_OS_UNIX
   plugin_proc = dlsym( mod_exe, proc_name );
#elif defined( RETROFLAT_OS_WIN )
   memset( proc_name_ex, '\0', MAUG_PATH_SZ_MAX + 1 );

   /* Append a _ to the proc_name to match calling convention name scheme. */
   maug_snprintf( proc_name_ex, MAUG_PATH_SZ_MAX, "%s_", proc_name );
   plugin_proc = (mplug_proc_t)GetProcAddress( mod_exe, proc_name_ex );
#else
#  error "dlsym undefined!"
#endif

   if( (mplug_proc_t)NULL == plugin_proc ) {
      error_printf( "unable to load proc: %s", proc_name );
      retval = MERROR_FILE;
      goto cleanup;
   }

   retval = plugin_proc( data, data_sz );

cleanup:
   return retval;
}

void mplug_free( mplug_mod_t mod_exe ) {
#ifdef RETROFLAT_OS_UNIX
   dlclose( mod_exe );
#elif defined( RETROFLAT_OS_WIN )
   FreeLibrary( mod_exe );
#else
#  error "dlclose undefined!"
#endif /* RETROFLAT_OS_UNIX || RETROFLAT_OS_WIN */
}

#endif /* MPLUG_C */

#endif /* !MPLUG_H */

