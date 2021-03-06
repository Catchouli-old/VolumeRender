#ifndef VLR_RENDERING_CAMERA
#define VLR_RENDERING_CAMERA

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <stdint.h>

namespace vlr
{
	namespace rendering
	{
		struct viewport
		{
			int32_t x, y, w, h;

			bool pointInViewport(int32_t x, int32_t y)
			{
				return x >= this->x && y >= this->y &&
					x < this->x + w && y < this->y + h;
			}
		};

		class Camera
		{
		public:
			Camera();
			Camera(const glm::vec3& position, const glm::quat& rotation);
			~Camera();

			void updateGL();
			
			inline glm::vec3 getPos() const;
			inline glm::quat getRot() const;

			inline glm::mat4 getProjectionMatrix() const;
			inline glm::mat4 getRotationMatrix() const;

			inline glm::mat4 getMVP() const;
			
			inline glm::vec3 getLeft() const;
			inline glm::vec3 getUp() const;
			inline glm::vec3 getForward() const;

			inline const viewport& getViewport() const;
			
			inline float getFov() const;
			inline float getAspect() const;
			inline float getNear() const;
			inline float getFar() const;

			glm::vec3 screenSpaceToWorld(float x, float y,
				float dist) const;

			glm::vec3 worldSpaceToScreen(float x, float y,
				float z) const;

			void setViewport(int32_t x, int32_t y, int32_t width, int32_t height);
			void perspective(float fov, float aspect, float near, float far);
			void orthographic(float scale, float aspect);

			void setPos(const glm::vec3& pos);
			void setRot(const glm::quat quat);

			void translate(const glm::vec3& offset);
			void rotate(const glm::quat& quat);

			void rotate(const glm::vec3& euler);

		protected:
			float _fov, _aspect, _near, _far;

			// Position & rotation
			glm::vec3 _position;
			glm::quat _rotation;

			// Viewport in screen space
			viewport _viewport;

			// MVP matrices
			glm::mat4 _projection;
			glm::mat4 _rotationMatrix;
			glm::mat4 _view;
			glm::mat4 _model;

		private:
			Camera(const Camera&);
		};
		
		glm::vec3 Camera::getPos() const
		{
			return _position;
		}
		
		glm::quat Camera::getRot() const
		{
			return _rotation;
		}

		glm::mat4 Camera::getProjectionMatrix() const
		{
			return _projection;
		}

		glm::mat4 Camera::getRotationMatrix() const
		{
			return _rotationMatrix;
		}

		glm::mat4 Camera::getMVP() const
		{
			// Calculate MVP
			return _projection * _model * _view;
		}
		
		glm::vec3 Camera::getLeft() const
		{
			return glm::vec3(-1.0f, 0, 0) * _rotation;
		}
		
		glm::vec3 Camera::getUp() const
		{
			return glm::vec3(0, 1.0f, 0) * _rotation;
		}
		
		glm::vec3 Camera::getForward() const
		{
			return glm::vec3(0, 0, -1.0f) * _rotation;
		}

		const viewport& Camera::getViewport() const
		{
			return _viewport;
		}
			
		float Camera::getFov() const
		{
			return _fov;
		}

		float Camera::getAspect() const
		{
			return _aspect;
		}

		float Camera::getNear() const
		{
			return _near;
		}

		float Camera::getFar() const
		{
			return _far;
		}
	}
}

#endif /* VLR_RENDERING_CAMERA */
