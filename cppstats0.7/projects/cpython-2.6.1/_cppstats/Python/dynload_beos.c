#include <kernel/image.h>
#include <kernel/OS.h>
#include <stdlib.h>
#include "Python.h"
#include "importdl.h"
const struct filedescr _PyImport_DynLoadFiletab[] = {
{".so", "rb", C_EXTENSION},
{"module.so", "rb", C_EXTENSION},
{0, 0}
};
#if defined(MAXPATHLEN) && !defined(_SYS_PARAM_H)
#undef MAXPATHLEN
#endif
#if defined(WITH_THREAD)
#include "pythread.h"
static PyThread_type_lock beos_dyn_lock;
#endif
static PyObject *beos_dyn_images = NULL;
static void beos_nuke_dyn( PyObject *item ) {
status_t retval;
if( item ) {
image_id id = (image_id)PyInt_AsLong( item );
retval = unload_add_on( id );
}
}
static void beos_cleanup_dyn( void ) {
if( beos_dyn_images ) {
int idx;
int list_size;
PyObject *id_list;
#if defined(WITH_THREAD)
PyThread_acquire_lock( beos_dyn_lock, 1 );
#endif
id_list = PyDict_Values( beos_dyn_images );
list_size = PyList_Size( id_list );
for( idx = 0; idx < list_size; idx++ ) {
PyObject *the_item;
the_item = PyList_GetItem( id_list, idx );
beos_nuke_dyn( the_item );
}
PyDict_Clear( beos_dyn_images );
#if defined(WITH_THREAD)
PyThread_free_lock( beos_dyn_lock );
#endif
}
}
static void beos_init_dyn( void ) {
static int32 init_count = 0;
int32 val;
val = atomic_add( &init_count, 1 );
if( beos_dyn_images == NULL && val == 0 ) {
beos_dyn_images = PyDict_New();
#if defined(WITH_THREAD)
beos_dyn_lock = PyThread_allocate_lock();
#endif
atexit( beos_cleanup_dyn );
}
}
static void beos_add_dyn( char *name, image_id id ) {
int retval;
PyObject *py_id;
if( beos_dyn_images == NULL ) {
beos_init_dyn();
}
#if defined(WITH_THREAD)
retval = PyThread_acquire_lock( beos_dyn_lock, 1 );
#endif
py_id = PyDict_GetItemString( beos_dyn_images, name );
if( py_id ) {
beos_nuke_dyn( py_id );
retval = PyDict_DelItemString( beos_dyn_images, name );
}
py_id = PyInt_FromLong( (long)id );
if( py_id ) {
retval = PyDict_SetItemString( beos_dyn_images, name, py_id );
}
#if defined(WITH_THREAD)
PyThread_release_lock( beos_dyn_lock );
#endif
}
dl_funcptr _PyImport_GetDynLoadFunc(const char *fqname, const char *shortname,
const char *pathname, FILE *fp) {
dl_funcptr p;
image_id the_id;
status_t retval;
char fullpath[PATH_MAX];
char funcname[258];
if( Py_VerboseFlag ) {
printf( "load_add_on( %s )\n", pathname );
}
if( pathname[0] != '/' ) {
(void)getcwd( fullpath, PATH_MAX );
(void)strncat( fullpath, "/", PATH_MAX );
(void)strncat( fullpath, pathname, PATH_MAX );
if( Py_VerboseFlag ) {
printf( "load_add_on( %s )\n", fullpath );
}
} else {
(void)strcpy( fullpath, pathname );
}
the_id = load_add_on( fullpath );
if( the_id < B_NO_ERROR ) {
char buff[256];
if( Py_VerboseFlag ) {
printf( "load_add_on( %s ) failed", fullpath );
}
if( the_id == B_ERROR )
PyOS_snprintf( buff, sizeof(buff),
"BeOS: Failed to load %.200s",
fullpath );
else
PyOS_snprintf( buff, sizeof(buff),
"Unknown error loading %.200s",
fullpath );
PyErr_SetString( PyExc_ImportError, buff );
return NULL;
}
PyOS_snprintf(funcname, sizeof(funcname), "init%.200s", shortname);
if( Py_VerboseFlag ) {
printf( "get_image_symbol( %s )\n", funcname );
}
retval = get_image_symbol( the_id, funcname, B_SYMBOL_TYPE_TEXT, &p );
if( retval != B_NO_ERROR || p == NULL ) {
char buff[256];
if( Py_VerboseFlag ) {
printf( "get_image_symbol( %s ) failed", funcname );
}
switch( retval ) {
case B_BAD_IMAGE_ID:
PyOS_snprintf( buff, sizeof(buff),
"can't load init function for dynamic module: "
"Invalid image ID for %.180s", fullpath );
break;
case B_BAD_INDEX:
PyOS_snprintf( buff, sizeof(buff),
"can't load init function for dynamic module: "
"Bad index for %.180s", funcname );
break;
default:
PyOS_snprintf( buff, sizeof(buff),
"can't load init function for dynamic module: "
"Unknown error looking up %.180s", funcname );
break;
}
retval = unload_add_on( the_id );
PyErr_SetString( PyExc_ImportError, buff );
return NULL;
}
beos_add_dyn( fqname, the_id );
return p;
}