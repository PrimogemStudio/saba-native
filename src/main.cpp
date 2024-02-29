#include <jni.h>
#include <jnipp.h>

import MMD;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
	jni::init(vm);
	JNIEnv* env = nullptr;
	auto ver = JNI_VERSION_21;
	int r = 0;
	for (int i = 0; i < 3; i++)
	{
		r = vm->GetEnv((void**)&env, ver);
		if (r == JNI_OK) break;
		ver -= 0x10000;
	}
	if (r != JNI_OK)
	{
		ver = JNI_VERSION_10;
		r = vm->GetEnv((void**)&env, ver);
		if (r != JNI_OK) return JNI_EVERSION;
	}
	r = MMD::RegisterMethods(env);
	if (r != JNI_OK) return JNI_ERR;
	return ver;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved)
{
}