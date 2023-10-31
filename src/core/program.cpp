#include "program.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>

#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#include "log.h"
#include "util.h"

#ifndef NDEBUG
#define WARNINGS_AS_ERRORS
#endif

static void DumpShaderSource(const std::string& source)
{
    std::stringstream ss(source);
    std::string line;
    int i = 1;
    while (std::getline(ss, line))
    {
        Log::printf("%04d: %s\n", i, line.c_str());
        i++;
    }
    Log::printf("\n");
}

static bool CompileShader(GLenum type, const std::string& source, GLint* shaderOut, const std::string& debugName)
{
    GLint shader = glCreateShader(type);
    int size = static_cast<int>(source.size());
    const GLchar* sourcePtr = source.c_str();
    glShaderSource(shader, 1, (const GLchar**)&sourcePtr, &size);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        Log::printf("shader compilation error for \"%s\"!\n", debugName.c_str());
    }

    GLint bufferLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &bufferLen);
    if (bufferLen > 1)
    {
        if (compiled)
        {
            Log::printf("shader compilation warning for \"%s\"!\n", debugName.c_str());
        }

        GLsizei len = 0;
        std::unique_ptr<char> buffer(new char[bufferLen]);
        glGetShaderInfoLog(shader, bufferLen, &len, buffer.get());
        Log::printf("%s\n", buffer.get());
        DumpShaderSource(source);
    }

#ifdef WARNINGS_AS_ERRORS
    if (!compiled || bufferLen > 1)
#else
    if (!compiled)
#endif
    {
        return false;
    }

    *shaderOut = shader;
    return true;
}

Program::Program() : program(0), vertShader(0), geomShader(0), fragShader(0)
{
}

Program::~Program()
{
    Delete();
}

bool Program::LoadVertFrag(const std::string& vertFilename, const std::string& fragFilename)
{
    return LoadVertGeomFrag(vertFilename, std::string(), fragFilename);
}

bool Program::LoadVertGeomFrag(const std::string& vertFilename, const std::string& geomFilename, const std::string& fragFilename)
{
    // Delete old shader/program
    Delete();

    const bool useGeomShader = !geomFilename.empty();

    if (useGeomShader)
    {
        debugName = vertFilename + " + " + geomFilename + " + " + fragFilename;
    }
    else
    {
        debugName = vertFilename + " + " + fragFilename;
    }

    std::string vertSource, geomSource, fragSource;
    if (!LoadFile(vertFilename, vertSource))
    {
        Log::printf("Failed to load vertex shader %s\n", vertFilename.c_str());
        return false;
    }

    if (useGeomShader)
    {
        if (!LoadFile(geomFilename, geomSource))
        {
            Log::printf("Failed to load geometry shader %s\n", geomFilename.c_str());
            return false;
        }
    }

    if (!LoadFile(fragFilename, fragSource))
    {
        Log::printf("Failed to load fragment shader \"%s\"\n", fragFilename.c_str());
        return false;
    }

    if (!CompileShader(GL_VERTEX_SHADER, vertSource, &vertShader, vertFilename))
    {
        Log::printf("Failed to compile vertex shader \"%s\"\n", vertFilename.c_str());
        return false;
    }

    if (useGeomShader)
    {
        if (!CompileShader(GL_GEOMETRY_SHADER, geomSource, &geomShader, geomFilename))
        {
            Log::printf("Failed to compile geometry shader \"%s\"\n", geomFilename.c_str());
            return false;
        }
    }

    if (!CompileShader(GL_FRAGMENT_SHADER, fragSource, &fragShader, fragFilename))
    {
        Log::printf("Failed to compile fragment shader \"%s\"\n", fragFilename.c_str());
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    if (useGeomShader)
    {
        glAttachShader(program, geomShader);
    }
    glLinkProgram(program);

    if (!CheckLinkStatus())
    {
        Log::printf("Failed to link program \"%s\"\n", debugName.c_str());

        // dump shader source for reference
        Log::printf("\n");
        Log::printf("%s =\n", vertFilename.c_str());
        DumpShaderSource(vertSource);
        if (useGeomShader)
        {
            Log::printf("%s =\n", geomFilename.c_str());
            DumpShaderSource(geomSource);
        }
        Log::printf("%s =\n", fragFilename.c_str());
        DumpShaderSource(fragSource);

        return false;
    }

    const int MAX_NAME_SIZE = 1028;
    static char name[MAX_NAME_SIZE];

    GLint numAttribs;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &numAttribs);
    for (int i = 0; i < numAttribs; ++i)
    {
        Variable v;
        GLsizei strLen;
        glGetActiveAttrib(program, i, MAX_NAME_SIZE, &strLen, &v.size, &v.type, name);
        v.loc = glGetAttribLocation(program, name);
        attribs[name] = v;
    }

    GLint numUniforms;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);
    for (int i = 0; i < numUniforms; ++i)
    {
        Variable v;
        GLsizei strLen;
        glGetActiveUniform(program, i, MAX_NAME_SIZE, &strLen, &v.size, &v.type, name);
        int loc = glGetUniformLocation(program, name);
        v.loc = loc;
        uniforms[name] = v;
    }

    return true;
}

