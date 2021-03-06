// CIS565 CUDA Rasterizer: A simple rasterization pipeline for Patrick Cozzi's CIS565: GPU Computing at the University of Pennsylvania
// Written by Yining Karl Li, Copyright (c) 2012 University of Pennsylvania
#include "main.h"

#include "util.h"
#include "statVal.h"
#ifndef __APPLE__
#include <GL/freeglut.h>
#endif
#include "variables.h"
#include <GL/glui.h>
//-------------------------------
//-------------MAIN--------------
//-------------------------------
HostStat statVal;
VertUniform vsUniform;
FragUniform fsUniform;

//-------------------------------
//------------GL STUFF-----------
//-------------------------------
int frame;
int fpstracker;
double seconds;
int fps = 0;
GLuint positionLocation = 0;
GLuint texcoordsLocation = 1;
const char *attributeLocations[] = { "Position", "Tex" };
GLuint pbo = (GLuint)NULL;
GLuint displayImage;
uchar4 *dptr;

ObjModel* mesh;

//float* vbo;
//int vbosize;
//float* cbo;
//int cbosize;
//float* nbo;
//int nbosize;
//float* tbo;
//int tbosize;
//int* ibo;
//int* nibo;
//int* tibo;
//int ibosize;

Param param;

//CUDA Resources
cudaGraphicsResource* cudaPboRc = 0;
size_t cudaRcSize;
//-------------------------------
//----------CUDA STUFF-----------
//-------------------------------
glm::mat4 cameraRotate;
int width=800; int height=800;
glm::vec4 lightPos( 2.0f, 2.0f, 2.0f, 1.0f );
glm::vec4 lightPos2;

glm::mat4 lookAt( glm::vec3 &eye, glm::vec3 &eyeLook, glm::vec3 &up )
{
    glm::mat4 mat;
    glm::vec3 V, UP, U, W;

    V = eyeLook - eye;
    V = glm::normalize( V);
    UP = glm::normalize( up );

    U = glm::cross( V, UP);
    U = glm::normalize( U );
    W = glm::cross( U, V );
    W = glm::normalize( W );

    mat[0] = glm::vec4( U, -eye[0] );
    mat[1] = glm::vec4( W, -eye[1] );
    mat[2] = glm::vec4( -V, -eye[2] );
    mat[3] = glm::vec4( 0.0f, 0.0f, 0.0f, 1.0f );
    
    return glm::transpose( mat );
    //return mat;
}

glm::mat4 perspective( float fov, float aspect, float zNear, float zFar )
{
    glm::mat4 mat;
    fov = fov * 3.1415926f / 180.0f;
    //mat[0] = glm::vec4( 1.0f / ( aspect * tan(fov/2.0f) ), 0.0f, 0.0f, 0.0f);
    //mat[1] = glm::vec4( 0.0f, 1.0f/tan(fov/2.0f), 0.0f, 0.0f );
    //mat[2] = glm::vec4( 0.0f, 0.0f, (zFar+zNear) / (zFar-zNear), -2.0f * zFar *zNear/(zFar-zNear) );
    //mat[3] = glm::vec4( 0.0f, 0.0f, 1.0f, 0.0f );
    //return glm::transpose( mat );

    mat[0] = glm::vec4( 1.0f / ( aspect * tan(fov/2.0f) ), 0.0f, 0.0f, 0.0f );
    mat[1] = glm::vec4( 0.0f, 1.0f/tan(fov/2.0f), 0.0f, 0.0f );
    mat[2] = glm::vec4( 0.0f, 0.0f, -(zFar+zNear)/(zFar-zNear), -1.0f );
    mat[3] = glm::vec4( 0, 0, 2*zFar*zNear/(zFar-zNear), 0.0f );

    return mat;
}

objLoader meshloader;

