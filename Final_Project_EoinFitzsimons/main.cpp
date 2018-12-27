// Windows includes (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

#define STB_IMAGE_IMPLEMENTATION
// Project includes
#include "maths_funcs.h"
#include "stb_image.h"

// GLM includes
#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <gtx/euler_angles.hpp> 
 
/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
#define MESH_NAME "../Lab5/windmill.dae"
#define MESH_NAME2 "../Lab5/arms.dae"


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

#pragma region SimpleTypes
typedef struct
{
	size_t mPointCount = 0;
	std::vector<vec3> mVertices;
	std::vector<vec3> mNormals;
	std::vector<vec2> mTextureCoords;
} ModelData;
#pragma endregion SimpleTypes

using namespace std;
GLuint shaderProgramID, shaderProgramID2;

ModelData mesh_data[2];
unsigned int mesh_vao = 0;
int width = 800;
int height = 600;

GLuint loc1, loc2, loc3;
GLfloat rotate_y = 0.0f;
GLuint tex[2], tex1; // for texture loader

mat4 Gview;
mat4 Gpersp;
mat4 Gmodel;

vec3 cameraPos = vec3(0.0f, 0.0f, -10.0f);

vec3 cameraUp = vec3(0.0f, 1.0f, 0.0f);

vec3 cameraFront = vec3(0.0f, 0.0f, -1.0f);

float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 800.0f / 2.0;
float lastY = 600.0 / 2.0;


#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

ModelData load_mesh(const char* file_name) {
	ModelData modelData;

	/* Use assimp to read the model file, forcing it to be read as    */
	/* triangles. The second flag (aiProcess_PreTransformVertices) is */
	/* relevant if there are multiple meshes in the model file that   */
	/* are offset from the origin. This is pre-transform them so      */
	/* they're in the right position.                                 */
	const aiScene* scene = aiImportFile(
		file_name,
		aiProcess_Triangulate | aiProcess_PreTransformVertices
	);

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", file_name);
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		modelData.mPointCount += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelData.mVertices.push_back(vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelData.mNormals.push_back(vec3(vn->x, vn->y, vn->z));
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				modelData.mTextureCoords.push_back(vec2(vt->x, vt->y));
			} 
			if (mesh->HasTangentsAndBitangents()) {
				/* You can extract tangents and bitangents here              */
				/* Note that you might need to make Assimp generate this     */
				/* data for you. Take a look at the flags that aiImportFile  */
				/* can take.                                                 */
			}
		}
	} 

	aiReleaseImport(scene);
	return modelData;
}

#pragma endregion MESH LOADING

#pragma region TEXTURE LOADING
/*----------------------------------------------------------------------------
TEXTURE LOADING FUNCTION
----------------------------------------------------------------------------*/

void loadTextures(GLuint texture, const char* filepath, int active_arg, const GLchar* texString, int texNum) {
	int x, y, n;
	int force_channels = 4;
	unsigned char *image_data = stbi_load(filepath, &x, &y, &n, force_channels);
	if (!image_data) {
		fprintf(stderr, "ERROR: could not load %s\n", filepath);

	}
	// NPOT check
	if ((x & (x - 1)) != 0 || (y & (y - 1)) != 0) {
		fprintf(stderr, "WARNING: texture %s is not power-of-2 dimensions\n",
			filepath);
	}

	glActiveTexture(active_arg);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		image_data);
	glUniform1i(glGetUniformLocation(shaderProgramID, texString), texNum);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_REPEAT);
	GLfloat max_aniso = 0.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_aniso);
	// set the maximum!
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_aniso);

}
#pragma endregion TEXTURE LOADING

// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS
char* readShaderSource(const char* shaderFile) {
	FILE* fp;
	fopen_s(&fp, shaderFile, "rb");

	if (fp == NULL) { return NULL; }

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		std::cerr << "Error creating shader..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	// check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024] = { '\0' };
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		std::cerr << "Error compiling "
			<< (ShaderType == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< " shader program: " << InfoLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders()
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	shaderProgramID = glCreateProgram();
	if (shaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgramID, "../Lab5/Shaders/simpleVertexShader.txt", GL_VERTEX_SHADER);
	AddShader(shaderProgramID, "../Lab5/Shaders/simpleFragmentShader.txt", GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { '\0' };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgramID);
	return shaderProgramID;
}

GLuint CompileShaders2()
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	shaderProgramID2 = glCreateProgram();
	if (shaderProgramID2 == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgramID2, "../Lab5/Shaders/simpleVertexShader1.txt", GL_VERTEX_SHADER);
	AddShader(shaderProgramID2, "../Lab5/Shaders/simpleFragmentShader1.txt", GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { '\0' };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgramID2);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID2, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID2, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgramID2);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID2, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgramID2, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgramID2);
	return shaderProgramID2;
}
#pragma endregion SHADER_FUNCTIONS

