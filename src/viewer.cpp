#include "viewer.h"

#include <SOIL2.h>
#include <imgui.h>

#include "camera.h"

using namespace Eigen;

Viewer::Viewer() {}

Viewer::~Viewer()
{
    for (Mesh *s : _shapes)
        delete s;
    for (Mesh *s : _pointLights)
        delete s;
}

////////////////////////////////////////////////////////////////////////////////
// GL stuff

// initialize OpenGL context
void Viewer::init(int w, int h)
{
    _winWidth = w;
    _winHeight = h;

    // set the background color, i.e., the color used
    // to fill the screen when calling glClear(GL_COLOR_BUFFER_BIT)
    glClearColor(0.f, 0.f, 0.f, 1.f);

    glEnable(GL_DEPTH_TEST);

    loadProgram();

    // Initilization of FBO
    _fbo.init(w, h);

    _quad = new Mesh();
    _quad->createGrid(2, 2);
    _quad->init();

    Mesh *floor = new Mesh();
    floor->createGrid(2, 2);
    floor->init();
    floor->transformationMatrix() = AngleAxisf(M_PI / 2.f, Vector3f(-1, 0, 0)) * Scaling(20.f, 20.f, 1.f) * Translation3f(-0.5, -0.5, -0.5);
    _shapes.push_back(floor);
    _specularCoef.push_back(0.f);

    Mesh *sphere = new Mesh();
    sphere->load(DATA_DIR "/models/sphere.off");
    sphere->init();
    sphere->transformationMatrix() = Translation3f(0, 0, 2.f) * Scaling(0.5f);
    _shapes.push_back(sphere);
    _specularCoef.push_back(0.3f);

    Mesh *tw = new Mesh();
    tw->load(DATA_DIR "/models/tw.off");
    tw->init();
    _shapes.push_back(tw);
    _specularCoef.push_back(0.75);

    Mesh *light = new Mesh();
    light->createSphere(0.025f);
    light->init();
    light->transformationMatrix() = Translation3f(_cam.sceneCenter() + _cam.sceneRadius() * Vector3f(Eigen::internal::random<float>(), Eigen::internal::random<float>(0.1f, 0.5f), Eigen::internal::random<float>()));
    _pointLights.push_back(light);
    _lightColors.push_back(Vector3f::Constant(0.8f));

    Mesh *coloredLights[2];
    Vector3f colors[] = {Vector3f(1, 0, 0), Vector3f(0, 0, 1)};
    for (int i = 0; i < 2; ++i)
    {
        coloredLights[i] = new Mesh();
        coloredLights[i]->createSphere(0.025f);
        coloredLights[i]->init();
        coloredLights[i]->transformationMatrix() = Translation3f(_cam.sceneCenter() + _cam.sceneRadius() * Vector3f(Eigen::internal::random<float>(), Eigen::internal::random<float>(0.1f, 0.5f), Eigen::internal::random<float>()));

        _pointLights.push_back(coloredLights[i]);
        _lightColors.push_back(colors[i]);
    }

    for (size_t i = 0; i < _shapes.size(); ++i)
        for (size_t j = 0; j < _pointLights.size(); ++j)
        {
            Mesh *shapeShadowVolume = _shapes[i]->computeShadowVolume(_shapes[i]->transformationMatrix().inverse() * _pointLights[j]->transformationMatrix().translation());
            shapeShadowVolume->init();
            shapeShadowVolume->transformationMatrix() = _shapes[i]->transformationMatrix();
            _shadowVolumes.push_back(shapeShadowVolume);
        }

    _lastTime = glfwGetTime();

    AlignedBox3f aabb;
    for (size_t i = 0; i < _shapes.size(); ++i)
        aabb.extend(_shapes[i]->boundingBox());

    _cam.setSceneCenter(aabb.center());
    _cam.setSceneRadius(aabb.sizes().maxCoeff());
    _cam.setSceneDistance(_cam.sceneRadius() * 3.f);
    _cam.setMinNear(0.1f);
    _cam.setNearFarOffsets(-_cam.sceneRadius() * 100.f, _cam.sceneRadius() * 100.f);
    _cam.setScreenViewport(AlignedBox2f(Vector2f(0.0, 0.0), Vector2f(w, h)));
}

void Viewer::reshape(int w, int h)
{
    _winWidth = w;
    _winHeight = h;
    _cam.setScreenViewport(AlignedBox2f(Vector2f(0.0, 0.0), Vector2f(w, h)));
    glViewport(0, 0, w, h);
}

