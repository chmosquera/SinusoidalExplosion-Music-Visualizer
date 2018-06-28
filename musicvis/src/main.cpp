/*
musicvisualizer base code
by Christian Eckhardt 2018
with some code snippets from Ian Thomas Dunn and Zoe Wood, based on their CPE CSC 471 base code
On Windows, it whould capture "what you here" automatically, as long as you have the Stereo Mix turned on!! (Recording devices -> activate)
*/

#include <iostream>
#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <thread>
#include <math.h>
#include <algorithm>   
#include <fstream>
#include <iostream>

#include "fftw/fftw3.h" 
#include "stb_image.h"
#include "GLSL.h"
#include "Program.h"
#include "MatrixStack.h"
#include "recordAudio.h"
#include "WindowManager.h"
#include "Shape.h"

#define MESHSIZE 1000
#define TEXSIZE 256  
#define FFTW_ESTIMATEE (1U << 6)

using namespace std;
using namespace glm;

extern captureAudio actualAudioData;
extern int running;

BYTE texels[TEXSIZE*TEXSIZE*4];
int renderstate = 1;//2..grid
shared_ptr<Shape> sky_sphere, shape, cube, plane;

float ampsList_LO[TEXSIZE];
float ampsList_HI[TEXSIZE];
float freqList_LO[TEXSIZE];
float freqList_HI[TEXSIZE];


fftw_complex * fft(int &length)
{
	static fftw_complex *out = NULL;
	if (out)delete[] out;
	int N = pow(2, 10);
	BYTE data[MAXS];
	int size = 0;
	actualAudioData.readAudio(data, size);
	length = size / 8;
	if (size == 0)
		return NULL;

	double *samples = new double[length];
	for (int ii = 0; ii < length; ii++)
	{
		float *f = (float*)&data[ii * 8];
		samples[ii] = (double)(*f);
	}
	out = new fftw_complex[length];
	fftw_plan p;
	fftw_complex fout[10000];
	p = fftw_plan_dft_r2c_1d(length, samples, out, FFTW_ESTIMATEE);

	fftw_execute(p);
	fftw_destroy_plan(p);

	return out;
}
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

BYTE delayfilter(BYTE old, BYTE actual, float mul)
{
	float fold = (float)old;
	float factual = (float)actual;
	float fres = fold - (fold - factual) / mul;
	if (fres > 255) fres = 255;
	else if (fres < 0)fres = 0;
	return (BYTE)fres;
}

