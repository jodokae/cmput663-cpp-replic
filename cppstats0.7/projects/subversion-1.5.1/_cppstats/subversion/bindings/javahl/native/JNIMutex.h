#if !defined(JNIMUTEX_H)
#define JNIMUTEX_H
class JNICriticalSection;
struct apr_pool_t;
struct apr_thread_mutex_t;
class JNIMutex {
public:
JNIMutex(apr_pool_t *pool);
~JNIMutex();
friend class JNICriticalSection;
private:
apr_thread_mutex_t *m_mutex;
};
#endif