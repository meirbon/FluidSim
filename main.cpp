#include <GL/glew.h>

#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#include <thread>

#include "Buffer.h"
#include "Camera.h"
#include "Plane.h"
#include "Shader.h"
#include "Simulator.h"
#include "Timer.h"
#include "VertexArray.h"
#include "Window.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

using namespace glm;

#define PARTICLE_COUNT 5000
#define SCRWIDTH 1024
#define SCRHEIGHT 768

inline void CheckGL(int line)
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
		std::cout << "LINE: " << line << ", OpenGL Error: " << glewGetErrorString(err) << ", " << err << std::endl;
}

#define CHECKGL() CheckGL(__LINE__)

int main(int argc, char *argv[])
{
	Timer timer{};
	auto window = Window("FluidSim", SCRWIDTH, SCRHEIGHT);
	Camera camera = Camera(vec3(0.0f, 25.0f, 30.0f));
	
	bool runSim = false;
	auto shader = Shader("shaders/sphere.vert", "shaders/sphere.frag");
	auto planeShader = Shader("shaders/plane.vert", "shaders/plane.frag");

	SimulationParams simulationParams{};
	simulationParams.particleRadius = .7f;
	simulationParams.smoothingRadius = 1.0f;
	simulationParams.smoothingRadius2 = 1.0f;
	simulationParams.restDensity = 15.0f;
	simulationParams.gravityMult = 2000.0f;
	simulationParams.particleMass = 0.1f;
	simulationParams.particleViscosity = 1.0f;
	simulationParams.particleDrag = 0.025f;

	Simulator simulator(16, vec3(-6.0f, 0.0f, 0.0f));
	const auto pid = simulator.addParams(simulationParams);
	simulator.addParticles(PARTICLE_COUNT, pid);
	simulator.addPlane(
		Plane(vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f), vec2(20.0f, 20.0f)));
	simulator.addPlane(
		Plane(vec3(-20.0f, 7.5f, 0.0f), vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f), vec2(20.0f, 7.5f)));
	simulator.addPlane(
		Plane(vec3(20.0f, 7.5f, 0.0f), vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f), vec2(20.0f, 7.5f)));
	simulator.addPlane(
		Plane(vec3(0.0f, 7.5f, -20.0f), vec3(-1.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), vec2(20.0f, 7.5f)));
	simulator.addPlane(
		Plane(vec3(0.0f, 7.5f, 20.0f), vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), vec2(20.0f, 7.5f)));

	const auto &particles = simulator.getParticles();

	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	std::string warn, err;
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, "models/sphere.obj");

	if (!err.empty())
		std::cout << "Model error: " << err << std::endl, exit(1);
	if (!ret)
		exit(1);

	std::vector<vec3> vertices, normals;
	for (auto &s : shapes)
	{
		int index_offset = 0;
		for (unsigned int f = 0; f < s.mesh.num_face_vertices.size(); f++)
		{
			int fv = s.mesh.num_face_vertices[f];
			for (int v = 0; v < fv; v++)
			{
				const auto &idx = s.mesh.indices[index_offset + v];
				const auto &vx = attrib.vertices[3 * idx.vertex_index + 0];
				const auto &vy = attrib.vertices[3 * idx.vertex_index + 1];
				const auto &vz = attrib.vertices[3 * idx.vertex_index + 2];
				if (!attrib.normals.empty())
				{
					const auto &nx = attrib.normals[3 * idx.normal_index + 0];
					const auto &ny = attrib.normals[3 * idx.normal_index + 1];
					const auto &nz = attrib.normals[3 * idx.normal_index + 2];

					normals.emplace_back(nx, ny, nz);
				}

				const vec3 vertex = vec3(vx, vy, vz);

				vertices.push_back(glm::normalize(vertex));
			}

			index_offset += fv;
		}
	}

	auto vBuffer = Buffer(GL_ARRAY_BUFFER, vertices.size(), sizeof(vec3), vertices.data(), 3);
	auto nBuffer = Buffer(GL_ARRAY_BUFFER, normals.size(), sizeof(vec3), normals.data(), 3);
	auto sphereVAO = VertexArray();
	sphereVAO.assignBuffer(0, vBuffer);
	sphereVAO.assignBuffer(1, nBuffer);

	shader.enable();
	shader.setUniformMatrix4fv("view", glm::mat4(1.0f));
	shader.setUniformMatrix4fv("projection", glm::mat4(1.0f));
	shader.setUniformMatrix4fv("model", glm::mat4(1.0f));
	shader.setUniform3f("lightIntensity", vec3(1.0f));
	shader.setUniform3f("lightDirection", glm::normalize(vec3(-1.0f, 1.0f, 0.0f)));
	shader.setUniform3f("ambient", vec3(0.1f));
	shader.disable();

	planeShader.enable();
	planeShader.setUniformMatrix4fv("view", glm::mat4(1.0f));
	planeShader.setUniformMatrix4fv("projection", glm::mat4(1.0f));
	planeShader.setUniformMatrix4fv("model", glm::mat4(1.0f));
	planeShader.setUniform3f("lightIntensity", vec3(1.0f));
	planeShader.setUniform3f("lightDirection", glm::normalize(vec3(-1.0f, -1.0f, 0.0f)));
	planeShader.setUniform3f("ambient", vec3(0.1f));
	planeShader.disable();

	timer.reset();
	float elapsed = 0.1f, elapsedSum = 0.0f;
	while (!window.shouldClose())
	{
		elapsedSum += elapsed;
		if (runSim)
			simulator.update(elapsed);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		shader.enable();
		shader.setUniformMatrix4fv("view", camera.GetViewMatrix());
		shader.setUniformMatrix4fv("projection", camera.GetProjectionMatrix(SCRWIDTH, SCRHEIGHT, 0.1f, 1e34f));
		shader.setUniform1f("radius", simulationParams.particleRadius);
		shader.setUniform3f("color", vec3(1.0f, 0.0f, 0.0f));

		sphereVAO.bind();
		for (const auto &p : particles)
		{
			shader.setUniform3f("position", p.position);
			glDrawArrays(GL_TRIANGLES, 0, vertices.size());
		}
		CHECKGL();
		sphereVAO.unbind();

		planeShader.enable();
		planeShader.setUniformMatrix4fv("view", camera.GetViewMatrix());
		planeShader.setUniformMatrix4fv("projection", camera.GetProjectionMatrix(SCRWIDTH, SCRHEIGHT, 0.1f, 1e34f));

		for (const auto &plane : simulator.getPlanes())
		{
			plane.draw(planeShader);
			CHECKGL();
		}

		const auto *keys = window.keys;

		if (keys[GLFW_KEY_LEFT_SHIFT])
			elapsed *= 5.0f;
		if (keys[GLFW_KEY_Q] || keys[GLFW_KEY_ESCAPE])
			window.close();
		if (keys[GLFW_KEY_W])
			camera.ProcessKeyboard(FORWARD, elapsed);
		if (keys[GLFW_KEY_S])
			camera.ProcessKeyboard(BACKWARD, elapsed);
		if (keys[GLFW_KEY_A])
			camera.ProcessKeyboard(LEFT, elapsed);
		if (keys[GLFW_KEY_D])
			camera.ProcessKeyboard(RIGHT, elapsed);
		if (keys[GLFW_KEY_SPACE])
			camera.ProcessKeyboard(UP, elapsed);
		if (keys[GLFW_KEY_LEFT_CONTROL])
			camera.ProcessKeyboard(DOWN, elapsed);
		if (keys[GLFW_KEY_UP])
			camera.ProcessMouseMovement(0.0f, 3.0f);
		if (keys[GLFW_KEY_DOWN])
			camera.ProcessMouseMovement(0.0f, -3.0f);
		if (keys[GLFW_KEY_LEFT])
			camera.ProcessMouseMovement(-3.0f, 0.0f);
		if (keys[GLFW_KEY_RIGHT])
			camera.ProcessMouseMovement(3.0f, 0.0f);
		if (keys[GLFW_KEY_R] && elapsedSum > 200.0f)
			runSim = !runSim, elapsedSum = 0.0f;
		if (keys[GLFW_KEY_BACKSPACE] && elapsedSum > 200.0f)
			simulator.reset(), elapsedSum = 0.0f;

		window.pollEvents();
		window.present();

		constexpr float desiredFrametime = 1000.0f / 60.0f;
		elapsed = timer.elapsed();
		if (elapsed != desiredFrametime)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(int(desiredFrametime - elapsed)));
			elapsed = timer.elapsed();
		}
		timer.reset();
	}

	return 0;
}