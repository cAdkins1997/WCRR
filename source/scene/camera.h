#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../glmdefines.h"
#include "../common.h"

namespace vulkan {
    class Camera;

    inline f32 lastX = 1920 / 2.0f;
    inline f32 lastY = 1080 / 2.0f;
    inline bool firstMouse = true;
    inline f32 deltaTime = 0.0f;
    inline f32 lastFrameTime = 0.0f;

    enum Camera_Movement {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT,
        UP,
        DOWN
    };

    constexpr f32 YAW = -90.0f;
    constexpr f32 PITCH = 0.0f;
    constexpr f32 SPEED = 1.5f;
    constexpr f32 SENSITIVITY = 0.0005f;
    constexpr f32 ZOOM =  45.0f;

    class Camera {
    public:
        glm::vec3 Position{};
        glm::vec3 Front;
        glm::vec3 Up{};
        glm::vec3 Right{};
        glm::vec3 WorldUp{};
        f32 yaw;
        f32 pitch;
        f32 movementSpeed;
        f32 mouseSensitivity;
        f32 zoom;
        bool enableMouseLook = false;

        explicit Camera(
            glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
            float _yaw = YAW,
            float _pitch = PITCH
            );

        Camera(f32 posX, f32 posY, f32 posZ, f32 upX, f32 upY, f32 upZ, f32 _yaw, f32 _pitch);

        [[nodiscard]] glm::mat4 get_view_matrix() const;

        void process_mouse_scroll(f32 yoffset);
        void process_mouse_movement(f32 xoffset, f32 yoffset, bool constrainPitch);
        void process_keyboard(Camera_Movement direction, f32 deltaTime);

    private:
        void update_camera_vectors();
    };
}