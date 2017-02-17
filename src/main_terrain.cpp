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
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <shader_utils.h>

#include <Buffer.h>
#include <Camera.h>
#include <File3ds.h>
#include <FrameBuffer.h>
#include <Light.h>
#include <Material.h>
#include <Mesh.h>
#include <Modifiers.h>
#include <PrimitiveFactory.h>
#include <Program.h>
#include <Scene.h>
#include <Shader.h>
#include <ShaderContext.h>
#include <Texture.h>
#include <Util.h>
#include <VarAttribute.h>
#include <VarUniform.h>
#include <vector>
#include <iostream> // std::cout
#include <sstream> // std::stringstream
#include <iomanip> // std::setprecision

#define TERRAIN_COLS    100
#define TERRAIN_ROWS    100
#define TERRAIN_WIDTH   10
#define TERRAIN_LENGTH  10
#define TERRAIN_HEIGHT  2
#define BOX_WIDTH       0.25
#define BOX_LENGTH      0.5
#define BOX_HEIGHT      0.1
#define BOX_SPEED       0.025f
#define BOX_ANGLE_SPEED 2.0

const char* DEFAULT_CAPTION = "My Textured Cube";

int init_screen_width = 800, init_screen_height = 600;
vt::Camera* camera;
vt::Mesh *mesh_skybox;
vt::Light *light, *light2, *light3;
vt::Texture *texture_box_color, *texture_box_normal, *texture_skybox;

bool left_mouse_down = false, right_mouse_down = false;
glm::vec2 prev_mouse_coord, mouse_drag;
glm::vec3 prev_orient, orient, orbit_speed = glm::vec3(0, -0.5, -0.5);
float prev_orbit_radius = 0, orbit_radius = 8, dolly_speed = 0.1, light_distance = 4;
bool show_bbox = false;
bool show_fps = false;
bool show_help = false;
bool show_lights = false;
bool show_normals = false;
bool wireframe_mode = false;
bool show_guide_wires = true;
bool show_axis = false;
bool show_axis_labels = false;
bool do_animation = true;
bool left_key = false;
bool right_key = false;
bool up_key = false;
bool down_key = false;
bool page_up_key = false;
bool page_down_key = false;
bool user_input = true;

int texture_id = 0;
float prev_zoom = 0, zoom = 1, ortho_dolly_speed = 0.1;

int angle_delta = 1;

unsigned char* height_map_pixel_data = NULL;
size_t tex_width = 0;
size_t tex_length = 0;

vt::Mesh* terrain;
vt::Mesh* box;

static vt::Mesh* create_terrain(std::string     name,
                                std::string     heightmap_png_filename,
                                int             cols,
                                int             rows,
                                float           width,
                                float           length,
                                float           height,
                                unsigned char** heightmap_pixel_data,
                                size_t*         tex_width,
                                size_t*         tex_length)
{
    if(!vt::read_png(heightmap_png_filename, reinterpret_cast<void**>(heightmap_pixel_data), tex_width, tex_length)) {
        return NULL;
    }
    vt::Mesh* terrain = vt::PrimitiveFactory::create_grid(name, cols, rows, width, length);
    terrain->set_smooth(true);
    size_t num_vertex = terrain->get_num_vertex();
    for(int i = 0; i < static_cast<int>(num_vertex); i++) {
        glm::vec3 vertex = terrain->get_vert_coord(i);
        glm::ivec2 tex_coord;
        tex_coord.x = static_cast<int>((*tex_width - 1) * (static_cast<float>(vertex.x) / width));
        tex_coord.y = static_cast<int>((*tex_length - 1) * (static_cast<float>(vertex.z) / length));
        int shade = (*heightmap_pixel_data)[tex_coord.y * (*tex_width) + tex_coord.x];
        vertex.y = static_cast<float>(shade) / 255 * height;
        terrain->set_vert_coord(i, vertex);
    }
    terrain->center_axis();
    terrain->set_origin(glm::vec3(0));
    return terrain;
}

