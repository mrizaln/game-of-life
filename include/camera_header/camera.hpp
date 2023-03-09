#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


class Camera
{
public:
    enum CameraMovement
    {
        FORWARD,
        BACKWARD,
        RIGHT,
        LEFT,
        UPWARD,
        DOWNWARD,
    };

    enum CameraMagnification
    {
        ZOOM,
        FOV
    };

    // default values for camera
    static inline const float s_YAW{ -90.0f };
    static inline const float s_PITCH{ 0.0f };
    static inline const float s_SPEED{ 2.5f };
    static inline const float s_SENSITIVITY{ 0.1f };
    static inline const float s_FOV{ 45.0f };
    static inline const float s_ZOOM{ 1.0f };
    
    // euler angles
    float pitch;            // in degrees
    float yaw;              // in degrees

    // camera vectors
    glm::vec3 position;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 front;
    glm::vec3 worldUp;              // used for up and down movement
    glm::vec3 horizontalFront;      // used for forth and back movement

    // camera attributes
    float fov;
    float zoom;
    float speed;
    float sensitivity;

    // constructor with vectors
    Camera(
        glm::vec3 position = {0.0f, 0.0f, 3.0f},
        glm::vec3 worldUp = {0.0f, 1.0f, 0.0f},
        float pitch = s_PITCH,
        float yaw = s_YAW
    )
        : front{ glm::vec3(0.0f, 0.0f, -1.0f) }
        , speed{ s_SPEED }
        , sensitivity{ s_SENSITIVITY }
        , fov{ s_FOV }
        , zoom{ s_ZOOM }
    {
        this->position = position;
        this->worldUp = worldUp;
        this->pitch = pitch;
        this->yaw = yaw;

        updateCameraVector();
    }

    // constructor with scalar
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch)
    {
        position = glm::vec3(posX, posY, posZ);
        worldUp = glm::vec3(upX, upY, upZ);
        this->yaw = yaw;
        this->pitch = pitch;

        updateCameraVector();
    }

    // return view matrix
    glm::mat4 getViewMatrix()
    {
        return glm::lookAt(position, position + front, up);
        // return getLookAtMatrix();                            // implemented manually
    }

    // process camera movement
    void moveCamera(CameraMovement movement, float deltaTime)
    {
        switch (movement)
        {
        case CameraMovement::FORWARD:
            // position += front * speed * deltaTime;
            position += horizontalFront * speed * deltaTime;
            break;
        case CameraMovement::BACKWARD:
            // position -= front * speed * deltaTime;
            position -= horizontalFront * speed * deltaTime;
            break;
        case CameraMovement::RIGHT:
            position += right * speed * deltaTime;
            break;
        case CameraMovement::LEFT:
            position -= right * speed * deltaTime;
            break;
        case CameraMovement::UPWARD:
            // position += up * speed * deltaTime;
            position += worldUp * speed * deltaTime;
            break;
        case CameraMovement::DOWNWARD:
            // position -= up * speed * deltaTime;
            position -= worldUp * speed * deltaTime;
            break;
        default:
            break;
        }
    }

    // process 3D rotation from mouse
    void processMouseMovement(float xOffset, float yOffset)
    {
        xOffset *= sensitivity;
        yOffset *= sensitivity;

        pitch += yOffset;
        yaw += xOffset;

        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        if (yaw > 360.0f) yaw = fmod(yaw, 360.0f);
        if (yaw <   0.0f) yaw = fmod(yaw, 360.0f) - 360.0f;

        updateCameraVector();
    }

    // process zoom in and zoom out from mouse scroll
    void processMouseScroll(float yOffset, CameraMagnification which=FOV)
    {
        switch (which)
        {
        case FOV:
            fov -= yOffset;
            if (fov < 1.0f) fov = 1.0f;
            if (fov > 179.0f) fov = 179.0f;
            return;
        case ZOOM:
            zoom = (yOffset > 0 ? zoom * 1.1f : zoom / 1.1f);
            speed = (yOffset > 0 ? speed / 1.1f : speed * 1.1f);
            return;
        }
    }

    // reset look, to origin
    void lookAtOrigin()
    {
        const auto& direction{ -position };                     // direction of camera = origin - camera.position; origin at (0,0,0)
        // const auto normalizedDirection{ glm::normalize(direction) };

        // if (abs(normalizedDirection.x - front.x) < 0.05f &&\
        //     abs(normalizedDirection.y - front.y) < 0.05f &&\
        //     abs(normalizedDirection.z - front.z) < 0.05f)
        //     return;

        yaw = 180.0f/M_PI * atan(direction.z/direction.x);      // returns -90 to 90

        // for some reason direction.x < 0 doesn't work, we need to add some error (in this case 1e-5)
        if (direction.x < 1e-5f)        // fix direction flipped when camera front reset to origin when it already points to origin
            yaw += 180.0f;

        pitch = 180.0f/M_PI * atan(direction.y/sqrt(direction.x*direction.x + direction.z*direction.z));

        updateCameraVector();
    }


private:
    void updateCameraVector()
    {
        glm::vec3 direction{};

        direction.y = sin(glm::radians(pitch));
        direction.x = cos(glm::radians(pitch)) * cos(glm::radians(yaw));
        direction.z = cos(glm::radians(pitch)) * sin(glm::radians(yaw));

        front = glm::normalize(direction);
        right = glm::normalize(glm::cross(front, worldUp));
        up    = glm::normalize(glm::cross(right, front));

        horizontalFront = glm::normalize(glm::vec3(direction.x, 0, direction.z));       // horizontal front y value is zero (only in xz plane aka horizontal)
    }


    // my own LookAt function implementation
    glm::mat4 getLookAtMatrix()
    {
        glm::mat4 rotation {
            glm::transpose(
                glm::mat4(
                    glm::vec4(right,            0.0f),
                    glm::vec4(up,               0.0f),
                    glm::vec4(-front,           0.0f),      // front reversed because NDC is left handed (?)
                    glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
                )
            )
        };

        glm::mat4 translation {
            glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
            glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
            glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
            glm::vec4(-position,        1.0f)
        };

        auto lookAt { rotation * translation };

        return lookAt;
    }
};


#endif