void Viewer::drawForward()
{
    glDepthMask(GL_TRUE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    _blinnPrg.activate();

    glUniformMatrix4fv(_blinnPrg.getUniformLocation("projection_matrix"), 1, GL_FALSE, _cam.computeProjectionMatrix().data());
    glUniformMatrix4fv(_blinnPrg.getUniformLocation("view_matrix"), 1, GL_FALSE, _cam.computeViewMatrix().data());

    Vector4f lightPos;
    lightPos << _pointLights[0]->transformationMatrix().translation(), 1.f;
    glUniform4fv(_blinnPrg.getUniformLocation("light_pos"), 1, (_cam.computeViewMatrix() * lightPos).eval().data());
    glUniform3fv(_blinnPrg.getUniformLocation("light_col"), 1, _lightColors[0].data());

    for (size_t i = 0; i < _shapes.size(); ++i)
    {
        glUniformMatrix4fv(_blinnPrg.getUniformLocation("model_matrix"), 1, GL_FALSE, _shapes[i]->transformationMatrix().data());
        Matrix3f normal_matrix = (_cam.computeViewMatrix() * _shapes[i]->transformationMatrix()).linear().inverse().transpose();
        glUniformMatrix3fv(_blinnPrg.getUniformLocation("normal_matrix"), 1, GL_FALSE, normal_matrix.data());
        glUniform1f(_blinnPrg.getUniformLocation("specular_coef"), _specularCoef[i]);

        _shapes[i]->draw(_blinnPrg);
    }

    _blinnPrg.deactivate();

    glDisable(GL_BLEND);

    drawLights();
    // drawShadowVolumes();
}

void Viewer::drawDeferred()
{
    _fbo.bind();

#if false
    /// DEPRECATED: Assignation des sorties des shaders
    glBindFragDataLocation(_gbufferPrg.id(), 0, "out_color");
    glBindFragDataLocation(_gbufferPrg.id(), 1, "out_normal");
#endif

    glViewport(0, 0, _fbo.width(), _fbo.height());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    _gbufferPrg.activate();

    glUniformMatrix4fv(_gbufferPrg.getUniformLocation("projection_matrix"), 1, GL_FALSE, _cam.computeProjectionMatrix().data());
    glUniformMatrix4fv(_gbufferPrg.getUniformLocation("view_matrix"), 1, GL_FALSE, _cam.computeViewMatrix().data());

    for (size_t i = 0; i < _shapes.size(); ++i)
    {
        glUniformMatrix4fv(_gbufferPrg.getUniformLocation("model_matrix"), 1, GL_FALSE, _shapes[i]->transformationMatrix().data());
        Matrix3f normal_matrix = (_cam.computeViewMatrix() * _shapes[i]->transformationMatrix()).linear().inverse().transpose();
        glUniformMatrix3fv(_gbufferPrg.getUniformLocation("normal_matrix"), 1, GL_FALSE, normal_matrix.data());
        glUniform1f(_gbufferPrg.getUniformLocation("specular_coef"), _specularCoef[i]);

        _shapes[i]->draw(_gbufferPrg);
    }

    _gbufferPrg.deactivate();

#if false
    /// DEBUG: Sauvegarde
    _fbo.savePNG("colors.png", 0);
    _fbo.savePNG("normals.png", 1);
#endif

    _fbo.unbind();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_BLEND);

    _deferredPrg.activate();

    for (int i = 0; i < 2; ++i)
    {
        glActiveTexture(i == 0 ? GL_TEXTURE0 : GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _fbo.textures[i]);
        glUniform1i(_deferredPrg.getUniformLocation(i == 0 ? "colorSampler" : "normalSampler"), i);
    }

    Vector2f windowSize = Vector2f(_winWidth, _winHeight);
    glUniform2fv(_deferredPrg.getUniformLocation("windowSize"), 1, windowSize.data());

    Matrix4f invProjMat = _cam.computeProjectionMatrix().inverse();
    glUniformMatrix4fv(_deferredPrg.getUniformLocation("invProjMat"), 1, GL_FALSE, invProjMat.data());

    for (size_t i = 0; i < _pointLights.size(); ++i)
    {
        Vector4f lightPos;
        lightPos << _pointLights[i]->transformationMatrix().translation(), 1.f;
        glUniform4fv(_deferredPrg.getUniformLocation("lightPos"), 1, (_cam.computeViewMatrix() * lightPos).eval().data());
        glUniform3fv(_deferredPrg.getUniformLocation("lightCol"), 1, _lightColors[i].data());

        _quad->draw(_deferredPrg);
    }

    _deferredPrg.deactivate();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo.id());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, _winWidth, _winHeight, 0, 0, _winWidth, _winHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    drawLights();
    // drawShadowVolumes();
}

void Viewer::drawLights()
{
    _simplePrg.activate();

    glUniformMatrix4fv(_simplePrg.getUniformLocation("projection_matrix"), 1, GL_FALSE, _cam.computeProjectionMatrix().data());
    glUniformMatrix4fv(_simplePrg.getUniformLocation("view_matrix"), 1, GL_FALSE, _cam.computeViewMatrix().data());

    for (int i = 0; i < _pointLights.size(); ++i)
    {
        Affine3f modelMatrix = _pointLights[i]->transformationMatrix();
        glUniformMatrix4fv(_simplePrg.getUniformLocation("model_matrix"), 1, GL_FALSE, modelMatrix.data());
        glUniform3fv(_simplePrg.getUniformLocation("light_col"), 1, _lightColors[i].data());

        _pointLights[i]->draw(_simplePrg);
    }

    _simplePrg.deactivate();
}

