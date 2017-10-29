#if !defined(JNIBYTEARRAY_H)
#define JNIBYTEARRAY_H
#include <jni.h>
class JNIByteArray {
private:
jbyteArray m_array;
jbyte *m_data;
bool m_deleteByteArray;
public:
bool isNull();
const signed char *getBytes();
int getLength();
JNIByteArray(jbyteArray jba, bool deleteByteArray = false);
~JNIByteArray();
};
#endif