static glm::vec2 limit_to_within_terrain(glm::vec2 pos, float width, float length)
{
    pos.x = std::max(pos.x, static_cast<float>(-width * 0.5));
    pos.x = std::min(pos.x, static_cast<float>(width * 0.5));
    pos.y = std::max(pos.y, static_cast<float>(-length * 0.5));
    pos.y = std::min(pos.y, static_cast<float>(length * 0.5));
    return pos;
}

static float lookup_terrain_height(glm::vec2      pos,
                                   float          width,
                                   float          length,
                                   float          height,
                                   unsigned char* heightmap_pixel_data,
                                   size_t         tex_width,
                                   size_t         tex_length)
{
    pos = limit_to_within_terrain(pos, width, length);
    glm::ivec2 tex_coord;
    tex_coord.x = (tex_width  - 1) * (pos.x + width  * 0.5) / width;
    tex_coord.y = (tex_length - 1) * (pos.y + length * 0.5) / length;
    int shade = height_map_pixel_data[tex_coord.y * tex_width + tex_coord.x];
    return -height * 0.5 + static_cast<float>(shade) / 255 * height;
}

static glm::vec3 lookup_terrain_normal(glm::vec2      pos,
                                       float          width,
                                       float          length,
                                       float          height,
                                       unsigned char* heightmap_pixel_data,
                                       size_t         tex_width,
                                       size_t         tex_length,
                                       float          sample_radius)
{
    glm::vec2 pos_left(pos.x - sample_radius, pos.y);
    pos_left = limit_to_within_terrain(pos_left, width, length);
    glm::vec3 point_left(pos_left.x, 0, pos_left.y);
    point_left.y = lookup_terrain_height(pos_left,
                                         width,
                                         length,
                                         height,
                                         height_map_pixel_data,
                                         tex_width,
                                         tex_length);
    glm::vec2 pos_right(pos.x + sample_radius, pos.y);
    pos_right = limit_to_within_terrain(pos_right, width, length);
    glm::vec3 point_right(pos_right.x, 0, pos_right.y);
    point_right.y = lookup_terrain_height(pos_right,
                                          width,
                                          length,
                                          height,
                                          height_map_pixel_data,
                                          tex_width,
                                          tex_length);
    glm::vec2 pos_back(pos.x, pos.y - sample_radius);
    pos_back = limit_to_within_terrain(pos_back, width, length);
    glm::vec3 point_back(pos_back.x, 0, pos_back.y);
    point_back.y = lookup_terrain_height(pos_back,
                                         width,
                                         length,
                                         height,
                                         height_map_pixel_data,
                                         tex_width,
                                         tex_length);
    glm::vec2 pos_front(pos.x, pos.y + sample_radius);
    pos_front = limit_to_within_terrain(pos_front, width, length);
    glm::vec3 point_front(pos_front.x, 0, pos_front.y);
    point_front.y = lookup_terrain_height(pos_front,
                                          width,
                                          length,
                                          height,
                                          height_map_pixel_data,
                                          tex_width,
                                          tex_length);
    glm::vec3 x_dir = glm::normalize(point_right - point_left);
    glm::vec3 z_dir = glm::normalize(point_front - point_back);
    return glm::normalize(glm::cross(z_dir, x_dir));
}

