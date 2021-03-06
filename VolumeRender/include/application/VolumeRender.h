#ifndef VLR_VOLUMERENDER_APPLICATION
#define VLR_VOLUMERENDER_APPLICATION

#include "app/Application.h"
#include "app/Framebuffer.h"
#include "maths/Types.h"
#include "rendering/rendering_attributes.h"
#include "rendering/Camera.h"
#include "resources/Octree.h"
#include "resources/Mesh.h"

#include <stdio.h>
#include <assimp/Importer.hpp>

namespace vlr
{
	class VolumeRender
		: public rendering::Application
	{
	public:
		VolumeRender(int32_t argc, char** argv);

		void generate();

		void update(double dt);

		void render();

		void genGrid();

		// Callbacks
		static void resize_callback(GLFWwindow* window,
			int32_t width, int32_t height);
		static void mouse_move_callback(GLFWwindow* window,
			double x, double y);
		static void mouse_callback(GLFWwindow* window, int32_t button,
			int32_t action, int32_t mods);
		static void key_callback(GLFWwindow* window, int32_t key,
			int32_t scancode, int32_t action, int32_t mods);

	private:
		int _scene;
		int _depth;

		const char* _treeFilename;
		char* tree_data;

		bool _saveTrees;

		rendering::rendering_attributes_t rendering_attributes;

		Assimp::Importer _aiImporter;
		const aiScene* _aiScene;

		float _nearDepth, _farDepth;

		double _mouseX, _mouseY;

		rendering::Mesh _mesh;
		
		glm::vec3 normal;
		int32_t lastx, lasty, lastz;
		
		rendering::Camera _camera;

		int32_t* _gpuTree;

		float _rot;

		bool _cursorLocked;

		glm::vec3 _camRot;
	};
}

#endif /* VLR_VOLUMERENDER_APPLICATION */
