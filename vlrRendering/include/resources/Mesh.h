#ifndef VLR_RENDERING_MESH_H
#define VLR_RENDERING_MESH_H

#include <stdio.h>
#include <glm/glm.hpp>
#include <gl/glew.h>
#include <stdint.h>
#include <assimp/postprocess.h>

#include "../util/CUDAUtil.h"
#include "../resources/Image.h"

namespace vlr
{
	namespace rendering
	{
		struct Vertex
		{
			glm::vec3 _pos;
			glm::vec3 _normal;
			glm::vec2 _texCoord;
		};

		class SubMesh
		{
		public:
			HOST_DEVICE_FUNC SubMesh()
				: _indices(nullptr), _vertices(nullptr),
				_indexCount(0), _materialIndex((uint32_t)-1)
			{

			}

			HOST_DEVICE_FUNC ~SubMesh()
			{
				delete[] _indices;
				delete[] _vertices;
			}

			int32_t* _indices;
			int32_t _indexCount;
			Vertex* _vertices;
			int32_t _vertexCount;

			uint32_t _materialIndex;
		};

		class Mesh
		{
		public:
			Mesh(bool storeTextures = false, aiPostProcessSteps normalMode = aiProcess_GenSmoothNormals);
			Mesh(const char* filename, bool storeTextures = false, aiPostProcessSteps normalMode = aiProcess_GenSmoothNormals);
			Mesh(const Mesh&);
			~Mesh();

			Mesh& operator=(const Mesh& other);

			void render();
			
			bool load(const char* filename);
			void unload();

			void transform(const glm::mat4& matrix);
			void calcMinMax();

			inline bool isLoaded() const;

			inline bool hasTextures() const;
			
			inline Image* getStoredTextures() const;

			inline int32_t getSubMeshCount() const;
			inline const SubMesh* getSubMesh(int32_t i) const;
			
			inline const glm::vec3* getMin() const;
			inline const glm::vec3* getMax() const;

		private:
			bool _test;

			bool _loaded;
			bool _storeTextures;

			SubMesh* _subMeshes;
			size_t _subMeshCount;

			aiPostProcessSteps _normalMode;

			size_t _textureCount;

			GLuint* _textures;
			Image* _images;

			glm::vec3 _min;
			glm::vec3 _max;
		};

		bool Mesh::isLoaded() const
		{
			return _loaded;
		}

		bool Mesh::hasTextures() const
		{
			return _textures != nullptr;
		}

		Image* Mesh::getStoredTextures() const
		{
			return _images;
		}

		int32_t Mesh::getSubMeshCount() const
		{
			return (int32_t)_subMeshCount;
		}

		const SubMesh* Mesh::getSubMesh(int32_t i) const
		{
			if (i < 0 || i >= (int32_t)_subMeshCount)
			{
				fprintf(stderr, "Attempted to access invalid submesh\n");

				return nullptr;
			}

			return &_subMeshes[i];
		}
		
		const glm::vec3* Mesh::getMin() const
		{
			return &_min;
		}

		const glm::vec3* Mesh::getMax() const
		{
			return &_max;
		}
	}
}

#endif /* VLR_RENDERING_MESH_H */
