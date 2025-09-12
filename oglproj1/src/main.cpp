#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

// Shader sources
const char *vsSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 model, view, projection;
uniform mat3 normalMatrix;
out vec3 FragPos, Normal;
void main() {
    FragPos = vec3(model * vec4(aPos,1.0));
    Normal = normalMatrix * aNormal;
    gl_Position = projection * view * vec4(FragPos,1.0);
}
)";
const char *fsSrc = R"(
#version 330 core
out vec4 FragColor;
in vec3 FragPos, Normal;
uniform vec3 lightPos, lightAmbient, lightDiffuse, lightSpecular;
uniform vec3 materialAmbient, materialDiffuse, materialSpecular, materialEmission;
uniform float materialShininess;
uniform vec3 viewPos;
void main() {
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    vec3 ambient = lightAmbient * materialAmbient;
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = lightDiffuse * (diff * materialDiffuse);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), materialShininess);
    vec3 specular = lightSpecular * (spec * materialSpecular);
    FragColor = vec4(ambient + diffuse + specular + materialEmission, 1.0);
}
)";

// Shader class
struct Shader {
	unsigned int ID;
	Shader(const char *vs, const char *fs) {
		auto compile = [](const char *src, GLenum type) {
			unsigned int s = glCreateShader(type);
			glShaderSource(s, 1, &src, NULL);
			glCompileShader(s);
			int ok;
			glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
			if (!ok) {
				char log[512];
				glGetShaderInfoLog(s, 512, NULL, log);
				std::cerr << log << std::endl;
			}
			return s;
		};
		unsigned v = compile(vs, GL_VERTEX_SHADER), f = compile(fs, GL_FRAGMENT_SHADER);
		ID = glCreateProgram();
		glAttachShader(ID, v);
		glAttachShader(ID, f);
		glLinkProgram(ID);
		int ok;
		glGetProgramiv(ID, GL_LINK_STATUS, &ok);
		if (!ok) {
			char log[512];
			glGetProgramInfoLog(ID, 512, NULL, log);
			std::cerr << log << std::endl;
		}
		glDeleteShader(v);
		glDeleteShader(f);
	}
	~Shader() { glDeleteProgram(ID); }
	void use() const { glUseProgram(ID); }
	void set(const char *n, const glm::mat4 &m) const {
		glUniformMatrix4fv(glGetUniformLocation(ID, n), 1, GL_FALSE, glm::value_ptr(m));
	}
	void set(const char *n, const glm::mat3 &m) const {
		glUniformMatrix3fv(glGetUniformLocation(ID, n), 1, GL_FALSE, glm::value_ptr(m));
	}
	void set(const char *n, const glm::vec3 &v) const {
		glUniform3fv(glGetUniformLocation(ID, n), 1, glm::value_ptr(v));
	}
	void set(const char *n, float v) const { glUniform1f(glGetUniformLocation(ID, n), v); }
};

