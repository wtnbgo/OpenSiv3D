# box2d library
add_library(Siv3D_ThirdParty_box2d OBJECT
  collision/b2_broad_phase.cpp
  collision/b2_chain_shape.cpp
  collision/b2_circle_shape.cpp
  collision/b2_collide_circle.cpp
  collision/b2_collide_edge.cpp
  collision/b2_collide_polygon.cpp
  collision/b2_collision.cpp
  collision/b2_distance.cpp
  collision/b2_dynamic_tree.cpp
  collision/b2_edge_shape.cpp
  collision/b2_polygon_shape.cpp
  collision/b2_time_of_impact.cpp
  common/b2_block_allocator.cpp
  common/b2_draw.cpp
  common/b2_math.cpp
  common/b2_settings.cpp
  common/b2_stack_allocator.cpp
  common/b2_timer.cpp
  dynamics/b2_body.cpp
  dynamics/b2_chain_circle_contact.cpp
  dynamics/b2_chain_polygon_contact.cpp
  dynamics/b2_circle_contact.cpp
  dynamics/b2_contact.cpp
  dynamics/b2_contact_manager.cpp
  dynamics/b2_contact_solver.cpp
  dynamics/b2_distance_joint.cpp
  dynamics/b2_edge_circle_contact.cpp
  dynamics/b2_edge_polygon_contact.cpp
  dynamics/b2_fixture.cpp
  dynamics/b2_friction_joint.cpp
  dynamics/b2_gear_joint.cpp
  dynamics/b2_island.cpp
  dynamics/b2_joint.cpp
  dynamics/b2_motor_joint.cpp
  dynamics/b2_mouse_joint.cpp
  dynamics/b2_polygon_circle_contact.cpp
  dynamics/b2_polygon_contact.cpp
  dynamics/b2_prismatic_joint.cpp
  dynamics/b2_pulley_joint.cpp
  dynamics/b2_revolute_joint.cpp
  dynamics/b2_weld_joint.cpp
  dynamics/b2_wheel_joint.cpp
  dynamics/b2_world.cpp
  dynamics/b2_world_callbacks.cpp
  rope/b2_rope.cpp
)
target_include_directories(Siv3D_ThirdParty_box2d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_box2d PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_box2d PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
