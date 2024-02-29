module;
#include <jni.h>
#include <jnipp.h>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <saba/Model/MMD/PMXModel.h>
#include <saba/Model/MMD/VMDAnimation.h>
module MMD;

using namespace MMD;
using namespace jni;

int PMXModel::RegisterMethods(JNIEnv* env)
{
	const auto native = env->FindClass("com/primogemstudio/advancedfmk/mmd/PMXModel");
	JNINativeMethod methods[5];
	methods[0] = JNIMethod("load", "(Ljava/io/File;)V", Load);
	methods[1] = JNIMethod("render", "(Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V", Render);
	methods[2] = JNIMethod("getTextures", "()Ljava/util/List;", GetTextures);
	methods[3] = JNIMethod("mappingVertices", "()V", MappingVertices);
	methods[4] = JNIMethod("getVertexCount", "()I", GetVertexCount);
	return env->RegisterNatives(native, methods, 5);
}

void PMXModel::Load(JNIEnv* env, const jobject obj, const jobject arg)
{
	Object self(obj), file(arg);
	const Class saba("com/primogemstudio/advancedfmk/mmd/SabaNative");
	const auto dir = file.call<Object>("getParentFile()Ljava/io/File;").call<std::string>("getAbsolutePath");
	const auto path = file.call<std::string>("getAbsolutePath");
	const auto model = new PMXModel();
	auto pmx = std::make_unique<saba::PMXModel>();
	if (!pmx->Load(path, dir))
	{
		Throw(env, "Failed to load pmx file");
		return;
	}
	model->model = std::move(pmx);
	self.set("ptr", (jlong)model);
}

static uint32_t ReadIndex(const void* indices, const int offset, const size_t size)
{
	switch (size) {
	default:
	case 1:
		return *((const uint8_t*)indices + offset);
	case 2:
		return *((const uint16_t*)indices + offset);
	case 4:
		return *((const uint32_t*)indices + offset);
	}
}

void PMXModel::Render(JNIEnv* env, const jobject obj, const jobject buff, const jobject constants)
{
	Object self(obj);
	auto buf = (byte_t*)env->GetDirectBufferAddress(buff);
	auto cb = (char*)env->GetDirectBufferAddress(constants);
	auto matrix = *(glm::mat4*)cb;
	auto normal = *(glm::mat3*)(cb + 64);
	auto padding = *(int*)(cb + 108);
	auto pmx = (const PMXModel*)self.get<jlong>("ptr");
	auto count = pmx->model->GetIndexCount();
	auto indexSize = pmx->model->GetIndexElementSize();
	auto indices = pmx->model->GetIndices();
	auto positions = pmx->model->GetUpdatePositions();
	auto uvs = pmx->model->GetUVs();
	for (int i = 0; i < count; i++)
	{
		auto index = ReadIndex(indices, i, indexSize);
		auto pos = matrix * glm::vec4(positions[index], 1);
		auto nv = glm::vec<3, byte_t>(glm::vec<3, int>(clamp(normal * positions[index], -1.f, 1.f) * 127.f) & 0xff);
		*(glm::vec3*)buf = pos;
		buf += sizeof(glm::vec3);
		*(uint32_t*)buf = 0xffffffff;
		buf += sizeof(int);
		*(glm::vec2*)buf = uvs[index];
		buf += sizeof(glm::vec2);
		*(jlong*)buf = *(jlong*)(cb + 100);
		buf += sizeof(jlong);
		*(decltype(nv)*)buf = nv;
		buf += sizeof(int) + padding;
	}
}

jobject PMXModel::GetTextures(JNIEnv* env, const jobject obj)
{
	Class file("java/io/File");
	Object self(obj), list(Class("java/util/ArrayList").newInstance());
	auto pmx = (PMXModel*)self.get<jlong>("ptr");
	auto materials = pmx->model->GetMaterials();
	auto count = pmx->model->GetMaterialCount();
	std::unordered_set<std::string> set;
	for (int i = 0; i < count; i++) set.emplace(materials[i].m_texture);
	for (auto& t : set)
	{
		list.call<void>("add(Ljava/lang/Object;)Z", file.newInstance(t));
		pmx->textures.emplace_back(t);
	}
	return list.makeLocalReference();
}

void PMXModel::MappingVertices(JNIEnv* env, const jobject obj)
{
	Object self(obj);
	auto pmx = (const PMXModel*)self.get<jlong>("ptr");
	auto atlas = self.get<Object>("atlas", "Lcom/primogemstudio/advancedfmk/mmd/renderer/MMDTextureAtlas;");
	auto indices = (char*)pmx->model->GetIndices();
	auto meshs = pmx->model->GetSubMeshes();
	auto materials = pmx->model->GetMaterials();
	auto count = pmx->model->GetSubMeshCount();
	auto indexSize = pmx->model->GetIndexElementSize();
	auto uvs = (glm::vec2*)pmx->model->GetUVs();
	auto buff = (glm::vec2*)env->GetDirectBufferAddress(atlas.get<Object>("buff", "Ljava/nio/ByteBuffer;").getHandle());
	std::unordered_map<std::string, int> map;
	std::unordered_set<uint32_t> cache;
	{
		int i = 0;
		for (auto& t : pmx->textures) map[t] = i++;
	}
	for (int i = 0; i < count; i++)
	{
		auto& m = meshs[i];
		auto& mt = materials[m.m_materialID];
		auto tid = map[mt.m_texture];
		for (int j = 0; j < m.m_vertexCount; j++)
		{
			auto index = ReadIndex(indices, j + m.m_beginIndex, indexSize);
			if (cache.contains(index)) continue;
			*buff = uvs[index];
			atlas.call<void>("mapping(I)V", tid);
			uvs[index] = *buff;
			cache.emplace(index);
		}
	}
}

jint PMXModel::GetVertexCount(JNIEnv* env, const jobject self)
{
	return ((PMXModel*)Object(self).get<jlong>("ptr"))->model->GetIndexCount();
}
