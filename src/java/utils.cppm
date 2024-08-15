module;
#include <jni.h>
#include <type_traits>
export module utils;

export inline void Throw(JNIEnv* env, const char* msg)
{
	env->ThrowNew(env->FindClass("com/sun/jdi/NativeMethodException"), msg);
}

export template <typename F> requires std::is_function_v<std::remove_pointer_t<F>>
consteval JNINativeMethod JNIMethod(const char* name, const char* sig, F ptr)
{
	return { const_cast<char*>(name),const_cast<char*>(sig), (void*)ptr };
}