void Viewer::drawShadowVolumes()
{
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    _simplePrg.activate();

    glUniformMatrix4fv(_simplePrg.getUniformLocation("projection_matrix"), 1, GL_FALSE, _cam.computeProjectionMatrix().data());
    glUniformMatrix4fv(_simplePrg.getUniformLocation("view_matrix"), 1, GL_FALSE, _cam.computeViewMatrix().data());

    for (int i = 0; i < _shadowVolumes.size(); ++i)
    {
        Affine3f modelMatrix = _shadowVolumes[i]->transformationMatrix();
        glUniformMatrix4fv(_simplePrg.getUniformLocation("model_matrix"), 1, GL_FALSE, modelMatrix.data());

        _shadowVolumes[i]->draw(_simplePrg);
    }

    _simplePrg.deactivate();

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_CULL_FACE);
}

void Viewer::updateScene()
{
    if (_animate && glfwGetTime() > _lastTime + 1.f / 60.f)
    {
        for (int i = 0; i < _pointLights.size(); ++i)
        {
            // update light position
            Vector3f lightPos = _pointLights[i]->transformationMatrix().translation();
            Vector3f lightDir = (lightPos - _cam.sceneCenter()) / _cam.sceneRadius();
            float radius = std::sqrt(lightDir.x() * lightDir.x() + lightDir.z() * lightDir.z());
            lightDir.x() = radius * cos(_lightAngle + i * M_PI / 2.f);
            lightDir.z() = radius * sin(_lightAngle + i * M_PI / 2.f);
            _pointLights[i]->transformationMatrix().translation() = _cam.sceneCenter() + _cam.sceneRadius() * lightDir;
        }
        _lightAngle += M_PI / 100.f;
        _lastTime = glfwGetTime();
    }

    if (_shadingMode == DEFERRED)
    {
        drawDeferred();
    }
    else
    {
        drawForward();
    }
}

void Viewer::loadProgram()
{
    _blinnPrg.loadFromFiles(DATA_DIR "/shaders/blinn.vert", DATA_DIR "/shaders/blinn.frag");
    _simplePrg.loadFromFiles(DATA_DIR "/shaders/simple.vert", DATA_DIR "/shaders/simple.frag");
    // _ambiantPrg.loadFromFiles(DATA_DIR "/shaders/ambiant.vert", DATA_DIR "/shaders/ambiant.frag");
    _gbufferPrg.loadFromFiles(DATA_DIR "/shaders/gbuffer.vert", DATA_DIR "/shaders/gbuffer.frag");
    _deferredPrg.loadFromFiles(DATA_DIR "/shaders/deferred.vert", DATA_DIR "/shaders/deferred.frag");
}

void Viewer::updateGUI()
{
    ImGui::RadioButton("Forward", (int *)&_shadingMode, FORWARD);
    ImGui::SameLine();
    ImGui::RadioButton("Deferred", (int *)&_shadingMode, DEFERRED);
    ImGui::Checkbox("Animate light", &_animate);
}

////////////////////////////////////////////////////////////////////////////////
// Events

/*
   callback to manage mouse : called when user press or release mouse button
   You can change in this function the way the user
   interact with the system.
 */
void Viewer::mousePressed(GLFWwindow *window, int button, int action)
{
    if (action == GLFW_PRESS)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            _cam.startRotation(_lastMousePos);
        }
        else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            _cam.startTranslation(_lastMousePos);
        }
        _button = button;
    }
    else if (action == GLFW_RELEASE)
    {
        if (_button == GLFW_MOUSE_BUTTON_LEFT)
        {
            _cam.endRotation();
        }
        else if (_button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            _cam.endTranslation();
        }
        _button = -1;
    }
}

/*
   callback to manage mouse : called when user move mouse with button pressed
   You can change in this function the way the user
   interact with the system.
 */
void Viewer::mouseMoved(int x, int y)
{
    if (_button == GLFW_MOUSE_BUTTON_LEFT)
    {
        _cam.dragRotate(Vector2f(x, y));
    }
    else if (_button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        _cam.dragTranslate(Vector2f(x, y));
    }
    _lastMousePos = Vector2f(x, y);
}

void Viewer::mouseScroll(double x, double y)
{
    _cam.zoom((y > 0) ? 1.1 : 1. / 1.1);
}

/*
   callback to manage keyboard interactions
   You can change in this function the way the user
   interact with the system.
 */
void Viewer::keyPressed(int key, int action, int mods)
{
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        loadProgram();
    }
    else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        _animate = !_animate;
    }
    else if (key == GLFW_KEY_D && action == GLFW_PRESS)
    {
        _shadingMode = ShadingMode((_shadingMode + 1) % 2);
    }
}

void Viewer::charPressed(int key) {}