// Mesh class
struct Mesh {
	std::vector<float> verts;
	std::vector<unsigned> idx;
	unsigned VAO = 0, VBO = 0, EBO = 0;
	~Mesh() {
		if (VAO) glDeleteVertexArrays(1, &VAO);
		if (VBO) glDeleteBuffers(1, &VBO);
		if (EBO) glDeleteBuffers(1, &EBO);
	}
	void setup() {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glBindVertexArray(0);
	}
	void draw() const {
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, idx.size(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
};

// Model loader
struct Model {
	Mesh mesh;
	bool load(const std::string &fn) {
		std::ifstream f(fn);
		if (!f.is_open()) return false;
		std::vector<glm::vec3> v, n;
		std::vector<unsigned> vi, ni;
		std::string l;
		while (std::getline(f, l)) {
			std::istringstream s(l);
			std::string t;
			s >> t;
			if (t == "v") {
				glm::vec3 p;
				s >> p.x >> p.y >> p.z;
				v.push_back(p);
			} else if (t == "vn") {
				glm::vec3 p;
				s >> p.x >> p.y >> p.z;
				n.push_back(p);
			} else if (t == "f") {
				for (int i = 0; i < 3; ++i) {
					std::string w;
					s >> w;
					size_t p1 = w.find('/'), p2 = w.find('/', p1 + 1);
					unsigned vi_ = std::stoi(w.substr(0, p1)) - 1, ni_ = vi_;
					if (p2 != std::string::npos && p2 + 1 < w.size()) ni_ = std::stoi(w.substr(p2 + 1)) - 1;
					vi.push_back(vi_);
					ni.push_back(ni_);
				}
			}
		}
		if (v.empty()) return false;
		glm::vec3 min = v[0], max = v[0];
		for (auto &p : v) {
			min = glm::min(min, p);
			max = glm::max(max, p);
		}
		glm::vec3 c = (min + max) * 0.5f, sz = max - min;
		float sc = 2.0f / glm::max(sz.x, glm::max(sz.y, sz.z));
		mesh.verts.clear();
		mesh.idx.clear();
		for (size_t i = 0; i < vi.size(); ++i) {
			glm::vec3 p = (v[vi[i]] - c) * sc;
			mesh.verts.insert(mesh.verts.end(), {p.x, p.y, p.z});
			glm::vec3 norm = ni[i] < n.size() ? n[ni[i]] : glm::normalize(p);
			mesh.verts.insert(mesh.verts.end(), {norm.x, norm.y, norm.z});
			mesh.idx.push_back(i);
		}
		mesh.setup();
		return true;
	}
	void draw() const { mesh.draw(); }
};

// Motion controller
enum OrientationType {
	Euler,
	Quaternion
};
enum InterpType {
	CatmullRom,
	BSpline
};

struct Keyframe {
	glm::vec3 position;
	glm::vec3 euler; // Euler angles in radians
	glm::quat quat;  // Quaternion
};

class MotionController {
  public:
	std::vector<Keyframe> keys;
	OrientationType orientType = OrientationType::Euler;
	InterpType interpType = InterpType::CatmullRom;

	// Add a keyframe (Euler or Quaternion)
	void addKey(const glm::vec3 &pos, const glm::vec3 &euler) {
		Keyframe k;
		k.position = pos;
		k.euler = euler;
		k.quat = glm::quat(euler);
		keys.push_back(k);
	}
	void addKey(const glm::vec3 &pos, const glm::quat &quat) {
		Keyframe k;
		k.position = pos;
		k.quat = quat;
		k.euler = glm::eulerAngles(quat);
		keys.push_back(k);
	}

	// Interpolate position
	glm::vec3 interpPos(float t) const {
		int N = keys.size();
		if (N < 2) return N ? keys[0].position : glm::vec3(0);
		float ft = t * (N - 1);
		int idx = glm::clamp(int(ft), 0, N - 2);
		float localT = ft - idx;
		// Get control points for spline
		auto get = [&](int i) { return keys[glm::clamp(i, 0, N - 1)].position; };
		if (interpType == InterpType::CatmullRom) {
			glm::vec3 p0 = get(idx - 1), p1 = get(idx), p2 = get(idx + 1), p3 = get(idx + 2);
			return 0.5f *
			       ((2.0f * p1) + (-p0 + p2) * localT + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * localT * localT +
			        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * localT * localT * localT);
		} else { // B-spline
			glm::vec3 p0 = get(idx - 1), p1 = get(idx), p2 = get(idx + 1), p3 = get(idx + 2);
			float t2 = localT * localT, t3 = t2 * localT;
			return ((-p0 + 3.0f * p1 + 3.0f * p2 - p3) * t3 + (3.0f * p0 - 6.0f * p1 + 3.0f * p2) * t2 +
			        (-3.0f * p0 + 3.0f * p2) * localT + (p0 + 4.0f * p1 + p2)) /
			       6.0f;
		}
	}

	// Interpolate orientation
	glm::quat interpQuat(float t) const {
		int N = keys.size();
		if (N < 2) return N ? keys[0].quat : glm::quat();
		float ft = t * (N - 1);
		int idx = glm::clamp(int(ft), 0, N - 2);
		float localT = ft - idx;
		if (orientType == OrientationType::Quaternion) {
			glm::quat q1 = keys[idx].quat, q2 = keys[idx + 1].quat;
			return glm::slerp(q1, q2, localT);
		} else {
			glm::vec3 e1 = keys[idx].euler, e2 = keys[idx + 1].euler;
			glm::vec3 e = glm::mix(e1, e2, localT);
			return glm::quat(e);
		}
	}

	// Get transform matrix at t (0..1)
	glm::mat4 getTransform(float t) const {
		glm::vec3 pos = interpPos(t);
		glm::quat rot = interpQuat(t);
		glm::mat4 T = glm::translate(glm::mat4(1), pos);
		glm::mat4 R = glm::mat4_cast(rot);
		return T * R;
	}
};

// Globals
int W = 600, H = 600;
int angle = 0;
const int FPS = 60;
std::chrono::steady_clock::time_point lastTime;
std::unique_ptr<Shader> shader;
std::unique_ptr<Model> model;
glm::mat4 modelMat = glm::mat4(1.0f);
MotionController motion;
float motionTime = 0;

// Init
void init() {
	shader = std::make_unique<Shader>(vsSrc, fsSrc);
	model = std::make_unique<Model>();
	if (!model->load("teapot.obj")) std::cerr << "Failed to load model.\n";
	glClearDepth(1.0f);
	lastTime = std::chrono::steady_clock::now();

	motion.orientType = OrientationType::Quaternion; // or Euler
	motion.interpType = InterpType::CatmullRom;      // or BSpline

	motion.addKey(glm::vec3(0, 0, 0), glm::quat(glm::vec3(0, 0, 0)));
	motion.addKey(glm::vec3(2, 0, 0), glm::quat(glm::vec3(0, glm::radians(90.0f), 0)));
	motion.addKey(glm::vec3(2, 2, 0), glm::quat(glm::vec3(glm::radians(90.0f), glm::radians(90.0f), 0)));
	motion.addKey(glm::vec3(0, 2, 0), glm::quat(glm::vec3(glm::radians(180.0f), 0, 0)));
}

// animation happens here
void update() {
	auto now = std::chrono::steady_clock::now();
	static const int frameTime = 1000 / FPS;
	if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count() >= frameTime) {
		// 	angle = (angle + 5) % 360;
		// 	modelMat = glm::rotate(glm::mat4(1), glm::radians(float(angle)), glm::vec3(0, 1, 0));
		motionTime += 0.01f; // speed
		if (motionTime > 1.0f) motionTime = 0.0f;
		modelMat = motion.getTransform(motionTime);
		lastTime = now;
	}
}

void render() {
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	shader->use();
	shader->set("lightPos", glm::vec3(5, 5, 5));
	shader->set("lightAmbient", glm::vec3(0.4f));
	shader->set("lightDiffuse", glm::vec3(0.3f));
	shader->set("lightSpecular", glm::vec3(0.4f));
	shader->set("materialAmbient", glm::vec3(0.11f, 0.06f, 0.11f));
	shader->set("materialDiffuse", glm::vec3(0.43f, 0.47f, 0.54f));
	shader->set("materialSpecular", glm::vec3(0.33f, 0.33f, 0.52f));
	shader->set("materialEmission", glm::vec3(0.1f, 0, 0.1f));
	shader->set("materialShininess", 10.0f);
	shader->set("viewPos", glm::vec3(0, 0, 5));
	glm::mat4 view = glm::translate(glm::mat4(1), glm::vec3(0, 0, -5));
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), float(W) / H, 1.0f, 2000.0f);
	shader->set("model", modelMat);
	shader->set("view", view);
	shader->set("projection", proj);
	shader->set("normalMatrix", glm::transpose(glm::inverse(glm::mat3(modelMat))));
	model->draw();
}

// Input/resize
void key(GLFWwindow *w, int k, int, int, int) {
	if (k == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, 1);
}
void resize(GLFWwindow *, int w, int h) {
	W = w;
	H = h;
	glViewport(0, 0, w, h);
}

// Main
int main() {
	if (!glfwInit()) return -1;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow *win = glfwCreateWindow(W, H, "OpenGLProject01", NULL, NULL);
	if (!win) {
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(win);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
	init();
	glfwSetKeyCallback(win, key);
	glfwSetFramebufferSizeCallback(win, resize);
	while (!glfwWindowShouldClose(win)) {
		update();
		render();
		glfwSwapBuffers(win);
		glfwPollEvents();
	}
	glfwTerminate();
	return 0;
}