int main(int argc, char** argv)
{

  bool loadedScene = false;
  for(int i=1; i<argc; i++)
  {
    string header; string data;
    istringstream liness(argv[i]);
    getline(liness, header, '='); getline(liness, data, '=');
    if(strcmp(header.c_str(), "mesh")==0)
    {
    
      //objLoader* loader = new objLoader(data, mesh);
      
      //delete loader;
      mesh = meshloader.load( data );
      if( mesh == NULL )
          loadedScene = false;
      else
          loadedScene = true;
    }
  }

  if(!loadedScene)
  {
    cout << "Usage: mesh=[obj file]" << endl;
    system("pause");
    return 0;
  }

  frame = 0;
  seconds = time (NULL);
  fpstracker = 0;

  //Setup variables for coordinate calculation
  statVal.eyePos = glm::vec3( 3.0f, 0.0f, 1.5f );
  statVal.eyeLook = glm::vec3( 0.0f, 0.0f, 0.0f );
  statVal.upDir = glm::vec3( 0.0f,1.0f, 0.0f );
  statVal.aspect = (float)width/(float)height;
  statVal.nearp = 1.0f;
  statVal.farp = 10.0f;
  statVal.FOV = 60.0f;
  vsUniform.viewingMat = glm::lookAt( statVal.eyePos, statVal.eyeLook, statVal.upDir );
  vsUniform.projMat = glm::perspective( statVal.FOV, statVal.aspect, statVal.nearp, statVal.farp );


  // Launch CUDA/GL
  #ifdef __APPLE__
  // Needed in OSX to force use of OpenGL3.2 
  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 2);
  glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  init();
  #else
  init(argc, argv);
  #endif

  initCuda();
  
  initVAO();
  initTextures();

  GLuint passthroughProgram;
  passthroughProgram = initShader("shaders/passthroughVS.glsl", "shaders/passthroughFS.glsl");

  glUseProgram(passthroughProgram);
  glActiveTexture(GL_TEXTURE0);

  #ifdef __APPLE__
    // send into GLFW main loop
    while(1){
      display();
      if (glfwGetKey(GLFW_KEY_ESC) == GLFW_PRESS || !glfwGetWindowParam( GLFW_OPENED )){
          kernelCleanup();
          cudaDeviceReset(); 
          exit(0);
      }
    }

    glfwTerminate();
  #else
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutIdleFunc( idle );
    glutReshapeFunc( reshape );

    glutMainLoop();
  #endif
  kernelCleanup();
  GLUI_Master.close_all();
  delete mesh;
  return 0;
}

//-------------------------------
//---------RUNTIME STUFF---------
//-------------------------------

void runCuda()
{
  // Map OpenGL buffer object for writing from CUDA on a single GPU
  // No data is moved (Win & Linux). When mapped to CUDA, OpenGL should not use this buffer
  dptr=NULL;

  //vbo = mesh->getVBO();
  //vbosize = mesh->getVBOsize();

  //float newcbo[] = {0.0, 1.0, 0.0, 
  //                  0.0, 0.0, 1.0, 
  //                  1.0, 0.0, 0.0};
  //cbo = newcbo;
  //cbosize = 9;

  //ibo = mesh->getIBO();
  //ibosize = mesh->getIBOsize();

  cudaGraphicsMapResources( 1, &cudaPboRc, 0 );
  cudaErrorCheck( cudaGraphicsResourceGetMappedPointer((void**) &dptr, &cudaRcSize, cudaPboRc ) );
  cudaRasterizeCore(dptr, width, height, frame, param, vsUniform, fsUniform);
  cudaGraphicsUnmapResources( 1, &cudaPboRc, 0 );

  //vbo = NULL;
  //cbo = NULL;
  //ibo = NULL;

  frame++;
  fpstracker++;

}

#ifdef __APPLE__

  void display(){
      runCuda();
      time_t seconds2 = time (NULL);

      if(seconds2-seconds >= 1){

        fps = fpstracker/(seconds2-seconds);
        fpstracker = 0;
        seconds = seconds2;

      }

      string title = "CIS565 Rasterizer | "+ utilityCore::convertIntToString((int)fps) + "FPS";

      glfwSetWindowTitle(title.c_str());


      glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pbo);
      glBindTexture(GL_TEXTURE_2D, displayImage);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 
            GL_RGBA, GL_UNSIGNED_BYTE, NULL);


      glClear(GL_COLOR_BUFFER_BIT);   

      // VAO, shader program, and texture already bound
      glDrawElements(GL_TRIANGLES, 6,  GL_UNSIGNED_SHORT, 0);

      glfwSwapBuffers();
  }

