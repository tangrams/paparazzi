#include "shader.h"

#include <iostream>

#include "platform.h"
#include "tangram.h"

#include "platform_gl.h"

Shader::Shader():m_program(0),m_fragmentShader(0),m_vertexShader(0) {

}

Shader::~Shader() {
    if (m_program != 0) {           // Avoid crash when no command line arguments supplied
        glDeleteProgram(m_program);
    }
}

bool Shader::load(const std::string& _fragmentSrc, const std::string& _vertexSrc) {
    m_vertexShader = compileShader(_vertexSrc, GL_VERTEX_SHADER);

    if(!m_vertexShader) {
        glDeleteShader(m_vertexShader);
        std::cout << "Error loading vertex shader src" << std::endl;
        return false;
    }

    m_fragmentShader = compileShader(_fragmentSrc, GL_FRAGMENT_SHADER);

    if(!m_fragmentShader) {
        std::cout << "Error loading fragment shader src" << std::endl;
        glDeleteShader(m_fragmentShader);
        return false;
    }

    m_program = glCreateProgram();

    glAttachShader(m_program, m_vertexShader);
    glAttachShader(m_program, m_fragmentShader);    
    glLinkProgram(m_program);

    GLint isLinked;
    glGetProgramiv(m_program, GL_LINK_STATUS, &isLinked);
    
    if (isLinked == GL_FALSE) {
        GLint infoLength = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength > 1) {
            std::vector<GLchar> infoLog(infoLength);
            glGetProgramInfoLog(m_program, infoLength, NULL, &infoLog[0]);
            std::string error(infoLog.begin(),infoLog.end());
            // printf("Error linking shader:\n%s\n", error);
            std::cerr << "Error linking shader: " << error << std::endl;

            std::size_t start = error.find("line ")+5;
            std::size_t end = error.find_last_of(")");
            std::string lineNum = error.substr(start,end-start);
        }
        glDeleteProgram(m_program);
        return false;
    } else {
        glDeleteShader(m_vertexShader);
        glDeleteShader(m_fragmentShader);
        return true;
    }
}

const GLint Shader::getAttribLocation(const std::string& _attribute) const {
    return glGetAttribLocation(m_program, _attribute.c_str());
}

void Shader::use() const {
    if(!isInUse()) {
        glUseProgram(getProgram());
    }
}

bool Shader::isInUse() const {
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    return (getProgram() == (GLuint)currentProgram);
}

GLuint Shader::compileShader(const std::string& _src, GLenum _type) {
    GLuint shader = glCreateShader(_type);
    const GLchar* source = (const GLchar*) _src.c_str();

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint isCompiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);

    if (isCompiled == GL_FALSE) {
        GLint infoLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength > 1) {
            std::vector<GLchar> infoLog(infoLength);
            glGetShaderInfoLog(shader, infoLength, NULL, &infoLog[0]);
            printf("Error compiling shader:\n%s\n", &infoLog[0]);
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

void Shader::detach(GLenum _type) {
    bool vert = (GL_VERTEX_SHADER & _type) == GL_VERTEX_SHADER;
    bool frag = (GL_FRAGMENT_SHADER & _type) == GL_FRAGMENT_SHADER;
    
    if(vert) {
        glDeleteShader(m_vertexShader);
        glDetachShader(m_vertexShader, GL_VERTEX_SHADER);
    }

    if(frag) {
        glDeleteShader(m_fragmentShader);
        glDetachShader(m_fragmentShader, GL_FRAGMENT_SHADER);
    }
}

GLint Shader::getUniformLocation(const std::string& _uniformName) const {
    GLint loc = glGetUniformLocation(m_program, _uniformName.c_str());
    if(loc == -1){
        // std::cerr << "Uniform " << _uniformName << " not found" << std::endl;
    }
    return loc;
}

void Shader::setUniform(const std::string& _name, float _x) {
    if(isInUse()) {
        glUniform1f(getUniformLocation(_name), _x);
        // std::cout << "Uniform " << _name << ": float(" << _x << ")" << std::endl;
    }
}

void Shader::setUniform(const std::string& _name, float _x, float _y) {
    if(isInUse()) {
        glUniform2f(getUniformLocation(_name), _x, _y);
        // std::cout << "Uniform " << _name << ": vec2(" << _x << "," << _y << ")" << std::endl;
    }
}

void Shader::setUniform(const std::string& _name, float _x, float _y, float _z) {
    if(isInUse()) {
        glUniform3f(getUniformLocation(_name), _x, _y, _z);
        // std::cout << "Uniform " << _name << ": vec3(" << _x << "," << _y << "," << _z <<")" << std::endl;
    }
}

void Shader::setUniform(const std::string& _name, float _x, float _y, float _z, float _w) {
    if(isInUse()) {
        glUniform4f(getUniformLocation(_name), _x, _y, _z, _w);
        // std::cout << "Uniform " << _name << ": vec3(" << _x << "," << _y << "," << _z <<")" << std::endl;
    }
}

void Shader::setUniform(const std::string& _name, const Fbo* _fbo, unsigned int _texLoc){
    if(isInUse()) {
        glActiveTexture(GL_TEXTURE0 + _texLoc);
        glBindTexture(GL_TEXTURE_2D, _fbo->getTextureId());
        glUniform1i(getUniformLocation(_name), _texLoc);
    }
}

void Shader::setUniform(const std::string& _name, const glm::mat2& _value, bool _transpose){
    if(isInUse()) {
        glUniformMatrix2fv(getUniformLocation(_name), 1, _transpose, &_value[0][0]);
    }
}

void Shader::setUniform(const std::string& _name, const glm::mat3& _value, bool _transpose){
    if(isInUse()) {
        glUniformMatrix3fv(getUniformLocation(_name), 1, _transpose, &_value[0][0]);
    }
}

void Shader::setUniform(const std::string& _name, const glm::mat4& _value, bool _transpose){
    if(isInUse()) {
        glUniformMatrix4fv(getUniformLocation(_name), 1, _transpose, &_value[0][0]);
    }
}