// VBO Functions - click on + to expand
#pragma region VBO_FUNCTIONS

GLuint vao[2];

void generateObjectBufferMesh(int index, const char* mesh) {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	//Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
	//Might be an idea to do a check for that before generating and binding the buffer.

	mesh_data[index] = load_mesh(mesh);
	unsigned int vp_vbo = 0;
	loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
	loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
	loc3 = glGetAttribLocation(shaderProgramID, "vt");

	glGenBuffers(1, &vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data[index].mPointCount * sizeof(vec3), &mesh_data[index].mVertices[0], GL_STATIC_DRAW);
	unsigned int vn_vbo = 0;
	glGenBuffers(1, &vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data[index].mPointCount * sizeof(vec3), &mesh_data[index].mNormals[0], GL_STATIC_DRAW);

//	This is for texture coordinates which you don't currently need, so I have commented it out
	unsigned int vt_vbo = 0;
	glGenBuffers (1, &vt_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data[index].mPointCount * 2 * sizeof(float), &mesh_data[index].mTextureCoords[0], GL_STATIC_DRAW);
	
	glBindVertexArray(vao[index]);

	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	//	This is for texture coordinates which you don't currently need, so I have commented it out
	glEnableVertexAttribArray (loc3);
	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	glVertexAttribPointer (loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);
}

#pragma endregion VBO_FUNCTIONS


void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);

	glBindVertexArray(vao[0]);
	
	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(shaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(shaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(shaderProgramID, "proj");
	int texture_num_loc = glGetUniformLocation(shaderProgramID, "texture_num");

	mat4 base = Gmodel;

	// Root of the Hierarchy
	//mat4 view = identity_mat4();

	mat4 gview = look_at(cameraPos, cameraPos + cameraFront, cameraUp);
	glm::mat4 Gview;

	Gview = glm::eulerAngleXY(yaw, pitch);
	Gview[3][0] = cameraPos.v[0];
	Gview[3][1] = cameraPos.v[1];
	Gview[3][2] = cameraPos.v[2];


	/*for (int i = 0; i < 4; i++) {
	for (int j = 0; j < 4; j++)
	Gview[i][j] = gview.m[i*4 + j];
	}*/
	/*gView[0][0] = cameraPos.x;
	gView[0][1] = cameraPos.y;
	gView[0][2] = cameraPos.z;*/
	base = rotate_z_deg(base, 180); // GO OFF THIS TO GET IN RIGHT POSITION
	base = rotate_y_deg(base, 180);
	


	// update uniforms & draw
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, Gpersp.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, glm::value_ptr(Gview));
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, base.m);

	//glActiveTexture(GL_TEXTURE0);
	glUniform1i(texture_num_loc, 0);
	glBindVertexArray(vao[0]);
	glBindTexture(GL_TEXTURE_2D, tex[0]);
	//glUniform1i(glGetUniformLocation(shaderProgramID, "basic_texture"), 0); // gltexture0 
	glDrawArrays(GL_TRIANGLES, 0, mesh_data[0].mPointCount);

	glBindVertexArray(vao[1]);
	// Set up the child matrix
	mat4 modelChild = identity_mat4();
	modelChild = rotate_z_deg(modelChild, 180);
	modelChild = rotate_z_deg(modelChild, rotate_y);
	modelChild = translate(modelChild, vec3(0.0f, 7.0f, -0.15f));
	modelChild = scale(modelChild, vec3(0.3f, 0.3f, 0.3f));

	// Apply the root matrix to the child matrix
	modelChild = base * modelChild;

	// Update the appropriate uniform and draw the mesh again
	glActiveTexture(GL_TEXTURE1);
	glUniform1i(texture_num_loc, 1);
	//glBindVertexArray(vao[1]);
	glBindTexture(GL_TEXTURE_2D, tex[1]);
	//glUniform1i(glGetUniformLocation(shaderProgramID, "metal_texture"), 1);
	//
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, modelChild.m);
	glDrawArrays(GL_TRIANGLES, 0, mesh_data[1].mPointCount);

	glutSwapBuffers();
}


