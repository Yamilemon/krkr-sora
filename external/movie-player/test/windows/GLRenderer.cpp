#include "GLRenderer.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <iostream>

#define USE_PBO_TEXTURE

#ifdef _DEBUG
#define GL_CHECK(stmt)                          \
  do {                                          \
    stmt;                                       \
    check_ogl_error(#stmt, __FILE__, __LINE__); \
  } while (0)
#define ASSERT(exp, ...) \
  if (!(exp)) {          \
    printf(__VA_ARGS__); \
    abort();             \
  }
#else
#define GL_CHECK(stmt)   stmt
#define ASSERT(exp, ...) ((void)0)
#endif

static inline void
check_ogl_error(const char *stmt, const char *fname, int line)
{
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    printf("OpenGL error %08x, at %s:%i - for %s\n", err, fname, line, stmt);
    abort();
  }
}

static void
glfw_error_callback(int error, const char *description)
{
  printf("Error %d: %s\n", error, description);
}

void
glfw_resize(GLFWwindow *window, int w, int h)
{
  glViewport(0, 0, w, h);
}

GLFWwindow *
Renderer::InitGL(int width, int height, const char *windowTitle)
{
  glfwSetErrorCallback(glfw_error_callback);

  if (!glfwInit()) {
    std::cerr << "glfw init failed" << std::endl;
    exit(-1);
  }

  m_width  = width;
  m_height = height;

  m_window = glfwCreateWindow(width, height, windowTitle, NULL, NULL);
  if (!m_window) {
    glfwTerminate();
    std::cerr << "window creation failed" << std::endl;
    exit(-1);
  }
  glfwMakeContextCurrent(m_window);

  // glfwSwapInterval(0); // fps 制限解除

  glewExperimental = GL_TRUE;
  glewInit();
  glfwSetWindowSizeCallback(m_window, glfw_resize);

  glViewport(0, 0, width, height);
  glScissor(0, 0, width, height);

  // glMatrixMode(GL_PROJECTION);
  // glLoadIdentity();
  // glOrtho(0, width, 0, height, -1, 1);
  // glMatrixMode(GL_MODELVIEW);
  // glLoadIdentity();
  // glDisable(GL_DEPTH_TEST);
  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  return m_window;
}

void
Renderer::DoneGL()
{
  glfwTerminate();
}

// フリップ動作をVSYNC同期するかどうか
void
Renderer::EnableVSync(bool flag)
{
  glfwSwapInterval(flag ? 1 : 0);
}

void
Renderer::SetTextureRenderScale(float scale)
{
  static const GLfloat position[] = {
    -scale, scale, -scale, -scale, scale, scale, scale, -scale,
  };
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo[0]);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(position), position);
}

void
Renderer::InitRenderer()
{
  // シェーダ作成
  const char *VertexShaderSource = "#version 450\n"
                                   "layout( location = 0 ) in vec3 position;\n"
                                   "layout( location = 1 ) in vec2 uv;\n"
                                   "layout( location = 0 ) out vec2 vuv;\n"
                                   "void main() {\n"
                                   "    gl_Position = vec4(position, 1.0 );\n"
                                   "    vuv = uv;\n"
                                   "}\n";

  const char *FragmentShaderSource = "#version 450\n"
                                     "layout( location = 0 ) uniform sampler2D Texture;\n"
                                     "layout( location = 0 ) in vec2 vuv;\n"
                                     "layout( location = 0 ) out vec4 color;\n"
                                     "void main() {\n"
                                     "    color = texture2D(Texture, vuv);\n"
                                     "}\n";

  GLint result;
  GLchar infoLog[1024] = {};

  // vs
  m_vsId = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(m_vsId, 1, &VertexShaderSource, NULL);
  glCompileShader(m_vsId);
  glGetShaderiv(m_vsId, GL_COMPILE_STATUS, &result);
  if (result == GL_FALSE) {
    glGetShaderInfoLog(m_vsId, sizeof(infoLog), nullptr, infoLog);
    ASSERT(false, "Failed to compile vertex shader: %s\n", infoLog);
  }

  // fs
  m_fsId = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(m_fsId, 1, &FragmentShaderSource, NULL);
  glCompileShader(m_fsId);
  if (result == GL_FALSE) {
    glGetShaderInfoLog(m_fsId, sizeof(infoLog), nullptr, infoLog);
    ASSERT(false, "Failed to compile fragment shader: %s\n", infoLog);
  }

  // shader program
  m_programId = glCreateProgram();
  glAttachShader(m_programId, m_vsId);
  glAttachShader(m_programId, m_fsId);
  glLinkProgram(m_programId);
  glGetProgramiv(m_programId, GL_LINK_STATUS, &result);
  if (result == GL_FALSE) {
    glGetProgramInfoLog(m_programId, sizeof(infoLog), nullptr, infoLog);
    ASSERT(false, "Failed to link program: %s\n", infoLog);
  }

  // 頂点配列
  glGenVertexArrays(1, &m_vao);
  glBindVertexArray(m_vao);
  glGenBuffers(2, m_vbo);

  // pos
  static const GLfloat position[] = {
    -0.9f, 0.9f, -0.9f, -0.9f, 0.9f, 0.9f, 0.9f, -0.9f,
  };
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo[0]);
  glBufferData(GL_ARRAY_BUFFER, sizeof(position), position, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0); // pos = 0
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

  // uv
  static const GLfloat uv[] = {
    0, 0, 0, 1, 1, 0, 1, 1,
  };
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo[1]);
  glBufferData(GL_ARRAY_BUFFER, sizeof(uv), uv, GL_STATIC_DRAW);
  glEnableVertexAttribArray(1); // uv = 1
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
}

void
Renderer::DoneRenderer()
{
  glBindVertexArray(0);
  glUseProgram(0);
  glDetachShader(m_programId, m_vsId);
  glDetachShader(m_programId, m_fsId);
  glDeleteProgram(m_programId);
  glDeleteShader(m_vsId);
  glDeleteShader(m_fsId);
  glDeleteBuffers(2, m_vbo);
  glDeleteVertexArrays(1, &m_vao);
#ifdef USE_PBO_TEXTURE
  glDeleteBuffers(1, &m_pbo);
#endif
}

void
Renderer::Render(int frame)
{
  glClearColor(0.0f, 0.0f, 0.4f, 1.0f);
  glClearDepthf(1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glUseProgram(m_programId);
  glBindTexture(GL_TEXTURE_2D, m_textureId);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindTexture(GL_TEXTURE_2D, 0);
  glUseProgram(0);
}

void
Renderer::InitTexture(int width, int height)
{
  m_texW     = width;
  m_texH     = height;
  m_texBytes = width * height * 4;

  glGenTextures(1, &m_textureId);
  // glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glBindTexture(GL_TEXTURE_2D, m_textureId);

#ifdef USE_PBO_TEXTURE
  // PBO を作成
  glGenBuffers(1, &m_pbo);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, m_pbo);
  glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, m_texBytes, 0, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
#endif

  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_2D, 0);
}

void
Renderer::UpdateTexture(const uint8_t *pixels, int sizeBytes)
{
#ifdef USE_PBO_TEXTURE
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
  GLubyte *texPixels = (GLubyte *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
  if (texPixels) {
    memcpy(texPixels, pixels, m_texBytes);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
  }
  pixels = 0;
#endif

  glBindTexture(GL_TEXTURE_2D, m_textureId);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_texW, m_texH, GL_BGRA, GL_UNSIGNED_BYTE,
                  pixels);

  glBindTexture(GL_TEXTURE_2D, 0);

#ifdef USE_PBO_TEXTURE
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif
}