void write_to_tex(GLuint texturebuf,int resx,int resy)
{
	int length;

	static float arg = 0;
	arg += 0.1;
	float erg = sin(arg) + 1;
	erg *= 100;
	int y = 0;
	fftw_complex *outfft = fft(length);				// array 
	float fm = 0;

	float dataSize = resx;

	// print all values into a file?
	fstream file;
	file.open("../resources/soundData.txt", ios::out);	// ---------- open file | write
	if (!file.is_open()) {
		cout << "Warning: Could not open file - soundData.txt" << endl;
	} 
	file << "freq" << '\t' << "hi_freq" << '\t' << "lo_amplitude" << '\t' << "hi_amplitude" << endl;

	//low
	for (int x = 0; x < resx; ++x)					// x accesses each frequency, resx depends on TEXSIZE - figure out a value for texsize 
	{
		if (x >= length / 2)break;
		float fftd = sqrt(outfft[x][0] * outfft[x][0] + outfft[x][1] * outfft[x][1]);			/// fftd - amplitude
		fm = max(fm, abs(fftd));							// what're they doing with fm?
		BYTE oldval = texels[x * 4 + y * resx * 4 + 0];
		texels[x * 4 + y * resx * 4 + 0] = delayfilter(oldval, (BYTE)(fftd*60.0), 15);
		texels[x * 4 + y * resx * 4 + 1] = (BYTE)erg;
		texels[x * 4 + y * resx * 4 + 2] = (BYTE)erg;
		texels[x * 4 + y * resx * 4 + 3] = (BYTE)erg;

		ampsList_LO[x] = 10.0 * log(fftd + 1.0);
		freqList_LO[x] = outfft[x][1];

		// populate array of amplitudes && frequencies
		//if (x < (0.10 *resx)) {
		//	ampsList_LO[x] = 10.0 * log(fftd + 1.0);
		//	freqList_LO[x] = outfft[x][1];
		//} 
		//if (x > (0.30 * resx)) {
		//	ampsList_HI[x] = 10.0 * log(fftd + 1.0);
		//	freqList_HI[x] = outfft[x][1];
		//}
	}

	//high
	for (int y = 0; y < resx; ++y)
	{
		if ((y+resx) >= length / 2)break;
		float fftd = sqrt(outfft[y + resx][0] * outfft[y + resx][0] + outfft[y + resx][1] * outfft[y + resx][1]);
		//cout << "high amp: " << fftd << endl;
		fm = max(fm, abs(fftd));
		BYTE oldval = texels[y*resx * 4 + 0];
		texels[ y*resx * 4 + 0] = delayfilter(oldval, (BYTE)(fftd*60.0), 15);
		texels[ y*resx * 4 + 1] = (BYTE)erg;
		texels[ y*resx * 4 + 2] = (BYTE)erg;
		texels[ y*resx * 4 + 3] = (BYTE)erg;

		// populate array of high amplitudes && high frequencies
		ampsList_HI[y] = fm;
		freqList_HI[y] = outfft[y + resx][1];
	}
	
	for (int y = 1; y < resy; y++) {			// what does erg do?
		for (int x = 1; x < resx; ++x) {
			int erg = (int)texels[(y - 1)*resx * 4] + (int)texels[x * 4];
			erg /= 3;
			texels[x * 4 + y * resx * 4] = erg;
		}
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texturebuf);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXSIZE, TEXSIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
	glGenerateMipmap(GL_TEXTURE_2D);

}
double get_last_elapsed_time()
{
	static double lasttime = glfwGetTime();
	double actualtime =glfwGetTime();
	double difference = actualtime- lasttime;
	lasttime = actualtime;
	return difference;
}
class camera
{
public:
	glm::vec3 pos, rot;
	int w, a, s, d, q, e, z, c;
	camera()
	{
		w = a = s = d = q = e = z = c = 0;
		pos = rot = glm::vec3(0, 0, 0);
	}
	glm::mat4 process(double ftime)
	{
		float speed = 0;
		if (w == 1)
		{
			speed = 90*ftime;
		}
		else if (s == 1)
		{
			speed = -90*ftime;
		}
		float yangle=0;
		if (a == 1)
			yangle = -3*ftime;
		else if(d==1)
			yangle = 3*ftime;
		rot.y += yangle;
		float zangle = 0;
		if (q == 1)
			zangle = -3 * ftime;
		else if (e == 1)
			zangle = 3 * ftime;
		rot.z += zangle;
		float xangle = 0;
		if (z == 1)
			xangle = -0.3 * ftime;
		else if (c == 1)
			xangle = 0.3 * ftime;
		rot.x += xangle;

		glm::mat4 R = glm::rotate(glm::mat4(1), rot.y, glm::vec3(0, 1, 0));
		glm::mat4 Rz = glm::rotate(glm::mat4(1), rot.z, glm::vec3(0, 0, 1));
		glm::mat4 Rx = glm::rotate(glm::mat4(1), rot.x, glm::vec3(1, 0, 0));
		glm::vec4 dir = glm::vec4(0, 0, speed,1);
		R = Rx * Rz * R;
		dir = dir*R;
		pos += glm::vec3(dir.x, dir.y, dir.z);
		glm::mat4 T = glm::translate(glm::mat4(1), pos);
		return R*T;
	}
};

camera mycam;

class Application : public EventCallbacks
{

public:

	WindowManager * windowManager = nullptr;

	// Our shader program
	std::shared_ptr<Program> prog, heightshader, skyprog, linesshader, objprog, tessprog, sideprog;

	// Contains vertex information for OpenGL
	GLuint VertexArrayID;

	// Data necessary to give our box to OpenGL
	GLuint MeshPosID, MeshTexID, IndexBufferIDBox;

	//texture data
	GLuint Texture,AudioTex, AudioTexBuf;
	GLuint Texture2,HeightTex;

