#pragma once

// #define GLEW_STATIC
#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <cstdint>

class Renderer
{
public:
  GLFWwindow *InitGL(int width, int height, const char *windowTitle = "OpenGL Window");
  void DoneGL();

  void InitRenderer();
  void DoneRenderer();

  void EnableVSync(bool flag);

  void Render(int frame);

  void InitTexture(int width, int height);
  void UpdateTexture(const uint8_t *pixels, int sizeBytes);
  void SetTextureRenderScale(float scale);

  GLFWwindow *GLFWWindow() const { return m_window; }

private:
  GLFWwindow *m_window;
  int m_width, m_height;

  GLuint m_vsId, m_fsId, m_programId;
  GLuint m_textureId;
  int m_texW, m_texH, m_texBytes;
  GLuint m_vao;
  GLuint m_vbo[2];
  GLuint m_pbo;
};
