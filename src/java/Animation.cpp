module;
#include <jni.h>
#include <jnipp.h>
#include <memory>
#include <saba/Model/MMD/VMDAnimation.h>
module MMD;

using namespace MMD;
using namespace jni;

int Animation::RegisterMethods(JNIEnv* env)
{
	const auto native = env->FindClass("com/primogemstudio/advancedfmk/mmd/PMXModel$Animation");
	JNINativeMethod methods[2];
	methods[0] = JNIMethod("setupAnimation", "()V", SetupAnimation);
	methods[1] = JNIMethod("updateAnimation", "(FF)V", UpdateAnimation);
	return env->RegisterNatives(native, methods, 2);
}

void Animation::SetupAnimation(JNIEnv* env, const jobject obj)
{
	Object self(obj);
	auto pmx = (PMXModel*)self.get<jlong>("ptr");
	self.get<Object>("model", "Lcom/primogemstudio/advancedfmk/mmd/PMXModel;").set("animationTime", 0.f);
	pmx->animation.reset();
	pmx->model->InitializeAnimation();
	pmx->animation = std::make_unique<saba::VMDAnimation>();
	if (!pmx->animation->Create(pmx->model))
	{
		Throw(env, "Failed to create VMDAnimation");
		return;
	}
	auto count = self.call<int>("size");
	for (int i = 0; i < count; i++)
	{
		auto f = self.call<Object>("get", i);
		auto path = f.call<std::string>("getAbsolutePath");
		saba::VMDFile vmd;
		if (!ReadVMDFile(&vmd, path.c_str()))
		{
			Throw(env, ("Failed to read VMD file" + path).c_str());
			return;
		}
		if (!pmx->animation->Add(vmd))
		{
			Throw(env, ("Failed to add VMDAnimation: " + path).c_str());
			return;
		}
	}
	pmx->animation->SyncPhysics(0);
	self.set("maxFrame", pmx->animation->GetMaxKeyTime());
}

void Animation::UpdateAnimation(JNIEnv* env, jobject obj, jfloat frame, jfloat elapsed)
{
	Object self(obj);
	auto pmx = (const PMXModel*)self.get<jlong>("ptr");
	pmx->model->Update();
	pmx->model->BeginAnimation();
	pmx->model->UpdateAllAnimation(pmx->animation.get(), frame, elapsed);
	pmx->model->EndAnimation();
}