#else

  void display()
  {
      statVal.eyePos = glm::vec3(cameraRotate * statVal.initialEyePos );
    //uniform variables setting
      vsUniform.viewingMat = glm::lookAt( statVal.eyePos, statVal.eyeLook, statVal.upDir );
      vsUniform.normalMat = glm::transpose( glm::inverse( vsUniform.viewingMat ) );
      //vsUniform.projMat = glm::perspective( statVal.FOV, statVal.aspect, statVal.nearp, statVal.farp );
      //vsUniform.viewingMat = lookAt( statVal.eyePos, statVal.eyeLook, statVal.upDir );
      //vsUniform.projMat = perspective( statVal.FOV, statVal.aspect, statVal.nearp, statVal.farp );
      lightPos2 = vsUniform.viewingMat * glm::vec4(lightPos);

    runCuda();
	time_t seconds2 = time (NULL);

    if(seconds2-seconds >= 1)
    {

      fps = fpstracker/(seconds2-seconds);
      fpstracker = 0;
      seconds = seconds2;

    }

    string title = "CIS565 Rasterizer | "+ utilityCore::convertIntToString((int)fps) + "FPS";
    glutSetWindowTitle(title.c_str());

    glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pbo);
    glBindTexture(GL_TEXTURE_2D, displayImage);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 
        GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glClearColor( 0, 0, 0, 0 );
    glClear(GL_COLOR_BUFFER_BIT);   

    // VAO, shader program, and texture already bound
    glDrawElements(GL_TRIANGLES, 6,  GL_UNSIGNED_SHORT, 0);

    //glutPostRedisplay();
    glutSwapBuffers();
  }

  void keyboard(unsigned char key, int x, int y)
  {
    switch (key) 
    {
       case(27):
         shut_down(1);    
         break;
    }
  }

#endif

void reshape( int w, int h )
{
    width = w;
    height = h;
    statVal.aspect = (float)w/(float)h;
    //vsUniform.projMat = perspective( statVal.FOV, statVal.aspect, statVal.nearp, statVal.farp );
    vsUniform.projMat = glm::perspective( statVal.FOV, statVal.aspect, statVal.nearp, statVal.farp);
    initDeviceBuf( w, h, 
                   param.vbo, param.vbosize,
                   param.cbo, param.cbosize, 
                   param.nbo, param.nbosize,
                   param.tbo, param.tbosize,
                   param.ibo, param.nibo, param.tibo, param.ibosize );
    
    glViewport( 0, 0, w, h );
}

void idle()
{
    glutPostRedisplay();

}
//-------------------------------
//----------SETUP STUFF----------
//-------------------------------

  void init(int argc, char* argv[])
  {

    glutInit(&argc, argv );
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH );
    glutInitContextVersion( 3,3);
    glutInitContextFlags( GLUT_FORWARD_COMPATIBLE );
    glutInitContextProfile( GLUT_COMPATIBILITY_PROFILE );

    glutInitWindowSize(width, height);
    int win_id = glutCreateWindow("CIS565 Rasterizer");

    initGLUI( win_id );
    // Init GLEW
    glewInit();
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
      /* Problem: glewInit failed, something is seriously wrong. */
      std::cout << "glewInit failed, aborting." << std::endl;
      std::cout<<"Error: "<<glewGetErrorString(err)<<endl;
      exit (1);
    }

    initVAO();
    initTextures();
  }

void initPBO(GLuint* pbo)
{
  if (pbo) 
  {
    // set up vertex data parameter
    int num_texels = width*height;
    int num_values = num_texels * 4;
    int size_tex_data = sizeof(GLubyte) * num_values;
    
    // Generate a buffer ID called a PBO (Pixel Buffer Object)
    glGenBuffers(1,pbo);
    // Make this the current UNPACK buffer (OpenGL is state-based)
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo);
    // Allocate data for the buffer. 4-channel 8-bit image
    glBufferData(GL_PIXEL_UNPACK_BUFFER, size_tex_data, NULL, GL_DYNAMIC_COPY);
    //cudaGLRegisterBufferObject( *pbo );
    cudaGraphicsGLRegisterBuffer( &cudaPboRc, *pbo, cudaGraphicsRegisterFlagsNone );
  }
}
  float newcbo[] = {1.0f, 1.0f, 1.0f, 
                    1.0f, 1.0f, 1.0f, 
                    1.0f, 1.0f, 1.0f};
