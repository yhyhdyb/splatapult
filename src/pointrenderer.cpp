#include "pointrenderer.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "image.h"
#include "log.h"
#include "texture.h"
#include "radix_sort.hpp"

PointRenderer::PointRenderer()
{
}

PointRenderer::~PointRenderer()
{
}

bool PointRenderer::Init(std::shared_ptr<PointCloud> pointCloud)
{
    Image pointImg;
    if (!pointImg.Load("texture/sphere.png"))
    {
        Log::printf("Error loading sphere.png\n");
        return false;
    }

    Texture::Params texParams = {FilterType::LinearMipmapLinear, FilterType::Linear, WrapType::ClampToEdge, WrapType::ClampToEdge};
    pointTex = std::make_shared<Texture>(pointImg, texParams);
    pointProg = std::make_shared<Program>();
    if (!pointProg->LoadVertGeomFrag("shader/point_vert.glsl", "shader/point_geom.glsl", "shader/point_frag.glsl"))
    {
        Log::printf("Error loading point shaders!\n");
        return false;
    }

    preSortProg = std::make_shared<Program>();
    if (!preSortProg->LoadCompute("shader/presort_compute.glsl"))
    {
        Log::printf("Error loading point pre-sort compute shader!\n");
        return false;
    }

    BuildVertexArrayObject(pointCloud);
    depthVec.resize(pointCloud->size());
    keyBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, true);
    valBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, true);
    posBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, posVec);
    sorter = std::make_shared<rgc::radix_sort::sorter>(pointCloud->size());

    return true;
}

void PointRenderer::Render(const glm::mat4& cameraMat, const glm::vec4& viewport,
                           const glm::vec2& nearFar, float fovy)
{
    ZoneScoped;

    const size_t numPoints = posVec.size();

    {
        ZoneScopedNC("depth compute", tracy::Color::Red4);

        // transform forward vector into world space
        glm::vec3 forward = glm::mat3(cameraMat) * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 eye = glm::vec3(cameraMat[3]);

        preSortProg->Bind();
        preSortProg->SetUniform("forward", forward);
        preSortProg->SetUniform("eye", eye);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer->GetObj());

        const int LOCAL_SIZE = 256;
        glDispatchCompute(((GLuint)numPoints + (LOCAL_SIZE - 1)) / LOCAL_SIZE, 1, 1); // Assuming LOCAL_SIZE threads per group
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    {
        ZoneScopedNC("sort", tracy::Color::Red4);

        sorter->sort(keyBuffer->GetObj(), valBuffer->GetObj(), numPoints);
    }

    {
        ZoneScopedNC("copy sorted indices", tracy::Color::DarkGreen);

        glBindBuffer(GL_COPY_READ_BUFFER, valBuffer->GetObj());
        glBindBuffer(GL_COPY_WRITE_BUFFER, pointVao->GetElementBuffer()->GetObj());
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, numPoints * sizeof(uint32_t));
    }

    {
        ZoneScopedNC("draw", tracy::Color::Red4);
        float width = viewport.z;
        float height = viewport.w;
        float aspectRatio = width / height;
        glm::mat4 modelViewMat = glm::inverse(cameraMat);
        glm::mat4 projMat = glm::perspective(fovy, aspectRatio, nearFar.x, nearFar.y);

        pointProg->Bind();
        pointProg->SetUniform("modelViewMat", modelViewMat);
        pointProg->SetUniform("projMat", projMat);
        pointProg->SetUniform("pointSize", 0.02f);  // in ndc space?!?
        pointProg->SetUniform("invAspectRatio", 1.0f / aspectRatio);

        // use texture unit 0 for colorTexture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pointTex->texture);
        pointProg->SetUniform("colorTex", 0);
        pointVao->DrawElements(GL_POINTS);
    }
}

void PointRenderer::BuildVertexArrayObject(std::shared_ptr<PointCloud> pointCloud)
{
    pointVao = std::make_shared<VertexArrayObject>();

    // convert pointCloud positions and colors into buffers
    size_t numPoints = pointCloud->size();
    posVec.reserve(numPoints);

    std::vector<glm::vec4> colorVec;
    colorVec.reserve(numPoints);
    for (auto&& p : pointCloud->GetPointVec())
    {
        posVec.emplace_back(glm::vec4(p.position[0], p.position[1], p.position[2], 1.0f));
        colorVec.emplace_back(glm::vec4(p.color[0] / 255.0f, p.color[1] / 255.0f, p.color[2] / 255.0f, 1.0f));
    }
    auto positionBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, posVec);
    auto colorBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, colorVec);

    // build element array
    indexVec.reserve(numPoints);
    assert(numPoints <= std::numeric_limits<uint32_t>::max());
    for (uint32_t i = 0; i < (uint32_t)numPoints; i++)
    {
        indexVec.push_back(i);
    }
    auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec, true); // dynamic

    // setup vertex array object with buffers
    pointVao->SetAttribBuffer(pointProg->GetAttribLoc("position"), positionBuffer);
    pointVao->SetAttribBuffer(pointProg->GetAttribLoc("color"), colorBuffer);
    pointVao->SetElementBuffer(indexBuffer);
}