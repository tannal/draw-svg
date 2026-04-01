#include "osdtext.h"
#include <iostream>
#include <vector>
#include <cstring>

// 1. 定义 STB_TRUETYPE 的实现宏
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// 2. 引入直接存储字体的字节数组头文件
#include "CS248/font_data_DejaVu.h"
#include "console.h"

using namespace std;

namespace CS248 {

struct point {
    GLfloat x;
    GLfloat y;
    GLfloat s;
    GLfloat t;
};

OSDText::OSDText() {
    use_hdpi = false;
    font_buffer = nullptr; 
    lines = vector<OSDLine>(); 
    next_id = 0;
}

OSDText::~OSDText() {
    lines.clear();
    glDeleteProgram(program);
    glDeleteBuffers(1, &vbo);
}

int OSDText::init(bool use_hdpi) {
    this->use_hdpi = use_hdpi;

    // 初始化字体句柄
    if (!stbtt_InitFont(&font_info, assets_DejaVuSans_ttf, 0)) {
        out_err("stb_truetype: 无法从 assets_DejaVuSans_ttf 初始化字体");
        return -1;
    }

    // 编译着色器
    program = compile_shaders();
    if(program) {
        attribute_coord = get_attribu(program, "coord");
        uniform_tex     = get_uniform(program, "tex");
        uniform_color   = get_uniform(program, "color");
        if (attribute_coord == -1 || uniform_tex == -1 || uniform_color == -1) {
            return -1;
        }
    } else {
        return -1;
    }

    glGenBuffers(1, &vbo);
    return 0;
}

void OSDText::render() {
    glUseProgram(program);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (size_t i = 0; i < lines.size(); i++) {
        draw_line(lines[i]);
    }

    glUseProgram(0);
}

void OSDText::clear() {
    lines.clear();
}

void OSDText::resize(size_t w, size_t h) {
    sx = 2.0f / w;
    sy = 2.0f / h;
}

int OSDText::add_line(float x, float y, string text, size_t size, Color color) {
    OSDLine new_line;
    new_line.x = x;
    new_line.y = y;
    new_line.text = text;
    // 默认字号从 16 提升到 24 或更高，确保清晰
    new_line.size = (size == 16) ? 28 : size; 
    new_line.color = color;

    if (use_hdpi) new_line.size *= 2;

    new_line.id = next_id++;
    lines.push_back(new_line);
    return new_line.id;
}

void OSDText::draw_line(OSDLine line) {
    glUniform4fv(uniform_color, 1, (GLfloat*)&line.color);

    // 获取当前字号的缩放比例
    float scale = stbtt_ScaleForPixelHeight(&font_info, (float)line.size);

    // 获取字体的度量信息以实现完美对齐
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &lineGap);
    
    // baseline 定义了文字底部的参考线，所有字符应基于此对齐
    float baseline = (float)ascent * scale;

    GLuint tex;
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(uniform_tex, 0);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnableVertexAttribArray(attribute_coord);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);

    float cur_x = line.x;
    float cur_y = line.y;
    const char* text_ptr = line.text.c_str();

    for (int i = 0; text_ptr[i]; i++) {
        int w, h, xoff, yoff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font_info, 0, scale, text_ptr[i], &w, &h, &xoff, &yoff);

        if (bitmap) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap);

            // 核心对齐逻辑：
            // cur_y 是行起始位置。
            // baseline 让文字重心落在参考线上。
            // yoff 是字符顶部相对于基线的偏移（通常为负值）。
            float x_pos = cur_x + xoff * sx;
            float y_pos = -cur_y - (baseline + yoff) * sy; 
            
            float gw = w * sx;
            float gh = h * sy;

            point box[4] = {
                {x_pos,      -y_pos,      0, 0},
                {x_pos + gw, -y_pos,      1, 0},
                {x_pos,      -y_pos - gh, 0, 1},
                {x_pos + gw, -y_pos - gh, 1, 1},
            };

            glBufferData(GL_ARRAY_BUFFER, sizeof(box), box, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            stbtt_FreeBitmap(bitmap, nullptr);
        }

        // 精确计算下一个字符的间距
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font_info, text_ptr[i], &advance, &lsb);
        cur_x += advance * scale * sx;

        // 处理字距微调 (Kerning)，让字符间距更自然
        if (text_ptr[i+1]) {
            cur_x += stbtt_GetCodepointKernAdvance(&font_info, text_ptr[i], text_ptr[i+1]) * scale * sx;
        }
    }

    glDisableVertexAttribArray(attribute_coord);
    glDeleteTextures(1, &tex);
}

// 属性设置辅助函数
void OSDText::del_line(int line_id) {
    for (auto it = lines.begin(); it != lines.end(); ++it) {
        if (it->id == line_id) {
            lines.erase(it);
            break;
        }
    }
}

void OSDText::set_anchor(int id, float x, float y) { for(auto& l : lines) if(l.id == id) { l.x = x; l.y = y; } }
void OSDText::set_text(int id, string text) { for(auto& l : lines) if(l.id == id) { l.text = text; } }
void OSDText::set_size(int id, size_t size) { for(auto& l : lines) if(l.id == id) { l.size = size; } }
void OSDText::set_color(int id, Color color) { for(auto& l : lines) if(l.id == id) { l.color = color; } }

GLuint OSDText::compile_shaders() {
    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);

    const char *vert_shader_src = "#version 120\n"
        "attribute vec4 coord;\n"
        "varying vec2 texpos;\n"
        "void main(void) {\n"
        "  gl_Position = vec4(coord.xy, 0, 1);\n"
        "  texpos = coord.zw;\n"
        "}";

    const char *frag_shader_src = "#version 120\n"
        "varying vec2 texpos;\n"
        "uniform sampler2D tex;\n"
        "uniform vec4 color;\n"
        "void main(void) {\n"
        "  gl_FragColor = vec4(1, 1, 1, texture2D(tex, texpos).a) * color;\n"
        "}";

    glShaderSource(vert_shader, 1, &vert_shader_src, NULL);
    glCompileShader(vert_shader);
    glShaderSource(frag_shader, 1, &frag_shader_src, NULL);
    glCompileShader(frag_shader);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert_shader);
    glAttachShader(prog, frag_shader);
    glLinkProgram(prog);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
    return prog;
}

GLint OSDText::get_attribu(GLuint prog, const char *name) {
    GLint attr = glGetAttribLocation(prog, name);
    if(attr == -1) fprintf(stderr, "Cannot bind attribute %s\n", name);
    return attr;
}

GLint OSDText::get_uniform(GLuint prog, const char *name) {
    GLint uni = glGetUniformLocation(prog, name);
    if(uni == -1) fprintf(stderr, "Cannot bind uniform %s\n", name);
    return uni;
}

} // namespace CS248