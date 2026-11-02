/*
 * Bruce Lee: Dragon Warrior - PS Vita port
 * Port code Copyright (C) 2026 Leandro Ferrete; see AUTHORS.md and LICENSE.
 */

#ifndef BRUCELEE_JAVA_SHIM_H
#define BRUCELEE_JAVA_SHIM_H

#include <stdarg.h>
#include <falso_jni/jni.h>
#include <falso_jni/FalsoJNI_ImplBridge.h>

#ifdef __cplusplus
extern "C" {
#endif

void      java_shim_init(void);

jmethodID jni_shim_register_method(const char *name, const char *sig);
jmethodID jni_shim_get_method_id(const char *name);
int       jni_shim_has_method(jmethodID id);

jobject   jni_shim_object_call(jmethodID id, va_list args);
void      jni_shim_void_call(jmethodID id, va_list args);
jboolean  jni_shim_boolean_call(jmethodID id, va_list args);
jbyte     jni_shim_byte_call(jmethodID id, va_list args);
jchar     jni_shim_char_call(jmethodID id, va_list args);
jshort    jni_shim_short_call(jmethodID id, va_list args);
jint      jni_shim_int_call(jmethodID id, va_list args);
jlong     jni_shim_long_call(jmethodID id, va_list args);
jfloat    jni_shim_float_call(jmethodID id, va_list args);
jdouble   jni_shim_double_call(jmethodID id, va_list args);

#ifdef __cplusplus
}
#endif

#endif
