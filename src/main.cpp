/**
 * From the OpenGL Programming wikibook: http://en.wikibooks.org/wiki/OpenGL_Programming
 * This file is in the public domain.
 * Contributors: Sylvain Beucler
 * Enhanced by: Jerry Chen
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* Use glew.h instead of gl.h to get all the GL prototypes declared */
#include <GL/glew.h>
/* Using the GLUT library for the base windowing setup */
#include <GL/glut.h>
/* GLM */
// #define GLM_MESSAGES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <shader_utils.h>

#include "res_texture.c"
#include "res_texture2.c"

#include <Buffer.h>
#include <Camera.h>
#include <FrameBuffer.h>
#include <ShaderContext.h>
#include <Light.h>
#include <Material.h>
#include <Mesh.h>
#include <PrimitiveFactory.h>
#include <File3ds.h>
#include <Program.h>
#include <Shader.h>
#include <Scene.h>
#include <Texture.h>
#include <Util.h>
#include <VarAttribute.h>
#include <VarUniform.h>
#include <vector>
#include <iostream> // std::cout
#include <sstream> // std::stringstream
#include <iomanip> // std::setprecision

const char* DEFAULT_CAPTION = "My Textured Cube";

int init_screen_width = 800, init_screen_height = 600;
vt::Camera* camera;
vt::Mesh *mesh_skybox, *mesh_box;
vt::Light *light, *light2, *light3;
vt::Texture *texture_box_color, *texture_box_normal, *texture_skybox;

bool left_mouse_down = false, right_mouse_down = false;
glm::vec2 prev_mouse_coord, mouse_drag;
glm::vec3 prev_orient, orient, orbit_speed = glm::vec3(0, -0.5, -0.5);
float prev_orbit_radius = 0, orbit_radius = 8, dolly_speed = 0.1, light_distance = 4;
bool wireframe_mode = false;
bool show_fps = false;
bool show_axis = false;
bool show_bbox = false;
bool show_normals = false;
bool show_lights = false;

int texture_id = 0;
float prev_zoom = 0, zoom = 1, ortho_dolly_speed = 0.1;

