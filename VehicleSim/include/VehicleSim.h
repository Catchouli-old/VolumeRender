#ifndef VLR_VEHICLESIM_APPLICATION
#define VLR_VEHICLESIM_APPLICATION

// Application headers
#include "app/Application.h"
#include "rendering/Camera.h"

// Rocket interfaces
#include "system/RocketSystem.h"
#include "rendering/ShellRenderInterfaceOpenGL.h"
#include "input/InputConverter.h"

// Standard headers
#include <stdio.h>

namespace vlr
{
	class VehicleSim
	: public common::Application
	{
	public:
		VehicleSim();
		~VehicleSim();

		void update(double dt);

		void render();

		// Callbacks
		static void mouse_move_callback(GLFWwindow* window,
			double x, double y);
		static void mouse_callback(GLFWwindow* window, int button,
			int action, int mods);
		static void key_callback(GLFWwindow* window, int key,
			int scancode, int action, int mods);
		static void scroll_callback(GLFWwindow* window, double x,
			double y);
		static void scroll_callback(GLFWwindow* window,
			unsigned int codepoint);

	protected:
		VehicleSim(const VehicleSim&);

	private:
		// Camera
		common::Camera _camera;

		// Rocket
		InputConverter _inputConverter;
		Rocket::Core::Context* _rocketContext;
		Rocket::Core::ElementDocument* _document;
		RocketSystem _rocketSystem;
		ShellRenderInterfaceOpenGL _rocketRenderer;
	};

}

#endif /* VLR_VEHICLESIM_APPLICATION */