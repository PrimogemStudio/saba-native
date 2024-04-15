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
	auto indexSize = pmx->model->GetIndexElementSize();
	auto indices = pmx->model->GetIndices();
	auto positions = (glm::vec3*)pmx->model->GetUpdatePositions();
	auto normals = (glm::vec<3, byte_t>*)pmx->model->GetUpdateNormals();
	auto uvs = pmx->model->GetUVs();
	auto vc = pmx->model->GetVertexCount();
	auto meshs = pmx->model->GetSubMeshes();
	auto mc = pmx->model->GetSubMeshCount();
	auto mats = pmx->model->GetMaterials();
	for (int i = 0; i < vc; i++)
	{
		normals[i] = glm::vec<3, byte_t>(glm::vec<3, int>(clamp(normal * positions[i], -1.f, 1.f) * 127.f) & 0xff);
		positions[i] = matrix * glm::vec4(positions[i], 1);
	}
	for (int i = 0; i < mc; i++)
	{
		auto& mesh = meshs[i];
		auto& mat = mats[mesh.m_materialID];
		byte_t color[] = { static_cast<byte_t>(mat.m_diffuse.r * 255),static_cast<byte_t>(mat.m_diffuse.g * 255),static_cast<byte_t>(mat.m_diffuse.b * 255),static_cast<byte_t>(mat.m_alpha * 255) };
		for (int j = mesh.m_beginIndex; j < mesh.m_beginIndex + mesh.m_vertexCount; j++)
		{
			auto index = ReadIndex(indices, j, indexSize);
			*(glm::vec3*)buf = positions[index];
			buf += sizeof(glm::vec3);
			*(uint32_t*)buf = *(uint32_t*)color;
			buf += sizeof(uint32_t);
			*(glm::vec2*)buf = uvs[index];
			buf += sizeof(glm::vec2);
			*(jlong*)buf = *(jlong*)(cb + 100);
			buf += sizeof(jlong);
			*(decltype(normals))buf = normals[index];
			buf += sizeof(uint32_t) + padding;
		}
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
		if (t.empty()) continue;
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
		for (auto& t : pmx->textures)
		{
			if (t.empty()) continue;
			map[t] = i++;
		}
		map[""] = -1;
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
