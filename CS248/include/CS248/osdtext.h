#ifndef CS248_TEXTOSD_H
#define CS248_TEXTOSD_H

#include <string>
#include <vector>

// 确保 GLEW 以静态库方式引入（如果你的环境是这样配置的）
#define GLEW_STATIC 
#include "GL/glew.h"

#include "color.h"

// 引入 stb_truetype 结构定义
// 注意：我们在 .h 中只包含定义，实现在 .cpp 中通过宏开启
#include "stb_truetype.h"

// 外部引用的 Base64 字体数据，定义在 osdfont.cpp 中
extern const std::string osdfont_base64_1, osdfont_base64_2,
                         osdfont_base64_3, osdfont_base64_4,
                         osdfont_base64_5, osdfont_base64_6;

namespace CS248 {

/**
 * 屏幕显示行数据结构
 */
struct OSDLine {
  // 行的唯一标识 ID
  int id;

  // 屏幕空间锚点位置 (NDC 坐标)
  float x, y;

  // 文本内容
  std::string text;

  // 字体大小 (像素)
  size_t size;

  // 字体颜色
  Color color;
};

/**
 * 提供屏幕文本显示接口
 * 该版本已重写为使用 stb_truetype，不再依赖外部 FreeType 链接库
 */
class OSDText {
 public:

  /**
   * 构造函数
   */
  OSDText();

  /**
   * 析构函数，释放 stb 相关缓冲区和 GL 资源
   */
  ~OSDText();

  /**
   * 初始化资源
   * 解析 Base64 字体并初始化 stb_truetype 句柄
   * \param use_hdpi 是否在高清显示器上渲染（字体大小翻倍）
   * \return 0 成功, -1 失败
   */
  int init(bool use_hdpi);

  /**
   * 渲染所有文本行
   */
  void render();

  /**
   * 清除所有行
   */
  void clear();

  /**
   * 当窗口大小改变时更新缩放系数
   */
  void resize(size_t w, size_t h);

  /**
   * 添加一行文本
   */
  int add_line(float x, float y, std::string text = "",
               size_t size = 16, Color color = Color::White);

  /**
   * 删除指定 ID 的行
   */
  void del_line(int line_id);

  // 设置属性的辅助函数
  void set_anchor(int line_id, float x, float y);
  void set_text(int line_id, std::string text);
  void set_size(int line_id, size_t size);
  void set_color(int line_id, Color color);

 private:

  // 渲染单行文本的核心函数
  void draw_line(OSDLine line);

  // 是否使用高清缩放
  bool use_hdpi;

  // 屏幕缩放因子 (用于将像素坐标映射到 NDC)
  float sx, sy;

  // 自增 ID 计数器
  int next_id;

  // --- stb_truetype 相关成员 ---
  // 必须保留解码后的字体二进制数据，stb 会持续引用它
  unsigned char* font_buffer; 
  
  // stb 字体解析器状态
  stbtt_fontinfo font_info;

  // 存储所有待绘制的行
  std::vector<OSDLine> lines;

  // OpenGL 资源句柄
  GLuint vbo;
  GLuint program;
  GLint attribute_coord;
  GLint uniform_tex;
  GLint uniform_color;

  // 着色器辅助函数
  GLuint compile_shaders();
  GLint  get_attribu(GLuint program, const char *name);
  GLint  get_uniform(GLuint program, const char *name);

}; // class OSDText

} // namespace CS248

#endif // CS248_TEXTOSD_H