#ifndef __shader__h
#define __shader__h
#include <string.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLFW/glfw3.h>
class Shader
{
public:
    ~Shader()
    {
        glDeleteProgram(program);
        printf("glDeleteProgram *********************************\n");
    }

    char* load_shader(const char* filename)
    {
        std::string src;
        std::string line;
        std::ifstream ifs{filename};
        while (getline(ifs, line)) {
            src += line + "\n";
        }
        
        std::cerr << src << std::endl;
        return strndup(src.c_str(), src.size());
    }
    
    GLuint create_shader(GLenum type, const char *source)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint compile_ret;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_ret);
        if (compile_ret == GL_FALSE) {
            char buf[512];
            glGetShaderInfoLog(shader, sizeof buf - 1, NULL, buf);
            fprintf(stderr, "%s\n", buf);
            return 0;
        }

        return shader;
    }

    GLuint create_program(const char *vsr, const char *fsr)
    {
        vs = create_shader(GL_VERTEX_SHADER, vsr);
        fs = create_shader(GL_FRAGMENT_SHADER, fsr);
        if (vs == 0 || fs == 0) return 0;

        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        if (!program) {
            std::cout << "create programe failure!" << program <<std::endl;
            exit(-1);
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        printf("vs -> %d, fs -> %d, program is -> %d \n", vs, fs, program);
        printf("*********************************\n");
        return program;
    }

public:
    GLuint program;
    GLuint vs;
    GLuint fs;
};
#endif