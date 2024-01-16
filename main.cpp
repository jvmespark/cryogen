//#define GLEW_STATIC
//#include <gl/glew.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <iostream>

#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
using namespace std;

#include "Shader.h"
#include "Camera.h"
#include "TextureManager.h"
#include "enemy.h"

const GLuint WIDTH = 1280, HEIGHT = 720;

// one cube vertices
GLfloat vertices[] = {
	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
	-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
	-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

	-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	-0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
	 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

	-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	-0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
	-0.5f,  0.5f, -0.5f,  0.0f, 1.0f
};

void keysProcess();
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

GLuint VAO, VBO;
Shader ourShader;
GLFWwindow* window;
int screenWidth, screenHeight;

void glInit () {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	window = glfwCreateWindow(WIDTH, HEIGHT, "doom-like", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	glfwGetFramebufferSize(window, &screenWidth, &screenHeight);
	glViewport(0, 0, screenWidth, screenHeight);
	glEnable(GL_DEPTH_TEST);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetKeyCallback(window, key_callback);
}

void vaoInit() {
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);
}

void vboInit() {
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), static_cast<GLvoid *>(nullptr));
	glEnableVertexAttribArray(0);
	
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), reinterpret_cast<GLvoid *>(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);
}

int port=8080;
int newSd;
int clientSd;
int serverSd;