void initCuda()
{
  // Use device with highest Gflops/s
  cudaGLSetGLDevice( compat_getMaxGflopsDeviceId() );

  initPBO(&pbo);



  memset( &param, 0, sizeof( Param ) );

  param.vbo = mesh->vbo;
  param.vbosize = 3* mesh->numVert;

  param.nbo = mesh->nbo;
  param.nbosize = 3* mesh->numNrml;
  param.tbo = mesh->tbo;
  param.tbosize = 2 * mesh->numTxcoord;

  param.cbo = newcbo;
  param.cbosize = 9;

  param.ibo = mesh->ibo;
  param.nibo = mesh->nibo;
  param.tibo = mesh->tibo;
  param.ibosize = mesh->numIdx;

  param.groups = mesh->groups;
  param.numGroup = mesh->numGroup;

    //upload textures to devices
  for( int i = 0; i < mesh->numGroup; ++i )
  {
      if( mesh->groups[i].sampler2D )
      {
          initDeviceTexBuf( mesh->groups[i].sampler2D, mesh->groups[i].sampler_w,  mesh->groups[i].sampler_h );
      }
  }

  //upload vertices attributes to devices
  initDeviceBuf( width, height,
                 mesh->vbo, param.vbosize,
                 newcbo, param.cbosize,
                 param.nbo, param.nbosize,
                 mesh->tbo, param.tbosize, 
                 param.ibo, param.nibo, param.tibo, param.ibosize
                 );



  // Clean up on program exit
  atexit(cleanupCuda);

  //runCuda();
}

void initTextures(){
    glGenTextures(1,&displayImage);
    glBindTexture(GL_TEXTURE_2D, displayImage);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA,
        GL_UNSIGNED_BYTE, NULL);
}

void initVAO(void)
{
    GLfloat vertices[] =
    { 
        -1.0f, -1.0f, 
         1.0f, -1.0f, 
         1.0f,  1.0f, 
        -1.0f,  1.0f, 
    };

    GLfloat texcoords[] = 
    { 
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f,1.0f,
        0.0f, 1.0f
    };

    GLushort indices[] = { 0, 1, 3, 3, 1, 2 };

    GLuint vertexBufferObjID[3];
    glGenBuffers(3, vertexBufferObjID);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObjID[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer((GLuint)positionLocation, 2, GL_FLOAT, GL_FALSE, 0, 0); 
    glEnableVertexAttribArray(positionLocation);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObjID[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
    glVertexAttribPointer((GLuint)texcoordsLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(texcoordsLocation);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexBufferObjID[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
}

GLuint initShader(const char *vertexShaderPath, const char *fragmentShaderPath)
{
    GLuint program = glslUtility::createProgram(vertexShaderPath, fragmentShaderPath, attributeLocations, 2);
    GLint location;

    glUseProgram(program);
    
    if ((location = glGetUniformLocation(program, "u_image")) != -1)
    {
        glUniform1i(location, 0);
    }

    return program;
}

//-------------------------------
//---------CLEANUP STUFF---------
//-------------------------------

void cleanupCuda()
{
  if(pbo)
  {
      deletePBO(&pbo);
        cudaGraphicsUnregisterResource( cudaPboRc );
  }

  if(displayImage) deleteTexture(&displayImage);

  cudaDeviceReset();
}

void deletePBO(GLuint* pbo)
{
  if (pbo)
  {
    // unregister this buffer object with CUDA
    cudaGLUnregisterBufferObject(*pbo);
    
    glBindBuffer(GL_ARRAY_BUFFER, *pbo);
    glDeleteBuffers(1, pbo);
    
    *pbo = (GLuint)NULL;
  }
}

void deleteTexture(GLuint* tex)
{
    glDeleteTextures(1, tex);
    *tex = (GLuint)NULL;
}
 
void shut_down(int return_code)
{
  
  kernelCleanup();
  //cudaDeviceReset();
  #ifdef __APPLE__
  glfwTerminate();
  #endif
  exit(return_code);
}


void initGLUI( int win_id )
{
    GLUI *glui_obj = GLUI_Master.create_glui( "glui" );
    glui_obj->set_main_gfx_window( win_id );

    GLUI_Master.set_glutIdleFunc( idle );
    GLUI_Master.set_glutSpecialFunc( NULL );

    GLUI_Rotation *view_rot = glui_obj->add_rotation( "Camera", &cameraRotate[0][0] );
	view_rot->set_spin( 0 );
	glui_obj->add_column( false );

    GLUI_Rotation *obj_rot = glui_obj->add_rotation( "Camera", &cameraRotate[0][0] );
	obj_rot->set_spin(0 );
 
	glui_obj->add_column( false );

    statVal.initialEyePos = glm::vec4( statVal.eyePos , 1.0f ) ;


}