bool Program::LoadCompute(const std::string& computeFilename)
{
    // Delete old shader/program
    Delete();

    debugName = computeFilename;

    std::string computeSource;
    if (!LoadFile(computeFilename, computeSource))
    {
        Log::printf("Failed to load compute shader %s\n", computeFilename.c_str());
    }

    if (!CompileShader(GL_COMPUTE_SHADER, computeSource, &computeShader, computeFilename))
    {
        Log::printf("Failed to compile compute shader %s\n", computeFilename.c_str());
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, computeShader);
    glLinkProgram(program);

    if (!CheckLinkStatus())
    {
        Log::printf("Failed to link program \"%s\"\n", debugName.c_str());

        // dump shader source for reference
        Log::printf("\n");
        Log::printf("%s =\n", computeFilename.c_str());
        DumpShaderSource(computeSource);

        return false;
    }

    const int MAX_NAME_SIZE = 1028;
    static char name[MAX_NAME_SIZE];

    GLint numUniforms;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);
    for (int i = 0; i < numUniforms; ++i)
    {
        Variable v;
        GLsizei strLen;
        glGetActiveUniform(program, i, MAX_NAME_SIZE, &strLen, &v.size, &v.type, name);
        int loc = glGetUniformLocation(program, name);
        v.loc = loc;
        uniforms[name] = v;
    }

    // TODO: build reflection info on shader storage blocks

    return true;
}

void Program::Bind() const
{
    glUseProgram(program);
}

int Program::GetUniformLoc(const std::string& name) const
{
    auto iter = uniforms.find(name);
    if (iter != uniforms.end())
    {
        return iter->second.loc;
    }
    else
    {
        assert(false);
        std::cerr << "could not find uniform " << name << " for program " << debugName << std::endl;
        return 0;
    }
}

int Program::GetAttribLoc(const std::string& name) const
{
    auto iter = attribs.find(name);
    if (iter != attribs.end())
    {
        return iter->second.loc;
    }
    else
    {
        std::cerr << "could not find attrib " << name << " for program " << debugName << std::endl;
        assert(false);
        return 0;
    }
}

void Program::SetUniform(int loc, int value) const
{
    glUniform1i(loc, value);
}

void Program::SetUniform(int loc, float value) const
{
    glUniform1f(loc, value);
}

void Program::SetUniform(int loc, const glm::vec2& value) const
{
    glUniform2fv(loc, 1, (float*)&value);
}

void Program::SetUniform(int loc, const glm::vec3& value) const
{
    glUniform3fv(loc, 1, (float*)&value);
}

void Program::SetUniform(int loc, const glm::vec4& value) const
{
    glUniform4fv(loc, 1, (float*)&value);
}

void Program::SetUniform(int loc, const glm::mat2& value) const
{
    glUniformMatrix2fv(loc, 1, GL_FALSE, (float*)&value);
}

void Program::SetUniform(int loc, const glm::mat3& value) const
{
    glUniformMatrix3fv(loc, 1, GL_FALSE, (float*)&value);
}

void Program::SetUniform(int loc, const glm::mat4& value) const
{
    glUniformMatrix4fv(loc, 1, GL_FALSE, (float*)&value);
}

void Program::SetAttrib(int loc, float* values, size_t stride) const
{
    glVertexAttribPointer(loc, 1, GL_FLOAT, GL_FALSE, (GLsizei)stride, values);
    glEnableVertexAttribArray(loc);
}

void Program::SetAttrib(int loc, glm::vec2* values, size_t stride) const
{
    glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, (GLsizei)stride, values);
    glEnableVertexAttribArray(loc);
}

void Program::SetAttrib(int loc, glm::vec3* values, size_t stride) const
{
    glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, (GLsizei)stride, values);
    glEnableVertexAttribArray(loc);
}

void Program::SetAttrib(int loc, glm::vec4* values, size_t stride) const
{
    glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, values);
    glEnableVertexAttribArray(loc);
}

void Program::Delete()
{
    debugName = "";

    glDeleteShader(vertShader);
    vertShader = 0;
    glDeleteShader(geomShader);
    geomShader = 0;
    glDeleteShader(fragShader);
    fragShader = 0;
    glDeleteShader(computeShader);
    computeShader = 0;
    glDeleteProgram(program);
    program = 0;

    uniforms.clear();
    attribs.clear();
}

bool Program::CheckLinkStatus()
{
    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        Log::printf("Failed to link shaders \"%s\"\n", debugName.c_str());
    }

    const GLint MAX_BUFFER_LEN = 4096;
    GLsizei bufferLen = 0;
    std::unique_ptr<char> buffer(new char[MAX_BUFFER_LEN]);
    glGetProgramInfoLog(program, MAX_BUFFER_LEN, &bufferLen, buffer.get());
    if (bufferLen > 0)
    {
        if (linked)
        {
            Log::printf("Warning during linking shaders \"%s\"\n", debugName.c_str());
        }
        Log::printf("%s\n", buffer.get());
    }

#ifdef WARNINGS_AS_ERRORS
    if (!linked || bufferLen > 1)
#else
    if (!linked)
#endif
    {
        return false;
    }

    return true;
}