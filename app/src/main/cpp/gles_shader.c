//
// Created by maks on 04.12.2024.
//
#include "gles_shader.h"
#include <string.h>

#define LOG_TAG __FILE_NAME__
#include "log.h"

static void printInfoLog(GLuint object, GLenum identifier, PFNGLGETSHADERIVPROC glGetiv, PFNGLGETSHADERINFOLOGPROC glGetInfoLog) {
    GLint logLength = 0;
    glGetiv(object, GL_INFO_LOG_LENGTH, &logLength);
    if(logLength == 0) {
        LOGE("Unknown object %x compilation failure", identifier);
        return;
    }
    logLength++;
    GLchar infoLog[logLength];
    glGetInfoLog(object, logLength, NULL, infoLog);
    LOGE("Object %x compilation/link failure:\n%s", identifier, infoLog);
    glDeleteShader(object);
}

static GLuint compileShader(shader_source_t* source) {
    GLuint shader = glCreateShader(source->pipelineStage);
    if(shader == 0) return 0;
    for(int i = 0; i < source->sourceCount; i++) {
        LOGI("Uploading source: %s", source->shaderSources[i]);
    }
    glShaderSource(shader, source->sourceCount, source->shaderSources, NULL);
    glCompileShader(shader);
    GLint compileStatus = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
    if(compileStatus == GL_TRUE) {
        return shader;
    }else {
        printInfoLog(shader, source->pipelineStage, glGetShaderiv, glGetShaderInfoLog);
        return 0;
    }
}

bool createProgram(GLuint* programId, size_t nShaders, shader_source_t* shaders) {
    GLuint program = glCreateProgram();
    if(program == 0) return false;
    GLuint shaderIds[nShaders];
    for(size_t i = 0; i < nShaders; i++) {
        GLuint shader = compileShader(&shaders[i]);
        shaderIds[i] = shader;
        if(shader == 0) goto fail;
        glAttachShader(program, shader);
    }
    glLinkProgram(program);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if(linkStatus == GL_TRUE) {
        for(size_t i = 0; i < nShaders; i++) glDeleteShader(shaderIds[i]);
        *programId = program;
        return true;
    } else {
        printInfoLog(program, 0xFAFE, glGetProgramiv, glGetProgramInfoLog);
    }
    fail:
    for(size_t i = 0; i < nShaders; i++) {
        GLuint shader = shaderIds[i];
        if(shader == 0) break;
        glDeleteShader(shader);
    }
    glDeleteProgram(program);
    return false;
}