void netInit(int p1Port, int p2Port, char* serverIp) {
	// get port from terminal argv, later switch to docker
	// set up the server (p1)
	// handshake to client (p2)
		// continue to try to connect to the other player
		// once connnected, accept the ping

	sockaddr_in servAddr;
    bzero((char*)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(p1Port);

	serverSd = socket(AF_INET, SOCK_STREAM, 0);
    //bind the socket to its local address
    int bindStatus = bind(serverSd, (struct sockaddr*) &servAddr, sizeof(servAddr));

    //listen for up to 5 requests at a time
    listen(serverSd, 5);
    //receive a request from client using accept
    //we need a new address to connect with the client
    sockaddr_in newSockAddr;
    socklen_t newSockAddrSize = sizeof(newSockAddr);

	// handshake to client (p2), continue to try to connect to the other player
	//setup a socket and connection tools 
	// todo get the local lan ipv4 address and make serverIp here
	//char* serverIp = "127.0.0.1";
    struct hostent* host = gethostbyname(serverIp); 
    sockaddr_in sendSockAddr;   
    bzero((char*)&sendSockAddr, sizeof(sendSockAddr)); 
    sendSockAddr.sin_family = AF_INET; 
    sendSockAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
    sendSockAddr.sin_port = htons(p2Port);
    clientSd = socket(AF_INET, SOCK_STREAM, 0);

    // keep trying to connect
	int successfulConnnect = connect(clientSd, (sockaddr*) &sendSockAddr, sizeof(sendSockAddr));
    while (successfulConnnect < 0) {
		std::cout<<"Connecting ..."<<std::endl;
		successfulConnnect = connect(clientSd, (sockaddr*) &sendSockAddr, sizeof(sendSockAddr));
	}
    cout << "Connected to the server!" << endl;

	//accept, create a new socket descriptor to 
    //handle the new connection with client
	newSd = accept(serverSd, (sockaddr *)&newSockAddr, &newSockAddrSize);
}

// texture
float texBlend = 0.5f;
const int texCount = 2;
char* images[texCount] = { "ok.png", "awesomeface.png" };
GLuint textures[texCount];

void loadTextures() {
	TextureManager::Inst()->LoadTexture(images[0], 0, GL_RGBA, GL_RGBA, 0, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	TextureManager::Inst()->LoadTexture(images[1], 1, GL_RGBA, GL_RGBA, 0, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void bindTextures() {
	glActiveTexture(GL_TEXTURE0);
	TextureManager::Inst()->BindTexture(0);
	glUniform1i(glGetUniformLocation(ourShader.Program, "ourTexture1"), 0);

	glActiveTexture(GL_TEXTURE1);
	TextureManager::Inst()->BindTexture(1);
	glUniform1i(glGetUniformLocation(ourShader.Program, "ourTexture2"), 1);

	glUniform1f(glGetUniformLocation(ourShader.Program, "texBlend"), texBlend);
}

// transform
glm::mat4 model, view, proj;
GLuint modelLoc, viewLoc, projLoc;
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

void initTransform() {
	modelLoc = glGetUniformLocation(ourShader.Program, "model");
	viewLoc = glGetUniformLocation(ourShader.Program, "view");
	projLoc = glGetUniformLocation(ourShader.Program, "proj");
}

GLfloat currentTime = 0.0f, deltaTime = 0.0f, lastFrame = 0.0f;
int main(int argc, char* argv[]) {	
	if (argc != 4) {
		std::cerr<<"Needs p1Port, p2Port and ip"<<std::endl;
		exit(0);
	}
	int p1Port = atoi(argv[1]);
	int p2Port = atoi(argv[2]);
	char *serverIp = argv[3];
	netInit(p1Port, p2Port, serverIp);

	glInit();
	vaoInit();
	vboInit();

	ourShader = Shader("default.vs", "default.frag");
	loadTextures();
	initTransform();
	
	// send intial data
	string x = to_string(camera.Position.x);
	string y = to_string(camera.Position.y);
	string z = to_string(camera.Position.z);
	string msgStr = x + ',' + y + ',' + z;
	send(clientSd, msgStr.data(), msgStr.size(), 0);

	while (!glfwWindowShouldClose(window)) {
		// calculate deltaTime
		currentTime = glfwGetTime();
		deltaTime = currentTime - lastFrame;
		lastFrame = currentTime;

		// get network data
		std::string posdata;
		char msg[256];
		int n = recv(newSd, msg, sizeof(msg), 0);
		posdata.append(msg, msg + n);
		vector<float> posarr;
		std::string delimiter = ",";
		size_t pos = 0;
		std::string token;
		while ((pos = posdata.find(delimiter)) != std::string::npos) {
			token = posdata.substr(0, pos);
			posarr.push_back(std::stof(token));
			posdata.erase(0, pos + delimiter.length());
		}
		posarr.push_back(std::stof(posdata));
		glm::vec3 enemyPos = glm::vec3(posarr[0],posarr[1],posarr[2]);

		// Start Render
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		keysProcess();
		bindTextures();
		ourShader.Use();

		glBindVertexArray(VAO);

		view = glm::mat4(1.0f);
		view = camera.GetViewMatrix();

		proj = glm::mat4(1.0f);
		proj = glm::perspective(camera.Zoom, float(screenWidth) / float(screenHeight), 0.1f, 100.0f);

		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));

		// stream position data into drawEnemy every frame
		drawEnemy(model, modelLoc, enemyPos);
		glDrawArrays(GL_TRIANGLES, 0, 36);

		glBindVertexArray(0);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// close ports
	close(clientSd);
	close(newSd);
	close(serverSd);

	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);

	TextureManager::Inst()->UnloadAllTextures();
	glfwTerminate();

	return 0;
}

// input handle
bool keys[1024];

void keysProcess() {
	// send positon data
	string x = to_string(camera.Position.x);
	string y = to_string(camera.Position.y);
	string z = to_string(camera.Position.z);
	string msgStr = x + ',' + y + ',' + z;
	send(clientSd, msgStr.data(), msgStr.size(), 0);

	float cameraSpeed = 5.0f * deltaTime;

	if (keys[GLFW_KEY_ESCAPE])
		glfwSetWindowShouldClose(window, GL_TRUE);

	if (keys[GLFW_KEY_W])
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (keys[GLFW_KEY_S])
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (keys[GLFW_KEY_A])
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (keys[GLFW_KEY_D])
		camera.ProcessKeyboard(RIGHT, deltaTime);
	if (keys[GLFW_KEY_E])
		camera.ProcessKeyboard(UP, deltaTime);
	if (keys[GLFW_KEY_Q])
		camera.ProcessKeyboard(DOWN, deltaTime);
	
	if (keys[GLFW_KEY_L])
		camera.FPS_Camera = !camera.FPS_Camera;

	if (keys[GLFW_KEY_UP])
	{
		if (texBlend >= 1.0f) return;
		texBlend += 0.01f;
	}
	if (keys[GLFW_KEY_DOWN])
	{
		if (texBlend <= 0.0f) return;
		texBlend -= 0.01f;
	}
}

bool firstMouse = true;
GLfloat lastX = WIDTH / 2;
GLfloat	lastY = HEIGHT / 2;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	GLfloat xoffset = xpos - lastX;
	GLfloat yoffset = lastY - ypos;
	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	camera.ProcessMouseScroll(yoffset);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
	if (action == GLFW_PRESS)
		keys[key] = true;
	else if (action == GLFW_RELEASE)
		keys[key] = false;
}