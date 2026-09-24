#include "shaders/textureshader.h"

#include <QOpenGLContext>

using namespace mixxx;

void TextureShader::init() {
    QString vertexShaderCode = QStringLiteral(R"--(
uniform highp mat4 matrix;
attribute highp vec4 position; // use vec4 here (will be padded) for matrix multiplication
attribute highp vec2 texcoord;
varying highp vec2 vTexcoord;
void main()
{
    vTexcoord = texcoord;
    gl_Position = matrix * position;
}
)--");

    // #version 120 is not valid GLSL ES, so only request it for desktop OpenGL.
    const QOpenGLContext* pContext = QOpenGLContext::currentContext();
    const bool isOpenGLES = pContext && pContext->isOpenGLES();
    QString fragmentShaderCode = isOpenGLES ? QString() : QStringLiteral("#version 120\n");
    fragmentShaderCode += QStringLiteral(R"--(
uniform sampler2D texture;
varying highp vec2 vTexcoord;
uniform highp float alpha;
void main()
{
    gl_FragColor = texture2D(texture, vTexcoord) * vec4(1.0, 1.0, 1.0, alpha > .0 ? alpha : 1.0);
}
)--");

    load(vertexShaderCode, fragmentShaderCode);

    m_matrixLocation = uniformLocation("matrix");
    m_positionLocation = attributeLocation("position");
    m_texcoordLocation = attributeLocation("texcoord");
    m_textureLocation = uniformLocation("texture");
}
