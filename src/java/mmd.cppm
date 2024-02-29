module;
#include <jni.h>
#include <jnipp.h>
#include <unordered_map>
#include <saba/Model/MMD/PMXModel.h>
#include <saba/Model/MMD/VMDAnimation.h>
export module MMD;

import utils;

export namespace MMD
{
	class PMXModel
	{
	public:
		std::shared_ptr<saba::PMXModel>	model;
		std::unique_ptr<saba::VMDAnimation>	animation;
		std::vector<std::string> textures;

		static int RegisterMethods(JNIEnv* env);
		static void Load(JNIEnv* env, jobject obj, jobject file);
		static void Render(JNIEnv* env, jobject obj, jobject buff, jobject constants);
		static jobject GetTextures(JNIEnv* env, jobject obj);
		static void MappingVertices(JNIEnv* env, jobject obj);
		static jint GetVertexCount(JNIEnv* env, jobject obj);
	};

	class Animation
	{
	public:
		static int RegisterMethods(JNIEnv* env);
		static void SetupAnimation(JNIEnv* env, jobject obj);
		static void UpdateAnimation(JNIEnv* env, jobject obj, jfloat frame, jfloat elapsed);
	};

	int RegisterMethods(JNIEnv* env);
}

module:private;

static void Release(JNIEnv* env, jclass cls, jclass clz, jlong ptr)
{
	const static std::unordered_map<std::string, int> mapping{
	{ "com.primogemstudio.advancedfmk.mmd.PMXModel", 1 }
	};
	using namespace jni;
	switch (mapping.at(Class(clz).getName()))
	{
	case 1:
		delete (MMD::PMXModel*)ptr;
		break;
	default:
		break;
	}
}

int MMD::RegisterMethods(JNIEnv* env)
{
	const auto native = env->FindClass("com/primogemstudio/advancedfmk/mmd/SabaNative");
	constexpr JNINativeMethod methods[] = { JNIMethod("release", "(Ljava/lang/Class;J)V", Release) };
	return PMXModel::RegisterMethods(env) + Animation::RegisterMethods(env) + env->RegisterNatives(native, methods, 1);
}