	// Toggles - sent to the shaders
	int rSpeaker = 0, lSpeaker = 0;

	// transitioning objects
	//float initPos[2], finalPos[2];
	glm::vec2 finalPos = glm::vec2(0.0);
	glm::vec2 initPos = glm::vec2(0.0);

	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GL_TRUE);
		if (key == GLFW_KEY_W && action == GLFW_PRESS) mycam.w = 1;
		if (key == GLFW_KEY_W && action == GLFW_RELEASE) mycam.w = 0;
		if (key == GLFW_KEY_S && action == GLFW_PRESS)mycam.s = 1;
		if (key == GLFW_KEY_S && action == GLFW_RELEASE) mycam.s = 0;
		if (key == GLFW_KEY_A && action == GLFW_PRESS) mycam.a = 1;
		if (key == GLFW_KEY_A && action == GLFW_RELEASE) mycam.a = 0;
		if (key == GLFW_KEY_D && action == GLFW_PRESS)mycam.d = 1;
		if (key == GLFW_KEY_D && action == GLFW_RELEASE)mycam.d = 0;
		if (key == GLFW_KEY_Q && action == GLFW_PRESS)mycam.q = 1;
		if (key == GLFW_KEY_Q && action == GLFW_RELEASE) mycam.q = 0;
		if (key == GLFW_KEY_E && action == GLFW_PRESS) mycam.e = 1;
		if (key == GLFW_KEY_E && action == GLFW_RELEASE) mycam.e = 0;
		if (key == GLFW_KEY_Z && action == GLFW_PRESS)mycam.z = 1;
		if (key == GLFW_KEY_Z && action == GLFW_RELEASE)mycam.z = 0;
		if (key == GLFW_KEY_C && action == GLFW_PRESS)mycam.c = 1;
		if (key == GLFW_KEY_C && action == GLFW_RELEASE)mycam.c = 0;
		//if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE)
		//{
		//	if (renderstate == 1)
		//		renderstate = 2;
		//	else
		//		renderstate = 1;
		//}
		if (key == GLFW_KEY_RIGHT_SHIFT && action == GLFW_PRESS) rSpeaker = 1;
		if (key == GLFW_KEY_RIGHT_SHIFT && action == GLFW_RELEASE) rSpeaker = 0;
		if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_PRESS) lSpeaker = 1;
		if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_RELEASE) lSpeaker = 0;
		
	}

	float p2wX(int pX, int width) {

		float wX = pX / (width / 2.0) - 1.0;
		return wX;
	}
	float p2wY(int pY, int height) {

		// fix y-coord to be standard pixel coords (change sign)
		pY = 0 - pY;

		float wY = pY / (height / 2.0) + 1.0;
		return wY;
	}


	// callback for the mouse when clicked move the triangle when helper functions
	// written
	void mouseCallback(GLFWwindow *window, int button, int action, int mods)
	{
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);

		if (action == GLFW_PRESS)
		{
			double x = 0, y = 0;
			glfwGetCursorPos(window, &x, &y);

			// let initPos be the previous finalPos
			initPos = finalPos;

			finalPos.x = p2wX(x, width);
			finalPos.y = p2wY(y, height);
		}
	}

	//if the window is resized, capture the new size and reset the viewport
	void resizeCallback(GLFWwindow *window, int in_width, int in_height)
	{
		//get the window size - may be different then pixels for retina
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
	}

	void init_mesh()
	{

		//generate the VAO
		glGenVertexArrays(1, &VertexArrayID);
		glBindVertexArray(VertexArrayID);

		//generate vertex buffer to hand off to OGL
		glGenBuffers(1, &MeshPosID);
		glBindBuffer(GL_ARRAY_BUFFER, MeshPosID);
		glm::vec3 *vertices = new glm::vec3[MESHSIZE * MESHSIZE * 6];
		for (int x = 0; x < MESHSIZE; x++)
		{
			for (int z = 0; z < MESHSIZE; z++)
			{
				vertices[x * 6 + z*MESHSIZE * 6 + 0] = vec3(0.0, 0.0, 0.0) + vec3(x, 0, z);//LD
				vertices[x * 6 + z*MESHSIZE * 6 + 1] = vec3(1.0, 0.0, 0.0) + vec3(x, 0, z);//RD
				vertices[x * 6 + z*MESHSIZE * 6 + 2] = vec3(1.0, 0.0, 1.0) + vec3(x, 0, z);//RU
				vertices[x * 6 + z*MESHSIZE * 6 + 3] = vec3(0.0, 0.0, 0.0) + vec3(x, 0, z);//LD
				vertices[x * 6 + z*MESHSIZE * 6 + 4] = vec3(1.0, 0.0, 1.0) + vec3(x, 0, z);//RU
				vertices[x * 6 + z*MESHSIZE * 6 + 5] = vec3(0.0, 0.0, 1.0) + vec3(x, 0, z);//LU

			}
	
		}
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * MESHSIZE * MESHSIZE * 6, vertices, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		delete[] vertices;
		//tex coords
		float t = 1. / MESHSIZE;
		vec2 *tex = new vec2[MESHSIZE * MESHSIZE * 6];
		for (int x = 0; x<MESHSIZE; x++)
			for (int y = 0; y < MESHSIZE; y++)
			{
				tex[x * 6 + y*MESHSIZE * 6 + 0] = vec2(0.0, 0.0)+ vec2(x, y)*t;	//LD
				tex[x * 6 + y*MESHSIZE * 6 + 1] = vec2(t, 0.0)+ vec2(x, y)*t;	//RD
				tex[x * 6 + y*MESHSIZE * 6 + 2] = vec2(t, t)+ vec2(x, y)*t;		//RU
				tex[x * 6 + y*MESHSIZE * 6 + 3] = vec2(0.0, 0.0) + vec2(x, y)*t;	//LD
				tex[x * 6 + y*MESHSIZE * 6 + 4] = vec2(t, t) + vec2(x, y)*t;		//RU
				tex[x * 6 + y*MESHSIZE * 6 + 5] = vec2(0.0, t)+ vec2(x, y)*t;	//LU
			}
		glGenBuffers(1, &MeshTexID);
		//set the current state to focus on our vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, MeshTexID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * MESHSIZE * MESHSIZE * 6, tex, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		delete[] tex;
		glGenBuffers(1, &IndexBufferIDBox);
		//set the current state to focus on our vertex buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferIDBox);
		GLuint *elements = new GLuint[MESHSIZE * MESHSIZE * 8];
	//	GLuint ele[10000];
		int ind = 0,i=0;
		for (i = 0; i<(MESHSIZE * MESHSIZE * 8); i+=8, ind+=6)
			{
			elements[i + 0] = ind + 0;
			elements[i + 1] = ind + 1;
			elements[i + 2] = ind + 1;
			elements[i + 3] = ind + 2;
			elements[i + 4] = ind + 2;
			elements[i + 5] = ind + 5;
			elements[i + 6] = ind + 5;
			elements[i + 7] = ind + 0;
			}			
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)*MESHSIZE * MESHSIZE * 8, elements, GL_STATIC_DRAW);
		delete[] elements;
		glBindVertexArray(0);
	}

	// Read in meshes
	void initShape()
	{
		string resourceDirectory = "../resources";
		// Initialize mesh.
		shape = make_shared<Shape>();
		shape->loadMesh(resourceDirectory + "/sphere50.obj");
		shape->resize();
		shape->init();

		cube = make_shared<Shape>();
		cube->loadMesh(resourceDirectory + "/cube.obj");
		cube->resize();
		cube->init();

		plane = make_shared<Shape>();
		plane->loadMesh(resourceDirectory + "/sphere.obj");
		plane->resize();
		plane->init();
	}

	/*Note that any gl calls must always happen after a GL state is initialized */
	void initGeom()
	{
		//initialize the net mesh
		init_mesh();

		string resourceDirectory = "../resources";

		// Initialize mesh.
		sky_sphere = make_shared<Shape>();
		sky_sphere->loadMesh(resourceDirectory + "/sphere.obj");
		sky_sphere->resize();
		sky_sphere->init();



		int width, height, channels;
		char filepath[1000];

		//texture 1
		string str = resourceDirectory + "/sky.jpg";
		strcpy(filepath, str.c_str());
		unsigned char* data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &Texture);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		//texture 2
		str = resourceDirectory + "/sky1.jpg";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &Texture2);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, Texture2);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		//texture 3
		str = resourceDirectory + "/height.png";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &HeightTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, HeightTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);


		//[TWOTEXTURES]
		//set the 2 textures to the correct samplers in the fragment shader:
		GLuint Tex1Location = glGetUniformLocation(prog->pid, "tex");//tex, tex2... sampler in the fragment shader
		GLuint Tex2Location = glGetUniformLocation(prog->pid, "tex2");
		// Then bind the uniform samplers to texture units:
		glUseProgram(prog->pid);
		glUniform1i(Tex1Location, 0);
		glUniform1i(Tex2Location, 1);

		Tex1Location = glGetUniformLocation(heightshader->pid, "tex");//tex, tex2... sampler in the fragment shader
		Tex2Location = glGetUniformLocation(heightshader->pid, "tex2");
		glUseProgram(heightshader->pid);
		glUniform1i(Tex1Location, 0);
		glUniform1i(Tex2Location, 1);

		Tex1Location = glGetUniformLocation(skyprog->pid, "tex");//tex, tex2... sampler in the fragment shader
		Tex2Location = glGetUniformLocation(skyprog->pid, "tex2");
		glUseProgram(skyprog->pid);
		glUniform1i(Tex1Location, 0);
		glUniform1i(Tex2Location, 1);
		
		Tex1Location = glGetUniformLocation(linesshader->pid, "tex");//tex, tex2... sampler in the fragment shader
		Tex2Location = glGetUniformLocation(linesshader->pid, "tex2");
		glUseProgram(linesshader->pid);
		glUniform1i(Tex1Location, 0);
		glUniform1i(Tex2Location, 1);

		// dynamic audio texture
		