void updateScene() {


	//printf("  %i rotate\n", rotate_y);
	// Draw the next frame
	static DWORD last_time = 0;
	DWORD curr_time = timeGetTime();
	if (last_time == 0)
		last_time = curr_time;
	float delta = (curr_time - last_time) * 0.001f;
	last_time = curr_time;

	// Rotate the model slowly around the y axis at 20 degrees per second
	rotate_y -= 20.0f * delta;
	rotate_y = fmodf(rotate_y, 360.0f);
	glutPostRedisplay();
}


void init()
{
	// Set up the shaders
	GLuint shaderProgramID = CompileShaders();
	//GLuint shaderProgramID2 = CompileShaders2();
	glGenVertexArrays(2, vao);
	glGenTextures(2, tex);
	// load mesh into a vertex buffer array
	generateObjectBufferMesh(0, MESH_NAME);
	//load_texture("../lab5/brown.jpg", tex[0]);
	loadTextures(tex[0], "../lab5/brown.jpg", GL_TEXTURE0, "basic_texture", 0);
	loadTextures(tex[1], "../lab5/texture3.jpg", GL_TEXTURE1, "metal_texture", 1);
	generateObjectBufferMesh(1, MESH_NAME2);
	//load_texture("../lab5/texture2.jpg", tex[1]);
	Gpersp = perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);
	Gmodel = identity_mat4();
}

GLfloat release;

// Placeholder code for the keypress
void keypress(unsigned char key, int x, int y) {

	vec3 front;
	float xoffset = 0.0f;
	float yoffset = 0.0f;

	float cameraSpeed = 0.5f;

	if (key == 'g') {
		Gmodel = translate(Gmodel, vec3(0.0f, 1.0f, 0.0f));
	}
	else if (key == 'v') {
		Gmodel = translate(Gmodel, vec3(0.0f, -1.0f, 0.0f));
	}
	else if (key == 'c') {
		Gmodel = translate(Gmodel, vec3(1.0f, 0.0f, 0.0f));
	}
	else if (key == 'b') {
		Gmodel = translate(Gmodel, vec3(-1.0f, 1.0f, 0.0f));
	}

	if (key == 'w') {
		cameraPos += (cameraFront)*(cameraSpeed);
	}
	else if (key == 's') {
		cameraPos -= (cameraFront)*(cameraSpeed);
	}
	else if (key == 'a') {
		cameraPos -= normalise(cross(cameraFront, cameraUp))*(cameraSpeed);
	}
	else if (key == 'd') {
		cameraPos += normalise(cross(cameraFront, cameraUp))*(cameraSpeed);
	}
	else if (key == 'j') {
		xoffset -= 0.15f;
		yaw += xoffset;
		if (pitch > 89.0f) {
			pitch = 89.0f;
		}
		if (pitch < -89.0f) {
			pitch = -89.0f;
		}
	}
	else if (key == 'l') {
		xoffset += 0.15f;
		yaw += xoffset;
		if (pitch > 89.0f) {
			pitch = 89.0f;
		}
		if (pitch < -89.0f) {
			pitch = -89.0f;
		}
	}
	else if (key == 'i') {
		yoffset += 0.15f;
		pitch += yoffset;
		front.v[0] = cos(radians(pitch)) * cos(radians(yaw));
		front.v[1] = sin(radians(pitch));
		front.v[2] = cos(radians(pitch)) * sin(radians(yaw));
		cameraFront = normalise(front);
	}
	else if (key == 'k') {
		yoffset -= 0.15f;
		pitch += yoffset; 
		front.v[0] = cos(radians(pitch)) * cos(radians(yaw));
		front.v[1] = sin(radians(pitch));
		front.v[2] = cos(radians(pitch)) * sin(radians(yaw));
		cameraFront = normalise(front);
	}
	static DWORD last_time = 0;
	if (key == 'p') {
		
	
		rotate_y += 2.0f;

	}
}

void keyUp(unsigned char key, int x, int y) {

	if (key == 'p') {
		rotate_y = release;
		rotate_y -= 20.0f;
	}

}

int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("Hello Triangle");

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutIdleFunc(updateScene);
	glutKeyboardFunc(keypress);
	glutKeyboardUpFunc(keyUp);

	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}
	// Set up your objects and shaders
	init();
	// Begin infinite event loop
	glutMainLoop();
	return 0;
}
