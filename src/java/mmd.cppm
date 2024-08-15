module;
#include <jni.h>
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
		std::vector<uint32_t> indices;
		std::vector<glm::vec<4, uint8_t>> colors;

		static int RegisterMethods(JNIEnv* env);
		static void Load(JNIEnv* env, jobject obj, jobject file);
		static void Render(JNIEnv* env, jobject obj, jlong buff, jobject constants);
		static jobject GetTextures(JNIEnv* env, jobject obj);
		static void MappingVertices(JNIEnv* env, jobject obj);
		static jint GetVertexCount(JNIEnv* env, jobject obj);
		static jint GetIndexCount(JNIEnv* env, jobject obj);
		static jobject GetIndices(JNIEnv* env, jobject obj);
		static void Release(JNIEnv* env, jclass cls, jlong ptr);
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

int MMD::RegisterMethods(JNIEnv* env)
{
	return PMXModel::RegisterMethods(env) + Animation::RegisterMethods(env);
}