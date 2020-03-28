#pragma once
#include <glm.hpp>

const int COLLISION_SAMPLES = 200;
const float M_PI = 3.1415926535897932384626433832795;

struct UniformBufferObject {
	alignas(4) float time;
	alignas(4) float yDirection;
	alignas(4) float playerRadius;
	alignas(8) glm::vec2 resolution;
	alignas(16) glm::vec3 cameraEye;
	alignas(16) glm::vec3 cameraFront;
	alignas(16) glm::vec3 worldUp;
	alignas(16) glm::vec3 playerPos;
	alignas(16) glm::vec4 playerColor;
	alignas(16) glm::mat4 viewMat;
};

struct StorageBufferObject {
	alignas(4) float deltaTime;
	alignas(4) float radius;
	alignas(4) float mass;
	alignas(4) float damping;
	alignas(16) glm::vec3 velocity;
	alignas(16) glm::vec3 force;
	alignas(16) glm::vec3 position;
	alignas(16) glm::vec3 debug;
	alignas(16) glm::vec3 collisionDirs[COLLISION_SAMPLES];
};

