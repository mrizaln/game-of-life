#ifndef SHADER_HPP
#define SHADER_HPP

#include <fstream>
#include <sstream>
#include <string>

#include <glbinding/gl/gl.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

class Shader
{
public:
    // the program ID
    unsigned int ID;

    // default constructor use the default shader
    //-----------------------------------------------------------------------------------
    Shader()
    {
        const char* vDefaultShaderCode{ "#version 330 core\n"
                                        "layout (location = 0) in vec3 aPos;\n"
                                        "void main()\n"
                                        "{\n"
                                        "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                        "}\0" };

        const char* fDefaultShaderCode{ "#version 330 core\n"
                                        "out vec4 FragColor;\n"
                                        "void main()\n"
                                        "{\n"
                                        "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
                                        "}\0" };

        ID = compileShader(vDefaultShaderCode, fDefaultShaderCode);
    }

    // constructor read and build the shader
    //-----------------------------------------------------------------------------------
    Shader(const char* vertexPath, const char* fragmentPath)
    {
        // 1. retrieve the vertex/fragment source code from filePath
        std::string   vertexCode;
        std::string   fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;
        // ensure ifstream object can throw exceptions:
        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try {
            // open files
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vShaderStream, fShaderStream;
            // read file's buffer contents into streams
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();
            // close file handlers
            vShaderFile.close();
            fShaderFile.close();
            // convert stream into string
            vertexCode   = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        } catch (std::ifstream::failure e) {
            spdlog::error("(Shader) Shader file not successfully read");
        }
        const char* vShaderCode{ vertexCode.c_str() };
        const char* fShaderCode{ fragmentCode.c_str() };

        ID = compileShader(vShaderCode, fShaderCode);
    }

    // use/activate the shader
    //-----------------------------------------------------------------------------------
    void use() const
    {
        gl::glUseProgram(ID);
    }

    // utility uniform functions
    //-----------------------------------------------------------------------------------
    void setBool(const std::string& name, bool value) const
    {
        gl::glUniform1i(gl::glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    //-----------------------------------------------------------------------------------
    void setInt(const std::string& name, int value) const
    {
        gl::glUniform1i(gl::glGetUniformLocation(ID, name.c_str()), value);
    }
    //-----------------------------------------------------------------------------------
    void setFloat(const std::string& name, float value) const
    {
        gl::glUniform1f(gl::glGetUniformLocation(ID, name.c_str()), value);
    }
    //-----------------------------------------------------------------------------------
    void setVec2(const std::string& name, const glm::vec2& value) const
    {
        gl::glUniform2fv(gl::glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    void setVec2(const std::string& name, float x, float y) const
    {
        gl::glUniform2f(gl::glGetUniformLocation(ID, name.c_str()), x, y);
    }
    //-----------------------------------------------------------------------------------
    void setVec3(const std::string& name, const glm::vec3& value) const
    {
        gl::glUniform3fv(gl::glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    void setVec3(const std::string& name, float x, float y, float z) const
    {
        gl::glUniform3f(gl::glGetUniformLocation(ID, name.c_str()), x, y, z);
    }
    //-----------------------------------------------------------------------------------
    void setVec4(const std::string& name, const glm::vec4& value) const
    {
        gl::glUniform4fv(gl::glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    void setVec4(const std::string& name, float x, float y, float z, float w)
    {
        gl::glUniform4f(gl::glGetUniformLocation(ID, name.c_str()), x, y, z, w);
    }
    //-----------------------------------------------------------------------------------
    void setMat2(const std::string& name, const glm::mat2& mat) const
    {
        gl::glUniformMatrix2fv(gl::glGetUniformLocation(ID, name.c_str()), 1, gl::GL_FALSE, &mat[0][0]);
    }
    //-----------------------------------------------------------------------------------
    void setMat3(const std::string& name, const glm::mat3& mat) const
    {
        gl::glUniformMatrix3fv(gl::glGetUniformLocation(ID, name.c_str()), 1, gl::GL_FALSE, &mat[0][0]);
    }
    //-----------------------------------------------------------------------------------
    void setMat4(const std::string& name, const glm::mat4& mat) const
    {
        gl::glUniformMatrix4fv(gl::glGetUniformLocation(ID, name.c_str()), 1, gl::GL_FALSE, &mat[0][0]);
    }

private:
    unsigned int compileShader(const char* vShaderCode, const char* fShaderCode)
    {
        // 2. compile shaders
        //-----------------------------------------------------------------------------------
        unsigned int vertex, fragment;

        // vertex shader
        vertex = glCreateShader(gl::GL_VERTEX_SHADER);
        gl::glShaderSource(vertex, 1, &vShaderCode, NULL);
        gl::glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");    // print compile error if any

        // fragment shader
        fragment = glCreateShader(gl::GL_FRAGMENT_SHADER);
        gl::glShaderSource(fragment, 1, &fShaderCode, NULL);
        gl::glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");    // print compile error if any

        // shader program
        ID = gl::glCreateProgram();
        gl::glAttachShader(ID, vertex);
        gl::glAttachShader(ID, fragment);
        gl::glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");    // print linking error if any

        // delete the shaders as they're linked onto our program now and no longer necessary
        gl::glDeleteShader(vertex);
        gl::glDeleteShader(fragment);

        return ID;
    }

    // utility function for checking shader compilation/linking errors
    //-----------------------------------------------------------------------------------
    void checkCompileErrors(unsigned int shader, std::string type)
    {
        int  success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            gl::glGetShaderiv(shader, gl::GL_COMPILE_STATUS, &success);
            if (!success) {
                spdlog::error("(Shader) Shader compilation error ({}): {}", type, infoLog);
            }
        } else {
            gl::glGetProgramiv(shader, gl::GL_LINK_STATUS, &success);
            if (!success) {
                spdlog::error("(Shader) Shader linking error ({}): {}", type, infoLog);
            }
        }
    }
};

#endif
