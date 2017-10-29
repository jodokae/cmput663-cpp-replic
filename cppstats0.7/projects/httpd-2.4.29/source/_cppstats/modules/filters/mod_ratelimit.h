#if !defined(_MOD_RATELIMIT_H_)
#define _MOD_RATELIMIT_H_
#if !defined(WIN32)
#define AP_RL_DECLARE(type) type
#define AP_RL_DECLARE_NONSTD(type) type
#define AP_RL_DECLARE_DATA
#elif defined(AP_RL_DECLARE_STATIC)
#define AP_RL_DECLARE(type) type __stdcall
#define AP_RL_DECLARE_NONSTD(type) type
#define AP_RL_DECLARE_DATA
#elif defined(AP_RL_DECLARE_EXPORT)
#define AP_RL_DECLARE(type) __declspec(dllexport) type __stdcall
#define AP_RL_DECLARE_NONSTD(type) __declspec(dllexport) type
#define AP_RL_DECLARE_DATA __declspec(dllexport)
#else
#define AP_RL_DECLARE(type) __declspec(dllimport) type __stdcall
#define AP_RL_DECLARE_NONSTD(type) __declspec(dllimport) type
#define AP_RL_DECLARE_DATA __declspec(dllimport)
#endif
AP_RL_DECLARE_DATA extern const apr_bucket_type_t ap_rl_bucket_type_end;
AP_RL_DECLARE_DATA extern const apr_bucket_type_t ap_rl_bucket_type_start;
#define AP_RL_BUCKET_IS_END(e) (e->type == &ap_rl_bucket_type_end)
#define AP_RL_BUCKET_IS_START(e) (e->type == &ap_rl_bucket_type_start)
AP_RL_DECLARE(apr_bucket*) ap_rl_end_create(apr_bucket_alloc_t *list);
AP_RL_DECLARE(apr_bucket*) ap_rl_start_create(apr_bucket_alloc_t *list);
#endif