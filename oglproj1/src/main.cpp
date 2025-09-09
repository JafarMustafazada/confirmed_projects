#include <iostream>
#include <functional>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>


// --- shaders ---
static const char *VS_SRC = R"(
#version 330 core
layout(location=0) in vec3 a;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(a,1.0); }
)";
static const char *FS_SRC = R"(
#version 330 core
out vec4 o; 
void main(){ o = vec4(0.7,0.5,0.2,1.0); }
)";

// --- cube geometry (36 verts) ---
static const float CUBE_VERTS[] = {
	-0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f,
	0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
	-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
	0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f,
	-0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
	-0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
	0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f,
	0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
	-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f,
	0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,
	-0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f,
	0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f
};

// --- utilities ---
GLFWwindow *init_window(int w = 800, int h = 600, const char *title = "Euler/Quat") {
	if (!glfwInit()) return nullptr;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow *win = glfwCreateWindow(w, h, title, nullptr, nullptr);
	if (!win) {
		glfwTerminate();
		return nullptr;
	}
	glfwMakeContextCurrent(win);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cerr << "glad\n";
		return nullptr;
	}
	glViewport(0, 0, w, h);
	return win;
}

GLuint compile_shader(GLenum t, const char *src) {
	GLuint s = glCreateShader(t);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char b[512];
		glGetShaderInfoLog(s, 512, nullptr, b);
		std::cerr << b;
	}
	return s;
}

GLuint create_program(const char *vs, const char *fs) {
	GLuint p = glCreateProgram();
	GLuint vsId = compile_shader(GL_VERTEX_SHADER, vs), fsId = compile_shader(GL_FRAGMENT_SHADER, fs);
	glAttachShader(p, vsId);
	glAttachShader(p, fsId);
	glLinkProgram(p);
	glDeleteShader(vsId);
	glDeleteShader(fsId);
	return p;
}

GLuint make_cube_vao() {
	GLuint VAO, VBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTS), CUBE_VERTS, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
	return VAO;
}

// --- math helpers (simple single-purpose functions) ---

// 1) Euler angles (deg) -> 4x4 rotation matrix (Z * Y * X order)
glm::mat4 eulerToMatrixDeg(const glm::vec3 &eulerDeg) {
	glm::vec3 r = glm::radians(eulerDeg);
	glm::mat4 rx = glm::rotate(glm::mat4(1.0f), r.x, glm::vec3(1, 0, 0));
	glm::mat4 ry = glm::rotate(glm::mat4(1.0f), r.y, glm::vec3(0, 1, 0));
	glm::mat4 rz = glm::rotate(glm::mat4(1.0f), r.z, glm::vec3(0, 0, 1));
	return rz * ry * rx;
}

// 2) Euler angles (deg) -> quaternion (same Z * Y * X order)
glm::quat eulerToQuatDeg(const glm::vec3 &eulerDeg) {
	glm::vec3 r = glm::radians(eulerDeg);
	glm::quat qx = glm::angleAxis(r.x, glm::vec3(1, 0, 0));
	glm::quat qy = glm::angleAxis(r.y, glm::vec3(0, 1, 0));
	glm::quat qz = glm::angleAxis(r.z, glm::vec3(0, 0, 1));
	return qz * qy * qx;
}

// 3) Quaternion -> 4x4 matrix
glm::mat4 quatToMatrix(const glm::quat &q) { return glm::mat4_cast(q); }

// --- render loop (accepts a callable that produces model matrix each frame) ---
void render_loop(GLFWwindow *win, GLuint prog, GLuint vao, std::function<glm::mat4()> getModel) {
	int w, h;
	glfwGetFramebufferSize(win, &w, &h);
	glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -3.0f));
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)w / (float)h, 0.1f, 100.0f);
	glEnable(GL_DEPTH_TEST);
	while (!glfwWindowShouldClose(win)) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glm::mat4 model = getModel();
		glm::mat4 mvp = proj * view * model;
		glUseProgram(prog);
		glUniformMatrix4fv(glGetUniformLocation(prog, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glfwSwapBuffers(win);
		glfwPollEvents();
	}
}

int main(int argc, char **argv) {
	GLFWwindow *win = init_window();
	if (!win) return -1;
	GLuint prog = create_program(VS_SRC, FS_SRC);
	GLuint cubeVAO = make_cube_vao();

	// fixed angles (degrees)
	glm::vec3 anglesDeg(30.0f, 45.0f, 60.0f);

	// build matrices/quaternion via functions
	glm::mat4 modelFromEuler = eulerToMatrixDeg(anglesDeg);
	glm::quat q = eulerToQuatDeg(anglesDeg);
	glm::mat4 modelFromQuat = quatToMatrix(q);

	// choose which model to render (swap modelFromEuler vs modelFromQuat)
	auto getModel = [modelFromQuat]() -> glm::mat4 {
		// translate so cube is centered in view
		return glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)) * modelFromQuat;
	};

	render_loop(win, prog, cubeVAO, getModel);

	glfwDestroyWindow(win);
	glfwTerminate();
	return 0;
}
