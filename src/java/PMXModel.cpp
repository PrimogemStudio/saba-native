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
	JNINativeMethod methods[8];
	methods[0] = JNIMethod("load", "(Ljava/io/File;)V", Load);
	methods[1] = JNIMethod("render", "(JLjava/nio/ByteBuffer;)V", Render);
	methods[2] = JNIMethod("getTextures", "()Ljava/util/List;", GetTextures);
	methods[3] = JNIMethod("mappingVertices", "()V", MappingVertices);
	methods[4] = JNIMethod("getVertexCount", "()I", GetVertexCount);
	methods[5] = JNIMethod("getIndices", "()Ljava/nio/ByteBuffer;", GetIndices);
	methods[6] = JNIMethod("release", "(J)V", Release);
	methods[7] = JNIMethod("getIndexCount", "()I", GetIndexCount);
	return env->RegisterNatives(native, methods, 8);
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
	model->indices.resize(model->model->GetIndexCount());
	model->colors.resize(model->model->GetVertexCount());
	for (int i = 0; i < model->indices.size(); i++)
	{
		model->indices[i] = ReadIndex(model->model->GetIndices(), i, model->model->GetIndexElementSize());
	}
	auto meshs = model->model->GetSubMeshes();
	auto mc = model->model->GetSubMeshCount();
	auto mats = model->model->GetMaterials();
	std::vector<bool> cache(model->colors.size());
	for (int i = 0; i < mc; i++)
	{
		auto& mesh = meshs[i];
		auto& mat = mats[mesh.m_materialID];
		glm::vec<4, uint8_t> color = { static_cast<byte_t>(mat.m_diffuse.r * 255),static_cast<byte_t>(mat.m_diffuse.g * 255),static_cast<byte_t>(mat.m_diffuse.b * 255),static_cast<byte_t>(mat.m_alpha * 255) };
		for (int j = mesh.m_beginIndex; j < mesh.m_beginIndex + mesh.m_vertexCount; j++)
		{
			if (auto index = model->indices[j]; !cache[index])
			{
				model->colors[index] = color;
				cache[index] = true;
			}
		}
	}
}

void PMXModel::Render(JNIEnv* env, const jobject obj, const jlong buff, const jobject constants)
{
	Object self(obj);
	auto buf = (byte_t*)buff;
	auto cb = (char*)env->GetDirectBufferAddress(constants);
	auto matrix = *(glm::mat4*)cb;
	auto normal = *(glm::mat3*)(cb + 64);
	auto padding = *(int*)(cb + 108);
	auto pmx = (const PMXModel*)self.get<jlong>("ptr");
	auto positions = (glm::vec3*)pmx->model->GetUpdatePositions();
	auto normals = (glm::vec3*)pmx->model->GetUpdateNormals();
	auto uvs = pmx->model->GetUVs();
	auto vc = pmx->model->GetVertexCount();
	for (int i = 0; i < vc; i++)
	{
		*(glm::vec3*)buf = matrix * glm::vec4(positions[i], 1);
		buf += sizeof(glm::vec3);
		*(uint32_t*)buf = *(uint32_t*)&pmx->colors[i];
		buf += sizeof(uint32_t);
		*(glm::vec2*)buf = uvs[i];
		buf += sizeof(glm::vec2);
		*(jlong*)buf = *(jlong*)(cb + 100);
		buf += sizeof(jlong);
		*(glm::vec<3, byte_t>*)buf = glm::vec<3, byte_t>(glm::vec<3, int>(normal * normals[i] * 127.f) & 0xff);
		buf += sizeof(uint32_t) + padding;
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
	glm::vec2 uv;
	Object buff = env->NewDirectByteBuffer(&uv, sizeof(glm::vec2));
	buff.call<void>("order(Ljava/nio/ByteOrder;)Ljava/nio/ByteBuffer;", Class("java/nio/ByteOrder").call<Object>("nativeOrder", "()Ljava/nio/ByteOrder;"));
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
			uv = uvs[index];
			atlas.call<void>("mapping(ILjava/nio/ByteBuffer;)V", tid, buff);
			uvs[index] = uv;
			cache.emplace(index);
		}
	}
}

jint PMXModel::GetVertexCount(JNIEnv* env, const jobject self)
{
	return ((PMXModel*)Object(self).get<jlong>("ptr"))->model->GetVertexCount();
}

jint PMXModel::GetIndexCount(JNIEnv* env, const jobject obj)
{
	return ((PMXModel*)Object(obj).get<jlong>("ptr"))->indices.size();
}

jobject PMXModel::GetIndices(JNIEnv* env, const jobject self)
{
	auto model = (PMXModel*)Object(self).get<jlong>("ptr");
	return env->NewDirectByteBuffer(model->indices.data(), model->indices.size() * sizeof(int));
}

void PMXModel::Release(JNIEnv* env, jclass cls, const jlong ptr)
{
	delete (PMXModel*)ptr;
}