/*
		int datasize = TEXSIZE *TEXSIZE * 4 * sizeof(GLfloat);
		glGenBuffers(1, &AudioTexBuf);
		glBindBuffer(GL_TEXTURE_BUFFER, AudioTexBuf);
		glBufferData(GL_TEXTURE_BUFFER, datasize, NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_TEXTURE_BUFFER, 0);

		glGenTextures(1, &AudioTex);
		glBindTexture(GL_TEXTURE_BUFFER, AudioTex);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, AudioTexBuf);
		glBindTexture(GL_TEXTURE_BUFFER, 0);
		//glBindImageTexture(2, AudioTex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
		*/
		for (int ii = 0; ii < TEXSIZE*TEXSIZE * 4; ii++)
			texels[ii] = 0;
		glGenTextures(1, &AudioTex);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, AudioTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXSIZE, TEXSIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
		//glGenerateMipmap(GL_TEXTURE_2D);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		initShape();

	}
	
	//General OGL initialization - set OGL state here
	void init(const std::string& resourceDirectory)
	{
		GLSL::checkVersion();

		// Set background color.
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		
		glEnable(GL_DEPTH_TEST); // Enable z-buffer test.

		// Initialize the GLSL program.
		skyprog = std::make_shared<Program>();
		skyprog->setVerbose(true);
		skyprog->setShaderNames(resourceDirectory + "/sky_vertex.glsl", resourceDirectory + "/sky_fragment.glsl");
		if (!skyprog->init()) {
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		skyprog->addUniform("P");
		skyprog->addUniform("V");
		skyprog->addUniform("M");
		skyprog->addAttribute("vertPos");
		skyprog->addAttribute("vertTex");

		// Initialize the GLSL program.
		prog = std::make_shared<Program>();
		prog->setVerbose(true);
		prog->setShaderNames(resourceDirectory + "/shader_vertex.glsl", resourceDirectory + "/shader_fragment.glsl");
		if (!prog->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		prog->addUniform("P");
		prog->addUniform("V");
		prog->addUniform("M");
		prog->addUniform("campos");
		prog->addAttribute("vertPos");
		prog->addAttribute("vertNor");
		prog->addAttribute("vertTex");

		// Initialize the GLSL program.
		heightshader = std::make_shared<Program>();
		heightshader->setVerbose(true);
		heightshader->setShaderNames(resourceDirectory + "/height_vertex.glsl", resourceDirectory + "/height_frag.glsl", resourceDirectory + "/geometry.glsl");
		if (!heightshader->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		heightshader->addUniform("P");
		heightshader->addUniform("V");
		heightshader->addUniform("M");
		heightshader->addUniform("camoff");
		heightshader->addUniform("campos");
		heightshader->addAttribute("vertPos");
		heightshader->addAttribute("vertTex");
		heightshader->addUniform("bgcolor");
		heightshader->addUniform("renderstate");
		heightshader->addUniform("amplitude");
		heightshader->addUniform("freq");

		// Initialize the GLSL program.
		linesshader = std::make_shared<Program>();
		linesshader->setVerbose(true);
		linesshader->setShaderNames(resourceDirectory + "/lines_height_vertex.glsl", resourceDirectory + "/lines_height_frag.glsl", resourceDirectory + "/lines_geometry.glsl");
		if (!linesshader->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		linesshader->addUniform("P");
		linesshader->addUniform("V");
		linesshader->addUniform("M");
		linesshader->addUniform("camoff");
		linesshader->addUniform("campos");
		linesshader->addAttribute("vertPos");
		linesshader->addAttribute("vertTex");
		linesshader->addUniform("bgcolor");

		
		// Initialize the GLSL program.
		objprog = std::make_shared<Program>();
		objprog->setVerbose(true);
		objprog->setShaderNames(resourceDirectory + "/obj.vert", resourceDirectory + "/obj.frag", resourceDirectory + "/obj.geom");
		objprog->init();
		objprog->addUniform("P");
		objprog->addUniform("V");
		objprog->addUniform("M");
		objprog->addAttribute("vertPos");
		objprog->addAttribute("vertCol");
		objprog->addUniform("amplitude");
		objprog->addUniform("freq");
		objprog->addUniform("time");
		objprog->addUniform("pointA");
		objprog->addUniform("pointB");

		 //Initialize the GLSL program.
		tessprog = std::make_shared<Program>();
		tessprog->setVerbose(true);
		tessprog->setShaderNames(resourceDirectory + "/normal_lines.vert", resourceDirectory + "/normal_lines.frag", resourceDirectory + "/normal_lines.geom");
		tessprog->init();
		tessprog->addUniform("P");
		tessprog->addUniform("V");
		tessprog->addUniform("M");
		tessprog->addAttribute("vertPos");
		tessprog->addAttribute("vertCol");
		tessprog->addUniform("amplitude");
		tessprog->addUniform("freq");
		tessprog->addUniform("time");
		tessprog->addUniform("angle");

		//Initialize the GLSL program.
		sideprog = std::make_shared<Program>();
		sideprog->setVerbose(true);
		sideprog->setShaderNames(resourceDirectory + "/boombox.vert", resourceDirectory + "/boombox.frag", resourceDirectory + "/boombox.geom");
		sideprog->init();
		sideprog->addUniform("P");
		sideprog->addUniform("V");
		sideprog->addUniform("M");
		sideprog->addAttribute("vertPos");
		sideprog->addAttribute("vertCol");
		sideprog->addUniform("amplitude");
		sideprog->addUniform("freq");
		sideprog->addUniform("time");
		sideprog->addUniform("angle");
		
	}


	/****DRAW
	This is the most important function in your program - this is where you
	will actually issue the commands to draw any geometry you have set up to
	draw
	********/
	void render()
	{
		static double count = 0;
		double frametime = get_last_elapsed_time();

		write_to_tex(AudioTex, TEXSIZE, TEXSIZE);			// write audio onto texture
			
		// Get current frame buffer size.
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		float aspect = width/(float)height;
		glViewport(0, 0, width, height);

		// Clear framebuffer.
		//glClearColor(0.8f, 0.8f, 1.0f, 1.0f);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 V, M, P;		
		V = mycam.process(frametime);
		M = glm::mat4(1);
		P = glm::perspective((float)(3.14159 / 4.), (float)((float)width/ (float)height), 0.01f, 100000.0f); 

		/*******************************************/
		/*************** EXPLOSION *****************/
		/*******************************************/

		// all obj transformations
		glm::mat4  TransObj = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -50.0));
		M = TransObj;

		objprog->bind();
		
		// time affects speed of sin wave
		static float time = 0.0;
		time += 0.005;

		/************* FREQUENCY ********************/
		// frequency -- add variance to sin wave speed	
		float freq = 0;
		for (int i = 0; i < sizeof(freqList_LO)/sizeof(*freqList_LO); i++) {
			freq += freqList_LO[i];
		}
		freq /= (sizeof(freqList_LO) / sizeof(*freqList_LO));

			// delay frequency
			static float oldfreq = 0.0;			// added delay filter for change in frequency
			float diff = (freq - oldfreq) / 50.0;		// smaller diff --> softer variance
			float actualfreq = (oldfreq + diff);
			oldfreq = actualfreq;
			float delayfreq = abs(actualfreq + 1.0);

		/************* EXPLODE ********************/
		// amplitude affects explode scale and object scale
		float amplitude = 0.0; float lo_amplitude = 0.0;
		for (int i = 0; i < sizeof(ampsList_LO)/sizeof(*ampsList_LO); i++) {
			amplitude += ampsList_LO[i];
		}
		amplitude /= (sizeof(ampsList_LO) / sizeof(*ampsList_LO));	

			// delay filter
			static float oldamplitude = 0;
			float actualamplitude = oldamplitude + (amplitude - oldamplitude) * .1;
			oldamplitude = actualamplitude;
			float delayamplitude = actualamplitude;

		/************* EXTRAS ********************/
		static float w = 0.0;
		if (freq > 0.0) w += frametime/10.0;
		else if (freq < 0.0) w -= frametime/10.0;
		if (freq > 1.0) {
			w += 5.0 * frametime;
		}
		else if (freq < -1.0) {
			w -= 5.0 * frametime;
		}

		// transform
		mat4 SpinCW = rotate(mat4(1.0), w, vec3(0, 0, 1));
		static float pos = 0.0;
		//cout << "freq: " << freq << "  amplitude: " << delayamplitude << endl;
		pos += (freq/100.0);
		mat4 Side = translate(mat4(1.0), vec3(pos, 0.0, 0.0));

		// scale
		mat4 Scale = scale(mat4(1.0), vec3(delayamplitude));
		M = TransObj *Side* SpinCW * Scale;
		
		glUniformMatrix4fv(objprog->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(objprog->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(objprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		glUniform1f(objprog->getUniform("amplitude"), delayamplitude);
		glUniform1f(objprog->getUniform("freq"), pos);
		glUniform1f(objprog->getUniform("time"), time);
		shape->draw(objprog, false);

		objprog->unbind();

		/************************ Sky Sphere **************************/
		TransObj = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0));
		static float tw = 0.0;
		tw += 0.01 * abs(freq);
	
		SpinCW = rotate(mat4(1.0), tw, vec3(0, 0, 1));

		TransObj = translate(mat4(1.0), -mycam.pos);
		Scale = scale(mat4(1.0), vec3(100.0));
		M = TransObj * SpinCW * Scale;

		tessprog->bind();
		glUniformMatrix4fv(tessprog->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(tessprog->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(tessprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		glUniform1f(tessprog->getUniform("amplitude"), delayamplitude);
		glUniform1f(tessprog->getUniform("freq"), pos);
		glUniform1f(tessprog->getUniform("time"), time);
		glUniform1f(tessprog->getUniform("angle"), tw);
		glUniform1f(tessprog->getUniform("amplitude"), delayamplitude);
		//glUniform1f(tessprog->getUniform("time"), time);

		plane->draw(tessprog, false);
		tessprog->unbind();

		/************** Side Speakers *****************/
		sideprog->bind();

		glUniformMatrix4fv(sideprog->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(sideprog->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(sideprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		glUniform1f(sideprog->getUniform("amplitude"), delayamplitude);
		glUniform1f(sideprog->getUniform("freq"), pos);
		glUniform1f(sideprog->getUniform("time"), time);
		glUniform1f(sideprog->getUniform("amplitude"), delayamplitude);

		// variables
		static float ab = 0.0;
		float r = 20.0;

		float x = r * sin(ab);
		float y = r * cos(ab);

		// transformations 
		//mat4 leftTrans = glm::translate(glm::mat4(1.0f), glm::vec3(-20.0f, 0.0f, -50.0));
		//mat4 rightTrans = glm::translate(glm::mat4(1.0f), glm::vec3(20.0f, 0.0f, -50.0));
		mat4 leftTrans = glm::translate(glm::mat4(1.0f), glm::vec3(-x, -y, -50.0));
		mat4 rightTrans = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, -50.0));
		float lw = pos, rw = -pos;
		SpinCW = rotate(mat4(1.0), lw, vec3(0, 1, 0));
		mat4 SpinCCW = rotate(mat4(1.0), rw, vec3(0, 1, 0));
		Side = translate(mat4(1.0), vec3(0.0, pos, 0.0));
		Scale = scale(mat4(1.0), vec3(2.0 + pos));

		if (lSpeaker && rSpeaker) {
			Scale = scale(mat4(1.0), vec3(delayamplitude/2.0));
			ab += 0.01 * abs(freq);
		}

		cout << "lSpeaker: " << lSpeaker << "  rSPeaker: " << rSpeaker << endl;
		// draw left speaker
		if (lSpeaker) {
			M = leftTrans * Side * SpinCW * Scale;
			glUniformMatrix4fv(sideprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			cube->draw(sideprog, false);

			x = r * sin(ab + 45); y = r * cos(ab + 45);
			leftTrans = glm::translate(glm::mat4(1.0f), glm::vec3(-x, -y, -50.0));
			M = leftTrans * Side * SpinCCW * Scale;
			glUniformMatrix4fv(sideprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			cube->draw(sideprog, false);

			x = r * sin(ab - 45); y = r * cos(ab - 45);
			leftTrans = glm::translate(glm::mat4(1.0f), glm::vec3(-x, -y, -50.0));
			M = leftTrans * Side * SpinCCW * Scale;
			glUniformMatrix4fv(sideprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			cube->draw(sideprog, false);
		}
		else {
			M = leftTrans * Side * Scale;
			glUniformMatrix4fv(sideprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			cube->draw(sideprog, false);
		}

		// draw right speaker
		if (rSpeaker) {
			M = rightTrans * Side * SpinCCW * Scale;
			glUniformMatrix4fv(sideprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			cube->draw(sideprog, false);

			x = r * sin(ab + 45); y = r * cos(ab + 45);
			rightTrans = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, -50.0));
			M = rightTrans * Side * SpinCW * Scale;
			glUniformMatrix4fv(sideprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			cube->draw(sideprog, false);

			x = r * sin(ab - 45); y = r * cos(ab - 45);
			rightTrans = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, -50.0));
			M = rightTrans * Side * SpinCW * Scale;
			glUniformMatrix4fv(sideprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			cube->draw(sideprog, false);
		}
		else {
			M = rightTrans * Side * Scale;
			glUniformMatrix4fv(sideprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			cube->draw(sideprog, false);
		}
		

		sideprog->unbind();
	

		glBindVertexArray(0);
	}

};
//******************************************************************************************
int main(int argc, char **argv)
{
	std::string resourceDir = "../resources"; // Where the resources are loaded from
	if (argc >= 2)
	{
		resourceDir = argv[1];
	}

	Application *application = new Application();

	/* your main will always include a similar set up to establish your window
		and GL context, etc. */
	WindowManager * windowManager = new WindowManager();
	windowManager->init(1920, 1080);
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;

	/* This is the code that will likely change program to program as you
		may need to initialize or set up different data and state */
	// Initialize scene.
	application->init(resourceDir);
	application->initGeom();

	thread t1(start_recording);
	// Loop until the user closes the window.
	while(! glfwWindowShouldClose(windowManager->getHandle()))
	{
		// Render scene.
		application->render();

		// Swap front and back buffers.
		glfwSwapBuffers(windowManager->getHandle());
		// Poll for and process events.
		glfwPollEvents();
	}
	running = FALSE;
	t1.join();

	// Quit program.
	windowManager->shutdown();
	return 0;
}