int init_resources()
{
    vt::Scene* scene = vt::Scene::instance();

    mesh_skybox = vt::PrimitiveFactory::create_viewport_quad("grid");
    scene->set_skybox(mesh_skybox);

    scene->add_mesh(mesh_box = vt::PrimitiveFactory::create_box("box"));

    mesh_box->set_origin(glm::vec3(-0.5, -0.5, -0.5)); // box

    vt::Material* bump_mapped_material = new vt::Material(
            "bump_mapped",
            "src/shaders/bump_mapped.v.glsl",
            "src/shaders/bump_mapped.f.glsl",
            true,   // use_ambient_color
            false,  // gen_normal_map
            true,   // use_phong_shading
            true,   // use_texture_mapping
            true,   // use_bump_mapping
            false,  // use_env_mapping
            false,  // use_env_mapping_dbl_refract
            false,  // use_ssao
            false,  // use_bloom_kernel
            false,  // use_texture2
            false,  // use_fragment_world_pos
            false,  // skybox
            false); // overlay
    vt::Program* bump_mapped_program = bump_mapped_material->get_program();
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_ATTRIBUTE, "texcoord");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_ATTRIBUTE, "vertex_normal");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_ATTRIBUTE, "vertex_position");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_ATTRIBUTE, "vertex_tangent");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "ambient_color");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "bump_texture");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "camera_pos");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "color_texture");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "light_color");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "light_count");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "light_enabled");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "light_pos");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "model_xform");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "mvp_xform");
    bump_mapped_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "normal_xform");
    scene->add_material(bump_mapped_material);

    vt::Material* skybox_material = new vt::Material(
            "skybox",
            "src/shaders/skybox.v.glsl",
            "src/shaders/skybox.f.glsl",
            false,  // use_ambient_color
            false,  // gen_normal_map
            false,  // use_phong_shading
            false,  // use_texture_mapping
            false,  // use_bump_mapping
            false,  // use_env_mapping
            false,  // use_env_mapping_dbl_refract
            false,  // use_ssao
            false,  // use_bloom_kernel
            false,  // use_texture2
            false,  // use_fragment_world_pos
            true,   // skybox
            false); // overlay
    vt::Program* skybox_material_program = skybox_material->get_program();
    skybox_material_program->add_var(vt::Program::VAR_TYPE_UNIFORM, "env_map_texture");
    skybox_material_program->add_var(vt::Program::VAR_TYPE_UNIFORM, "inv_normal_xform");
    skybox_material_program->add_var(vt::Program::VAR_TYPE_UNIFORM, "inv_projection_xform");
    scene->add_material(skybox_material);

    vt::Material* ambient_material = new vt::Material(
            "ambient",
            "src/shaders/ambient.v.glsl",
            "src/shaders/ambient.f.glsl",
            true,   // use_ambient_color
            false,  // gen_normal_map
            false,  // use_phong_shading
            false,  // use_texture_mapping
            false,  // use_bump_mapping
            false,  // use_env_mapping
            false,  // use_env_mapping_dbl_refract
            false,  // use_ssao
            false,  // use_bloom_kernel
            false,  // use_texture2
            false,  // use_fragment_world_pos
            false,  // skybox
            false); // overlay
    vt::Program* ambient_program = ambient_material->get_program();
    ambient_program->add_var(vt::Program::VAR_TYPE_ATTRIBUTE, "vertex_position");
    ambient_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "ambient_color");
    ambient_program->add_var(vt::Program::VAR_TYPE_UNIFORM,   "mvp_xform");
    scene->add_material(ambient_material);
    scene->set_wireframe_material(ambient_material);

    texture_box_color = new vt::Texture(
            "chesterfield_color",
            "data/chesterfield_color.png");
    scene->add_texture(               texture_box_color);
    bump_mapped_material->add_texture(texture_box_color);

    texture_box_normal = new vt::Texture(
            "chesterfield_normal",
            "data/chesterfield_normal.png");
    scene->add_texture(               texture_box_normal);
    bump_mapped_material->add_texture(texture_box_normal);

    texture_skybox = new vt::Texture(
            "skybox_texture",
            "data/SaintPetersSquare2/posx.png",
            "data/SaintPetersSquare2/negx.png",
            "data/SaintPetersSquare2/posy.png",
            "data/SaintPetersSquare2/negy.png",
            "data/SaintPetersSquare2/posz.png",
            "data/SaintPetersSquare2/negz.png");
    scene->add_texture(          texture_skybox);
    skybox_material->add_texture(texture_skybox);

    glm::vec3 origin = glm::vec3();
    camera = new vt::Camera("camera", origin + glm::vec3(0, 0, orbit_radius), origin);
    scene->set_camera(camera);

    scene->add_light(light  = new vt::Light("light1", origin + glm::vec3(light_distance, 0, 0), glm::vec3(1, 0, 0)));
    scene->add_light(light2 = new vt::Light("light2", origin + glm::vec3(0, light_distance, 0), glm::vec3(0, 1, 0)));
    scene->add_light(light3 = new vt::Light("light3", origin + glm::vec3(0, 0, light_distance), glm::vec3(0, 0, 1)));

    mesh_skybox->set_material(skybox_material);
    mesh_skybox->set_texture_index(mesh_skybox->get_material()->get_texture_index_by_name("skybox_texture"));

    // box
    mesh_box->set_material(bump_mapped_material);
    mesh_box->set_texture_index(     mesh_box->get_material()->get_texture_index_by_name("chesterfield_color"));
    mesh_box->set_bump_texture_index(mesh_box->get_material()->get_texture_index_by_name("chesterfield_normal"));
    mesh_box->set_ambient_color(glm::vec3(0, 0, 0));

    return 1;
}

int deinit_resources()
{
    return 1;
}

void onIdle()
{
    glutPostRedisplay();
}

void onTick()
{
    static unsigned int prev_tick = 0;
    static unsigned int frames = 0;
    unsigned int tick = glutGet(GLUT_ELAPSED_TIME);
    unsigned int delta_time = tick-prev_tick;
    static float fps = 0;
    if(delta_time > 1000) {
        fps = 1000.0*frames/delta_time;
        frames = 0;
        prev_tick = tick;
    }
    if(show_fps && delta_time > 100) {
        std::stringstream ss;
        ss << std::setprecision(2) << std::fixed << fps << " FPS, "
            << "Mouse: {" << mouse_drag.x << ", " << mouse_drag.y << "}, "
            << "Yaw=" << ORIENT_YAW(orient) << ", Pitch=" << ORIENT_PITCH(orient) << ", Radius=" << orbit_radius << ", "
            << "Zoom=" << zoom;
        //ss << "Width=" << camera->get_width() << ", Width=" << camera->get_height();
        glutSetWindowTitle(ss.str().c_str());
    }
    frames++;
}

void onDisplay()
{
    onTick();

    vt::Scene* scene = vt::Scene::instance();

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if(wireframe_mode) {
        scene->render(false, false, vt::Scene::use_material_type_t::USE_WIREFRAME_MATERIAL);
    } else {
        scene->render();
    }

    if(show_axis || show_bbox || show_normals) {
        scene->render_lines(show_axis, show_bbox, show_normals);
    }
    if(show_lights) {
        scene->render_lights();
    }
    glutSwapBuffers();
}

void set_mesh_visibility(bool visible)
{
    mesh_box->set_visible(visible); // box
}

