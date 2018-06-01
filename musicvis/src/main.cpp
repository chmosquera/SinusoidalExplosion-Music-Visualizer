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
shared_ptr<Shape> sky_sphere, shape;

vector<float> fftd_vector = vector<float>(30);


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

	// ADDED
	//cout << "size: " << outfft.size() << endl;
	//cout << "low: "  << outfft[10] << "	high: " << outfft[100] << endl;

	// END
	float fm = 0;
	//low
	for (int x = 0; x < resx; ++x)					// x accesses each frequency, resx depends on TEXSIZE - figure out a value for texsize 
		{
		if (x >= length/2)break;
		float fftd = sqrt(outfft[x][0]* outfft[x][0]+ outfft[x][1]* outfft[x][1]);			/// fftd - amplitude
		//cout << "low amp: " << fftd << endl;
		fm = max(fm, abs(fftd));
		BYTE oldval = texels[x * 4 + y*resx * 4 + 0];
		texels[x * 4 + y*resx * 4 + 0] = delayfilter(oldval,(BYTE)(fftd*60.0),15);
		texels[x * 4 + y*resx * 4 + 1] = (BYTE)erg;
		texels[x * 4 + y*resx * 4 + 2] = (BYTE)erg;
		texels[x * 4 + y*resx * 4 + 3] = (BYTE)erg;

		// populate array of fftd values
		fftd_vector.erase(fftd_vector.begin());		// remove first element
		fftd_vector.push_back(fftd);				// insert new element

		// test out numbers in vector of fftd
		//for (int i = 0; i < 30; i++) {
		//	cout << fftd_vector[i] << " ";
		//}
		//cout << endl;
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
	}
	
	for (int y = 1; y <resy; y++)
		for (int x = 1; x < resx; ++x)
			{
			int erg = (int)texels[(y - 1)*resx * 4] + (int)texels[x * 4];
			erg /= 3;
			texels[x * 4 + y*resx * 4] = erg;
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
	std::shared_ptr<Program> prog, heightshader, skyprog, linesshader, objprog;

	// Contains vertex information for OpenGL
	GLuint VertexArrayID;

	// Data necessary to give our box to OpenGL
	GLuint MeshPosID, MeshTexID, IndexBufferIDBox;

	//texture data
	GLuint Texture,AudioTex, AudioTexBuf;
	GLuint Texture2,HeightTex;

	// Toggles - sent to the shaders
	int PAUSE = 0, RESET = 0;
	int REVERSE = 1;

	// transitioning objects
	//float initPos[2], finalPos[2];
	glm::vec2 finalPos = glm::vec2(0.0);
	glm::vec2 initPos = glm::vec2(0.0);
	float transClock = 0;

	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
		
		if (key == GLFW_KEY_W && action == GLFW_PRESS)
		{
			mycam.w = 1;
		}
		if (key == GLFW_KEY_W && action == GLFW_RELEASE)
		{
			mycam.w = 0;
		}
		if (key == GLFW_KEY_S && action == GLFW_PRESS)
		{
			mycam.s = 1;
		}
		if (key == GLFW_KEY_S && action == GLFW_RELEASE)
		{
			mycam.s = 0;
		}
		if (key == GLFW_KEY_A && action == GLFW_PRESS)
		{
			mycam.a = 1;
		}
		if (key == GLFW_KEY_A && action == GLFW_RELEASE)
		{
			mycam.a = 0;
		}
		if (key == GLFW_KEY_D && action == GLFW_PRESS)
		{
			mycam.d = 1;
		}
		if (key == GLFW_KEY_D && action == GLFW_RELEASE)
		{
			mycam.d = 0;
		}
		if (key == GLFW_KEY_Q && action == GLFW_PRESS)
		{
			mycam.q = 1;
		}
		if (key == GLFW_KEY_Q && action == GLFW_RELEASE)
		{
			mycam.q = 0;
		}
		if (key == GLFW_KEY_E && action == GLFW_PRESS)
		{
			mycam.e = 1;
		}
		if (key == GLFW_KEY_E && action == GLFW_RELEASE)
		{
			mycam.e = 0;
		}
		if (key == GLFW_KEY_Z && action == GLFW_PRESS)
		{
			mycam.z = 1;
		}
		if (key == GLFW_KEY_Z && action == GLFW_RELEASE)
		{
			mycam.z = 0;
		}
		if (key == GLFW_KEY_C && action == GLFW_PRESS)
		{
			mycam.c = 1;
		}
		if (key == GLFW_KEY_C && action == GLFW_RELEASE)
		{
			mycam.c = 0;
		}
		if (key == GLFW_KEY_R && action == GLFW_PRESS) RESET = !RESET;
		if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE)
		{
			if (renderstate == 1)
				renderstate = 2;
			else
				renderstate = 1;
		}
		
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

			transClock = 0;
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
		shape->loadMesh(resourceDirectory + "/sphere.obj");
		shape->resize();
		shape->init();

	}

	/*Note that any gl calls must always happen after a GL state is initialized */
	void initGeom()
	{
		//initialize the net mesh
		init_mesh();

		string resourceDirectory = "../resources";

		// Initialize mesh.
		sky_sphere = make_shared<Shape>();
		//shape->loadMesh(resourceDirectory + "/t800.obj");
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
		// Enable z-buffer test.
		glEnable(GL_DEPTH_TEST);
		// Initialize the GLSL program.
		skyprog = std::make_shared<Program>();
		skyprog->setVerbose(true);
		skyprog->setShaderNames(resourceDirectory + "/sky_vertex.glsl", resourceDirectory + "/sky_fragment.glsl");
		if (!skyprog->init())
		{
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
		//objprog->addAttribute("vertTex");
		//objprog->addUniform("camoff");
		objprog->addUniform("camPos");
		objprog->addUniform("explode");
		objprog->addUniform("time");
		objprog->addUniform("transClock");
		objprog->addUniform("pointA");
		objprog->addUniform("pointB");
		
		
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
		glClearColor(0.8f, 0.8f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Create the matrix stacks - please leave these alone for now
		
		glm::mat4 V, M, P;
		
		V = mycam.process(frametime);
		M = glm::mat4(1);
		P = glm::perspective((float)(3.14159 / 4.), (float)((float)width/ (float)height), 0.01f, 100000.0f); 

		//animation with the model matrix:
		static float w = 0.0;
		w += 1.0 * frametime;
		float trans = 0;
		w = 0.6;
		glm::mat4 RotateY = glm::rotate(glm::mat4(1.0f), w, glm::vec3(0.0f, 1.0f, 0.0f));
		float angle = 3.1415926/2.0;
		glm::mat4 RotateX = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 TransZ = glm::translate(glm::mat4(1.0f), -mycam.pos);
		glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(0.8f, 0.8f, 0.8f));

		M =  TransZ *RotateY * RotateX * S;

		// Draw sky sphere
		skyprog->bind();

		
		//send the matrices to the shaders
		glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, &P[0][0]);				// why are we sending to prog?
		glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);

		glActiveTexture(GL_TEXTURE0);
		if(renderstate==1) glBindTexture(GL_TEXTURE_2D, Texture);
		else if (renderstate == 2) glBindTexture(GL_TEXTURE_2D, Texture2);
		
		glDisable(GL_DEPTH_TEST);
		sky_sphere->draw(skyprog,FALSE);
		glEnable(GL_DEPTH_TEST);

			heightshader->bind();
			//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glm::mat4 TransY = glm::translate(glm::mat4(1.0f), glm::vec3(-500.0f, -9.0f, -500));
			M = TransY;
			glUniformMatrix4fv(heightshader->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			glUniformMatrix4fv(heightshader->getUniform("P"), 1, GL_FALSE, &P[0][0]);
			glUniformMatrix4fv(heightshader->getUniform("V"), 1, GL_FALSE, &V[0][0]);


			vec3 offset = mycam.pos;
			offset.y = 0;
			offset.x = (int)offset.x;
			offset.z = (int)offset.z;
			offset = vec3(0, 0, 0);
			vec3 bg = vec3(254. / 255., 225. / 255., 168. / 255.);
			if (renderstate == 2)
				bg = vec3(49. / 255., 88. / 255., 114. / 255.);
			glUniform3fv(heightshader->getUniform("camoff"), 1, &offset[0]);
			glUniform3fv(heightshader->getUniform("campos"), 1, &mycam.pos[0]);
			glUniform3fv(heightshader->getUniform("bgcolor"), 1, &bg[0]);
			glUniform1i(heightshader->getUniform("renderstate"), renderstate);

			glBindVertexArray(VertexArrayID);
			glActiveTexture(GL_TEXTURE0);
			//glBindTexture(GL_TEXTURE_2D, HeightTex);
			glBindTexture(GL_TEXTURE_2D, AudioTex);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, HeightTex);
		
			glDrawArrays(GL_TRIANGLES, 0, MESHSIZE*MESHSIZE * 6);			
			heightshader->unbind();

		
		if (renderstate == 2)
		{
			linesshader->bind();
			//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glm::mat4 TransY = glm::translate(glm::mat4(1.0f), glm::vec3(-500.0f, -9.0f+0.2, -500));
			M = TransY;
			glUniformMatrix4fv(linesshader->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			glUniformMatrix4fv(linesshader->getUniform("P"), 1, GL_FALSE, &P[0][0]);
			glUniformMatrix4fv(linesshader->getUniform("V"), 1, GL_FALSE, &V[0][0]);

			vec3 offset = mycam.pos;
			offset.y = 0;
			offset.x = (int)offset.x;
			offset.z = (int)offset.z;
			offset = vec3(0, 0, 0);
			vec3 bg = vec3(254. / 255., 225. / 255., 168. / 255.);
			if (renderstate == 2)
				bg = vec3(49. / 255., 88. / 255., 114. / 255.);
			glUniform3fv(linesshader->getUniform("camoff"), 1, &offset[0]);
			glUniform3fv(linesshader->getUniform("campos"), 1, &mycam.pos[0]);
			glUniform3fv(linesshader->getUniform("bgcolor"), 1, &bg[0]);

			glBindVertexArray(VertexArrayID);
			//
			glActiveTexture(GL_TEXTURE0);

			//glBindTexture(GL_TEXTURE_2D, HeightTex);
			glBindTexture(GL_TEXTURE_2D, AudioTex);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, HeightTex);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferIDBox);
			glDrawElements(GL_LINES, MESHSIZE*MESHSIZE * 8, GL_UNSIGNED_INT, (void*)0);
			glDrawArrays(GL_TRIANGLES, 0, MESHSIZE*MESHSIZE * 6);
			linesshader->unbind();
		}

		
		// all obj transformations
		glm::mat4  TransObj = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10));
		M = TransObj;

		objprog->bind();

		// update explode factor & timer
		static float explode = 0, time = 0.0;
		if (RESET == 1) explode = 0;
		//if (PAUSE == 0) {										// testing exploding -- commented out
		//	if (REVERSE == 1) explode += 0.01;
		//	else if (REVERSE == -1) explode -= 0.01;
		//	time += 0.02;
		//}
		//if (explode < -0.1) explode = -0.1;

		float explode_avg = 0.0;
		for (int i = 0; i < fftd_vector.size(); i++) {
			explode_avg += fftd_vector[i];
		}
		explode_avg /= fftd_vector.size();
		cout << explode_avg << endl;
		explode = 100*explode_avg;
		//cout << explode << endl;

		// draw exploding object -------------------
		// movement transition timer
		transClock += 0.008;

		//cout << "time: " << time << endl;
		// send the uniforms to the gpu
		glUniformMatrix4fv(objprog->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(objprog->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(objprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		glUniform3fv(objprog->getUniform("camoff"), 1, &offset[0]);
		glUniform3fv(objprog->getUniform("campos"), 1, &mycam.pos[0]);
		glUniform1f(objprog->getUniform("explode"), explode);
		glUniform1f(objprog->getUniform("time"), time);
		glUniform1f(objprog->getUniform("transClock"), transClock);

		shape->draw(objprog, false);

		objprog->unbind();
		

	

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
