#ifndef VT_XFORM_OBJECT_H_
#define VT_XFORM_OBJECT_H_

#include <NamedObject.h>
#include <Util.h>
#include <glm/glm.hpp>
#include <set>

namespace vt {

class XformObject : public NamedObject
{
public:
    // for guide wires
    glm::vec3 m_debug_target_dir;
    glm::vec3 m_debug_end_effector_tip_dir;
    glm::vec3 m_debug_local_pivot;
    glm::vec3 m_debug_local_target;

    XformObject(std::string name,
                glm::vec3   origin = glm::vec3(0),
                glm::vec3   orient = glm::vec3(0),
                glm::vec3   scale  = glm::vec3(1));
    virtual ~XformObject();

    // basic features
    const glm::vec3 &get_origin() const { return m_origin; }
    void set_origin(glm::vec3 origin);
    const glm::vec3 &get_orient() const { return m_orient; }
    void set_orient(glm::vec3 orient);
    const glm::vec3 &get_scale() const { return m_scale; }
    void set_scale(glm::vec3 scale);
    void reset_xform();

    // constraints
    const glm::ivec3 &get_enable_orient_constraints() const                               { return m_enable_orient_constraints; }
    void set_enable_orient_constraints(glm::ivec3 enable_orient_constraints)              { m_enable_orient_constraints = enable_orient_constraints; }
    const glm::vec3 &get_orient_constraints_center() const                                { return m_orient_constraints_center; }
    void set_orient_constraints_center(glm::vec3 orient_constraints_center)               { m_orient_constraints_center = orient_constraints_center; }
    const glm::vec3 &get_orient_constraints_max_deviation() const                         { return m_orient_constraints_max_deviation; }
    void set_orient_constraints_max_deviation(glm::vec3 orient_constraints_max_deviation) { m_orient_constraints_max_deviation = orient_constraints_max_deviation; }
    bool is_violate_constraints() const;
    void apply_constraints();

    // coordinate system conversions
    glm::vec3 map_to_abs_coord(glm::vec3 local_point = glm::vec3(0));
    glm::vec3 map_to_parent_coord(glm::vec3 abs_point) const;
    glm::vec3 map_to_origin_in_parent_coord(glm::vec3 abs_point) const;
    glm::vec3 get_abs_left_direction();
    glm::vec3 get_abs_up_direction();
    glm::vec3 get_abs_heading();

    // hierarchy related
    void link_parent(XformObject* parent, bool keep_xform = false);
    XformObject* get_parent() const        { return m_parent; }
    std::set<XformObject*> &get_children() { return m_children; }
    void unlink_children();

    // advanced features
    void point_at_local(glm::vec3 local_target, glm::vec3* up_direction = NULL);
    void point_at(glm::vec3 target, glm::vec3* up_direction = NULL);
    void rotate(float angle_delta, glm::vec3 pivot);
    bool solve_ik_ccd(XformObject* root,
                      glm::vec3    local_end_effector_tip,
                      glm::vec3    target,
                      int          iters,
                      float        accept_distance);
    void update_boid(glm::vec3 target, float forward_speed, float angle_delta, float avoid_radius);

    // basic features
    const glm::mat4 &get_xform(bool trace_down = true);
    const glm::mat4 &get_normal_xform();
    glm::mat4 get_local_rotate_xform() const;

protected:
    // basic features
    glm::vec3 m_origin;
    glm::vec3 m_orient;
    glm::vec3 m_scale;
    glm::mat4 m_xform;
    glm::mat4 m_normal_xform;

    // constraints
    glm::ivec3 m_enable_orient_constraints;
    glm::vec3 m_orient_constraints_center;
    glm::vec3 m_orient_constraints_max_deviation;

    // hierarchy related
    XformObject* m_parent;
    std::set<XformObject*> m_children;

    // caching
    void mark_dirty_xform()
    {
        m_is_dirty_xform        = true;
        m_is_dirty_normal_xform = true;
    }

    // caching (to be implemented by derived class)
    virtual void update_xform() = 0;

private:
    // caching
    bool m_is_dirty_xform;
    bool m_is_dirty_normal_xform;

    // advanced features (to be implemented by derived class)
    virtual void rebase(glm::mat4* basis = NULL) {}
    virtual void set_axis(glm::vec3 axis) {}

    // caching
    void update_xform_hier();
    void update_normal_xform();
};

}

#endif
