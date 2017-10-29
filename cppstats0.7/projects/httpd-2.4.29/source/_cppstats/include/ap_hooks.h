#if !defined(AP_HOOKS_H)
#define AP_HOOKS_H
#if defined(AP_HOOK_PROBES_ENABLED) && !defined(APR_HOOK_PROBES_ENABLED)
#define APR_HOOK_PROBES_ENABLED 1
#endif
#if defined(APR_HOOK_PROBES_ENABLED)
#include "ap_hook_probes.h"
#endif
#include "apr.h"
#include "apr_hooks.h"
#include "apr_optional_hooks.h"
#if defined(DOXYGEN)
#define AP_DECLARE_STATIC
#define AP_DECLARE_EXPORT
#endif
#define AP_DECLARE_HOOK(ret,name,args) APR_DECLARE_EXTERNAL_HOOK(ap,AP,ret,name,args)
#define AP_IMPLEMENT_HOOK_BASE(name) APR_IMPLEMENT_EXTERNAL_HOOK_BASE(ap,AP,name)
#define AP_IMPLEMENT_HOOK_VOID(name,args_decl,args_use) APR_IMPLEMENT_EXTERNAL_HOOK_VOID(ap,AP,name,args_decl,args_use)
#define AP_IMPLEMENT_HOOK_RUN_ALL(ret,name,args_decl,args_use,ok,decline) APR_IMPLEMENT_EXTERNAL_HOOK_RUN_ALL(ap,AP,ret,name,args_decl, args_use,ok,decline)
#define AP_IMPLEMENT_HOOK_RUN_FIRST(ret,name,args_decl,args_use,decline) APR_IMPLEMENT_EXTERNAL_HOOK_RUN_FIRST(ap,AP,ret,name,args_decl, args_use,decline)
#define AP_IMPLEMENT_OPTIONAL_HOOK_RUN_ALL(ret,name,args_decl,args_use,ok, decline) APR_IMPLEMENT_OPTIONAL_HOOK_RUN_ALL(ap,AP,ret,name,args_decl, args_use,ok,decline)
#define AP_OPTIONAL_HOOK(name,fn,pre,succ,order) APR_OPTIONAL_HOOK(ap,name,fn,pre,succ,order)
#endif