int init_resources()
{
    vt::Scene* scene = vt::Scene::instance();

    mesh_skybox = vt::PrimitiveFactory::create_viewport_quad("grid");
    scene->set_skybox(mesh_skybox);

    vt::Material* ambient_material = new vt::Material(
            "ambient",
            "src/shaders/ambient.v.glsl",
            "src/shaders/ambient.f.glsl");
    scene->add_material(ambient_material);
    scene->set_wireframe_material(ambient_material);

    vt::Material* skybox_material = new vt::Material(
            "skybox",
            "src/shaders/skybox.v.glsl",
            "src/shaders/skybox.f.glsl",
            true); // use_overlay
    scene->add_material(skybox_material);

    vt::Material* bump_mapped_material = new vt::Material(
            "bump_mapped",
            "src/shaders/bump_mapped.v.glsl",
            "src/shaders/bump_mapped.f.glsl");
    scene->add_material(bump_mapped_material);

    vt::Material* phong_material = new vt::Material(
            "phong",
            "src/shaders/phong.v.glsl",
            "src/shaders/phong.f.glsl");
    scene->add_material(phong_material);

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

    glm::vec3 origin = glm::vec3();
    camera = new vt::Camera("camera", origin + glm::vec3(0, 0, orbit_radius), origin);
    scene->set_camera(camera);

    scene->add_light(light  = new vt::Light("light1", origin + glm::vec3(light_distance, 0, 0), glm::vec3(1, 0, 0)));
    scene->add_light(light2 = new vt::Light("light2", origin + glm::vec3(0, light_distance, 0), glm::vec3(0, 1, 0)));
    scene->add_light(light3 = new vt::Light("light3", origin + glm::vec3(0, 0, light_distance), glm::vec3(0, 0, 1)));

    mesh_skybox->set_material(skybox_material);
    mesh_skybox->set_texture_index(mesh_skybox->get_material()->get_texture_index_by_name("skybox_texture"));

    terrain = create_terrain("terrain",
                             "data/heightmap.png",
                             TERRAIN_COLS,
                             TERRAIN_ROWS,
                             TERRAIN_WIDTH,
                             TERRAIN_LENGTH,
                             TERRAIN_HEIGHT,
                             &height_map_pixel_data,
                             &tex_width,
                             &tex_length);
    terrain->set_material(phong_material);
    terrain->set_ambient_color(glm::vec3(0));
    scene->add_mesh(terrain);

    box = vt::PrimitiveFactory::create_box("box", BOX_WIDTH, BOX_LENGTH, BOX_HEIGHT);
    box->center_axis(vt::BBoxObject::ALIGN_Z_MIN);
    box->set_origin(glm::vec3(0));
    box->set_material(bump_mapped_material);
    box->set_texture_index(     box->get_material()->get_texture_index_by_name("chesterfield_color"));
    box->set_bump_texture_index(box->get_material()->get_texture_index_by_name("chesterfield_normal"));
    box->set_ambient_color(glm::vec3(0));
    scene->add_mesh(box);

    vt::Scene::instance()->m_debug_target = box->get_origin();

    return 1;
}