void onKeyboard(unsigned char key, int x, int y)
{
    switch(key) {
        case 'b': // bbox
            show_bbox = !show_bbox;
            break;
        case 'f': // frame rate
            show_fps = !show_fps;
            if(!show_fps) {
                glutSetWindowTitle(DEFAULT_CAPTION);
            }
            break;
        case 'l': // lights
            show_lights = !show_lights;
            break;
        case 'n': // normals
            show_normals = !show_normals;
            break;
        case 'p': // projection
            if(camera->get_projection_mode() == vt::Camera::PROJECTION_MODE_PERSPECTIVE) {
                camera->set_projection_mode(vt::Camera::PROJECTION_MODE_ORTHO);
            } else if(camera->get_projection_mode() == vt::Camera::PROJECTION_MODE_ORTHO) {
                camera->set_projection_mode(vt::Camera::PROJECTION_MODE_PERSPECTIVE);
            }
            break;
        case 'w': // wireframe
            wireframe_mode = !wireframe_mode;
            if(wireframe_mode) {
                glPolygonMode(GL_FRONT, GL_LINE);
                mesh_box->set_ambient_color(glm::vec3(1, 1, 1));
            } else {
                glPolygonMode(GL_FRONT, GL_FILL);
                mesh_box->set_ambient_color(glm::vec3(0, 0, 0));
            }
            break;
        case 'x': // axis
            show_axis = !show_axis;
            break;
        case 27: // escape
            exit(0);
            break;
    }
}

void onSpecial(int key, int x, int y)
{
    switch(key) {
        case GLUT_KEY_F1:
            light->set_enabled(!light->get_enabled());
            break;
        case GLUT_KEY_F2:
            light2->set_enabled(!light2->get_enabled());
            break;
        case GLUT_KEY_F3:
            light3->set_enabled(!light3->get_enabled());
            break;
        case GLUT_KEY_LEFT:
        case GLUT_KEY_RIGHT:
        case GLUT_KEY_UP:
        case GLUT_KEY_DOWN:
            break;
    }
}

void onSpecialUp(int key, int x, int y)
{
    switch(key) {
        case GLUT_KEY_F1:
        case GLUT_KEY_LEFT:
        case GLUT_KEY_RIGHT:
        case GLUT_KEY_UP:
        case GLUT_KEY_DOWN:
            break;
    }
}

void onMouse(int button, int state, int x, int y)
{
    if(state == GLUT_DOWN) {
        prev_mouse_coord.x = x;
        prev_mouse_coord.y = y;
        if(button == GLUT_LEFT_BUTTON) {
            left_mouse_down = true;
            prev_orient = orient;
        }
        if(button == GLUT_RIGHT_BUTTON) {
            right_mouse_down = true;
            if(camera->get_projection_mode() == vt::Camera::PROJECTION_MODE_PERSPECTIVE) {
                prev_orbit_radius = orbit_radius;
            } else if (camera->get_projection_mode() == vt::Camera::PROJECTION_MODE_ORTHO) {
                prev_zoom = zoom;
            }
        }
    }
    else {
        left_mouse_down = right_mouse_down = false;
    }
}

void onMotion(int x, int y)
{
    if(left_mouse_down || right_mouse_down) {
        mouse_drag = glm::vec2(x, y) - prev_mouse_coord;
    }
    if(left_mouse_down) {
        orient = prev_orient+glm::vec3(0, mouse_drag.y*ORIENT_PITCH(orbit_speed), mouse_drag.x*ORIENT_YAW(orbit_speed));
        camera->orbit(orient, orbit_radius);
    }
    if(right_mouse_down) {
        if(camera->get_projection_mode() == vt::Camera::PROJECTION_MODE_PERSPECTIVE) {
            orbit_radius = prev_orbit_radius + mouse_drag.y*dolly_speed;
            camera->orbit(orient, orbit_radius);
        } else if (camera->get_projection_mode() == vt::Camera::PROJECTION_MODE_ORTHO) {
            zoom = prev_zoom + mouse_drag.y*ortho_dolly_speed;
            camera->set_zoom(&zoom);
        }
    }
}

void onReshape(int width, int height)
{
    camera->resize_viewport(width, height);
}

int main(int argc, char* argv[])
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA|GLUT_ALPHA|GLUT_DOUBLE|GLUT_DEPTH/*|GLUT_STENCIL*/);
    glutInitWindowSize(init_screen_width, init_screen_height);
    glutCreateWindow(DEFAULT_CAPTION);

    GLenum glew_status = glewInit();
    if(glew_status != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(glew_status));
        return 1;
    }

    if(!GLEW_VERSION_2_0) {
        fprintf(stderr, "Error: your graphic card does not support OpenGL 2.0\n");
        return 1;
    }

    char* s = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    printf("GLSL version %s\n", s);

    if(init_resources()) {
        glutDisplayFunc(onDisplay);
        glutKeyboardFunc(onKeyboard);
        glutSpecialFunc(onSpecial);
        glutSpecialUpFunc(onSpecialUp);
        glutMouseFunc(onMouse);
        glutMotionFunc(onMotion);
        glutReshapeFunc(onReshape);
        glutIdleFunc(onIdle);
        //glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glutMainLoop();
        deinit_resources();
    }

    return 0;
}