int deinit_resources()
{
    if(height_map_pixel_data) {
        delete []height_map_pixel_data;
    }
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
    unsigned int delta_time = tick - prev_tick;
    static float fps = 0;
    if(delta_time > 1000) {
        fps = 1000.0 * frames / delta_time;
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
    if(left_key) {
        box->rotate(BOX_ANGLE_SPEED, box->get_abs_heading());
        user_input = true;
    }
    if(right_key) {
        box->rotate(-BOX_ANGLE_SPEED, box->get_abs_heading());
        user_input = true;
    }
    if(up_key) {
        box->set_origin(box->get_origin() + box->get_abs_up_direction() * BOX_SPEED);
        user_input = true;
    }
    if(down_key) {
        box->set_origin(box->get_origin() - box->get_abs_up_direction() * BOX_SPEED);
        user_input = true;
    }
    if(user_input) {
        glm::vec2 pos_within_terrain = limit_to_within_terrain(glm::vec2(box->get_origin().x, box->get_origin().z),
                                                               TERRAIN_WIDTH,
                                                               TERRAIN_LENGTH);
        float terrain_height = lookup_terrain_height(pos_within_terrain,
                                                     TERRAIN_WIDTH,
                                                     TERRAIN_LENGTH,
                                                     TERRAIN_HEIGHT,
                                                     height_map_pixel_data,
                                                     tex_width,
                                                     tex_length);
        glm::vec3 terrain_normal = lookup_terrain_normal(pos_within_terrain,
                                                         TERRAIN_WIDTH,
                                                         TERRAIN_LENGTH,
                                                         TERRAIN_HEIGHT,
                                                         height_map_pixel_data,
                                                         tex_width,
                                                         tex_length,
                                                         BOX_WIDTH * 0.5);
        box->set_origin(glm::vec3(pos_within_terrain.x, terrain_height, pos_within_terrain.y));
        glm::vec3 abs_heading = box->get_abs_heading();
        glm::vec3 pivot       = glm::cross(terrain_normal, abs_heading);
        float     delta_angle = glm::degrees(glm::angle(terrain_normal, abs_heading));
        box->rotate(-delta_angle, pivot);
        vt::Scene::instance()->m_debug_target = box->get_origin();
        user_input = false;
    }
    static int angle = 0;
    angle = (angle + angle_delta) % 360;
}

char* get_help_string()
{
    return const_cast<char*>("HUD text");
}

void onDisplay()
{
    if(do_animation) {
        onTick();
    }
    vt::Scene* scene = vt::Scene::instance();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(wireframe_mode) {
        scene->render(true, false, false, vt::Scene::use_material_type_t::USE_WIREFRAME_MATERIAL);
    } else {
        scene->render();
    }
    if(show_guide_wires || show_axis || show_axis_labels || show_bbox || show_normals || show_help) {
        scene->render_lines_and_text(show_guide_wires, show_axis, show_axis_labels, show_bbox, show_normals, show_help, get_help_string());
    }
    if(show_lights) {
        scene->render_lights();
    }
    glutSwapBuffers();
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
        case 'g': // guide wires
            show_guide_wires = !show_guide_wires;
            if(show_guide_wires) {
                vt::Scene::instance()->m_debug_target = box->get_origin();
            }
            break;
        case 'h': // help
            show_help = !show_help;
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
                terrain->set_ambient_color(glm::vec3(1));
                box->set_ambient_color(    glm::vec3(1));
            } else {
                glPolygonMode(GL_FRONT, GL_FILL);
                terrain->set_ambient_color(glm::vec3(0));
                box->set_ambient_color(    glm::vec3(0));
            }
            break;
        case 'x': // axis
            show_axis = !show_axis;
            break;
        case 'z': // axis labels
            show_axis_labels = !show_axis_labels;
            break;
        case 32: // space
            do_animation = !do_animation;
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
            light->set_enabled(!light->is_enabled());
            break;
        case GLUT_KEY_F2:
            light2->set_enabled(!light2->is_enabled());
            break;
        case GLUT_KEY_F3:
            light3->set_enabled(!light3->is_enabled());
            break;
        case GLUT_KEY_HOME: // target
            {
                box->set_origin(glm::vec3(0));
                vt::Scene::instance()->m_debug_target = box->get_origin();
                user_input = true;
            }
            break;
        case GLUT_KEY_LEFT:
            left_key = true;
            break;
        case GLUT_KEY_RIGHT:
            right_key = true;
            break;
        case GLUT_KEY_UP:
            up_key = true;
            break;
        case GLUT_KEY_DOWN:
            down_key = true;
            break;
        case GLUT_KEY_PAGE_UP:
            page_up_key = true;
            break;
        case GLUT_KEY_PAGE_DOWN:
            page_down_key = true;
            break;
    }
}

void onSpecialUp(int key, int x, int y)
{
    switch(key) {
        case GLUT_KEY_LEFT:
            left_key = false;
            break;
        case GLUT_KEY_RIGHT:
            right_key = false;
            break;
        case GLUT_KEY_UP:
            up_key = false;
            break;
        case GLUT_KEY_DOWN:
            down_key = false;
            break;
        case GLUT_KEY_PAGE_UP:
            page_up_key = false;
            break;
        case GLUT_KEY_PAGE_DOWN:
            page_down_key = false;
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
        orient = prev_orient + glm::vec3(0, mouse_drag.y * ORIENT_PITCH(orbit_speed), mouse_drag.x * ORIENT_YAW(orbit_speed));
        camera->orbit(orient, orbit_radius);
    }
    if(right_mouse_down) {
        if(camera->get_projection_mode() == vt::Camera::PROJECTION_MODE_PERSPECTIVE) {
            orbit_radius = prev_orbit_radius + mouse_drag.y * dolly_speed;
            camera->orbit(orient, orbit_radius);
        } else if (camera->get_projection_mode() == vt::Camera::PROJECTION_MODE_ORTHO) {
            zoom = prev_zoom + mouse_drag.y * ortho_dolly_speed;
            camera->set_zoom(&zoom);
        }
    }
}

void onReshape(int width, int height)
{
    camera->resize(0, 0, width, height);
    glViewport(0, 0, width, height);
}

int main(int argc, char* argv[])
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_ALPHA | GLUT_DOUBLE | GLUT_DEPTH /*| GLUT_STENCIL*/);
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

    const char* s = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
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
